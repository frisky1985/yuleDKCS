import Foundation

/// YuleDKCS SDK 主入口
public final class YuleDKCS {
    
    // MARK: - Singleton
    
    /// SDK 共享实例
    public static let shared = YuleDKCS()
    
    // MARK: - Properties
    
    private var apiKey: String?
    private var isInitialized = false
    private let queue = DispatchQueue(label: "com.yuledkcs.sdk", qos: .userInitiated)
    
    /// SDK 版本号
    public static let version = "1.0.0"
    
    // MARK: - Managers
    
    /// 钥匙管理器
    public lazy var keyManager: KeyManager = {
        KeyManager.shared
    }()
    
    /// BLE 管理器
    public lazy var bleManager: BLEManager = {
        BLEManager.shared
    }()
    
    // MARK: - Initialization
    
    private init() {}
    
    // MARK: - Public Methods
    
    /// 初始化 SDK
    /// - Parameters:
    ///   - apiKey: API 密钥
    ///   - environment: 环境配置（可选）
    public func initialize(apiKey: String, environment: Environment = .production) {
        queue.sync {
            guard !isInitialized else {
                print("[YuleDKCS] SDK already initialized")
                return
            }
            
            self.apiKey = apiKey
            self.isInitialized = true
            
            // 初始化 FFI 桥接
            FFIBridge.shared.initialize()
            
            // 配置环境
            configureEnvironment(environment)
            
            print("[YuleDKCS] SDK initialized successfully (v\(YuleDKCS.version))")
        }
    }
    
    /// 检查 SDK 是否已初始化
    public func checkInitialized() throws {
        guard isInitialized else {
            throw YuleDKCSError.notInitialized
        }
    }
    
    /// 获取当前 API Key
    func getApiKey() -> String? {
        return apiKey
    }
    
    // MARK: - Private Methods
    
    private func configureEnvironment(_ environment: Environment) {
        switch environment {
        case .development:
            print("[YuleDKCS] Running in development mode")
        case .staging:
            print("[YuleDKCS] Running in staging mode")
        case .production:
            print("[YuleDKCS] Running in production mode")
        }
    }
}

// MARK: - Environment

extension YuleDKCS {
    public enum Environment {
        case development
        case staging
        case production
    }
}

// MARK: - Errors

public enum YuleDKCSError: Error, LocalizedError {
    case notInitialized
    case invalidApiKey
    case networkError(Error)
    case bleError(BLEError)
    case cryptoError(CryptoError)
    case keyNotFound
    case permissionDenied
    case invalidCommand
    case ffiError(String)
    
    public var errorDescription: String? {
        switch self {
        case .notInitialized:
            return "SDK 未初始化"
        case .invalidApiKey:
            return "无效的 API Key"
        case .networkError(let error):
            return "网络错误: \(error.localizedDescription)"
        case .bleError(let error):
            return "BLE 错误: \(error.localizedDescription)"
        case .cryptoError(let error):
            return "加密错误: \(error.localizedDescription)"
        case .keyNotFound:
            return "钥匙未找到"
        case .permissionDenied:
            return "权限被拒绝"
        case .invalidCommand:
            return "无效命令"
        case .ffiError(let msg):
            return "FFI 错误: \(msg)"
        }
    }
}