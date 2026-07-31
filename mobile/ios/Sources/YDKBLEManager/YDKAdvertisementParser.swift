import Foundation
import CoreBluetooth

// MARK: - BLE 广播 AD 结构

/// 单个 BLE 广播 AD 结构 (Advertising Data element)
/// 线格式: [length(1)][type(1)][data(length-1)]
public struct YDKADStructure: Equatable {
    public let length: UInt8
    public let type: UInt8
    public let data: Data

    public init(length: UInt8, type: UInt8, data: Data) {
        self.length = length
        self.type = type
        self.data = data
    }
}

/// BLE 广播 AD type 常量 (Bluetooth Core Spec Vol 3 Part C §11)
public enum YDKADType {
    public static let flags: UInt8 = 0x01
    public static let incomplete16BitServiceUUIDs: UInt8 = 0x02
    public static let complete16BitServiceUUIDs: UInt8 = 0x03
    public static let incomplete32BitServiceUUIDs: UInt8 = 0x04
    public static let complete32BitServiceUUIDs: UInt8 = 0x05
    public static let incomplete128BitServiceUUIDs: UInt8 = 0x06
    public static let complete128BitServiceUUIDs: UInt8 = 0x07
    public static let shortLocalName: UInt8 = 0x08
    public static let completeLocalName: UInt8 = 0x09
    public static let serviceData16BitUUID: UInt8 = 0x16
    public static let serviceData128BitUUID: UInt8 = 0x21
    public static let manufacturerSpecific: UInt8 = 0xFF
}

/// 厂商自定义数据: [companyID(2, LE)][vendorData...]
public struct YDKManufacturerPayload: Equatable {
    public let companyID: UInt16
    public let vendorData: Data

    public init(companyID: UInt16, vendorData: Data) {
        self.companyID = companyID
        self.vendorData = vendorData
    }
}

/// 已知厂商 Company ID (Bluetooth SIG Assigned Numbers)
public enum YDKCompanyID {
    /// Apple, Inc. — CCC 车端低功耗广播使用 (见 ble_kw47a.c)
    public static let apple: UInt16 = 0x004C
}

// MARK: - 广告包解析器

/// BLE 广告包解析 — 纯逻辑、无状态、可单测 (构造字节 → vehicleId / protocol)
///
/// 参考:
/// - CCC 车辆广播: embedded/ccc_protocol/src/ble/ble_kw47a.c `ble_enter_lp_mode()`
/// - ICCE 车辆广播: embedded/icce_protocol/src/ble/ble_manager.c `ble_start_advertising()`
public enum YDKAdvertisementParser {

    // MARK: AD 结构迭代

    /// 将原始 scan record (广播数据字节) 拆分为 AD 结构序列。
    /// 容错: 遇到 0 长度 (AD 结束标记) 或越界/截断时停止, 不抛异常。
    public static func parseADStructures(from scanRecord: Data) -> [YDKADStructure] {
        var structures: [YDKADStructure] = []
        var index = 0
        let count = scanRecord.count
        while index < count {
            let length = Int(scanRecord[index])
            if length == 0 { break }                                  // AD 结束标记
            guard index + 1 + length <= count else { break }          // 截断的 AD
            let type = scanRecord[index + 1]
            let payload = scanRecord.subdata(in: (index + 2)..<(index + 1 + length))
            structures.append(YDKADStructure(length: UInt8(length), type: type, data: payload))
            index += 1 + length
        }
        return structures
    }

    // MARK: 厂商数据

    /// 解析 Manufacturer Specific Data: [companyID(2, BLE 小端)][vendorData...]
    public static func parseManufacturerData(_ data: Data) -> YDKManufacturerPayload? {
        guard data.count >= 2 else { return nil }
        let companyID = UInt16(data[0]) | (UInt16(data[1]) << 8)
        let vendorData = data.subdata(in: 2..<data.count)
        return YDKManufacturerPayload(companyID: companyID, vendorData: vendorData)
    }

    // MARK: CCC 车辆标识 (iBeacon 风格广播)

    /// 从 CCC 厂商数据中提取 vehicleId。
    ///
    /// 参考实现 (`ble_kw47a.c ble_enter_lp_mode`) 的低功耗广播为 iBeacon 风格:
    /// ```
    /// vendorData:
    ///   [0]       0x02  iBeacon subtype
    ///   [1]       0x15  iBeacon payload 长度 (21)
    ///   [2...22]  20 字节 proximity UUID — 由 keymgmt 模块按车辆填充 → vehicleId
    /// ```
    ///
    /// TODO-verify: 生产 CCC 车辆的 connectable 广播结构以 CCC-TS-101 §4.3 (Advertising) 为准;
    /// 当前解析基于仓库内车端参考实现 (LP 模式), 若规范使用其他 subtype/长度需同步调整。
    public static func cccVehicleID(from payload: YDKManufacturerPayload) -> String? {
        let vendor = payload.vendorData
        guard vendor.count >= 22 else { return nil }
        guard vendor[0] == 0x02 else { return nil }   // iBeacon subtype
        guard vendor[1] == 0x15 else { return nil }   // iBeacon payload 长度
        return uuidString(from: vendor.subdata(in: 2..<22))
    }

    /// 便捷入口: 直接从 manufacturer data 字节解析 CCC vehicleId
    public static func cccVehicleID(fromManufacturerData data: Data?) -> String? {
        guard let data = data, let mfr = parseManufacturerData(data) else { return nil }
        return cccVehicleID(from: mfr)
    }

    // MARK: ICCOA 车辆标识

    /// ICCOA 车辆标识提取。
    ///
    /// TODO-verify: ICCOA.DK.TS.002 BLE 广告章节未收录在仓库知识库, 车端参考实现
    /// (embedded/iccoa_protocol) 亦无广播数据结构定义; 在规范确认前不猜测编码, 返回 nil。
    /// 当前 ICCOA 车辆仅能通过 service UUID 0xFEF5 识别, 无法在广告中确认 vehicleId。
    public static func iccoaVehicleID(from payload: YDKManufacturerPayload?, localName: String?) -> String? {
        // TODO-verify: 待 ICCOA BLE 广告规范确认后实现
        return nil
    }

    // MARK: ICCE 车辆标识

    /// ICCE 车辆标识提取。
    ///
    /// 参考实现 (`embedded/icce_protocol/src/ble/ble_manager.c ble_start_advertising`)
    /// 广播仅含: Flags(0x01) + 16-bit Service UUIDs(0x03) + Local Name(0x09),
    /// 无 manufacturer data; 广播中唯一可作为标识的是设备名。
    ///
    /// TODO-verify: T/CA 110-2020 是否在设备名中编码 vehicleId (如 "DK-<id>" 前缀约定),
    /// 需规范原文确认; 当前将完整设备名作为 vehicleId 使用。
    public static func icceVehicleID(from payload: YDKManufacturerPayload?, localName: String?) -> String? {
        guard let name = localName, !name.isEmpty else { return nil }
        return name
    }

    // MARK: 从 CoreBluetooth 广告字典提取公共字段

    /// 从 CoreBluetooth 广告字典中提取公共字段 (service UUIDs / mfr data / 名称 / 可连接)
    public static func extract(from advertisementData: [String: Any])
        -> (serviceUUIDs: [CBUUID], manufacturerData: Data?, localName: String?, isConnectable: Bool) {
        let serviceUUIDs = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? []
        let manufacturerData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data
        let localName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let isConnectable = advertisementData[CBAdvertisementDataIsConnectable] as? Bool ?? false
        return (serviceUUIDs, manufacturerData, localName, isConnectable)
    }

    // MARK: 工具

    /// 16 字节 → UUID 字符串 (8-4-4-4-12, 大写)
    public static func uuidString(from data: Data) -> String? {
        guard data.count == 16 else { return nil }
        let hex = hexString(data).uppercased()
        let parts = [
            String(hex.prefix(8)),
            String(hex.dropFirst(8).prefix(4)),
            String(hex.dropFirst(12).prefix(4)),
            String(hex.dropFirst(16).prefix(4)),
            String(hex.dropFirst(20))
        ]
        return parts.joined(separator: "-")
    }

    /// 字节 → 小写 hex 字符串
    public static func hexString(_ data: Data) -> String {
        data.map { String(format: "%02x", $0) }.joined()
    }
}
