/******************************************************************************
 * @file    se050_keymgr.c
 * @brief   SE050 密钥管理模块实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    符合 MISRA-C 2012 规范
 *          支持密钥生成、导入、导出、删除和证书功能
 ******************************************************************************/

#include "se050_adapter.h"
#include <string.h>

/******************************************************************************
 * MISRA-C 2012 合规注意事项
 * - 规则 8.4: 提供兼容的声明
 * - 规则 10.1: 避免隐式类型转换
 * - 规则 15.5: 单一出口点
 * - 规则 17.7: 检查函数返回值
 ******************************************************************************/

/******************************************************************************
 * 版本信息
 ******************************************************************************/
#define SE050_KEYMGR_VERSION_MAJOR      (1U)
#define SE050_KEYMGR_VERSION_MINOR      (0U)
#define SE050_KEYMGR_VERSION_PATCH      (0U)

/******************************************************************************
 * SE05x 密钥标识符类型定义
 ******************************************************************************/
typedef uint32_t SE05x_KeyID_t;

/******************************************************************************
 * 内部宏定义
 ******************************************************************************/
#define SE050_KEYMGR_MAGIC              (0x4B4D4752U)   /* 'KMGR' */

/* SE050 APDU 指令定义 */
#define SE050_INS_WRITE                 (0x01U)
#define SE050_INS_READ                  (0x02U)
#define SE050_INS_CRYPTO                (0x03U)
#define SE050_INS_MGMT                  (0x04U)
#define SE050_INS_ATTEST                (0x05U)

/* SE050 P1/P2 参数 */
#define SE050_P1_GENERATE               (0x01U)
#define SE050_P1_IMPORT                 (0x02U)
#define SE050_P1_EXPORT                 (0x03U)
#define SE050_P1_DELETE                 (0x04U)
#define SE050_P1_GET_ATTR               (0x05U)

#define SE050_P2_ECC_KEY_PAIR           (0x01U)
#define SE050_P2_AES_KEY                (0x02U)
#define SE050_P2_PUBLIC_KEY             (0x03U)

/* ECC 密钥大小定义 */
#define SE050_ECC_P256_FIELD_SIZE       (32U)
#define SE050_ECC_P384_FIELD_SIZE       (48U)
#define SE050_ECC_P256_PUB_KEY_SIZE     (65U)  /* 0x04 + X(32) + Y(32) */
#define SE050_ECC_P384_PUB_KEY_SIZE     (97U)  /* 0x04 + X(48) + Y(48) */
#define SE050_ECC_P256_SIG_SIZE         (64U)  /* R(32) + S(32) */
#define SE050_ECC_P384_SIG_SIZE         (96U)  /* R(48) + S(48) */

/* AES 密钥大小定义 */
#define SE050_AES_128_KEY_SIZE          (16U)
#define SE050_AES_256_KEY_SIZE          (32U)

/* 证书相关定义 */
#define SE050_MAX_CERT_SIZE             (2048U)
#define SE050_ATTEST_MAGIC              (0xA5A5U)
#define SE050_ATTEST_VERSION            (0x0001U)

/* APDU 缓冲区大小 */
#define SE050_MAX_APDU_LEN              (256U)
#define SE050_MAX_RSP_LEN               (256U)

/* 会话密钥起始 ID */
#define SE050_SESSION_KEY_ID_START      (0x00000001U)
#define SE050_SESSION_KEY_ID_END        (0x0000FFFFU)

/* 持久密钥起始 ID */
#define SE050_PERSISTENT_KEY_ID_START   (0x00010000U)
#define SE050_PERSISTENT_KEY_ID_END     (0x7FFFFFFFU)

/******************************************************************************
 * 内部数据结构
 ******************************************************************************/

/**
 * @brief 密钥管理器上下文
 */
typedef struct {
    uint32_t magic;                     /**< 魔数，用于验证初始化状态 */
    bool initialized;                   /**< 初始化标志 */
    SE05x_KeyID_t session_key_counter;  /**< 会话密钥计数器 */
    uint32_t active_key_count;          /**< 活动密钥数量 */
} se050_keymgr_ctx_t;

/**
 * @brief 密钥属性内部结构
 */
typedef struct {
    SE05x_KeyID_t key_id;
    se050_key_type_t type;
    uint16_t permissions;
    uint32_t access_counter;
    bool persistent;
    bool exists;
    uint32_t create_time;
    uint32_t valid_until;
} se050_key_attr_internal_t;

/**
 * @brief 证书数据结构
 */
typedef struct {
    uint16_t magic;
    uint16_t version;
    uint32_t key_id;
    uint32_t timestamp;
    uint8_t signature[SE050_ECC_P256_SIG_SIZE];
    uint8_t pub_key[SE050_ECC_P256_PUB_KEY_SIZE];
    uint16_t cert_data_len;
} se050_attest_cert_t;

/******************************************************************************
 * 全局上下文
 ******************************************************************************/
static se050_keymgr_ctx_t g_keymgr_ctx = {
    0U,
    false,
    SE050_SESSION_KEY_ID_START,
    0U
};

/******************************************************************************
 * 内部静态函数声明
 ******************************************************************************/
static error_t se050_keymgr_check_init(void);
static error_t se050_keymgr_check_key_id(SE05x_KeyID_t key_id);
static error_t se050_keymgr_is_persistent(SE05x_KeyID_t key_id, bool *persistent);
static error_t se050_keymgr_get_key_size(se050_key_type_t type, size_t *key_size);
static error_t se050_keymgr_get_pubkey_size(se050_key_type_t type, size_t *pubkey_size);
static error_t se050_keymgr_transceive(
    const uint8_t *tx_buf,
    size_t tx_len,
    uint8_t *rx_buf,
    size_t *rx_len
);
static error_t se050_keymgr_build_apdu_header(
    uint8_t *apdu,
    size_t *offset,
    uint8_t ins,
    uint8_t p1,
    uint8_t p2
);
static error_t se050_keymgr_write_key_id(
    uint8_t *apdu,
    size_t *offset,
    SE05x_KeyID_t key_id
);
static error_t se050_keymgr_write_permissions(
    uint8_t *apdu,
    size_t *offset,
    uint16_t permissions
);
static error_t se050_keymgr_parse_key_attr(
    const uint8_t *rsp,
    size_t rsp_len,
    se050_key_attr_internal_t *attr
);
static error_t se050_keymgr_secure_clear(void *buffer, size_t len);

/******************************************************************************
 * 内部静态函数实现
 ******************************************************************************/

/**
 * @brief 检查密钥管理器初始化状态
 * @return OK 成功，ERROR_NOT_INITIALIZED 未初始化
 */
static error_t se050_keymgr_check_init(void)
{
    error_t result;

    if ((g_keymgr_ctx.initialized == true) &&
        (g_keymgr_ctx.magic == SE050_KEYMGR_MAGIC)) {
        result = OK;
    } else {
        result = ERROR_NOT_INITIALIZED;
    }

    return result;
}

/**
 * @brief 检查密钥 ID 有效性
 * @param[in] key_id 密钥标识符
 * @return OK 成功，ERROR_INVALID_PARAM 参数无效
 */
static error_t se050_keymgr_check_key_id(SE05x_KeyID_t key_id)
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
 * @brief 判断密钥是否为持久性密钥
 * @param[in] key_id 密钥标识符
 * @param[out] persistent 是否为持久性密钥
 * @return OK 成功
 */
static error_t se050_keymgr_is_persistent(SE05x_KeyID_t key_id, bool *persistent)
{
    error_t result = OK;

    if (persistent == NULL) {
        result = ERROR_INVALID_PARAM;
    } else {
        if ((key_id >= SE050_PERSISTENT_KEY_ID_START) &&
            (key_id <= SE050_PERSISTENT_KEY_ID_END)) {
            *persistent = true;
        } else {
            *persistent = false;
        }
    }

    return result;
}

/**
 * @brief 获取密钥大小
 * @param[in] type 密钥类型
 * @param[out] key_size 密钥大小（字节）
 * @return OK 成功，ERROR_INVALID_PARAM 不支持的类型
 */
static error_t se050_keymgr_get_key_size(se050_key_type_t type, size_t *key_size)
{
    error_t result = OK;

    if (key_size == NULL) {
        result = ERROR_INVALID_PARAM;
    } else {
        switch (type) {
            case SE050_KEY_TYPE_ECC_P256:
                *key_size = SE050_ECC_P256_FIELD_SIZE;
                break;
            case SE050_KEY_TYPE_ECC_P384:
                *key_size = SE050_ECC_P384_FIELD_SIZE;
                break;
            case SE050_KEY_TYPE_AES_128:
                *key_size = SE050_AES_128_KEY_SIZE;
                break;
            case SE050_KEY_TYPE_AES_256:
                *key_size = SE050_AES_256_KEY_SIZE;
                break;
            default:
                result = ERROR_INVALID_PARAM;
                *key_size = 0U;
                break;
        }
    }

    return result;
}

/**
 * @brief 获取公钥大小
 * @param[in] type 密钥类型
 * @param[out] pubkey_size 公钥大小（字节）
 * @return OK 成功，ERROR_INVALID_PARAM 不支持的类型
 */
static error_t se050_keymgr_get_pubkey_size(se050_key_type_t type, size_t *pubkey_size)
{
    error_t result = OK;

    if (pubkey_size == NULL) {
        result = ERROR_INVALID_PARAM;
    } else {
        switch (type) {
            case SE050_KEY_TYPE_ECC_P256:
                *pubkey_size = SE050_ECC_P256_PUB_KEY_SIZE;
                break;
            case SE050_KEY_TYPE_ECC_P384:
                *pubkey_size = SE050_ECC_P384_PUB_KEY_SIZE;
                break;
            default:
                /* AES 密钥没有公钥 */
                result = ERROR_INVALID_PARAM;
                *pubkey_size = 0U;
                break;
        }
    }

    return result;
}

/**
 * @brief 与 SE050 进行数据交换
 * @param[in] tx_buf 发送缓冲区
 * @param[in] tx_len 发送长度
 * @param[out] rx_buf 接收缓冲区
 * @param[in,out] rx_len 接收缓冲区大小/实际接收长度
 * @return OK 成功
 */
static error_t se050_keymgr_transceive(
    const uint8_t *tx_buf,
    size_t tx_len,
    uint8_t *rx_buf,
    size_t *rx_len)
{
    error_t result = OK;

    /* MISRA-C: 参数检查 */
    if ((tx_buf == NULL) || (rx_buf == NULL) || (rx_len == NULL)) {
        result = ERROR_INVALID_PARAM;
    } else if ((tx_len == 0U) || (*rx_len == 0U)) {
        result = ERROR_INVALID_PARAM;
    } else {
        /* 这里调用底层 HAL 的 I2C/SPI 交换函数 */
        /* 实际实现需要与 NXP SE050 硬件驱动集成 */
        /* 模拟成功响应 - SW1=0x90, SW2=0x00 */
        if (*rx_len >= 2U) {
            rx_buf[0U] = 0x90U;
            rx_buf[1U] = 0x00U;
            *rx_len = 2U;
        }
    }

    return result;
}

/**
 * @brief 构建 APDU 命令头
 * @param[out] apdu APDU 缓冲区
 * @param[in,out] offset 当前偏移量
 * @param[in] ins 指令码
 * @param[in] p1 P1 参数
 * @param[in] p2 P2 参数
 * @return OK 成功
 */
static error_t se050_keymgr_build_apdu_header(
    uint8_t *apdu,
    size_t *offset,
    uint8_t ins,
    uint8_t p1,
    uint8_t p2)
{
    error_t result = OK;
    size_t idx = 0U;

    if ((apdu == NULL) || (offset == NULL)) {
        result = ERROR_INVALID_PARAM;
    } else {
        /* CLA: Class byte */
        apdu[idx] = 0x80U;
        idx++;

        /* INS: Instruction */
        apdu[idx] = ins;
        idx++;

        /* P1: Parameter 1 */
        apdu[idx] = p1;
        idx++;

        /* P2: Parameter 2 */
        apdu[idx] = p2;
        idx++;

        /* Lc: Length of data (placeholder) */
        apdu[idx] = 0x00U;
        idx++;

        *offset = idx;
    }

    return result;
}

/**
 * @brief 写入密钥 ID 到 APDU
 * @param[out] apdu APDU 缓冲区
 * @param[in,out] offset 当前偏移量
 * @param[in] key_id 密钥标识符
 * @return OK 成功
 */
static error_t se050_keymgr_write_key_id(
    uint8_t *apdu,
    size_t *offset,
    SE05x_KeyID_t key_id)
{
    error_t result = OK;
    size_t idx;

    if ((apdu == NULL) || (offset == NULL)) {
        result = ERROR_INVALID_PARAM;
    } else {
        idx = *offset;

        /* 密钥 ID (4 字节，大端序) */
        apdu[idx] = (uint8_t)((key_id >> 24) & 0xFFU);
        idx++;
        apdu[idx] = (uint8_t)((key_id >> 16) & 0xFFU);
        idx++;
        apdu[idx] = (uint8_t)((key_id >> 8) & 0xFFU);
        idx++;
        apdu[idx] = (uint8_t)(key_id & 0xFFU);
        idx++;

        *offset = idx;
    }

    return result;
}

/**
 * @brief 写入权限到 APDU
 * @param[out] apdu APDU 缓冲区
 * @param[in,out] offset 当前偏移量
 * @param[in] permissions 权限位图
 * @return OK 成功
 */
static error_t se050_keymgr_write_permissions(
    uint8_t *apdu,
    size_t *offset,
    uint16_t permissions)
{
    error_t result = OK;
    size_t idx;

    if ((apdu == NULL) || (offset == NULL)) {
        result = ERROR_INVALID_PARAM;
    } else {
        idx = *offset;

        /* 权限 (2 字节，大端序) */
        apdu[idx] = (uint8_t)((permissions >> 8) & 0xFFU);
        idx++;
        apdu[idx] = (uint8_t)(permissions & 0xFFU);
        idx++;

        *offset = idx;
    }

    return result;
}

/**
 * @brief 解析密钥属性响应
 * @param[in] rsp 响应数据
 * @param[in] rsp_len 响应长度
 * @param[out] attr 密钥属性结构
 * @return OK 成功
 */
static error_t se050_keymgr_parse_key_attr(
    const uint8_t *rsp,
    size_t rsp_len,
    se050_key_attr_internal_t *attr)
{
    error_t result = OK;

    if ((rsp == NULL) || (attr == NULL)) {
        result = ERROR_INVALID_PARAM;
    } else if (rsp_len < 16U) {
        result = ERROR_INVALID_PARAM;
    } else {
        /* 解析 TLV 格式的属性数据 */
        size_t idx = 0U;

        /* Key ID (4 bytes) */
        attr->key_id = ((uint32_t)rsp[idx] << 24) |
                       ((uint32_t)rsp[idx + 1U] << 16) |
                       ((uint32_t)rsp[idx + 2U] << 8) |
                       (uint32_t)rsp[idx + 3U];
        idx += 4U;

        /* Key type (1 byte) */
        if (rsp[idx] < (uint8_t)SE050_KEY_TYPE_MAX) {
            attr->type = (se050_key_type_t)rsp[idx];
        } else {
            attr->type = SE050_KEY_TYPE_MAX;
        }
        idx++;

        /* Permissions (2 bytes) */
        attr->permissions = ((uint16_t)rsp[idx] << 8) | (uint16_t)rsp[idx + 1U];
        idx += 2U;

        /* Access counter (4 bytes) */
        attr->access_counter = ((uint32_t)rsp[idx] << 24) |
                               ((uint32_t)rsp[idx + 1U] << 16) |
                               ((uint32_t)rsp[idx + 2U] << 8) |
                               (uint32_t)rsp[idx + 3U];
        idx += 4U;

        /* Persistent flag (1 byte) */
        attr->persistent = (rsp[idx] != 0U) ? true : false;
        idx++;

        /* Exists flag (1 byte) */
        attr->exists = (rsp[idx] != 0U) ? true : false;
    }

    return result;
}

/**
 * @brief 安全清除缓冲区（防止优化）
 * @param[out] buffer 缓冲区
 * @param[in] len 缓冲区大小
 * @return OK 成功
 */
static error_t se050_keymgr_secure_clear(void *buffer, size_t len)
{
    error_t result = OK;
    volatile uint8_t *p;
    size_t i;

    if ((buffer == NULL) || (len == 0U)) {
        result = ERROR_INVALID_PARAM;
    } else {
        p = (volatile uint8_t *)buffer;
        for (i = 0U; i < len; i++) {
            p[i] = 0U;
        }
    }

    return result;
}

/******************************************************************************
 * 密钥生成接口
 ******************************************************************************/

/**
 * @brief 生成密钥
 * @param[in] key_id 密钥标识符
 * @param[in] type 密钥类型 (P-256, P-384, AES-128/256)
 * @param[in] permissions 密钥权限
 * @param[in] persistent 是否为持久性密钥
 * @return OK 成功
 */
error_t se050_key_generate(
    SE05x_KeyID_t key_id,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_LEN];
    uint8_t response[SE050_MAX_RSP_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t key_size = 0U;
    uint8_t p2_value = 0U;

    /* 检查初始化状态 */
    result = se050_keymgr_check_init();
    if (result != OK) {
        /* 未初始化 */
    } else {
        /* 检查密钥 ID */
        result = se050_keymgr_check_key_id(key_id);
    }

    if (result == OK) {
        /* 检查密钥类型有效性 */
        if (type >= SE050_KEY_TYPE_MAX) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 获取密钥大小 */
        result = se050_keymgr_get_key_size(type, &key_size);
    }

    if (result == OK) {
        /* 确定 P2 值 */
        switch (type) {
            case SE050_KEY_TYPE_ECC_P256:
            case SE050_KEY_TYPE_ECC_P384:
                p2_value = SE050_P2_ECC_KEY_PAIR;
                break;
            case SE050_KEY_TYPE_AES_128:
            case SE050_KEY_TYPE_AES_256:
                p2_value = SE050_P2_AES_KEY;
                break;
            default:
                result = ERROR_INVALID_PARAM;
                break;
        }
    }

    if (result == OK) {
        /* 构建 APDU 命令 */
        result = se050_keymgr_build_apdu_header(
            apdu, &offset,
            SE050_INS_CRYPTO,
            SE050_P1_GENERATE,
            p2_value);
    }

    if (result == OK) {
        /* 写入密钥 ID */
        result = se050_keymgr_write_key_id(apdu, &offset, key_id);
    }

    if (result == OK) {
        /* 写入权限 */
        result = se050_keymgr_write_permissions(apdu, &offset, permissions);
    }

    if (result == OK) {
        /* 写入持久性标志 */
        apdu[offset] = persistent ? 0x01U : 0x00U;
        offset++;

        /* 写入密钥类型 */
        apdu[offset] = (uint8_t)type;
        offset++;

        /* 更新 Lc 字段 */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 设置期望响应长度 */
        apdu[offset] = (uint8_t)(key_size & 0xFFU);
        offset++;

        /* 发送 APDU */
        result = se050_keymgr_transceive(apdu, offset, response, &resp_len);
    }

    if (result == OK) {
        /* 检查响应状态 */
        if ((resp_len >= 2U) && (response[resp_len - 2U] == 0x90U) &&
            (response[resp_len - 1U] == 0x00U)) {
            /* 成功 */
            if (persistent == false) {
                g_keymgr_ctx.session_key_counter++;
            }
            g_keymgr_ctx.active_key_count++;
        } else {
            result = ERROR_SE_FAILURE;
        }
    }

    /* 安全清除 APDU 缓冲区 */
    (void)se050_keymgr_secure_clear(apdu, sizeof(apdu));

    return result;
}

/******************************************************************************
 * 密钥导入接口
 ******************************************************************************/

/**
 * @brief 导入密钥到 SE050
 * @param[in] key_id 密钥标识符
 * @param[in] key_data 密钥数据
 * @param[in] key_len 密钥长度
 * @param[in] type 密钥类型
 * @param[in] permissions 密钥权限
 * @param[in] persistent 是否为持久性密钥
 * @return OK 成功
 */
error_t se050_key_import(
    SE05x_KeyID_t key_id,
    const uint8_t *key_data,
    size_t key_len,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_LEN];
    uint8_t response[SE050_MAX_RSP_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t expected_key_size = 0U;
    uint8_t p2_value = 0U;

    /* 检查初始化状态 */
    result = se050_keymgr_check_init();
    if (result != OK) {
        /* 未初始化 */
    } else {
        /* 检查密钥 ID */
        result = se050_keymgr_check_key_id(key_id);
    }

    if (result == OK) {
        /* 检查密钥数据 */
        if ((key_data == NULL) || (key_len == 0U)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 获取期望的密钥大小 */
        result = se050_keymgr_get_key_size(type, &expected_key_size);
    }

    if (result == OK) {
        /* 验证密钥长度 */
        if (key_len != expected_key_size) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 验证密钥类型并设置 P2 */
        switch (type) {
            case SE050_KEY_TYPE_ECC_P256:
            case SE050_KEY_TYPE_ECC_P384:
                p2_value = SE050_P2_ECC_KEY_PAIR;
                break;
            case SE050_KEY_TYPE_AES_128:
            case SE050_KEY_TYPE_AES_256:
                p2_value = SE050_P2_AES_KEY;
                break;
            default:
                result = ERROR_INVALID_PARAM;
                break;
        }
    }

    if (result == OK) {
        /* 检查缓冲区是否足够 */
        if ((key_len + 12U) > SE050_MAX_APDU_LEN) {
            result = ERROR_BUFFER_TOO_SMALL;
        }
    }

    if (result == OK) {
        /* 构建 APDU 命令 */
        result = se050_keymgr_build_apdu_header(
            apdu, &offset,
            SE050_INS_WRITE,
            SE050_P1_IMPORT,
            p2_value);
    }

    if (result == OK) {
        /* 写入密钥 ID */
        result = se050_keymgr_write_key_id(apdu, &offset, key_id);
    }

    if (result == OK) {
        /* 写入权限 */
        result = se050_keymgr_write_permissions(apdu, &offset, permissions);
    }

    if (result == OK) {
        /* 写入持久性标志 */
        apdu[offset] = persistent ? 0x01U : 0x00U;
        offset++;

        /* 写入密钥类型 */
        apdu[offset] = (uint8_t)type;
        offset++;

        /* 写入密钥数据 */
        (void)memcpy(&apdu[offset], key_data, key_len);
        offset += key_len;

        /* 更新 Lc 字段 */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 发送 APDU */
        result = se050_keymgr_transceive(apdu, offset, response, &resp_len);
    }

    if (result == OK) {
        /* 检查响应状态 */
        if ((resp_len >= 2U) && (response[resp_len - 2U] == 0x90U) &&
            (response[resp_len - 1U] == 0x00U)) {
            /* 成功 */
            g_keymgr_ctx.active_key_count++;
        } else {
            result = ERROR_SE_FAILURE;
        }
    }

    /* 安全清除 APDU 缓冲区 */
    (void)se050_keymgr_secure_clear(apdu, sizeof(apdu));

    return result;
}

/******************************************************************************
 * 密钥导出接口
 ******************************************************************************/

/**
 * @brief 导出公钥
 * @param[in] key_id 密钥标识符
 * @param[out] pub_key 公钥缓冲区
 * @param[in,out] pub_key_len 缓冲区大小/实际公钥长度
 * @return OK 成功
 */
error_t se050_key_export(
    SE05x_KeyID_t key_id,
    uint8_t *pub_key,
    size_t *pub_key_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_LEN];
    uint8_t response[SE050_MAX_RSP_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t expected_pubkey_size = 0U;
    se050_key_attr_internal_t attr;

    /* 检查初始化状态 */
    result = se050_keymgr_check_init();
    if (result != OK) {
        /* 未初始化 */
    } else {
        /* 检查密钥 ID */
        result = se050_keymgr_check_key_id(key_id);
    }

    if (result == OK) {
        /* 检查输出缓冲区 */
        if ((pub_key == NULL) || (pub_key_len == NULL) || (*pub_key_len == 0U)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 获取密钥属性以确定类型 */
        result = se050_get_key_attr(key_id, (se050_key_attr_t *)&attr);
    }

    if (result == OK) {
        /* 获取公钥大小 */
        result = se050_keymgr_get_pubkey_size(attr.type, &expected_pubkey_size);
    }

    if (result == OK) {
        /* 检查缓冲区大小 */
        if (*pub_key_len < expected_pubkey_size) {
            result = ERROR_BUFFER_TOO_SMALL;
            *pub_key_len = expected_pubkey_size;
        }
    }

    if (result == OK) {
        /* 构建 APDU 命令 */
        result = se050_keymgr_build_apdu_header(
            apdu, &offset,
            SE050_INS_READ,
            SE050_P1_EXPORT,
            SE050_P2_PUBLIC_KEY);
    }

    if (result == OK) {
        /* 写入密钥 ID */
        result = se050_keymgr_write_key_id(apdu, &offset, key_id);
    }

    if (result == OK) {
        /* 更新 Lc 字段 */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 设置期望响应长度 */
        apdu[offset] = (uint8_t)(expected_pubkey_size & 0xFFU);
        offset++;

        /* 发送 APDU */
        result = se050_keymgr_transceive(apdu, offset, response, &resp_len);
    }

    if (result == OK) {
        /* 检查响应状态和数据 */
        if ((resp_len >= (expected_pubkey_size + 2U)) &&
            (response[resp_len - 2U] == 0x90U) &&
            (response[resp_len - 1U] == 0x00U)) {
            /* 复制公钥数据 (排除状态字) */
            (void)memcpy(pub_key, response, expected_pubkey_size);
            *pub_key_len = expected_pubkey_size;
        } else {
            result = ERROR_SE_FAILURE;
        }
    }

    /* 安全清除 APDU 缓冲区 */
    (void)se050_keymgr_secure_clear(apdu, sizeof(apdu));

    return result;
}

/******************************************************************************
 * 密钥删除接口
 ******************************************************************************/

/**
 * @brief 删除密钥（安全清除）
 * @param[in] key_id 密钥标识符
 * @return OK 成功
 */
error_t se050_key_delete(SE05x_KeyID_t key_id)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_LEN];
    uint8_t response[SE050_MAX_RSP_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    /* 检查初始化状态 */
    result = se050_keymgr_check_init();
    if (result != OK) {
        /* 未初始化 */
    } else {
        /* 检查密钥 ID */
        result = se050_keymgr_check_key_id(key_id);
    }

    if (result == OK) {
        /* 构建 APDU 命令 */
        result = se050_keymgr_build_apdu_header(
            apdu, &offset,
            SE050_INS_MGMT,
            SE050_P1_DELETE,
            0x00U);
    }

    if (result == OK) {
        /* 写入密钥 ID */
        result = se050_keymgr_write_key_id(apdu, &offset, key_id);
    }

    if (result == OK) {
        /* 写入安全删除标志 */
        apdu[offset] = 0x01U;  /* 启用安全清除 */
        offset++;

        /* 更新 Lc 字段 */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 发送 APDU */
        result = se050_keymgr_transceive(apdu, offset, response, &resp_len);
    }

    if (result == OK) {
        /* 检查响应状态 */
        if ((resp_len >= 2U) && (response[resp_len - 2U] == 0x90U) &&
            (response[resp_len - 1U] == 0x00U)) {
            /* 成功删除 */
            if (g_keymgr_ctx.active_key_count > 0U) {
                g_keymgr_ctx.active_key_count--;
            }
        } else if ((resp_len >= 2U) && (response[resp_len - 2U] == 0x6AU) &&
                   (response[resp_len - 1U] == 0x82U)) {
            /* 密钥不存在 - 视为成功 */
            result = OK;
        } else {
            result = ERROR_SE_FAILURE;
        }
    }

    /* 安全清除 APDU 缓冲区 */
    (void)se050_keymgr_secure_clear(apdu, sizeof(apdu));

    return result;
}

/******************************************************************************
 * 密钥证书接口
 ******************************************************************************/

/**
 * @brief 生成密钥证书（证明密钥在 SE050 中生成）
 * @param[in] key_id 密钥标识符
 * @param[out] cert 证书缓冲区
 * @param[in,out] cert_len 缓冲区大小/实际证书长度
 * @return OK 成功
 */
error_t se050_key_attest(
    SE05x_KeyID_t key_id,
    uint8_t *cert,
    size_t *cert_len)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_LEN];
    uint8_t response[SE050_MAX_RSP_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    size_t expected_cert_size = sizeof(se050_attest_cert_t);
    se050_key_attr_internal_t attr;

    /* 检查初始化状态 */
    result = se050_keymgr_check_init();
    if (result != OK) {
        /* 未初始化 */
    } else {
        /* 检查密钥 ID */
        result = se050_keymgr_check_key_id(key_id);
    }

    if (result == OK) {
        /* 检查输出缓冲区 */
        if ((cert == NULL) || (cert_len == NULL) || (*cert_len == 0U)) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 获取密钥属性 */
        result = se050_get_key_attr(key_id, (se050_key_attr_t *)&attr);
    }

    if (result == OK) {
        /* 检查密钥是否存在 */
        if (attr.exists == false) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 检查缓冲区大小 */
        if (*cert_len < expected_cert_size) {
            result = ERROR_BUFFER_TOO_SMALL;
            *cert_len = expected_cert_size;
        }
    }

    if (result == OK) {
        /* 构建 APDU 命令 */
        result = se050_keymgr_build_apdu_header(
            apdu, &offset,
            SE050_INS_ATTEST,
            0x01U,  /* 生成证书 */
            0x00U);
    }

    if (result == OK) {
        /* 写入密钥 ID */
        result = se050_keymgr_write_key_id(apdu, &offset, key_id);
    }

    if (result == OK) {
        /* 写入证书版本 */
        apdu[offset] = (uint8_t)((SE050_ATTEST_VERSION >> 8) & 0xFFU);
        offset++;
        apdu[offset] = (uint8_t)(SE050_ATTEST_VERSION & 0xFFU);
        offset++;

        /* 更新 Lc 字段 */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 设置期望响应长度 */
        apdu[offset] = (uint8_t)(expected_cert_size & 0xFFU);
        offset++;

        /* 发送 APDU */
        result = se050_keymgr_transceive(apdu, offset, response, &resp_len);
    }

    if (result == OK) {
        /* 检查响应状态和数据 */
        if ((resp_len >= (expected_cert_size + 2U)) &&
            (response[resp_len - 2U] == 0x90U) &&
            (response[resp_len - 1U] == 0x00U)) {
            /* 验证证书魔数 */
            uint16_t magic = ((uint16_t)response[0] << 8) | (uint16_t)response[1];
            if (magic == SE050_ATTEST_MAGIC) {
                /* 复制证书数据 (排除状态字) */
                (void)memcpy(cert, response, expected_cert_size);
                *cert_len = expected_cert_size;
            } else {
                result = ERROR_SE_FAILURE;
            }
        } else {
            result = ERROR_SE_FAILURE;
        }
    }

    /* 安全清除 APDU 缓冲区 */
    (void)se050_keymgr_secure_clear(apdu, sizeof(apdu));

    return result;
}

/******************************************************************************
 * 密钥属性查询接口
 ******************************************************************************/

/**
 * @brief 获取密钥属性
 * @param[in] key_id 密钥标识符
 * @param[out] attr 密钥属性结构
 * @return OK 成功
 */
error_t se050_get_key_attr(SE05x_KeyID_t key_id, se050_key_attr_t *attr)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_LEN];
    uint8_t response[SE050_MAX_RSP_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;
    se050_key_attr_internal_t internal_attr;
    bool persistent = false;

    /* 检查初始化状态 */
    result = se050_keymgr_check_init();
    if (result != OK) {
        /* 未初始化 */
    } else {
        /* 检查密钥 ID */
        result = se050_keymgr_check_key_id(key_id);
    }

    if (result == OK) {
        /* 检查输出缓冲区 */
        if (attr == NULL) {
            result = ERROR_INVALID_PARAM;
        }
    }

    if (result == OK) {
        /* 初始化输出结构 */
        (void)memset(attr, 0, sizeof(se050_key_attr_t));

        /* 构建 APDU 命令 */
        result = se050_keymgr_build_apdu_header(
            apdu, &offset,
            SE050_INS_READ,
            SE050_P1_GET_ATTR,
            0x00U);
    }

    if (result == OK) {
        /* 写入密钥 ID */
        result = se050_keymgr_write_key_id(apdu, &offset, key_id);
    }

    if (result == OK) {
        /* 更新 Lc 字段 */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 设置期望响应长度 */
        apdu[offset] = 0x00U;  /* 最大长度 */
        offset++;

        /* 发送 APDU */
        result = se050_keymgr_transceive(apdu, offset, response, &resp_len);
    }

    if (result == OK) {
        /* 检查响应状态和数据 */
        if ((resp_len >= 14U) &&
            (response[resp_len - 2U] == 0x90U) &&
            (response[resp_len - 1U] == 0x00U)) {
            /* 解析属性 */
            result = se050_keymgr_parse_key_attr(
                response, resp_len - 2U, &internal_attr);

            if (result == OK) {
                /* 填充输出结构 */
                attr->key_id = internal_attr.key_id;
                attr->type = internal_attr.type;
                attr->permissions = internal_attr.permissions;
                attr->access_counter = internal_attr.access_counter;

                /* 判断持久性 */
                result = se050_keymgr_is_persistent(key_id, &persistent);
                if (result == OK) {
                    attr->persistent = persistent;
                }
            }
        } else if ((resp_len >= 2U) && (response[resp_len - 2U] == 0x6AU) &&
                   (response[resp_len - 1U] == 0x82U)) {
            /* 密钥不存在 */
            attr->key_id = key_id;
            attr->type = SE050_KEY_TYPE_MAX;
            attr->persistent = false;
            result = OK;  /* 查询成功，但密钥不存在 */
        } else {
            result = ERROR_SE_FAILURE;
        }
    }

    /* 安全清除 APDU 缓冲区 */
    (void)se050_keymgr_secure_clear(apdu, sizeof(apdu));

    return result;
}

/******************************************************************************
 * 密钥存在性检查接口
 ******************************************************************************/

/**
 * @brief 检查密钥是否存在
 * @param[in] key_id 密钥标识符
 * @param[out] exists 是否存在
 * @param[out] key_info 密钥信息 (可为 NULL)
 * @return OK 成功
 */
error_t se050_key_exists(
    SE05x_KeyID_t key_id,
    bool *exists,
    se050_key_info_t *key_info)
{
    error_t result;
    se050_key_attr_t attr;

    /* 参数检查 */
    if (exists == NULL) {
        result = ERROR_INVALID_PARAM;
    } else {
        /* 获取密钥属性 */
        result = se050_get_key_attr(key_id, &attr);

        if (result == OK) {
            /* 判断是否存在 */
            *exists = (attr.type != SE050_KEY_TYPE_MAX);

            /* 填充密钥信息（如果提供） */
            if (key_info != NULL) {
                (void)memset(key_info, 0, sizeof(se050_key_info_t));
                key_info->key_id = attr.key_id;
                key_info->type = attr.type;
                key_info->permissions = attr.permissions;
                key_info->exists = *exists;

                /* 如果存在，尝试获取公钥 */
                if (*exists == true) {
                    size_t pubkey_len = sizeof(key_info->public_key);
                    (void)se050_key_export(key_id, key_info->public_key, &pubkey_len);
                    key_info->pub_key_len = pubkey_len;
                }
            }
        }
    }

    return result;
}

/******************************************************************************
 * 密钥管理器初始化和反初始化
 ******************************************************************************/

/**
 * @brief 初始化密钥管理器
 * @return OK 成功
 */
error_t se050_keymgr_init(void)
{
    error_t result = OK;

    if (g_keymgr_ctx.initialized == true) {
        result = ERROR_ALREADY_INITIALIZED;
    } else {
        /* 初始化上下文 */
        g_keymgr_ctx.magic = SE050_KEYMGR_MAGIC;
        g_keymgr_ctx.initialized = true;
        g_keymgr_ctx.session_key_counter = SE050_SESSION_KEY_ID_START;
        g_keymgr_ctx.active_key_count = 0U;
    }

    return result;
}

/**
 * @brief 反初始化密钥管理器
 * @return OK 成功
 */
error_t se050_keymgr_deinit(void)
{
    error_t result;

    result = se050_keymgr_check_init();
    if (result == OK) {
        /* 清除所有会话密钥 */
        /* 实际实现应该遍历并删除所有会话密钥 */

        /* 清除上下文 */
        g_keymgr_ctx.magic = 0U;
        g_keymgr_ctx.initialized = false;
        g_keymgr_ctx.session_key_counter = SE050_SESSION_KEY_ID_START;
        g_keymgr_ctx.active_key_count = 0U;
    }

    return result;
}

/**
 * @brief 获取密钥管理器版本
 * @return 版本字符串
 */
const char* se050_keymgr_get_version(void)
{
    static const char version[] = "1.0.0";
    return version;
}

/**
 * @brief 获取活动密钥数量
 * @param[out] count 密钥数量
 * @return OK 成功
 */
error_t se050_keymgr_get_active_key_count(uint32_t *count)
{
    error_t result;

    result = se050_keymgr_check_init();
    if (result == OK) {
        if (count == NULL) {
            result = ERROR_INVALID_PARAM;
        } else {
            *count = g_keymgr_ctx.active_key_count;
        }
    }

    return result;
}

/******************************************************************************
 * 批量密钥操作接口
 ******************************************************************************/

/**
 * @brief 删除所有会话密钥
 * @return OK 成功
 */
error_t se050_key_delete_all_session(void)
{
    error_t result = OK;
    SE05x_KeyID_t key_id;
    uint32_t deleted_count = 0U;

    result = se050_keymgr_check_init();

    if (result == OK) {
        /* 遍历会话密钥范围并删除 */
        for (key_id = SE050_SESSION_KEY_ID_START;
             key_id <= SE050_SESSION_KEY_ID_END;
             key_id++) {
            bool exists = false;
            (void)se050_key_exists(key_id, &exists, NULL);

            if (exists == true) {
                error_t del_result = se050_key_delete(key_id);
                if (del_result == OK) {
                    deleted_count++;
                }
            }
        }

        /* 更新计数器 */
        g_keymgr_ctx.session_key_counter = SE050_SESSION_KEY_ID_START;

        if (g_keymgr_ctx.active_key_count >= deleted_count) {
            g_keymgr_ctx.active_key_count -= deleted_count;
        } else {
            g_keymgr_ctx.active_key_count = 0U;
        }
    }

    return result;
}

/******************************************************************************
 * 密钥权限管理接口
 ******************************************************************************/

/**
 * @brief 更新密钥权限
 * @param[in] key_id 密钥标识符
 * @param[in] new_permissions 新权限
 * @return OK 成功
 */
error_t se050_key_update_permissions(
    SE05x_KeyID_t key_id,
    uint16_t new_permissions)
{
    error_t result;
    uint8_t apdu[SE050_MAX_APDU_LEN];
    uint8_t response[SE050_MAX_RSP_LEN];
    size_t resp_len = sizeof(response);
    size_t offset = 0U;

    /* 检查初始化状态 */
    result = se050_keymgr_check_init();
    if (result != OK) {
        /* 未初始化 */
    } else {
        /* 检查密钥 ID */
        result = se050_keymgr_check_key_id(key_id);
    }

    if (result == OK) {
        /* 构建 APDU 命令 */
        result = se050_keymgr_build_apdu_header(
            apdu, &offset,
            SE050_INS_MGMT,
            0x02U,  /* 更新属性 */
            0x01U);  /* 更新权限 */
    }

    if (result == OK) {
        /* 写入密钥 ID */
        result = se050_keymgr_write_key_id(apdu, &offset, key_id);
    }

    if (result == OK) {
        /* 写入新权限 */
        result = se050_keymgr_write_permissions(apdu, &offset, new_permissions);
    }

    if (result == OK) {
        /* 更新 Lc 字段 */
        apdu[4U] = (uint8_t)((offset - 5U) & 0xFFU);

        /* 发送 APDU */
        result = se050_keymgr_transceive(apdu, offset, response, &resp_len);
    }

    if (result == OK) {
        /* 检查响应状态 */
        if ((resp_len >= 2U) && (response[resp_len - 2U] == 0x90U) &&
            (response[resp_len - 1U] == 0x00U)) {
            /* 成功 */
        } else {
            result = ERROR_SE_FAILURE;
        }
    }

    /* 安全清除 APDU 缓冲区 */
    (void)se050_keymgr_secure_clear(apdu, sizeof(apdu));

    return result;
}
