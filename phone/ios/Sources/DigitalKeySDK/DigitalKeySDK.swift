// DigitalKeySDK.swift
// 数字钥匙SDK主入口
// 版本: 1.0.0

import Foundation
import Combine

// MARK: - SDK统一错误类型

/// 数字钥匙SDK统一错误枚举
public enum DigitalKeyError: Error, LocalizedError {
    /// 网络错误
    case network(code: Int, message: String)
    /// 认证错误
    case auth(code: Int, message: String)
    /// 密钥错误
    case key(code: Int, message: String)
    /// 车辆错误
    case vehicle(code: Int, message: String)
    /// 硬件错误
    case hardware(message: String)
    /// SDK未初始化
    case notConfigured
    /// 参数错误
    case invalidParameter(message: String)
    /// 超时
    case timeout(message: String)

    public var errorDescription: String? {
        switch self {
        case .network(let code, let msg): return "[网络错误 \(code)] \(msg)"
        case .auth(let code, let msg): return "[认证错误 \(code)] \(msg)"
        case .key(let code, let msg): return "[密钥错误 \(code)] \(msg)"
        case .vehicle(let code, let msg): return "[车辆错误 \(code)] \(msg)"
        case .hardware(let msg): return "[硬件错误] \(msg)"
        case .notConfigured: return "SDK未初始化，请先调用 DigitalKeySDK.configure()"
        case .invalidParameter(let msg): return "[参数错误] \(msg)"
        case .timeout(let msg): return "[超时] \(msg)"
        }
    }
}

// MARK: - SDK配置

/// SDK配置信息
public struct SdkConfig {
    /// 服务器地址
    public let serverUrl: String
    /// 应用标识
    public let appId: String
    /// 客户端ID
    public let clientId: String
    /// API Key
    public let apiKey: String
    /// 是否启用日志
    public let enableLog: Bool
    /// 请求超时时间（秒）
    public let timeoutInterval: TimeInterval

    public init(
        serverUrl: String,
        appId: String,
        clientId: String,
        apiKey: String,
        enableLog: Bool = false,
        timeoutInterval: TimeInterval = 30.0
    ) {
        self.serverUrl = serverUrl
        self.appId = appId
        self.clientId = clientId
        self.apiKey = apiKey
        self.enableLog = enableLog
        self.timeoutInterval = timeoutInterval
    }
}

// MARK: - SDK主类

/// 数字钥匙SDK主入口
/// 使用单例模式，通过 configure() 初始化
public class DigitalKeySDK {

    // MARK: - 单例

    /// 共享实例
    public static var shared: DigitalKeySDK {
        guard let instance = _shared else {
            fatalError("DigitalKeySDK 未配置，请先调用 DigitalKeySDK.configure()")
        }
        return instance
    }

    private static var _shared: DigitalKeySDK?

    /// SDK是否已配置
    public static var isConfigured: Bool {
        return _shared != nil
    }

    // MARK: - 属性

    /// SDK配置
    public private(set) var config: SdkConfig

    /// 密钥管理器
    public lazy var keyManager: KeyManaging = KeyManager(sdk: self)

    /// 车辆控制器
    public lazy var vehicleController: VehicleControlling = VehicleController(sdk: self)

    /// 分享管理器
    public lazy var shareManager: ShareManaging = ShareManager(sdk: self)

    /// 通道管理器
    public lazy var channelManager: ChannelManaging = ChannelManager(sdk: self)

    /// 安全模块
    public lazy var securityModule: SecurityManaging = SecurityModule(sdk: self)

    /// 网络客户端
    lazy var apiClient: ApiClient = ApiClient(config: config)

    /// 撤销信号集合
    private var cancellables = Set<AnyCancellable>()

    // MARK: - 初始化

    /// 配置SDK（仅可调用一次）
    /// - Parameters:
    ///   - serverUrl: 服务器地址，例如 "https://api.digitalkey.cn"
    ///   - appId: 应用标识，例如 "com.example.app"
    ///   - clientId: 客户端ID，例如 "ios_xxxxx"
    ///   - apiKey: API密钥
    /// - Throws: 配置失败时抛出异常
    public static func configure(
        serverUrl: String,
        appId: String,
        clientId: String,
        apiKey: String
    ) throws {
        try configure(config: SdkConfig(
            serverUrl: serverUrl,
            appId: appId,
            clientId: clientId,
            apiKey: apiKey
        ))
    }

    /// 配置SDK（完整配置）
    /// - Parameter config: SDK配置
    /// - Throws: 配置失败时抛出异常
    public static func configure(config: SdkConfig) throws {
        // 验证必填参数
        guard !config.serverUrl.isEmpty else {
            throw DigitalKeyError.invalidParameter(message: "serverUrl 不能为空")
        }
        guard !config.appId.isEmpty else {
            throw DigitalKeyError.invalidParameter(message: "appId 不能为空")
        }
        guard !config.clientId.isEmpty else {
            throw DigitalKeyError.invalidParameter(message: "clientId 不能为空")
        }
        guard !config.apiKey.isEmpty else {
            throw DigitalKeyError.invalidParameter(message: "apiKey 不能为空")
        }

        // 验证URL格式
        guard URL(string: config.serverUrl) != nil else {
            throw DigitalKeyError.invalidParameter(message: "serverUrl 格式不合法")
        }

        _shared = DigitalKeySDK(config: config)
        log("SDK配置完成: \(config.serverUrl), appId=\(config.appId)")
    }

    /// 重置SDK（用于测试或切换账号）
    public static func reset() {
        _shared = nil
        log("SDK已重置")
    }

    private init(config: SdkConfig) {
        self.config = config
    }

    // MARK: - 日志

    /// 内部日志输出
    static func log(_ message: String) {
        guard let sdk = _shared, sdk.config.enableLog else { return }
        print("[DigitalKeySDK] \(message)")
    }

    /// 内部错误日志输出
    static func logError(_ message: String) {
        guard let sdk = _shared, sdk.config.enableLog else { return }
        print("[DigitalKeySDK] ❌ \(message)")
    }
}
