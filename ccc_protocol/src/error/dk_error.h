/**
 * @file dk_error.h
 * @brief 统一错误码定义 - Vehicle TCU端
 * @version 2.0.0
 * @date 2026-05-06
 * 
 * 三端统一错误码: Cloud / SDK / Vehicle
 */

#ifndef DK_ERROR_H
#define DK_ERROR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// 错误码类别
//=============================================================================

/**
 * @brief 错误码类别
 */
typedef enum {
    DK_CATEGORY_SUCCESS   = 0x00,  /**< 成功 */
    DK_CATEGORY_REQUEST   = 0x01,  /**< 请求参数错误 */
    DK_CATEGORY_AUTH      = 0x02,  /**< 认证鉴权错误 */
    DK_CATEGORY_KEY       = 0x03,  /**< 密钥管理错误 */
    DK_CATEGORY_VEHICLE   = 0x04,  /**< 车辆控制错误 */
    DK_CATEGORY_SHARE     = 0x05,  /**< 分享业务错误 */
    DK_CATEGORY_DEVICE    = 0x06,  /**< 设备硬件错误 */
    DK_CATEGORY_VENDOR   = 0x07,  /**< 厂商适配错误 */
    DK_CATEGORY_TRANSPORT = 0x08,  /**< 通信链路错误 */
    DK_CATEGORY_SYSTEM    = 0x09,  /**< 系统内部错误 */
    DK_CATEGORY_TCU       = 0xAC, /**< TCU专用错误 */
} dk_error_category_t;

//=============================================================================
// 成功类错误码 (0x0000-0x00FF)
//=============================================================================

#define DK_SUCCESS           ((uint16_t)0x0000)  /**< 成功 */
#define DK_SUCCESS_ASYNC     ((uint16_t)0x0001)  /**< 异步成功，已接收 */
#define DK_SUCCESS_PARTIAL   ((uint16_t)0x0002)  /**< 部分成功 */

//=============================================================================
// 请求参数错误类 (0x0100-0x01FF)
//=============================================================================

#define DK_ERR_INVALID_REQUEST    ((uint16_t)0x0101)  /**< 无效请求 */
#define DK_ERR_INVALID_PARAMETER  ((uint16_t)0x0102)  /**< 参数错误 */
#define DK_ERR_MISSING_PARAMETER  ((uint16_t)0x0103)  /**< 缺少参数 */
#define DK_ERR_INVALID_FORMAT     ((uint16_t)0x0104)  /**< 格式错误 */
#define DK_ERR_INVALID_LENGTH     ((uint16_t)0x0105)  /**< 长度错误 */
#define DK_ERR_INVALID_SIGNATURE  ((uint16_t)0x0106)  /**< 签名无效 */
#define DK_ERR_RATE_LIMIT         ((uint16_t)0x0107)  /**< 请求过于频繁 */
#define DK_ERR_QUOTA_EXCEEDED     ((uint16_t)0x0108)  /**< 配额超限 */
#define DK_ERR_VERSION_MISMATCH   ((uint16_t)0x0109)  /**< 版本不匹配 */

//=============================================================================
// 认证鉴权错误类 (0x0200-0x02FF)
//=============================================================================

#define DK_ERR_UNAUTHORIZED      ((uint16_t)0x0201)  /**< 未授权 */
#define DK_ERR_INVALID_TOKEN     ((uint16_t)0x0202)  /**< Token无效 */
#define DK_ERR_TOKEN_EXPIRED     ((uint16_t)0x0203)  /**< Token过期 */
#define DK_ERR_FORBIDDEN         ((uint16_t)0x0204)  /**< 禁止访问 */
#define DK_ERR_ACCESS_DENIED     ((uint16_t)0x0205)  /**< 权限不足 */
#define DK_ERR_USER_DISABLED     ((uint16_t)0x0206)  /**< 用户已禁用 */
#define DK_ERR_ACCOUNT_LOCKED    ((uint16_t)0x0207)  /**< 账户已锁定 */
#define DK_ERR_SESSION_EXPIRED   ((uint16_t)0x0208)  /**< 会话已过期 */

//=============================================================================
// 密钥管理错误类 (0x0300-0x03FF)
//=============================================================================

#define DK_ERR_KEY_NOT_FOUND             ((uint16_t)0x0301)  /**< 密钥不存在 */
#define DK_ERR_KEY_EXISTS                ((uint16_t)0x0302)  /**< 密钥已存在 */
#define DK_ERR_KEY_EXPIRED               ((uint16_t)0x0303)  /**< 密钥已过期 */
#define DK_ERR_KEY_REVOKED               ((uint16_t)0x0304)  /**< 密钥已撤销 */
#define DK_ERR_KEY_SUSPENDED             ((uint16_t)0x0305)  /**< 密钥已挂起 */
#define DK_ERR_KEY_PENDING               ((uint16_t)0x0306)  /**< 密钥待激活 */
#define DK_ERR_KEY_MAX_REACHED           ((uint16_t)0x0307)  /**< 密钥数量达上限 */
#define DK_ERR_KEY_MAX_USES              ((uint16_t)0x0308)  /**< 使用次数已用完 */
#define DK_ERR_DISTANCE_EXCEEDED         ((uint16_t)0x0309)  /**< 距离超出限制 */
#define DK_ERR_KEY_TYPE_NOT_ALLOWED      ((uint16_t)0x030A)  /**< 密钥类型不允许 */
#define DK_ERR_KEY_NOT_ACTIVE            ((uint16_t)0x030B)  /**< 密钥未激活 */
#define DK_ERR_KEY_VALIDITY_NOT_STARTED  ((uint16_t)0x030C)  /**< 密钥未到生效时间 */
#define DK_ERR_KEY_BIND_FAILED           ((uint16_t)0x030D)  /**< 密钥绑定失败 */
#define DK_ERR_KEY_UNBIND_FAILED         ((uint16_t)0x030E)  /**< 密钥解绑失败 */

//=============================================================================
// 车辆控制错误类 (0x0400-0x04FF)
//=============================================================================

#define DK_ERR_VEHICLE_NOT_FOUND       ((uint16_t)0x0401)  /**< 车辆不存在 */
#define DK_ERR_VEHICLE_OFFLINE         ((uint16_t)0x0402)  /**< 车辆离线 */
#define DK_ERR_VEHICLE_NOT_BOUND       ((uint16_t)0x0403)  /**< 车辆未绑定 */
#define DK_ERR_TCU_OFFLINE             ((uint16_t)0x0404)  /**< TCU离线 */
#define DK_ERR_TCU_TIMEOUT             ((uint16_t)0x0405)  /**< TCU超时 */
#define DK_ERR_TCU_ERROR               ((uint16_t)0x0406)  /**< TCU内部错误 */
#define DK_ERR_COMMAND_FAILED          ((uint16_t)0x0407)  /**< 指令执行失败 */
#define DK_ERR_COMMAND_TIMEOUT         ((uint16_t)0x0408)  /**< 指令执行超时 */
#define DK_ERR_COMMAND_REJECTED        ((uint16_t)0x0409)  /**< 指令被拒绝 */
#define DK_ERR_COMMAND_INVALID         ((uint16_t)0x040A)  /**< 指令无效 */
#define DK_ERR_COMMAND_IN_PROGRESS     ((uint16_t)0x040B)  /**< 指令执行中 */
#define DK_ERR_VEHICLE_NOT_SUPPORTED   ((uint16_t)0x040C)  /**< 车辆不支持该操作 */
#define DK_ERR_BATTERY_LOW             ((uint16_t)0x040D)  /**< 电瓶电量低 */
#define DK_ERR_ENGINE_RUNNING          ((uint16_t)0x040E)  /**< 引擎运行中 */
#define DK_ERR_VEHICLE_MOVING          ((uint16_t)0x040F)  /**< 车辆正在移动 */
#define DK_ERR_DOOR_OPEN               ((uint16_t)0x0410)  /**< 车门未关闭 */

//=============================================================================
// 分享业务错误类 (0x0500-0x05FF)
//=============================================================================

#define DK_ERR_SHARE_NOT_FOUND     ((uint16_t)0x0501)  /**< 分享不存在 */
#define DK_ERR_SHARE_EXPIRED       ((uint16_t)0x0502)  /**< 分享已过期 */
#define DK_ERR_SHARE_ACCEPTED      ((uint16_t)0x0503)  /**< 分享已被接受 */
#define DK_ERR_SHARE_CANCELLED     ((uint16_t)0x0504)  /**< 分享已取消 */
#define DK_ERR_SHARE_MAX_USES      ((uint16_t)0x0505)  /**< 分享次数已用完 */
#define DK_ERR_SHARE_NOT_ALLOWED   ((uint16_t)0x0506)  /**< 不允许分享 */
#define DK_ERR_CODE_INVALID        ((uint16_t)0x0507)  /**< 分享码无效 */
#define DK_ERR_CODE_EXPIRED       ((uint16_t)0x0508)  /**< 分享码过期 */
#define DK_ERR_VENDOR_NOT_MATCH    ((uint16_t)0x0509)  /**< 厂商不匹配 */
#define DK_ERR_SHARE_TO_SELF       ((uint16_t)0x050A)  /**< 不能分享给自己 */

//=============================================================================
// 设备硬件错误类 (0x0600-0x06FF)
//=============================================================================

#define DK_ERR_DEVICE_NOT_FOUND      ((uint16_t)0x0601)  /**< 设备不存在 */
#define DK_ERR_DEVICE_NOT_BOUND      ((uint16_t)0x0602)  /**< 设备未绑定 */
#define DK_ERR_DEVICE_DISABLED       ((uint16_t)0x0603)  /**< 设备已禁用 */
#define DK_ERR_DEVICE_NOT_SUPPORTED  ((uint16_t)0x0604)  /**< 设备不支持 */
#define DK_ERR_BLE_DISABLED          ((uint16_t)0x0605)  /**< 蓝牙未开启 */
#define DK_ERR_NFC_DISABLED          ((uint16_t)0x0606)  /**< NFC未开启 */
#define DK_ERR_UWB_DISABLED          ((uint16_t)0x0607)  /**< UWB未开启 */
#define DK_ERR_SE_NOT_AVAILABLE      ((uint16_t)0x0608)  /**< 安全元件不可用 */
#define DK_ERR_SE_ERROR              ((uint16_t)0x0609)  /**< SE通信错误 */
#define DK_ERR_STORAGE_FULL          ((uint16_t)0x060A)  /**< 存储满 */
#define DK_ERR_BLE_SCAN_FAILED       ((uint16_t)0x060B)  /**< BLE扫描失败 */
#define DK_ERR_BLE_CONNECT_FAILED    ((uint16_t)0x060C)  /**< BLE连接失败 */
#define DK_ERR_NFC_READ_FAILED       ((uint16_t)0x060D)  /**< NFC读取失败 */
#define DK_ERR_NFC_WRITE_FAILED      ((uint16_t)0x060E)  /**< NFC写入失败 */
#define DK_ERR_UWB_RANGING_FAILED    ((uint16_t)0x060F)  /**< UWB测距失败 */

//=============================================================================
// 厂商适配错误类 (0x0700-0x07FF)
//=============================================================================

#define DK_ERR_VENDOR_NOT_SUPPORTED ((uint16_t)0x0701)  /**< 厂商不支持 */
#define DK_ERR_ADAPTER_NOT_FOUND    ((uint16_t)0x0702)  /**< 适配器未找到 */
#define DK_ERR_VENDOR_API_ERROR     ((uint16_t)0x0703)  /**< 厂商API错误 */
#define DK_ERR_VENDOR_API_TIMEOUT   ((uint16_t)0x0704)  /**< 厂商API超时 */
#define DK_ERR_VENDOR_AUTH_FAILED   ((uint16_t)0x0705)  /**< 厂商认证失败 */
#define DK_ERR_VENDOR_BIND_FAILED   ((uint16_t)0x0706)  /**< 厂商绑定失败 */
#define DK_ERR_PROTOCOL_ERROR       ((uint16_t)0x0707)  /**< 协议错误 */

//=============================================================================
// 通信链路错误类 (0x0800-0x08FF)
//=============================================================================

#define DK_ERR_NETWORK_ERROR         ((uint16_t)0x0801)  /**< 网络错误 */
#define DK_ERR_NETWORK_TIMEOUT       ((uint16_t)0x0802)  /**< 网络超时 */
#define DK_ERR_SERVER_UNREACHABLE    ((uint16_t)0x0803)  /**< 服务器不可达 */
#define DK_ERR_CONNECTION_REFUSED    ((uint16_t)0x0804)  /**< 连接被拒绝 */
#define DK_ERR_MQTT_DISCONNECTED     ((uint16_t)0x0805)  /**< MQTT断开 */
#define DK_ERR_MQTT_PUBLISH_FAILED   ((uint16_t)0x0806)  /**< MQTT发布失败 */
#define DK_ERR_GRPC_UNAVAILABLE      ((uint16_t)0x0807)  /**< gRPC不可用 */
#define DK_ERR_GRPC_DEADLINE         ((uint16_t)0x0808)  /**< gRPC超时 */

//=============================================================================
// 系统内部错误类 (0x0900-0x09FF)
//=============================================================================

#define DK_ERR_INTERNAL_ERROR         ((uint16_t)0x0901)  /**< 内部错误 */
#define DK_ERR_SERVICE_UNAVAILABLE   ((uint16_t)0x0902)  /**< 服务不可用 */
#define DK_ERR_DATABASE_ERROR        ((uint16_t)0x0903)  /**< 数据库错误 */
#define DK_ERR_CACHE_ERROR           ((uint16_t)0x0904)  /**< 缓存错误 */
#define DK_ERR_QUEUE_ERROR           ((uint16_t)0x0905)  /**< 队列错误 */
#define DK_ERR_CONFIG_ERROR          ((uint16_t)0x0906)  /**< 配置错误 */
#define DK_ERR_CRYPTO_ERROR          ((uint16_t)0x0907)  /**< 加密错误 */
#define DK_ERR_SIGN_ERROR            ((uint16_t)0x0908)  /**< 签名错误 */
#define DK_ERR_VERIFY_ERROR          ((uint16_t)0x0909)  /**< 验签错误 */
#define DK_ERR_FEATURE_NOT_SUPPORTED ((uint16_t)0x090A)  /**< 功能不支持 */
#define DK_ERR_MAINTENANCE           ((uint16_t)0x090B)  /**< 系统维护 */
#define DK_ERR_CAPACITY_EXCEEDED     ((uint16_t)0x090C)  /**< 容量超限 */

//=============================================================================
// TCU车端专用错误类 (0xAC00-0xACFF)
//=============================================================================

#define DK_ERR_TCU_BLE_ERROR        ((uint16_t)0xAC01)  /**< BLE通信错误 */
#define DK_ERR_TCU_NFC_ERROR        ((uint16_t)0xAC02)  /**< NFC通信错误 */
#define DK_ERR_TCU_UWB_ERROR        ((uint16_t)0xAC03)  /**< UWB通信错误 */
#define DK_ERR_TCU_SE_ERROR         ((uint16_t)0xAC04)  /**< SE通信错误 */
#define DK_ERR_TCU_PAIRING_FAILED   ((uint16_t)0xAC05)  /**< 配对失败 */
#define DK_ERR_TCU_AUTH_FAILED      ((uint16_t)0xAC06)  /**< 认证失败 */
#define DK_ERR_TCU_CRYPTO_FAILED   ((uint16_t)0xAC07)  /**< 加密失败 */
#define DK_ERR_TCU_STORAGE_ERROR    ((uint16_t)0xAC08)  /**< 存储错误 */
#define DK_ERR_TCU_POWER_LOW        ((uint16_t)0xAC09)  /**< 电量低 */
#define DK_ERR_TCU_WATCHDOG_RESET   ((uint16_t)0xAC0A)  /**< 看门狗复位 */
#define DK_ERR_TCU_ANTENNA_FAULT   ((uint16_t)0xAC0B)  /**< 天线故障 */
#define DK_ERR_TCU_MSG_QUEUE_FULL   ((uint16_t)0xAC0C)  /**< 消息队列满 */

//=============================================================================
// 错误结构体和函数
//=============================================================================

/**
 * @brief 统一错误结构
 */
typedef struct {
    uint16_t code;           /**< 错误码 */
    char message[128];       /**< 错误信息 */
    char trace_id[32];       /**< 追踪ID */
    char details[256];      /**< 错误详情 */
} dk_error_t;

/**
 * @brief 获取错误码类别
 * @param code 错误码
 * @return 错误类别
 */
static inline dk_error_category_t dk_error_get_category(uint16_t code) {
    return (dk_error_category_t)(code >> 8);
}

/**
 * @brief 判断是否成功
 * @param code 错误码
 * @return true成功，false失败
 */
static inline bool dk_is_success(uint16_t code) {
    return code == DK_SUCCESS;
}

/**
 * @brief 获取错误码名称
 * @param code 错误码
 * @return 错误码名称字符串
 */
const char* dk_error_name(uint16_t code);

/**
 * @brief 获取错误信息
 * @param code 错误码
 * @return 错误信息字符串
 */
const char* dk_error_message(uint16_t code);

/**
 * @brief 创建错误
 * @param code 错误码
 * @return 错误结构
 */
dk_error_t dk_error_create(uint16_t code);

/**
 * @brief 创建带详情的错误
 * @param code 错误码
 * @param details 详情格式字符串
 * @return 错误结构
 */
dk_error_t dk_error_create_with_details(uint16_t code, const char* details, ...);

/**
 * @brief 创建带追踪ID的错误
 * @param code 错误码
 * @param trace_id 追踪ID
 * @return 错误结构
 */
dk_error_t dk_error_create_with_trace(uint16_t code, const char* trace_id);

/**
 * @brief 格式化错误码为16进制字符串
 * @param code 错误码
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 */
void dk_error_format_hex(uint16_t code, char* buffer, size_t size);

/**
 * @brief 打印错误信息
 * @param err 错误结构
 */
void dk_error_print(const dk_error_t* err);

/**
 * @brief 获取类别的名称
 * @param category 类别
 * @return 类别名称
 */
const char* dk_error_category_name(dk_error_category_t category);

#ifdef __cplusplus
}
#endif

#endif /* DK_ERROR_H */
