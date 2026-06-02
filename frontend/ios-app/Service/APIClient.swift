import Foundation

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

/// 基础 API 客户端
/// 封装 URLSession，统一处理请求构造、响应解析、错误处理
class APIClient {
    static let shared = APIClient()

    /// 可注入的 URLSession（测试时替换为 MockURLSession 配置）
    var session: URLSession

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

    init(session: URLSession? = nil) {
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30
        config.timeoutIntervalForResource = 60
        self.session = session ?? URLSession(configuration: config)
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

    // MARK: - Helper

    private func mapNetworkError(_ error: Error) -> Error {
        let nsError = error as NSError
        switch nsError.code {
        case NSURLErrorTimedOut:
            return APIError.timeout
        case NSURLErrorNotConnectedToInternet:
            return APIError.networkError("无网络连接")
        case NSURLErrorNetworkConnectionLost:
            return APIError.networkError("网络连接断开")
        default:
            return APIError.networkError(nsError.localizedDescription)
        }
    }
}

/// 错误响应的 body 结构
private struct ErrorBody: Decodable {
    let message: String?
}

// MARK: - JSON 编码工具

extension Encodable {
    func toJSONData() -> Data? {
        let encoder = JSONEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        return try? encoder.encode(self)
    }
}
