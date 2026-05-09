/******************************************************************************
 * @file    dkcs.h
 * @brief   Digital Key Connectivity Stack - 主头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 ******************************************************************************/

#ifndef DKCS_H
#define DKCS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/******************************************************************************
 * 版本信息
 ******************************************************************************/
#define DKCS_VERSION_MAJOR     1
#define DKCS_VERSION_MINOR     0
#define DKCS_VERSION_PATCH     0
#define DKCS_VERSION_STRING    "1.0.0"

/******************************************************************************
 * 错误码定义
 ******************************************************************************/
typedef enum {
    OK = 0,
    ERROR_INVALID_PARAM = -1,
    ERROR_NO_MEMORY = -2,
    ERROR_NOT_SUPPORTED = -3,
    ERROR_NOT_INITIALIZED = -4,
    ERROR_ALREADY_INITIALIZED = -5,
    ERROR_CRYPTO_FAILURE = -6,
    ERROR_TRANSPORT_FAILURE = -7,
    ERROR_TIMEOUT = -8,
    ERROR_AUTH_FAILURE = -9,
    ERROR_PROTOCOL_FAILURE = -10,
    ERROR_SE_FAILURE = -11,
    ERROR_BUFFER_TOO_SMALL = -12,
} error_t;

/******************************************************************************
 * 协议类型定义
 ******************************************************************************/
typedef enum {
    PROTOCOL_CCC = 0,      /**< CCC Digital Key R3 */
    PROTOCOL_ICCE = 1,     /**< ICCE 智慧车联 */
    PROTOCOL_ICCOA = 2,    /**< ICCOA 开放联盟 */
    PROTOCOL_MAX
} protocol_type_t;

/******************************************************************************
 * 连接方式定义
 ******************************************************************************/
typedef enum {
    CONN_BLE = 0,          /**< Bluetooth Low Energy */
    CONN_NFC = 1,          /**< Near Field Communication */
    CONN_UWB = 2,          /**< Ultra-Wideband (精确测距) */
    CONN_HYBRID = 3,       /**< 混合模式 */
    CONN_MAX
} connection_type_t;

/******************************************************************************
 * 安全元件类型
 ******************************************************************************/
typedef enum {
    SE_NONE = 0,           /**< 无安全元件 */
    SE_ESE = 1,            /**< Embedded SE (eSE) */
    SE_SIM = 2,            /**< SIM-based SE */
    SE_TEE = 3,            /**< Trusted Execution Environment */
    SE_SOFTWARE = 4,       /**< 软实现 (仅测试) */
} se_type_t;

/******************************************************************************
 * 数字钥匙角色
 ******************************************************************************/
typedef enum {
    KEY_ROLE_OWNER = 0,    /**< 车主 */
    KEY_ROLE_ADMIN = 1,    /**< 管理员 */
    KEY_ROLE_GUEST = 2,    /**< 客人 */
    KEY_ROLE_TEMPORARY = 3,/**< 临时钥匙 */
    KEY_ROLE_MAX
} key_role_t;

/******************************************************************************
 * 数字钥匙状态
 ******************************************************************************/
typedef enum {
    KEY_STATUS_INACTIVE = 0,
    KEY_STATUS_ACTIVE = 1,
    KEY_STATUS_SUSPENDED = 2,
    KEY_STATUS_REVOKED = 3,
    KEY_STATUS_EXPIRED = 4,
} key_status_t;

/******************************************************************************
 * 权限定义 (位字段)
 ******************************************************************************/
#define PERM_UNLOCK            (1U << 0)   /**< 解锁 */
#define PERM_LOCK              (1U << 1)   /**< 上锁 */
#define PERM_ENGINE_START      (1U << 2)   /**< 启动发动机 */
#define PERM_TRUNK             (1U << 3)   /**< 后备箱 */
#define PERM_WINDOWS           (1U << 4)   /**< 车窗控制 */
#define PERM_CLIMATE           (1U << 5)   /**< 空调控制 */
#define PERM_CHARGE_PORT       (1U << 6)   /**< 充电口 */
#define PERM_SHARE             (1U << 7)   /**< 分享权限 */
#define PERM_REMOTE_CONTROL    (1U << 8)   /**< 远程控制 */
#define PERM_Valet_MODE        (1U << 9)   /**< 代客泊车模式 */
#define PERM_GEOFENCE          (1U << 10)  /**< 地理围栏 */
#define PERM_SPEED_LIMIT       (1U << 11)  /**< 速度限制 */

/******************************************************************************
 * 固定长度定义
 ******************************************************************************/
#define KEY_ID_LEN             16
#define VIN_LEN                17
#define DEVICE_ID_LEN          16
#define KEY_FINGERPRINT_LEN    16
#define ECC_P256_KEY_LEN       32
#define ECC_P256_PUB_KEY_LEN   65
#define AES_KEY_LEN            16
#define AES_IV_LEN             12
#define AES_TAG_LEN            16
#define MAX_CERT_LEN           1024
#define MAX_APDU_LEN           512
#define MAX_FRAGMENT_SIZE      247

/******************************************************************************
 * 数字钥匙信息结构
 ******************************************************************************/
typedef struct {
    uint8_t     key_id[KEY_ID_LEN];
    uint8_t     vin[VIN_LEN];
    uint8_t     device_id[DEVICE_ID_LEN];
    uint8_t     fingerprint[KEY_FINGERPRINT_LEN];
    key_role_t role;
    uint16_t    permissions;
    uint32_t    valid_from;
    uint32_t    valid_until;
    key_status_t status;
    protocol_type_t protocol;
    connection_type_t preferred_conn;
} key_info_t;

/******************************************************************************
 * 会话配置结构
 ******************************************************************************/
typedef struct {
    protocol_type_t protocol;
    connection_type_t conn_type;
    se_type_t se_type;
    uint32_t timeout_ms;
    bool enable_mitm_protection;
    bool enable_replay_protection;
    bool enable_ranging;            /**< UWB 测距 */
} session_config_t;

/******************************************************************************
 * 事件回调函数类型
 ******************************************************************************/
typedef void (*dkcs_event_callback_t)(
    uint32_t event_id,
    const void *event_data,
    size_t data_len,
    void *user_ctx
);

typedef void (*dkcs_auth_complete_callback_t)(
    error_t result,
    const key_info_t *key_info,
    void *user_ctx
);

/******************************************************************************
 * 主接口函数声明
 ******************************************************************************/

/**
 * @brief 初始化 DKCS 库
 */
error_t dkcs_init(const session_config_t *config);

/**
 * @brief 反初始化 DKCS 库
 */
error_t dkcs_deinit(void);

/**
 * @brief 获取版本信息
 */
const char* dkcs_get_version(void);

/**
 * @brief 注册事件回调
 */
error_t dkcs_register_callback(
    dkcs_event_callback_t callback,
    void *user_ctx
);

/**
 * @brief 启动配对流程
 */
error_t dkcs_pairing_start(
    protocol_type_t protocol,
    const uint8_t *vin,
    dkcs_auth_complete_callback_t callback,
    void *user_ctx
);

/**
 * @brief 解锁车辆
 */
error_t dkcs_vehicle_unlock(
    const uint8_t *key_id,
    const uint8_t *vin
);

/**
 * @brief 上锁车辆
 */
error_t dkcs_vehicle_lock(
    const uint8_t *key_id,
    const uint8_t *vin
);

/**
 * @brief 启动发动机
 */
error_t dkcs_engine_start(
    const uint8_t *key_id,
    const uint8_t *vin
);

/**
 * @brief 分享钥匙
 */
error_t dkcs_key_share(
    const uint8_t *key_id,
    key_role_t target_role,
    uint16_t permissions,
    uint32_t valid_duration_sec
);

/**
 * @brief 吊销钥匙
 */
error_t dkcs_key_revoke(const uint8_t *key_id);

/**
 * @brief 获取钥匙列表
 */
error_t dkcs_key_list(
    key_info_t *keys,
    size_t *key_count
);

/**
 * @brief 导入钥匙
 */
error_t dkcs_key_import(
    const uint8_t *key_data,
    size_t key_len,
    key_info_t *key_info
);

/**
 * @brief 导出钥匙
 */
error_t dkcs_key_export(
    const uint8_t *key_id,
    uint8_t *key_data,
    size_t *key_len
);

/******************************************************************************
 * 协议特定接口 (可选)
 ******************************************************************************/
#ifdef ENABLE_CCC
#include "ccc.h"
#endif

#ifdef ENABLE_ICCE
#include "icce.h"
#endif

#ifdef ENABLE_ICCOA
#include "iccoa.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* DKCS_H */
