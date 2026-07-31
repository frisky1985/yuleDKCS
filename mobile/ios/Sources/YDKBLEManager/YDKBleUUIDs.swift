import Foundation
import CoreBluetooth

// MARK: - GATT UUID 注册表

/// 数字钥匙相关 BLE Service/Characteristic UUID — 多协议支持
///
/// 参考:
/// - CCC Digital Key v4.0 (Service 0xFFD1)
/// - ICCOA Digital Key 4.0 (Service 0xFEF5)
/// - ICCE T/CA 110-2020 (Service 0xFEFA)
public struct YDKBleUUIDs {

    // MARK: CCC (0xFFD1)
    public static let cccService        = CBUUID(string: "FFD1")
    public static let cccPairingChar    = CBUUID(string: "FFD2")
    public static let cccKeyDataChar    = CBUUID(string: "FFD3")
    public static let cccAuthChar       = CBUUID(string: "FFD4")
    public static let cccStateChar      = CBUUID(string: "FFD5")
    public static let cccUwbConfigChar  = CBUUID(string: "FFD6")
    public static let cccRssiChar       = CBUUID(string: "FFD7")

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
