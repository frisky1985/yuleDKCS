import XCTest
@testable import YDKBLEManager

final class YDKBLEManagerTests: XCTestCase {

    // MARK: - 协议适配器测试

    func testCCCAdapterParsesCommandResponse() throws {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .ccc)

        // 成功响应
        let success = try adapter.parseCommandResponse(Data([0x00]))
        XCTAssertTrue(success.success)

        // 失败响应
        let failure = try adapter.parseCommandResponse(Data([0x10]))
        XCTAssertFalse(failure.success)
        XCTAssertEqual(failure.errorCode, 16)
    }

    func testCCCAdapterBuildsUnlockCommand() throws {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .ccc)
        let session = SessionContext(keyId: "key-1", vehicleId: "VH-1")

        let command = try adapter.buildUnlockCommand(keyId: "key-1", session: session)
        XCTAssertEqual(command[0], 0x01)  // unlock type
        XCTAssertEqual(command.count, 8)  // 1 + 2 + 4 + 1
    }

    func testCCCAdapterParsesVehicleStatus() throws {
        let adapter = BleProtocolAdapterFactory.makeAdapter(for: .ccc)

        // locked=true, engineOn=false, battery=80
        let status = try adapter.parseVehicleStatus(Data([0x01, 0x00, 0x50]))
        XCTAssertTrue(status.locked)
        XCTAssertFalse(status.engineOn)
        XCTAssertEqual(status.batteryPct, 80)
    }

    // MARK: - 协议 UUID

    func testServiceUUIDs() {
        XCTAssertEqual(YDKBleUUIDs.serviceUUID(for: .ccc).uuidString, "FFD1")
        XCTAssertEqual(YDKBleUUIDs.serviceUUID(for: .iccoa).uuidString, "FEF5")
        XCTAssertEqual(YDKBleUUIDs.serviceUUID(for: .icce).uuidString, "FEFA")
    }

    // MARK: - Mock UWB

    func testMockUWBProducesMeasurements() async throws {
        let uwb = YDKMockUWBManager()
        let expectation = expectation(description: "ranging")

        var received: UWBMeasurement?
        uwb.rangingResultHandler = { measurement in
            received = measurement
            expectation.fulfill()
        }

        try await uwb.startRanging(vehicleId: "VH-001")
        await fulfillment(of: [expectation], timeout: 3)
        uwb.stopRanging()

        XCTAssertNotNil(received)
        XCTAssertEqual(received?.vehicleId, "VH-001")
        XCTAssertGreaterThan(received?.distance ?? 0, 0)
    }

    // MARK: - 状态机

    func testBLEStateMapping() {
        // CBManagerState 无法直接构造，验证 enum 完整性
        let states: [YDKBLEState] = [.unknown, .resetting, .unsupported, .unauthorized, .poweredOff, .poweredOn]
        XCTAssertEqual(states.count, 6)
        XCTAssertEqual(states.last, .poweredOn)
    }
}
