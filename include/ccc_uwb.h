/******************************************************************************
 * @file    ccc_uwb.h
 * @brief   CCC Digital Key R3 UWB 超宽带测距功能头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 *
 * @note    实现 CCC Digital Key Release 3 规范 2.0.0r2
 *          - UWB 会话管理和测距操作
 *          - STS (Scrambled Timestamp Sequence) 安全测距
 *          - 支持 CCC 和 IEEE 802.15.4z 测距协议
 *          - 中继攻击防护
 ******************************************************************************/

#ifndef CCC_UWB_H
#define CCC_UWB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dkcs.h"
#include "ccc.h"

/******************************************************************************
 * CCC UWB 错误码
 ******************************************************************************/
typedef enum {
    CCC_UWB_OK = 0,
    CCC_UWB_ERROR_INIT_FAILED = -200,
    CCC_UWB_ERROR_ALREADY_INIT = -201,
    CCC_UWB_ERROR_NOT_INITIALIZED = -202,
    CCC_UWB_ERROR_SESSION_ACTIVE = -203,
    CCC_UWB_ERROR_NO_SESSION = -204,
    CCC_UWB_ERROR_RANGING_FAILED = -205,
    CCC_UWB_ERROR_INVALID_CONFIG = -206,
    CCC_UWB_ERROR_UNSUPPORTED_PROTOCOL = -207,
    CCC_UWB_ERROR_STS_KEY_FAILED = -208,
    CCC_UWB_ERROR_TIMEOUT = -209,
    CCC_UWB_ERROR_SECURITY_VIOLATION = -210,
    CCC_UWB_ERROR_HARDWARE_FAILURE = -211,
    CCC_UWB_ERROR_INVALID_MEASUREMENT = -212,
    CCC_UWB_ERROR_AOA_NOT_SUPPORTED = -213,
    CCC_UWB_ERROR_RELAY_ATTACK_DETECTED = -214,
} ccc_uwb_error_t;

/******************************************************************************
 * UWB 测距协议定义
 ******************************************************************************/
typedef enum {
    CCC_UWB_PROTOCOL_CCC_R3 = 0,        /**< CCC R3 专有协议 */
    CCC_UWB_PROTOCOL_IEEE_802_15_4Z = 1, /**< IEEE 802.15.4z HRP UWB */
    CCC_UWB_PROTOCOL_MAX
} ccc_uwb_protocol_t;

/******************************************************************************
 * UWB 测距角色定义
 ******************************************************************************/
typedef enum {
    CCC_UWB_ROLE_INITIATOR = 0,         /**< 测距发起者 (通常 mobile device) */
    CCC_UWB_ROLE_RESPONDER = 1,         /**< 测距响应者 (通常 vehicle) */
    CCC_UWB_ROLE_MAX
} ccc_uwb_role_t;

/******************************************************************************
 * UWB 会话状态
 ******************************************************************************/
typedef enum {
    CCC_UWB_SESSION_STATE_IDLE = 0,
    CCC_UWB_SESSION_STATE_INITIALIZING = 1,
    CCC_UWB_SESSION_STATE_READY = 2,
    CCC_UWB_SESSION_STATE_RANGING = 3,
    CCC_UWB_SESSION_STATE_STOPPING = 4,
    CCC_UWB_SESSION_STATE_ERROR = 5,
} ccc_uwb_session_state_t;

/******************************************************************************
 * UWB 测距状态
 ******************************************************************************/
typedef enum {
    CCC_UWB_RANGING_STATE_IDLE = 0,
    CCC_UWB_RANGING_STATE_ONGOING = 1,
    CCC_UWB_RANGING_STATE_COMPLETED = 2,
    CCC_UWB_RANGING_STATE_FAILED = 3,
    CCC_UWB_RANGING_STATE_ABORTED = 4,
} ccc_uwb_ranging_state_t;

/******************************************************************************
 * UWB 能力标志 (UWB Caps)
 ******************************************************************************/
#define CCC_UWB_CAP_SECURE_RANGING      (1U << 0)   /**< 支持安全测距 */
#define CCC_UWB_CAP_AOA_AZIMUTH         (1U << 1)   /**< 支持方位角测量 */
#define CCC_UWB_CAP_AOA_ELEVATION       (1U << 2)   /**< 支持仰角测量 */
#define CCC_UWB_CAP_MULTI_NODE          (1U << 3)   /**< 支持多节点测距 */
#define CCC_UWB_CAP_RANGING_RATE_CTRL   (1U << 4)   /**< 支持测距速率控制 */
#define CCC_UWB_CAP_DIAGNOSTICS         (1U << 5)   /**< 支持诊断数据 */

/******************************************************************************
 * UWB 配置 ID 定义 (CCC R3 规范)
 ******************************************************************************/
typedef enum {
    CCC_UWB_CONFIG_ID_1 = 0x01,         /**< 配置 1: 基础测距 */
    CCC_UWB_CONFIG_ID_2 = 0x02,         /**< 配置 2: 高速测距 */
    CCC_UWB_CONFIG_ID_3 = 0x03,         /**< 配置 3: 高精度测距 */
    CCC_UWB_CONFIG_ID_4 = 0x04,         /**< 配置 4: 长距离测距 */
    CCC_UWB_CONFIG_ID_5 = 0x05,         /**< 配置 5: 安全测距 (默认) */
    CCC_UWB_CONFIG_ID_6 = 0x06,         /**< 配置 6: 安全 + AoA */
} ccc_uwb_config_id_t;

/******************************************************************************
 * UWB 固定长度定义
 ******************************************************************************/
#define CCC_UWB_SESSION_ID_LEN          4       /**< 会话 ID 长度 */
#define CCC_UWB_STS_KEY_LEN             16      /**< STS 密钥长度 (128-bit) */
#define CCC_UWB_STS_IV_LEN              8       /**< STS IV 长度 (64-bit) */
#define CCC_UWB_PUB_KEY_LEN             65      /**< P-256 公钥长度 */
#define CCC_UWB_PRIVATE_KEY_LEN         32      /**< P-256 私钥长度 */
#define CCC_UWB_RANGING_NONCE_LEN       8       /**< 测距随机数长度 */
#define CCC_UWB_TIMESTAMP_LEN           6       /**< UWB 时间戳长度 (40-bit) */
#define CCC_UWB_MAX_ANCHORS             8       /**< 最大锚点数 */
#define CCC_UWB_MIN_RANGING_INTERVAL_MS 20      /**< 最小测距间隔 (ms) */
#define CCC_UWB_DEFAULT_RANGING_INTERVAL_MS 100 /**< 默认测距间隔 (ms) */

/******************************************************************************
 * STS (Scrambled Timestamp Sequence) 配置
 * 符合 CCC R3 2.0.0r2 规范 Section 12.3
 ******************************************************************************/
typedef struct {
    uint8_t  sts_key[CCC_UWB_STS_KEY_LEN];      /**< STS 加密密钥 */
    uint8_t  sts_iv[CCC_UWB_STS_IV_LEN];        /**< STS 初始向量 */
    uint8_t  sts_index;                          /**< STS 索引 */
    bool     dynamic_sts_enabled;               /**< 动态 STS 启用 */
    uint32_t dynamic_sts_interval_ms;           /**< 动态 STS 更新间隔 */
} ccc_uwb_sts_config_t;

/******************************************************************************
 * UWB 测距配置结构
 ******************************************************************************/
typedef struct {
    /* 基础配置 */
    uint8_t                 uwb_session_id[CCC_UWB_SESSION_ID_LEN]; /**< 会话 ID */
    ccc_uwb_protocol_t      ranging_protocol;                       /**< 测距协议 */
    ccc_uwb_config_id_t     uwb_config_id;                          /**< 配置标识 */
    uint32_t                uwb_caps;                               /**< UWB 能力 */
    ccc_uwb_role_t          role;                                   /**< 测距角色 */

    /* 测距参数 */
    uint32_t                ranging_interval_ms;                    /**< 测距间隔 (ms) */
    uint32_t                session_timeout_ms;                     /**< 会话超时 (ms) */
    uint16_t                max_ranging_rounds;                     /**< 最大测距轮次 */
    uint8_t                 uwb_channel;                            /**< UWB 信道 (5, 6, 8, 9) */
    uint8_t                 preamble_code;                          /**< 前导码索引 */

    /* STS 安全测距配置 */
    ccc_uwb_sts_config_t    sts_config;                             /**< STS 配置 */
    bool                    secure_ranging_enabled;                 /**< 启用安全测距 */

    /* 会话密钥材料 (用于 STS 密钥派生) */
    uint8_t                 session_key[CCC_SESSION_KEY_LEN];       /**< 会话加密密钥 */
    uint8_t                 session_mac_key[CCC_MAC_KEY_LEN];       /**< 会话 MAC 密钥 */
} ccc_uwb_config_t;

/******************************************************************************
 * UWB 测距结果结构
 ******************************************************************************/
typedef struct {
    uint64_t    timestamp_ms;           /**< 测量时间戳 (ms) */
    float       distance_m;             /**< 距离 (米) */
    float       accuracy_m;             /**< 精度估计 (米) */
    int8_t      rssi_dbm;               /**< 信号强度 (dBm) */
    uint8_t     quality;                /**< 测量质量 (0-100) */
    bool        is_valid;               /**< 测量是否有效 */
} ccc_uwb_distance_measurement_t;

/******************************************************************************
 * UWB AoA (Angle of Arrival) 测量结果
 ******************************************************************************/
typedef struct {
    bool        azimuth_available;      /**< 方位角可用 */
    float       azimuth_deg;            /**< 方位角 (度), -180 ~ +180 */
    float       azimuth_accuracy_deg;   /**< 方位角精度 (度) */
    uint8_t     azimuth_quality;        /**< 方位角质量 (0-100) */

    bool        elevation_available;    /**< 仰角可用 */
    float       elevation_deg;          /**< 仰角 (度), -90 ~ +90 */
    float       elevation_accuracy_deg; /**< 仰角精度 (度) */
    uint8_t     elevation_quality;      /**< 仰角质量 (0-100) */
} ccc_uwb_aoa_measurement_t;

/******************************************************************************
 * UWB 多节点锚点信息
 ******************************************************************************/
typedef struct {
    uint8_t     anchor_id;                              /**< 锚点 ID */
    uint8_t     anchor_address[2];                      /**< 锚点地址 */
    ccc_uwb_distance_measurement_t distance;            /**< 距离测量 */
    ccc_uwb_aoa_measurement_t aoa;                      /**< AoA 测量 */
} ccc_uwb_anchor_measurement_t;

/******************************************************************************
 * UWB 测距会话上下文
 ******************************************************************************/
typedef struct {
    ccc_uwb_session_state_t     state;                  /**< 会话状态 */
    ccc_uwb_ranging_state_t     ranging_state;          /**< 测距状态 */
    ccc_uwb_config_t            config;                 /**< 测距配置 */

    /* 会话密钥 */
    uint8_t                     ranging_key[CCC_SESSION_KEY_LEN];   /**< 测距密钥 */
    uint8_t                     sts_key[CCC_UWB_STS_KEY_LEN];       /**< 当前 STS 密钥 */
    uint8_t                     sts_counter;                         /**< STS 计数器 */

    /* 当前测量结果 */
    ccc_uwb_distance_measurement_t last_distance;       /**< 最新距离测量 */
    ccc_uwb_aoa_measurement_t      last_aoa;            /**< 最新 AoA 测量 */

    /* 多节点测距 (车辆通常有多个锚点) */
    ccc_uwb_anchor_measurement_t   anchors[CCC_UWB_MAX_ANCHORS];
    uint8_t                        anchor_count;        /**< 活跃锚点数 */

    /* 安全统计 */
    uint32_t                       ranging_counter;     /**< 测距次数统计 */
    uint32_t                       security_violations; /**< 安全违规次数 */
    uint32_t                       relay_attack_counter; /**< 中继攻击检测次数 */

    /* 时间戳 */
    uint64_t                       session_start_time;  /**< 会话开始时间 */
    uint64_t                       last_ranging_time;   /**< 上次测距时间 */
} ccc_uwb_session_context_t;

/******************************************************************************
 * UWB 回调函数类型
 ******************************************************************************/

/**
 * @brief 测距结果回调
 * @param distance 距离测量结果
 * @param aoa AoA 测量结果 (可能为 NULL)
 * @param user_ctx 用户上下文
 */
typedef void (*ccc_uwb_ranging_callback_t)(
    const ccc_uwb_distance_measurement_t *distance,
    const ccc_uwb_aoa_measurement_t *aoa,
    void *user_ctx
);

/**
 * @brief 测距事件回调
 * @param event_id 事件 ID (0=开始, 1=停止, 2=错误)
 * @param event_data 事件数据
 * @param data_len 数据长度
 * @param user_ctx 用户上下文
 */
typedef void (*ccc_uwb_event_callback_t)(
    uint32_t event_id,
    const void *event_data,
    size_t data_len,
    void *user_ctx
);

/**
 * @brief 安全违规回调
 * @param violation_type 违规类型
 * @param timestamp_ms 违规时间戳
 * @param user_ctx 用户上下文
 */
typedef void (*ccc_uwb_security_callback_t)(
    uint32_t violation_type,
    uint64_t timestamp_ms,
    void *user_ctx
);

/******************************************************************************
 * UWB API 函数声明 - 会话管理
 ******************************************************************************/

/**
 * @brief 初始化 UWB 测距会话
 *
 * 根据 CCC R3 2.0.0r2 规范 Section 12.2 初始化 UWB 测距会话。
 * 会话 ID 必须唯一标识一个测距会话。
 *
 * @param config UWB 测距配置
 * @param session 输出会话上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_init_ranging(
    const ccc_uwb_config_t *config,
    ccc_uwb_session_context_t **session
);

/**
 * @brief 开始 UWB 测距
 *
 * 启动 UWB 测距会话。根据配置，可以是在 initiator 或 responder 模式。
 * 安全测距模式下会自动处理 STS 密钥更新。
 *
 * @param session 会话上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_start_ranging(ccc_uwb_session_context_t *session);

/**
 * @brief 停止 UWB 测距
 *
 * 停止当前测距会话，释放相关资源但保留会话上下文。
 *
 * @param session 会话上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_stop_ranging(ccc_uwb_session_context_t *session);

/**
 * @brief 终止 UWB 测距会话
 *
 * 完全终止测距会话并销毁上下文。
 *
 * @param session 会话上下文
 */
void ccc_uwb_terminate_session(ccc_uwb_session_context_t *session);

/******************************************************************************
 * UWB API 函数声明 - 配置操作
 ******************************************************************************/

/**
 * @brief 从 CCC 会话派生 UWB 配置
 *
 * 根据 CCC Digital Key R3 规范 Section 12.3，
 * 从已建立的 CCC 安全会话派生 UWB 测距所需的 STS 密钥。
 *
 * @param ccc_session CCC 会话上下文
 * @param uwb_config 输出 UWB 配置
 * @return error_t 错误码
 */
error_t ccc_uwb_derive_config_from_session(
    const ccc_session_context_t *ccc_session,
    ccc_uwb_config_t *uwb_config
);

/**
 * @brief 更新 STS 密钥
 *
 * 动态更新 STS 密钥，用于安全测距。
 * 符合 CCC R3 2.0.0r2 动态 STS 要求。
 *
 * @param session 会话上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_update_sts_key(ccc_uwb_session_context_t *session);

/**
 * @brief 设置测距间隔
 *
 * @param session 会话上下文
 * @param interval_ms 新的测距间隔 (ms)，必须 >= 20ms
 * @return error_t 错误码
 */
error_t ccc_uwb_set_ranging_interval(
    ccc_uwb_session_context_t *session,
    uint32_t interval_ms
);

/******************************************************************************
 * UWB API 函数声明 - 测距数据获取
 ******************************************************************************/

/**
 * @brief 获取最新距离测量
 *
 * 获取最近一次测距的距离结果。
 *
 * @param session 会话上下文
 * @param distance 输出距离测量结果
 * @return error_t 错误码
 */
error_t ccc_uwb_get_distance(
    ccc_uwb_session_context_t *session,
    ccc_uwb_distance_measurement_t *distance
);

/**
 * @brief 获取方位角 (Azimuth)
 *
 * 如果设备支持 AoA，获取方位角测量。
 * 方位角范围: -180° ~ +180° (0° 为正前方)
 *
 * @param session 会话上下文
 * @param azimuth_deg 输出方位角 (度)
 * @param accuracy_deg 输出精度估计 (度)
 * @return error_t 错误码
 */
error_t ccc_uwb_get_azimuth(
    ccc_uwb_session_context_t *session,
    float *azimuth_deg,
    float *accuracy_deg
);

/**
 * @brief 获取仰角 (Elevation)
 *
 * 如果设备支持 AoA，获取仰角测量。
 * 仰角范围: -90° ~ +90° (0° 为水平)
 *
 * @param session 会话上下文
 * @param elevation_deg 输出仰角 (度)
 * @param accuracy_deg 输出精度估计 (度)
 * @return error_t 错误码
 */
error_t ccc_uwb_get_elevation(
    ccc_uwb_session_context_t *session,
    float *elevation_deg,
    float *accuracy_deg
);

/**
 * @brief 获取完整 AoA 测量
 *
 * 获取方位角和仰角 (如果都可用)。
 *
 * @param session 会话上下文
 * @param aoa 输出 AoA 测量结果
 * @return error_t 错误码
 */
error_t ccc_uwb_get_aoa(
    ccc_uwb_session_context_t *session,
    ccc_uwb_aoa_measurement_t *aoa
);

/******************************************************************************
 * UWB API 函数声明 - 多节点测距
 ******************************************************************************/

/**
 * @brief 获取所有锚点测量
 *
 * 车辆通常有多个 UWB 锚点，获取所有锚点的测量数据。
 *
 * @param session 会话上下文
 * @param anchors 输出锚点测量数组
 * @param anchor_count 输入输出锚点数量
 * @return error_t 错误码
 */
error_t ccc_uwb_get_all_anchor_measurements(
    ccc_uwb_session_context_t *session,
    ccc_uwb_anchor_measurement_t *anchors,
    uint8_t *anchor_count
);

/******************************************************************************
 * UWB API 函数声明 - 安全功能
 ******************************************************************************/

/**
 * @brief 验证测距结果安全性
 *
 * 检查测距结果是否通过安全验证，检测潜在的中继攻击。
 *
 * @param session 会话上下文
 * @param measurement 距离测量结果
 * @return true 如果安全验证通过
 */
bool ccc_uwb_verify_ranging_security(
    ccc_uwb_session_context_t *session,
    const ccc_uwb_distance_measurement_t *measurement
);

/**
 * @brief 检测中继攻击
 *
 * 基于测距时间分析和统计方法检测中继攻击。
 *
 * @param session 会话上下文
 * @param timestamp_ms 当前时间戳
 * @return true 如果检测到潜在中继攻击
 */
bool ccc_uwb_detect_relay_attack(
    ccc_uwb_session_context_t *session,
    uint64_t timestamp_ms
);

/**
 * @brief 获取安全统计信息
 *
 * @param session 会话上下文
 * @param violation_count 输出安全违规次数
 * @param relay_count 输出中继攻击检测次数
 * @return error_t 错误码
 */
error_t ccc_uwb_get_security_stats(
    ccc_uwb_session_context_t *session,
    uint32_t *violation_count,
    uint32_t *relay_count
);

/******************************************************************************
 * UWB API 函数声明 - 回调注册
 ******************************************************************************/

/**
 * @brief 注册测距结果回调
 *
 * @param session 会话上下文
 * @param callback 回调函数
 * @param user_ctx 用户上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_register_ranging_callback(
    ccc_uwb_session_context_t *session,
    ccc_uwb_ranging_callback_t callback,
    void *user_ctx
);

/**
 * @brief 注册事件回调
 *
 * @param session 会话上下文
 * @param callback 回调函数
 * @param user_ctx 用户上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_register_event_callback(
    ccc_uwb_session_context_t *session,
    ccc_uwb_event_callback_t callback,
    void *user_ctx
);

/**
 * @brief 注册安全违规回调
 *
 * @param session 会话上下文
 * @param callback 回调函数
 * @param user_ctx 用户上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_register_security_callback(
    ccc_uwb_session_context_t *session,
    ccc_uwb_security_callback_t callback,
    void *user_ctx
);

/******************************************************************************
 * UWB API 函数声明 - 诊断和统计
 ******************************************************************************/

/**
 * @brief 获取测距统计信息
 *
 * @param session 会话上下文
 * @param total_measurements 输出总测量次数
 * @param valid_measurements 输出有效测量次数
 * @param average_quality 输出平均质量
 * @return error_t 错误码
 */
error_t ccc_uwb_get_ranging_stats(
    ccc_uwb_session_context_t *session,
    uint32_t *total_measurements,
    uint32_t *valid_measurements,
    uint8_t *average_quality
);

/**
 * @brief 清除测距统计
 *
 * @param session 会话上下文
 * @return error_t 错误码
 */
error_t ccc_uwb_clear_ranging_stats(ccc_uwb_session_context_t *session);

/******************************************************************************
 * UWB API 函数声明 - 辅助函数
 ******************************************************************************/

/**
 * @brief 生成随机会话 ID
 *
 * @param session_id 输出会话 ID (4字节)
 * @return error_t 错误码
 */
error_t ccc_uwb_generate_session_id(uint8_t session_id[CCC_UWB_SESSION_ID_LEN]);

/**
 * @brief 派生 STS 密钥
 *
 * 使用 HKDF 从会话密钥派生 STS 密钥。
 *
 * @param master_key 主密钥
 * @param key_len 密钥长度
 * @param sts_index STS 索引
 * @param sts_key 输出 STS 密钥
 * @return error_t 错误码
 */
error_t ccc_uwb_derive_sts_key(
    const uint8_t *master_key,
    size_t key_len,
    uint8_t sts_index,
    uint8_t sts_key[CCC_UWB_STS_KEY_LEN]
);

/**
 * @brief 验证 UWB 配置有效性
 *
 * @param config UWB 配置
 * @return true 如果配置有效
 */
bool ccc_uwb_validate_config(const ccc_uwb_config_t *config);

/**
 * @brief 获取 UWB 错误码字符串
 *
 * @param error 错误码
 * @return const char* 错误描述字符串
 */
const char* ccc_uwb_error_string(ccc_uwb_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* CCC_UWB_H */
