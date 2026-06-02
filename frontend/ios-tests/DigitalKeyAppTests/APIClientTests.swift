import XCTest
@testable import DigitalKeySDK

/// APIClient 单元测试
///
/// 使用 MockURLProtocol 模拟网络层，验证:
/// - URL、HTTP method、headers 构造是否正确
/// - JSON body 编码是否正确
/// - Query parameters 是否正确
/// - 正常响应解析
/// - HTTP 错误处理 (4xx, 5xx)
/// - 网络错误处理 (超时、断网)
final class APIClientTests: XCTestCase {
    var apiClient: APIClient!

    override func setUp() {
        super.setUp()
        MockURLProtocol.reset()
        // 使用 mock session
        apiClient = APIClient(session: .mockSession)
    }

    override func tearDown() {
        MockURLProtocol.reset()
        apiClient = nil
        super.tearDown()
    }

    // MARK: - Helper

    /// 等待异步请求完成
    private func waitForRequest(timeout: TimeInterval = 2.0) {
        let expectation = expectation(description: "Wait for network request")
        // 给异步请求一点时间完成
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
            expectation.fulfill()
        }
        wait(for: [expectation], timeout: timeout)
    }

    // MARK: - URL 构造测试

    func testGETRequestBuildsCorrectURL() throws {
        // Arrange: 配置一个空成功响应
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNotNil(captured)
        XCTAssertEqual(captured?.httpMethod, "GET")
        XCTAssertTrue(captured?.url?.absoluteString.hasPrefix("https://api.digitalkey.cn/v1/vehicles") ?? false)
    }

    func testPOSTRequestBuildsCorrectURL() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(path: "/vehicles", method: .post)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNotNil(captured)
        XCTAssertEqual(captured?.httpMethod, "POST")
        XCTAssertTrue(captured?.url?.absoluteString == "https://api.digitalkey.cn/v1/vehicles")
    }

    func testDELETERequestBuildsCorrectURL() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(path: "/vehicles/vehicle_001", method: .delete)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNotNil(captured)
        XCTAssertEqual(captured?.httpMethod, "DELETE")
        XCTAssertTrue(captured?.url?.absoluteString == "https://api.digitalkey.cn/v1/vehicles/vehicle_001")
    }

    // MARK: - HTTP Method 测试

    func testAllHTTPMethods() throws {
        let methods: [(HTTPMethod, String)] = [
            (.get, "GET"),
            (.post, "POST"),
            (.put, "PUT"),
            (.patch, "PATCH"),
            (.delete, "DELETE"),
        ]

        for (method, expected) in methods {
            MockURLProtocol.reset()
            MockURLProtocol.configureEmptySuccess()

            let request = APIRequest(path: "/test", method: method)
            apiClient.performRaw(request) { _ in }
            waitForRequest()

            let captured = MockURLProtocol.lastRequest
            XCTAssertEqual(captured?.httpMethod, expected, "Expected HTTP method \(expected)")
        }
    }

    // MARK: - Query Parameters 测试

    func testQueryParametersAreSentCorrectly() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act: GET /vehicles?status=active&page=1&per_page=20
        let request = APIRequest(
            path: "/vehicles",
            method: .get,
            queryItems: [
                URLQueryItem(name: "status", value: "active"),
                URLQueryItem(name: "page", value: "1"),
                URLQueryItem(name: "per_page", value: "20"),
            ]
        )
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNotNil(captured?.url)

        let components = captured.flatMap { URLComponents(url: $0.url!, resolvingAgainstBaseURL: false) }
        XCTAssertNotNil(components)
        XCTAssertTrue(components?.queryItems?.contains(URLQueryItem(name: "status", value: "active")) ?? false)
        XCTAssertTrue(components?.queryItems?.contains(URLQueryItem(name: "page", value: "1")) ?? false)
        XCTAssertTrue(components?.queryItems?.contains(URLQueryItem(name: "per_page", value: "20")) ?? false)
        XCTAssertEqual(components?.queryItems?.count, 3)
    }

    func testRequestWithoutQueryParamsHasNoQueryString() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNotNil(captured?.url)
        XCTAssertNil(captured.flatMap { URLComponents(url: $0.url!, resolvingAgainstBaseURL: false) }?.queryItems)
    }

    // MARK: - Headers 测试

    func testDefaultHeadersAreSet() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNotNil(captured)
        XCTAssertEqual(captured?.value(forHTTPHeaderField: "Content-Type"), "application/json")
        XCTAssertEqual(captured?.value(forHTTPHeaderField: "Accept"), "application/json")
    }

    func testCustomHeadersOverrideDefaultHeaders() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act: 自定义 Content-Type
        let request = APIRequest(
            path: "/vehicles",
            method: .post,
            headers: ["Content-Type": "application/xml"]
        )
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert: 自定义 header 应覆盖默认
        let captured = MockURLProtocol.lastRequest
        XCTAssertEqual(captured?.value(forHTTPHeaderField: "Content-Type"), "application/xml")
    }

    func testCustomHeadersAreAddedAlongsideDefaults() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(
            path: "/vehicles",
            method: .get,
            headers: ["X-Request-ID": "abc-123"]
        )
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertEqual(captured?.value(forHTTPHeaderField: "Content-Type"), "application/json")
        XCTAssertEqual(captured?.value(forHTTPHeaderField: "X-Request-ID"), "abc-123")
    }

    // MARK: - Request Body 测试

    func testPOSTRequestIncludesBody() throws {
        // Arrange
        struct CreateVehicleBody: Encodable {
            let vin: String
            let brand: String
            let model: String
        }
        MockURLProtocol.configureSuccess(["id": "vehicle_new", "status": "created"] as [String: String])

        let body = CreateVehicleBody(vin: "WBA1234567890", brand: "BMW", model: "iX3")
        let bodyData = try JSONEncoder().encode(body)

        // Act
        let request = APIRequest(path: "/vehicles", method: .post, body: bodyData)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNotNil(captured?.httpBody)
        let sentBody = try JSONSerialization.jsonObject(with: captured!.httpBody!) as? [String: String]
        XCTAssertEqual(sentBody?["vin"], "WBA1234567890")
        XCTAssertEqual(sentBody?["brand"], "BMW")
        XCTAssertEqual(sentBody?["model"], "iX3")
    }

    func testGETRequestHasNoBody() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNil(captured?.httpBody)
    }

    func testDELETERequestHasNoBody() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act
        let request = APIRequest(path: "/vehicles/vehicle_001", method: .delete)
        apiClient.performRaw(request) { _ in }

        waitForRequest()

        // Assert
        let captured = MockURLProtocol.lastRequest
        XCTAssertNil(captured?.httpBody)
    }

    // MARK: - 正常响应解析测试

    func testSuccessfulGETReturnsDecodedResponse() throws {
        // Arrange: 模拟车辆列表 JSON 响应
        let json = """
        {
            "vehicles": [
                {
                    "id": "vehicle_001",
                    "vin": "WBA1234567890",
                    "brand": "BMW",
                    "model": "iX3",
                    "year": 2024,
                    "color": "White",
                    "nickname": "My BMW",
                    "status": {
                        "is_online": true,
                        "battery_level": 85,
                        "fuel_level": null,
                        "mileage": 12580,
                        "location": null,
                        "last_updated": "2026-06-01T10:00:00Z"
                    },
                    "key_info": {
                        "key_id": "key_001",
                        "key_type": "OWNER",
                        "permissions": ["unlock", "lock", "start"],
                        "valid_from": "2026-01-01T00:00:00Z",
                        "valid_until": null,
                        "is_primary": true
                    }
                }
            ]
        }
        """
        MockURLProtocol.responseData = json.data(using: .utf8)
        MockURLProtocol.responseStatusCode = 200

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success(let response):
            XCTAssertEqual(response.vehicles.count, 1)
            XCTAssertEqual(response.vehicles[0].id, "vehicle_001")
            XCTAssertEqual(response.vehicles[0].brand, "BMW")
            XCTAssertEqual(response.vehicles[0].keyInfo?.keyType, .owner)
        case .failure(let error):
            XCTFail("Expected success, got error: \(error)")
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func testSuccessfulPOSTReturnsCreatedResource() throws {
        // Arrange: 模拟创建成功后返回完整对象
        let json = """
        {
            "id": "vehicle_003",
            "vin": "NEW_VIN_9999",
            "brand": "Brand",
            "model": "Model",
            "year": 2024,
            "color": "White",
            "status": {
                "is_online": false,
                "battery_level": 0,
                "fuel_level": null,
                "mileage": 0,
                "location": null,
                "last_updated": "2026-06-01T10:00:00Z"
            },
            "key_info": {
                "key_id": "key_003",
                "key_type": "OWNER",
                "permissions": ["unlock", "lock", "start"],
                "valid_from": "2026-06-01T00:00:00Z",
                "valid_until": null,
                "is_primary": true
            }
        }
        """
        MockURLProtocol.responseData = json.data(using: .utf8)
        MockURLProtocol.responseStatusCode = 201

        // Act
        let request = APIRequest(path: "/vehicles", method: .post)
        let expectation = self.expectation(description: "POST vehicle")

        var result: Result<Vehicle, Error>?
        apiClient.perform(request) { (res: Result<Vehicle, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success(let vehicle):
            XCTAssertEqual(vehicle.id, "vehicle_003")
            XCTAssertEqual(vehicle.vin, "NEW_VIN_9999")
            XCTAssertEqual(vehicle.keyInfo?.keyType, .owner)
        case .failure(let error):
            XCTFail("Expected success, got error: \(error)")
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func testEmptySuccessResponseForVoidEndpoint() throws {
        // Arrange: 模拟 204 No Content
        MockURLProtocol.configureEmptySuccess(statusCode: 204)

        // Act
        let request = APIRequest(path: "/vehicles/vehicle_001/unlock", method: .post)
        let expectation = self.expectation(description: "POST unlock")

        var result: Result<Void, Error>?
        apiClient.performRaw(request) { (res: Result<Void, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTAssertTrue(true)
        case .failure(let error):
            XCTFail("Expected success, got error: \(error)")
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    // MARK: - 4xx 错误处理测试

    func test401ReturnsUnauthorizedError() throws {
        // Arrange
        MockURLProtocol.configureError(statusCode: 401, message: "无效的认证令牌")

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles with 401")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected unauthorized error")
        case .failure(let error):
            XCTAssertTrue(error is APIError)
            if case APIError.unauthorized = error {
                // 正确
            } else {
                XCTFail("Expected unauthorized, got \(error)")
            }
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func test404ReturnsNotFoundError() throws {
        // Arrange
        MockURLProtocol.configureError(statusCode: 404, message: "车辆不存在")

        // Act
        let request = APIRequest(path: "/vehicles/nonexistent", method: .get)
        let expectation = self.expectation(description: "GET nonexistent vehicle")

        var result: Result<Vehicle, Error>?
        apiClient.perform(request) { (res: Result<Vehicle, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected not found error")
        case .failure(let error):
            if case APIError.notFound = error {
                // 正确
            } else {
                XCTFail("Expected notFound, got \(error)")
            }
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func test400ReturnsHTTPErrorWithMessage() throws {
        // Arrange
        MockURLProtocol.configureError(statusCode: 400, message: "VIN 格式不正确")

        // Act
        let request = APIRequest(path: "/vehicles", method: .post)
        let expectation = self.expectation(description: "POST vehicle with 400")

        var result: Result<Vehicle, Error>?
        apiClient.perform(request) { (res: Result<Vehicle, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected HTTP 400 error")
        case .failure(let error):
            guard case APIError.httpError(let code, let msg) = error else {
                XCTFail("Expected httpError, got \(error)")
                return
            }
            XCTAssertEqual(code, 400)
            XCTAssertEqual(msg, "VIN 格式不正确")
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func test422ValidationError() throws {
        // Arrange
        let json = """
        {"message": "品牌不能为空", "errors": {"brand": ["不能为空"]}}
        """
        MockURLProtocol.responseData = json.data(using: .utf8)
        MockURLProtocol.responseStatusCode = 422

        // Act
        let request = APIRequest(path: "/vehicles", method: .post)
        let expectation = self.expectation(description: "POST vehicle with 422")

        var result: Result<Vehicle, Error>?
        apiClient.perform(request) { (res: Result<Vehicle, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected HTTP 422 error")
        case .failure(let error):
            guard case APIError.httpError(let code, let msg) = error else {
                XCTFail("Expected httpError, got \(error)")
                return
            }
            XCTAssertEqual(code, 422)
            XCTAssertEqual(msg, "品牌不能为空")
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    // MARK: - 5xx 错误处理测试

    func test500ReturnsServerError() throws {
        // Arrange
        MockURLProtocol.configureError(statusCode: 500, message: "内部服务器错误")

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles with 500")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected server error")
        case .failure(let error):
            guard case APIError.serverError(let msg) = error else {
                XCTFail("Expected serverError, got \(error)")
                return
            }
            XCTAssertEqual(msg, "内部服务器错误")
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func test503WithNoMessage() throws {
        // Arrange
        MockURLProtocol.configureError(statusCode: 503)

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles with 503")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected server error")
        case .failure(let error):
            guard case APIError.serverError(let msg) = error else {
                XCTFail("Expected serverError, got \(error)")
                return
            }
            XCTAssertNil(msg)
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    // MARK: - 网络错误测试

    func testNetworkTimeoutReturnsTimeoutError() throws {
        // Arrange: 模拟超时错误
        let timeoutError = NSError(
            domain: NSURLErrorDomain,
            code: NSURLErrorTimedOut,
            userInfo: [NSLocalizedDescriptionKey: "请求超时"]
        )
        MockURLProtocol.configureNetworkError(timeoutError)

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles timeout")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected timeout error")
        case .failure(let error):
            guard case APIError.timeout = error else {
                XCTFail("Expected timeout, got \(error)")
                return
            }
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func testNoInternetReturnsNetworkError() throws {
        // Arrange
        let noConnectionError = NSError(
            domain: NSURLErrorDomain,
            code: NSURLErrorNotConnectedToInternet,
            userInfo: [NSLocalizedDescriptionKey: "无网络连接"]
        )
        MockURLProtocol.configureNetworkError(noConnectionError)

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles no network")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected network error")
        case .failure(let error):
            guard case APIError.networkError = error else {
                XCTFail("Expected networkError, got \(error)")
                return
            }
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func testConnectionLostReturnsNetworkError() throws {
        // Arrange
        let connectionError = NSError(
            domain: NSURLErrorDomain,
            code: NSURLErrorNetworkConnectionLost,
            userInfo: [NSLocalizedDescriptionKey: "网络连接断开"]
        )
        MockURLProtocol.configureNetworkError(connectionError)

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles connection lost")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected network error")
        case .failure(let error):
            guard case APIError.networkError = error else {
                XCTFail("Expected networkError, got \(error)")
                return
            }
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    // MARK: - 响应解码错误测试

    func testMalformedJSONReturnsDecodingError() throws {
        // Arrange: 返回格式错误的 JSON
        MockURLProtocol.responseData = "{invalid json}".data(using: .utf8)
        MockURLProtocol.responseStatusCode = 200

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles malformed JSON")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected decoding error")
        case .failure(let error):
            guard case APIError.decodingError = error else {
                XCTFail("Expected decodingError, got \(error)")
                return
            }
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    func testEmptyResponseBodyForDecodableReturnsInvalidResponse() throws {
        // Arrange: 空响应体
        MockURLProtocol.responseData = Data()
        MockURLProtocol.responseStatusCode = 200

        // Act
        let request = APIRequest(path: "/vehicles", method: .get)
        let expectation = self.expectation(description: "GET vehicles empty response")

        var result: Result<VehiclesListResponse, Error>?
        apiClient.perform(request) { (res: Result<VehiclesListResponse, Error>) in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        // Assert
        switch result {
        case .success:
            XCTFail("Expected decoding error")
        case .failure:
            // 可以是 decoding 或 invalid response 错误
            XCTAssertTrue(true)
        case nil:
            XCTFail("Expected result, got nil")
        }
    }

    // MARK: - 多请求并发测试

    func testMultipleRequestsAreCapturedIndependently() throws {
        // Arrange
        MockURLProtocol.configureEmptySuccess()

        // Act: 发送3个请求
        let req1 = APIRequest(path: "/vehicles", method: .get)
        let req2 = APIRequest(path: "/vehicles/vehicle_001", method: .get)
        let req3 = APIRequest(path: "/share-requests", method: .get)

        apiClient.performRaw(req1) { _ in }
        apiClient.performRaw(req2) { _ in }
        apiClient.performRaw(req3) { _ in }

        waitForRequest(timeout: 3.0)

        // Assert
        XCTAssertEqual(MockURLProtocol.capturedRequests.count, 3)
        XCTAssertTrue(MockURLProtocol.capturedRequests[0].url?.absoluteString.contains("/vehicles$") ?? false
            || MockURLProtocol.capturedRequests[0].url?.absoluteString == "https://api.digitalkey.cn/v1/vehicles")
        XCTAssertTrue(MockURLProtocol.capturedRequests[2].url?.absoluteString.contains("/share-requests") ?? false)
    }
}

// MARK: - Test Helper Types

/// 用于测试的车辆列表响应结构
private struct VehiclesListResponse: Decodable {
    let vehicles: [Vehicle]
}
