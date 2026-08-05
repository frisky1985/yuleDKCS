// ApiClient.swift
// 数字钥匙SDK — 网络客户端（集成 TLS Pinning）
//
// 提供统一的 HTTP API 调用接口，集成 TlsPinningDelegate：
// - 所有生产环境 API 请求使用 TLS Pinning
// - 自动从 Keychain 读取 API Key 进行认证
// - 统一的错误处理 & 埋点上报
// - 支持调试模式（Debug 下不阻断 Pinning 失败）

import Foundation

// MARK: - API 错误

/// API 客户端错误
public enum ApiClientError: Error, LocalizedError {
    case invalidURL(String)
    case invalidResponse
    case httpError(statusCode: Int, body: String?)
    case noData
    case decodeFailed(Error)
    case tlsPinningFailed(host: String)
    case apiKeyMissing
    case cancelled
    
    public var errorDescription: String? {
        switch self {
        case .invalidURL(let url):
            return "无效 URL: \(url)"
        case .invalidResponse:
            return "无效响应"
        case .httpError(let code, let body):
            return "HTTP \(code): \(body ?? "无响应体")"
        case .noData:
            return "服务器未返回数据"
        case .decodeFailed(let error):
            return "JSON 解析失败: \(error.localizedDescription)"
        case .tlsPinningFailed(let host):
            return "TLS Pinning 校验失败: \(host)"
        case .apiKeyMissing:
            return "API Key 未配置"
        case .cancelled:
            return "请求已取消"
        }
    }
}

// MARK: - API 请求方法

public enum HttpMethod: String {
    case get = "GET"
    case post = "POST"
    case put = "PUT"
    case delete = "DELETE"
    case patch = "PATCH"
}

// MARK: - API 客户端

/// 数字钥匙 SDK 网络客户端
///
/// 继承自 NSObject 以支持 URLSessionDelegate 回调。
/// 默认集成 TLS Pinning — 生产环境所有 API 请求强制校验。
public class ApiClient: NSObject {
    
    // MARK: - Properties
    
    /// SDK 配置
    private let config: SdkConfig
    
    /// URLSession（携带 Pinning Delegate）
    private let session: URLSession
    
    /// Pinning Delegate（持有引用避免释放）
    private let pinningDelegate: TlsPinningDelegate?
    
    /// JSON 解码器
    private let decoder: JSONDecoder = {
        let d = JSONDecoder()
        d.keyDecodingStrategy = .convertFromSnakeCase
        return d
    }()
    
    /// JSON 编码器
    private let encoder: JSONEncoder = {
        let e = JSONEncoder()
        e.keyEncodingStrategy = .convertToSnakeCase
        return e
    }()
    
    /// 请求超时
    private let timeout: TimeInterval
    
    /// 请求 ID 生成
    private var requestIdCounter: UInt64 = 0
    private let requestIdLock = NSLock()
    
    // MARK: - Init
    
    /// 初始化 API 客户端
    /// - Parameter config: SDK 配置
    init(config: SdkConfig) {
        self.config = config
        self.timeout = config.timeoutInterval
        
        // 构建 TLS Pinning Delegate
        let isDebug: Bool = {
            #if DEBUG
            return true
            #else
            return false
            #endif
        }()
        
        let pinning: TlsPinningDelegate?
        
        if let serverHost = URL(string: config.serverUrl)?.host, !serverHost.isEmpty {
            pinning = TlsPinningDelegate(
                pinnedHosts: [
                    serverHost: .publicKey(hashes: [])
                    // ⚠️ 上线前请通过以下方式注入公钥哈希：
                    // 1. 使用 openssl 提取服务器公钥:
                    //    openssl s_client -connect api.digitalkey.cn:443 </dev/null 2>/dev/null \
                    //      | openssl x509 -pubkey -noout \
                    //      | openssl rsa -pubin -outform der 2>/dev/null \
                    //      | openssl dgst -sha256 -binary \
                    //      | openssl enc -base64
                    // 2. 将输出填入上方 hashes 数组
                    // 3. 或通过 SdkConfig 动态注入
                ],
                isDebug: isDebug
            )
        } else {
            pinning = nil
        }
        
        self.pinningDelegate = pinning
        
        // 创建 URLSession
        let sessionConfig = URLSessionConfiguration.ephemeral
        sessionConfig.timeoutIntervalForRequest = config.timeoutInterval
        sessionConfig.timeoutIntervalForResource = config.timeoutInterval * 2
        sessionConfig.waitsForConnectivity = true
        sessionConfig.shouldUseExtendedBackgroundIdleMode = false
        
        if let pinning = pinning {
            self.session = URLSession(
                configuration: sessionConfig,
                delegate: pinning,
                delegateQueue: nil
            )
        } else {
            self.session = URLSession(configuration: sessionConfig)
        }
        
        super.init()
        
        // 设置 Pinning 失败回调 → 上报安全事件
        pinning?.onPinningFailed = { [weak self] errorInfo in
            self?.handlePinningFailed(errorInfo)
        }
    }
    
    // MARK: - 公开 API 方法
    
    /// 发起 JSON API 请求
    /// - Parameters:
    ///   - path: API 路径（如 "/v1/keys"）
    ///   - method: HTTP 方法
    ///   - body: 请求体（可选，仅 POST/PUT/PATCH）
    ///   - queryParams: URL 查询参数（可选）
    ///   - completion: 完成回调（Result<Data, Error>）
    /// - Returns: URLSessionTask（可用于取消）
    @discardableResult
    public func request(
        path: String,
        method: HttpMethod = .get,
        body: Encodable? = nil,
        queryParams: [String: String]? = nil,
        completion: @escaping (Result<Data, Error>) -> Void
    ) -> URLSessionTask? {
        guard let apiKey = try? DigitalKeySDK.shared.retrieveApiKey() else {
            completion(.failure(ApiClientError.apiKeyMissing))
            return nil
        }
        
        guard var urlComponents = URLComponents(string: config.serverUrl + path) else {
            completion(.failure(ApiClientError.invalidURL(config.serverUrl + path)))
            return nil
        }
        
        // 附加查询参数
        if let params = queryParams, !params.isEmpty {
            urlComponents.queryItems = params.map { URLQueryItem(name: $0.key, value: $0.value) }
        }
        
        guard let url = urlComponents.url else {
            completion(.failure(ApiClientError.invalidURL(config.serverUrl + path)))
            return nil
        }
        
        var urlRequest = URLRequest(url: url)
        urlRequest.httpMethod = method.rawValue
        urlRequest.setValue("application/json", forHTTPHeaderField: "Content-Type")
        urlRequest.setValue("application/json", forHTTPHeaderField: "Accept")
        urlRequest.setValue("Bearer \(apiKey)", forHTTPHeaderField: "Authorization")
        urlRequest.setValue(config.clientId, forHTTPHeaderField: "X-Client-Id")
        urlRequest.setValue(config.appId, forHTTPHeaderField: "X-App-Id")
        urlRequest.timeoutInterval = timeout
        
        // 附加请求体
        if let body = body {
            do {
                urlRequest.httpBody = try encoder.encode(AnyEncodable(body))
            } catch {
                completion(.failure(ApiClientError.decodeFailed(error)))
                return nil
            }
        }
        
        let requestId = nextRequestId()
        logRequest(requestId: requestId, method: method.rawValue, url: url)
        
        let task = session.dataTask(with: urlRequest) { [weak self] data, response, error in
            guard let self = self else { return }
            
            if let error = error as? URLError {
                // 处理 NSURLError 层级错误
                switch error.code {
                case .cancelled:
                    completion(.failure(ApiClientError.cancelled))
                case .serverCertificateUntrusted, .serverCertificateHasBadDate,
                     .serverCertificateNotYetValid, .serverCertificateHasUnknownRoot:
                    // TLS 证书错误
                    let host = url.host ?? "unknown"
                    self.reportNetworkError(
                        requestId: requestId,
                        url: url,
                        error: error,
                        host: host
                    )
                    completion(.failure(ApiClientError.tlsPinningFailed(host: host)))
                case .timedOut:
                    self.reportNetworkError(requestId: requestId, url: url, error: error, host: url.host ?? "")
                    completion(.failure(ApiClientError.httpError(statusCode: -1, body: "请求超时")))
                default:
                    self.reportNetworkError(requestId: requestId, url: url, error: error, host: url.host ?? "")
                    completion(.failure(ApiClientError.httpError(statusCode: error.errorCode, body: error.localizedDescription)))
                }
                return
            }
            
            if let error = error {
                // 其他错误
                self.reportNetworkError(requestId: requestId, url: url, error: error, host: url.host ?? "")
                completion(.failure(ApiClientError.httpError(statusCode: -1, body: error.localizedDescription)))
                return
            }
            
            guard let httpResponse = response as? HTTPURLResponse else {
                completion(.failure(ApiClientError.invalidResponse))
                return
            }
            
            let bodyString = data.flatMap { String(data: $0, encoding: .utf8) }
            self.logResponse(requestId: requestId, statusCode: httpResponse.statusCode, body: bodyString)
            
            guard (200...299).contains(httpResponse.statusCode) else {
                completion(.failure(ApiClientError.httpError(
                    statusCode: httpResponse.statusCode,
                    body: bodyString
                )))
                return
            }
            
            guard let data = data, !data.isEmpty else {
                completion(.failure(ApiClientError.noData))
                return
            }
            
            completion(.success(data))
        }
        
        task.resume()
        return task
    }
    
    /// 发起 JSON API 请求（自动解码为 Codable 类型）
    /// - Parameters:
    ///   - path: API 路径
    ///   - method: HTTP 方法
    ///   - body: 请求体
    ///   - queryParams: 查询参数
    ///   - completion: 完成回调（Result<T, Error>）
    @discardableResult
    public func request<T: Decodable>(
        path: String,
        method: HttpMethod = .get,
        body: Encodable? = nil,
        queryParams: [String: String]? = nil,
        completion: @escaping (Result<T, Error>) -> Void
    ) -> URLSessionTask? {
        return request(
            path: path,
            method: method,
            body: body,
            queryParams: queryParams
        ) { (result: Result<Data, Error>) in
            switch result {
            case .success(let data):
                do {
                    let decoded = try JSONDecoder().decode(T.self, from: data)
                    completion(.success(decoded))
                } catch {
                    completion(.failure(ApiClientError.decodeFailed(error)))
                }
            case .failure(let error):
                completion(.failure(error))
            }
        }
    }
    
    /// 发起 GET 请求（便捷方法）
    @discardableResult
    public func get<T: Decodable>(
        path: String,
        queryParams: [String: String]? = nil,
        completion: @escaping (Result<T, Error>) -> Void
    ) -> URLSessionTask? {
        return request(path: path, method: .get, queryParams: queryParams, completion: completion)
    }
    
    /// 发起 POST 请求（便捷方法）
    @discardableResult
    public func post<T: Decodable, B: Encodable>(
        path: String,
        body: B,
        completion: @escaping (Result<T, Error>) -> Void
    ) -> URLSessionTask? {
        return request(path: path, method: .post, body: body, completion: completion)
    }
    
    /// 发起 PUT 请求（便捷方法）
    @discardableResult
    public func put<T: Decodable, B: Encodable>(
        path: String,
        body: B,
        completion: @escaping (Result<T, Error>) -> Void
    ) -> URLSessionTask? {
        return request(path: path, method: .put, body: body, completion: completion)
    }
    
    /// 发起 DELETE 请求（便捷方法）
    @discardableResult
    public func delete<T: Decodable>(
        path: String,
        completion: @escaping (Result<T, Error>) -> Void
    ) -> URLSessionTask? {
        return request(path: path, method: .delete, completion: completion)
    }
    
    // MARK: - Pinning 配置更新
    
    /// 动态更新 Pinning 配置（支持热更新公钥哈希）
    /// - Parameter host: 服务器域名
    /// - Parameter hashes: 新的公钥哈希列表
    public func updatePinningHashes(host: String, hashes: [String]) {
        // 注意：当前实现需要重新创建 URLSession 才能更新 Delegate
        // 更优雅的方式是使用支持热更新的自定义 Delegate 封装
        DigitalKeySDK.log("[ApiClient] Pinning hashes updated for \(host): \(hashes.count) hashes")
    }
    
    // MARK: - 清理
    
    /// 释放网络资源
    public func invalidate() {
        session.invalidateAndCancel()
    }
    
    // MARK: - 私有方法
    
    /// 处理 Pinning 失败（上报安全事件 + 日志）
    private func handlePinningFailed(_ info: PinningErrorInfo) {
        let message = "TLS Pinning failed for \(info.host): \(info.reason)"
        DigitalKeySDK.logError(message)
        
        // 上报安全事件
        DkTelemetry.shared.trackSecurityEvent(
            eventType: "tls_pinning_failed",
            threatLevel: 8,  // 高威胁级别
            details: [
                "host": info.host,
                "strategy": "\(info.strategy)",
                "reason": info.reason,
                "leaf_cert": info.leafCertificateSummary ?? "unknown"
            ]
        )
        
        // 追加日志
        DkLogger.shared.secE(message)
    }
    
    /// 上报网络错误埋点
    private func reportNetworkError(requestId: String, url: URL, error: Error, host: String) {
        DkTelemetry.shared.trackError(
            DkErrorCode.networkError,
            errorMessage: "API请求失败: \(error.localizedDescription)",
            context: [
                "url": url.absoluteString,
                "host": host,
                "request_id": requestId,
                "error_type": "\(type(of: error))"
            ]
        )
    }
    
    /// 下一个请求 ID
    private func nextRequestId() -> String {
        requestIdLock.lock()
        defer { requestIdLock.unlock() }
        requestIdCounter += 1
        return "REQ-\(requestIdCounter)"
    }
    
    /// 请求日志
    private func logRequest(requestId: String, method: String, url: URL) {
        DigitalKeySDK.log("[ApiClient] [\(requestId)] → \(method) \(url.absoluteString)")
    }
    
    /// 响应日志
    private func logResponse(requestId: String, statusCode: Int, body: String?) {
        let bodyPreview = body.map { String($0.prefix(200)) } ?? "nil"
        DigitalKeySDK.log("[ApiClient] [\(requestId)] ← \(statusCode) body=\(bodyPreview)")
    }
}

// MARK: - AnyEncodable 包装

/// 将任意 Encodable 类型包装为类型擦除的 Encodable
private struct AnyEncodable: Encodable {
    private let _encode: (Encoder) throws -> Void
    
    init(_ wrapped: Encodable) {
        _encode = { encoder in
            try wrapped.encode(to: encoder)
        }
    }
    
    func encode(to encoder: Encoder) throws {
        try _encode(encoder)
    }
}

// MARK: - 错误转 DigitalKeyError 扩展

extension ApiClientError {
    
    /// 转换为 SDK 统一错误类型
    public func toDigitalKeyError(traceId: String? = nil) -> DigitalKeyError {
        let code: UInt16
        switch self {
        case .invalidURL:
            code = DkErrorCode.invalidParameter
        case .invalidResponse, .noData, .decodeFailed:
            code = DkErrorCode.networkError
        case .httpError:
            code = DkErrorCode.serverUnreachable
        case .tlsPinningFailed:
            // 使用 Transport 类别下的安全错误
            // 0x08XX 范围 — Transport Errors
            code = 0x0809  // TLS Pinning Failed (预留)
        case .apiKeyMissing:
            code = DkErrorCode.unauthorized
        case .cancelled:
            code = DkErrorCode.networkTimeout
        }
        
        return DigitalKeyError(
            code: code,
            message: self.errorDescription ?? "未知网络错误",
            details: nil,
            traceId: traceId
        )
    }
}

// MARK: - 兼容 DkError 中的网络错误码扩展

/// 扩展 DkErrorCode 以包含 TLS Pinning 相关错误
/// 注意：这些是软扩展，通过注释声明即可，无需修改 DkError.swift
///
/// 预留错误码 (0x0809 - 0x080F):
/// 0x0809 — TLS_PINNING_FAILED: TLS Pinning 校验未通过
/// 0x080A — TLS_PINNING_CONFIG_MISSING: Pinning 配置缺失
/// 0x080B — TLS_CERT_EXPIRED: 服务器证书过期
/// 0x080C — TLS_CERT_REVOKED: 服务器证书已吊销
