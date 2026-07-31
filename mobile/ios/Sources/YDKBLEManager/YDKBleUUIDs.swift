import Foundation
import CoreBluetooth

// MARK: - GATT UUID 注册表

/// 数字钥匙相关 BLE Service/Characteristic UUID — 多协议支持
///
/// 参考:
/// - CCC Digital Key v4.0 (Service 0xFFF5, CCC-TS-101 Table 19-6)
/// - ICCOA Digital Key 4.0 (Service 0xFEF5)
/// - ICCE T/CA 110-2020 (Service 0xFEFA)
public struct YDKBleUUIDs {

    // MARK: CCC (0xFFF5, CCC-TS-101 v4.0.0 Table 19-6)
    /// CCC_DK_UUID — DK Service (primary, 仅一个实例)
    public static let cccService        = CBUUID(string: "FFF5")
    /// CCCServiceDataIntent UUID — 广告 Service Data (Table 19-2)
    public static let cccServiceDataIntent = CBUUID(string: "5810BBC0-B499-11E9-A2A3-2A2AE2DBCCE4")
    /// UUID_SPSM — 读取 DK Service SPSM (Table 19-7/8)
    public static let cccSpsm           = CBUUID(string: "D3B5A130-9E23-4B3A-8BE4-6B1EE5F980A3")
    /// UUID_SPSM_DK_VERSION — 车辆 SPSM+版本 (Table 19-9/10)
    public static let cccSpsmDkVersion  = CBUUID(string: "AE285B91-6D23-23F1-CA12-6B1EE5B780A3")
    /// UUID_DEVICE_DK_VERSION — 设备所选版本 (Table 19-11/12)
    public static let cccDeviceDkVersion = CBUUID(string: "BD4B9502-3F54-11EC-B919-0242AC120005")

    // MARK: CCC R3.0 兼容特征 (FFD2-FFD7, 参考实现 ble_kw47a.c)
    // 2b-E 按 CCC-TS-101 v4.0.0 Table 19-6..19-12 重写 CCC UUID 注册表时删除了
    // R3.0 特征常量, 但 YDKBLEManager 的特征匹配仍引用它们 — 此处恢复以修复编译。
    // ⚠️ v4.0.0 控制通道为 SPSM (L2CAP CoC, 见 cccSpsm), 特征直写仅用于 R3.0 兼容。
    public static let cccPairingChar   = CBUUID(string: "FFD2")
    public static let cccKeyDataChar   = CBUUID(string: "FFD3")
    public static let cccAuthChar      = CBUUID(string: "FFD4")
    public static let cccStateChar     = CBUUID(string: "FFD5")
    public static let cccUwbConfigChar = CBUUID(string: "FFD6")
    public static let cccRssiChar      = CBUUID(string: "FFD7")

    // MARK: ICCE (0xFEFA, T/CA 110-2020)
    public static let icceService          = CBUUID(string: "FEFA")
    public static let icceKeyStatusChar    = CBUUID(string: "FEFB")
    public static let icceRangingDataChar  = CBUUID(string: "FEFC")
    public static let icceAuthChallengeChar = CBUUID(string: "FEFD")
    public static let icceControlCmdChar   = CBUUID(string: "FEFE")
    public static let icceSessionKeyChar   = CBUUID(string: "FEFF")

    // MARK: ICCOA (0xFEF5)
    public static let iccoaService = CBUUID(string: "FEF5")

    // MARK: Standard
    public static let deviceInfoService = CBUUID(string: "180A")
    public static let manufacturerName  = CBUUID(string: "2A00")
    public static let modelNumber       = CBUUID(string: "2A24")
    public static let firmwareRevision  = CBUUID(string: "2A26")

    /// 根据协议类型获取对应的 Service UUID
    public static func serviceUUID(for type: YDKBleProtocolType) -> CBUUID {
        type.serviceUUID
    }
}
