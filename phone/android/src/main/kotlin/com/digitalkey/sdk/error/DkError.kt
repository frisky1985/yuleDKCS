// DkError.kt - 统一错误码 - Android SDK端
// 三端统一错误码: Cloud / SDK / Vehicle

package com.digitalkey.sdk.error

/**
 * 错误码类别
 */
enum class ErrorCategory(val value: UByte) {
    SUCCESS(0x00u),    // 成功
    REQUEST(0x01u),    // 请求参数错误
    AUTH(0x02u),       // 认证鉴权错误
    KEY(0x03u),        // 密钥管理错误
    VEHICLE(0x04u),    // 车辆控制错误
    SHARE(0x05u),      // 分享业务错误
    DEVICE(0x06u),    // 设备硬件错误
    VENDOR(0x07u),   // 厂商适配错误
    TRANSPORT(0x08u), // 通信链路错误
    SYSTEM(0x09u),    // 系统内部错误
    TCU(0xACu);       // TCU专用错误

    companion object {
        fun fromCode(code: Int): ErrorCategory {
            return entries.find { it.value == (code shr 8).toUByte() } ?: SYSTEM
        }
    }
}

/**
 * 统一错误码
 */
object DkErrorCode {
    // 成功类 (0x0000-0x00FF)
    const val SUCCESS: Int = 0x0000
    const val SUCCESS_ASYNC: Int = 0x0001
    const val SUCCESS_PARTIAL: Int = 0x0002

    // 请求参数错误类 (0x0100-0x01FF)
    const val ERR_INVALID_REQUEST: Int = 0x0101
    const val ERR_INVALID_PARAMETER: Int = 0x0102
    const val ERR_MISSING_PARAMETER: Int = 0x0103
    const val ERR_INVALID_FORMAT: Int = 0x0104
    const val ERR_INVALID_LENGTH: Int = 0x0105
    const val ERR_INVALID_SIGNATURE: Int = 0x0106
    const val ERR_RATE_LIMIT: Int = 0x0107
    const val ERR_QUOTA_EXCEEDED: Int = 0x0108
    const val ERR_VERSION_MISMATCH: Int = 0x0109

    // 认证鉴权错误类 (0x0200-0x02FF)
    const val ERR_UNAUTHORIZED: Int = 0x0201
    const val ERR_INVALID_TOKEN: Int = 0x0202
    const val ERR_TOKEN_EXPIRED: Int = 0x0203
    const val ERR_FORBIDDEN: Int = 0x0204
    const val ERR_ACCESS_DENIED: Int = 0x0205
    const val ERR_USER_DISABLED: Int = 0x0206
    const val ERR_ACCOUNT_LOCKED: Int = 0x0207
    const val ERR_SESSION_EXPIRED: Int = 0x0208

    // 密钥管理错误类 (0x0300-0x03FF)
    const val ERR_KEY_NOT_FOUND: Int = 0x0301
    const val ERR_KEY_EXISTS: Int = 0x0302
    const val ERR_KEY_EXPIRED: Int = 0x0303
    const val ERR_KEY_REVOKED: Int = 0x0304
    const val ERR_KEY_SUSPENDED: Int = 0x0305
    const val ERR_KEY_PENDING: Int = 0x0306
    const val ERR_KEY_MAX_REACHED: Int = 0x0307
    const val ERR_KEY_MAX_USES: Int = 0x0308
    const val ERR_DISTANCE_EXCEEDED: Int = 0x0309
    const val ERR_KEY_TYPE_NOT_ALLOWED: Int = 0x030A
    const val ERR_KEY_NOT_ACTIVE: Int = 0x030B
    const val ERR_KEY_VALIDITY_NOT_STARTED: Int = 0x030C
    const val ERR_KEY_BIND_FAILED: Int = 0x030D
    const val ERR_KEY_UNBIND_FAILED: Int = 0x030E

    // 车辆控制错误类 (0x0400-0x04FF)
    const val ERR_VEHICLE_NOT_FOUND: Int = 0x0401
    const val ERR_VEHICLE_OFFLINE: Int = 0x0402
    const val ERR_VEHICLE_NOT_BOUND: Int = 0x0403
    const val ERR_TCU_OFFLINE: Int = 0x0404
    const val ERR_TCU_TIMEOUT: Int = 0x0405
    const val ERR_TCU_ERROR: Int = 0x0406
    const val ERR_COMMAND_FAILED: Int = 0x0407
    const val ERR_COMMAND_TIMEOUT: Int = 0x0408
    const val ERR_COMMAND_REJECTED: Int = 0x0409
    const val ERR_COMMAND_INVALID: Int = 0x040A
    const val ERR_COMMAND_IN_PROGRESS: Int = 0x040B
    const val ERR_VEHICLE_NOT_SUPPORTED: Int = 0x040C
    const val ERR_BATTERY_LOW: Int = 0x040D
    const val ERR_ENGINE_RUNNING: Int = 0x040E
    const val ERR_VEHICLE_MOVING: Int = 0x040F
    const val ERR_DOOR_OPEN: Int = 0x0410

    // 分享业务错误类 (0x0500-0x05FF)
    const val ERR_SHARE_NOT_FOUND: Int = 0x0501
    const val ERR_SHARE_EXPIRED: Int = 0x0502
    const val ERR_SHARE_ACCEPTED: Int = 0x0503
    const val ERR_SHARE_CANCELLED: Int = 0x0504
    const val ERR_SHARE_MAX_USES: Int = 0x0505
    const val ERR_SHARE_NOT_ALLOWED: Int = 0x0506
    const val ERR_CODE_INVALID: Int = 0x0507
    const val ERR_CODE_EXPIRED: Int = 0x0508
    const val ERR_VENDOR_NOT_MATCH: Int = 0x0509
    const val ERR_SHARE_TO_SELF: Int = 0x050A

    // 设备硬件错误类 (0x0600-0x06FF)
    const val ERR_DEVICE_NOT_FOUND: Int = 0x0601
    const val ERR_DEVICE_NOT_BOUND: Int = 0x0602
    const val ERR_DEVICE_DISABLED: Int = 0x0603
    const val ERR_DEVICE_NOT_SUPPORTED: Int = 0x0604
    const val ERR_BLE_DISABLED: Int = 0x0605
    const val ERR_NFC_DISABLED: Int = 0x0606
    const val ERR_UWB_DISABLED: Int = 0x0607
    const val ERR_SE_NOT_AVAILABLE: Int = 0x0608
    const val ERR_SE_ERROR: Int = 0x0609
    const val ERR_STORAGE_FULL: Int = 0x060A
    const val ERR_BLE_SCAN_FAILED: Int = 0x060B
    const val ERR_BLE_CONNECT_FAILED: Int = 0x060C
    const val ERR_NFC_READ_FAILED: Int = 0x060D
    const val ERR_NFC_WRITE_FAILED: Int = 0x060E
    const val ERR_UWB_RANGING_FAILED: Int = 0x060F

    // 厂商适配错误类 (0x0700-0x07FF)
    const val ERR_VENDOR_NOT_SUPPORTED: Int = 0x0701
    const val ERR_ADAPTER_NOT_FOUND: Int = 0x0702
    const val ERR_VENDOR_API_ERROR: Int = 0x0703
    const val ERR_VENDOR_API_TIMEOUT: Int = 0x0704
    const val ERR_VENDOR_AUTH_FAILED: Int = 0x0705
    const val ERR_VENDOR_BIND_FAILED: Int = 0x0706
    const val ERR_PROTOCOL_ERROR: Int = 0x0707

    // 通信链路错误类 (0x0800-0x08FF)
    const val ERR_NETWORK_ERROR: Int = 0x0801
    const val ERR_NETWORK_TIMEOUT: Int = 0x0802
    const val ERR_SERVER_UNREACHABLE: Int = 0x0803
    const val ERR_CONNECTION_REFUSED: Int = 0x0804
    const val ERR_MQTT_DISCONNECTED: Int = 0x0805
    const val ERR_MQTT_PUBLISH_FAILED: Int = 0x0806
    const val ERR_GRPC_UNAVAILABLE: Int = 0x0807
    const val ERR_GRPC_DEADLINE: Int = 0x0808

    // 系统内部错误类 (0x0900-0x09FF)
    const val ERR_INTERNAL_ERROR: Int = 0x0901
    const val ERR_SERVICE_UNAVAILABLE: Int = 0x0902
    const val ERR_DATABASE_ERROR: Int = 0x0903
    const val ERR_CACHE_ERROR: Int = 0x0904
    const val ERR_QUEUE_ERROR: Int = 0x0905
    const val ERR_CONFIG_ERROR: Int = 0x0906
    const val ERR_CRYPTO_ERROR: Int = 0x0907
    const val ERR_SIGN_ERROR: Int = 0x0908
    const val ERR_VERIFY_ERROR: Int = 0x0909
    const val ERR_FEATURE_NOT_SUPPORTED: Int = 0x090A
    const val ERR_MAINTENANCE: Int = 0x090B
    const val ERR_CAPACITY_EXCEEDED: Int = 0x090C

    // TCU车端专用错误类 (0xAC00-0xACFF)
    const val ERR_TCU_BLE_ERROR: Int = 0xAC01
    const val ERR_TCU_NFC_ERROR: Int = 0xAC02
    const val ERR_TCU_UWB_ERROR: Int = 0xAC03
    const val ERR_TCU_SE_ERROR: Int = 0xAC04
    const val ERR_TCU_PAIRING_FAILED: Int = 0xAC05
    const val ERR_TCU_AUTH_FAILED: Int = 0xAC06
    const val ERR_TCU_CRYPTO_FAILED: Int = 0xAC07
    const val ERR_TCU_STORAGE_ERROR: Int = 0xAC08
    const val ERR_TCU_POWER_LOW: Int = 0xAC09
    const val ERR_TCU_WATCHDOG_RESET: Int = 0xAC0A
    const val ERR_TCU_ANTENNA_FAULT: Int = 0xAC0B
    const val ERR_TCU_MSG_QUEUE_FULL: Int = 0xAC0C
}

/**
 * 统一错误结构
 */
data class DigitalKeyError(
    val code: Int,
    override val message: String,
    val details: Map<String, Any>? = null,
    val traceId: String? = null,
    val cause: Throwable? = null
) : Exception(message) {

    val category: ErrorCategory
        get() = ErrorCategory.fromCode(code)

    val hexCode: String
        get() = "0x%04X".format(code)

    val isSuccess: Boolean
        get() = code == DkErrorCode.SUCCESS

    companion object {
        val SUCCESS = DigitalKeyError(DkErrorCode.SUCCESS, "Success")

        fun fromCode(code: Int): DigitalKeyError {
            return DigitalKeyError(code, getErrorMessage(code))
        }

        fun fromCode(code: Int, details: Map<String, Any>?): DigitalKeyError {
            return DigitalKeyError(code, getErrorMessage(code), details)
        }
    }

    fun withTraceId(traceId: String): DigitalKeyError {
        return copy(traceId = traceId)
    }

    fun withDetails(vararg pairs: Pair<String, Any>): DigitalKeyError {
        return copy(details = mapOf(*pairs))
    }

    fun withCause(cause: Throwable): DigitalKeyError {
        return copy(cause = cause)
    }

    fun toMap(): Map<String, Any?> {
        return buildMap {
            put("code", code)
            put("code_hex", hexCode)
            put("name", getErrorName(code))
            put("message", message)
            put("category", category.name)
            details?.let { put("details", it) }
            traceId?.let { put("trace_id", it) }
        }
    }
}

/**
 * 获取错误码名称
 */
fun getErrorName(code: Int): String = errorNames[code] ?: "ERR_UNKNOWN"

/**
 * 获取错误信息
 */
fun getErrorMessage(code: Int): String = errorMessages[code] ?: "Unknown error"

private val errorNames = mapOf(
    DkErrorCode.SUCCESS to "SUCCESS",
    DkErrorCode.SUCCESS_ASYNC to "SUCCESS_ASYNC",
    DkErrorCode.SUCCESS_PARTIAL to "SUCCESS_PARTIAL",
    DkErrorCode.ERR_INVALID_REQUEST to "ERR_INVALID_REQUEST",
    DkErrorCode.ERR_INVALID_PARAMETER to "ERR_INVALID_PARAMETER",
    DkErrorCode.ERR_MISSING_PARAMETER to "ERR_MISSING_PARAMETER",
    DkErrorCode.ERR_INVALID_FORMAT to "ERR_INVALID_FORMAT",
    DkErrorCode.ERR_INVALID_LENGTH to "ERR_INVALID_LENGTH",
    DkErrorCode.ERR_INVALID_SIGNATURE to "ERR_INVALID_SIGNATURE",
    DkErrorCode.ERR_RATE_LIMIT to "ERR_RATE_LIMIT",
    DkErrorCode.ERR_QUOTA_EXCEEDED to "ERR_QUOTA_EXCEEDED",
    DkErrorCode.ERR_VERSION_MISMATCH to "ERR_VERSION_MISMATCH",
    DkErrorCode.ERR_UNAUTHORIZED to "ERR_UNAUTHORIZED",
    DkErrorCode.ERR_INVALID_TOKEN to "ERR_INVALID_TOKEN",
    DkErrorCode.ERR_TOKEN_EXPIRED to "ERR_TOKEN_EXPIRED",
    DkErrorCode.ERR_FORBIDDEN to "ERR_FORBIDDEN",
    DkErrorCode.ERR_ACCESS_DENIED to "ERR_ACCESS_DENIED",
    DkErrorCode.ERR_USER_DISABLED to "ERR_USER_DISABLED",
    DkErrorCode.ERR_ACCOUNT_LOCKED to "ERR_ACCOUNT_LOCKED",
    DkErrorCode.ERR_SESSION_EXPIRED to "ERR_SESSION_EXPIRED",
    DkErrorCode.ERR_KEY_NOT_FOUND to "ERR_KEY_NOT_FOUND",
    DkErrorCode.ERR_KEY_EXISTS to "ERR_KEY_EXISTS",
    DkErrorCode.ERR_KEY_EXPIRED to "ERR_KEY_EXPIRED",
    DkErrorCode.ERR_KEY_REVOKED to "ERR_KEY_REVOKED",
    DkErrorCode.ERR_KEY_SUSPENDED to "ERR_KEY_SUSPENDED",
    DkErrorCode.ERR_KEY_PENDING to "ERR_KEY_PENDING",
    DkErrorCode.ERR_KEY_MAX_REACHED to "ERR_KEY_MAX_REACHED",
    DkErrorCode.ERR_KEY_MAX_USES to "ERR_KEY_MAX_USES",
    DkErrorCode.ERR_DISTANCE_EXCEEDED to "ERR_DISTANCE_EXCEEDED",
    DkErrorCode.ERR_KEY_TYPE_NOT_ALLOWED to "ERR_KEY_TYPE_NOT_ALLOWED",
    DkErrorCode.ERR_KEY_NOT_ACTIVE to "ERR_KEY_NOT_ACTIVE",
    DkErrorCode.ERR_KEY_VALIDITY_NOT_STARTED to "ERR_KEY_VALIDITY_NOT_STARTED",
    DkErrorCode.ERR_KEY_BIND_FAILED to "ERR_KEY_BIND_FAILED",
    DkErrorCode.ERR_KEY_UNBIND_FAILED to "ERR_KEY_UNBIND_FAILED",
    DkErrorCode.ERR_VEHICLE_NOT_FOUND to "ERR_VEHICLE_NOT_FOUND",
    DkErrorCode.ERR_VEHICLE_OFFLINE to "ERR_VEHICLE_OFFLINE",
    DkErrorCode.ERR_VEHICLE_NOT_BOUND to "ERR_VEHICLE_NOT_BOUND",
    DkErrorCode.ERR_TCU_OFFLINE to "ERR_TCU_OFFLINE",
    DkErrorCode.ERR_TCU_TIMEOUT to "ERR_TCU_TIMEOUT",
    DkErrorCode.ERR_TCU_ERROR to "ERR_TCU_ERROR",
    DkErrorCode.ERR_COMMAND_FAILED to "ERR_COMMAND_FAILED",
    DkErrorCode.ERR_COMMAND_TIMEOUT to "ERR_COMMAND_TIMEOUT",
    DkErrorCode.ERR_COMMAND_REJECTED to "ERR_COMMAND_REJECTED",
    DkErrorCode.ERR_COMMAND_INVALID to "ERR_COMMAND_INVALID",
    DkErrorCode.ERR_COMMAND_IN_PROGRESS to "ERR_COMMAND_IN_PROGRESS",
    DkErrorCode.ERR_VEHICLE_NOT_SUPPORTED to "ERR_VEHICLE_NOT_SUPPORTED",
    DkErrorCode.ERR_BATTERY_LOW to "ERR_BATTERY_LOW",
    DkErrorCode.ERR_ENGINE_RUNNING to "ERR_ENGINE_RUNNING",
    DkErrorCode.ERR_VEHICLE_MOVING to "ERR_VEHICLE_MOVING",
    DkErrorCode.ERR_DOOR_OPEN to "ERR_DOOR_OPEN",
    DkErrorCode.ERR_SHARE_NOT_FOUND to "ERR_SHARE_NOT_FOUND",
    DkErrorCode.ERR_SHARE_EXPIRED to "ERR_SHARE_EXPIRED",
    DkErrorCode.ERR_SHARE_ACCEPTED to "ERR_SHARE_ACCEPTED",
    DkErrorCode.ERR_SHARE_CANCELLED to "ERR_SHARE_CANCELLED",
    DkErrorCode.ERR_SHARE_MAX_USES to "ERR_SHARE_MAX_USES",
    DkErrorCode.ERR_SHARE_NOT_ALLOWED to "ERR_SHARE_NOT_ALLOWED",
    DkErrorCode.ERR_CODE_INVALID to "ERR_CODE_INVALID",
    DkErrorCode.ERR_CODE_EXPIRED to "ERR_CODE_EXPIRED",
    DkErrorCode.ERR_VENDOR_NOT_MATCH to "ERR_VENDOR_NOT_MATCH",
    DkErrorCode.ERR_SHARE_TO_SELF to "ERR_SHARE_TO_SELF",
    DkErrorCode.ERR_DEVICE_NOT_FOUND to "ERR_DEVICE_NOT_FOUND",
    DkErrorCode.ERR_DEVICE_NOT_BOUND to "ERR_DEVICE_NOT_BOUND",
    DkErrorCode.ERR_DEVICE_DISABLED to "ERR_DEVICE_DISABLED",
    DkErrorCode.ERR_DEVICE_NOT_SUPPORTED to "ERR_DEVICE_NOT_SUPPORTED",
    DkErrorCode.ERR_BLE_DISABLED to "ERR_BLE_DISABLED",
    DkErrorCode.ERR_NFC_DISABLED to "ERR_NFC_DISABLED",
    DkErrorCode.ERR_UWB_DISABLED to "ERR_UWB_DISABLED",
    DkErrorCode.ERR_SE_NOT_AVAILABLE to "ERR_SE_NOT_AVAILABLE",
    DkErrorCode.ERR_SE_ERROR to "ERR_SE_ERROR",
    DkErrorCode.ERR_STORAGE_FULL to "ERR_STORAGE_FULL",
    DkErrorCode.ERR_BLE_SCAN_FAILED to "ERR_BLE_SCAN_FAILED",
    DkErrorCode.ERR_BLE_CONNECT_FAILED to "ERR_BLE_CONNECT_FAILED",
    DkErrorCode.ERR_NFC_READ_FAILED to "ERR_NFC_READ_FAILED",
    DkErrorCode.ERR_NFC_WRITE_FAILED to "ERR_NFC_WRITE_FAILED",
    DkErrorCode.ERR_UWB_RANGING_FAILED to "ERR_UWB_RANGING_FAILED",
    DkErrorCode.ERR_VENDOR_NOT_SUPPORTED to "ERR_VENDOR_NOT_SUPPORTED",
    DkErrorCode.ERR_ADAPTER_NOT_FOUND to "ERR_ADAPTER_NOT_FOUND",
    DkErrorCode.ERR_VENDOR_API_ERROR to "ERR_VENDOR_API_ERROR",
    DkErrorCode.ERR_VENDOR_API_TIMEOUT to "ERR_VENDOR_API_TIMEOUT",
    DkErrorCode.ERR_VENDOR_AUTH_FAILED to "ERR_VENDOR_AUTH_FAILED",
    DkErrorCode.ERR_VENDOR_BIND_FAILED to "ERR_VENDOR_BIND_FAILED",
    DkErrorCode.ERR_PROTOCOL_ERROR to "ERR_PROTOCOL_ERROR",
    DkErrorCode.ERR_NETWORK_ERROR to "ERR_NETWORK_ERROR",
    DkErrorCode.ERR_NETWORK_TIMEOUT to "ERR_NETWORK_TIMEOUT",
    DkErrorCode.ERR_SERVER_UNREACHABLE to "ERR_SERVER_UNREACHABLE",
    DkErrorCode.ERR_CONNECTION_REFUSED to "ERR_CONNECTION_REFUSED",
    DkErrorCode.ERR_MQTT_DISCONNECTED to "ERR_MQTT_DISCONNECTED",
    DkErrorCode.ERR_MQTT_PUBLISH_FAILED to "ERR_MQTT_PUBLISH_FAILED",
    DkErrorCode.ERR_GRPC_UNAVAILABLE to "ERR_GRPC_UNAVAILABLE",
    DkErrorCode.ERR_GRPC_DEADLINE to "ERR_GRPC_DEADLINE",
    DkErrorCode.ERR_INTERNAL_ERROR to "ERR_INTERNAL_ERROR",
    DkErrorCode.ERR_SERVICE_UNAVAILABLE to "ERR_SERVICE_UNAVAILABLE",
    DkErrorCode.ERR_DATABASE_ERROR to "ERR_DATABASE_ERROR",
    DkErrorCode.ERR_CACHE_ERROR to "ERR_CACHE_ERROR",
    DkErrorCode.ERR_QUEUE_ERROR to "ERR_QUEUE_ERROR",
    DkErrorCode.ERR_CONFIG_ERROR to "ERR_CONFIG_ERROR",
    DkErrorCode.ERR_CRYPTO_ERROR to "ERR_CRYPTO_ERROR",
    DkErrorCode.ERR_SIGN_ERROR to "ERR_SIGN_ERROR",
    DkErrorCode.ERR_VERIFY_ERROR to "ERR_VERIFY_ERROR",
    DkErrorCode.ERR_FEATURE_NOT_SUPPORTED to "ERR_FEATURE_NOT_SUPPORTED",
    DkErrorCode.ERR_MAINTENANCE to "ERR_MAINTENANCE",
    DkErrorCode.ERR_CAPACITY_EXCEEDED to "ERR_CAPACITY_EXCEEDED",
    DkErrorCode.ERR_TCU_BLE_ERROR to "ERR_TCU_BLE_ERROR",
    DkErrorCode.ERR_TCU_NFC_ERROR to "ERR_TCU_NFC_ERROR",
    DkErrorCode.ERR_TCU_UWB_ERROR to "ERR_TCU_UWB_ERROR",
    DkErrorCode.ERR_TCU_SE_ERROR to "ERR_TCU_SE_ERROR",
    DkErrorCode.ERR_TCU_PAIRING_FAILED to "ERR_TCU_PAIRING_FAILED",
    DkErrorCode.ERR_TCU_AUTH_FAILED to "ERR_TCU_AUTH_FAILED",
    DkErrorCode.ERR_TCU_CRYPTO_FAILED to "ERR_TCU_CRYPTO_FAILED",
    DkErrorCode.ERR_TCU_STORAGE_ERROR to "ERR_TCU_STORAGE_ERROR",
    DkErrorCode.ERR_TCU_POWER_LOW to "ERR_TCU_POWER_LOW",
    DkErrorCode.ERR_TCU_WATCHDOG_RESET to "ERR_TCU_WATCHDOG_RESET",
    DkErrorCode.ERR_TCU_ANTENNA_FAULT to "ERR_TCU_ANTENNA_FAULT",
    DkErrorCode.ERR_TCU_MSG_QUEUE_FULL to "ERR_TCU_MSG_QUEUE_FULL"
)

private val errorMessages = mapOf(
    DkErrorCode.SUCCESS to "成功",
    DkErrorCode.SUCCESS_ASYNC to "异步成功，已接收",
    DkErrorCode.SUCCESS_PARTIAL to "部分成功",
    DkErrorCode.ERR_INVALID_REQUEST to "无效请求",
    DkErrorCode.ERR_INVALID_PARAMETER to "参数错误",
    DkErrorCode.ERR_MISSING_PARAMETER to "缺少参数",
    DkErrorCode.ERR_INVALID_FORMAT to "格式错误",
    DkErrorCode.ERR_INVALID_LENGTH to "长度错误",
    DkErrorCode.ERR_INVALID_SIGNATURE to "签名无效",
    DkErrorCode.ERR_RATE_LIMIT to "请求过于频繁",
    DkErrorCode.ERR_QUOTA_EXCEEDED to "配额超限",
    DkErrorCode.ERR_VERSION_MISMATCH to "版本不匹配",
    DkErrorCode.ERR_UNAUTHORIZED to "未授权",
    DkErrorCode.ERR_INVALID_TOKEN to "Token无效",
    DkErrorCode.ERR_TOKEN_EXPIRED to "Token过期",
    DkErrorCode.ERR_FORBIDDEN to "禁止访问",
    DkErrorCode.ERR_ACCESS_DENIED to "权限不足",
    DkErrorCode.ERR_USER_DISABLED to "用户已禁用",
    DkErrorCode.ERR_ACCOUNT_LOCKED to "账户已锁定",
    DkErrorCode.ERR_SESSION_EXPIRED to "会话已过期",
    DkErrorCode.ERR_KEY_NOT_FOUND to "密钥不存在",
    DkErrorCode.ERR_KEY_EXISTS to "密钥已存在",
    DkErrorCode.ERR_KEY_EXPIRED to "密钥已过期",
    DkErrorCode.ERR_KEY_REVOKED to "密钥已撤销",
    DkErrorCode.ERR_KEY_SUSPENDED to "密钥已挂起",
    DkErrorCode.ERR_KEY_PENDING to "密钥待激活",
    DkErrorCode.ERR_KEY_MAX_REACHED to "密钥数量达上限",
    DkErrorCode.ERR_KEY_MAX_USES to "使用次数已用完",
    DkErrorCode.ERR_DISTANCE_EXCEEDED to "距离超出限制",
    DkErrorCode.ERR_KEY_TYPE_NOT_ALLOWED to "密钥类型不允许",
    DkErrorCode.ERR_KEY_NOT_ACTIVE to "密钥未激活",
    DkErrorCode.ERR_KEY_VALIDITY_NOT_STARTED to "密钥未到生效时间",
    DkErrorCode.ERR_KEY_BIND_FAILED to "密钥绑定失败",
    DkErrorCode.ERR_KEY_UNBIND_FAILED to "密钥解绑失败",
    DkErrorCode.ERR_VEHICLE_NOT_FOUND to "车辆不存在",
    DkErrorCode.ERR_VEHICLE_OFFLINE to "车辆离线",
    DkErrorCode.ERR_VEHICLE_NOT_BOUND to "车辆未绑定",
    DkErrorCode.ERR_TCU_OFFLINE to "TCU离线",
    DkErrorCode.ERR_TCU_TIMEOUT to "TCU超时",
    DkErrorCode.ERR_TCU_ERROR to "TCU内部错误",
    DkErrorCode.ERR_COMMAND_FAILED to "指令执行失败",
    DkErrorCode.ERR_COMMAND_TIMEOUT to "指令执行超时",
    DkErrorCode.ERR_COMMAND_REJECTED to "指令被拒绝",
    DkErrorCode.ERR_COMMAND_INVALID to "指令无效",
    DkErrorCode.ERR_COMMAND_IN_PROGRESS to "指令执行中",
    DkErrorCode.ERR_VEHICLE_NOT_SUPPORTED to "车辆不支持该操作",
    DkErrorCode.ERR_BATTERY_LOW to "电瓶电量低",
    DkErrorCode.ERR_ENGINE_RUNNING to "引擎运行中",
    DkErrorCode.ERR_VEHICLE_MOVING to "车辆正在移动",
    DkErrorCode.ERR_DOOR_OPEN to "车门未关闭",
    DkErrorCode.ERR_SHARE_NOT_FOUND to "分享不存在",
    DkErrorCode.ERR_SHARE_EXPIRED to "分享已过期",
    DkErrorCode.ERR_SHARE_ACCEPTED to "分享已被接受",
    DkErrorCode.ERR_SHARE_CANCELLED to "分享已取消",
    DkErrorCode.ERR_SHARE_MAX_USES to "分享次数已用完",
    DkErrorCode.ERR_SHARE_NOT_ALLOWED to "不允许分享",
    DkErrorCode.ERR_CODE_INVALID to "分享码无效",
    DkErrorCode.ERR_CODE_EXPIRED to "分享码过期",
    DkErrorCode.ERR_VENDOR_NOT_MATCH to "厂商不匹配",
    DkErrorCode.ERR_SHARE_TO_SELF to "不能分享给自己",
    DkErrorCode.ERR_DEVICE_NOT_FOUND to "设备不存在",
    DkErrorCode.ERR_DEVICE_NOT_BOUND to "设备未绑定",
    DkErrorCode.ERR_DEVICE_DISABLED to "设备已禁用",
    DkErrorCode.ERR_DEVICE_NOT_SUPPORTED to "设备不支持",
    DkErrorCode.ERR_BLE_DISABLED to "蓝牙未开启",
    DkErrorCode.ERR_NFC_DISABLED to "NFC未开启",
    DkErrorCode.ERR_UWB_DISABLED to "UWB未开启",
    DkErrorCode.ERR_SE_NOT_AVAILABLE to "安全元件不可用",
    DkErrorCode.ERR_SE_ERROR to "SE通信错误",
    DkErrorCode.ERR_STORAGE_FULL to "存储满",
    DkErrorCode.ERR_BLE_SCAN_FAILED to "BLE扫描失败",
    DkErrorCode.ERR_BLE_CONNECT_FAILED to "BLE连接失败",
    DkErrorCode.ERR_NFC_READ_FAILED to "NFC读取失败",
    DkErrorCode.ERR_NFC_WRITE_FAILED to "NFC写入失败",
    DkErrorCode.ERR_UWB_RANGING_FAILED to "UWB测距失败",
    DkErrorCode.ERR_VENDOR_NOT_SUPPORTED to "厂商不支持",
    DkErrorCode.ERR_ADAPTER_NOT_FOUND to "适配器未找到",
    DkErrorCode.ERR_VENDOR_API_ERROR to "厂商API错误",
    DkErrorCode.ERR_VENDOR_API_TIMEOUT to "厂商API超时",
    DkErrorCode.ERR_VENDOR_AUTH_FAILED to "厂商认证失败",
    DkErrorCode.ERR_VENDOR_BIND_FAILED to "厂商绑定失败",
    DkErrorCode.ERR_PROTOCOL_ERROR to "协议错误",
    DkErrorCode.ERR_NETWORK_ERROR to "网络错误",
    DkErrorCode.ERR_NETWORK_TIMEOUT to "网络超时",
    DkErrorCode.ERR_SERVER_UNREACHABLE to "服务器不可达",
    DkErrorCode.ERR_CONNECTION_REFUSED to "连接被拒绝",
    DkErrorCode.ERR_MQTT_DISCONNECTED to "MQTT断开",
    DkErrorCode.ERR_MQTT_PUBLISH_FAILED to "MQTT发布失败",
    DkErrorCode.ERR_GRPC_UNAVAILABLE to "gRPC不可用",
    DkErrorCode.ERR_GRPC_DEADLINE to "gRPC超时",
    DkErrorCode.ERR_INTERNAL_ERROR to "内部错误",
    DkErrorCode.ERR_SERVICE_UNAVAILABLE to "服务不可用",
    DkErrorCode.ERR_DATABASE_ERROR to "数据库错误",
    DkErrorCode.ERR_CACHE_ERROR to "缓存错误",
    DkErrorCode.ERR_QUEUE_ERROR to "队列错误",
    DkErrorCode.ERR_CONFIG_ERROR to "配置错误",
    DkErrorCode.ERR_CRYPTO_ERROR to "加密错误",
    DkErrorCode.ERR_SIGN_ERROR to "签名错误",
    DkErrorCode.ERR_VERIFY_ERROR to "验签错误",
    DkErrorCode.ERR_FEATURE_NOT_SUPPORTED to "功能不支持",
    DkErrorCode.ERR_MAINTENANCE to "系统维护",
    DkErrorCode.ERR_CAPACITY_EXCEEDED to "容量超限",
    DkErrorCode.ERR_TCU_BLE_ERROR to "BLE通信错误",
    DkErrorCode.ERR_TCU_NFC_ERROR to "NFC通信错误",
    DkErrorCode.ERR_TCU_UWB_ERROR to "UWB通信错误",
    DkErrorCode.ERR_TCU_SE_ERROR to "SE通信错误",
    DkErrorCode.ERR_TCU_PAIRING_FAILED to "配对失败",
    DkErrorCode.ERR_TCU_AUTH_FAILED to "认证失败",
    DkErrorCode.ERR_TCU_CRYPTO_FAILED to "加密失败",
    DkErrorCode.ERR_TCU_STORAGE_ERROR to "存储错误",
    DkErrorCode.ERR_TCU_POWER_LOW to "电量低",
    DkErrorCode.ERR_TCU_WATCHDOG_RESET to "看门狗复位",
    DkErrorCode.ERR_TCU_ANTENNA_FAULT to "天线故障",
    DkErrorCode.ERR_TCU_MSG_QUEUE_FULL to "消息队列满"
)
