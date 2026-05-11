/******************************************************************************
 * @file    se050_crypto.c
 * @brief   SE050 安全元件加密适配器 (修复版 - 集成 mbedtls AES-GCM)
 * @author  YuleTech
 * @version 2.0.0
 * @date    2026-05-09
 * 
 * @note    CRITICAL FIX: 替换明文传输为真正的 AES-128-GCM 加密
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "se050_crypto.h"
#include "dkcs_error.h"

/* 引入 mbedtls 头文件 */
#include "mbedtls/cipher.h"
#include "mbedtls/gcm.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"

/******************************************************************************
 * 内部状态和上下文
 ******************************************************************************/
static mbedtls_entropy_context entropy_ctx;
static mbedtls_ctr_drbg_context ctr_drbg_ctx;
static int g_crypto_initialized = 0;

/******************************************************************************
 * 初始化加密子系统
 ******************************************************************************/
error_t se050_crypto_init(void)
{
    if (g_crypto_initialized) {
        return OK;
    }
    
    const char *pers = "se050_crypto";
    
    mbedtls_entropy_init(&entropy_ctx);
    mbedtls_ctr_drbg_init(&ctr_drbg_ctx);
    
    int ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg_ctx,
        mbedtls_entropy_func,
        &entropy_ctx,
        (const unsigned char *)pers,
        strlen(pers)
    );
    
    if (ret != 0) {
        return ERROR_CRYPTO_INIT_FAILED;
    }
    
    g_crypto_initialized = 1;
    return OK;
}

/******************************************************************************
 * 反初始化
 ******************************************************************************/
void se050_crypto_deinit(void)
{
    if (g_crypto_initialized) {
        mbedtls_ctr_drbg_free(&ctr_drbg_ctx);
        mbedtls_entropy_free(&entropy_ctx);
        g_crypto_initialized = 0;
    }
}

/******************************************************************************
 * AES-128-GCM 加密
 * CRITICAL FIX: 真正的加密实现，替换明文传输
 ******************************************************************************/
error_t ccc_encrypt_aes_gcm(
    const uint8_t *key,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *plaintext,
    size_t plaintext_len,
    uint8_t *ciphertext,
    size_t *ciphertext_len,
    uint8_t *tag,
    size_t tag_len)
{
    if (!g_crypto_initialized) {
        se050_crypto_init();
    }
    
    /* 参数验证 */
    if (key == NULL || iv == NULL || plaintext == NULL || ciphertext == NULL || tag == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (iv_len != 12) {
        return ERROR_INVALID_IV_LENGTH;
    }
    
    if (tag_len != 16) {
        return ERROR_INVALID_TAG_LENGTH;
    }
    
    /* 初始化 GCM 上下文 */
    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);
    
    /* 设置密钥 (AES-128) */
    int ret = mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return ERROR_CRYPTO_KEY_SETUP;
    }
    
    /* 执行加密 */
    ret = mbedtls_gcm_crypt_and_tag(
        &gcm_ctx,
        MBEDTLS_GCM_ENCRYPT,
        plaintext_len,
        iv, iv_len,
        aad, aad_len,
        plaintext, ciphertext,
        tag_len, tag
    );
    
    mbedtls_gcm_free(&gcm_ctx);
    
    if (ret != 0) {
        return ERROR_CRYPTO_ENCRYPT_FAILED;
    }
    
    *ciphertext_len = plaintext_len;
    
    return OK;
}

/******************************************************************************
 * AES-128-GCM 解密
 ******************************************************************************/
error_t ccc_decrypt_aes_gcm(
    const uint8_t *key,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    uint8_t *plaintext,
    size_t *plaintext_len,
    const uint8_t *tag,
    size_t tag_len)
{
    if (!g_crypto_initialized) {
        se050_crypto_init();
    }
    
    /* 参数验证 */
    if (key == NULL || iv == NULL || ciphertext == NULL || plaintext == NULL || tag == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (iv_len != 12) {
        return ERROR_INVALID_IV_LENGTH;
    }
    
    if (tag_len != 16) {
        return ERROR_INVALID_TAG_LENGTH;
    }
    
    /* 初始化 GCM 上下文 */
    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);
    
    /* 设置密钥 */
    int ret = mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return ERROR_CRYPTO_KEY_SETUP;
    }
    
    /* 执行解密并验证 Tag */
    ret = mbedtls_gcm_auth_decrypt(
        &gcm_ctx,
        ciphertext_len,
        iv, iv_len,
        aad, aad_len,
        tag, tag_len,
        ciphertext, plaintext
    );
    
    mbedtls_gcm_free(&gcm_ctx);
    
    if (ret != 0) {
        /* Tag 验证失败 */
        return ERROR_CRYPTO_AUTH_FAILED;
    }
    
    *plaintext_len = ciphertext_len;
    
    return OK;
}

/******************************************************************************
 * 生成随机密钥
 ******************************************************************************/
error_t ccc_generate_key(uint8_t *key, size_t key_len)
{
    if (!g_crypto_initialized) {
        se050_crypto_init();
    }
    
    if (key == NULL || key_len != 16) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 使用高质量随机数生成器 */
    int ret = mbedtls_ctr_drbg_random(&ctr_drbg_ctx, key, key_len);
    if (ret != 0) {
        return ERROR_CRYPTO_RANDOM_GENERATION;
    }
    
    return OK;
}

/******************************************************************************
 * 生成随机 IV
 ******************************************************************************/
error_t ccc_generate_iv(uint8_t *iv, size_t iv_len)
{
    if (!g_crypto_initialized) {
        se050_crypto_init();
    }
    
    if (iv == NULL || iv_len != 12) {
        return ERROR_INVALID_PARAM;
    }
    
    int ret = mbedtls_ctr_drbg_random(&ctr_drbg_ctx, iv, iv_len);
    if (ret != 0) {
        return ERROR_CRYPTO_RANDOM_GENERATION;
    }
    
    return OK;
}

/******************************************************************************
 * 密钥派生 (HKDF-SHA256)
 ******************************************************************************/
error_t ccc_derive_key(
    const uint8_t *shared_secret,
    size_t secret_len,
    const uint8_t *salt,
    size_t salt_len,
    const uint8_t *info,
    size_t info_len,
    uint8_t *derived_key,
    size_t derived_key_len)
{
    if (!g_crypto_initialized) {
        se050_crypto_init();
    }
    
    if (shared_secret == NULL || derived_key == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 使用 mbedtls HKDF */
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        return ERROR_CRYPTO_HKDF_FAILED;
    }
    
    int ret = mbedtls_hkdf(
        md_info,
        salt, salt_len,
        shared_secret, secret_len,
        info, info_len,
        derived_key, derived_key_len
    );
    
    if (ret != 0) {
        return ERROR_CRYPTO_HKDF_FAILED;
    }
    
    return OK;
}

/******************************************************************************
 * 低级别安全内存拷贝 (防止优化)
 ******************************************************************************/
static volatile uint8_t *secure_memset(volatile uint8_t *ptr, uint8_t value, size_t num)
{
    volatile uint8_t *p = ptr;
    while (num--) {
        *p++ = value;
    }
    return ptr;
}

/******************************************************************************
 * 安全清除敏感数据
 ******************************************************************************/
void ccc_secure_zero(void *ptr, size_t len)
{
    if (ptr != NULL && len > 0) {
        secure_memset((volatile uint8_t *)ptr, 0, len);
    }
}

/******************************************************************************
 * 保留旧版本函数以保持向后兼容 (但已安全实现)
 ******************************************************************************/
error_t se050_encrypt_data(
    const uint8_t *key,
    const uint8_t *plaintext,
    size_t plaintext_len,
    uint8_t *ciphertext,
    size_t *ciphertext_len)
{
    /* 生成随机 IV */
    uint8_t iv[12];
    error_t ret = ccc_generate_iv(iv, sizeof(iv));
    if (ret != OK) {
        return ret;
    }
    
    /* 计算完整的密文长度: IV (12) + ciphertext + tag (16) */
    if (*ciphertext_len < 12 + plaintext_len + 16) {
        return ERROR_BUFFER_TOO_SMALL;
    }
    
    /* 写入 IV */
    memcpy(ciphertext, iv, 12);
    
    uint8_t tag[16];
    size_t out_len;
    
    /* 加密 */
    ret = ccc_encrypt_aes_gcm(
        key,
        iv, 12,
        NULL, 0,  /* 无 AAD */
        plaintext, plaintext_len,
        ciphertext + 12, &out_len,
        tag, 16
    );
    
    if (ret != OK) {
        ccc_secure_zero(iv, sizeof(iv));
        ccc_secure_zero(tag, sizeof(tag));
        return ret;
    }
    
    /* 写入 Tag */
    memcpy(ciphertext + 12 + plaintext_len, tag, 16);
    *ciphertext_len = 12 + plaintext_len + 16;
    
    /* 清除敏感数据 */
    ccc_secure_zero(iv, sizeof(iv));
    ccc_secure_zero(tag, sizeof(tag));
    
    return OK;
}

error_t se050_decrypt_data(
    const uint8_t *key,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    uint8_t *plaintext,
    size_t *plaintext_len)
{
    if (ciphertext_len < 12 + 16) {
        return ERROR_INVALID_CIPHERTEXT;
    }
    
    const uint8_t *iv = ciphertext;
    const uint8_t *encrypted_data = ciphertext + 12;
    size_t encrypted_len = ciphertext_len - 12 - 16;
    const uint8_t *tag = ciphertext + ciphertext_len - 16;
    
    if (*plaintext_len < encrypted_len) {
        return ERROR_BUFFER_TOO_SMALL;
    }
    
    error_t ret = ccc_decrypt_aes_gcm(
        key,
        iv, 12,
        NULL, 0,
        encrypted_data, encrypted_len,
        plaintext, plaintext_len,
        tag, 16
    );
    
    return ret;
}
