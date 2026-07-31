import Foundation

/// yuleDKCS SDK 错误类型
public enum YDKError: Error, LocalizedError {
    case notInitialized
    case notAuthenticated
    case hubError(String, String)   // (errorCode, errorMsg)
    case httpError(Int)             // HTTP 状态码错误 (4xx/5xx)
    case networkError(Error)
    case timeout
    case decodingFailed(String)     // JSON 解析失败
    case internal_(String)

    public var errorDescription: String? {
        switch self {
        case .notInitialized:        return "SDK 未初始化"
        case .notAuthenticated:      return "未登录，请先调用 setToken()"
        case .hubError(let code, let msg): return "[\(code)] \(msg)"
        case .httpError(let code):   return "HTTP 错误: \(code)"
        case .networkError(let e):   return "网络错误: \(e.localizedDescription)"
        case .timeout:               return "请求超时"
        case .decodingFailed(let d): return "JSON 解析失败: \(d)"
        case .internal_(let msg):    return "SDK 内部错误: \(msg)"
        }
    }
}

// MARK: - Hub REST Gateway 错误响应格式
public struct HubErrorResponse: Decodable {
    public let error: String?
    public let message: String?
    public let code: String?
}
