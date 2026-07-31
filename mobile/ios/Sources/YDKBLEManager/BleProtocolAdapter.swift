import Foundation
import CoreBluetooth

// MARK: - 协议适配器协议

/// BLE 协议适配器 — 封装各协议（CCC/ICCOA/ICCE）的指令编解码
///
/// 每个协议一个实现，通过 `makeAdapter(for:)` 工厂方法创建。
public protocol BleProtocolAdapter: AnyObject {
    /// 协议类型
    var protocolType: YDKBleProtocolType { get }

    /// 解析广告包 → 车辆信息
    /// - Returns: nil 表示非本协议的广告包
    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise?

    /// 构建解锁指令
    func buildUnlockCommand(keyId: String, session: SessionContext) throws -> Data

    /// 构建锁车指令
    func buildLockCommand(keyId: String, session: SessionContext) throws -> Data

    /// 构建启动引擎指令
    func buildStartEngineCommand(keyId: String, session: SessionContext) throws -> Data

    /// 解析指令响应
    func parseCommandResponse(_ data: Data) throws -> CommandResult

    /// 读取车辆状态
    func parseVehicleStatus(_ data: Data) throws -> VehicleStatus
}

// MARK: - 工厂

public enum BleProtocolAdapterFactory {
    /// 创建协议适配器
    public static func makeAdapter(for type: YDKBleProtocolType) -> BleProtocolAdapter {
        switch type {
        case .ccc:   return CCCBleAdapter()
        case .iccoa: return ICCOABleAdapter()
        case .icce:  return ICCEBleAdapter()
        }
    }
}

// MARK: - 指令类型

/// BLE 控制指令类型
public enum BleCommandType: UInt8 {
    case unlock = 0x01
    case lock = 0x02
    case engineOn = 0x03
    case engineOff = 0x04
    case status = 0x05
}
