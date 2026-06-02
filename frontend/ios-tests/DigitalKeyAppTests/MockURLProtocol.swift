import Foundation

/// Mock URLProtocol 用于拦截和模拟网络请求
///
/// 使用方式:
/// 1. 在测试 setUp 中配置 MockURLProtocol
/// 2. 创建 URLSession 时传入 MockURLProtocol 配置
/// 3. 设置预期的响应数据/错误
/// 4. 验证捕获到的请求是否符合预期
class MockURLProtocol: URLProtocol {
    // MARK: - 静态配置

    /// 模拟的响应数据
    static var responseData: Data?

    /// 模拟的响应 HTTP 状态码和 headers
    static var responseStatusCode: Int = 200
    static var responseHeaders: [String: String] = ["Content-Type": "application/json"]

    /// 模拟的网络错误
    static var responseError: Error?

    /// 模拟的响应延迟（秒）
    static var responseDelay: TimeInterval = 0

    // MARK: - 请求捕获

    /// 所有被拦截的请求（可用于验证）
    private(set) static var capturedRequests: [URLRequest] = []

    /// 最后捕获的请求
    static var lastRequest: URLRequest? {
        capturedRequests.last
    }

    /// 重置所有状态
    static func reset() {
        responseData = nil
        responseStatusCode = 200
        responseHeaders = ["Content-Type": "application/json"]
        responseError = nil
        responseDelay = 0
        capturedRequests = []
    }

    // MARK: - URLProtocol Overrides

    override class func canInit(with request: URLRequest) -> Bool {
        return true // 拦截所有请求
    }

    override class func canonicalRequest(for request: URLRequest) -> URLRequest {
        return request
    }

    override func startLoading() {
        // 捕获请求
        Self.capturedRequests.append(request)

        // 模拟延迟
        if Self.responseDelay > 0 {
            Thread.sleep(forTimeInterval: Self.responseDelay)
        }

        // 模拟网络错误
        if let error = Self.responseError {
            client?.urlProtocol(self, didFailWithError: error)
            client?.urlProtocolDidFinishLoading(self)
            return
        }

        // 模拟 HTTP 响应
        let url = request.url ?? URL(string: "https://mock.local")!
        let httpResponse = HTTPURLResponse(
            url: url,
            statusCode: Self.responseStatusCode,
            httpVersion: "HTTP/1.1",
            headerFields: Self.responseHeaders
        )!

        client?.urlProtocol(self, didReceive: httpResponse, cacheStoragePolicy: .notAllowed)

        // 模拟响应数据
        if let data = Self.responseData {
            client?.urlProtocol(self, didLoad: data)
        }

        client?.urlProtocolDidFinishLoading(self)
    }

    override func stopLoading() {
        // 不需要额外清理
    }
}

// MARK: - 便捷工厂方法

extension MockURLProtocol {
    /// 配置一个成功的 JSON 响应
    static func configureSuccess<T: Encodable>(_ value: T, statusCode: Int = 200) {
        let encoder = JSONEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        responseData = try? encoder.encode(value)
        responseStatusCode = statusCode
        responseError = nil
    }

    /// 配置一个空成功响应（204 No Content）
    static func configureEmptySuccess(statusCode: Int = 204) {
        responseData = nil
        responseStatusCode = statusCode
        responseError = nil
    }

    /// 配置一个错误响应
    static func configureError(statusCode: Int, message: String? = nil) {
        struct ErrorResponse: Encodable {
            let message: String?
        }
        let body = ErrorResponse(message: message)
        let encoder = JSONEncoder()
        responseData = try? encoder.encode(body)
        responseStatusCode = statusCode
        responseError = nil
    }

    /// 配置一个网络错误
    static func configureNetworkError(_ error: Error) {
        responseData = nil
        responseStatusCode = 0
        responseError = error
    }
}

// MARK: - Helper: 创建 Mock URLSession

extension URLSession {
    /// 返回一个使用 MockURLProtocol 的 URLSession，用于测试
    static var mockSession: URLSession {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [MockURLProtocol.self]
        return URLSession(configuration: config)
    }
}
