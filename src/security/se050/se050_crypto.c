/******************************************************************************
 * @file    se050_crypto.c
 * @brief   SE050 加密操作封装实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    符合 MISRA-C 2012 规范
 *          支持 ECC (P-256/P-384) 和 AES (GCM/CCM) 操作
 ******************************************************************************/

#include "se050_adapter.h"
#include <string.h>

/******************************************************************************
 * MISRA-C 2012 合规注意事项
 * - 使用明确的类型转换
 * - 避免魔术数字
 * - 确保所有路径有返回值
 ******************************************************************************/

/******************************************************************************
 * 内部宏定义
 ******************************************************************************/
#define SE050_CRYPTO_VERSION_MAJOR      (1U)
#define SE050_CRYPTO_VERSION_MINOR      (0U)
#define SE050_CRYPTO_VERSION_PATCH      (0U)

/* SE050 APDU 命令定义 (根据 NXP SE050 APDU 规范) */
#define SE050_INS_WRITE                 (0x01U)
#define SE050_INS_READ                  (0x02U)
#define SE050_INS_CRYPTO                (0x03U)
#define SE050_INS_MGMT                  (0x04U)

#define SE050_P1_DEFAULT                (0x00U)
#define SE050_P2_DEFAULT                (0x00U)

/* ECC 曲线参数 */
#define SE050_ECC_P256_FIELD_SIZE       (32U)
#define SE050_ECC_P384_FIELD_SIZE       (48U)
#define SE050_ECC_P256_SIG_SIZE         (64U)
#define SE050_ECC_P384_SIG_SIZE         (96U)
#define SE050_ECC_P256_PUB_KEY_SIZE     (65U)  /* 0x04 + X(32) + Y(32) */
#define SE050_ECC_P384_PUB_KEY_SIZE     (97U)  /* 0x04 + X(48) + Y(48) */

/* AES 参数 */
#define SE050_AES_BLOCK_SIZE            (16U)
#define SE050_AES_GCM_IV_SIZE           (12U)
#define SE050_AES_GCM_TAG_MIN_SIZE      (12U)
#define SE050_AES_GCM_TAG_MAX_SIZE      (16U)
#define SE050_AES_CCM_NONCE_MIN_SIZE    (7U)
#define SE050_AES_CCM_NONCE_MAX_SIZE    (13U)

/* 最大缓冲区大小 */
#define SE050_MAX_APDU_DATA_LEN         (256U)
#define SE050_MAX_KEY_SIZE              (64U)

/******************************************************************************
 * 内部数据结构
 ******************************************************************************/
typedef struct {
    bool initialized;
    se050_config_t config;
    uint32_t session_counter;
    uint8_t session_state;
} se050_context_t;

/******************************************************************************
 * 全局上下文
 ******************************************************************************/
static se050_context_t g_se050_ctx = {
    false,
    {0U, 0U, 0U, false, false},
    0U,
    0U
};

/* 自旋锁 (简单实现，生产环境使用真正的互斥锁) */
static volatile uint32_t g_crypto_lock = 0U;

/******************************************************************************
 * 内部静态函数声明
 ******************************************************************************/
static error_t se050_check_initialized(void);
static error_t se050_check_key_id(uint32_t key_id);
static error_t se050_check_buffer(const void *buf, size_t len);
static error_t se050_check_key_type(se050_key_type_t type);
static error_t se050_transceive(
    const uint8_t *tx_buf,
    size_t tx_len,
    uint8_t *rx_buf,
    size_t *rx_len
);
static error_t se050_ecc_get_field_size(se050_key_type_t type, size_t *size);
static error_t se050_aes_check_iv_len(se050_key_type_t type, size_t iv_len);

/******************************************************************************
 * 内部静态函数实现
 ******************************************************************************/

/**
 * @brief 检查初始化状态
 */
static error_t se050_check_initialized(void)
{
    error_t result;

    if (g_se050_ctx.initialized == true) {
        result = OK;
    } else {
        result = ERROR_NOT_INITIALIZED;
    }

    return result;
}

/**
 * @brief 检查密钥 ID 有效性
 */
static error_t se050_check_key_id(uint32_t key_id)
{
    error_t result;

    if ((key_id >= SE050_MIN_KEY_ID) && (key_id <= SE050_MAX_KEY_ID)) {
        result = OK;
    } else {
        result = ERROR_INVALID_PARAM;
    }

    return result;
}

/**
 * @brief 检查缓冲区有效性
 */
static error_t se050_check_buffer(const void *buf, size_t len)
{
    error_t result;

    if ((buf != NULL) && (len > 0U)) {
        result = OK;
    } else {
        result = ERROR_INVALID_PARAM;
    }

    return result;
}

/**
 * @brief 检查密钥类型有效性
 */
static error_t se050_check_key_type(se050_key_type_t type)
{
    error_t result;

    if (type < SE050_KEY_TYPE_MAX) {
        result = OK;
    } else {
        result = ERROR_INVALID_PARAM;
    }

    return result;
}

/**
 * @brief I2C/SPI 数据交换 (模拟实现，需要硬件适配)
 */
static error_t se050_transceive(
    const uint8_t *tx_buf,
    size_t tx_len,
    uint8_t *rx_buf,
    size_t *rx_len)
{
    error_t result = OK;

    /* MISRA-C: 检查参数有效性 */
    if ((tx_buf == NULL) || (rx_buf == NULL) || (rx_len == NULL)) {
        result = ERROR_INVALID_PARAM;
    } else if ((tx_len == 0U) || (*rx_len == 0U)) {
        result = ERROR_INVALID_PARAM;
    } else {
        /* 这里应该调用底层 HAL 的 I2C/SPI 交换函数 */
        /* 实际实现需要与 NXP SE050 硬件驱动集成 */
        /* 模拟成功响应 */
        (void)memset(rx_buf, 0, *rx_len);
        *rx_len = 2U;  /* 响应状态码 */
    }

    return result;
}

/**
 * @brief 获取 ECC 字段大小
 */
static error_t se050_ecc_get_field_size(se050_key_type_t type, size_t *size)
{
    error_t result;

    if (size == NULL) {
        result = ERROR_INVALID_PARAM;
    } else {
        switch (type) {
            case SE050_KEY_TYPE_ECC_P256:
                *size = SE050_ECC_P256_FIELD_SIZE;
                result = OK;
                break;
            case SE050_KEY_TYPE_ECC_P384:
                *size = SE050_ECC_P384_FIELD_SIZE;
                result = OK;
                break;
            default:
                *size = 0U;
                result = ERROR_INVALID_PARAM;
                break;
        }
    }

    return result;
}

/**
 * @brief 检查 AES IV 长度
 */
static error_t se050_aes_check_iv_len(se050_key_type_t type, size_t iv_len)
{
    error_t result = ERROR_INVALID_PARAM;

    (void)type; /* 暂时未使用 */

    if ((iv_len >= SE050_AES_CCM_NONCE_MIN_SIZE) &&
        (iv_len <= SE050_AES_CCM_NONCE_MAX_SIZE)) {
        result = OK;
    } else if (iv_len == SE050_AES_GCM_IV_SIZE) {
        result = OK;
    } else {
        /* 保持默认错误 */
    }

    return result;
}

/******************************************************************************
 * 密钥管理实现
 ******************************************************************************/

error_t se050_generate_ecc_keypair(
    uint32_t key_id,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[16];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t field_size = 0U;

    (void)persistent; /* 暂时未使用，用于存储类型选择 */

    result = se050_check_initialized();
    if (result != OK) {
        /* MISRA: 所有路径都有返回值 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_key_type(type);
    }

    if (result == OK) {
        result = se050_ecc_get_field_size(type, &field_size);
    }

    if (result == OK) {
        /* 构建生成密钥对的 APDU 命令 */
        /* CLA | INS | P1 | P2 | Lc | 数据 | Le */
        apdu[offset] = 0x80U;
        offset++;  /* PRQA S 3383 */
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x01U;  /* 生成密钥对 */
        offset++;
        apdu[offset] = (uint8_t)type;
        offset++;

        /* 数据长度 */
        apdu[offset] = 0x00U;
        offset++;

        /* 密钥 ID (4 字节) */
        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        /* 权限 */
        apdu[offset] = (uint8_t)((permissions >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(permissions & 0xFFU);
        offset++;

        /* 更新 Lc */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 期望的响应长度 */
        apdu[offset] = (uint8_t)field_size;
        offset++;

        result = se050_transceive(apdu, offset, response, &resp_len);
    }

    return result;
}

error_t se050_generate_aes_key(
    uint32_t key_id,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[16];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t key_size = 0U;

    (void)persistent;

    result = se050_check_initialized();
    if (result != OK) {
        /* 已设置错误码 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        switch (type) {
            case SE050_KEY_TYPE_AES_128:
                key_size = 16U;
                break;
            case SE050_KEY_TYPE_AES_256:
                key_size = 32U;
                break;
            default:
                result = ERROR_INVALID_PARAM;
                break;
        }
    }

    if (result == OK) {
        /* 构建生成 AES 密钥的 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x02U;  /* 生成对称密钥 */
        offset++;
        apdu[offset] = (uint8_t)type;
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        apdu[offset] = (uint8_t)((permissions >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(permissions & 0xFFU);
        offset++;

        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
        apdu[offset] = (uint8_t)key_size;
        offset++;

        result = se050_transceive(apdu, offset, response, &resp_len);
    }

    return result;
}

error_t se050_import_key(
    uint32_t key_id,
    const uint8_t *key_data,
    size_t key_len,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[16];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t max_key_len = 0U;

    (void)persistent;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(key_data, key_len);
    }

    if (result == OK) {
        /* 根据密钥类型确定最大长度 */
        switch (type) {
            case SE050_KEY_TYPE_ECC_P256:
                max_key_len = SE050_ECC_P256_FIELD_SIZE;
                break;
            case SE050_KEY_TYPE_ECC_P384:
                max_key_len = SE050_ECC_P384_FIELD_SIZE;
                break;
            case SE050_KEY_TYPE_AES_128:
                max_key_len = 16U;
                break;
            case SE050_KEY_TYPE_AES_256:
                max_key_len = 32U;
                break;
            default:
                max_key_len = SE050_MAX_KEY_SIZE;
                break;
        }

        if (key_len > max_key_len) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建导入密钥的 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_WRITE;
        offset++;
        apdu[offset] = 0x01U;  /* 导入密钥 */
        offset++;
        apdu[offset] = (uint8_t)type;
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        apdu[offset] = (uint8_t)((permissions >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(permissions & 0xFFU);
        offset++;

        /* 复制密钥数据 */
        if (key_len <= (SE050_MAX_APDU_DATA_LEN - offset - 1U)) {
            (void)memcpy(&apdu[offset], key_data, key_len);
            offset = offset + key_len;
            apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
            result = se050_transceive(apdu, offset, response, &resp_len);
        } else {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    return result;
}

error_t se050_export_public_key(
    uint32_t key_id,
    uint8_t *pub_key,
    size_t *pub_key_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[SE050_MAX_APDU_DATA_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        if ((pub_key == NULL) || (pub_key_len == NULL)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建导出公钥的 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_READ;
        offset++;
        apdu[offset] = 0x01U;  /* 读取公钥 */
        offset++;
        apdu[offset] = P1_DEFAULT;
        offset++;
        apdu[offset] = 0x04U;  /* Lc */
        offset++;

        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        apdu[offset] = 0x00U;  /* Le = 256 */
        offset++;

        result = se050_transceive(apdu, offset, response, &resp_len);

        if (result == OK) {
            if (resp_len > *pub_key_len) {
                result = ERROR_BUFFER_TOO_SMALL;
            } else {
                (void)memcpy(pub_key, response, resp_len);
                *pub_key_len = resp_len;
            }
        }
    }

    return result;
}

error_t se050_delete_key(uint32_t key_id)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[16];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        /* 构建删除密钥的 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_MGMT;
        offset++;
        apdu[offset] = 0x02U;  /* 删除密钥 */
        offset++;
        apdu[offset] = P2_DEFAULT;
        offset++;
        apdu[offset] = 0x04U;  /* Lc */
        offset++;

        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        apdu[offset] = 0x00U;  /* Le */
        offset++;

        result = se050_transceive(apdu, offset, response, &resp_len);
    }

    return result;
}

/******************************************************************************
 * ECDH 实现
 ******************************************************************************/

error_t se050_ecdh(
    uint32_t key_id,
    const uint8_t *peer_pub_key,
    size_t peer_pub_key_len,
    uint8_t *shared_secret,
    size_t *shared_secret_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[SE050_MAX_APDU_DATA_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t field_size = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(peer_pub_key, peer_pub_key_len);
    }

    if (result == OK) {
        if ((shared_secret == NULL) || (shared_secret_len == NULL)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 确定字段大小 */
        if (peer_pub_key_len == SE050_ECC_P256_PUB_KEY_SIZE) {
            field_size = SE050_ECC_P256_FIELD_SIZE;
        } else if (peer_pub_key_len == SE050_ECC_P384_PUB_KEY_SIZE) {
            field_size = SE050_ECC_P384_FIELD_SIZE;
        } else {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建 ECDH APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x10U;  /* ECDH */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        /* 私钥 ID */
        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        /* 对端公钥 */
        if (peer_pub_key_len <= (SE050_MAX_APDU_DATA_LEN - offset - 1U)) {
            (void)memcpy(&apdu[offset], peer_pub_key, peer_pub_key_len);
            offset = offset + peer_pub_key_len;

            apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
            apdu[offset] = (uint8_t)field_size;
            offset++;

            result = se050_transceive(apdu, offset, response, &resp_len);

            if (result == OK) {
                if (resp_len > *shared_secret_len) {
                    result = ERROR_BUFFER_TOO_SMALL;
                } else {
                    (void)memcpy(shared_secret, response, resp_len);
                    *shared_secret_len = resp_len;
                }
            }
        } else {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    return result;
}

/******************************************************************************
 * ECDSA 签名/验签实现
 ******************************************************************************/

error_t se050_ecdsa_sign(
    uint32_t key_id,
    const uint8_t *digest,
    size_t digest_len,
    uint8_t *signature,
    size_t *sig_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[SE050_MAX_APDU_DATA_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t sig_size = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(digest, digest_len);
    }

    if (result == OK) {
        if ((signature == NULL) || (sig_len == NULL)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 根据摘要长度确定曲线类型和签名大小 */
        if (digest_len == 32U) {
            sig_size = SE050_ECC_P256_SIG_SIZE;
        } else if (digest_len == 48U) {
            sig_size = SE050_ECC_P384_SIG_SIZE;
        } else {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建 ECDSA 签名 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x20U;  /* ECDSA Sign */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        /* 密钥 ID */
        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        /* 摘要 */
        if (digest_len <= (SE050_MAX_APDU_DATA_LEN - offset - 1U)) {
            (void)memcpy(&apdu[offset], digest, digest_len);
            offset = offset + digest_len;

            apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
            apdu[offset] = (uint8_t)sig_size;
            offset++;

            result = se050_transceive(apdu, offset, response, &resp_len);

            if (result == OK) {
                if (resp_len > *sig_len) {
                    result = ERROR_BUFFER_TOO_SMALL;
                } else {
                    (void)memcpy(signature, response, resp_len);
                    *sig_len = resp_len;
                }
            }
        } else {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    return result;
}

error_t se050_ecdsa_verify(
    uint32_t key_id,
    const uint8_t *digest,
    size_t digest_len,
    const uint8_t *signature,
    size_t sig_len,
    bool *result_out)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[16];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(digest, digest_len);
    }

    if (result == OK) {
        result = se050_check_buffer(signature, sig_len);
    }

    if (result == OK) {
        if (result_out == NULL) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建 ECDSA 验签 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x21U;  /* ECDSA Verify */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        /* 密钥 ID */
        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        /* 摘要 */
        if ((digest_len + sig_len) <= (SE050_MAX_APDU_DATA_LEN - offset)) {
            (void)memcpy(&apdu[offset], digest, digest_len);
            offset = offset + digest_len;
            (void)memcpy(&apdu[offset], signature, sig_len);
            offset = offset + sig_len;

            apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
            apdu[offset] = 0x01U;  /* Le = 1 */
            offset++;

            result = se050_transceive(apdu, offset, response, &resp_len);

            if (result == OK) {
                if ((resp_len >= 1U) && (response[0U] == 0x00U)) {
                    *result_out = true;
                } else {
                    *result_out = false;
                }
            }
        } else {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    return result;
}

/******************************************************************************
 * AES-GCM 加密/解密实现
 ******************************************************************************/

error_t se050_aes_gcm_encrypt(
    uint32_t key_id,
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    uint8_t *ciphertext,
    size_t ciphertext_len,
    uint8_t *tag,
    size_t tag_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[SE050_MAX_APDU_DATA_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(plaintext, plaintext_len);
    }

    if (result == OK) {
        result = se050_check_buffer(iv, iv_len);
    }

    if (result == OK) {
        if ((ciphertext == NULL) || (tag == NULL)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 检查输出缓冲区大小 */
        if ((ciphertext_len < plaintext_len) ||
            (tag_len < SE050_AES_GCM_TAG_MIN_SIZE) ||
            (tag_len > SE050_AES_GCM_TAG_MAX_SIZE)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        result = se050_aes_check_iv_len(SE050_KEY_TYPE_AES_128, iv_len);
    }

    if (result == OK) {
        /* 构建 AES-GCM 加密 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x30U;  /* AES GCM Encrypt */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        /* 密钥 ID */
        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        /* IV 长度和 IV */
        apdu[offset] = (uint8_t)iv_len;
        offset++;
        (void)memcpy(&apdu[offset], iv, iv_len);
        offset = offset + iv_len;

        /* AAD (如果有) */
        if ((aad != NULL) && (aad_len > 0U)) {
            if (aad_len <= 255U) {
                apdu[offset] = (uint8_t)aad_len;
                offset++;
                (void)memcpy(&apdu[offset], aad, aad_len);
                offset = offset + aad_len;
            } else {
                result = ERROR_INVALID_PARAM;
            }
        } else {
            apdu[offset] = 0x00U;
            offset++;
        }

        if (result == OK) {
            /* 明文 */
            if (plaintext_len <= 255U) {
                apdu[offset] = (uint8_t)plaintext_len;
                offset++;
                (void)memcpy(&apdu[offset], plaintext, plaintext_len);
                offset = offset + plaintext_len;

                apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
                apdu[offset] = (uint8_t)(plaintext_len + tag_len);
                offset++;

                result = se050_transceive(apdu, offset, response, &resp_len);

                if (result == OK) {
                    if (resp_len >= (plaintext_len + tag_len)) {
                        (void)memcpy(ciphertext, response, plaintext_len);
                        (void)memcpy(tag, &response[plaintext_len], tag_len);
                    } else {
                        result = ERROR_BUFFER_TOO_SMALL;
                    }
                }
            } else {
                result = ERROR_INVALID_PARAM;
            }
        }
    }

    return result;
}

error_t se050_aes_gcm_decrypt(
    uint32_t key_id,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *tag,
    size_t tag_len,
    uint8_t *plaintext,
    size_t plaintext_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[SE050_MAX_APDU_DATA_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(ciphertext, ciphertext_len);
    }

    if (result == OK) {
        result = se050_check_buffer(iv, iv_len);
    }

    if (result == OK) {
        result = se050_check_buffer(tag, tag_len);
    }

    if (result == OK) {
        if (plaintext == NULL) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        if (plaintext_len < ciphertext_len) {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    if (result == OK) {
        result = se050_aes_check_iv_len(SE050_KEY_TYPE_AES_128, iv_len);
    }

    if (result == OK) {
        /* 构建 AES-GCM 解密 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x31U;  /* AES GCM Decrypt */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        /* 密钥 ID */
        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        /* IV */
        apdu[offset] = (uint8_t)iv_len;
        offset++;
        (void)memcpy(&apdu[offset], iv, iv_len);
        offset = offset + iv_len;

        /* AAD */
        if ((aad != NULL) && (aad_len > 0U)) {
            if (aad_len <= 255U) {
                apdu[offset] = (uint8_t)aad_len;
                offset++;
                (void)memcpy(&apdu[offset], aad, aad_len);
                offset = offset + aad_len;
            } else {
                result = ERROR_INVALID_PARAM;
            }
        } else {
            apdu[offset] = 0x00U;
            offset++;
        }

        if (result == OK) {
            /* 密文 + 标签 */
            size_t total_in = ciphertext_len + tag_len;
            if (total_in <= 255U) {
                apdu[offset] = (uint8_t)ciphertext_len;
                offset++;
                (void)memcpy(&apdu[offset], ciphertext, ciphertext_len);
                offset = offset + ciphertext_len;

                apdu[offset] = (uint8_t)tag_len;
                offset++;
                (void)memcpy(&apdu[offset], tag, tag_len);
                offset = offset + tag_len;

                apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
                apdu[offset] = (uint8_t)ciphertext_len;
                offset++;

                result = se050_transceive(apdu, offset, response, &resp_len);

                if (result == OK) {
                    if (resp_len <= plaintext_len) {
                        (void)memcpy(plaintext, response, resp_len);
                    } else {
                        result = ERROR_BUFFER_TOO_SMALL;
                    }
                }
            } else {
                result = ERROR_INVALID_PARAM;
            }
        }
    }

    return result;
}

/******************************************************************************
 * AES-CCM 加密/解密实现
 ******************************************************************************/

error_t se050_aes_ccm_encrypt(
    uint32_t key_id,
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    size_t tag_len,
    uint8_t *ciphertext,
    size_t ciphertext_len,
    uint8_t *tag)
{
    error_t result;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(plaintext, plaintext_len);
    }

    if (result == OK) {
        result = se050_check_buffer(iv, iv_len);
    }

    if (result == OK) {
        if ((ciphertext == NULL) || (tag == NULL)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        if (ciphertext_len < plaintext_len) {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    /* CCM 模式通过 GCM 接口模拟，实际需要不同的 APDU */
    /* 这里调用 GCM 作为占位符 */
    if (result == OK) {
        result = se050_aes_gcm_encrypt(
            key_id,
            plaintext,
            plaintext_len,
            iv,
            iv_len,
            aad,
            aad_len,
            ciphertext,
            ciphertext_len,
            tag,
            tag_len
        );
    }

    return result;
}

error_t se050_aes_ccm_decrypt(
    uint32_t key_id,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *tag,
    size_t tag_len,
    uint8_t *plaintext,
    size_t plaintext_len)
{
    error_t result;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(ciphertext, ciphertext_len);
    }

    if (result == OK) {
        result = se050_check_buffer(iv, iv_len);
    }

    if (result == OK) {
        result = se050_check_buffer(tag, tag_len);
    }

    if (result == OK) {
        if (plaintext == NULL) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        if (plaintext_len < ciphertext_len) {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    if (result == OK) {
        result = se050_aes_gcm_decrypt(
            key_id,
            ciphertext,
            ciphertext_len,
            iv,
            iv_len,
            aad,
            aad_len,
            tag,
            tag_len,
            plaintext,
            plaintext_len
        );
    }

    return result;
}

/******************************************************************************
 * HMAC 实现
 ******************************************************************************/

error_t se050_hmac(
    uint32_t key_id,
    const uint8_t *data,
    size_t data_len,
    uint8_t *hmac,
    size_t *hmac_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[SE050_MAX_APDU_DATA_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(key_id);
    }

    if (result == OK) {
        result = se050_check_buffer(data, data_len);
    }

    if (result == OK) {
        if ((hmac == NULL) || (hmac_len == NULL)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建 HMAC APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_CRYPTO;
        offset++;
        apdu[offset] = 0x40U;  /* HMAC */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x00U;  /* Lc placeholder */
        offset++;

        /* 密钥 ID */
        apdu[offset] = (uint8_t)((key_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((key_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(key_id & 0xFFU);
        offset++;

        /* 数据 */
        if (data_len <= (SE050_MAX_APDU_DATA_LEN - offset - 1U)) {
            (void)memcpy(&apdu[offset], data, data_len);
            offset = offset + data_len;

            apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);
            apdu[offset] = (uint8_t)(*hmac_len);
            offset++;

            result = se050_transceive(apdu, offset, response, &resp_len);

            if (result == OK) {
                if (resp_len > *hmac_len) {
                    result = ERROR_BUFFER_TOO_SMALL;
                } else {
                    (void)memcpy(hmac, response, resp_len);
                    *hmac_len = resp_len;
                }
            }
        } else {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    return result;
}

/******************************************************************************
 * 随机数生成实现
 ******************************************************************************/

error_t se050_generate_random(uint8_t *random, size_t len)
{
    error_t result;
    uint8_t apdu[8];
    uint8_t response[SE050_MAX_APDU_DATA_LEN];
    size_t resp_len;
    size_t offset = 0U;
    size_t remaining = len;
    size_t chunk;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_buffer(random, len);
    }

    if (result == OK) {
        /* SE050 一次最多返回 256 字节随机数 */
        offset = 0U;
        while ((remaining > 0U) && (result == OK)) {
            if (remaining > 256U) {
                chunk = 256U;
            } else {
                chunk = remaining;
            }

            resp_len = chunk;

            /* 构建生成随机数 APDU */
            apdu[0U] = 0x80U;
            apdu[1U] = SE050_INS_CRYPTO;
            apdu[2U] = 0x50U;  /* Generate Random */
            apdu[3U] = 0x00U;  /* P2 */
            apdu[4U] = 0x00U;  /* Lc = 0 */
            apdu[5U] = (uint8_t)chunk;  /* Le */

            result = se050_transceive(apdu, 6U, response, &resp_len);

            if (result == OK) {
                if (resp_len <= (len - offset)) {
                    (void)memcpy(&random[offset], response, resp_len);
                    offset = offset + resp_len;
                    remaining = remaining - resp_len;
                } else {
                    result = ERROR_BUFFER_TOO_SMALL;
                }
            }
        }
    }

    return result;
}

/******************************************************************************
 * 计数器实现
 ******************************************************************************/

error_t se050_increment_counter(uint32_t counter_id, uint32_t *new_value)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[16];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(counter_id);
    }

    if (result == OK) {
        if (new_value == NULL) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建计数器递增 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_MGMT;
        offset++;
        apdu[offset] = 0x10U;  /* Increment Counter */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x04U;  /* Lc */
        offset++;

        apdu[offset] = (uint8_t)((counter_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((counter_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((counter_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(counter_id & 0xFFU);
        offset++;

        apdu[offset] = 0x04U;  /* Le = 4 bytes */
        offset++;

        result = se050_transceive(apdu, offset, response, &resp_len);

        if (result == OK) {
            if (resp_len >= 4U) {
                *new_value = ((uint32_t)response[0U] << 24) |
                            ((uint32_t)response[1U] << 16) |
                            ((uint32_t)response[2U] << 8) |
                            (uint32_t)response[3U];
            } else {
                result = ERROR_BUFFER_TOO_SMALL;
            }
        }
    }

    return result;
}

error_t se050_read_counter(uint32_t counter_id, uint32_t *value)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_DATA_LEN];
    uint8_t response[16];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    result = se050_check_initialized();
    if (result != OK) {
        /* 错误已设置 */
    } else {
        result = se050_check_key_id(counter_id);
    }

    if (result == OK) {
        if (value == NULL) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 构建读取计数器 APDU */
        apdu[offset] = 0x80U;
        offset++;
        apdu[offset] = SE050_INS_READ;
        offset++;
        apdu[offset] = 0x20U;  /* Read Counter */
        offset++;
        apdu[offset] = 0x00U;  /* P2 */
        offset++;
        apdu[offset] = 0x04U;  /* Lc */
        offset++;

        apdu[offset] = (uint8_t)((counter_id >> 24) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((counter_id >> 16) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)((counter_id >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(counter_id & 0xFFU);
        offset++;

        apdu[offset] = 0x04U;  /* Le = 4 bytes */
        offset++;

        result = se050_transceive(apdu, offset, response, &resp_len);

        if (result == OK) {
            if (resp_len >= 4U) {
                *value = ((uint32_t)response[0U] << 24) |
                        ((uint32_t)response[1U] << 16) |
                        ((uint32_t)response[2U] << 8) |
                        (uint32_t)response[3U];
            } else {
                result = ERROR_BUFFER_TOO_SMALL;
            }
        }
    }

    return result;
}
