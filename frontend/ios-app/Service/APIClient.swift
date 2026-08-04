import Foundation
import DigitalKeySDK

/// 通用的 API 错误类型
enum APIError: LocalizedError, Equatable {
    case invalidURL
    case invalidResponse
    case httpError(statusCode: Int, message: String?)
    case decodingError(String)
    case networkError(String)
    case encodingError(String)
    case unauthorized
    case notFound
    case serverError(String?)
    case timeout
    case tlsPinningFailed(host: String, reason: String)
    case unknown(String)

    var errorDescription: String? {
        switch self {
        case .invalidURL: return "无效的 URL"
        case .invalidResponse: return "无效的响应"
        case .httpError(let code, let msg): return "HTTP \(code): \(msg ?? "未知错误")"
        case .decodingError(let s): return "解码失败: \(s)"
        case .networkError(let s): return "网络错误: \(s)"
        case .encodingError(let s): return "编码失败: \(s)"
        case .unauthorized: return "未授权"
        case .notFound: return "资源未找到"
        case .serverError(let s): return "服务器错误: \(s ?? "")"
        case .timeout: return "请求超时"
        case .tlsPinningFailed(let host, let reason): return "TLS Pinning 校验失败: \(host) - \(reason)"
        case .unknown(let s): return "未知错误: \(s)"
        }
    }
}

/// HTTP 方法
enum HTTPMethod: String {
    case get = "GET"
    case post = "POST"
    case put = "PUT"
    case patch = "PATCH"
    case delete = "DELETE"
}

/// API 请求配置
struct APIRequest {
    let path: String
    let method: HTTPMethod
    let headers: [String: String]
    let body: Data?
    let queryItems: [URLQueryItem]

    init(
        path: String,
        method: HTTPMethod = .get,
        headers: [String: String] = [:],
        body: Data? = nil,
        queryItems: [URLQueryItem] = []
    ) {
        self.path = path
        self.method = method
        self.headers = headers
        self.body = body
        self.queryItems = queryItems
    }
}

/// TLS Pinning 配置
struct TLSPinningConfig {
    /// 域名 → 公钥 SHA-256 Base64 哈希列表
    let pinnedHosts: [String: [String]]
    /// 是否为 Debug 模式（Debug 下 Pinning 失败仅记录，不阻断请求）
    let isDebug: Bool

    init(pinnedHosts: [String: [String]], isDebug: Bool = false) {
        self.pinnedHosts = pinnedHosts
        self.isDebug = isDebug
    }
}

/// 基础 API 客户端
/// 封装 URLSession，统一处理请求构造、响应解析、错误处理
///
/// ## TLS Pinning 支持
/// 通过 `TLSPinningConfig` 配置证书锁定，支持公钥哈希（推荐）和证书哈希两种策略：
/// - 使用 `TlsPinningDelegate` 实现 `URLSessionDelegate` 回调
/// - 生产环境严格校验，Debug 模式下失败仅记录
/// - 支持多公钥哈希轮换
/// - Pinning 失败时返回 `APIError.tlsPinningFailed`
class APIClient {
    static let shared = APIClient()

    /// 可注入的 URLSession（测试时替换为 MockURLSession 配置）
    var session: URLSession

    /// TLS Pinning Delegate（持有引用避免释放）
    private var pinningDelegate: TlsPinningDelegate?

    /// 后端 base URL
    var baseURL: String {
        // TODO: 从配置读取
        return "https://api.digitalkey.cn/v1"
    }

    /// 默认 headers（如认证 token）
    var defaultHeaders: [String: String] {
        var headers: [String: String] = [
            "Content-Type": "application/json",
            "Accept": "application/json",
        ]
        // TODO: 从 Keychain 读取 token
        // if let token = KeychainManager.shared.getToken() {
        //     headers["Authorization"] = "Bearer \(token)"
        // }
        return headers
    }

    /// TLS Pinning 配置
    private var tlsConfig: TLSPinningConfig?

    // MARK: - Init

    /// 初始化 API 客户端
    /// - Parameters:
    ///   - session: 可选的注入 URLSession（测试时使用 MockURLProtocol 的 session）
    ///   - tlsConfig: TLS Pinning 配置（生产环境必须配置）
    init(session: URLSession? = nil, tlsConfig: TLSPinningConfig? = nil) {
        self.tlsConfig = tlsConfig

        if let session = session {
            // 测试用：使用注入的 Mock Session
            self.session = session
            self.pinningDelegate = nil
        } else if let config = tlsConfig, !config.pinnedHosts.isEmpty {
            // 生产环境：创建带 TLS Pinning 的 URLSession
            let strategies = config.pinnedHosts.mapValues { hashes -> PinningStrategy in
                return .publicKey(hashes: hashes)
            }
            let delegate = TlsPinningDelegate(
                pinnedHosts: strategies,
                isDebug: config.isDebug
            )
            delegate.onPinningFailed = { [weak self] errorInfo in
                self?.handlePinningFailed(errorInfo: errorInfo)
            }
            self.pinningDelegate = delegate

            let sessionConfig = URLSessionConfiguration.default
            sessionConfig.timeoutIntervalForRequest = 30
            sessionConfig.timeoutIntervalForResource = 60
            self.session = URLSession(
                configuration: sessionConfig,
                delegate: delegate,
                delegateQueue: nil
            )
        } else {
            // 无 Pinning 配置的回退逻辑
            let config = URLSessionConfiguration.default
            config.timeoutIntervalForRequest = 30
            config.timeoutIntervalForResource = 60
            self.session = URLSession(configuration: config)
            self.pinningDelegate = nil
        }
    }

    // MARK: - Pinning 配置更新

    /// 更新 TLS Pinning 配置（运行时热更新公钥哈希）
    /// - Parameter config: 新的 Pinning 配置
    ///
    /// 注意：此操作会重新创建 URLSession，当前正在执行的请求不受影响。
    func updatePinningConfig(_ config: TLSPinningConfig) {
        tlsConfig = config

        if config.pinnedHosts.isEmpty {
            // 清空 Pinning，创建普通 Session
            let sessionConfig = URLSessionConfiguration.default
            sessionConfig.timeoutIntervalForRequest = 30
            sessionConfig.timeoutIntervalForResource = 60
            session = URLSession(configuration: sessionConfig)
            pinningDelegate = nil
        } else {
            let strategies = config.pinnedHosts.mapValues { hashes -> PinningStrategy in
                return .publicKey(hashes: hashes)
            }
            let delegate = TlsPinningDelegate(
                pinnedHosts: strategies,
                isDebug: config.isDebug
            )
            delegate.onPinningFailed = { [weak self] errorInfo in
                self?.handlePinningFailed(errorInfo: errorInfo)
            }
            pinningDelegate = delegate

            let sessionConfig = URLSessionConfiguration.default
            sessionConfig.timeoutIntervalForRequest = 30
            sessionConfig.timeoutIntervalForResource = 60
            session = URLSession(
                configuration: sessionConfig,
                delegate: delegate,
                delegateQueue: nil
            )
        }
    }

    /// 动态更新 Pinning 公钥哈希
    /// - Parameters:
    ///   - host: 目标服务器域名
    ///   - hashes: 新的公钥 SHA-256 Base64 哈希列表
    func updatePinningHashes(host: String, hashes: [String]) {
        var updated = tlsConfig ?? TLSPinningConfig(pinnedHosts: [:])
        var hosts = updated.pinnedHosts
        hosts[host] = hashes
        updated = TLSPinningConfig(
            pinnedHosts: hosts,
            isDebug: updated.isDebug
        )
        updatePinningConfig(updated)
    }

    // MARK: - 请求构造

    /// 构造完整的 URLRequest
    func buildRequest(_ apiRequest: APIRequest) throws -> URLRequest {
        guard var components = URLComponents(string: baseURL + apiRequest.path) else {
            throw APIError.invalidURL
        }

        if !apiRequest.queryItems.isEmpty {
            components.queryItems = apiRequest.queryItems
        }

        guard let url = components.url else {
            throw APIError.invalidURL
        }

        var request = URLRequest(url: url)
        request.httpMethod = apiRequest.method.rawValue
        request.httpBody = apiRequest.body
        request.timeoutInterval = 30

        // 合并默认 headers 和请求级 headers（请求级覆盖默认）
        let mergedHeaders = defaultHeaders.merging(apiRequest.headers) { _, new in new }
        for (key, value) in mergedHeaders {
            request.setValue(value, forHTTPHeaderField: key)
        }

        return request
    }

    // MARK: - 请求执行

    /// 执行请求并解析 JSON 响应
    @discardableResult
    func perform<T: Decodable>(
        _ apiRequest: APIRequest,
        decoder: JSONDecoder? = nil,
        completion: @escaping (Result<T, Error>) -> Void
    ) -> URLSessionTask? {
        let request: URLRequest
        do {
            request = try buildRequest(apiRequest)
        } catch {
            completion(.failure(error))
            return nil
        }

        let task = session.dataTask(with: request) { data, response, error in
            self.handleResponse(data: data, response: response, error: error, decoder: decoder, completion: completion)
        }
        task.resume()
        return task
    }

    /// 执行请求，不解析响应（仅验证状态码）
    @discardableResult
    func performRaw(
        _ apiRequest: APIRequest,
        completion: @escaping (Result<Void, Error>) -> Void
    ) -> URLSessionTask? {
        let request: URLRequest
        do {
            request = try buildRequest(apiRequest)
        } catch {
            completion(.failure(error))
            return nil
        }

        let task = session.dataTask(with: request) { data, response, error in
            if let error = error {
                completion(.failure(self.mapNetworkError(error)))
                return
            }

            guard let httpResponse = response as? HTTPURLResponse else {
                completion(.failure(APIError.invalidResponse))
                return
            }

            switch httpResponse.statusCode {
            case 200...299:
                completion(.success(()))
            case 401:
                completion(.failure(APIError.unauthorized))
            case 404:
                completion(.failure(APIError.notFound))
            case 500...599:
                let message = data.flatMap { try? JSONDecoder().decode(ErrorBody.self, from: $0) }?.message
                completion(.failure(APIError.serverError(message)))
            default:
                let message = data.flatMap { try? JSONDecoder().decode(ErrorBody.self, from: $0) }?.message
                completion(.failure(APIError.httpError(statusCode: httpResponse.statusCode, message: message)))
            }
        }
        task.resume()
        return task
    }

    // MARK: - 响应处理

    private func handleResponse<T: Decodable>(
        data: Data?,
        response: URLResponse?,
        error: Error?,
        decoder: JSONDecoder?,
        completion: @escaping (Result<T, Error>) -> Void
    ) {
        if let error = error {
            completion(.failure(mapNetworkError(error)))
            return
        }

        guard let httpResponse = response as? HTTPURLResponse else {
            completion(.failure(APIError.invalidResponse))
            return
        }

        guard let data = data else {
            completion(.failure(APIError.invalidResponse))
            return
        }

        switch httpResponse.statusCode {
        case 200...299:
            let jsonDecoder = decoder ?? JSONDecoder()
            // 配置默认的日期解码策略
            jsonDecoder.keyDecodingStrategy = .convertFromSnakeCase
            do {
                let decoded = try jsonDecoder.decode(T.self, from: data)
                completion(.success(decoded))
            } catch {
                completion(.failure(APIError.decodingError(error.localizedDescription)))
            }
        case 401:
            completion(.failure(APIError.unauthorized))
        case 404:
            completion(.failure(APIError.notFound))
        case 500...599:
            let message = (try? JSONDecoder().decode(ErrorBody.self, from: data))?.message
            completion(.failure(APIError.serverError(message)))
        default:
            let message = (try? JSONDecoder().decode(ErrorBody.self, from: data))?.message
            completion(.failure(APIError.httpError(statusCode: httpResponse.statusCode, message: message)))
        }
    }

    // MARK: - TLS Pinning 处理

    /// 处理 Pinning 失败事件（日志 + 回调）
    private func handlePinningFailed(errorInfo: PinningErrorInfo) {
        NSLog("[APIClient] TLS Pinning 失败: \(errorInfo.host) - \(errorInfo.reason)")

        // 在 Debug 模式下打印详细日志
        #if DEBUG
        NSLog("[APIClient] Pinning Detail: strategy=\(errorInfo.strategy), leaf=\(errorInfo.leafCertificateSummary ?? "N/A")")
        #endif

        // 通知观察者（可在应用层添加 UI 提示或上报）
        NotificationCenter.default.post(
            name: .tlsPinningFailed,
            object: nil,
            userInfo: [
                "host": errorInfo.host,
                "reason": errorInfo.reason,
                "timestamp": Date()
            ]
        )
    }

    // MARK: - Helper

    private func mapNetworkError(_ error: Error) -> Error {
        // 检查是否是 TLS Pinning 相关的错误
        if let pinningError = error as? PinningError {
            return APIError.tlsPinningFailed(
                host: pinningError.host,
                reason: pinningError.reason
            )
        }

        let nsError = error as NSError
        switch nsError.code {
        case NSURLErrorTimedOut:
            return APIError.timeout
        case NSURLErrorNotConnectedToInternet:
            return APIError.networkError("无网络连接")
        case NSURLErrorNetworkConnectionLost:
            return APIError.networkError("网络连接断开")
        case NSURLErrorServerCertificateUntrusted,
             NSURLErrorServerCertificateHasBadDate,
             NSURLErrorServerCertificateNotYetValid,
             NSURLErrorServerCertificateHasUnknownRoot:
            let host = nsError.userInfo[NSURLErrorFailingURLStringErrorKey] as? String ?? "unknown"
            return APIError.tlsPinningFailed(host: host, reason: nsError.localizedDescription)
        default:
            return APIError.networkError(nsError.localizedDescription)
        }
    }
}

/// Pinning 错误（用于桥接底层 Pinning 错误到 APIError）
struct PinningError: Error {
    let host: String
    let reason: String
}

/// 错误响应的 body 结构
private struct ErrorBody: Decodable {
    let message: String?
}

// MARK: - Notification 扩展

extension Notification.Name {
    /// TLS Pinning 失败通知
    static let tlsPinningFailed = Notification.Name("cn.digitalkey.tlsPinningFailed")
}

// MARK: - JSON 编码工具

extension Encodable {
    func toJSONData() -> Data? {
        let encoder = JSONEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        return try? encoder.encode(self)
    }
}
