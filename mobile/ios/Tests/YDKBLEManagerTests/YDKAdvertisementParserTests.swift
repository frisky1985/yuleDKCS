import XCTest
import CoreBluetooth
@testable import YDKBLEManager

/// 2b-A: 广告包解析单测 — 构造字节 → vehicleId / 协议
final class YDKAdvertisementParserTests: XCTestCase {

    // MARK: AD 结构迭代

    func testParseADStructures() {
        // Flags(3) + Service UUIDs(5) + Local Name(6)
        var record = Data()
        record.append(contentsOf: [0x02, 0x01, 0x06])                    // flags
        record.append(contentsOf: [0x05, 0x03, 0xF5, 0xFF, 0xF5, 0xFE])  // 16-bit service UUIDs (FFF5, FEF5)
        record.append(contentsOf: [0x05, 0x09, 0x44, 0x4B, 0x2D, 0x31])  // "DK-1"

        let structures = YDKAdvertisementParser.parseADStructures(from: record)
        XCTAssertEqual(structures.count, 3)
        XCTAssertEqual(structures[0].type, YDKADType.flags)
        XCTAssertEqual(structures[0].data, Data([0x06]))
        XCTAssertEqual(structures[1].type, YDKADType.complete16BitServiceUUIDs)
        XCTAssertEqual(structures[1].data, Data([0xF5, 0xFF, 0xF5, 0xFE]))
        XCTAssertEqual(structures[2].type, YDKADType.completeLocalName)
        XCTAssertEqual(structures[2].data, Data([0x44, 0x4B, 0x2D, 0x31]))
    }

    func testParseADStructuresHandlesTruncationAndTerminator() {
        // 声明 0x10 长度但只有 2 字节数据 → 截断, 保留前面的结构
        var record = Data()
        record.append(contentsOf: [0x02, 0x01, 0x06])
        record.append(contentsOf: [0x10, 0xFF, 0x4C, 0x00])
        let structures = YDKAdvertisementParser.parseADStructures(from: record)
        XCTAssertEqual(structures.count, 1)
        XCTAssertEqual(structures[0].type, YDKADType.flags)

        // 0 长度 = 结束标记, 之后的数据忽略
        let withTerminator = Data([0x02, 0x01, 0x06, 0x00, 0x02, 0x01, 0x06])
        XCTAssertEqual(YDKAdvertisementParser.parseADStructures(from: withTerminator).count, 1)

        // 空数据
        XCTAssertEqual(YDKAdvertisementParser.parseADStructures(from: Data()).count, 0)
    }

    // MARK: 厂商数据解析

    func testParseManufacturerData() {
        let data = Data([0x4C, 0x00, 0x02, 0x15, 0xAA])
        let payload = YDKAdvertisementParser.parseManufacturerData(data)
        XCTAssertEqual(payload?.companyID, YDKCompanyID.apple)
        XCTAssertEqual(payload?.vendorData, Data([0x02, 0x15, 0xAA]))
    }

    func testParseManufacturerDataTooShort() {
        XCTAssertNil(YDKAdvertisementParser.parseManufacturerData(Data([0x4C])))
        XCTAssertNil(YDKAdvertisementParser.parseManufacturerData(Data()))
    }

    // MARK: CCC vehicleId (iBeacon 风格, 参考 ble_kw47a.c)

    func testCCCBeaconVehicleID() {
        var vendor = Data([0x02, 0x15])  // iBeacon subtype + 长度
        vendor.append(contentsOf: [0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
                                   0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xFF])
        let payload = YDKManufacturerPayload(companyID: YDKCompanyID.apple, vendorData: vendor)
        XCTAssertEqual(YDKAdvertisementParser.cccVehicleID(from: payload),
                       "E0E1E2E3-E4E5-E6E7-E8E9-EAEBECEDEEFF")
    }

    func testCCCBeaconRejectsInvalidLayout() {
        let uuidBytes = [UInt8](repeating: 0xAB, count: 16)
        // 错误的 company ID (非 Apple)
        XCTAssertNil(YDKAdvertisementParser.cccVehicleID(from:
            YDKManufacturerPayload(companyID: 0x1234, vendorData: Data([0x02, 0x15] + uuidBytes))))
        // 错误的 iBeacon subtype
        XCTAssertNil(YDKAdvertisementParser.cccVehicleID(from:
            YDKManufacturerPayload(companyID: YDKCompanyID.apple, vendorData: Data([0x03, 0x15] + uuidBytes))))
        // 错误的 iBeacon 长度字段
        XCTAssertNil(YDKAdvertisementParser.cccVehicleID(from:
            YDKManufacturerPayload(companyID: YDKCompanyID.apple, vendorData: Data([0x02, 0x14] + uuidBytes))))
        // 数据过短
        XCTAssertNil(YDKAdvertisementParser.cccVehicleID(from:
            YDKManufacturerPayload(companyID: YDKCompanyID.apple, vendorData: Data([0x02, 0x15]))))
    }

    func testCCCVehicleIDFromManufacturerDataBytes() {
        var mfr = Data([0x4C, 0x00, 0x02, 0x15])
        mfr.append(contentsOf: [0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
                                0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xFF])
        XCTAssertEqual(YDKAdvertisementParser.cccVehicleID(fromManufacturerData: mfr),
                       "E0E1E2E3-E4E5-E6E7-E8E9-EAEBECEDEEFF")
        XCTAssertNil(YDKAdvertisementParser.cccVehicleID(fromManufacturerData: nil))
    }

    // MARK: 完整广告解析 (adapter 入口)

    func testCCCBleAdapterParsesBeaconAdvertisement() {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .ccc)
        var mfr = Data([0x4C, 0x00, 0x02, 0x15])
        mfr.append(contentsOf: [0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
                                0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xFF])
        let advertisement: [String: Any] = [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FFF5")],
            CBAdvertisementDataManufacturerDataKey: mfr,
            CBAdvertisementDataIsConnectable: true
        ]

        let vehicle = adapter.parseAdvertisement(advertisement, rssi: -45)
        XCTAssertNotNil(vehicle)
        XCTAssertEqual(vehicle?.vehicleId, "E0E1E2E3-E4E5-E6E7-E8E9-EAEBECEDEEFF")
        XCTAssertEqual(vehicle?.protocolType, YDKBleProtocolType.ccc.rawValue)
        XCTAssertEqual(vehicle?.rssi, -45)
        XCTAssertEqual(vehicle?.manufacturerData, mfr)
    }

    func testCCCBleAdapterRejectsMissingService() {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .ccc)
        // 广播不包含 CCC Service (0xFFF5) → 拒绝
        let advertisement: [String: Any] = [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FEF5")],
            CBAdvertisementDataManufacturerDataKey: Data([0x4C, 0x00, 0x02, 0x15])
        ]
        XCTAssertNil(adapter.parseAdvertisement(advertisement, rssi: -45))
    }

    func testCCCBleAdapterRejectsUnparseableManufacturerData() {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .ccc)
        // 有 service UUID, 但 mfr data 不是 iBeacon 布局 → 不伪造 vehicleId, 返回 nil
        let advertisement: [String: Any] = [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FFF5")],
            CBAdvertisementDataManufacturerDataKey: Data([0x4C, 0x00, 0x99])
        ]
        XCTAssertNil(adapter.parseAdvertisement(advertisement, rssi: -45))
    }

    func testCCCBleAdapterRejectsEmptyAdvertisement() {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .ccc)
        XCTAssertNil(adapter.parseAdvertisement([:], rssi: -45))
    }

    // MARK: ICCE (设备名标识, 参考 icce ble_manager.c)

    func testICCEAdapterUsesLocalName() {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .icce)
        let advertisement: [String: Any] = [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FEFA")],
            CBAdvertisementDataLocalNameKey: "DK-ABC123"
        ]
        let vehicle = adapter.parseAdvertisement(advertisement, rssi: -50)
        XCTAssertNotNil(vehicle)
        XCTAssertEqual(vehicle?.vehicleId, "DK-ABC123")
        XCTAssertEqual(vehicle?.protocolType, YDKBleProtocolType.icce.rawValue)
    }

    func testICCEAdapterRejectsMissingLocalName() {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .icce)
        let advertisement: [String: Any] = [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FEFA")]
        ]
        XCTAssertNil(adapter.parseAdvertisement(advertisement, rssi: -50))
    }

    // MARK: ICCOA (规范未确认前不猜测)

    func testICCOAAdapterReturnsNilUntilSpecConfirmed() {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .iccoa)
        let advertisement: [String: Any] = [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FEF5")],
            CBAdvertisementDataLocalNameKey: "DK-ABC123"
        ]
        XCTAssertNil(adapter.parseAdvertisement(advertisement, rssi: -50))
    }

    // MARK: 工具函数

    func testUUIDStringFormatting() {
        let bytes: [UInt8] = [0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
                              0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xFF]
        XCTAssertEqual(YDKAdvertisementParser.uuidString(from: Data(bytes)),
                       "E0E1E2E3-E4E5-E6E7-E8E9-EAEBECEDEEFF")
        XCTAssertNil(YDKAdvertisementParser.uuidString(from: Data([0x01, 0x02])))
    }

    func testHexString() {
        XCTAssertEqual(YDKAdvertisementParser.hexString(Data([0x00, 0x0F, 0xAB])), "000fab")
    }
}
