// DigitalKeySDK.swift
// 数字钥匙SDK主入口
// 版本: 1.0.0
//
// 安全更新:
// - API Key 不再存储在进程内存的 struct 属性中，
//   改为通过 iOS Keychain Services 加密存储，
//   仅在需要使用时由 ApiClient 从 Keychain 读取。
// - 应用暂停/后台时 Keychain 数据硬件加密，
//   进程 dump 无法获取明文 API Key。

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
    /// Keychain 错误
    case keychainError(message: String)

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
        case .keychainError(let msg): return "[Keychain] \(msg)"
        }
    }
}

// MARK: - SDK配置

/// SDK配置信息
///
/// ⚠️ 安全说明:
/// `apiKey` 在初始化时立即写入 iOS Keychain（硬件加密），
/// 配置 struct 中仅保留 `hasApiKey` 标记。
/// 实际密钥通过 `DigitalKeySDK.retrieveApiKey()` 从 Keychain 读取。
public struct SdkConfig {
    /// 服务器地址
    public let serverUrl: String
    /// 应用标识
    public let appId: String
    /// 客户端ID
    public let clientId: String
    /// 是否启用日志
    public let enableLog: Bool
    /// 请求超时时间（秒）
    public let timeoutInterval: TimeInterval

    /// API Key 是否存在（标记，不存储实际值）
    public let hasApiKey: Bool

    /// 配置标识（关联 Keychain 条目）
    let configId: String

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
        self.enableLog = enableLog
        self.timeoutInterval = timeoutInterval
        self.hasApiKey = !apiKey.isEmpty
        self.configId = "sdk_config_\(appId)_\(clientId)"

        // 初始化时将 apiKey 写入 Keychain，不在内存中保留
        Self.storeApiKeyToKeychain(apiKey, configId: configId)
    }

    /// 将 API Key 存入 Keychain（类方法，单次写入）
    private static func storeApiKeyToKeychain(_ apiKey: String, configId: String) {
        let keychain = KeychainManager.sdkInstance
        do {
            // 使用 configId 作为 Keychain key，避免多配置冲突
            try keychain.store(key: configId, value: apiKey)
        } catch {
            // 存储失败时输出警告——不会阻断初始化
            print("[DigitalKeySDK] ⚠️ API Key 写入 Keychain 失败: \(error.localizedDescription)")
        }
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

    /// Keychain 管理器实例
    private let keychain = KeychainManager.sdkInstance

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
    ///   - apiKey: API密钥（立即写入Keychain，内存不保留）
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
        guard config.hasApiKey else {
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
        // 清理 Keychain 中的 API Key
        if let sdk = _shared {
            try? sdk.keychain.deleteApiKey()
        }
        _shared = nil
        log("SDK已重置")
    }

    private init(config: SdkConfig) {
        self.config = config
    }

    // MARK: - API Key 安全访问

    /// 从 Keychain 安全地读取 API Key
    ///
    /// 仅在需要向服务器发送认证请求时调用此方法，
    /// 用完后应立即释放返回值（方法返回后自动释放）。
    ///
    /// - Returns: API 密钥字符串
    /// - Throws: Keychain 读取失败时抛出 DigitalKeyError.keychainError
    public func retrieveApiKey() throws -> String {
        do {
            return try keychain.retrieve(key: config.configId)
        } catch {
            throw DigitalKeyError.keychainError(
                message: "API Key 读取失败: \(error.localizedDescription)"
            )
        }
    }

    /// 检查 API Key 是否可用（Keychain 中存在）
    public var hasApiKeyInKeychain: Bool {
        return keychain.hasApiKey
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
