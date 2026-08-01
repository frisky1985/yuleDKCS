import XCTest
@testable import YDKHubClient

/// Phase 4.1 / W3 — ListKeys / GetKey / UnbindKey / CancelShare wire 形状契约测试 (iOS)
///
/// 背景: 4.1 审计遗留 "iOS 侧 ListKeys/GetKey/unbindKey/cancelShare wire 断言缺失
/// (需先给 YDKHubClient 加 transport 注入缝)"。本文件配套的注入缝见
/// YDKHubClient.swift 的 internal `init(config:session:)` — 允许注入挂载了
/// MockURLProtocol 的 URLSession, 从而在**不触碰 request() 请求管线**的前提下
/// 捕获 SDK 实际发出的 URLRequest, 断言方法/路径/query/请求体/认证头。
///
/// 与 RequestShapeContractTests（字节形状镜像验证）互补: 本文件是真实 wire 捕获,
/// 覆盖无 body 的 GET/DELETE 类接口（bindKey/acceptShare 的 body 形状已由前者覆盖）。
final class YDKHubClientWireShapeContractTests: XCTestCase {

    override func setUp() {
        super.setUp()
        MockURLProtocol.reset()
    }

    override func tearDown() {
        MockURLProtocol.reset()
        super.tearDown()
    }

    // MARK: - 测试基建

    /// 构造挂载 MockURLProtocol 的 client（走注入缝, 不经真实网络）。
    private func makeClient(token: String? = nil) -> YDKHubClient {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [MockURLProtocol.self]
        let session = URLSession(configuration: config)
        let client = YDKHubClient(
            config: SDKConfig(hubEndpoint: "hub.test.local", hubPort: 8080, enableLogging: false),
            session: session
        )
        if let token = token {
            client.setToken(token)
        }
        return client
    }

    /// 断言通用请求头（SDK 所有请求统一携带）。
    private func assertCommonHeaders(_ req: URLRequest) {
        XCTAssertEqual(req.value(forHTTPHeaderField: "Content-Type"), "application/json")
        XCTAssertEqual(req.value(forHTTPHeaderField: "Accept"), "application/json")
        XCTAssertEqual(req.value(forHTTPHeaderField: "X-SDK-Version"), "1.0.0")
        XCTAssertEqual(req.value(forHTTPHeaderField: "X-Platform"), "ios")
    }

    // MARK: - ListKeys wire 形状

    /// listKeys: GET /api/v1/keys + query (vehicleId/status 透传) + Bearer 认证头。
    func testListKeysWireShapeWithQueryAndAuth() async throws {
        MockURLProtocol.responseBody = Data(#"{"keys":[]}"#.utf8)
        let client = makeClient(token: "wire-token-001")

        let keys = try await client.listKeys(vehicleId: "VH-WIRE-001", status: "ACTIVE")

        XCTAssertEqual(keys, [], "空 keys 响应应解码为空数组")
        let req = try XCTUnwrap(MockURLProtocol.requests.last, "应捕获到 1 个请求")
        XCTAssertEqual(req.httpMethod, "GET")
        XCTAssertEqual(req.url?.path, "/api/v1/keys")
        XCTAssertNil(req.httpBody, "GET listKeys 不得携带 body")

        // query 参数形状: camelCase 字段名透传
        let query = try XCTUnwrap(
            req.url.flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false)?.queryItems },
            "listKeys 应携带 query items"
        )
        XCTAssertEqual(Set(query.map(\.name)), Set(["vehicleId", "status"]))
        XCTAssertEqual(query.first(where: { $0.name == "vehicleId" })?.value, "VH-WIRE-001")
        XCTAssertEqual(query.first(where: { $0.name == "status" })?.value, "ACTIVE")

        // 认证头
        XCTAssertEqual(req.value(forHTTPHeaderField: "Authorization"), "Bearer wire-token-001")
        assertCommonHeaders(req)
    }

    /// listKeys 无过滤条件: 不携带任何 query 参数。
    func testListKeysWireShapeWithoutFilters() async throws {
        MockURLProtocol.responseBody = Data(#"{"keys":[]}"#.utf8)
        let client = makeClient()

        _ = try await client.listKeys()

        let req = try XCTUnwrap(MockURLProtocol.requests.last)
        XCTAssertEqual(req.httpMethod, "GET")
        XCTAssertEqual(req.url?.path, "/api/v1/keys")
        let query = req.url.flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false)?.queryItems }
        XCTAssertNil(query, "无过滤条件时不得携带 query")
    }

    // MARK: - GetKey wire 形状

    /// getKey: GET /api/v1/keys/{keyId}, keyId 拼入路径, 无 body/query。
    func testGetKeyWireShape() async throws {
        let keyJSON = """
        {"keyId":"K-WIRE-001","vehicleId":"VH-WIRE-001","deviceId":"D-WIRE-001",
         "keyType":"OWNER","status":"ACTIVE","validFrom":0,"validUntil":0,"createdAt":0}
        """
        MockURLProtocol.responseBody = Data(keyJSON.utf8)
        let client = makeClient(token: "wire-token-002")

        let key = try await client.getKey(keyId: "K-WIRE-001")

        XCTAssertEqual(key.keyId, "K-WIRE-001")
        XCTAssertEqual(key.vehicleId, "VH-WIRE-001")
        let req = try XCTUnwrap(MockURLProtocol.requests.last)
        XCTAssertEqual(req.httpMethod, "GET")
        XCTAssertEqual(req.url?.path, "/api/v1/keys/K-WIRE-001", "keyId 必须拼入路径")
        XCTAssertNil(req.httpBody)
        XCTAssertNil(
            req.url.flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false)?.queryItems },
            "getKey 不得携带 query"
        )
        XCTAssertEqual(req.value(forHTTPHeaderField: "Authorization"), "Bearer wire-token-002")
        assertCommonHeaders(req)
    }

    // MARK: - UnbindKey wire 形状

    /// unbindKey: DELETE /api/v1/keys/{keyId}, 无 body。
    func testUnbindKeyWireShape() async throws {
        MockURLProtocol.responseBody = Data("{}".utf8)
        let client = makeClient(token: "wire-token-003")

        try await client.unbindKey(keyId: "K-WIRE-002")

        let req = try XCTUnwrap(MockURLProtocol.requests.last)
        XCTAssertEqual(req.httpMethod, "DELETE")
        XCTAssertEqual(req.url?.path, "/api/v1/keys/K-WIRE-002", "keyId 必须拼入路径")
        XCTAssertNil(req.httpBody, "DELETE unbindKey 不得携带 body")
        XCTAssertNil(
            req.url.flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false)?.queryItems },
            "unbindKey 不得携带 query"
        )
        XCTAssertEqual(req.value(forHTTPHeaderField: "Authorization"), "Bearer wire-token-003")
        assertCommonHeaders(req)
    }

    // MARK: - CancelShare wire 形状

    /// cancelShare: DELETE /api/v1/shares/{shareId}, 无 body。
    func testCancelShareWireShape() async throws {
        MockURLProtocol.responseBody = Data("{}".utf8)
        let client = makeClient(token: "wire-token-004")

        try await client.cancelShare(shareId: "SH-WIRE-001")

        let req = try XCTUnwrap(MockURLProtocol.requests.last)
        XCTAssertEqual(req.httpMethod, "DELETE")
        XCTAssertEqual(req.url?.path, "/api/v1/shares/SH-WIRE-001", "shareId 必须拼入路径")
        XCTAssertNil(req.httpBody, "DELETE cancelShare 不得携带 body")
        XCTAssertNil(
            req.url.flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false)?.queryItems },
            "cancelShare 不得携带 query"
        )
        XCTAssertEqual(req.value(forHTTPHeaderField: "Authorization"), "Bearer wire-token-004")
        assertCommonHeaders(req)
    }
}

/// 请求捕获器: 拦截 URLSession 全部请求并记录, 返回可配置的模拟响应。
/// 仅测试用, 不进入交付物。
final class MockURLProtocol: URLProtocol {

    /// 捕获到的请求（按发出顺序）。
    static var requests: [URLRequest] = []
    /// 模拟响应状态码 / body。
    static var responseStatus: Int = 200
    static var responseBody: Data = Data("{}".utf8)

    /// 清空捕获状态（setUp/tearDown 调用）。
    static func reset() {
        requests = []
        responseStatus = 200
        responseBody = Data("{}".utf8)
    }

    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }

    override func startLoading() {
        MockURLProtocol.requests.append(request)
        let response = HTTPURLResponse(
            url: request.url!,
            statusCode: MockURLProtocol.responseStatus,
            httpVersion: "HTTP/1.1",
            headerFields: ["Content-Type": "application/json"]
        )!
        client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
        client?.urlProtocol(self, didLoad: MockURLProtocol.responseBody)
        client?.urlProtocolDidFinishLoading(self)
    }

    override func stopLoading() {}
}
