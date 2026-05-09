/******************************************************************************
 * @file    iccoa.h
 * @brief   ICCOA (智慧车联开放联盟) 协议栈头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 * 
 * @note    实现 ICCOA Digital Key 规范
 *          - ICCOA-DK-001 数字钥匙技术规范
 *          - 支持 BLE/NFC 双模式
 *          - 与主流手机厂商兼容 (小米、OPPO、vivo、一加)
 ******************************************************************************/

#ifndef ICCOA_H
#define ICCOA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dkcs.h"

/******************************************************************************
 * ICCOA 版本信息
 ******************************************************************************/
#define ICCOA_VERSION_MAJOR         1
#define ICCOA_VERSION_MINOR         2
#define ICCOA_VERSION_PATCH         0

/******************************************************************************
 * ICCOA 固定长度定义
 ******************************************************************************/
#define ICCOA_DEVICE_ID_LEN         16
#define ICCOA_KEY_ID_LEN            16
#define ICCOA_TOKEN_LEN             32
#define ICCOA_MAC_LEN               16
#define ICCOA_NONCE_LEN             8
#define ICCOA_CHALLENGE_LEN         16
#define ICCOA_ECC_P256_KEY_LEN      32
#define ICCOA_ECC_P256_PUB_KEY_LEN  65
#define ICCOA_AES_KEY_LEN           16
#define ICCOA_AES_IV_LEN            16
#define ICCOA_MAX_PAYLOAD_LEN       1024
#define ICCOA_MAX_FRAGMENT_SIZE     512
#define ICCOA_MAX_BT_ADDR_LEN       6

/******************************************************************************
 * ICCOA 命令类型 (TLV 格式)
 ******************************************************************************/
typedef enum {
    /* 系统命令 (0x00-0x0F) */
    ICCOA_CMD_GET_VERSION = 0x00,
    ICCOA_CMD_GET_DEVICE_INFO = 0x01,
    ICCOA_CMD_PING = 0x02,
    ICCOA_CMD_RESET = 0x03,
    ICCOA_CMD_GET_STATUS = 0x04,
    
    /* 配对命令 (0x10-0x1F) */
    ICCOA_CMD_PAIRING_START = 0x10,
    ICCOA_CMD_PAIRING_EXCHANGE_KEY = 0x11,
    ICCOA_CMD_PAIRING_AUTH = 0x12,
    ICCOA_CMD_PAIRING_CONFIRM = 0x13,
    ICCOA_CMD_PAIRING_COMPLETE = 0x14,
    ICCOA_CMD_PAIRING_CANCEL = 0x15,
    
    /* 会话命令 (0x20-0x2F) */
    ICCOA_CMD_SESSION_CREATE = 0x20,
    ICCOA_CMD_SESSION_AUTH = 0x21,
    ICCOA_CMD_SESSION_RENEW = 0x22,
    ICCOA_CMD_SESSION_CLOSE = 0x23,
    
    /* 钥匙管理 (0x30-0x3F) */
    ICCOA_CMD_KEY_REGISTER = 0x30,
    ICCOA_CMD_KEY_ACTIVATE = 0x31,
    ICCOA_CMD_KEY_SUSPEND = 0x32,
    ICCOA_CMD_KEY_RESUME = 0x33,
    ICCOA_CMD_KEY_DELETE = 0x34,
    ICCOA_CMD_KEY_LIST = 0x35,
    ICCOA_CMD_KEY_INFO = 0x36,
    ICCOA_CMD_KEY_SHARE = 0x37,
    ICCOA_CMD_KEY_TRANSFER = 0x38,
    
    /* 车辆控制 (0x40-0x4F) */
    ICCOA_CMD_VEHICLE_UNLOCK = 0x40,
    ICCOA_CMD_VEHICLE_LOCK = 0x41,
    ICCOA_CMD_ENGINE_START = 0x42,
    ICCOA_CMD_ENGINE_STOP = 0x43,
    ICCOA_CMD_TRUNK_OPEN = 0x44,
    ICCOA_CMD_TRUNK_CLOSE = 0x45,
    ICCOA_CMD_WINDOWS_UP = 0x46,
    ICCOA_CMD_WINDOWS_DOWN = 0x47,
    ICCOA_CMD_CLIMATE_ON = 0x48,
    ICCOA_CMD_CLIMATE_OFF = 0x49,
    ICCOA_CMD_HORN = 0x4A,
    ICCOA_CMD_LIGHTS = 0x4B,
    ICCOA_CMD_CHARGE_PORT_OPEN = 0x4C,
    ICCOA_CMD_CHARGE_PORT_CLOSE = 0x4D,
    
    /* 状态查询 (0x50-0x5F) */
    ICCOA_CMD_QUERY_STATUS = 0x50,
    ICCOA_CMD_SUBSCRIBE_EVENT = 0x51,
    ICCOA_CMD_UNSUBSCRIBE_EVENT = 0x52,
    ICCOA_CMD_EVENT_NOTIFY = 0x53,
    
    /* OTA 命令 (0x60-0x6F) */
    ICCOA_CMD_OTA_CHECK = 0x60,
    ICCOA_CMD_OTA_DOWNLOAD = 0x61,
    ICCOA_CMD_OTA_INSTALL = 0x62,
    ICCOA_CMD_OTA_STATUS = 0x63,
    ICCOA_CMD_OTA_CONFIRM = 0x64,
    
    /* 扩展命令 (0xF0-0xFF) */
    ICCOA_CMD_VENDOR_SPECIFIC = 0xF0,
    ICCOA_CMD_DEBUG = 0xFE,
    ICCOA_CMD_RESERVED = 0xFF,
} iccoa_command_t;

/******************************************************************************
 * ICCOA 响应码
 ******************************************************************************/
typedef enum {
    ICCOA_OK = 0x00,
    ICCOA_ERR_UNSUPPORTED_CMD = 0x01,
    ICCOA_ERR_INVALID_PARAM = 0x02,
    ICCOA_ERR_INVALID_LENGTH = 0x03,
    ICCOA_ERR_AUTH_FAIL = 0x04,
    ICCOA_ERR_KEY_NOT_FOUND = 0x05,
    ICCOA_ERR_KEY_REVOKED = 0x06,
    ICCOA_ERR_KEY_EXPIRED = 0x07,
    ICCOA_ERR_SESSION_INVALID = 0x08,
    ICCOA_ERR_BUSY = 0x09,
    ICCOA_ERR_TIMEOUT = 0x0A,
    ICCOA_ERR_MEMORY = 0x0B,
    ICCOA_ERR_CRYPTO = 0x0C,
    ICCOA_ERR_TRANSPORT = 0x0D,
    ICCOA_ERR_INTERNAL = 0x0F,
    
    /* 扩展错误码 */
    ICCOA_ERR_PAIRING_CANCELLED = 0x10,
    ICCOA_ERR_PAIRING_TIMEOUT = 0x11,
    ICCOA_ERR_VERSION_MISMATCH = 0x12,
    ICCOA_ERR_SE_UNAVAILABLE = 0x13,
    ICCOA_ERR_PERMISSION_DENIED = 0x14,
    ICCOA_ERR_GEO_FENCE = 0x15,
    ICCOA_ERR_SPEED_LIMIT = 0x16,
} iccoa_error_code_t;

/******************************************************************************
 * ICCOA TLV 类型
 ******************************************************************************/
typedef enum {
    ICCOA_TLV_VERSION = 0x01,
    ICCOA_TLV_DEVICE_ID = 0x02,
    ICCOA_TLV_KEY_ID = 0x03,
    ICCOA TLV_TOKEN = 0x04,
    ICCOA_TLV_PUBLIC_KEY = 0x05,
    ICCOA_TLV_SIGNATURE = 0x06,
    ICCOA_TLV_CERTIFICATE = 0x07,
    ICCOA_TLV_CHALLENGE = 0x08,
    ICCOA_TLV_RESPONSE = 0x09,
    ICCOA_TLV_SESSION_ID = 0x0A,
    ICCOA_TLV_MAC = 0x0B,
    ICCOA TLV_TIMESTAMP = 0x0C,
    ICCOA_TLV_NONCE = 0x0D,
    ICCOA_TLV_PAYLOAD = 0x0E,
    ICCOA_TLV_ERROR_CODE = 0x0F,
    ICCOA_TLV_KEY_ROLE = 0x10,
    ICCOA_TLV_PERMISSIONS = 0x11,
    ICCOA TLV_VALID_FROM = 0x12,
    ICCOA TLV_VALID_UNTIL = 0x13,
    ICCOA_TLV_BT_ADDR = 0x14,
    ICCOA_TLV_BLE_ADV_DATA = 0x15,
    ICCOA TLV_OTA_INFO = 0x16,
    ICCOA TLV_VEHICLE_STATUS = 0x17,
    ICCOA TLV_CONTROL_RESULT = 0x18,
} iccoa_tlv_type_t;

/******************************************************************************
 * ICCOA 会话状态
 ******************************************************************************/
typedef enum {
    ICCOA_SESSION_STATE_IDLE = 0,
    ICCOA_SESSION_STATE_CONNECTING = 1,
    ICCOA_SESSION_STATE_HANDSHAKING = 2,
    ICCOA_SESSION_STATE_AUTHENTICATING = 3,
    ICCOA_SESSION_STATE_ACTIVE = 4,
    ICCOA_SESSION_STATE_RENEWING = 5,
    ICCOA_SESSION_STATE_CLOSING = 6,
} iccoa_session_state_t;

/******************************************************************************
 * ICCOA 配对状态
 ******************************************************************************/
typedef enum {
    ICCOA_PAIRING_STATE_IDLE = 0,
    ICCOA_PAIRING_STATE_STARTED = 1,
    ICCOA_PAIRING_STATE_KEY_EXCHANGED = 2,
    ICCOA_PAIRING_STATE_AUTHENTICATED = 3,
    ICCOA_PAIRING_STATE_CONFIRMED = 4,
    ICCOA_PAIRING_STATE_COMPLETED = 5,
    ICCOA_PAIRING_STATE_FAILED = 6,
} iccoa_pairing_state_t;

/******************************************************************************
 * ICCOA 连接信息
 ******************************************************************************/
typedef enum {
    ICCOA_CONN_BLE = 0,
    ICCOA_CONN_NFC = 1,
    ICCOA_CONN_HYBRID = 2,
} iccoa_connection_type_t;

/******************************************************************************
 * ICCOA TLV 元素结构
 ******************************************************************************/
typedef struct {
    iccoa_tlv_type_t type;
    uint16_t length;
    uint8_t *value;
} iccoa_tlv_t;

/******************************************************************************
 * ICCOA 消息头结构
 ******************************************************************************/
typedef struct __attribute__((packed)) {
    uint8_t magic;                  /**< 魔数 0xA5 */
    uint8_t version;                /**< 协议版本 */
    uint16_t length;                /**< 整个消息长度 */
    uint8_t cmd;                    /**< 命令码 */
    uint8_t flags;                  /**< 标志位 */
    uint16_t sequence;              /**< 序列号 */
} iccoa_message_header_t;

#define ICCOA_MAGIC                 0xA5
#define ICCOA_FLAG_ENCRYPTED        (1 << 0)
#define ICCOA_FLAG_FRAGMENTED       (1 << 1)
#define ICCOA_FLAG_LAST_FRAGMENT    (1 << 2)
#define ICCOA_FLAG_RESPONSE         (1 << 3)
#define ICCOA_FLAG_ERROR            (1 << 4)

/******************************************************************************
 * ICCOA 完整消息结构
 ******************************************************************************/
typedef struct {
    iccoa_message_header_t header;
    iccoa_tlv_t *tlvs;
    uint8_t tlv_count;
    uint8_t *raw_data;
    size_t raw_len;
} iccoa_message_t;

/******************************************************************************
 * ICCOA 会话上下文
 ******************************************************************************/
typedef struct {
    iccoa_session_state_t state;
    iccoa_pairing_state_t pairing_state;
    iccoa_connection_type_t conn_type;
    
    uint32_t session_id;
    uint8_t device_id[ICCOA_DEVICE_ID_LEN];
    uint8_t key_id[ICCOA_KEY_ID_LEN];
    
    /* 密钥材料 */
    uint8_t device_private_key[ICCOA_ECC_P256_KEY_LEN];
    uint8_t device_public_key[ICCOA_ECC_P256_PUB_KEY_LEN];
    uint8_t vehicle_public_key[ICCOA_ECC_P256_PUB_KEY_LEN];
    
    /* 会话密钥 */
    uint8_t session_key_enc[ICCOA_AES_KEY_LEN];
    uint8_t session_key_mac[ICCOA_AES_KEY_LEN];
    
    /* 计数器 */
    uint16_t tx_sequence;
    uint16_t rx_sequence;
    uint8_t nonce[ICCOA_NONCE_LEN];
    
    /* 连接信息 */
    uint8_t bt_address[ICCOA_MAX_BT_ADDR_LEN];
    uint32_t last_activity;
    uint32_t session_timeout_ms;
    
    bool is_secure;
    bool is_authenticated;
} iccoa_session_context_t;

/******************************************************************************
 * ICCOA 配对配置
 ******************************************************************************/
typedef struct {
    uint8_t device_id[ICCOA_DEVICE_ID_LEN];
    uint8_t vehicle_id[ICCOA_DEVICE_ID_LEN];
    uint8_t bt_address[ICCOA_MAX_BT_ADDR_LEN];
    key_role_t requested_role;
    uint16_t permissions;
    uint32_t valid_duration;
    bool enable_ble;
    bool enable_nfc;
    bool require_auth;
} iccoa_pairing_config_t;

/******************************************************************************
 * ICCOA 车辆状态
 ******************************************************************************/
typedef struct {
    uint8_t lock_status;
    uint8_t engine_status;
    uint8_t door_status;
    uint8_t trunk_status;
    uint8_t window_status;
    uint8_t fuel_level;
    int8_t temperature;
    uint8_t battery_voltage;
    uint32_t mileage;
    uint8_t charge_port_status;
    uint8_t alarm_status;
} iccoa_vehicle_status_t;

/******************************************************************************
 * ICCOA OTA 信息
 ******************************************************************************/
typedef struct {
    uint32_t version;
    uint32_t size;
    uint8_t hash[32];
    uint8_t component;
    char description[64];
} iccoa_ota_info_t;

/******************************************************************************
 * ICCOA SE 接口
 ******************************************************************************/
typedef struct {
    error_t (*init)(void);
    error_t (*deinit)(void);
    error_t (*generate_keypair)(uint8_t *pub, uint8_t *priv);
    error_t (*sign)(const uint8_t *data, size_t len, uint8_t *sig);
    error_t (*verify)(const uint8_t *data, size_t len,
                           const uint8_t *pub, const uint8_t *sig);
    error_t (*derive_key)(const uint8_t *shared_secret,
                               const uint8_t *salt,
                               uint8_t *key);
    error_t (*aes_encrypt)(const uint8_t *key, const uint8_t *iv,
                                const uint8_t *plain, size_t len,
                                uint8_t *cipher);
    error_t (*aes_decrypt)(const uint8_t *key, const uint8_t *iv,
                                const uint8_t *cipher, size_t len,
                                uint8_t *plain);
    error_t (*compute_mac)(const uint8_t *key,
                                const uint8_t *data, size_t len,
                                uint8_t *mac);
    error_t (*store_key)(uint32_t slot, const uint8_t *key, size_t len);
    error_t (*load_key)(uint32_t slot, uint8_t *key, size_t *len);
} iccoa_se_interface_t;

/******************************************************************************
 * ICCOA API 函数声明
 ******************************************************************************/

/**
 * @brief 初始化 ICCOA 协议栈
 */
error_t iccoa_init(const iccoa_se_interface_t *se_if);

/**
 * @brief 反初始化 ICCOA 协议栈
 */
error_t iccoa_deinit(void);

/**
 * @brief 构建 TLV 元素
 */
error_t iccoa_build_tlv(
    iccoa_tlv_t *tlv,
    iccoa_tlv_type_t type,
    uint16_t length,
    const uint8_t *value
);

/**
 * @brief 解析 TLV 数据
 */
error_t iccoa_parse_tlvs(
    const uint8_t *data,
    size_t len,
    iccoa_tlv_t *tlvs,
    uint8_t *tlv_count
);

/**
 * @brief 构建消息
 */
error_t iccoa_build_message(
    iccoa_message_t *msg,
    iccoa_command_t cmd,
    const iccoa_tlv_t *tlvs,
    uint8_t tlv_count,
    uint16_t sequence
);

/**
 * @brief 序列化消息
 */
error_t iccoa_serialize_message(
    const iccoa_message_t *msg,
    uint8_t *buffer,
    size_t *len
);

/**
 * @brief 反序列化消息
 */
error_t iccoa_deserialize_message(
    const uint8_t *data,
    size_t len,
    iccoa_message_t *msg
);

/**
 * @brief 创建配对会话
 */
error_t iccoa_pairing_start(
    const iccoa_pairing_config_t *config,
    iccoa_session_context_t **session
);

/**
 * @brief 交换密钥
 */
error_t iccoa_pairing_exchange_key(
    iccoa_session_context_t *session,
    const uint8_t *vehicle_pub_key,
    uint8_t *device_pub_key
);

/**
 * @brief 配对认证
 */
error_t iccoa_pairing_authenticate(
    iccoa_session_context_t *session,
    const uint8_t *challenge,
    uint8_t *response
);

/**
 * @brief 确认配对
 */
error_t iccoa_pairing_confirm(
    iccoa_session_context_t *session,
    const uint8_t *confirmation
);

/**
 * @brief 完成配对
 */
error_t iccoa_pairing_complete(
    iccoa_session_context_t *session,
    key_info_t *key_info
);

/**
 * @brief 取消配对
 */
error_t iccoa_pairing_cancel(iccoa_session_context_t *session);

/**
 * @brief 创建会话
 */
error_t iccoa_session_create(
    iccoa_session_context_t *session,
    const uint8_t *key_id
);

/**
 * @brief 会话认证
 */
error_t iccoa_session_authenticate(
    iccoa_session_context_t *session,
    const uint8_t *token,
    const uint8_t *mac
);

/**
 * @brief 更新会话密钥
 */
error_t iccoa_session_renew(
    iccoa_session_context_t *session,
    uint8_t *new_token
);

/**
 * @brief 关闭会话
 */
error_t iccoa_session_close(iccoa_session_context_t *session);

/**
 * @brief 加密消息
 */
error_t iccoa_encrypt_message(
    iccoa_session_context_t *session,
    iccoa_message_t *msg
);

/**
 * @brief 解密消息
 */
error_t iccoa_decrypt_message(
    iccoa_session_context_t *session,
    iccoa_message_t *msg
);

/**
 * @brief 计算消息 MAC
 */
error_t iccoa_compute_message_mac(
    iccoa_session_context_t *session,
    const iccoa_message_t *msg,
    uint8_t *mac
);

/**
 * @brief 验证消息 MAC
 */
error_t iccoa_verify_message_mac(
    iccoa_session_context_t *session,
    const iccoa_message_t *msg,
    const uint8_t *mac
);

/**
 * @brief 发送命令
 */
error_t iccoa_send_command(
    iccoa_session_context_t *session,
    iccoa_command_t cmd,
    const iccoa_tlv_t *tlvs,
    uint8_t tlv_count,
    iccoa_message_t *response
);

/**
 * @brief 车辆控制
 */
error_t iccoa_vehicle_control(
    iccoa_session_context_t *session,
    iccoa_command_t cmd,
    const uint8_t *params,
    size_t params_len,
    uint8_t *result
);

/**
 * @brief 获取车辆状态
 */
error_t iccoa_get_vehicle_status(
    iccoa_session_context_t *session,
    iccoa_vehicle_status_t *status
);

/**
 * @brief 注册新钥匙
 */
error_t iccoa_key_register(
    iccoa_session_context_t *session,
    const key_info_t *key_info
);

/**
 * @brief 激活钥匙
 */
error_t iccoa_key_activate(
    iccoa_session_context_t *session, const uint8_t *key_id);

/**
 * @brief 挂起钥匙
 */
error_t iccoa_key_suspend(iccoa_session_context_t *session, const uint8_t *key_id);

/**
 * @brief 恢复钥匙
 */
error_t iccoa_key_resume(iccoa_session_context_t *session, const uint8_t *key_id);

/**
 * @brief 删除钥匙
 */
error_t iccoa_key_delete(iccoa_session_context_t *session, const uint8_t *key_id);

/**
 * @brief 获取钥匙列表
 */
error_t iccoa_key_list(
    iccoa_session_context_t *session,
    key_info_t *keys,
    size_t *key_count
);

/**
 * @brief 分享钥匙
 */
error_t iccoa_key_share(
    iccoa_session_context_t *session,
    const uint8_t *key_id,
    key_role_t target_role,
    uint16_t permissions,
    uint32_t validity,
    uint8_t *share_data,
    size_t *share_len
);

/**
 * @brief OTA 检查更新
 */
error_t iccoa_ota_check(
    iccoa_session_context_t *session,
    uint32_t current_version,
    iccoa_ota_info_t *ota_info
);

/**
 * @brief OTA 下载进度
 */
error_t iccoa_ota_download(
    iccoa_session_context_t *session,
    uint32_t offset,
    uint8_t *data,
    size_t *len
);

/**
 * @brief OTA 安装
 */
error_t iccoa_ota_install(
    iccoa_session_context_t *session,
    const uint8_t *firmware,
    size_t len
);

/**
 * @brief 解析错误码
 */
const char* iccoa_error_to_string(iccoa_error_code_t err);

/**
 * @brief 释放消息资源
 */
void iccoa_free_message(iccoa_message_t *msg);

/**
 * @brief 释放会话资源
 */
void iccoa_session_destroy(iccoa_session_context_t *session);

#ifdef __cplusplus
}
#endif

#endif /* ICCOA_H */
