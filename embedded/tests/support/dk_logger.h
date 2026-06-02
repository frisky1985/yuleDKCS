/**
 * dk_logger.h — Test-compatible logger header
 *
 * This OVERRIDES the real dk_logger.h when placed earlier in the include path.
 * The real header's dk_logger_log() declaration uses va_list but the DK_LOG()
 * macro passes raw arguments. This version fixes that mismatch for tests.
 */

#ifndef DK_LOGGER_H
#define DK_LOGGER_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels */
#define DK_LOG_LEVEL_TRACE  0
#define DK_LOG_LEVEL_DEBUG  1
#define DK_LOG_LEVEL_INFO   2
#define DK_LOG_LEVEL_WARN   3
#define DK_LOG_LEVEL_ERROR  4
#define DK_LOG_LEVEL_FATAL  5

typedef uint8_t dk_log_level_t;

/* Tags */
#define DK_LOG_TAG_INIT      "INIT"
#define DK_LOG_TAG_KEYMGR   "KEYMGR"
#define DK_LOG_TAG_AUTH     "AUTH"
#define DK_LOG_TAG_BLE      "BLE"
#define DK_LOG_TAG_NFC      "NFC"
#define DK_LOG_TAG_UWB      "UWB"
#define DK_LOG_TAG_SEC      "SEC"
#define DK_LOG_TAG_VEHICLE  "VEH"
#define DK_LOG_TAG_SERVICE  "SVC"
#define DK_LOG_TAG_UNIFIED  "UNI"
#define DK_LOG_TAG_EDGE     "EDGE"

/* Log output function — uses ... to match DK_LOG macro expansion */
void dk_logger_log(dk_log_level_t level, const char* tag,
                   const char* file, int line,
                   const char* format, ...);

/* Core log macro */
#define DK_LOG(level, tag, format, ...) \
    dk_logger_log(level, tag, __FILE__, __LINE__, format, ##__VA_ARGS__)

/* Convenience macros */
#define DK_LOG_TRACE(tag, format, ...)  DK_LOG(DK_LOG_LEVEL_TRACE, tag, format, ##__VA_ARGS__)
#define DK_LOG_DEBUG(tag, format, ...)  DK_LOG(DK_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define DK_LOG_INFO(tag, format, ...)   DK_LOG(DK_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define DK_LOG_WARN(tag, format, ...)   DK_LOG(DK_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#define DK_LOG_ERROR(tag, format, ...)  DK_LOG(DK_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define DK_LOG_FATAL(tag, format, ...)  DK_LOG(DK_LOG_LEVEL_FATAL, tag, format, ##__VA_ARGS__)

/* Thread-aware variants (thread ID macro used in real code) */
#define DK_LOG_THREAD_INFO(tag, format, ...)  DK_LOG_INFO(tag, format, ##__VA_ARGS__)
#define DK_LOG_THREAD_ERROR(tag, format, ...) DK_LOG_ERROR(tag, format, ##__VA_ARGS__)

/* Module-specific shortcuts */
#define DK_LOG_KEYMGR_INFO(format, ...)  DK_LOG_INFO(DK_LOG_TAG_KEYMGR, format, ##__VA_ARGS__)
#define DK_LOG_KEYMGR_ERROR(format, ...) DK_LOG_ERROR(DK_LOG_TAG_KEYMGR, format, ##__VA_ARGS__)
#define DK_LOG_AUTH_INFO(format, ...)    DK_LOG_INFO(DK_LOG_TAG_AUTH, format, ##__VA_ARGS__)
#define DK_LOG_AUTH_ERROR(format, ...)   DK_LOG_ERROR(DK_LOG_TAG_AUTH, format, ##__VA_ARGS__)
#define DK_LOG_BLE_INFO(format, ...)     DK_LOG_INFO(DK_LOG_TAG_BLE, format, ##__VA_ARGS__)
#define DK_LOG_BLE_ERROR(format, ...)    DK_LOG_ERROR(DK_LOG_TAG_BLE, format, ##__VA_ARGS__)
#define DK_LOG_NFC_INFO(format, ...)     DK_LOG_INFO(DK_LOG_TAG_NFC, format, ##__VA_ARGS__)
#define DK_LOG_NFC_ERROR(format, ...)    DK_LOG_ERROR(DK_LOG_TAG_NFC, format, ##__VA_ARGS__)
#define DK_LOG_UWB_INFO(format, ...)     DK_LOG_INFO(DK_LOG_TAG_UWB, format, ##__VA_ARGS__)
#define DK_LOG_UWB_ERROR(format, ...)    DK_LOG_ERROR(DK_LOG_TAG_UWB, format, ##__VA_ARGS__)
#define DK_LOG_SEC_INFO(format, ...)     DK_LOG_INFO(DK_LOG_TAG_SEC, format, ##__VA_ARGS__)
#define DK_LOG_SEC_ERROR(format, ...)    DK_LOG_ERROR(DK_LOG_TAG_SEC, format, ##__VA_ARGS__)
#define DK_LOG_SEC_WARN(format, ...)     DK_LOG_WARN(DK_LOG_TAG_SEC, format, ##__VA_ARGS__)

/* RFC 5424 syslog-compatible aliases */
#define DK_LOG_EMERG  DK_LOG_FATAL
#define DK_LOG_ALERT  DK_LOG_ERROR
#define DK_LOG_CRIT   DK_LOG_ERROR
#define DK_LOG_NOTICE DK_LOG_INFO

#ifdef __cplusplus
}
#endif

#endif /* DK_LOGGER_H */
