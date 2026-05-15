/**
 * @file dk_logger.h
 * @brief 统一日志接口 - Vehicle TCU端
 * @version 2.0.0
 * @date 2026-05-06
 * 
 * 三端统一日志规范
 */

#ifndef DK_LOGGER_H
#define DK_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// 日志级别
//=============================================================================

#define DK_LOG_LEVEL_TRACE  0   /**< 跟踪详情 */
#define DK_LOG_LEVEL_DEBUG  1   /**< 调试信息 */
#define DK_LOG_LEVEL_INFO   2   /**< 一般信息 */
#define DK_LOG_LEVEL_WARN   3   /**< 警告信息 */
#define DK_LOG_LEVEL_ERROR  4   /**< 错误信息 */
#define DK_LOG_LEVEL_FATAL  5   /**< 致命错误 */

typedef uint8_t dk_log_level_t;

//=============================================================================
// 日志模块标签
//=============================================================================

#define DK_LOG_TAG_INIT      "INIT"      /**< 初始化 */
#define DK_LOG_TAG_KEYMGR   "KEYMGR"    /**< 密钥管理 */
#define DK_LOG_TAG_AUTH     "AUTH"      /**< 认证模块 */
#define DK_LOG_TAG_BLE      "BLE"       /**< BLE通信 */
#define DK_LOG_TAG_NFC      "NFC"       /**< NFC通信 */
#define DK_LOG_TAG_UWB      "UWB"       /**< UWB通信 */
#define DK_LOG_TAG_SEC      "SEC"       /**< 安全模块 */
#define DK_LOG_TAG_TRANSPORT "TRANSPORT" /**< 传输层 */
#define DK_LOG_TAG_PROTO    "PROTO"     /**< 协议解析 */
#define DK_LOG_TAG_SERVICE  "SERVICE"   /**< 业务服务 */
#define DK_LOG_TAG_MQTT     "MQTT"      /**< MQTT通信 */
#define DK_LOG_TAG_DB       "DB"        /**< 数据库 */
#define DK_LOG_TAG_CACHE    "CACHE"     /**< 缓存 */

//=============================================================================
// 日志输出配置
//=============================================================================

/**
 * @brief 日志输出目标
 */
typedef enum {
    DK_LOG_TARGET_UART    = 0x01,  /**< UART串口 */
    DK_LOG_TARGET_CAN     = 0x02,  /**< CAN总线 */
    DK_LOG_TARGET_FLASH   = 0x04,  /**< Flash存储 */
    DK_LOG_TARGET_RTT     = 0x08,  /**< J-Link RTT */
} dk_log_target_t;

/**
 * @brief 日志配置
 */
typedef struct {
    dk_log_level_t level;          /**< 日志级别 */
    uint8_t targets;               /**< 输出目标 */
    bool enable_timestamp;         /**< 是否输出时间戳 */
    bool enable_trace_id;         /**< 是否输出追踪ID */
    bool enable_colors;            /**< 是否输出颜色 */
    uint32_t flash_buffer_size;    /**< Flash缓冲区大小 */
} dk_logger_config_t;

//=============================================================================
// 日志函数声明
//=============================================================================

/**
 * @brief 初始化日志模块
 * @param config 日志配置
 */
void dk_logger_init(const dk_logger_config_t* config);

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void dk_logger_set_level(dk_log_level_t level);

/**
 * @brief 设置追踪ID
 * @param trace_id 追踪ID
 */
void dk_logger_set_trace_id(const char* trace_id);

/**
 * @brief 获取追踪ID
 * @return 追踪ID
 */
const char* dk_logger_get_trace_id(void);

/**
 * @brief 核心日志输出函数
 * @param level 日志级别
 * @param tag 模块标签
 * @param file 文件名
 * @param line 行号
 * @param format 格式化字符串
 * @param args 可变参数
 */
void dk_logger_log(dk_log_level_t level, const char* tag, 
                   const char* file, int line,
                   const char* format, va_list args);

/**
 * @brief 日志输出宏
 * @param level 日志级别
 * @param tag 模块标签
 * @param format 格式化字符串
 * @param ... 参数
 */
#define DK_LOG(level, tag, format, ...) \
    dk_logger_log(level, tag, __FILE__, __LINE__, format, ##__VA_ARGS__)

//=============================================================================
// 便捷日志宏
//=============================================================================

/**
 * @brief 跟踪日志
 */
#define DK_LOG_TRACE(tag, format, ...) \
    DK_LOG(DK_LOG_LEVEL_TRACE, tag, format, ##__VA_ARGS__)

/**
 * @brief 调试日志
 */
#define DK_LOG_DEBUG(tag, format, ...) \
    DK_LOG(DK_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)

/**
 * @brief 信息日志
 */
#define DK_LOG_INFO(tag, format, ...) \
    DK_LOG(DK_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)

/**
 * @brief 警告日志
 */
#define DK_LOG_WARN(tag, format, ...) \
    DK_LOG(DK_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)

/**
 * @brief 错误日志
 */
#define DK_LOG_ERROR(tag, format, ...) \
    DK_LOG(DK_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)

/**
 * @brief 致命错误日志
 */
#define DK_LOG_FATAL(tag, format, ...) \
    DK_LOG(DK_LOG_LEVEL_FATAL, tag, format, ##__VA_ARGS__)

//=============================================================================
// 带追踪ID的日志宏
//=============================================================================

/**
 * @brief 带追踪ID的信息日志
 */
#define DK_LOGI(tag, format, ...) do { \
    const char* _tid = dk_logger_get_trace_id(); \
    if (_tid) { \
        DK_LOG_INFO(tag, "[T:%s] " format, _tid, ##__VA_ARGS__); \
    } else { \
        DK_LOG_INFO(tag, format, ##__VA_ARGS__); \
    } \
} while(0)

/**
 * @brief 带追踪ID的错误日志
 */
#define DK_LOGE(tag, format, ...) do { \
    const char* _tid = dk_logger_get_trace_id(); \
    if (_tid) { \
        DK_LOG_ERROR(tag, "[T:%s] " format, _tid, ##__VA_ARGS__); \
    } else { \
        DK_LOG_ERROR(tag, format, ##__VA_ARGS__); \
    } \
} while(0)

//=============================================================================
// 专用模块日志函数
//=============================================================================

/* 密钥管理日志 */
#define DK_LOG_KEYMGR_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_KEYMGR, format, ##__VA_ARGS__)
#define DK_LOG_KEYMGR_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_KEYMGR, format, ##__VA_ARGS__)
#define DK_LOG_KEYMGR_DEBUG(format, ...) DK_LOG_DEBUG(DK_LOG_TAG_KEYMGR, format, ##__VA_ARGS__)

/* 认证日志 */
#define DK_LOG_AUTH_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_AUTH, format, ##__VA_ARGS__)
#define DK_LOG_AUTH_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_AUTH, format, ##__VA_ARGS__)

/* BLE日志 */
#define DK_LOG_BLE_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_BLE, format, ##__VA_ARGS__)
#define DK_LOG_BLE_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_BLE, format, ##__VA_ARGS__)
#define DK_LOG_BLE_DEBUG(format, ...) DK_LOG_DEBUG(DK_LOG_TAG_BLE, format, ##__VA_ARGS__)

/* NFC日志 */
#define DK_LOG_NFC_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_NFC, format, ##__VA_ARGS__)
#define DK_LOG_NFC_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_NFC, format, ##__VA_ARGS__)
#define DK_LOG_NFC_DEBUG(format, ...) DK_LOG_DEBUG(DK_LOG_TAG_NFC, format, ##__VA_ARGS__)

/* UWB日志 */
#define DK_LOG_UWB_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_UWB, format, ##__VA_ARGS__)
#define DK_LOG_UWB_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_UWB, format, ##__VA_ARGS__)
#define DK_LOG_UWB_DEBUG(format, ...) DK_LOG_DEBUG(DK_LOG_TAG_UWB, format, ##__VA_ARGS__)

/* 安全模块日志 */
#define DK_LOG_SEC_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_SEC, format, ##__VA_ARGS__)
#define DK_LOG_SEC_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_SEC, format, ##__VA_ARGS__)
#define DK_LOG_SEC_WARN(format, ...)  DK_LOG_WARN(DK_LOG_TAG_SEC, format, ##__VA_ARGS__)

/* 协议日志 */
#define DK_LOG_PROTO_DEBUG(format, ...) DK_LOG_DEBUG(DK_LOG_TAG_PROTO, format, ##__VA_ARGS__)
#define DK_LOG_PROTO_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_PROTO, format, ##__VA_ARGS__)

/* MQTT日志 */
#define DK_LOG_MQTT_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_MQTT, format, ##__VA_ARGS__)
#define DK_LOG_MQTT_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_MQTT, format, ##__VA_ARGS__)

//=============================================================================
// 错误码日志宏
//=============================================================================

/**
 * @brief 使用错误码记录错误日志
 * @param tag 模块标签
 * @param err_code 错误码
 * @param msg 额外信息
 */
#define DK_LOG_ERR_CODE(tag, err_code, msg) do { \
    DK_LOG_ERROR(tag, "Error 0x%04X: %s - %s", \
                 (err_code), dk_error_name(err_code), (msg)); \
} while(0)

/**
 * @brief 记录函数入口
 */
#define DK_LOG_ENTRY() DK_LOG_DEBUG(DK_LOG_TAG_INIT, "Enter %s", __FUNCTION__)

/**
 * @brief 记录函数退出
 */
#define DK_LOG_EXIT() DK_LOG_DEBUG(DK_LOG_TAG_INIT, "Exit %s", __FUNCTION__)

/**
 * @brief 记录函数退出并返回
 * @param ret 返回值
 */
#define DK_LOG_EXIT_RET(ret) DK_LOG_DEBUG(DK_LOG_TAG_INIT, "Exit %s with %d", __FUNCTION__, (ret))

//=============================================================================
// 性能日志宏
//=============================================================================

/**
 * @brief 开始计时
 * @param name 计时器名称
 */
#define DK_TIMER_START(name) uint32_t _timer_##name = dk_timer_get_ms()

/**
 * @brief 结束计时并记录
 * @param name 计时器名称
 * @param tag 模块标签
 */
#define DK_TIMER_END(name, tag) do { \
    uint32_t _elapsed = dk_timer_get_ms() - _timer_##name; \
    DK_LOG_INFO(tag, "Timer %s: %ums", #name, _elapsed); \
} while(0)

/**
 * @brief 获取当前毫秒数
 * @return 毫秒数
 */
uint32_t dk_timer_get_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* DK_LOGGER_H */
