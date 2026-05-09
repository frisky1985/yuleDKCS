/******************************************************************************
 * @file    icce.h
 * @brief   ICCE (智慧车联产业生态联盟) 协议栈头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 * 
 * @note    实现 ICCE Digital Key 规范
 *          - ICCE-DK-001 数字钥匙技术规范
 *          - ICCE-SEC-001 安全架构规范
 *          - 支持 BLE/NFC 双模式
 *          - 适配国产 SE 芯片 (华大、国密、联发科)
 ******************************************************************************/

#ifndef ICCE_H
#define ICCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dkcs.h"

/******************************************************************************
 * ICCE 版本信息
 ******************************************************************************/
#define ICCE_VERSION_MAJOR          2
#define ICCE_VERSION_MINOR          0
#define ICCE_VERSION_PATCH          0

/******************************************************************************
 * ICCE 固定长度定义
 ******************************************************************************/
#define ICCE_DEVICE_ID_LEN          16
#define ICCE_KEY_ID_LEN             16
#define ICCE_VEHICLE_ID_LEN         17
#define ICCE_SE_ID_LEN              16
#define ICCE_APDU_MAX_LEN           255
#define ICCE_CERT_MAX_LEN           1024
#define ICCE_SIGNATURE_LEN          64
#define ICCE_ECC_SM2_KEY_LEN        32
#define ICCE_ECC_SM2_PUB_KEY_LEN    65
#define ICCE_SM4_KEY_LEN            16
#define ICCE_SM4_IV_LEN             16
#define ICCE_SESSION_KEY_LEN        16
#define ICCE_MAC_LEN                16
#define ICCE_CHALLENGE_LEN          16
#define ICCE_MAX_FRAGMENT_SIZE      247

/******************************************************************************
 * ICCE 响应码定义
 ******************************************************************************/
typedef enum {
    ICCE_OK = 0x0000,
    ICCE_ERR_INVALID_CMD = 0x0001,
    ICCE_ERR_INVALID_PARAM = 0x0002,
    ICCE_ERR_INVALID_LENGTH = 0x0003,
    ICCE_ERR_AUTH_FAILED = 0x0004,
    ICCE_ERR_SE_COMM = 0x0005,
    ICCE_ERR_CRYPTO_FAIL = 0x0006,
    ICCE_ERR_KEY_NOT_FOUND = 0x0007,
    ICCE_ERR_KEY_REVOKED = 0x0008,
    ICCE_ERR_KEY_EXPIRED = 0x0009,
    ICCE_ERR_SESSION_INVALID = 0x000A,
    ICCE_ERR_TIMEOUT = 0x000B,
    ICCE_ERR_BUSY = 0x000C,
    ICCE_ERR_MEMORY = 0x000D,
    ICCE_ERR_INTERNAL = 0x00FF,
} icce_error_code_t;

/******************************************************************************
 * ICCE 命令类型 (CLA = 0x00)
 ******************************************************************************/
typedef enum {
    /* 管理命令 */
    ICCE_CMD_SELECT = 0xA4,
    ICCE_CMD_VERIFY_PIN = 0x20,
    ICCE_CMD_CHANGE_PIN = 0x24,
    
    /* 配对命令 */
    ICCE_CMD_PAIRING_INIT = 0xE0,
    ICCE_CMD_PAIRING_AUTH = 0xE1,
    ICCE_CMD_PAIRING_CONFIRM = 0xE2,
    ICCE_CMD_PAIRING_COMPLETE = 0xE3,
    
    /* 会话命令 */
    ICCE_CMD_SESSION_START = 0xE4,
    ICCE_CMD_SESSION_ESTABLISH = 0xE5,
    ICCE_CMD_SESSION_CLOSE = 0xE6,
    
    /* 钥匙管理 */
    ICCE_CMD_KEY_IMPORT = 0xE7,
    ICCE_CMD_KEY_EXPORT = 0xE8,
    ICCE_CMD_KEY_DELETE = 0xE9,
    ICCE_CMD_KEY_INFO = 0xEA,
    
    /* 车辆控制 */
    ICCE_CMD_VEHICLE_UNLOCK = 0xEB,
    ICCE_CMD_VEHICLE_LOCK = 0xEC,
    ICCE_CMD_ENGINE_START = 0xED,
    ICCE_CMD_ENGINE_STOP = 0xEE,
    ICCE_CMD_TRUNK_OPEN = 0xEF,
    ICCE_CMD_WINDOWS_CTRL = 0xF0,
    ICCE_CMD_CLIMATE_CTRL = 0xF1,
    ICCE_CMD_CHARGE_CTRL = 0xF2,
    
    /* 状态查询 */
    ICCE_CMD_STATUS_GET = 0xF3,
    ICCE_CMD_STATUS_SUBSCRIBE = 0xF4,
    ICCE_CMD_STATUS_UNSUBSCRIBE = 0xF5,
    
    /* OTA 命令 */
    ICCE_CMD_OTA_PREPARE = 0xF6,
    ICCE_CMD_OTA_TRANSFER = 0xF7,
    ICCE_CMD_OTA_VERIFY = 0xF8,
    ICCE_CMD_OTA_COMMIT = 0xF9,
    
    /* 设备管理 */
    ICCE_CMD_DEVICE_INFO = 0xFA,
    ICCE_CMD_DEVICE_BIND = 0xFB,
    ICCE_CMD_DEVICE_UNBIND = 0xFC,
    
    /* 调试命令 */
    ICCE_CMD_DEBUG_GET_LOG = 0xFD,
    ICCE_CMD_DEBUG_SELF_TEST = 0xFE,
} icce_command_t;

/******************************************************************************
 * ICCE 加密算法类型
 ******************************************************************************/
typedef enum {
    ICCE_CRYPTO_SM2 = 0x00,         /**< 国密 SM2 椭圆曲线 */
    ICCE_CRYPTO_SM4 = 0x01,         /**< 国密 SM4 对称加密 */
    ICCE_CRYPTO_ECC_P256 = 0x02,    /**< 标准 ECC P-256 (兼容模式) */
    ICCE_CRYPTO_AES128 = 0x03,      /**< 标准 AES-128 (兼容模式) */
} icce_crypto_type_t;

/******************************************************************************
 * ICCE SE 类型
 ******************************************************************************/
typedef enum {
    ICCE_SE_TYPE_UNKNOWN = 0x00,
    ICCE_SE_TYPE_HW = 0x01,         /**< 硬件 SE */
    ICCE_SE_TYPE_SIM = 0x02,        /**< SIM 卡 SE */
    ICCE_SE_TYPE_ESE = 0x03,        /**< 嵌入式 eSE */
    ICCE_SE_TYPE_TEE = 0x04,        /**< TEE 环境 */
} icce_se_type_t;

/******************************************************************************
 * ICCE 配对模式
 ******************************************************************************/
typedef enum {
    ICCE_PAIRING_MODE_STANDARD = 0,     /**< 标准配对模式 */
    ICCE_PAIRING_MODE_FAST = 1,         /**< 快速配对模式 (预配置) */
    ICCE_PAIRING_MODE_OFFLINE = 2,      /**< 离线配对模式 */
} icce_pairing_mode_t;

/******************************************************************************
 * ICCE 会话状态
 ******************************************************************************/
typedef enum {
    ICCE_SESSION_CLOSED = 0,
    ICCE_SESSION_OPENING = 1,
    ICCE_SESSION_HANDSHAKE = 2,
    ICCE_SESSION_ESTABLISHED = 3,
    ICCE_SESSION_RENEGOTIATING = 4,
    ICCE_SESSION_CLOSING = 5,
} icce_session_state_t;

/******************************************************************************
 * ICCE APDU 命令头结构
 ******************************************************************************/
typedef struct __attribute__((packed)) {
    uint8_t cla;                    /**< 命令类别 */
    uint8_t ins;                    /**< 指令码 */
    uint8_t p1;                     /**< 参数1 */
    uint8_t p2;                     /**< 参数2 */
    uint8_t lc;                     /**< 数据长度 */
    uint8_t data[ICCE_APDU_MAX_LEN];/**< 数据字段 */
    uint8_t le;                     /**< 期望响应长度 */
} icce_apdu_t;

/******************************************************************************
 * ICCE APDU 响应结构
 ******************************************************************************/
typedef struct __attribute__((packed)) {
    uint8_t data[ICCE_APDU_MAX_LEN];/**< 响应数据 */
    uint8_t sw1;                    /**< 状态字1 */
    uint8_t sw2;                    /**< 状态字2 */
    uint16_t len;                   /**< 数据长度 */
} icce_apdu_response_t;

/******************************************************************************
 * ICCE 证书结构
 ******************************************************************************/
typedef struct {
    uint8_t version;
    uint8_t cert_type;
    uint16_t issuer_len;
    uint8_t issuer[64];
    uint16_t subject_len;
    uint8_t subject[64];
    uint8_t device_id[ICCE_DEVICE_ID_LEN];
    uint8_t public_key[ICCE_ECC_SM2_PUB_KEY_LEN];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t signature[ICCE_SIGNATURE_LEN];
    uint16_t cert_len;
} icce_certificate_t;

/******************************************************************************
 * ICCE 安全通道配置
 ******************************************************************************/
typedef struct {
    icce_crypto_type_t crypto_type;
    icce_se_type_t se_type;
    bool enable_mutual_auth;
    bool enable_channel_binding;
    uint32_t session_timeout_ms;
    uint8_t max_retry_count;
} icce_security_config_t;

/******************************************************************************
 * ICCE 会话上下文
 ******************************************************************************/
typedef struct {
    icce_session_state_t state;
    uint32_t session_id;
    uint8_t device_id[ICCE_DEVICE_ID_LEN];
    uint8_t vehicle_id[ICCE_VEHICLE_ID_LEN];
    
    /* 密钥材料 */
    uint8_t device_private_key[ICCE_ECC_SM2_KEY_LEN];
    uint8_t device_public_key[ICCE_ECC_SM2_PUB_KEY_LEN];
    uint8_t vehicle_public_key[ICCE_ECC_SM2_PUB_KEY_LEN];
    
    /* 会话密钥 - SM4 或 AES */
    uint8_t session_key_enc[ICCE_SM4_KEY_LEN];
    uint8_t session_key_mac[ICCE_SM4_KEY_LEN];
    uint8_t iv[ICCE_SM4_IV_LEN];
    
    /* 计数器 */
    uint32_t cmd_counter;
    uint32_t resp_counter;
    
    /* 安全参数 */
    icce_security_config_t security;
    uint32_t last_activity;
    bool is_authenticated;
} icce_session_context_t;

/******************************************************************************
 * ICCE 配对信息
 ******************************************************************************/
typedef struct {
    icce_pairing_mode_t mode;
    uint8_t vehicle_id[ICCE_VEHICLE_ID_LEN];
    uint8_t device_id[ICCE_DEVICE_ID_LEN];
    icce_se_type_t se_type;
    icce_crypto_type_t crypto_preference;
    key_role_t requested_role;
    uint16_t permissions;
    uint8_t pin_code[6];
    bool require_biometric;
} icce_pairing_info_t;

/******************************************************************************
 * ICCE 车辆状态
 ******************************************************************************/
typedef struct {
    uint8_t lock_status;            /**< 锁车状态 */
    uint8_t engine_status;          /**< 发动机状态 */
    uint8_t door_status[4];         /**< 四门状态 */
    uint8_t window_status[4];       /**< 车窗状态 */
    uint8_t trunk_status;           /**< 后备箱状态 */
    uint8_t fuel_level;             /**< 油量/电量 */
    int8_t temperature;             /**< 车内温度 */
    uint32_t mileage;               /**< 里程 */
    uint8_t charge_status;          /**< 充电状态 */
} icce_vehicle_status_t;

/******************************************************************************
 * ICCE OTA 信息
 ******************************************************************************/
typedef struct {
    uint32_t firmware_version;
    uint32_t firmware_size;
    uint8_t firmware_hash[32];
    uint8_t signature[ICCE_SIGNATURE_LEN];
    uint8_t component_id;
} icce_ota_info_t;

/******************************************************************************
 * ICCE SE 接口回调
 ******************************************************************************/
typedef struct {
    error_t (*open_channel)(uint8_t *atr, size_t *atr_len);
    error_t (*close_channel)(void);
    error_t (*transmit)(const uint8_t *apdu, size_t apdu_len,
                              uint8_t *response, size_t *response_len);
    error_t (*select_applet)(const uint8_t *aid, size_t aid_len);
    error_t (*verify_pin)(const uint8_t *pin, size_t pin_len);
    error_t (*generate_sm2_key)(uint8_t *pub_key, uint8_t *priv_key);
    error_t (*sm2_sign)(const uint8_t *data, size_t len,
                              uint8_t *signature);
    error_t (*sm2_verify)(const uint8_t *data, size_t len,
                                const uint8_t *pub_key,
                                const uint8_t *signature);
    error_t (*sm4_encrypt)(const uint8_t *key,
                                 const uint8_t *iv,
                                 const uint8_t *plaintext,
                                 size_t len,
                                 uint8_t *ciphertext);
    error_t (*sm4_decrypt)(const uint8_t *key,
                                 const uint8_t *iv,
                                 const uint8_t *ciphertext,
                                 size_t len,
                                 uint8_t *plaintext);
} icce_se_interface_t;

/******************************************************************************
 * ICCE API 函数声明
 ******************************************************************************/

/**
 * @brief 初始化 ICCE 协议栈
 */
error_t icce_init(const icce_se_interface_t *se_if);

/**
 * @brief 反初始化 ICCE 协议栈
 */
error_t icce_deinit(void);

/**
 * @brief 构建 APDU 命令
 */
error_t icce_build_apdu(
    icce_apdu_t *apdu,
    uint8_t cla,
    icce_command_t ins,
    uint8_t p1,
    uint8_t p2,
    const uint8_t *data,
    uint8_t lc,
    uint8_t le
);

/**
 * @brief 发送 APDU 命令
 */
error_t icce_send_apdu(
    icce_session_context_t *session,
    const icce_apdu_t *apdu,
    icce_apdu_response_t *response
);

/**
 * @brief 解析 APDU 响应
 */
error_t icce_parse_response(
    const uint8_t *raw_data,
    size_t raw_len,
    icce_apdu_response_t *response
);

/**
 * @brief 创建配对会话
 */
error_t icce_pairing_init(
    const icce_pairing_info_t *info,
    icce_session_context_t **session
);

/**
 * @brief 执行配对认证
 */
error_t icce_pairing_authenticate(
    icce_session_context_t *session,
    const icce_certificate_t *device_cert,
    const icce_certificate_t *vehicle_cert,
    uint8_t *auth_data,
    size_t *auth_len
);

/**
 * @brief 完成配对
 */
error_t icce_pairing_complete(
    icce_session_context_t *session,
    const uint8_t *confirmation,
    size_t confirm_len
);

/**
 * @brief 建立安全会话
 */
error_t icce_session_start(
    icce_session_context_t *session,
    const icce_security_config_t *config
);

/**
 * @brief 关闭会话
 */
error_t icce_session_close(icce_session_context_t *session);

/**
 * @brief 加密 APDU 数据
 */
error_t icce_encrypt_apdu(
    icce_session_context_t *session,
    const uint8_t *plaintext,
    size_t plain_len,
    uint8_t *ciphertext,
    size_t *cipher_len
);

/**
 * @brief 解密 APDU 数据
 */
error_t icce_decrypt_apdu(
    icce_session_context_t *session,
    const uint8_t *ciphertext,
    size_t cipher_len,
    uint8_t *plaintext,
    size_t *plain_len
);

/**
 * @brief 计算 MAC
 */
error_t icce_compute_mac(
    icce_session_context_t *session,
    const uint8_t *data,
    size_t len,
    uint8_t *mac
);

/**
 * @brief 验证 MAC
 */
error_t icce_verify_mac(
    icce_session_context_t *session,
    const uint8_t *data,
    size_t len,
    const uint8_t *mac
);

/**
 * @brief 车辆控制命令
 */
error_t icce_vehicle_control(
    icce_session_context_t *session,
    icce_command_t cmd,
    const uint8_t *params,
    size_t params_len,
    uint8_t *response,
    size_t *response_len
);

/**
 * @brief 获取车辆状态
 */
error_t icce_get_vehicle_status(
    icce_session_context_t *session,
    icce_vehicle_status_t *status
);

/**
 * @brief 导入数字钥匙
 */
error_t icce_key_import(
    icce_session_context_t *session,
    const uint8_t *key_data,
    size_t key_len,
    key_info_t *key_info
);

/**
 * @brief 导出数字钥匙
 */
error_t icce_key_export(
    icce_session_context_t *session,
    const uint8_t *key_id,
    uint8_t *key_data,
    size_t *key_len
);

/**
 * @brief 删除数字钥匙
 */
error_t icce_key_delete(
    icce_session_context_t *session,
    const uint8_t *key_id
);

/**
 * @brief OTA 更新准备
 */
error_t icce_ota_prepare(
    icce_session_context_t *session,
    const icce_ota_info_t *ota_info
);

/**
 * @brief OTA 数据传输
 */
error_t icce_ota_transfer(
    icce_session_context_t *session,
    uint32_t offset,
    const uint8_t *data,
    size_t len
);

/**
 * @brief OTA 验证并提交
 */
error_t icce_ota_commit(
    icce_session_context_t *session,
    uint8_t *result
);

/**
 * @brief SM2 共享秘钠计算
 */
error_t icce_sm2_compute_shared_secret(
    const uint8_t *priv_key,
    const uint8_t *pub_key,
    uint8_t *shared_secret
);

/**
 * @brief SM4 加密 (含填充)
 */
error_t icce_sm4_encrypt_cbc(
    const uint8_t *key,
    const uint8_t *iv,
    const uint8_t *plaintext,
    size_t plain_len,
    uint8_t *ciphertext,
    size_t *cipher_len
);

/**
 * @brief SM4 解密 (含填充)
 */
error_t icce_sm4_decrypt_cbc(
    const uint8_t *key,
    const uint8_t *iv,
    const uint8_t *ciphertext,
    size_t cipher_len,
    uint8_t *plaintext,
    size_t *plain_len
);

/**
 * @brief 解析错误码
 */
const char* icce_error_to_string(icce_error_code_t err);

/**
 * @brief 释放会话资源
 */
void icce_session_destroy(icce_session_context_t *session);

#ifdef __cplusplus
}
#endif

#endif /* ICCE_H */
