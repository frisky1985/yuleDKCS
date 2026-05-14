import Foundation
import Combine

/// API 服务协议
public protocol APIServiceProtocol {
    func login(username: String, password: String) -> AnyPublisher<LoginResponse, Error>
    func register(username: String, email: String, password: String) -> AnyPublisher<AuthResponse, Error>
    func getKeys(page: Int?, pageSize: Int?) -> AnyPublisher<KeysListResponse, Error>
    func getKeyDetail(keyId: String) -> AnyPublisher<DigitalKey, Error>
    func activateKey(keyId: String) -> AnyPublisher<DigitalKey, Error>
    func deactivateKey(keyId: String) -> AnyPublisher<DigitalKey, Error>
    func getKeyLogs(keyId: String, page: Int?, pageSize: Int?) -> AnyPublisher<LogsResponse, Error>
    func shareKey(keyId: String, request: ShareKeyRequest) -> AnyPublisher<ShareResponse, Error>
    func revokeKey(keyId: String) -> AnyPublisher<Void, Error>
}

/// API 服务实现
public class APIService: APIServiceProtocol {
    
    // MARK: - Singleton
    public static let shared = APIService()
    
    // MARK: - Properties
    private let baseURL: URL
    private let session: URLSession
    private var authToken: String?
    private let tokenKey = "com.yuledkcs.authToken"
    
    // MARK: - Initialization
    private init() {
        // 开发环境使用 localhost
        #if DEBUG
        self.baseURL = URL(string: "http://localhost:8080/api/v1")!
        #else
        self.baseURL = URL(string: "https://api.yuledkcs.com/api/v1")!
        #endif
        
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30
        config.timeoutIntervalForResource = 300
        self.session = URLSession(configuration: config)
        
        // 从 Keychain 读取 token
        self.authToken = KeychainHelper.shared.read(key: tokenKey)
    }
    
    // MARK: - Auth Token Management
    
    public func setAuthToken(_ token: String) {
        self.authToken = token
        KeychainHelper.shared.save(key: tokenKey, value: token)
    }
    
    public func clearAuthToken() {
        self.authToken = nil
        KeychainHelper.shared.delete(key: tokenKey)
    }
    
    public func isAuthenticated() -> Bool {
        return authToken != nil
    }
    
    // MARK: - API Methods
    
    public func login(username: String, password: String) -> AnyPublisher<LoginResponse, Error> {
        let request = LoginRequest(username: username, password: password)
        return performRequest(endpoint: "/auth/login", method: "POST", body: request)
            .handleEvents(receiveOutput: { [weak self] response in
                if let token = response.data?.token {
                    self?.setAuthToken(token)
                }
            })
            .eraseToAnyPublisher()
    }
    
    public func register(username: String, email: String, password: String) -> AnyPublisher<AuthResponse, Error> {
        let request = RegisterRequest(username: username, email: email, password: password)
        return performRequest(endpoint: "/auth/register", method: "POST", body: request)
    }
    
    public func getKeys(page: Int? = nil, pageSize: Int? = nil) -> AnyPublisher<KeysListResponse, Error> {
        var queryItems: [URLQueryItem] = []
        if let page = page {
            queryItems.append(URLQueryItem(name: "page", value: String(page)))
        }
        if let pageSize = pageSize {
            queryItems.append(URLQueryItem(name: "page_size", value: String(pageSize)))
        }
        return performRequest(endpoint: "/keys", method: "GET", queryItems: queryItems)
    }
    
    public func getKeyDetail(keyId: String) -> AnyPublisher<DigitalKey, Error> {
        return performRequest(endpoint: "/keys/\(keyId)", method: "GET")
    }
    
    public func activateKey(keyId: String) -> AnyPublisher<DigitalKey, Error> {
        return performRequest(endpoint: "/keys/\(keyId)/activate", method: "POST")
    }
    
    public func deactivateKey(keyId: String) -> AnyPublisher<DigitalKey, Error> {
        return performRequest(endpoint: "/keys/\(keyId)/deactivate", method: "POST")
    }
    
    public func getKeyLogs(keyId: String, page: Int? = nil, pageSize: Int? = nil) -> AnyPublisher<LogsResponse, Error> {
        var queryItems: [URLQueryItem] = []
        if let page = page {
            queryItems.append(URLQueryItem(name: "page", value: String(page)))
        }
        if let pageSize = pageSize {
            queryItems.append(URLQueryItem(name: "page_size", value: String(pageSize)))
        }
        return performRequest(endpoint: "/keys/\(keyId)/logs", method: "GET", queryItems: queryItems)
    }
    
    public func shareKey(keyId: String, request: ShareKeyRequest) -> AnyPublisher<ShareResponse, Error> {
        return performRequest(endpoint: "/keys/\(keyId)/share", method: "POST", body: request)
    }
    
    public func revokeKey(keyId: String) -> AnyPublisher<Void, Error> {
        return performRequest(endpoint: "/keys/\(keyId)", method: "DELETE")
            .map { (_: ApiResponse) in () }
            .eraseToAnyPublisher()
    }
    
    // MARK: - Private Methods
    
    private func performRequest<T: Decodable>(
        endpoint: String,
        method: String,
        queryItems: [URLQueryItem] = [],
        body: Encodable? = nil
    ) -> AnyPublisher<T, Error> {
        
        var urlComponents = URLComponents(url: baseURL.appendingPathComponent(endpoint), resolvingAgainstBaseURL: true)
        if !queryItems.isEmpty {
            urlComponents?.queryItems = queryItems
        }
        
        guard let url = urlComponents?.url else {
            return Fail(error: URLError(.badURL)).eraseToAnyPublisher()
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = method
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        // 添加认证头
        if let token = authToken {
            request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }
        
        // 添加请求体
        if let body = body {
            do {
                request.httpBody = try JSONEncoder().encode(body)
            } catch {
                return Fail(error: error).eraseToAnyPublisher()
            }
        }
        
        return session.dataTaskPublisher(for: request)
            .tryMap { data, response -> Data in
                guard let httpResponse = response as? HTTPURLResponse else {
                    throw URLError(.badServerResponse)
                }
                
                if httpResponse.statusCode == 401 {
                    throw APIError.unauthorized
                }
                
                guard (200...299).contains(httpResponse.statusCode) else {
                    throw APIError.httpError(statusCode: httpResponse.statusCode, data: data)
                }
                
                return data
            }
            .decode(type: T.self, decoder: JSONDecoder())
            .receive(on: DispatchQueue.main)
            .eraseToAnyPublisher()
    }
}

// MARK: - API Error

public enum APIError: Error {
    case unauthorized
    case httpError(statusCode: Int, data: Data)
    case decodingError(Error)
}

// MARK: - Request/Response Models

struct LoginRequest: Codable {
    let username: String
    let password: String
}

struct RegisterRequest: Codable {
    let username: String
    let email: String
    let password: String
}

struct ShareKeyRequest: Codable {
    let shared_to_username: String
    let expires_at: String?
    let permissions: [String: Bool]
}

public struct LoginResponse: Codable {
    public let code: Int
    public let message: String
    public let data: LoginData?
}

public struct LoginData: Codable {
    public let token: String
    public let type: String
    public let user: UserData
}

public struct AuthResponse: Codable {
    public let code: Int
    public let message: String
    public let data: UserData?
}

public struct UserData: Codable {
    public let id: String
    public let username: String
    public let email: String
    public let role: String
}

public struct KeysListResponse: Codable {
    public let code: Int
    public let message: String
    public let data: KeysData?
}

public struct KeysData: Codable {
    public let list: [DigitalKey]
    public let total: Int
    public let page: Int
    public let page_size: Int
}

public struct LogsResponse: Codable {
    public let code: Int
    public let message: String
    public let data: LogsData?
}

public struct LogsData: Codable {
    public let list: [KeyUsageLog]
    public let total: Int
    public let page: Int
    public let page_size: Int
}

public struct ShareResponse: Codable {
    public let code: Int
    public let message: String
    public let data: ShareData?
}

public struct ShareData: Codable {
    public let share_id: String
    public let qr_code_url: String
    public let share_link: String
    public let expires_at: String
}

struct ApiResponse: Codable {
    let code: Int
    let message: String
}

// MARK: - Keychain Helper

class KeychainHelper {
    static let shared = KeychainHelper()
    
    func save(key: String, value: String) {
        let data = value.data(using: .utf8)!
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: key,
            kSecValueData as String: data
        ]
        
        SecItemDelete(query as CFDictionary)
        SecItemAdd(query as CFDictionary, nil)
    }
    
    func read(key: String) -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: key,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        
        var result: AnyObject?
        SecItemCopyMatching(query as CFDictionary, &result)
        
        guard let data = result as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }
    
    func delete(key: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: key
        ]
        SecItemDelete(query as CFDictionary)
    }
}
