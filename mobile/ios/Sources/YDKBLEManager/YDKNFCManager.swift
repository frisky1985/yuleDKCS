import Foundation

// MARK: - NFC 备用解锁

/// NFC 车辆信息
public struct NFCVehicleInfo {
    public let vehicleId: String
    public let tagId: String
    public let protocolType: Int
    public init(vehicleId: String, tagId: String, protocolType: Int) {
        self.vehicleId = vehicleId
        self.tagId = tagId
        self.protocolType = protocolType
    }
}

/// NFC 指令类型
public enum NFCCommandType: UInt8 {
    case unlock = 0x01
    case lock = 0x02
    case startEngine = 0x03
}

/// NFC 管理器接口
///
/// 实现说明:
/// - iOS: CoreNFC (NDEF) — 读取车辆 NFC 标签; 钱包 NFC (Apple CarKey) 由钱包层负责
/// - Android: HCE (Host Card Emulation) + 标签读取
/// - 本期提供接口 + 说明，真实实现需硬件环境
public protocol YDKNFCManaging: AnyObject {
    /// 读取车辆 NFC 标签（手机没电/无网络时）
    func readVehicleTag() async throws -> NFCVehicleInfo
    /// 通过 NFC 通道发送指令
    func sendCommandViaNFC(command: NFCCommandType) async throws
}
