import Foundation
import CoreBluetooth

// MARK: - BLE 状态

public enum YDKBLEState: String {
    case unknown, resetting, unsupported, unauthorized, poweredOff, poweredOn

    public init(cbState: CBManagerState) {
        switch cbState {
        case .unknown: self = .unknown
        case .resetting: self = .resetting
        case .unsupported: self = .unsupported
        case .unauthorized: self = .unauthorized
        case .poweredOff: self = .poweredOff
        case .poweredOn: self = .poweredOn
        @unknown default: self = .unknown
        }
    }
}

public enum YDKConnectionState: String {
    case disconnected, scanning, connecting, connected, discovering, disconnecting
}

// MARK: - 车辆广播信息

/// 扫描到的车辆广告（对应 sdk.proto VehicleAdvertise）
public struct VehicleAdvertise: Equatable {
    public let vehicleId: String
    public let rssi: Int
    public let protocolType: Int      // CCC / ICCOA / ICCE
    public let supportsUWB: Bool
    public let manufacturerData: Data?

    public init(vehicleId: String, rssi: Int, protocolType: Int, supportsUWB: Bool, manufacturerData: Data?) {
        self.vehicleId = vehicleId
        self.rssi = rssi
        self.protocolType = protocolType
        self.supportsUWB = supportsUWB
        self.manufacturerData = manufacturerData
    }
}

// MARK: - 连接结果

public struct ConnectResponse {
    public let success: Bool
    public let error: String?
    public init(success: Bool, error: String? = nil) {
        self.success = success
        self.error = error
    }
}

// MARK: - 本地控制结果

public struct LocalControlResponse {
    public let success: Bool
    public let error: String?
    public init(success: Bool, error: String? = nil) {
        self.success = success
        self.error = error
    }
}

// MARK: - 车辆状态

public struct VehicleStatus {
    public let locked: Bool
    public let engineOn: Bool
    public let batteryPct: Int32
    public let error: String?
    public init(locked: Bool, engineOn: Bool, batteryPct: Int32, error: String? = nil) {
        self.locked = locked
        self.engineOn = engineOn
        self.batteryPct = batteryPct
        self.error = error
    }
}

// MARK: - 命令结果

public struct CommandResult {
    public let success: Bool
    public let errorCode: Int32
    public let errorMessage: String?
    public init(success: Bool, errorCode: Int32 = 0, errorMessage: String? = nil) {
        self.success = success
        self.errorCode = errorCode
        self.errorMessage = errorMessage
    }
}

// MARK: - 会话上下文

/// BLE 安全会话上下文（由协议适配器维护）
public struct SessionContext {
    public let keyId: String
    public let vehicleId: String
    public var sessionHandle: UInt16 = 0
    public var counter: UInt32 = 0
    /// ICCE 用户 ID — control_command_t.user_id (BE u32), 由绑定/认证流程填充
    public var userId: UInt32 = 0
    /// ICCE 会话密钥 — SM4 取前 16 字节 (裁决 AD-7); HMAC-SHA256 取全长 (裁决 AD-6);
    /// nil 表示未协商 (仅调试/预认证阶段)
    public var sessionKey: Data? = nil
    /// ICCE 会话 IV (16 字节); nil 时回退全零 (仅调试, 裁决 AD-7)
    public var sessionIv: Data? = nil

    public init(keyId: String, vehicleId: String,
                sessionHandle: UInt16 = 0, counter: UInt32 = 0,
                userId: UInt32 = 0, sessionKey: Data? = nil, sessionIv: Data? = nil) {
        self.keyId = keyId
        self.vehicleId = vehicleId
        self.sessionHandle = sessionHandle
        self.counter = counter
        self.userId = userId
        self.sessionKey = sessionKey
        self.sessionIv = sessionIv
    }
}

// MARK: - BLE 协议类型

/// 数字钥匙 BLE 协议类型
public enum YDKBleProtocolType: Int, CaseIterable {
    case ccc = 1    // CCC Digital Key v4.0 (0xFFF5)
    case iccoa = 2  // ICCOA Digital Key (0xFEF5)
    case icce = 3   // ICCE Digital Key (0xFEFA)

    public var serviceUUID: CBUUID {
        switch self {
        case .ccc:   return CBUUID(string: "FFF5")
        case .iccoa: return CBUUID(string: "FEF5")
        case .icce:  return CBUUID(string: "FEFA")
        }
    }
}
