import Foundation

/// yuleDKCS SDK 错误类型
public enum YDKError: Error, LocalizedError {
    case notInitialized
    case notAuthenticated
    case hubError(String, String)   // (errorCode, errorMsg)
    case networkError(Error)
    case timeout
    case internal_(String)

    public var errorDescription: String? {
        switch self {
        case .notInitialized:        return "SDK 未初始化"
        case .notAuthenticated:      return "未登录，请先调用 setToken()"
        case .hubError(let code, let msg): return "[\(code)] \(msg)"
        case .networkError(let e):   return "网络错误: \(e.localizedDescription)"
        case .timeout:               return "请求超时"
        case .internal_(let msg):    return "SDK 内部错误: \(msg)"
        }
    }
}
