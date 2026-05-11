/******************************************************************************
 * @file    ccc.h
 * @brief   CCC Digital Key R3 协议栈头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 * 
 * @note    实现 CCC Digital Key Release 3 规范
 *          - CCC-TS-DigitalKey-R3 主规范
 *          - 支持 BLE/NFC/UWB 三模式
 *          - 支持主钥匙 + 朋友钥匙 + 客人钥匙
 ******************************************************************************/

#ifndef CCC_H
#define CCC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dkcs.h"

/******************************************************************************
 * CCC 特定错误码
 ******************************************************************************/
typedef enum {
    CCC_OK = 0,
    CCC_ERROR_CERT_INVALID = -100,
    CCC_ERROR_CERT_EXPIRED = -101,
    CCC_ERROR_SIGNATURE_INVALID = -102,
    CCC_ERROR_CHALLENGE_FAILED = -103,
    CCC_ERROR_KEY_AGREEMENT_FAILED = -104,
    CCC_ERROR_SESSION_ESTABLISH_FAILED = -105,
    CCC_ERROR_INVALID_MESSAGE = -106,
    CCC_ERROR_VERSION_MISMATCH = -107,
    CCC_ERROR_DEVICE_NOT_AUTHORIZED = -108,
    CCC_ERROR_KEY_LIMIT_REACHED = -109,
    CCC_ERROR_INVALID_KEY_ID = -110,
} ccc_error_t;

/******************************************************************************
 * CCC 版本定义
 ******************************************************************************/
#define CCC_VERSION_MAJOR           3
#define CCC_VERSION_MINOR           0
#define CCC_VERSION_REVISION        0

/******************************************************************************
 * CCC 固定长度定义
 ******************************************************************************/
#define CCC_EPHEMERAL_KEY_LEN       32
#define CCC_SHARED_SECRET_LEN       32
#define CCC_MASTER_SECRET_LEN       48
#define CCC_SESSION_KEY_LEN         16
#define CCC_MAC_KEY_LEN             16
#define CCC_NONCE_LEN               8
#define CCC_CHALLENGE_LEN           32
#define CCC_DEVICE_ID_LEN           16
#define CCC_KEY_ID_LEN              16
#define CCC_MAX_CERT_CHAIN_LEN      3
#define CCC_MAX_APDU_LEN            247
#define CCC_MAX_FRAGMENT_SIZE       247
#define CCC_MAX_KEYS_PER_VEHICLE    16

/******************************************************************************
 * CCC 命令定义 (Digital Key Service)
 ******************************************************************************/
typedef enum {
    /* 标准命令 */
    CCC_CMD_PAIRING_REQUEST = 0x01,
    CCC_CMD_PAIRING_RESPONSE = 0x02,
    CCC_CMD_AUTHENTICATION_REQUEST = 0x03,
    CCC_CMD_AUTHENTICATION_RESPONSE = 0x04,
    CCC_CMD_CONTROL_FLOW = 0x05,
    CCC_CMD_SESSION_ESTABLISHMENT = 0x06,
    CCC_CMD_SECURE_MESSAGE = 0x07,
    
    /* 车辆控制命令 */
    CCC_CMD_VEHICLE_UNLOCK = 0x11,
    CCC_CMD_VEHICLE_LOCK = 0x12,
    CCC_CMD_ENGINE_START = 0x13,
    CCC_CMD_ENGINE_STOP = 0x14,
    CCC_CMD_TRUNK_UNLOCK = 0x15,
    CCC_CMD_TRUNK_LOCK = 0x16,
    CCC_CMD_CLIMATE_ON = 0x17,
    CCC_CMD_CLIMATE_OFF = 0x18,
    CCC_CMD_CHARGE_PORT_OPEN = 0x19,
    CCC_CMD_CHARGE_PORT_CLOSE = 0x1A,
    
    /* 钥匙管理命令 */
    CCC_CMD_KEY_PROVISIONING = 0x21,
    CCC_CMD_KEY_IMPORT = 0x22,
    CCC_CMD_KEY_EXPORT = 0x23,
    CCC_CMD_KEY_REVOCATION = 0x24,
    CCC_CMD_KEY_INFO = 0x25,
    CCC_CMD_KEY_SHARING = 0x26,
    
    /* 状态查询命令 */
    CCC_CMD_STATUS_REQUEST = 0x31,
    CCC_CMD_STATUS_RESPONSE = 0x32,
    
    /* OTA 命令 */
    CCC_CMD_OTA_START = 0x41,
    CCC_CMD_OTA_DATA = 0x42,
    CCC_CMD_OTA_COMPLETE = 0x43,
    CCC_CMD_OTA_STATUS = 0x44,
} ccc_command_t;

/******************************************************************************
 * CCC 会话状态
 ******************************************************************************/
typedef enum {
    CCC_SESSION_STATE_IDLE = 0,
    CCC_SESSION_STATE_PAIRING = 1,
    CCC_SESSION_STATE_AUTHENTICATING = 2,
    CCC_SESSION_STATE_ESTABLISHING = 3,
    CCC_SESSION_STATE_ACTIVE = 4,
    CCC_SESSION_STATE_TERMINATING = 5,
} ccc_session_state_t;

/******************************************************************************
 * CCC 配对状态
 ******************************************************************************/
typedef enum {
    CCC_PAIRING_STATE_IDLE = 0,
    CCC_PAIRING_STATE_WAITING_DEVICE = 1,
    CCC_PAIRING_STATE_DEVICE_VERIFIED = 2,
    CCC_PAIRING_STATE_WAITING_VEHICLE = 3,
    CCC_PAIRING_STATE_COMPLETE = 4,
    CCC_PAIRING_STATE_FAILED = 5,
} ccc_pairing_state_t;

/******************************************************************************
 * CCC 证书类型
 ******************************************************************************/
typedef enum {
    CCC_CERT_TYPE_DEVICE = 0x01,
    CCC_CERT_TYPE_VEHICLE = 0x02,
    CCC_CERT_TYPE_SERVER = 0x03,
    CCC_CERT_TYPE_INTERMEDIATE = 0x04,
    CCC_CERT_TYPE_ROOT = 0x05,
} ccc_cert_type_t;

/******************************************************************************
 * CCC 证书结构
 ******************************************************************************/
typedef struct {
    ccc_cert_type_t type;
    uint8_t serial_number[16];
    uint8_t issuer_id[16];
    uint8_t subject_id[16];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t public_key[CCC_EPHEMERAL_KEY_LEN * 2 + 1];
    uint8_t signature[64];
} ccc_certificate_t;

/******************************************************************************
 * CCC 证书链结构
 ******************************************************************************/
typedef struct {
    ccc_certificate_t certs[CCC_MAX_CERT_CHAIN_LEN];
    uint8_t cert_count;
} ccc_cert_chain_t;

/******************************************************************************
 * CCC 会话上下文
 ******************************************************************************/
typedef struct {
    ccc_session_state_t state;
    ccc_pairing_state_t pairing_state;
    
    /* 密钥材料 */
    uint8_t device_private_key[CCC_EPHEMERAL_KEY_LEN];
    uint8_t device_public_key[CCC_EPHEMERAL_KEY_LEN * 2 + 1];
    uint8_t vehicle_public_key[CCC_EPHEMERAL_KEY_LEN * 2 + 1];
    uint8_t ephemeral_private_key[CCC_EPHEMERAL_KEY_LEN];
    uint8_t ephemeral_public_key[CCC_EPHEMERAL_KEY_LEN * 2 + 1];
    
    /* 会话密钥 */
    uint8_t shared_secret[CCC_SHARED_SECRET_LEN];
    uint8_t master_secret[CCC_MASTER_SECRET_LEN];
    uint8_t session_key_enc[CCC_SESSION_KEY_LEN];
    uint8_t session_key_mac[CCC_MAC_KEY_LEN];
    
    /* 计数器和随机数 */
    uint64_t session_nonce;
    uint32_t message_counter;
    uint8_t challenge[CCC_CHALLENGE_LEN];
    
    /* 证书 */
    ccc_cert_chain_t device_cert_chain;
    ccc_cert_chain_t vehicle_cert_chain;
    
    /* 会话参数 */
    uint32_t session_timeout_ms;
    uint32_t last_activity;
    bool is_secure_session;
} ccc_session_context_t;

/******************************************************************************
 * CCC 配对配置
 ******************************************************************************/
typedef struct {
    uint8_t vehicle_vin[VIN_LEN];
    uint8_t device_id[CCC_DEVICE_ID_LEN];
    ccc_cert_chain_t device_cert_chain;
    key_role_t requested_role;
    uint16_t requested_permissions;
    bool require_user_auth;
    bool require_biometric;
} ccc_pairing_config_t;

/******************************************************************************
 * CCC 车辆状态响应
 ******************************************************************************/
typedef struct {
    uint8_t vehicle_lock_status;
    uint8_t engine_status;
    uint8_t door_status;
    uint8_t trunk_status;
    uint8_t window_status;
    uint8_t charge_port_status;
    uint8_t fuel_battery_level;
    int8_t interior_temperature;
    uint32_t odometer;
    uint8_t alarm_status;
} ccc_vehicle_status_t;

/******************************************************************************
 * CCC 安全元件接口
 ******************************************************************************/
typedef struct {
    error_t (*init)(void);
    error_t (*deinit)(void);
    error_t (*generate_key_pair)(uint8_t *pub_key, uint8_t *priv_key);
    error_t (*sign)(const uint8_t *data, size_t len, 
                         const uint8_t *priv_key, 
                         uint8_t *signature);
    error_t (*verify)(const uint8_t *data, size_t len,
                           const uint8_t *pub_key,
                           const uint8_t *signature);
    error_t (*derive_shared_secret)(const uint8_t *priv_key,
                                         const uint8_t *pub_key,
                                         uint8_t *shared_secret);
    error_t (*secure_store)(uint32_t key_id, 
                                  const uint8_t *data, 
                                  size_t len);
    error_t (*secure_read)(uint32_t key_id,
                                uint8_t *data,
                                size_t *len);
} ccc_se_interface_t;

/******************************************************************************
 * CCC API 函数声明
 ******************************************************************************/

/**
 * @brief 初始化 CCC 协议栈
 */
error_t ccc_init(const ccc_se_interface_t *se_interface);

/**
 * @brief 反初始化 CCC 协议栈
 */
error_t ccc_deinit(void);

/**
 * @brief 创建配对会话
 */
error_t ccc_create_pairing_session(
    const ccc_pairing_config_t *config,
    ccc_session_context_t **session
);

/**
 * @brief 开始配对流程 (Device -> Vehicle)
 */
error_t ccc_start_pairing(
    ccc_session_context_t *session,
    uint8_t *request_data,
    size_t *request_len
);

/**
 * @brief 处理配对响应 (Vehicle -> Device)
 */
error_t ccc_process_pairing_response(
    ccc_session_context_t *session,
    const uint8_t *response_data,
    size_t response_len
);

/**
 * @brief 完成配对确认
 */
error_t ccc_complete_pairing(
    ccc_session_context_t *session,
    uint8_t *confirmation_data,
    size_t *confirmation_len
);

/**
 * @brief 建立安全会话
 */
error_t ccc_establish_session(
    ccc_session_context_t *session,
    const uint8_t *vehicle_public_key,
    const uint8_t *vehicle_challenge
);

/**
 * @brief 加密消息
 */
error_t ccc_encrypt_message(
    ccc_session_context_t *session,
    ccc_command_t command,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t *encrypted_data,
    size_t *encrypted_len
);

/**
 * @brief 解密消息
 */
error_t ccc_decrypt_message(
    ccc_session_context_t *session,
    const uint8_t *encrypted_data,
    size_t encrypted_len,
    ccc_command_t *command,
    uint8_t *payload,
    size_t *payload_len
);

/**
 * @brief 发送车辆控制命令
 */
error_t ccc_send_vehicle_command(
    ccc_session_context_t *session,
    ccc_command_t command,
    const uint8_t *params,
    size_t params_len,
    uint8_t *response,
    size_t *response_len
);

/**
 * @brief 获取车辆状态
 */
error_t ccc_get_vehicle_status(
    ccc_session_context_t *session,
    ccc_vehicle_status_t *status
);

/**
 * @brief 导入数字钥匙 (从友情钥匙)
 */
error_t ccc_import_key(
    ccc_session_context_t *session,
    const uint8_t *key_data,
    size_t key_len,
    key_info_t *key_info
);

/**
 * @brief 分享钥匙给其他设备
 */
error_t ccc_share_key(
    ccc_session_context_t *session,
    const uint8_t *target_key_id,
    key_role_t target_role,
    uint16_t permissions,
    uint32_t validity_period,
    uint8_t *share_data,
    size_t *share_len
);

/******************************************************************************
 * 基于证书的验证函数声明
 ******************************************************************************/

/**
 * @brief 序列化 CCC 证书为 X.509 DER 格式
 */
error_t ccc_serialize_certificate(const ccc_certificate_t *cert, uint8_t *out, size_t *out_len);

/**
 * @brief 从 X.509 DER 格式解析 CCC 证书
 */
error_t ccc_parse_certificate(const uint8_t *data, size_t data_len, ccc_certificate_t *cert);

/**
 * @brief 验证单个 X.509 证书
 */
error_t ccc_verify_certificate(const ccc_certificate_t *cert, const uint8_t *trusted_pubkey);

/**
 * @brief 验证证书链
 */
error_t ccc_validate_cert_chain(
    const ccc_cert_chain_t *cert_chain,
    const ccc_certificate_t *trusted_root
);

/**
 * @brief 获取证书序列化后的长度
 */
size_t ccc_get_certificate_length(const ccc_certificate_t *cert);

/**
 * @brief 生成自签名证书 (用于测试)
 */
error_t ccc_generate_self_signed_certificate(
    ccc_cert_type_t cert_type,
    const uint8_t *subject_id,
    const uint8_t *key_pair,
    uint32_t validity_days,
    ccc_certificate_t *cert
);

/**
 * @brief 生成随机挑战
 */
error_t ccc_generate_challenge(uint8_t *challenge);

/**
 * @brief 验证挑战响应
 */
error_t ccc_verify_challenge_response(
    const uint8_t *challenge,
    const uint8_t *response,
    const uint8_t *public_key
);

/**
 * @brief 生成 HKDF 密钥
 */
error_t ccc_derive_session_keys(
    const uint8_t *shared_secret,
    const uint8_t *salt,
    uint8_t *enc_key,
    uint8_t *mac_key
);

/**
 * @brief 计算消息 MAC
 */
error_t ccc_compute_mac(
    const uint8_t *mac_key,
    const uint8_t *message,
    size_t message_len,
    uint8_t *mac
);

/**
 * @brief 验证消息 MAC
 */
error_t ccc_verify_mac(
    const uint8_t *mac_key,
    const uint8_t *message,
    size_t message_len,
    const uint8_t *mac
);

/**
 * @brief 回收会话资源
 */
void ccc_destroy_session(ccc_session_context_t *session);

/******************************************************************************
 * CCC UWB 测距扩展 (可选)
 ******************************************************************************/
#ifdef CCC_ENABLE_UWB

typedef struct {
    uint32_t ranging_interval_ms;
    uint8_t preferred_ranging_protocol;
    bool enable_secure_ranging;
    uint8_t uwb_channel;
    uint8_t preamble_code;
} ccc_uwb_config_t;

error_t ccc_uwb_init(const ccc_uwb_config_t *config);
error_t ccc_uwb_start_ranging(ccc_session_context_t *session);
error_t ccc_uwb_stop_ranging(ccc_session_context_t *session);
error_t ccc_uwb_get_distance(float *distance_m, float *accuracy_m);

#endif /* CCC_ENABLE_UWB */

/******************************************************************************
 * CCC BLE 传输层扩展
 ******************************************************************************/
#ifdef CCC_ENABLE_BLE

typedef enum {
    CCC_BLE_STATE_DISCONNECTED = 0,
    CCC_BLE_STATE_ADVERTISING = 1,
    CCC_BLE_STATE_SCANNING = 2,
    CCC_BLE_STATE_CONNECTING = 3,
    CCC_BLE_STATE_CONNECTED = 4,
    CCC_BLE_STATE_ENCRYPTED = 5,
    CCC_BLE_STATE_PAIRING = 6,
} ccc_ble_state_t;

error_t ccc_ble_init(void);
error_t ccc_ble_deinit(void);
error_t ccc_ble_start_advertising(const uint8_t *vin);
error_t ccc_ble_stop_advertising(void);
error_t ccc_ble_connect(const uint8_t *vehicle_mac);
error_t ccc_ble_disconnect(void);
error_t ccc_ble_send_data(const uint8_t *data, size_t len);
error_t ccc_ble_receive_data(uint8_t *data, size_t *len, uint32_t timeout_ms);
error_t ccc_ble_set_event_callback(void (*callback)(uint32_t event, void *data));

#endif /* CCC_ENABLE_BLE */

/******************************************************************************
 * CCC NFC 传输层扩展
 ******************************************************************************/
#ifdef CCC_ENABLE_NFC

typedef enum {
    CCC_NFC_TYPE_POLLER = 0,
    CCC_NFC_TYPE_LISTENER = 1,
} ccc_nfc_mode_t;

error_t ccc_nfc_init(ccc_nfc_mode_t mode);
error_t ccc_nfc_deinit(void);
error_t ccc_nfc_start_emulation(const uint8_t *aid, size_t aid_len);
error_t ccc_nfc_stop_emulation(void);
error_t ccc_nfc_transceive(const uint8_t *apdu, size_t apdu_len,
                                 uint8_t *response, size_t *response_len);

#endif /* CCC_ENABLE_NFC */

#ifdef __cplusplus
}
#endif

#endif /* CCC_H */
