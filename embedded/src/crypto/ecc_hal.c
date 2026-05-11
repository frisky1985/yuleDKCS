/******************************************************************************
 * @file    ecc_hal.c
 * @brief   ECC 硬件加速抽象层实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 * @note    支持 SE050, CryptoCell, mbedtls 多后端
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "ecc_hal.h"

#ifdef ENABLE_SE050
#include "se050_adapter.h"
#endif

/* 线程支持 (如可用) */
#ifdef ENABLE_THREAD_SAFETY
#include <pthread.h>
#define ECC_HAL_LOCK()    pthread_mutex_lock(&g_ecc_ctx.mutex)
#define ECC_HAL_UNLOCK()  pthread_mutex_unlock(&g_ecc_ctx.mutex)
#else
#define ECC_HAL_LOCK()    do { } while(0)
#define ECC_HAL_UNLOCK()  do { } while(0)
#endif

/******************************************************************************
 * 配置宏 - 可通过 CMake 或编译开关配置
 ******************************************************************************/
#ifndef ENABLE_SE050
#define ENABLE_SE050 1
#endif

#ifndef ENABLE_CRYPTCELL
#define ENABLE_CRYPTCELL 0
#endif

#ifndef ENABLE_MBEDTLS
#define ENABLE_MBEDTLS 1
#endif

/******************************************************************************
 * 内部数据结构
 ******************************************************************************/

typedef struct {
    ecc_key_handle_t handle;
    ecc_curve_t curve;
    uint16_t flags;
    bool in_use;
    union {
        struct {
            uint32_t se050_key_id;
        } se050;
        struct {
            void *mbedtls_keypair;
        } mbedtls;
        struct {
            uint32_t tee_key_id;
        } tee;
    } backend_data;
} ecc_key_slot_t;

#define MAX_KEY_SLOTS 16

typedef struct {
    bool initialized;
    ecc_backend_t active_backend;
    ecc_hal_config_t config;
    ecc_key_slot_t key_slots[MAX_KEY_SLOTS];
    uint32_t next_handle;
    ecc_capabilities_t caps;
#ifdef ENABLE_THREAD_SAFETY
    pthread_mutex_t mutex;
#endif
    /* 最近使用缓存 */
    struct {
        ecc_key_handle_t handle;
        ecc_key_slot_t *slot;
        uint32_t last_access;
    } cache[4];
    uint32_t access_counter;
} ecc_hal_context_t;

static ecc_hal_context_t g_ecc_ctx = {0};

/******************************************************************************
 * 内部函数声明
 ******************************************************************************/
static error_t se050_init(const ecc_hal_config_t *config);
static error_t se050_deinit(void);
static error_t se050_generate_keypair(ecc_curve_t curve, uint16_t flags, ecc_key_handle_t *handle);
static error_t se050_delete_key(ecc_key_handle_t handle);
static error_t se050_ecdh(ecc_key_handle_t priv_handle, const uint8_t *pub_key, size_t pub_len, ecdh_shared_secret_t *secret);
static error_t se050_ecdsa_sign(ecc_key_handle_t handle, const uint8_t *digest, size_t digest_len, ecdsa_signature_t *sig);
static error_t se050_ecdsa_verify(ecc_key_handle_t handle, const uint8_t *digest, size_t digest_len, const ecdsa_signature_t *sig);

static error_t mbedtls_init(const ecc_hal_config_t *config);
static error_t mbedtls_deinit(void);
static error_t mbedtls_generate_keypair(ecc_curve_t curve, uint16_t flags, ecc_key_handle_t *handle);
static error_t mbedtls_delete_key(ecc_key_handle_t handle);
static error_t mbedtls_ecdh(ecc_key_handle_t priv_handle, const uint8_t *pub_key, size_t pub_len, ecdh_shared_secret_t *secret);
static error_t mbedtls_ecdsa_sign(ecc_key_handle_t handle, const uint8_t *digest, size_t digest_len, ecdsa_signature_t *sig);
static error_t mbedtls_ecdsa_verify(ecc_key_handle_t handle, const uint8_t *digest, size_t digest_len, const ecdsa_signature_t *sig);

static error_t cryptcell_init(const ecc_hal_config_t *config);
static error_t cryptcell_deinit(void);

/******************************************************************************
 * 辅助函数
 ******************************************************************************/

static ecc_key_slot_t* find_key_slot(ecc_key_handle_t handle)
{
    /* 先检查缓存 */
    for (int i = 0; i < 4; i++) {
        if (g_ecc_ctx.cache[i].handle == handle && g_ecc_ctx.cache[i].slot != NULL) {
            g_ecc_ctx.cache[i].last_access = ++g_ecc_ctx.access_counter;
            return g_ecc_ctx.cache[i].slot;
        }
    }
    
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (g_ecc_ctx.key_slots[i].in_use && g_ecc_ctx.key_slots[i].handle == handle) {
            /* 更新缓存 - LRU替换 */
            int cache_idx = 0;
            uint32_t oldest = g_ecc_ctx.cache[0].last_access;
            for (int j = 1; j < 4; j++) {
                if (g_ecc_ctx.cache[j].last_access < oldest) {
                    oldest = g_ecc_ctx.cache[j].last_access;
                    cache_idx = j;
                }
            }
            g_ecc_ctx.cache[cache_idx].handle = handle;
            g_ecc_ctx.cache[cache_idx].slot = &g_ecc_ctx.key_slots[i];
            g_ecc_ctx.cache[cache_idx].last_access = ++g_ecc_ctx.access_counter;
            return &g_ecc_ctx.key_slots[i];
        }
    }
    return NULL;
}

static ecc_key_slot_t* alloc_key_slot(void)
{
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (!g_ecc_ctx.key_slots[i].in_use) {
            g_ecc_ctx.key_slots[i].in_use = true;
            g_ecc_ctx.key_slots[i].handle = g_ecc_ctx.next_handle++;
            if (g_ecc_ctx.next_handle == ECC_INVALID_KEY_HANDLE) {
                g_ecc_ctx.next_handle = 0;
            }
            return &g_ecc_ctx.key_slots[i];
        }
    }
    return NULL;
}

static void free_key_slot(ecc_key_slot_t *slot)
{
    if (slot) {
        memset(slot, 0, sizeof(*slot));
    }
}

/******************************************************************************
 * 安全内存操作 (volatile 防止优化)
 ******************************************************************************/
void ecc_hal_secure_zero(void *ptr, size_t len)
{
    volatile uint8_t *p = ptr;
    while (len--) {
        *p++ = 0;
    }
}

/******************************************************************************
 * 曲线工具函数
 ******************************************************************************/
size_t ecc_hal_get_key_length(ecc_curve_t curve)
{
    switch (curve) {
        case ECC_CURVE_P256:
            return 32;
        case ECC_CURVE_P384:
            return 48;
        case ECC_CURVE_P521:
            return 66;
        case ECC_CURVE_SM2:
            return 32;
        default:
            return 0;
    }
}

size_t ecc_hal_get_public_key_length(ecc_curve_t curve)
{
    /* 未压缩格式: 0x04 + X + Y */
    switch (curve) {
        case ECC_CURVE_P256:
            return 65;
        case ECC_CURVE_P384:
            return 97;
        case ECC_CURVE_P521:
            return 133;
        case ECC_CURVE_SM2:
            return 65;
        default:
            return 0;
    }
}

bool ecc_hal_validate_public_key(ecc_curve_t curve, const uint8_t *public_key, size_t key_len)
{
    if (!public_key || key_len == 0) {
        return false;
    }
    
    size_t expected_len = ecc_hal_get_public_key_length(curve);
    if (expected_len == 0 || key_len != expected_len) {
        return false;
    }
    
    /* 检查未压缩格式标识 */
    if (public_key[0] != 0x04) {
        return false;
    }
    
    return true;
}

/******************************************************************************
 * SE050 后端实现 (模拟/桥接层)
 ******************************************************************************/
#if ENABLE_SE050

/* SE050 特定定义 */
#define SE050_P256_KEY_ID_BASE  0x7B000100
#define SE050_CURVE_NIST_P256   0x03

typedef struct {
    uint32_t key_id;
    uint8_t key_type;
} se050_key_info_t;

static se050_key_info_t se050_keys[MAX_KEY_SLOTS];

static error_t se050_init(const ecc_hal_config_t *config)
{
    (void)config;
    /* 模拟 SE050 初始化 */
    memset(se050_keys, 0, sizeof(se050_keys));
    
    /* 检查 SE050 硬件存在性 */
    /* 实际项目中调用 SE050 HostLib 初始化 */
    
    return OK;
}

static error_t se050_deinit(void)
{
    /* 模拟 SE050 反初始化 */
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (se050_keys[i].key_id != 0) {
            /* 删除硬件密钥 */
            se050_keys[i].key_id = 0;
        }
    }
    return OK;
}

static error_t se050_generate_keypair(ecc_curve_t curve, uint16_t flags, ecc_key_handle_t *handle)
{
    if (curve != ECC_CURVE_P256) {
        return ERROR_NOT_SUPPORTED;
    }
    
    /* 分配 SE050 密钥 ID */
    uint32_t key_id = 0;
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (se050_keys[i].key_id == 0) {
            key_id = SE050_P256_KEY_ID_BASE + i;
            se050_keys[i].key_id = key_id;
            se050_keys[i].key_type = SE050_CURVE_NIST_P256;
            break;
        }
    }
    
    if (key_id == 0) {
        return ERROR_NO_MEMORY;
    }
    
    /* 在 SE050 内部生成密钥对 */
    /* 实际调用: Se05x_API_CreateP256Object() */
    
    ecc_key_slot_t *slot = alloc_key_slot();
    if (!slot) {
        return ERROR_NO_MEMORY;
    }
    
    slot->curve = curve;
    slot->flags = flags | ECC_KEY_FLAG_HARDWARE;
    slot->backend_data.se050.se050_key_id = key_id;
    
    *handle = slot->handle;
    return OK;
}

static error_t se050_delete_key(ecc_key_handle_t handle)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_PARAM;
    }
    
    uint32_t key_id = slot->backend_data.se050.se050_key_id;
    
    /* 在 SE050 中删除密钥 */
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (se050_keys[i].key_id == key_id) {
            /* 实际调用: Se05x_API_DeleteSecureObject() */
            se050_keys[i].key_id = 0;
            break;
        }
    }
    
    free_key_slot(slot);
    return OK;
}

static error_t se050_ecdh(ecc_key_handle_t priv_handle, const uint8_t *pub_key, 
                          size_t pub_len, ecdh_shared_secret_t *secret)
{
    ecc_key_slot_t *slot = find_key_slot(priv_handle);
    if (!slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    if (!(slot->flags & ECC_KEY_FLAG_USAGE_ECDH)) {
        return ERROR_ECC_KEY_USAGE_RESTRICTED;
    }
    
    if (!ecc_hal_validate_public_key(ECC_CURVE_P256, pub_key, pub_len)) {
        return ERROR_INVALID_PARAM;
    }
    
    uint32_t key_id = slot->backend_data.se050.key_id;
    
    /* 使用 SE050 计算 ECDH 共享密钥 */
    /* 实际调用: Se05x_API_ECDHGenerateSharedSecret() */
    
    /* 模拟生成随机共享密钥 (实际由硬件计算) */
    for (int i = 0; i < ECC_P256_KEY_LEN; i++) {
        secret->key[i] = (uint8_t)(key_id + i + pub_key[i]);
    }
    secret->validity = 0xFFFFFFFF;
    
    return OK;
}

static error_t se050_ecdsa_sign(ecc_key_handle_t handle, const uint8_t *digest, 
                                size_t digest_len, ecdsa_signature_t *sig)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    if (!(slot->flags & ECC_KEY_FLAG_USAGE_SIGN)) {
        return ERROR_ECC_KEY_USAGE_RESTRICTED;
    }
    
    if (!digest || digest_len != 32 || !sig) {
        return ERROR_INVALID_PARAM;
    }
    
    uint32_t key_id = slot->backend_data.se050.key_id;
    
    /* 使用 SE050 进行 ECDSA 签名 */
    /* 实际调用: Se05x_API_ECDSASign() */
    
    /* 模拟签名输出 (实际由硬件计算) */
    for (int i = 0; i < ECC_P256_KEY_LEN; i++) {
        sig->r[i] = (uint8_t)(key_id + digest[i]);
        sig->s[i] = (uint8_t)(key_id + digest[i] + 1);
    }
    
    return OK;
}

static error_t se050_ecdsa_verify(ecc_key_handle_t handle, const uint8_t *digest, 
                                  size_t digest_len, const ecdsa_signature_t *sig)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    if (!digest || digest_len != 32 || !sig) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 使用 SE050 进行验签 */
    /* 实际调用: Se05x_API_ECDSAVerify() */
    
    /* 模拟验签成功 (实际由硬件验证) */
    return OK;
}

static error_t se050_export_public_key(ecc_key_handle_t handle, uint8_t *public_key, size_t *key_len)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    if (!public_key || !key_len) {
        return ERROR_INVALID_PARAM;
    }
    
    uint32_t key_id = slot->backend_data.se050.key_id;
    
    /* 使用 SE050 导出公钥 */
    /* 实际调用: Se05x_API_ReadObject() */
    
    public_key[0] = 0x04; /* 未压缩格式 */
    for (int i = 1; i < 65; i++) {
        public_key[i] = (uint8_t)(key_id + i);
    }
    *key_len = 65;
    
    return OK;
}

#endif /* ENABLE_SE050 */

/******************************************************************************
 * mbedtls 后端实现 (软件后备)
 ******************************************************************************/
#if ENABLE_MBEDTLS

/* 简化的 mbedtls 模拟实现 */
typedef struct {
    uint8_t private_key[66];    /* 最大支持 P-521 */
    uint8_t public_key[133];    /* 未压缩格式 */
    size_t key_len;
    ecc_curve_t curve;
    bool has_private;
} mbedtls_keypair_t;

static mbedtls_keypair_t mbedtls_keys[MAX_KEY_SLOTS];

/* 简化的随机数生成 */
static void generate_random_bytes(uint8_t *buf, size_t len)
{
    static uint32_t seed = 0x12345678;
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        buf[i] = (uint8_t)(seed >> 16);
    }
}

/* 简化的 ECDH 计算 */
static void compute_ecdh_shared_secret(const uint8_t *priv_key, const uint8_t *pub_key, 
                                        uint8_t *shared_secret, size_t key_len)
{
    /* 模拟计算: 实际项目中使用 mbedtls_ecp_mul() */
    for (size_t i = 0; i < key_len && i < ECC_P256_KEY_LEN; i++) {
        shared_secret[i] = priv_key[i] ^ pub_key[i + 1];
    }
}

static error_t mbedtls_init(const ecc_hal_config_t *config)
{
    (void)config;
    memset(mbedtls_keys, 0, sizeof(mbedtls_keys));
    return OK;
}

static error_t mbedtls_deinit(void)
{
    /* 安全清除所有软件密钥 */
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (mbedtls_keys[i].key_len > 0) {
            ecc_hal_secure_zero(mbedtls_keys[i].private_key, sizeof(mbedtls_keys[i].private_key));
            ecc_hal_secure_zero(mbedtls_keys[i].public_key, sizeof(mbedtls_keys[i].public_key));
        }
    }
    memset(mbedtls_keys, 0, sizeof(mbedtls_keys));
    return OK;
}

static error_t mbedtls_generate_keypair(ecc_curve_t curve, uint16_t flags, ecc_key_handle_t *handle)
{
    size_t key_len = ecc_hal_get_key_length(curve);
    if (key_len == 0) {
        return ERROR_ECC_INVALID_CURVE;
    }
    
    /* 分配 mbedtls 密钥索引 */
    int idx = -1;
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (mbedtls_keys[i].key_len == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) {
        return ERROR_NO_MEMORY;
    }
    
    /* 生成随机私钥 (实际项目使用 mbedtls_ctr_drbg_random) */
    generate_random_bytes(mbedtls_keys[idx].private_key, key_len);
    
    /* 计算公钥 (实际项目使用 mbedtls_ecp_mul) */
    mbedtls_keys[idx].public_key[0] = 0x04;
    for (size_t i = 0; i < key_len; i++) {
        mbedtls_keys[idx].public_key[i + 1] = mbedtls_keys[idx].private_key[i] ^ 0x55;
        mbedtls_keys[idx].public_key[i + 1 + key_len] = mbedtls_keys[idx].private_key[i] ^ 0xAA;
    }
    
    mbedtls_keys[idx].key_len = key_len;
    mbedtls_keys[idx].curve = curve;
    mbedtls_keys[idx].has_private = true;
    
    ecc_key_slot_t *slot = alloc_key_slot();
    if (!slot) {
        mbedtls_keys[idx].key_len = 0;
        return ERROR_NO_MEMORY;
    }
    
    slot->curve = curve;
    slot->flags = flags;
    slot->backend_data.mbedtls.mbedtls_keypair = &mbedtls_keys[idx];
    
    *handle = slot->handle;
    return OK;
}

static error_t mbedtls_import_public_key(ecc_curve_t curve, const uint8_t *public_key, 
                                          size_t key_len, ecc_key_handle_t *handle)
{
    if (!ecc_hal_validate_public_key(curve, public_key, key_len)) {
        return ERROR_INVALID_PARAM;
    }
    
    int idx = -1;
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (mbedtls_keys[i].key_len == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) {
        return ERROR_NO_MEMORY;
    }
    
    size_t key_len_bytes = ecc_hal_get_key_length(curve);
    memcpy(mbedtls_keys[idx].public_key, public_key, key_len);
    mbedtls_keys[idx].key_len = key_len_bytes;
    mbedtls_keys[idx].curve = curve;
    mbedtls_keys[idx].has_private = false;
    
    ecc_key_slot_t *slot = alloc_key_slot();
    if (!slot) {
        mbedtls_keys[idx].key_len = 0;
        return ERROR_NO_MEMORY;
    }
    
    slot->curve = curve;
    slot->flags = ECC_KEY_FLAG_USAGE_VERIFY | ECC_KEY_FLAG_USAGE_ECDH;
    slot->backend_data.mbedtls.mbedtls_keypair = &mbedtls_keys[idx];
    
    *handle = slot->handle;
    return OK;
}

static error_t mbedtls_delete_key(ecc_key_handle_t handle)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_PARAM;
    }
    
    mbedtls_keypair_t *keypair = (mbedtls_keypair_t*)slot->backend_data.mbedtls.mbedtls_keypair;
    if (keypair) {
        ecc_hal_secure_zero(keypair->private_key, sizeof(keypair->private_key));
        keypair->key_len = 0;
    }
    
    free_key_slot(slot);
    return OK;
}

static error_t mbedtls_ecdh(ecc_key_handle_t priv_handle, const uint8_t *pub_key, 
                            size_t pub_len, ecdh_shared_secret_t *secret)
{
    ecc_key_slot_t *priv_slot = find_key_slot(priv_handle);
    if (!priv_slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    if (!(priv_slot->flags & ECC_KEY_FLAG_USAGE_ECDH)) {
        return ERROR_ECC_KEY_USAGE_RESTRICTED;
    }
    
    if (!ecc_hal_validate_public_key(priv_slot->curve, pub_key, pub_len)) {
        return ERROR_INVALID_PARAM;
    }
    
    mbedtls_keypair_t *keypair = (mbedtls_keypair_t*)priv_slot->backend_data.mbedtls.mbedtls_keypair;
    if (!keypair || !keypair->has_private) {
        return ERROR_ECC_KEY_NOT_FOUND;
    }
    
    /* 计算共享密钥 */
    compute_ecdh_shared_secret(keypair->private_key, pub_key, secret->key, keypair->key_len);
    secret->validity = 0xFFFFFFFF;
    
    return OK;
}

static error_t mbedtls_ecdsa_sign(ecc_key_handle_t handle, const uint8_t *digest, 
                                  size_t digest_len, ecdsa_signature_t *sig)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    if (!(slot->flags & ECC_KEY_FLAG_USAGE_SIGN)) {
        return ERROR_ECC_KEY_USAGE_RESTRICTED;
    }
    
    if (!digest || digest_len != 32 || !sig) {
        return ERROR_INVALID_PARAM;
    }
    
    mbedtls_keypair_t *keypair = (mbedtls_keypair_t*)slot->backend_data.mbedtls.mbedtls_keypair;
    if (!keypair || !keypair->has_private) {
        return ERROR_ECC_KEY_NOT_FOUND;
    }
    
    /* 简化签名 (实际项目使用 mbedtls_ecdsa_sign) */
    for (int i = 0; i < ECC_P256_KEY_LEN; i++) {
        sig->r[i] = keypair->private_key[i] ^ digest[i];
        sig->s[i] = keypair->private_key[i] ^ digest[i] ^ 0x55;
    }
    
    return OK;
}

static error_t mbedtls_ecdsa_verify(ecc_key_handle_t handle, const uint8_t *digest, 
                                    size_t digest_len, const ecdsa_signature_t *sig)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    if (!digest || digest_len != 32 || !sig) {
        return ERROR_INVALID_PARAM;
    }
    
    mbedtls_keypair_t *keypair = (mbedtls_keypair_t*)slot->backend_data.mbedtls.mbedtls_keypair;
    if (!keypair) {
        return ERROR_ECC_KEY_NOT_FOUND;
    }
    
    /* 模拟验签 (实际项目使用 mbedtls_ecdsa_verify) */
    return OK;
}

static error_t mbedtls_export_public_key(ecc_key_handle_t handle, uint8_t *public_key, size_t *key_len)
{
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    mbedtls_keypair_t *keypair = (mbedtls_keypair_t*)slot->backend_data.mbedtls.mbedtls_keypair;
    if (!keypair) {
        return ERROR_ECC_KEY_NOT_FOUND;
    }
    
    size_t pub_len = ecc_hal_get_public_key_length(keypair->curve);
    memcpy(public_key, keypair->public_key, pub_len);
    *key_len = pub_len;
    
    return OK;
}

#endif /* ENABLE_MBEDTLS */

/******************************************************************************
 * CryptoCell 后端实现 (占位符)
 ******************************************************************************/
#if ENABLE_CRYPTCELL

static error_t cryptcell_init(const ecc_hal_config_t *config)
{
    (void)config;
    /* CryptoCell 初始化 */
    /* 实际项目中调用 ARM CryptoCell 驱动 */
    return ERROR_NOT_SUPPORTED; /* 暂时不支持 */
}

static error_t cryptcell_deinit(void)
{
    return OK;
}

#endif /* ENABLE_CRYPTCELL */

/******************************************************************************
 * 公共 API 实现
 ******************************************************************************/

const char* ecc_hal_get_version(void)
{
    return ECC_HAL_VERSION_STRING;
}

error_t ecc_hal_init(const ecc_hal_config_t *config)
{
    if (g_ecc_ctx.initialized) {
        return ERROR_ALREADY_INITIALIZED;
    }
    
    if (!config) {
        return ERROR_INVALID_PARAM;
    }
    
    ECC_HAL_LOCK();
    
    memset(&g_ecc_ctx, 0, sizeof(g_ecc_ctx));
    memcpy(&g_ecc_ctx.config, config, sizeof(ecc_hal_config_t));
    
    /* 初始化缓存 */
    ecc_hal_clear_cache();
    g_ecc_ctx.access_counter = 1;
    
    /* 尝试按优先顺序初始化后端 */
    error_t result = ERROR_NOT_SUPPORTED;
    ecc_backend_t backend_order[] = {ECC_BACKEND_SE050, ECC_BACKEND_CRYPTCELL, ECC_BACKEND_MBEDTLS};
    
    if (config->preferred_backend < ECC_BACKEND_MAX && !config->force_software_fallback) {
        backend_order[0] = config->preferred_backend;
    }
    
    for (int i = 0; i < 3; i++) {
        switch (backend_order[i]) {
#if ENABLE_SE050
            case ECC_BACKEND_SE050:
                result = se050_init(config);
                if (result == OK) {
                    g_ecc_ctx.active_backend = ECC_BACKEND_SE050;
                    strncpy(g_ecc_ctx.caps.backend_name, "SE050", sizeof(g_ecc_ctx.caps.backend_name) - 1);
                    g_ecc_ctx.caps.backend_name[sizeof(g_ecc_ctx.caps.backend_name) - 1] = '\0';
                    g_ecc_ctx.caps.hardware_protection = true;
                    g_ecc_ctx.caps.side_channel_resistant = true;
                    g_ecc_ctx.caps.p384_supported = false; /* SE050 优先版本不支持 P-384 */
                }
                break;
#endif
#if ENABLE_CRYPTCELL
            case ECC_BACKEND_CRYPTCELL:
                result = cryptcell_init(config);
                if (result == OK) {
                    g_ecc_ctx.active_backend = ECC_BACKEND_CRYPTCELL;
                    strncpy(g_ecc_ctx.caps.backend_name, "CryptoCell", sizeof(g_ecc_ctx.caps.backend_name) - 1);
                    g_ecc_ctx.caps.backend_name[sizeof(g_ecc_ctx.caps.backend_name) - 1] = '\0';
                    g_ecc_ctx.caps.hardware_protection = true;
                    g_ecc_ctx.caps.side_channel_resistant = true;
                    g_ecc_ctx.caps.p384_supported = true;
                }
                break;
#endif
#if ENABLE_MBEDTLS
            case ECC_BACKEND_MBEDTLS:
                result = mbedtls_init(config);
                if (result == OK) {
                    g_ecc_ctx.active_backend = ECC_BACKEND_MBEDTLS;
                    strncpy(g_ecc_ctx.caps.backend_name, "mbedTLS", sizeof(g_ecc_ctx.caps.backend_name) - 1);
                    g_ecc_ctx.caps.backend_name[sizeof(g_ecc_ctx.caps.backend_name) - 1] = '\0';
                    g_ecc_ctx.caps.hardware_protection = false;
                    g_ecc_ctx.caps.side_channel_resistant = false;
                    g_ecc_ctx.caps.p384_supported = true;
                }
                break;
#endif
            default:
                continue;
        }
        
        if (result == OK) {
            break;
        }
    }
    
    if (result != OK) {
        ECC_HAL_UNLOCK();
        return ERROR_ECC_HARDWARE_FAILURE;
    }
    
    /* 设置能力 */
    g_ecc_ctx.caps.p256_supported = true;
    g_ecc_ctx.caps.ecdh_supported = true;
    g_ecc_ctx.caps.ecdsa_sign_supported = true;
    g_ecc_ctx.caps.ecdsa_verify_supported = true;
    g_ecc_ctx.caps.max_key_count = MAX_KEY_SLOTS;
    
    g_ecc_ctx.next_handle = 1;
    g_ecc_ctx.initialized = true;
    
    ECC_HAL_UNLOCK();
    return OK;
}

error_t ecc_hal_deinit(void)
{
    if (!g_ecc_ctx.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    ECC_HAL_LOCK();
    
    /* 清除缓存 */
    ecc_hal_clear_cache();
    
    /* 删除所有密钥 */
    for (int i = 0; i < MAX_KEY_SLOTS; i++) {
        if (g_ecc_ctx.key_slots[i].in_use) {
            ecc_hal_delete_key(g_ecc_ctx.key_slots[i].handle);
        }
    }
    
    /* 反初始化后端 */
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_SE050
        case ECC_BACKEND_SE050:
            se050_deinit();
            break;
#endif
#if ENABLE_CRYPTCELL
        case ECC_BACKEND_CRYPTCELL:
            cryptcell_deinit();
            break;
#endif
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            mbedtls_deinit();
            break;
#endif
        default:
            break;
    }
    
    memset(&g_ecc_ctx, 0, sizeof(g_ecc_ctx));
    
    ECC_HAL_UNLOCK();
    return OK;
}

error_t ecc_hal_get_capabilities(ecc_capabilities_t *caps)
{
    ECC_HAL_LOCK();
    
    if (!caps || !g_ecc_ctx.initialized) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    memcpy(caps, &g_ecc_ctx.caps, sizeof(ecc_capabilities_t));
    
    ECC_HAL_UNLOCK();
    return OK;
}

ecc_backend_t ecc_hal_get_current_backend(void)
{
    ECC_HAL_LOCK();
    ecc_backend_t backend = g_ecc_ctx.active_backend;
    ECC_HAL_UNLOCK();
    return backend;
}

/******************************************************************************
 * 密钥管理 API
 ******************************************************************************/

error_t ecc_hal_generate_keypair(ecc_curve_t curve, uint16_t flags, ecc_key_handle_t *handle)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !handle) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    if (curve != ECC_CURVE_P256 && curve != ECC_CURVE_P384) {
        if (curve == ECC_CURVE_P384 && !g_ecc_ctx.caps.p384_supported) {
            ECC_HAL_UNLOCK();
            return ERROR_ECC_INVALID_CURVE;
        }
        if (curve != ECC_CURVE_P256) {
            ECC_HAL_UNLOCK();
            return ERROR_ECC_INVALID_CURVE;
        }
    }
    
    /* 确保默认启用 ECDH 和 Sign */
    if (!(flags & (ECC_KEY_FLAG_USAGE_SIGN | ECC_KEY_FLAG_USAGE_ECDH | ECC_KEY_FLAG_USAGE_VERIFY))) {
        flags |= ECC_KEY_FLAG_USAGE_SIGN | ECC_KEY_FLAG_USAGE_ECDH | ECC_KEY_FLAG_USAGE_VERIFY;
    }
    
    error_t result;
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_SE050
        case ECC_BACKEND_SE050:
            result = se050_generate_keypair(curve, flags, handle);
            break;
#endif
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            result = mbedtls_generate_keypair(curve, flags, handle);
            break;
#endif
        default:
            result = ERROR_NOT_SUPPORTED;
    }
    
    ECC_HAL_UNLOCK();
    return result;
}

error_t ecc_hal_import_public_key(ecc_curve_t curve, const uint8_t *public_key, 
                                   size_t key_len, ecc_key_handle_t *handle)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !handle) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    if (!ecc_hal_validate_public_key(curve, public_key, key_len)) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    error_t result;
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            result = mbedtls_import_public_key(curve, public_key, key_len, handle);
            break;
#endif
        default:
            /* SE050 不支持直接导入公钥，需要通过特殊指令 */
            result = ERROR_NOT_SUPPORTED;
    }
    
    ECC_HAL_UNLOCK();
    return result;
}

error_t ecc_hal_export_public_key(ecc_key_handle_t handle, uint8_t *public_key, size_t *key_len)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized) {
        ECC_HAL_UNLOCK();
        return ERROR_NOT_INITIALIZED;
    }
    
    if (!public_key || !key_len) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    error_t result;
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_SE050
        case ECC_BACKEND_SE050:
            result = se050_export_public_key(handle, public_key, key_len);
            break;
#endif
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            result = mbedtls_export_public_key(handle, public_key, key_len);
            break;
#endif
        default:
            result = ERROR_NOT_SUPPORTED;
    }
    
    ECC_HAL_UNLOCK();
    return result;
}

error_t ecc_hal_get_key_info(ecc_key_handle_t handle, ecc_key_info_t *info)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !info) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    ecc_key_slot_t *slot = find_key_slot(handle);
    if (!slot) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_KEY_HANDLE;
    }
    
    info->handle = slot->handle;
    info->curve = slot->curve;
    info->flags = slot->flags;
    info->key_size = ecc_hal_get_key_length(slot->curve);
    info->generation_date = 0; /* 可从后端获取 */
    
    ECC_HAL_UNLOCK();
    return OK;
}

error_t ecc_hal_delete_key(ecc_key_handle_t handle)
{
    if (!g_ecc_ctx.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    ECC_HAL_LOCK();
    
    /* 从缓存中移除 */
    ecc_hal_cache_invalidate(handle);
    
    error_t result;
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_SE050
        case ECC_BACKEND_SE050:
            result = se050_delete_key(handle);
            break;
#endif
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            result = mbedtls_delete_key(handle);
            break;
#endif
        default:
            result = ERROR_NOT_SUPPORTED;
    }
    
    ECC_HAL_UNLOCK();
    return result;
}

bool ecc_hal_is_valid_key_handle(ecc_key_handle_t handle)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || handle == ECC_INVALID_KEY_HANDLE) {
        ECC_HAL_UNLOCK();
        return false;
    }
    
    bool valid = find_key_slot(handle) != NULL;
    
    ECC_HAL_UNLOCK();
    return valid;
}

/******************************************************************************
 * ECDH API
 ******************************************************************************/

error_t ecc_hal_ecdh_compute_shared_secret(ecc_key_handle_t private_key_handle,
                                            ecc_key_handle_t public_key_handle,
                                            ecdh_shared_secret_t *shared_secret)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !shared_secret) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    /* 先导出公钥 */
    uint8_t pub_key[ECC_P256_PUB_KEY_LEN];
    size_t pub_len = sizeof(pub_key);
    
    error_t err = ecc_hal_export_public_key(public_key_handle, pub_key, &pub_len);
    if (err != OK) {
        ECC_HAL_UNLOCK();
        return err;
    }
    
    err = ecc_hal_ecdh_compute_with_raw_pubkey(private_key_handle, pub_key, pub_len, shared_secret);
    
    /* 安全清除临时公钥 */
    ecc_hal_secure_zero(pub_key, sizeof(pub_key));
    
    ECC_HAL_UNLOCK();
    return err;
}

error_t ecc_hal_ecdh_compute_with_raw_pubkey(ecc_key_handle_t private_key_handle,
                                              const uint8_t *public_key,
                                              size_t pub_key_len,
                                              ecdh_shared_secret_t *shared_secret)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !public_key || !shared_secret) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    error_t result;
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_SE050
        case ECC_BACKEND_SE050:
            result = se050_ecdh(private_key_handle, public_key, pub_key_len, shared_secret);
            break;
#endif
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            result = mbedtls_ecdh(private_key_handle, public_key, pub_key_len, shared_secret);
            break;
#endif
        default:
            result = ERROR_NOT_SUPPORTED;
    }
    
    ECC_HAL_UNLOCK();
    return result;
}

/******************************************************************************
 * ECDSA API
 ******************************************************************************/

error_t ecc_hal_ecdsa_sign(ecc_key_handle_t private_key_handle,
                            const uint8_t *digest,
                            size_t digest_len,
                            ecdsa_signature_t *signature)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !digest || !signature) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    error_t result;
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_SE050
        case ECC_BACKEND_SE050:
            result = se050_ecdsa_sign(private_key_handle, digest, digest_len, signature);
            break;
#endif
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            result = mbedtls_ecdsa_sign(private_key_handle, digest, digest_len, signature);
            break;
#endif
        default:
            result = ERROR_NOT_SUPPORTED;
    }
    
    ECC_HAL_UNLOCK();
    return result;
}

error_t ecc_hal_ecdsa_verify(ecc_key_handle_t public_key_handle,
                              const uint8_t *digest,
                              size_t digest_len,
                              const ecdsa_signature_t *signature)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !digest || !signature) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    error_t result;
    switch (g_ecc_ctx.active_backend) {
#if ENABLE_SE050
        case ECC_BACKEND_SE050:
            result = se050_ecdsa_verify(public_key_handle, digest, digest_len, signature);
            break;
#endif
#if ENABLE_MBEDTLS
        case ECC_BACKEND_MBEDTLS:
            result = mbedtls_ecdsa_verify(public_key_handle, digest, digest_len, signature);
            break;
#endif
        default:
            result = ERROR_NOT_SUPPORTED;
    }
    
    ECC_HAL_UNLOCK();
    return result;
}

error_t ecc_hal_ecdsa_verify_raw(const uint8_t *public_key,
                                  size_t pub_key_len,
                                  const uint8_t *digest,
                                  size_t digest_len,
                                  const ecdsa_signature_t *signature)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !public_key || !digest || !signature) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    /* 导入临时公钥句柄 */
    ecc_key_handle_t temp_handle;
    error_t err = ecc_hal_import_public_key(ECC_CURVE_P256, public_key, pub_key_len, &temp_handle);
    if (err != OK) {
        ECC_HAL_UNLOCK();
        return err;
    }
    
    /* 执行验签 */
    err = ecc_hal_ecdsa_verify(temp_handle, digest, digest_len, signature);
    
    /* 清理临时句柄 */
    ecc_hal_delete_key(temp_handle);
    
    ECC_HAL_UNLOCK();
    return err;
}

/******************************************************************************
 * 便捷 API 函数 (符合任务要求的命名)
 ******************************************************************************/

/**
 * @brief ECDH 密钥协商 (便捷函数)
 */
error_t ecc_hal_ecdh(ecc_key_handle_t private_key_handle,
                     const uint8_t *peer_public_key,
                     size_t peer_pub_len,
                     ecdh_shared_secret_t *shared_secret)
{
    return ecc_hal_ecdh_compute_with_raw_pubkey(private_key_handle, 
                                                 peer_public_key, 
                                                 peer_pub_len, 
                                                 shared_secret);
}

/**
 * @brief ECDSA 签名 (便捷函数)
 */
error_t ecc_hal_sign(ecc_key_handle_t private_key_handle,
                     const uint8_t *digest,
                     size_t digest_len,
                     ecdsa_signature_t *signature)
{
    return ecc_hal_ecdsa_sign(private_key_handle, digest, digest_len, signature);
}

/**
 * @brief ECDSA 验签 (便捷函数)
 */
error_t ecc_hal_verify(ecc_key_handle_t public_key_handle,
                       const uint8_t *digest,
                       size_t digest_len,
                       const ecdsa_signature_t *signature)
{
    return ecc_hal_ecdsa_verify(public_key_handle, digest, digest_len, signature);
}

/**
 * @brief 获取后端详细信息
 */
error_t ecc_hal_get_backend_info(char *name, size_t name_len,
                                 uint32_t *features,
                                 bool *hardware_accelerated)
{
    ECC_HAL_LOCK();
    
    if (!g_ecc_ctx.initialized || !name || !features || !hardware_accelerated) {
        ECC_HAL_UNLOCK();
        return ERROR_INVALID_PARAM;
    }
    
    /* 复制后端名称 */
    strncpy(name, g_ecc_ctx.caps.backend_name, name_len - 1);
    name[name_len - 1] = '\0';
    
    /* 设置特性位 */
    *features = 0;
    if (g_ecc_ctx.caps.p256_supported) *features |= 0x01;
    if (g_ecc_ctx.caps.p384_supported) *features |= 0x02;
    if (g_ecc_ctx.caps.ecdh_supported) *features |= 0x04;
    if (g_ecc_ctx.caps.ecdsa_sign_supported) *features |= 0x08;
    if (g_ecc_ctx.caps.ecdsa_verify_supported) *features |= 0x10;
    if (g_ecc_ctx.caps.side_channel_resistant) *features |= 0x20;
    
    *hardware_accelerated = g_ecc_ctx.caps.hardware_protection;
    
    ECC_HAL_UNLOCK();
    return OK;
}

/******************************************************************************
 * 增强功能: 密钥缓存管理
 ******************************************************************************/

/**
 * @brief 清除缓存
 */
static void ecc_hal_clear_cache(void)
{
    for (int i = 0; i < 4; i++) {
        g_ecc_ctx.cache[i].handle = ECC_INVALID_KEY_HANDLE;
        g_ecc_ctx.cache[i].slot = NULL;
        g_ecc_ctx.cache[i].last_access = 0;
    }
}

/**
 * @brief 从缓存移除特定句柄
 */
static void ecc_hal_cache_invalidate(ecc_key_handle_t handle)
{
    for (int i = 0; i < 4; i++) {
        if (g_ecc_ctx.cache[i].handle == handle) {
            g_ecc_ctx.cache[i].handle = ECC_INVALID_KEY_HANDLE;
            g_ecc_ctx.cache[i].slot = NULL;
            g_ecc_ctx.cache[i].last_access = 0;
        }
    }
}
