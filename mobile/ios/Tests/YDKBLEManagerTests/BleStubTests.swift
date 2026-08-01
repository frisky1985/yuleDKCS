import XCTest
import CoreBluetooth
@testable import YDKBLEManager

// MARK: - 4.3 BLE 桩测试 (模拟车辆广播)
//
// Contract 映射 (docs/sdk/PHASE4-3-BLE-STUB-CONTRACT.md):
//   B2.1 → testScanFindsCCCStubSpecAdvertisement / testScanFindsLegacyBeaconCCC
//   B2.2 → testScanFiltersNonProtocolAdvertisement
//   B3.1 → testConnectDiscoversCCCServiceAndSPSM / testReadSPSMValue
//   B3.2 → testUnlockWritesSpecWireFrame
//   B4.1 → testUnlockWritesSpecWireFrame (wire 级帧断言)
//
// 桩注入点: YDKBLEManager.init(central:) (internal, @testable 可见)。
// 注意: FakeCentral 不模拟 OS 级 service 过滤, 所有预置广播都会触发
// onPeripheralDiscovered, 由 adapter 层 (handleDiscovered) 做二次过滤 —
// 这样才能真正验证 B2.2 的协议过滤逻辑。

// MARK: - 桩: GATT 特征

final class FakeCharacteristic: YDKCharacteristicManaging {
    let uuid: CBUUID
    var value: Data?

    init(uuid: CBUUID, value: Data? = nil) {
        self.uuid = uuid
        self.value = value
    }
}

// MARK: - 桩: GATT 服务

final class FakeService: YDKServiceManaging {
    let uuid: CBUUID
    var characteristics: [YDKCharacteristicManaging]?

    init(uuid: CBUUID, characteristics: [YDKCharacteristicManaging]) {
        self.uuid = uuid
        self.characteristics = characteristics
    }
}

// MARK: - 桩: 外设 (B 部分)

/// 模拟车辆外设: 预置 GATT 结构 (0xFFF5 服务 + SPSM/控制特征),
/// 记录 writeValue/readValue 调用, 支持同步回调 + 延迟指令响应。
final class FakePeripheral: YDKPeripheralManaging {
    let identifier: UUID
    let name: String?
    var services: [YDKServiceManaging]?

    var onServicesDiscovered: ((Error?) -> Void)?
    var onCharacteristicsDiscovered: ((Error?) -> Void)?
    var onWriteCompleted: ((Error?) -> Void)?
    var onValueUpdated: ((Data?, Error?) -> Void)?

    /// 记录 writeValue 调用 (B3.2 断言用)
    private(set) var writtenData: [Data] = []
    private(set) var writeTypes: [CBCharacteristicWriteType] = []
    /// 记录 readValue 调用
    private(set) var readCharacteristics: [CBUUID] = []

    /// true: discoverServices/discoverCharacteristics/writeValue 同步触发回调
    /// (模拟真实 GATT 立即完成); false: 需手动触发 (注入失败路径用)
    let autoCallback: Bool

    /// 写指令后延迟推送的 notify 响应帧 (模拟车辆对控制指令的响应)
    var autoResponse: (data: Data, delay: TimeInterval)?

    init(identifier: UUID = UUID(),
         name: String? = nil,
         services: [YDKServiceManaging] = [],
         autoCallback: Bool = true) {
        self.identifier = identifier
        self.name = name
        self.services = services
        self.autoCallback = autoCallback
    }

    func discoverServices(_ serviceUUIDs: [CBUUID]?) {
        guard autoCallback else { return }
        onServicesDiscovered?(nil)
    }

    func discoverCharacteristics(_ characteristicUUIDs: [CBUUID]?, for service: YDKServiceManaging) {
        guard autoCallback else { return }
        onCharacteristicsDiscovered?(nil)
    }

    func writeValue(_ data: Data, for characteristic: YDKCharacteristicManaging, type: CBCharacteristicWriteType) {
        writtenData.append(data)
        writeTypes.append(type)
        if autoCallback {
            onWriteCompleted?(nil)
        }
        // 车辆指令响应: 写完成后再延迟 notify (响应必须晚于 waitForResponse 挂起)
        if let autoResponse = autoResponse {
            DispatchQueue.main.asyncAfter(deadline: .now() + autoResponse.delay) { [weak self] in
                self?.onValueUpdated?(autoResponse.data, nil)
            }
        }
    }

    func readValue(for characteristic: YDKCharacteristicManaging) {
        readCharacteristics.append(characteristic.uuid)
        if autoCallback {
            onValueUpdated?(characteristic.value, nil)
        }
    }
}

// MARK: - 桩: 中央管理器 (A 部分)

/// 模拟 CBCentralManager: 注入预置外设列表与广播字典,
/// scanForPeripherals 时同步推送 onPeripheralDiscovered, connect 时同步回调。
final class FakeCentral: YDKCentralManaging {
    var state: CBManagerState = .unknown
    var isScanning = false

    var onStateChange: ((CBManagerState) -> Void)?
    var onPeripheralDiscovered: ((YDKPeripheralManaging, [String: Any], Int) -> Void)?
    var onPeripheralConnected: ((YDKPeripheralManaging) -> Void)?
    var onPeripheralFailedToConnect: ((YDKPeripheralManaging, Error?) -> Void)?
    var onPeripheralDisconnected: ((YDKPeripheralManaging, Error?) -> Void)?
    var onRestoreState: (([YDKPeripheralManaging]) -> Void)?

    /// 预置外设: (外设, 广播字典, rssi)
    private(set) var presets: [(peripheral: YDKPeripheralManaging, advertisement: [String: Any], rssi: Int)] = []
    /// 记录调用
    private(set) var connectedPeripherals: [YDKPeripheralManaging] = []
    private(set) var cancelledPeripherals: [YDKPeripheralManaging] = []
    private(set) var scannedServices: [CBUUID]?
    /// 记录最近一次 connect 的 options (2b-I B1.4 断言连接唤醒选项用)
    private(set) var connectOptions: [String: Any]?
    /// retrieveConnectedPeripherals 返回的系统已连接外设 (2b-I B1.5 回退路径用)
    var connectedSystemPeripherals: [YDKPeripheralManaging] = []

    /// 连接失败注入 (nil = 连接成功)
    var connectError: Error?

    /// 模拟 centralManagerDidUpdateState
    func simulateStateChange(_ newState: CBManagerState) {
        state = newState
        onStateChange?(newState)
    }

    /// 模拟 centralManager(_:willRestoreState:) — 驱动 onRestoreState 回调 (2b-I B1.3)
    func simulateRestoreState(_ peripherals: [YDKPeripheralManaging]) {
        onRestoreState?(peripherals)
    }

    /// 注入预置外设 (车辆广播桩)
    func preset(peripheral: YDKPeripheralManaging, advertisement: [String: Any], rssi: Int = -50) {
        presets.append((peripheral, advertisement, rssi))
    }

    func scanForPeripherals(withServices serviceUUIDs: [CBUUID]?, options: [String: Any]?) {
        isScanning = true
        scannedServices = serviceUUIDs
        // 同步推送全部预置广播 (不模拟 OS 级过滤, 见文件头注释)
        for (peripheral, advertisement, rssi) in presets {
            onPeripheralDiscovered?(peripheral, advertisement, rssi)
        }
    }

    func stopScan() {
        isScanning = false
    }

    func connect(_ peripheral: YDKPeripheralManaging, options: [String: Any]?) {
        connectedPeripherals.append(peripheral)
        connectOptions = options
        if let error = connectError {
            onPeripheralFailedToConnect?(peripheral, error)
        } else {
            onPeripheralConnected?(peripheral)
        }
    }

    func cancelPeripheralConnection(_ peripheral: YDKPeripheralManaging) {
        cancelledPeripherals.append(peripheral)
    }

    func retrieveConnectedPeripherals(withServices serviceUUIDs: [CBUUID]) -> [YDKPeripheralManaging] {
        connectedSystemPeripherals
    }
}

// MARK: - CCC 桩广播构造 (三种样本)

enum CCCStubAdvertisements {

    /// 规范 Owner Pairing Advertising (CCC-TS-101 v4.0.0 §19.2.1.3, Table 19-2):
    /// AD1: Length=0x03, Type=0x03 (16-bit Service UUID), Data=0xFFF5 (CCC_DK_UUID)
    /// AD2: Length=0x14, Type=0x21 (Service Data 128bit UUID),
    ///      Data[0..16]=CCCServiceDataIntent UUID, Data[16]=IntentConfiguration,
    ///      Data[17..19]=Vehicle Brand Identifier (2B)
    /// 注: 按 SDK 现有约定 (CCCSecureChannelTests.testServiceDataParsing),
    ///     0x21 AD 结构字节经 CBAdvertisementDataManufacturerDataKey 传递。
    static func specServiceData(brand: [UInt8] = [0x00, 0x2A], intentConfig: UInt8 = 0x01) -> [String: Any] {
        var ad = Data()
        ad.append(0x03); ad.append(0x03)                    // AD1: len=3, type=0x03
        ad.append(0xF5); ad.append(0xFF)                    //      0xFFF5 (LE)
        ad.append(0x14); ad.append(0x21)                    // AD2: len=0x14, type=0x21
        ad.append(contentsOf: YDKAdvertisementParser.cccServiceDataIntentUUIDBytes)
        ad.append(intentConfig)
        ad.append(contentsOf: brand)
        return [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FFF5")],
            CBAdvertisementDataManufacturerDataKey: ad,
            CBAdvertisementDataIsConnectable: true
        ]
    }

    /// R3.0 兼容回退: iBeacon 风格广播 (Apple 0x004C + 0x02 0x15 + 20B proximity UUID)
    static func legacyBeacon(uuid: [UInt8]) -> [String: Any] {
        var mfr = Data([0x4C, 0x00, 0x02, 0x15])
        mfr.append(contentsOf: uuid)
        return [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FFF5")],
            CBAdvertisementDataManufacturerDataKey: mfr,
            CBAdvertisementDataIsConnectable: true
        ]
    }

    /// 非本协议广播: 无 0xFFF5 (仅 ICCOA 0xFEF5, ICCOA 适配器当前恒返回 nil)
    static func nonCCC() -> [String: Any] {
        [
            CBAdvertisementDataServiceUUIDsKey: [CBUUID(string: "FEF5")],
            CBAdvertisementDataLocalNameKey: "DK-OTHER"
        ]
    }
}

// MARK: - B2 扫描发现 (桩)

final class BleStubTests: XCTestCase {

    /// B2.1: FakeCentral 注入 → scanVehicles 发现 CCC 桩广播 (规范 0x21 Service Data)
    /// → vehicleId = "ccc-" + brand identifier hex
    @MainActor
    func testScanFindsCCCStubSpecAdvertisement() async throws {
        let central = FakeCentral()
        let peripheral = FakePeripheral(name: "CCC-Stub")
        let ad = CCCStubAdvertisements.specServiceData(brand: [0x00, 0x2A])
        central.preset(peripheral: peripheral, advertisement: ad, rssi: -55)

        let manager = YDKBLEManager(central: central, enableLogging: false)
        central.simulateStateChange(.poweredOn)  // 必须晚于 init (onStateChange 在 init 时接线)
        let results = try await manager.scanVehicles(timeout: 0.2)

        XCTAssertEqual(results.count, 1)
        guard let vehicle = results.first else {
            XCTFail("CCC 桩广播未被发现")
            return
        }
        XCTAssertEqual(vehicle.vehicleId, "ccc-002a")   // brand = 0x002A
        XCTAssertEqual(vehicle.protocolType, YDKBleProtocolType.ccc.rawValue)
        XCTAssertEqual(vehicle.rssi, -55)
        XCTAssertEqual(vehicle.manufacturerData, ad[CBAdvertisementDataManufacturerDataKey] as? Data)
    }

    /// B2.1 (R3.0 兼容回退): iBeacon 风格广播经 manager 全链路 → vehicleId = proximity UUID
    @MainActor
    func testScanFindsLegacyBeaconCCC() async throws {
        let central = FakeCentral()
        let uuid: [UInt8] = [0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
                             0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xFF]
        central.preset(peripheral: FakePeripheral(name: "CCC-Legacy"),
                       advertisement: CCCStubAdvertisements.legacyBeacon(uuid: uuid),
                       rssi: -70)

        let manager = YDKBLEManager(central: central, enableLogging: false)
        central.simulateStateChange(.poweredOn)
        let results = try await manager.scanVehicles(timeout: 0.2)

        XCTAssertEqual(results.count, 1)
        XCTAssertEqual(results.first?.vehicleId, "E0E1E2E3-E4E5-E6E7-E8E9-EAEBECEDEEFF")
        XCTAssertEqual(results.first?.protocolType, YDKBleProtocolType.ccc.rawValue)
    }

    /// B2.2: 无 0xFFF5 的广播 → 三个协议适配器均不识别 → 过滤不返回
    @MainActor
    func testScanFiltersNonProtocolAdvertisement() async throws {
        let central = FakeCentral()
        central.preset(peripheral: FakePeripheral(name: "ICCOA-Stub"),
                       advertisement: CCCStubAdvertisements.nonCCC(),
                       rssi: -60)

        let manager = YDKBLEManager(central: central, enableLogging: false)
        central.simulateStateChange(.poweredOn)
        let results = try await manager.scanVehicles(timeout: 0.2)

        XCTAssertTrue(results.isEmpty, "非本协议广播不应被发现")
    }

    // MARK: B3 连接 + GATT 流程 (桩)

    /// B3.1: 连接 → 0xFFF5 服务发现 → 特征发现 → connected;
    /// 桩外设上可发现 SPSM 特征 (D3B5A130-...), 值 = 0x0080 大端
    @MainActor
    func testConnectDiscoversCCCServiceAndSPSM() async throws {
        let central = FakeCentral()
        let spsm = FakeCharacteristic(uuid: YDKBleUUIDs.cccSpsm, value: Data([0x00, 0x80])) // SPSM 0x0080 (BE)
        let control = FakeCharacteristic(uuid: YDKBleUUIDs.cccAuthChar)
        let service = FakeService(uuid: YDKBleUUIDs.cccService, characteristics: [spsm, control])
        let peripheral = FakePeripheral(name: "CCC-Stub", services: [service])
        central.preset(peripheral: peripheral, advertisement: CCCStubAdvertisements.specServiceData(), rssi: -50)

        let manager = YDKBLEManager(central: central, enableLogging: false)
        central.simulateStateChange(.poweredOn)
        _ = try await manager.scanVehicles(timeout: 0.2)
        try await manager.connectVehicle(vehicleId: "ccc-002a")

        XCTAssertEqual(manager.connectionState, .connected)
        XCTAssertEqual(central.connectedPeripherals.count, 1)
        XCTAssertTrue(central.connectedPeripherals[0] as AnyObject === peripheral as AnyObject)

        // 0xFFF5 服务 + SPSM 特征可从桩外设读取
        let cccService = peripheral.services?.first { $0.uuid == YDKBleUUIDs.cccService }
        XCTAssertNotNil(cccService)
        let spsmChar = cccService?.characteristics?.first { $0.uuid == YDKBleUUIDs.cccSpsm }
        XCTAssertNotNil(spsmChar)
        XCTAssertEqual(spsmChar?.value, Data([0x00, 0x80]) as Data?)
    }

    /// B3.1: SPSM 读取模拟 — readValue 经 onValueUpdated 回传预置值
    @MainActor
    func testReadSPSMValue() {
        let spsm = FakeCharacteristic(uuid: YDKBleUUIDs.cccSpsm, value: Data([0x00, 0x80]))
        let service = FakeService(uuid: YDKBleUUIDs.cccService, characteristics: [spsm])
        let peripheral = FakePeripheral(name: "CCC-Stub", services: [service])

        var received: Data?
        peripheral.onValueUpdated = { value, _ in received = value }
        peripheral.readValue(for: spsm)

        XCTAssertEqual(peripheral.readCharacteristics, [YDKBleUUIDs.cccSpsm])
        XCTAssertEqual(received, Data([0x00, 0x80]) as Data?)
    }

    // MARK: B3.2 + B4.1 指令构建 wire 级 (对照规范)

    /// B3.2/B4.1: 写 unlock 指令 → FakePeripheral 收到规范帧:
    ///   [0]=0x01 (SE 消息)  [1]=0x0B (DK_APDU_RQ)  [2-3]=length(BE)=payload 长度
    /// 响应: 车辆 notify 回 SE + DK_APDU_RS + SW=0x9000 → unlock 成功
    @MainActor
    func testUnlockWritesSpecWireFrame() async throws {
        let central = FakeCentral()
        let control = FakeCharacteristic(uuid: YDKBleUUIDs.cccAuthChar) // R3.0 控制通道特征
        let service = FakeService(uuid: YDKBleUUIDs.cccService, characteristics: [control])
        let peripheral = FakePeripheral(name: "CCC-Stub", services: [service])
        peripheral.autoResponse = (data: Data([0x01, 0x0C, 0x00, 0x02, 0x90, 0x00]), delay: 0.05)
        central.preset(peripheral: peripheral, advertisement: CCCStubAdvertisements.specServiceData(), rssi: -50)

        let manager = YDKBLEManager(central: central, enableLogging: false)
        central.simulateStateChange(.poweredOn)
        _ = try await manager.scanVehicles(timeout: 0.2)
        try await manager.connectVehicle(vehicleId: "ccc-002a")
        try await manager.unlock(vehicleId: "ccc-002a")

        // 车辆收到 1 帧, withResponse 写入
        XCTAssertEqual(peripheral.writtenData.count, 1)
        XCTAssertEqual(peripheral.writeTypes, [.withResponse])

        guard let frame = peripheral.writtenData.first else {
            XCTFail("FakePeripheral 未收到写入帧")
            return
        }

        // wire 格式 (CCC-TS-101 Table 19-19): SE 消息 + DK_APDU_RQ + 4B 帧头
        XCTAssertEqual(frame.count, CCCCommandFrame.headerLength + 8 + "local-key".count)
        XCTAssertEqual(frame[0], CCCMessageType.se.rawValue)                    // 0x01 SE 消息
        XCTAssertEqual(frame[1], CCCApduMessageID.dkApduRq.rawValue)            // 0x0B DK_APDU_RQ
        let declaredLength = (Int(frame[2]) << 8) | Int(frame[3])
        XCTAssertEqual(declaredLength, frame.count - CCCCommandFrame.headerLength)

        // 透传安全提供者: 载荷可解析回明文控制指令
        let parsed = try CCCCommandFrame.parse(frame)
        XCTAssertEqual(parsed.messageType, CCCMessageType.se.rawValue)
        XCTAssertEqual(parsed.messageID, CCCApduMessageID.dkApduRq.rawValue)
        let payload = CCCControlPayload.parse(parsed.payload)
        XCTAssertEqual(payload?.subcommand, BleCommandType.unlock.rawValue)     // 0x01 unlock
        XCTAssertEqual(payload?.keyId, "local-key")                             // manager 会话默认 keyId
    }
}
