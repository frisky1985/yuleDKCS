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

    // 以下成员为 internal（而非 private/fileprivate）:
    // 同模块的 YDKHubClient+Stream.swift 等扩展需跨文件访问（SSE 流/测试）。
    let baseURL: URL
    let session: URLSession
    private let config: SDKConfig
    private let logger: YDKLogger
    let decoder: JSONDecoder
    private let encoder: JSONEncoder

    var token: String?

    // MARK: - 初始化

    public convenience init(config: SDKConfig) {
        self.init(config: config, session: nil)
    }

    /// 测试注入缝 (internal): 允许注入自定义 URLSession（例如挂载 MockURLProtocol
    /// 的 session），用于 wire 形状断言。生产路径走 public init（session=nil）。
    /// 说明: 4.1 审计遗留 "iOS 侧 ListKeys/GetKey/unbindKey/cancelShare wire 断言
    /// 需先给 YDKHubClient 加 transport 注入缝" — 此处为最小化、纯增量改动,
    /// 不触碰 request() 请求管线。
    init(config: SDKConfig, session: URLSession?) {
        self.config = config
        self.logger = YDKLogger(enabled: config.enableLogging)
        self.baseURL = URL(string: "https://\(config.hubEndpoint):\(config.hubPort)/api/v1")!

        if let session = session {
            self.session = session
        } else {
            let delegate = YDKURLSessionDelegate()
            self.session = URLSession(
                configuration: .ephemeral,
                delegate: delegate,
                delegateQueue: nil
            )
        }

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

/// 日志器 — public: YDKKeyManager 模块（依赖 YDKHubClient）也使用该类型。
public final class YDKLogger {
    private let enabled: Bool
    public init(enabled: Bool) { self.enabled = enabled }
    public func log(_ message: String) {
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
    /// 从 JSON 兼容字典构造 — `[String: Any]` 不满足 Encodable, 需递归包装
    static func json(_ object: [String: Any]) -> AnyEncodable {
        AnyEncodable(JSONAny.object(object.mapValues(JSONAny.init)))
    }
    func encode(to encoder: Encoder) throws {
        try _encode(encoder)
    }
}

/// JSON 兼容值的递归包装 (String/Number/Bool/Object/Array/Null)
private enum JSONAny: Encodable {
    case string(String)
    case number(Double)
    case bool(Bool)
    case object([String: JSONAny])
    case array([JSONAny])
    case null

    init(_ value: Any) {
        switch value {
        case let s as String: self = .string(s)
        case let n as Int: self = .number(Double(n))
        case let n as Int32: self = .number(Double(n))
        case let n as Int64: self = .number(Double(n))
        case let n as UInt: self = .number(Double(n))
        case let n as Double: self = .number(n)
        case let n as Float: self = .number(Double(n))
        case let b as Bool: self = .bool(b)
        case let arr as [Any]: self = .array(arr.map(JSONAny.init))
        case let dict as [String: Any]: self = .object(dict.mapValues(JSONAny.init))
        case is NSNull: self = .null
        default: self = .string(String(describing: value))
        }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case .string(let s): try container.encode(s)
        case .number(let n): try container.encode(n)
        case .bool(let b): try container.encode(b)
        case .object(let dict):
            var keyed = encoder.container(keyedBy: JSONCodingKey.self)
            for (key, value) in dict {
                try keyed.encode(value, forKey: JSONCodingKey(stringValue: key))
            }
        case .array(let arr): try container.encode(arr)
        case .null: try container.encodeNil()
        }
    }
}

/// 动态 JSON 键
private struct JSONCodingKey: CodingKey {
    var stringValue: String
    var intValue: Int? { nil }
    init(stringValue: String) { self.stringValue = stringValue }
    init?(intValue: Int) { nil }
}
