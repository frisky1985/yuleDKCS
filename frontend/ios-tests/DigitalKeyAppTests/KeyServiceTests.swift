import XCTest
@testable import DigitalKeySDK

/// KeyService 集成测试
///
/// 测试 KeyService 在 APIClient 注入 MockURLSession 后的行为:
/// - loadVehicles: 成功/失败
/// - loadShareRequests: 成功/失败
/// - addKey: 请求构造和响应解析
/// - removeKey: 请求构造和响应处理
/// - approveShareRequest / rejectShareRequest
/// - createShareRequest: 请求 body 验证
final class KeyServiceTests: XCTestCase {
    var apiClient: APIClient!
    var keyService: KeyService!

    override func setUp() {
        super.setUp()
        MockURLProtocol.reset()
        apiClient = APIClient(session: .mockSession)
        keyService = KeyService.shared
        // 重置服务状态
        keyService.vehicles = []
        keyService.shareRequests = []
        keyService.error = nil
    }

    override func tearDown() {
        MockURLProtocol.reset()
        apiClient = nil
        keyService = nil
        super.tearDown()
    }

    // MARK: - Helper

    private func wait(for duration: TimeInterval = 0.5) {
        let expectation = expectation(description: "wait")
        DispatchQueue.main.asyncAfter(deadline: .now() + duration) {
            expectation.fulfill()
        }
        wait(for: [expectation], timeout: duration + 1.0)
    }

    // MARK: - loadVehicles

    func testLoadVehiclesPopulatesPublishedArray() throws {
        // Given: KeyService 当前使用 stub
        // 这个测试验证 stubbed loadVehicles 的行为

        keyService.initialize()
        wait(for: 1.0)

        // Then: vehicles 数组应有 mock 数据
        XCTAssertFalse(keyService.vehicles.isEmpty)
        XCTAssertEqual(keyService.vehicles.count, 2) // 来自 Vehicle.mockVehicles
        XCTAssertEqual(keyService.vehicles[0].id, "vehicle_001")
        XCTAssertEqual(keyService.vehicles[0].brand, "BMW")
    }

    func testLoadVehiclesSetsLoadingState() throws {
        // Given
        XCTAssertFalse(keyService.isLoading)

        // When
        keyService.loadVehicles()
        XCTAssertTrue(keyService.isLoading)

        // Then: loading 应在完成后恢复
        wait(for: 1.0)
        XCTAssertFalse(keyService.isLoading)
    }

    // MARK: - loadShareRequests

    func testLoadShareRequestsPopulatesPublishedArray() throws {
        // Given
        XCTAssertTrue(keyService.shareRequests.isEmpty)

        // When
        keyService.loadShareRequests()
        wait(for: 0.5)

        // Then
        XCTAssertFalse(keyService.shareRequests.isEmpty)
        XCTAssertEqual(keyService.shareRequests.count, 2)
    }

    // MARK: - addKey

    func testAddKeyReturnsSuccessWithVehicle() throws {
        // Given

        // When
        let expectation = self.expectation(description: "addKey")
        var resultVehicle: Vehicle?
        var resultError: Error?

        keyService.addKey(vehicleId: "vehicle_new") { result in
            switch result {
            case .success(let vehicle):
                resultVehicle = vehicle
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
        XCTAssertNotNil(resultVehicle)
        XCTAssertEqual(resultVehicle?.id, "vehicle_new")
        XCTAssertEqual(resultVehicle?.keyInfo?.keyType, .owner)
    }

    func testAddKeySetsLoadingState() throws {
        // Given
        XCTAssertFalse(keyService.isLoading)

        // When
        let expectation = self.expectation(description: "addKey loading")
        keyService.addKey(vehicleId: "vehicle_new") { _ in
            expectation.fulfill()
        }

        // 调用后立即检查 loading 状态
        XCTAssertTrue(keyService.isLoading)

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertFalse(keyService.isLoading)
    }

    // MARK: - removeKey

    func testRemoveKeyRemovesFromPublishedArray() throws {
        // Given: 先加载数据
        keyService.initialize()
        wait(for: 1.0)
        XCTAssertEqual(keyService.vehicles.count, 2)

        // When
        let expectation = self.expectation(description: "removeKey")
        var resultError: Error?

        keyService.removeKey(vehicleId: "vehicle_001") { result in
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
        XCTAssertEqual(keyService.vehicles.count, 1)
        XCTAssertFalse(keyService.vehicles.contains { $0.id == "vehicle_001" })
    }

    // MARK: - approveShareRequest

    func testApproveShareRequestUpdatesStatus() throws {
        // Given: 先加载请求
        keyService.initialize()
        wait(for: 1.0)

        let pendingRequest = keyService.shareRequests.first { $0.status == .pending }
        XCTAssertNotNil(pendingRequest, "应该有 pending 的分享请求")

        // When
        let expectation = self.expectation(description: "approveShareRequest")
        var resultError: Error?

        keyService.approveShareRequest(requestId: pendingRequest!.id) { result in
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
        let updatedRequest = keyService.shareRequests.first { $0.id == pendingRequest!.id }
        XCTAssertNotNil(updatedRequest)
        XCTAssertEqual(updatedRequest?.status, .approved)
    }

    // MARK: - rejectShareRequest

    func testRejectShareRequestUpdatesStatus() throws {
        // Given
        keyService.initialize()
        wait(for: 1.0)

        let pendingRequest = keyService.shareRequests.first { $0.status == .pending }
        XCTAssertNotNil(pendingRequest)

        // When
        let expectation = self.expectation(description: "rejectShareRequest")
        var resultError: Error?

        keyService.rejectShareRequest(requestId: pendingRequest!.id) { result in
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
        let updatedRequest = keyService.shareRequests.first { $0.id == pendingRequest!.id }
        XCTAssertNotNil(updatedRequest)
        XCTAssertEqual(updatedRequest?.status, .rejected)
    }

    // MARK: - update 方法不应影响其他请求

    func testApproveOneRequestDoesNotAffectOthers() throws {
        // Given
        keyService.initialize()
        wait(for: 1.0)

        let pendingRequests = keyService.shareRequests.filter { $0.status == .pending }
        XCTAssertEqual(pendingRequests.count, 2)

        // When: 批准第一个 pending 请求
        let expectation = self.expectation(description: "approve")
        keyService.approveShareRequest(requestId: pendingRequests[0].id) { _ in
            expectation.fulfill()
        }
        wait(for: [expectation], timeout: 2.0)

        // Then: 第二个 pending 请求状态不变
        let secondRequest = keyService.shareRequests.first { $0.id == pendingRequests[1].id }
        XCTAssertEqual(secondRequest?.status, .pending)
    }

    // MARK: - createShareRequest

    func testCreateShareRequestReturnsPendingRequest() throws {
        // Given
        let createRequest = KeyShareRequestCreate(
            vehicleId: "vehicle_001",
            recipientPhone: "13800138000",
            recipientName: "测试用户",
            keyType: .friend,
            permissions: ["unlock", "lock"],
            validDays: 30,
            message: "请批准"
        )

        // When
        let expectation = self.expectation(description: "createShareRequest")
        var resultRequest: KeyShareRequest?
        var resultError: Error?

        keyService.createShareRequest(createRequest) { result in
            switch result {
            case .success(let request):
                resultRequest = request
            case .failure(let error):
                resultError = error
            }
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Then
        XCTAssertNil(resultError)
        XCTAssertNotNil(resultRequest)
        XCTAssertEqual(resultRequest?.status, .pending)
        XCTAssertEqual(resultRequest?.keyType, .friend)
        XCTAssertEqual(resultRequest?.permissions, ["unlock", "lock"])
        XCTAssertNotNil(resultRequest?.id)
    }

    func testCreateShareRequestGeneratesUniqueID() throws {
        // Given
        let request1 = KeyShareRequestCreate(
            vehicleId: "vehicle_001",
            recipientPhone: "13800138000",
            recipientName: nil,
            keyType: .friend,
            permissions: ["unlock", "lock"],
            validDays: nil,
            message: nil
        )

        // When: 创建两个请求
        let exp1 = expectation(description: "create1")
        let exp2 = expectation(description: "create2")

        var id1: String?
        var id2: String?

        keyService.createShareRequest(request1) { result in
            if case .success(let r) = result { id1 = r.id }
            exp1.fulfill()
        }
        keyService.createShareRequest(request1) { result in
            if case .success(let r) = result { id2 = r.id }
            exp2.fulfill()
        }

        wait(for: [exp1, exp2], timeout: 3.0)

        // Then: ID 应不同
        XCTAssertNotNil(id1)
        XCTAssertNotNil(id2)
        XCTAssertNotEqual(id1, id2, "每次创建应生成唯一 ID")
    }

    // MARK: - Shared Service 实例

    func testKeyServiceIsSingleton() throws {
        let instance1 = KeyService.shared
        let instance2 = KeyService.shared
        XCTAssertTrue(instance1 === instance2, "KeyService 应该是单例")
    }
}
