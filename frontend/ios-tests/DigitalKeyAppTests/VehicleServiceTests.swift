import XCTest
@testable import DigitalKeySDK

/// VehicleService 测试
///
/// 测试车辆控制操作的异步行为:
/// - 锁定/解锁
/// - 引擎控制
/// - 后备箱控制
/// - 空调控制
/// - 鸣笛闪灯
/// - 车辆定位
/// - 状态刷新
/// - 操作状态管理 (isOperating)
/// - 错误处理 (operationError)
final class VehicleServiceTests: XCTestCase {
    var vehicleService: VehicleService!

    override func setUp() {
        super.setUp()
        MockURLProtocol.reset()
        vehicleService = VehicleService.shared
        // 重置状态
        vehicleService.isOperating = false
        vehicleService.operationError = nil
    }

    override func tearDown() {
        MockURLProtocol.reset()
        vehicleService = nil
        super.tearDown()
    }

    // MARK: - Helper

    /// 等待异步完成
    private func waitForAsync(timeout: TimeInterval = 2.0) {
        let expectation = expectation(description: "wait")
        DispatchQueue.main.asyncAfter(deadline: .now() + timeout) {
            expectation.fulfill()
        }
        wait(for: [expectation], timeout: timeout + 0.5)
    }

    // MARK: - 解锁操作

    func testUnlockVehicleReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "unlock")
        var resultError: Error?

        // When
        vehicleService.unlockVehicle(vehicleId: "vehicle_001") { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    func testUnlockVehicleSetsOperatingState() throws {
        // Given
        XCTAssertFalse(vehicleService.isOperating)
        let expectation = self.expectation(description: "unlock operating")

        // When
        vehicleService.unlockVehicle(vehicleId: "vehicle_001") { _ in
            expectation.fulfill()
        }

        // 调用后立即检查
        XCTAssertTrue(vehicleService.isOperating, "操作进行中时 isOperating 应为 true")

        wait(for: [expectation], timeout: 2.0)

        // Then: 完成后应重置
        XCTAssertFalse(vehicleService.isOperating, "操作完成后 isOperating 应为 false")
    }

    // MARK: - 锁定操作

    func testLockVehicleReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "lock")
        var resultError: Error?

        // When
        vehicleService.lockVehicle(vehicleId: "vehicle_001") { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    // MARK: - 引擎控制

    func testStartEngineReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "start engine")
        var resultError: Error?

        // When
        vehicleService.startEngine(vehicleId: "vehicle_001") { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    func testStopEngineReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "stop engine")
        var resultError: Error?

        // When
        vehicleService.stopEngine(vehicleId: "vehicle_001") { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    // MARK: - 后备箱

    func testOpenTrunkReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "open trunk")
        var resultError: Error?

        // When
        vehicleService.openTrunk(vehicleId: "vehicle_001") { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    // MARK: - 空调控制

    func testStartClimateReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "start climate")
        var resultError: Error?

        // When: 开启空调，温度 24°C
        vehicleService.startClimate(vehicleId: "vehicle_001", temperature: 24.0) { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    func testStartClimateWithDifferentTemperatures() throws {
        // Given: 测试各种温度设置
        let temperatures: [Double] = [16.0, 22.5, 26.0, 30.0]

        for temp in temperatures {
            MockURLProtocol.reset()
            vehicleService.operationError = nil

            let expectation = self.expectation(description: "climate \(temp)")
            var resultError: Error?

            vehicleService.startClimate(vehicleId: "vehicle_001", temperature: temp) { result in
                switch result {
                case .success:
                    break
                case .failure(let error):
                    resultError = error
                }
                expectation.fulfill()
            }

            wait(for: [expectation], timeout: 2.0)
            XCTAssertNil(resultError, "温度 \(temp) 设置应成功")
        }
    }

    func testStopClimateReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "stop climate")
        var resultError: Error?

        // When
        vehicleService.stopClimate(vehicleId: "vehicle_001") { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    // MARK: - 鸣笛闪灯

    func testHonkAndFlashReturnsSuccess() throws {
        // Given
        let expectation = self.expectation(description: "honk and flash")
        var resultError: Error?

        // When
        vehicleService.honkAndFlash(vehicleId: "vehicle_001") { result in
            switch result {
            case .success:
                break
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
    }

    // MARK: - 车辆定位

    func testLocateVehicleReturnsLocation() throws {
        // Given
        let expectation = self.expectation(description: "locate vehicle")
        var resultLocation: Location?
        var resultError: Error?

        // When
        vehicleService.locateVehicle(vehicleId: "vehicle_001") { result in
            switch result {
            case .success(let location):
                resultLocation = location
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
        XCTAssertNotNil(resultLocation)
        XCTAssertEqual(resultLocation?.latitude, 31.2304, accuracy: 0.001)
        XCTAssertEqual(resultLocation?.longitude, 121.4737, accuracy: 0.001)
        XCTAssertEqual(resultLocation?.address, "上海市浦东新区")
    }

    // MARK: - 状态刷新

    func testRefreshVehicleStatusReturnsValidStatus() throws {
        // Given
        let expectation = self.expectation(description: "refresh status")
        var resultStatus: VehicleStatus?
        var resultError: Error?

        // When
        vehicleService.refreshVehicleStatus(vehicleId: "vehicle_001") { result in
            switch result {
            case .success(let status):
                resultStatus = status
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
        XCTAssertNotNil(resultStatus)
        XCTAssertTrue(resultStatus!.isOnline)
        XCTAssertGreaterThanOrEqual(resultStatus!.batteryLevel, 50)
        XCTAssertLessThanOrEqual(resultStatus!.batteryLevel, 100)
        XCTAssertNotNil(resultStatus!.fuelLevel)
        XCTAssertGreaterThan(resultStatus!.mileage, 0)
    }

    func testRefreshVehicleStatusSetsOperatingState() throws {
        // Given
        XCTAssertFalse(vehicleService.isOperating)
        let expectation = self.expectation(description: "refresh operating")

        // When
        vehicleService.refreshVehicleStatus(vehicleId: "vehicle_001") { _ in
            expectation.fulfill()
        }

        // 调用后立即检查
        XCTAssertTrue(vehicleService.isOperating)

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertFalse(vehicleService.isOperating)
    }

    // MARK: - 操作状态测试

    func testConsecutiveOperationsResetOperatingState() throws {
        // Given
        let operations: [(String, (String, @escaping (Result<Void, Error>) -> Void) -> Void)] = [
            ("unlock", { id, cb in self.vehicleService.unlockVehicle(vehicleId: id, completion: cb) }),
            ("lock", { id, cb in self.vehicleService.lockVehicle(vehicleId: id, completion: cb) }),
            ("start", { id, cb in self.vehicleService.startEngine(vehicleId: id, completion: cb) }),
            ("stop", { id, cb in self.vehicleService.stopEngine(vehicleId: id, completion: cb) }),
            ("trunk", { id, cb in self.vehicleService.openTrunk(vehicleId: id, completion: cb) }),
        ]

        for (name, operation) in operations {
            MockURLProtocol.reset()
            vehicleService.isOperating = false
            vehicleService.operationError = nil

            let expectation = self.expectation(description: name)
            operation("vehicle_001") { result in
                expectation.fulfill()
            }

            wait(for: [expectation], timeout: 2.0)

            // Then: 每次操作完成都应重置状态
            XCTAssertFalse(vehicleService.isOperating, "\(name) 完成后 isOperating 应为 false")
            XCTAssertNil(vehicleService.operationError, "\(name) 完成后 operationError 应为 nil")
        }
    }

    // MARK: - 多车辆支持

    func testOperationsWorkWithDifferentVehicleIDs() throws {
        let vehicleIDs = ["vehicle_001", "vehicle_002", "vehicle_003"]

        for vid in vehicleIDs {
            let expectation = self.expectation(description: "lock \(vid)")
            var resultError: Error?

            vehicleService.lockVehicle(vehicleId: vid) { result in
                switch result {
                case .success:
                    break
                case .failure(let error):
                    resultError = error
                }
                expectation.fulfill()
            }

            wait(for: [expectation], timeout: 2.0)
            XCTAssertNil(resultError, "车辆 \(vid) 操作应成功")
        }
    }

    // MARK: - 快速连续操作

    func testRapidSequentialOperations() throws {
        // Given: 快速连续执行多个操作
        let expectation1 = expectation(description: "unlock")
        let expectation2 = expectation(description: "start")
        let expectation3 = expectation(description: "lock")

        var results: [Bool] = []

        // When
        vehicleService.unlockVehicle(vehicleId: "vehicle_001") { result in
            if case .success = result { results.append(true) }
            expectation1.fulfill()
        }

        vehicleService.startEngine(vehicleId: "vehicle_001") { result in
            if case .success = result { results.append(true) }
            expectation2.fulfill()
        }

        vehicleService.lockVehicle(vehicleId: "vehicle_001") { result in
            if case .success = result { results.append(true) }
            expectation3.fulfill()
        }

        wait(for: [expectation1, expectation2, expectation3], timeout: 5.0)

        // Then: 所有操作都应成功
        XCTAssertEqual(results.count, 3)
        XCTAssertEqual(results.filter { $0 }.count, 3)
    }

    // MARK: - Singleton

    func testVehicleServiceIsSingleton() throws {
        let instance1 = VehicleService.shared
        let instance2 = VehicleService.shared
        XCTAssertTrue(instance1 === instance2, "VehicleService 应该是单例")
    }
}
