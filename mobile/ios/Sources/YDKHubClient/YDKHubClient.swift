import Foundation

/// yuleDKCS Hub REST Gateway 客户端
///
/// 通过 HTTP/JSON 调用 Hub REST Gateway (:8080)。
/// 依赖: Foundation (URLSession) — 零第三方依赖。
///
/// 用法:
/// ```swift
/// let client = try YDKHubClient(config: SDKConfig(hubEndpoint: "hub.yuletech.com"))
/// client.setToken("session-token-from-oem-server")
/// let key = try await client.bindKey(vehicleId: "LSV...")
/// ```
public final class YDKHubClient {

    // MARK: - 内部状态

    private let baseURL: URL
    private let session: URLSession
    private let config: SDKConfig
    private let logger: YDKLogger
    private let decoder: JSONDecoder
    private let encoder: JSONEncoder

    fileprivate var token: String?

    // MARK: - 初始化

    public init(config: SDKConfig) {
        self.config = config
        self.logger = YDKLogger(enabled: config.enableLogging)
        self.baseURL = URL(string: "https://\(config.hubEndpoint):\(config.hubPort)/api/v1")!

        let delegate = YDKURLSessionDelegate()
        self.session = URLSession(
            configuration: .ephemeral,
            delegate: delegate,
            delegateQueue: nil
        )

        self.decoder = JSONDecoder()
        self.encoder = JSONEncoder()
    }

    // MARK: - Auth

    /// 设置用户 session token（从车厂 Server 获取后调用）
    public func setToken(_ token: String) {
        self.token = token
    }

    /// 清除 token（用户登出时调用）
    public func clearToken() {
        self.token = nil
    }

    /// 清理连接（关闭 URLSession）
    public func shutdown() {
        session.invalidateAndCancel()
    }
}

// MARK: - 内部 HTTP 请求封装

extension YDKHubClient {

    /// 发起 HTTP JSON 请求并解码响应
    func request<T: Decodable>(
        method: String,
        path: String,
        body: Encodable? = nil,
        query: [String: String]? = nil
    ) async throws -> T {
        var components = URLComponents(
            url: baseURL.appendingPathComponent(path),
            resolvingAgainstBaseURL: false
        )!
        if let query = query {
            components.queryItems = query.map { URLQueryItem(name: $0.key, value: $0.value) }
        }

        var req = URLRequest(url: components.url!)
        req.httpMethod = method
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue("application/json", forHTTPHeaderField: "Accept")
        req.setValue(Version.current, forHTTPHeaderField: "X-SDK-Version")
        req.setValue("ios", forHTTPHeaderField: "X-Platform")

        if let token = token {
            req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }

        if let body = body {
            req.httpBody = try encoder.encode(AnyEncodable(body))
        }

        logger.log("→ \(method) \(path)")

        let (data, response): (Data, URLResponse)
        do {
            (data, response) = try await session.data(for: req)
        } catch let error as URLError {
            if error.code == .timedOut {
                throw YDKError.timeout
            }
            throw YDKError.networkError(error)
        } catch {
            throw YDKError.networkError(error)
        }

        guard let httpResponse = response as? HTTPURLResponse else {
            throw YDKError.internal_("invalid response type")
        }

        logger.log("← \(httpResponse.statusCode)")

        // 错误映射
        if httpResponse.statusCode >= 400 {
            if let errorBody = try? decoder.decode(HubErrorResponse.self, from: data),
               let code = errorBody.code ?? errorBody.error {
                throw YDKError.hubError(code, errorBody.message ?? "")
            }
            throw YDKError.httpError(httpResponse.statusCode)
        }

        do {
            return try decoder.decode(T.self, from: data)
        } catch {
            // 204 No Content → 返回空解码
            if data.isEmpty, let empty = Optional<T>.none as? T {
                return empty
            }
            throw YDKError.decodingFailed(error.localizedDescription)
        }
    }
}

// MARK: - 辅助

final class YDKURLSessionDelegate: NSObject, URLSessionDelegate {
    // TLS 验证（后续可添加证书固定）
}

final class YDKLogger {
    private let enabled: Bool
    init(enabled: Bool) { self.enabled = enabled }
    func log(_ message: String) {
        guard enabled else { return }
        print("[YDKHubClient] \(message)")
    }
}

/// SDK 版本标识
enum Version {
    static let current = "1.0.0"
}

/// AnyEncodable 包装 — 支持字典字面量作为 body
struct AnyEncodable: Encodable {
    private let _encode: (Encoder) throws -> Void
    init(_ wrapped: Encodable) {
        _encode = { try wrapped.encode(to: $0) }
    }
    func encode(to encoder: Encoder) throws {
        try _encode(encoder)
    }
}
