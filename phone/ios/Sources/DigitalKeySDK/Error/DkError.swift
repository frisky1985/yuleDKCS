// DkError.swift - 统一错误码 - iOS SDK端
// 三端统一错误码: Cloud / SDK / Vehicle

import Foundation

// MARK: - Error Category

/// 错误码类别
public enum ErrorCategory: UInt8 {
    case success = 0x00
    case request = 0x01
    case auth = 0x02
    case key = 0x03
    case vehicle = 0x04
    case share = 0x05
    case device = 0x06
    case vendor = 0x07
    case transport = 0x08
    case system = 0x09
    case tcu = 0xAC
    
    public var name: String {
        switch self {
        case .success: return "SUCCESS"
        case .request: return "REQUEST"
        case .auth: return "AUTH"
        case .key: return "KEY"
        case .vehicle: return "VEHICLE"
        case .share: return "SHARE"
        case .device: return "DEVICE"
        case .vendor: return "VENDOR"
        case .transport: return "TRANSPORT"
        case .system: return "SYSTEM"
        case .tcu: return "TCU"
        }
    }
    
    public static func from(code: UInt16) -> ErrorCategory {
        return ErrorCategory(rawValue: UInt8(code >> 8)) ?? .system
    }
}

// MARK: - Error Code Constants

/// 统一错误码
public enum DkErrorCode {
    // MARK: Success (0x0000-0x00FF)
    public static let success: UInt16 = 0x0000
    public static let successAsync: UInt16 = 0x0001
    public static let successPartial: UInt16 = 0x0002
    
    // MARK: Request Errors (0x0100-0x01FF)
    public static let invalidRequest: UInt16 = 0x0101
    public static let invalidParameter: UInt16 = 0x0102
    public static let missingParameter: UInt16 = 0x0103
    public static let invalidFormat: UInt16 = 0x0104
    public static let invalidLength: UInt16 = 0x0105
    public static let invalidSignature: UInt16 = 0x0106
    public static let rateLimit: UInt16 = 0x0107
    public static let quotaExceeded: UInt16 = 0x0108
    public static let versionMismatch: UInt16 = 0x0109
    
    // MARK: Auth Errors (0x0200-0x02FF)
    public static let unauthorized: UInt16 = 0x0201
    public static let invalidToken: UInt16 = 0x0202
    public static let tokenExpired: UInt16 = 0x0203
    public static let forbidden: UInt16 = 0x0204
    public static let accessDenied: UInt16 = 0x0205
    public static let userDisabled: UInt16 = 0x0206
    public static let accountLocked: UInt16 = 0x0207
    public static let sessionExpired: UInt16 = 0x0208
    
    // MARK: Key Errors (0x0300-0x03FF)
    public static let keyNotFound: UInt16 = 0x0301
    public static let keyExists: UInt16 = 0x0302
    public static let keyExpired: UInt16 = 0x0303
    public static let keyRevoked: UInt16 = 0x0304
    public static let keySuspended: UInt16 = 0x0305
    public static let keyPending: UInt16 = 0x0306
    public static let keyMaxReached: UInt16 = 0x0307
    public static let keyMaxUses: UInt16 = 0x0308
    public static let distanceExceeded: UInt16 = 0x0309
    public static let keyTypeNotAllowed: UInt16 = 0x030A
    public static let keyNotActive: UInt16 = 0x030B
    public static let keyValidityNotStarted: UInt16 = 0x030C
    public static let keyBindFailed: UInt16 = 0x030D
    public static let keyUnbindFailed: UInt16 = 0x030E
    
    // MARK: Vehicle Errors (0x0400-0x04FF)
    public static let vehicleNotFound: UInt16 = 0x0401
    public static let vehicleOffline: UInt16 = 0x0402
    public static let vehicleNotBound: UInt16 = 0x0403
    public static let tcuOffline: UInt16 = 0x0404
    public static let tcuTimeout: UInt16 = 0x0405
    public static let tcuError: UInt16 = 0x0406
    public static let commandFailed: UInt16 = 0x0407
    public static let commandTimeout: UInt16 = 0x0408
    public static let commandRejected: UInt16 = 0x0409
    public static let commandInvalid: UInt16 = 0x040A
    public static let commandInProgress: UInt16 = 0x040B
    public static let vehicleNotSupported: UInt16 = 0x040C
    public static let batteryLow: UInt16 = 0x040D
    public static let engineRunning: UInt16 = 0x040E
    public static let vehicleMoving: UInt16 = 0x040F
    public static let doorOpen: UInt16 = 0x0410
    
    // MARK: Share Errors (0x0500-0x05FF)
    public static let shareNotFound: UInt16 = 0x0501
    public static let shareExpired: UInt16 = 0x0502
    public static let shareAccepted: UInt16 = 0x0503
    public static let shareCancelled: UInt16 = 0x0504
    public static let shareMaxUses: UInt16 = 0x0505
    public static let shareNotAllowed: UInt16 = 0x0506
    public static let codeInvalid: UInt16 = 0x0507
    public static let codeExpired: UInt16 = 0x0508
    public static let vendorNotMatch: UInt16 = 0x0509
    public static let shareToSelf: UInt16 = 0x050A
    
    // MARK: Device Errors (0x0600-0x06FF)
    public static let deviceNotFound: UInt16 = 0x0601
    public static let deviceNotBound: UInt16 = 0x0602
    public static let deviceDisabled: UInt16 = 0x0603
    public static let deviceNotSupported: UInt16 = 0x0604
    public static let bleDisabled: UInt16 = 0x0605
    public static let nfcDisabled: UInt16 = 0x0606
    public static let uwbDisabled: UInt16 = 0x0607
    public static let seNotAvailable: UInt16 = 0x0608
    public static let seError: UInt16 = 0x0609
    public static let storageFull: UInt16 = 0x060A
    public static let bleScanFailed: UInt16 = 0x060B
    public static let bleConnectFailed: UInt16 = 0x060C
    public static let nfcReadFailed: UInt16 = 0x060D
    public static let nfcWriteFailed: UInt16 = 0x060E
    public static let uwbRangingFailed: UInt16 = 0x060F
    
    // MARK: Vendor Errors (0x0700-0x07FF)
    public static let vendorNotSupported: UInt16 = 0x0701
    public static let adapterNotFound: UInt16 = 0x0702
    public static let vendorApiError: UInt16 = 0x0703
    public static let vendorApiTimeout: UInt16 = 0x0704
    public static let vendorAuthFailed: UInt16 = 0x0705
    public static let vendorBindFailed: UInt16 = 0x0706
    public static let protocolError: UInt16 = 0x0707
    
    // MARK: Transport Errors (0x0800-0x08FF)
    public static let networkError: UInt16 = 0x0801
    public static let networkTimeout: UInt16 = 0x0802
    public static let serverUnreachable: UInt16 = 0x0803
    public static let connectionRefused: UInt16 = 0x0804
    public static let mqttDisconnected: UInt16 = 0x0805
    public static let mqttPublishFailed: UInt16 = 0x0806
    public static let grpcUnavailable: UInt16 = 0x0807
    public static let grpcDeadline: UInt16 = 0x0808
    
    // MARK: System Errors (0x0900-0x09FF)
    public static let internalError: UInt16 = 0x0901
    public static let serviceUnavailable: UInt16 = 0x0902
    public static let databaseError: UInt16 = 0x0903
    public static let cacheError: UInt16 = 0x0904
    public static let queueError: UInt16 = 0x0905
    public static let configError: UInt16 = 0x0906
    public static let cryptoError: UInt16 = 0x0907
    public static let signError: UInt16 = 0x0908
    public static let verifyError: UInt16 = 0x0909
    public static let featureNotSupported: UInt16 = 0x090A
    public static let maintenance: UInt16 = 0x090B
    public static let capacityExceeded: UInt16 = 0x090C
    
    // MARK: TCU Errors (0xAC00-0xACFF)
    public static let tcuBleError: UInt16 = 0xAC01
    public static let tcuNfcError: UInt16 = 0xAC02
    public static let tcuUwbError: UInt16 = 0xAC03
    public static let tcuSeError: UInt16 = 0xAC04
    public static let tcuPairingFailed: UInt16 = 0xAC05
    public static let tcuAuthFailed: UInt16 = 0xAC06
    public static let tcuCryptoFailed: UInt16 = 0xAC07
    public static let tcuStorageError: UInt16 = 0xAC08
    public static let tcuPowerLow: UInt16 = 0xAC09
    public static let tcuWatchdogReset: UInt16 = 0xAC0A
    public static let tcuAntennaFault: UInt16 = 0xAC0B
    public static let tcuMsgQueueFull: UInt16 = 0xAC0C
}

// MARK: - Error Names

private let errorNames: [UInt16: String] = [
    DkErrorCode.success: "SUCCESS",
    DkErrorCode.successAsync: "SUCCESS_ASYNC",
    DkErrorCode.successPartial: "SUCCESS_PARTIAL",
    DkErrorCode.invalidRequest: "ERR_INVALID_REQUEST",
    DkErrorCode.invalidParameter: "ERR_INVALID_PARAMETER",
    DkErrorCode.missingParameter: "ERR_MISSING_PARAMETER",
    DkErrorCode.invalidFormat: "ERR_INVALID_FORMAT",
    DkErrorCode.invalidLength: "ERR_INVALID_LENGTH",
    DkErrorCode.invalidSignature: "ERR_INVALID_SIGNATURE",
    DkErrorCode.rateLimit: "ERR_RATE_LIMIT",
    DkErrorCode.quotaExceeded: "ERR_QUOTA_EXCEEDED",
    DkErrorCode.versionMismatch: "ERR_VERSION_MISMATCH",
    DkErrorCode.unauthorized: "ERR_UNAUTHORIZED",
    DkErrorCode.invalidToken: "ERR_INVALID_TOKEN",
    DkErrorCode.tokenExpired: "ERR_TOKEN_EXPIRED",
    DkErrorCode.forbidden: "ERR_FORBIDDEN",
    DkErrorCode.accessDenied: "ERR_ACCESS_DENIED",
    DkErrorCode.userDisabled: "ERR_USER_DISABLED",
    DkErrorCode.accountLocked: "ERR_ACCOUNT_LOCKED",
    DkErrorCode.sessionExpired: "ERR_SESSION_EXPIRED",
    DkErrorCode.keyNotFound: "ERR_KEY_NOT_FOUND",
    DkErrorCode.keyExists: "ERR_KEY_EXISTS",
    DkErrorCode.keyExpired: "ERR_KEY_EXPIRED",
    DkErrorCode.keyRevoked: "ERR_KEY_REVOKED",
    DkErrorCode.keySuspended: "ERR_KEY_SUSPENDED",
    DkErrorCode.keyPending: "ERR_KEY_PENDING",
    DkErrorCode.keyMaxReached: "ERR_KEY_MAX_REACHED",
    DkErrorCode.keyMaxUses: "ERR_KEY_MAX_USES",
    DkErrorCode.distanceExceeded: "ERR_DISTANCE_EXCEEDED",
    DkErrorCode.keyTypeNotAllowed: "ERR_KEY_TYPE_NOT_ALLOWED",
    DkErrorCode.keyNotActive: "ERR_KEY_NOT_ACTIVE",
    DkErrorCode.keyValidityNotStarted: "ERR_KEY_VALIDITY_NOT_STARTED",
    DkErrorCode.keyBindFailed: "ERR_KEY_BIND_FAILED",
    DkErrorCode.keyUnbindFailed: "ERR_KEY_UNBIND_FAILED",
    DkErrorCode.vehicleNotFound: "ERR_VEHICLE_NOT_FOUND",
    DkErrorCode.vehicleOffline: "ERR_VEHICLE_OFFLINE",
    DkErrorCode.vehicleNotBound: "ERR_VEHICLE_NOT_BOUND",
    DkErrorCode.tcuOffline: "ERR_TCU_OFFLINE",
    DkErrorCode.tcuTimeout: "ERR_TCU_TIMEOUT",
    DkErrorCode.tcuError: "ERR_TCU_ERROR",
    DkErrorCode.commandFailed: "ERR_COMMAND_FAILED",
    DkErrorCode.commandTimeout: "ERR_COMMAND_TIMEOUT",
    DkErrorCode.commandRejected: "ERR_COMMAND_REJECTED",
    DkErrorCode.commandInvalid: "ERR_COMMAND_INVALID",
    DkErrorCode.commandInProgress: "ERR_COMMAND_IN_PROGRESS",
    DkErrorCode.vehicleNotSupported: "ERR_VEHICLE_NOT_SUPPORTED",
    DkErrorCode.batteryLow: "ERR_BATTERY_LOW",
    DkErrorCode.engineRunning: "ERR_ENGINE_RUNNING",
    DkErrorCode.vehicleMoving: "ERR_VEHICLE_MOVING",
    DkErrorCode.doorOpen: "ERR_DOOR_OPEN",
    DkErrorCode.shareNotFound: "ERR_SHARE_NOT_FOUND",
    DkErrorCode.shareExpired: "ERR_SHARE_EXPIRED",
    DkErrorCode.shareAccepted: "ERR_SHARE_ACCEPTED",
    DkErrorCode.shareCancelled: "ERR_SHARE_CANCELLED",
    DkErrorCode.shareMaxUses: "ERR_SHARE_MAX_USES",
    DkErrorCode.shareNotAllowed: "ERR_SHARE_NOT_ALLOWED",
    DkErrorCode.codeInvalid: "ERR_CODE_INVALID",
    DkErrorCode.codeExpired: "ERR_CODE_EXPIRED",
    DkErrorCode.vendorNotMatch: "ERR_VENDOR_NOT_MATCH",
    DkErrorCode.shareToSelf: "ERR_SHARE_TO_SELF",
    DkErrorCode.deviceNotFound: "ERR_DEVICE_NOT_FOUND",
    DkErrorCode.deviceNotBound: "ERR_DEVICE_NOT_BOUND",
    DkErrorCode.deviceDisabled: "ERR_DEVICE_DISABLED",
    DkErrorCode.deviceNotSupported: "ERR_DEVICE_NOT_SUPPORTED",
    DkErrorCode.bleDisabled: "ERR_BLE_DISABLED",
    DkErrorCode.nfcDisabled: "ERR_NFC_DISABLED",
    DkErrorCode.uwbDisabled: "ERR_UWB_DISABLED",
    DkErrorCode.seNotAvailable: "ERR_SE_NOT_AVAILABLE",
    DkErrorCode.seError: "ERR_SE_ERROR",
    DkErrorCode.storageFull: "ERR_STORAGE_FULL",
    DkErrorCode.bleScanFailed: "ERR_BLE_SCAN_FAILED",
    DkErrorCode.bleConnectFailed: "ERR_BLE_CONNECT_FAILED",
    DkErrorCode.nfcReadFailed: "ERR_NFC_READ_FAILED",
    DkErrorCode.nfcWriteFailed: "ERR_NFC_WRITE_FAILED",
    DkErrorCode.uwbRangingFailed: "ERR_UWB_RANGING_FAILED",
    DkErrorCode.vendorNotSupported: "ERR_VENDOR_NOT_SUPPORTED",
    DkErrorCode.adapterNotFound: "ERR_ADAPTER_NOT_FOUND",
    DkErrorCode.vendorApiError: "ERR_VENDOR_API_ERROR",
    DkErrorCode.vendorApiTimeout: "ERR_VENDOR_API_TIMEOUT",
    DkErrorCode.vendorAuthFailed: "ERR_VENDOR_AUTH_FAILED",
    DkErrorCode.vendorBindFailed: "ERR_VENDOR_BIND_FAILED",
    DkErrorCode.protocolError: "ERR_PROTOCOL_ERROR",
    DkErrorCode.networkError: "ERR_NETWORK_ERROR",
    DkErrorCode.networkTimeout: "ERR_NETWORK_TIMEOUT",
    DkErrorCode.serverUnreachable: "ERR_SERVER_UNREACHABLE",
    DkErrorCode.connectionRefused: "ERR_CONNECTION_REFUSED",
    DkErrorCode.mqttDisconnected: "ERR_MQTT_DISCONNECTED",
    DkErrorCode.mqttPublishFailed: "ERR_MQTT_PUBLISH_FAILED",
    DkErrorCode.grpcUnavailable: "ERR_GRPC_UNAVAILABLE",
    DkErrorCode.grpcDeadline: "ERR_GRPC_DEADLINE",
    DkErrorCode.internalError: "ERR_INTERNAL_ERROR",
    DkErrorCode.serviceUnavailable: "ERR_SERVICE_UNAVAILABLE",
    DkErrorCode.databaseError: "ERR_DATABASE_ERROR",
    DkErrorCode.cacheError: "ERR_CACHE_ERROR",
    DkErrorCode.queueError: "ERR_QUEUE_ERROR",
    DkErrorCode.configError: "ERR_CONFIG_ERROR",
    DkErrorCode.cryptoError: "ERR_CRYPTO_ERROR",
    DkErrorCode.signError: "ERR_SIGN_ERROR",
    DkErrorCode.verifyError: "ERR_VERIFY_ERROR",
    DkErrorCode.featureNotSupported: "ERR_FEATURE_NOT_SUPPORTED",
    DkErrorCode.maintenance: "ERR_MAINTENANCE",
    DkErrorCode.capacityExceeded: "ERR_CAPACITY_EXCEEDED",
    DkErrorCode.tcuBleError: "ERR_TCU_BLE_ERROR",
    DkErrorCode.tcuNfcError: "ERR_TCU_NFC_ERROR",
    DkErrorCode.tcuUwbError: "ERR_TCU_UWB_ERROR",
    DkErrorCode.tcuSeError: "ERR_TCU_SE_ERROR",
    DkErrorCode.tcuPairingFailed: "ERR_TCU_PAIRING_FAILED",
    DkErrorCode.tcuAuthFailed: "ERR_TCU_AUTH_FAILED",
    DkErrorCode.tcuCryptoFailed: "ERR_TCU_CRYPTO_FAILED",
    DkErrorCode.tcuStorageError: "ERR_TCU_STORAGE_ERROR",
    DkErrorCode.tcuPowerLow: "ERR_TCU_POWER_LOW",
    DkErrorCode.tcuWatchdogReset: "ERR_TCU_WATCHDOG_RESET",
    DkErrorCode.tcuAntennaFault: "ERR_TCU_ANTENNA_FAULT",
    DkErrorCode.tcuMsgQueueFull: "ERR_TCU_MSG_QUEUE_FULL"
]

// MARK: - Error Messages

private let errorMessages: [UInt16: String] = [
    DkErrorCode.success: "成功",
    DkErrorCode.successAsync: "异步成功，已接收",
    DkErrorCode.successPartial: "部分成功",
    DkErrorCode.invalidRequest: "无效请求",
    DkErrorCode.invalidParameter: "参数错误",
    DkErrorCode.missingParameter: "缺少参数",
    DkErrorCode.invalidFormat: "格式错误",
    DkErrorCode.invalidLength: "长度错误",
    DkErrorCode.invalidSignature: "签名无效",
    DkErrorCode.rateLimit: "请求过于频繁",
    DkErrorCode.quotaExceeded: "配额超限",
    DkErrorCode.versionMismatch: "版本不匹配",
    DkErrorCode.unauthorized: "未授权",
    DkErrorCode.invalidToken: "Token无效",
    DkErrorCode.tokenExpired: "Token过期",
    DkErrorCode.forbidden: "禁止访问",
    DkErrorCode.accessDenied: "权限不足",
    DkErrorCode.userDisabled: "用户已禁用",
    DkErrorCode.accountLocked: "账户已锁定",
    DkErrorCode.sessionExpired: "会话已过期",
    DkErrorCode.keyNotFound: "密钥不存在",
    DkErrorCode.keyExists: "密钥已存在",
    DkErrorCode.keyExpired: "密钥已过期",
    DkErrorCode.keyRevoked: "密钥已撤销",
    DkErrorCode.keySuspended: "密钥已挂起",
    DkErrorCode.keyPending: "密钥待激活",
    DkErrorCode.keyMaxReached: "密钥数量达上限",
    DkErrorCode.keyMaxUses: "使用次数已用完",
    DkErrorCode.distanceExceeded: "距离超出限制",
    DkErrorCode.keyTypeNotAllowed: "密钥类型不允许",
    DkErrorCode.keyNotActive: "密钥未激活",
    DkErrorCode.keyValidityNotStarted: "密钥未到生效时间",
    DkErrorCode.keyBindFailed: "密钥绑定失败",
    DkErrorCode.keyUnbindFailed: "密钥解绑失败",
    DkErrorCode.vehicleNotFound: "车辆不存在",
    DkErrorCode.vehicleOffline: "车辆离线",
    DkErrorCode.vehicleNotBound: "车辆未绑定",
    DkErrorCode.tcuOffline: "TCU离线",
    DkErrorCode.tcuTimeout: "TCU超时",
    DkErrorCode.tcuError: "TCU内部错误",
    DkErrorCode.commandFailed: "指令执行失败",
    DkErrorCode.commandTimeout: "指令执行超时",
    DkErrorCode.commandRejected: "指令被拒绝",
    DkErrorCode.commandInvalid: "指令无效",
    DkErrorCode.commandInProgress: "指令执行中",
    DkErrorCode.vehicleNotSupported: "车辆不支持该操作",
    DkErrorCode.batteryLow: "电瓶电量低",
    DkErrorCode.engineRunning: "引擎运行中",
    DkErrorCode.vehicleMoving: "车辆正在移动",
    DkErrorCode.doorOpen: "车门未关闭",
    DkErrorCode.shareNotFound: "分享不存在",
    DkErrorCode.shareExpired: "分享已过期",
    DkErrorCode.shareAccepted: "分享已被接受",
    DkErrorCode.shareCancelled: "分享已取消",
    DkErrorCode.shareMaxUses: "分享次数已用完",
    DkErrorCode.shareNotAllowed: "不允许分享",
    DkErrorCode.codeInvalid: "分享码无效",
    DkErrorCode.codeExpired: "分享码过期",
    DkErrorCode.vendorNotMatch: "厂商不匹配",
    DkErrorCode.shareToSelf: "不能分享给自己",
    DkErrorCode.deviceNotFound: "设备不存在",
    DkErrorCode.deviceNotBound: "设备未绑定",
    DkErrorCode.deviceDisabled: "设备已禁用",
    DkErrorCode.deviceNotSupported: "设备不支持",
    DkErrorCode.bleDisabled: "蓝牙未开启",
    DkErrorCode.nfcDisabled: "NFC未开启",
    DkErrorCode.uwbDisabled: "UWB未开启",
    DkErrorCode.seNotAvailable: "安全元件不可用",
    DkErrorCode.seError: "SE通信错误",
    DkErrorCode.storageFull: "存储满",
    DkErrorCode.bleScanFailed: "BLE扫描失败",
    DkErrorCode.bleConnectFailed: "BLE连接失败",
    DkErrorCode.nfcReadFailed: "NFC读取失败",
    DkErrorCode.nfcWriteFailed: "NFC写入失败",
    DkErrorCode.uwbRangingFailed: "UWB测距失败",
    DkErrorCode.vendorNotSupported: "厂商不支持",
    DkErrorCode.adapterNotFound: "适配器未找到",
    DkErrorCode.vendorApiError: "厂商API错误",
    DkErrorCode.vendorApiTimeout: "厂商API超时",
    DkErrorCode.vendorAuthFailed: "厂商认证失败",
    DkErrorCode.vendorBindFailed: "厂商绑定失败",
    DkErrorCode.protocolError: "协议错误",
    DkErrorCode.networkError: "网络错误",
    DkErrorCode.networkTimeout: "网络超时",
    DkErrorCode.serverUnreachable: "服务器不可达",
    DkErrorCode.connectionRefused: "连接被拒绝",
    DkErrorCode.mqttDisconnected: "MQTT断开",
    DkErrorCode.mqttPublishFailed: "MQTT发布失败",
    DkErrorCode.grpcUnavailable: "gRPC不可用",
    DkErrorCode.grpcDeadline: "gRPC超时",
    DkErrorCode.internalError: "内部错误",
    DkErrorCode.serviceUnavailable: "服务不可用",
    DkErrorCode.databaseError: "数据库错误",
    DkErrorCode.cacheError: "缓存错误",
    DkErrorCode.queueError: "队列错误",
    DkErrorCode.configError: "配置错误",
    DkErrorCode.cryptoError: "加密错误",
    DkErrorCode.signError: "签名错误",
    DkErrorCode.verifyError: "验签错误",
    DkErrorCode.featureNotSupported: "功能不支持",
    DkErrorCode.maintenance: "系统维护",
    DkErrorCode.capacityExceeded: "容量超限",
    DkErrorCode.tcuBleError: "BLE通信错误",
    DkErrorCode.tcuNfcError: "NFC通信错误",
    DkErrorCode.tcuUwbError: "UWB通信错误",
    DkErrorCode.tcuSeError: "SE通信错误",
    DkErrorCode.tcuPairingFailed: "配对失败",
    DkErrorCode.tcuAuthFailed: "认证失败",
    DkErrorCode.tcuCryptoFailed: "加密失败",
    DkErrorCode.tcuStorageError: "存储错误",
    DkErrorCode.tcuPowerLow: "电量低",
    DkErrorCode.tcuWatchdogReset: "看门狗复位",
    DkErrorCode.tcuAntennaFault: "天线故障",
    DkErrorCode.tcuMsgQueueFull: "消息队列满"
]

// MARK: - DigitalKeyError

/// 统一错误结构
public struct DigitalKeyError: Error, CustomStringConvertible {
    public let code: UInt16
    public let message: String
    public let details: [String: Any]?
    public let traceId: String?
    public let cause: Error?
    
    public var category: ErrorCategory {
        return ErrorCategory.from(code: code)
    }
    
    public var hexCode: String {
        return String(format: "0x%04X", code)
    }
    
    public var name: String {
        return errorNames[code] ?? "ERR_UNKNOWN"
    }
    
    public var isSuccess: Bool {
        return code == DkErrorCode.success
    }
    
    public var description: String {
        var desc = "[\(hexCode)] \(name): \(message)"
        if let traceId = traceId {
            desc += " [trace_id=\(traceId)]"
        }
        return desc
    }
    
    public init(code: UInt16, message: String? = nil, details: [String: Any]? = nil, traceId: String? = nil, cause: Error? = nil) {
        self.code = code
        self.message = message ?? errorMessages[code] ?? "Unknown error"
        self.details = details
        self.traceId = traceId
        self.cause = cause
    }
    
    public func toMap() -> [String: Any] {
        var map: [String: Any] = [
            "code": code,
            "code_hex": hexCode,
            "name": name,
            "message": message,
            "category": category.name
        ]
        if let traceId = traceId {
            map["trace_id"] = traceId
        }
        if let details = details {
            map["details"] = details
        }
        return map
    }
    
    // MARK: - Factory Methods
    
    public static let success = DigitalKeyError(code: DkErrorCode.success)
    
    public static func from(code: UInt16) -> DigitalKeyError {
        return DigitalKeyError(code: code)
    }
    
    public static func from(code: UInt16, details: [String: Any]?) -> DigitalKeyError {
        return DigitalKeyError(code: code, details: details)
    }
    
    // MARK: - With Methods
    
    public func withTraceId(_ traceId: String) -> DigitalKeyError {
        return DigitalKeyError(code: code, message: message, details: details, traceId: traceId, cause: cause)
    }
    
    public func withDetails(_ details: [String: Any]) -> DigitalKeyError {
        return DigitalKeyError(code: code, message: message, details: details, traceId: traceId, cause: cause)
    }
}

// MARK: - Helper Functions

/// 获取错误码名称
public func getErrorName(_ code: UInt16) -> String {
    return errorNames[code] ?? "ERR_UNKNOWN"
}

/// 获取错误信息
public func getErrorMessage(_ code: UInt16) -> String {
    return errorMessages[code] ?? "Unknown error"
}
