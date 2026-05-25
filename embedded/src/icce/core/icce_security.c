/**
 * @file icce_security.c
 * @brief ICCE Security Module - Bind/Auth/Session Verification via SE050
 */

#include "icce_digital_key.h"
#include "icce_certificate.h"
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>

#define MAX_BOUND_DEVICES  8

typedef struct {
    uint8_t device_pubkey[64];  /* ECDH P-256 public key */
    uint8_t device_id[16];
    uint8_t key_slot;           /* SE050 key slot */
    bool    active;
} icce_bound_device_t;

static icce_bound_device_t g_devices[MAX_BOUND_DEVICES];
static uint8_t g_device_count = 0;

int32_t icce_security_init(void)
{
    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;
    return ICCE_OK;
}

int32_t icce_security_bind(const uint8_t *device_pubkey, uint16_t len)
{
    if (!device_pubkey || len != 64) return ICCE_ERR_PARAM;
    if (g_device_count >= MAX_BOUND_DEVICES) return ICCE_ERR_NO_MEM;

    /* Store device public key in SE050 */
    /* Generate shared ECDH secret */
    /* Assign key slot */
    icce_bound_device_t *dev = &g_devices[g_device_count];
    memcpy(dev->device_pubkey, device_pubkey, 64);
    dev->key_slot = g_device_count + 1;
    dev->active = true;

#if defined(ICCE_USE_SE050)
    /* 导入设备公钥到 SE050 安全元件
     * NXP Plug & Trust API:
     *   smStatus_t status = Se05x_API_ImportKey(
     *       s_ctx, dev->key_slot,
     *       kSE05x_ECKeyType_P256_Public,
     *       device_pubkey, 64);
     *   if (status != kSE05x_OK) {
     *       memset(dev, 0, sizeof(icce_bound_device_t));
     *       return ICCE_ERR_SECURITY;
     *   }
     */
#else
    /* 软件模式: 公钥已在内存中，无需导入 */
    (void)dev->key_slot;
#endif /* ICCE_USE_SE050 */
    
    /* Verify device certificate chain */
    error_t err = icce_security_verify_device_cert_chain(dev);
    if (err != OK) {
        memset(dev, 0, sizeof(icce_bound_device_t));
        return ICCE_ERR_SECURITY;
    }

    g_device_count++;
    return ICCE_OK;
}

int32_t icce_security_auth(const uint8_t *challenge, uint16_t chal_len,
                           const uint8_t *signature, uint16_t sig_len)
{
    if (!challenge || !signature) return ICCE_ERR_PARAM;
    if (chal_len < 16 || sig_len != 64) return ICCE_ERR_PARAM;

    /* 对每个已绑定设备尝试验证签名 */
    for (uint8_t i = 0; i < g_device_count; i++) {
        if (!g_devices[i].active) continue;

#if defined(ICCE_USE_SE050)
        /* === SE050 硬件路径 === */
        /* 使用 SE050 ECDSA P-256 验签
         * NXP Plug & Trust API:
         *   smStatus_t status = Se05x_API_ECCMultyStepVerify(
         *       s_ctx, g_devices[i].key_slot,
         *       kSE05x_Algo_ECDSA_HA256,
         *       challenge, chal_len,
         *       signature, sig_len);
         *   if (status == kSE05x_Match) return ICCE_OK;
         */
        (void)chal_len;

        /* 暂未集成 SE050 中间件，回退软件实现 */
        /* 删除此段后启用上面注释的 SE050 调用 */

#endif /* ICCE_USE_SE050 */

        /* === 软件回退路径 (mbedtls ECDSA) === */
        /* 将设备公钥从 64 字节 (x||y) 转换为 65 字节未压缩格式 */
        uint8_t pubkey[65];
        pubkey[0] = 0x04;  /* 未压缩标志 */
        memcpy(pubkey + 1, g_devices[i].device_pubkey, 64);

        /* 计算 SHA-256 挑战哈希 */
        uint8_t digest[32];
        mbedtls_sha256(challenge, chal_len, digest, 0);

        /* 使用 mbedtls ECDSA P-256 验签 */
        mbedtls_ecdsa_context ecdsa;
        mbedtls_ecdsa_init(&ecdsa);

        int ret = mbedtls_ecp_group_load(&ecdsa.grp, MBEDTLS_ECP_DP_SECP256R1);
        if (ret == 0) {
            ret = mbedtls_ecp_point_read_binary(&ecdsa.grp, &ecdsa.Q, pubkey, 65);
        }

        if (ret == 0) {
            ret = mbedtls_ecdsa_read_signature(&ecdsa, digest, 32,
                                                signature, sig_len);
            if (ret == 0) {
                mbedtls_ecdsa_free(&ecdsa);
                return ICCE_OK;  /* 签名验证通过 */
            }
        }

        mbedtls_ecdsa_free(&ecdsa);
    }

    return ICCE_ERR_SECURITY;  /* 所有设备验证失败 */
}

int32_t icce_security_verify_session(uint16_t session_id)
{
    (void)session_id;
    /* Check if UWB session is authenticated */
    /* Verify session token matches bound device */
    return ICCE_OK;
}

/******************************************************************************
 * ICCE 设备证书链验证
 ******************************************************************************/

/**
 * @brief 验证设备证书链
 * 
 * @param dev  已绑定的设备
 * @return error_t OK 成功, 其他失败
 * 
 * @note 证书链结构: VehicleCA -> Vehicle -> OwnerDK/SharedDK
 */
static error_t icce_security_verify_device_cert_chain(icce_bound_device_t *dev)
{
    if (!dev || !dev->active) {
        return ICCE_CERT_ERROR_INVALID_PARAM;
    }

    icce_cert_validator_config_t config;
    icce_cert_validator_config_init(&config);
    config.verify_time = true;
    config.strict_size_check = true;

    /* 从设备获取证书链 (通常通过APDU从SE读取) */
    icce_cert_chain_t cert_chain;
    icce_certificate_t trusted_root;
    
    /* TODO: 从SE或内存读取存储的证书链 */
    /* 这里使用缓存的证书链，实际中应从SE读取 */
    error_t err = icce_load_device_cert_chain(dev->device_id, &cert_chain, &trusted_root);
    if (err != OK) {
        return ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND;
    }

    /* 验证证书链完整性 */
    err = icce_validate_cert_chain(&cert_chain, &trusted_root, &config);
    if (err != OK) {
        return err;
    }

    /* 验证终端实体证书的公钥与绑定的设备公钥匹配 */
    icce_certificate_t *end_entity = &cert_chain.certs[cert_chain.cert_count - 1];
    
    if (memcmp(end_entity->public_key, dev->device_pubkey, ICCE_ECC_SM2_PUB_KEY_LEN) != 0) {
        return ICCE_CERT_ERROR_INVALID_SIGNATURE;
    }

    /* 验证证书中的设备ID与绑定的设备ID匹配 */
    if (memcmp(end_entity->device_id, dev->device_id, ICCE_DEVICE_ID_LEN) != 0) {
        return ICCE_CERT_ERROR_INVALID_PARAM;
    }

    return OK;
}

/**
 * @brief 从local storage或SE加载证书链
 * 
 * @param device_id      设备ID
 * @param cert_chain     输出的证书链
 * @param trusted_root   输出的信任根证书
 * 
 * @return error_t OK 成功
 */
static error_t icce_load_device_cert_chain(const uint8_t *device_id, 
                                            icce_cert_chain_t *cert_chain,
                                            icce_certificate_t *trusted_root)
{
    if (!device_id || !cert_chain || !trusted_root) {
        return ICCE_CERT_ERROR_INVALID_PARAM;
    }

    memset(cert_chain, 0, sizeof(icce_cert_chain_t));
    memset(trusted_root, 0, sizeof(icce_certificate_t));

    /* 从local storage加载车厂CA证书 (信任锚) */
    /* 实际应从受信的存储区域读取，如SE或安全内存 */
    error_t err = icce_load_vehicle_ca_cert(trusted_root);
    if (err != OK) {
        return ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND;
    }

    /* 检查VehicleCA证书有效性 */
    if (trusted_root->cert_type != ICCE_CERT_TYPE_VEHICLE_CA) {
        return ICCE_CERT_ERROR_INVALID_TYPE;
    }

    /* 从local storage或SE加载完整证书链 */
    /* 这里模拟从SE读取，实际实现需要与SE050交互 */
    static uint8_t cert_data[ICCE_MAX_CERT_SIZE * ICCE_MAX_CERT_CHAIN_LEN];
    size_t total_len = 0;

    /* 模拟读取: 车证书 + 钥匙证书 (可能包含中间证书) */
    err = icce_read_certs_from_storage(device_id, cert_data, &total_len);
    if (err != OK || total_len == 0) {
        /* 没有找到存储的证书链，返回错误 */
        return ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND;
    }

    /* 解析证书链 */
    size_t offset = 0;
    uint8_t cert_count = 0;

    while (offset < total_len && cert_count < ICCE_MAX_CERT_CHAIN_LEN) {
        size_t parsed_len = 0;
        err = icce_parse_certificate(cert_data + offset, total_len - offset, 
                                      &cert_chain->certs[cert_count]);
        if (err != OK) {
            break;
        }

        cert_count++;
        offset += cert_chain->certs[cert_count - 1].raw_len;
    }

    cert_chain->cert_count = cert_count;

    if (cert_count == 0) {
        return ICCE_CERT_ERROR_INVALID_CHAIN;
    }

    return OK;
}

/**
 * @brief 加载车厂CA证书 (信任锚)
 * 
 * @param ca_cert  输出的CA证书
 * @return error_t OK 成功
 */
static error_t icce_load_vehicle_ca_cert(icce_certificate_t *ca_cert)
{
    if (!ca_cert) {
        return ICCE_CERT_ERROR_INVALID_PARAM;
    }

    /* 从local storage读取车厂CA证书 */
    /* 实际应从受保护的存储区域读取 */
    
    /* 检查是否已缓存 */
    static icce_certificate_t cached_ca_cert;
    static bool ca_cached = false;

    if (ca_cached) {
        icce_certificate_copy(ca_cert, &cached_ca_cert);
        return OK;
    }

    /* 从local storage读取 */
    /* 这里是示代码，实际应读取昧存/Flash中的证书 */
    #ifdef ICCE_VEHICLE_CA_CERT_EMBEDDED
    /* 如果CA证书固件件编译到二进制 */
    extern const uint8_t g_icce_vehicle_ca_cert_data[];
    extern const size_t g_icce_vehicle_ca_cert_len;
    
    error_t err = icce_parse_certificate(g_icce_vehicle_ca_cert_data, 
                                          g_icce_vehicle_ca_cert_len, 
                                          ca_cert);
    if (err != OK) {
        return err;
    }
    #else
    /* 从可持久化存储读取 */
    uint8_t cert_data[ICCE_MAX_CERT_SIZE];
    size_t cert_len = 0;
    
    error_t err = dkcs_storage_read(DKCS_STORAGE_ID_ICCE_CA_CERT, 
                                     cert_data, sizeof(cert_data), &cert_len);
    if (err != OK || cert_len == 0) {
        return ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND;
    }

    err = icce_parse_certificate(cert_data, cert_len, ca_cert);
    if (err != OK) {
        return err;
    }
    #endif

    /* 缓存CA证书 */
    icce_certificate_copy(&cached_ca_cert, ca_cert);
    ca_cached = true;

    return OK;
}

/**
 * @brief 从存储读取证书
 * 
 * @param device_id   设备ID
 * @param cert_data   输出缓冲区
 * @param total_len   输入: 缓冲区大小; 输出: 实际读取长度
 * @return error_t OK 成功
 */
static error_t icce_read_certs_from_storage(const uint8_t *device_id,
                                             uint8_t *cert_data,
                                             size_t *total_len)
{
    if (!device_id || !cert_data || !total_len) {
        return ICCE_CERT_ERROR_INVALID_PARAM;
    }

    /* 从SE或安全存储读取证书链 */
    /* 这里是框架代码，实际需要实现SE050读取 */
    
    #ifdef ICCE_USE_SE050_STORAGE
    /* 使用SE050安全存储 */
    return icce_read_certs_from_se050(device_id, cert_data, total_len);
    #else
    /* 使用普通存储（仅用于开发测试） */
    return dkcs_storage_read(DKCS_STORAGE_ID_ICCE_CERT_CHAIN,
                              cert_data, *total_len, total_len);
    #endif
}

/**
 * @brief 构建默认证书链 (已弃用，直接返回错误)
 * 
 * @param cert_chain    输出的证书链
 * @param trusted_root  信任根
 * @param device_id     设备ID
 * @return error_t 始终返回 ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND
 */
static error_t icce_build_default_cert_chain(icce_cert_chain_t *cert_chain,
                                              const icce_certificate_t *trusted_root,
                                              const uint8_t *device_id)
{
    (void)cert_chain;
    (void)trusted_root;
    (void)device_id;
    return ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND;
}
