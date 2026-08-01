import XCTest
import CoreBluetooth
@testable import YDKBLEManager

// MARK: - 2b-I 后台 BLE 测试
//
// Contract 映射 (docs/sdk/PHASE2B-I-BACKGROUND-CONTRACT.md):
//   B1.1 → testProductionInitPassesRestoreIdentifierToCentral / testCentralOptionsHelper
//   B1.2 → testProductionInitPassesRestoreIdentifierToCentral (工厂捕获 options 断言)
//   B1.3 → testRestoreCallbackDeliversPeripheralsAndReconnects (fake 驱动 onRestoreState)
//   B1.4 → testConnectUsesNotifyWakeOptions
//   B1.5 → testRestoreCallbackDeliversPeripheralsAndReconnects / testRetrieveConnectedPeripheralsFallbackConnects
//
// 平台事实 (Apple 官方):
//   1. 后台 BLE = Info.plist UIBackgroundModes [bluetooth-central] + CBCentralManager
//      state restoration (CBCentralManagerOptionRestoreIdentifierKey)
//   2. willRestoreState 恢复的外设位于 dict[CBCentralManagerRestoredStatePeripheralsKey]
//   3. 连接唤醒 = CBConnectPeripheralOptionNotifyOnConnectionKey / NotifyOnDisconnectionKey
//   4. 后台扫描 AllowDuplicates 无效 — 保持 false 不变 (见 YDKBLEManager.scanVehicles)

final class BackgroundTests: XCTestCase {

    // MARK: B1.1/B1.2 — restore identifier 传递

    /// B1.1: options 构造 helper — 非空 identifier → 含 RestoreIdentifierKey; nil/空 → nil
    func testCentralOptionsHelper() {
        XCTAssertNil(YDKBLEManager.centralOptions(backgroundRestoreIdentifier: nil))
        XCTAssertNil(YDKBLEManager.centralOptions(backgroundRestoreIdentifier: ""))
        let options = YDKBLEManager.centralOptions(backgroundRestoreIdentifier: "com.yuledkcs.sdk.ble.restore")
        XCTAssertEqual(options?[CBCentralManagerOptionRestoreIdentifierKey] as? String,
                       "com.yuledkcs.sdk.ble.restore")
    }

    /// B1.1/B1.2 (AC-1): 生产 init 传 backgroundRestoreIdentifier 时,
    /// central 工厂收到的 options 必须含 RestoreIdentifierKey
    func testProductionInitPassesRestoreIdentifierToCentral() {
        var capturedOptions: [String: Any]?
        let central = FakeCentral()
        let manager = YDKBLEManager(centralFactory: { options in
            capturedOptions = options
            return central
        }, enableLogging: false, backgroundRestoreIdentifier: "com.yuledkcs.sdk.ble.restore")

        XCTAssertNotNil(manager)
        XCTAssertNotNil(capturedOptions, "非 nil restore identifier 时必须传 options")
        XCTAssertEqual(capturedOptions?[CBCentralManagerOptionRestoreIdentifierKey] as? String,
                       "com.yuledkcs.sdk.ble.restore")
    }

    /// AC-9 (不回归): 不传 backgroundRestoreIdentifier 时 options 为 nil, 与旧行为一致
    func testProductionInitWithoutIdentifierPassesNilOptions() {
        var capturedOptions: [String: Any]? = ["sentinel": true]
        let central = FakeCentral()
        let manager = YDKBLEManager(centralFactory: { options in
            capturedOptions = options
            return central
        }, enableLogging: false, backgroundRestoreIdentifier: nil)

        XCTAssertNotNil(manager)
        XCTAssertNil(capturedOptions, "默认 init 不应启用 state restoration")
    }

    // MARK: B1.3/B1.5 — willRestoreState 回调链 + 重连路径

    /// B1.3 + B1.5 (AC-2): fake 触发 restore → manager 记录恢复外设、复位连接状态 →
    /// connectVehicle 可按 name 命中恢复外设完成重连
    @MainActor
    func testRestoreCallbackDeliversPeripheralsAndReconnects() async throws {
        let central = FakeCentral()
        let control = FakeCharacteristic(uuid: YDKBleUUIDs.cccAuthChar)
        let service = FakeService(uuid: YDKBleUUIDs.cccService, characteristics: [control])
        let restoredCar = FakePeripheral(name: "RESTORED-CAR", services: [service])
        let otherCar = FakePeripheral(name: "OTHER-CAR")

        var handlerValues: [Int32] = []
        let manager = YDKBLEManager(central: central, enableLogging: false)
        manager.connectionChangeHandler = { handlerValues.append($0) }

        // 系统恢复: 被杀前已连接/扫描中的外设经 onRestoreState 交回
        central.simulateRestoreState([restoredCar, otherCar])
        XCTAssertEqual(manager.connectionState, .disconnected, "恢复后应复位为 disconnected")
        XCTAssertEqual(handlerValues.last, 0, "恢复后应通知连接状态复位")

        // 恢复 ≠ 已连接: connectVehicle 显式重连 (peripheralByVehicleId 按 name/identifier 命中)
        central.simulateStateChange(.poweredOn)
        try await manager.connectVehicle(vehicleId: "RESTORED-CAR")

        XCTAssertEqual(central.connectedPeripherals.count, 1)
        XCTAssertTrue(central.connectedPeripherals[0] as AnyObject === restoredCar as AnyObject,
                      "应重连到恢复的外设而非其他外设")
        XCTAssertEqual(manager.connectionState, .connected)
    }

    /// B1.5: 恢复场景的 retrieveConnectedPeripherals 回退 — 系统已连接外设
    /// (未在扫描结果中) 可按 identifier/name 匹配重连 (已有代码路径, 补测试)
    @MainActor
    func testRetrieveConnectedPeripheralsFallbackConnects() async throws {
        let central = FakeCentral()
        let control = FakeCharacteristic(uuid: YDKBleUUIDs.cccAuthChar)
        let service = FakeService(uuid: YDKBleUUIDs.cccService, characteristics: [control])
        let systemConnected = FakePeripheral(name: "SYSTEM-CONNECTED-CAR", services: [service])
        central.connectedSystemPeripherals = [systemConnected]   // retrieveConnectedPeripherals 返回

        let manager = YDKBLEManager(central: central, enableLogging: false)
        central.simulateStateChange(.poweredOn)
        try await manager.connectVehicle(vehicleId: "SYSTEM-CONNECTED-CAR")

        XCTAssertEqual(central.connectedPeripherals.count, 1)
        XCTAssertTrue(central.connectedPeripherals[0] as AnyObject === systemConnected as AnyObject)
        XCTAssertEqual(manager.connectionState, .connected)
    }

    // MARK: B1.4 — 连接唤醒选项

    /// B1.4 (AC-3): connect 时 central 收到的 options 必须含 NotifyOnConnection/Disconnection
    @MainActor
    func testConnectUsesNotifyWakeOptions() async throws {
        let central = FakeCentral()
        let control = FakeCharacteristic(uuid: YDKBleUUIDs.cccAuthChar)
        let service = FakeService(uuid: YDKBleUUIDs.cccService, characteristics: [control])
        let peripheral = FakePeripheral(name: "CCC-Stub", services: [service])
        central.preset(peripheral: peripheral, advertisement: CCCStubAdvertisements.specServiceData(), rssi: -50)

        let manager = YDKBLEManager(central: central, enableLogging: false)
        central.simulateStateChange(.poweredOn)
        _ = try await manager.scanVehicles(timeout: 0.2)
        try await manager.connectVehicle(vehicleId: "ccc-002a")

        let options = central.connectOptions
        XCTAssertEqual(options?[CBConnectPeripheralOptionNotifyOnConnectionKey] as? Bool, true,
                       "连接时应携带 NotifyOnConnectionKey (后台唤醒)")
        XCTAssertEqual(options?[CBConnectPeripheralOptionNotifyOnDisconnectionKey] as? Bool, true,
                       "连接时应携带 NotifyOnDisconnectionKey (后台唤醒)")
    }
}
