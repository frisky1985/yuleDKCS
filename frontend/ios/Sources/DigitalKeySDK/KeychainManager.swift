// KeychainManager.swift
// 数字钥匙SDK - Keychain 安全存储
//
// 使用 iOS Security.framework (Keychain Services) 存储敏感数据，
// 避免在进程内存中明文保留 API Key 等凭证。
//
// 特性:
// - AES-256 硬件加密（iOS Keychain 原生）
// - 设备锁定时自动加密（Accessible = afterFirstUnlockThisDeviceOnly）
// - 可通过 kSecAttrAccessControl 配置生物识别保护
// - 应用卸载时自动清除

import Foundation
import Security

// MARK: - Keychain 错误

/// Keychain 操作错误
enum KeychainError: Error, LocalizedError {
    case unexpectedStatus(OSStatus, String)
    case itemNotFound
    case duplicateItem
    case encodingError
    case decodingError

    var errorDescription: String? {
        switch self {
        case .unexpectedStatus(let status, let operation):
            return "[Keychain] \(operation) 失败: \(status) (\(secErrorMessage(status)))"
        case .itemNotFound:
            return "[Keychain] 未找到指定条目"
        case .duplicateItem:
            return "[Keychain] 条目已存在"
        case .encodingError:
            return "[Keychain] 编码错误"
        case .decodingError:
            return "[Keychain] 解码错误"
        }
    }

    private func secErrorMessage(_ status: OSStatus) -> String {
        switch status {
        case errSecSuccess: return "成功"
        case errSecUnimplemented: return "未实现"
        case errSecParam: return "参数错误"
        case errSecAllocate: return "内存分配失败"
        case errSecNotAvailable: return "功能不可用"
        case errSecAuthFailed: return "认证失败"
        case errSecDuplicateItem: return "重复条目"
        case errSecItemNotFound: return "条目未找到"
        case errSecInteractionNotAllowed: return "用户交互不允许"
        case errSecDecode: return "解码错误"
        case errSecUserCanceled: return "用户已取消"
        default: return "未知错误(\(status))"
        }
    }
}

// MARK: - Keychain 管理器

/// Keychain 安全存储管理器
///
/// 所有敏感数据通过 iOS Keychain Services 存储，应用内存中仅保留
/// 检索句柄/布尔标记，不缓存实际值。
///
/// 使用示例:
/// ```swift
/// let keychain = KeychainManager(service: "com.digitalkey.sdk")
/// try keychain.store(key: "apiKey", value: "sk-xxx...")
/// let key = try keychain.retrieve(key: "apiKey")
/// try keychain.delete(key: "apiKey")
/// ```
public class KeychainManager {

    /// Keychain 服务名称
    private let service: String

    /// Keychain 访问级别
    private let accessibility: CFString

    /// 初始化
    /// - Parameters:
    ///   - service: Keychain 服务标识（建议使用 bundle ID）
    ///   - accessibility: 访问级别，默认仅本机首次解锁后可用
    public init(
        service: String,
        accessibility: CFString = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
    ) {
        self.service = service
        self.accessibility = accessibility
    }

    // MARK: - 基础操作 (String)

    /// 存储字符串值到 Keychain
    /// - Parameters:
    ///   - key: 条目键名
    ///   - value: 要存储的字符串
    /// - Throws: KeychainError
    public func store(key: String, value: String) throws {
        guard let data = value.data(using: .utf8) else {
            throw KeychainError.encodingError
        }
        try store(key: key, data: data)
    }

    /// 从 Keychain 读取字符串值
    /// - Parameter key: 条目键名
    /// - Returns: 存储的字符串
    /// - Throws: KeychainError
    public func retrieve(key: String) throws -> String {
        let data = try retrieve(key: key)
        guard let string = String(data: data, encoding: .utf8) else {
            throw KeychainError.decodingError
        }
        return string
    }

    /// 检查 Keychain 中是否存在指定条目
    /// - Parameter key: 条目键名
    /// - Returns: 是否存在
    public func contains(key: String) -> Bool {
        let query = baseQuery(key: key) as [String: Any]
            .merging([kSecMatchLimit as String: kSecMatchLimitOne]) { $1 }
            .merging([kSecReturnAttributes as String: false]) { $1 }

        let status = SecItemCopyMatching(query as CFDictionary, nil)
        return status == errSecSuccess
    }

    /// 更新 Keychain 条目
    /// - Parameters:
    ///   - key: 条目键名
    ///   - value: 新字符串值
    /// - Throws: KeychainError
    public func update(key: String, value: String) throws {
        guard let data = value.data(using: .utf8) else {
            throw KeychainError.encodingError
        }
        try update(key: key, data: data)
    }

    /// 删除 Keychain 条目
    /// - Parameter key: 条目键名
    /// - Throws: KeychainError
    public func delete(key: String) throws {
        let query = baseQuery(key: key)
        let status = SecItemDelete(query as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw KeychainError.unexpectedStatus(status, "删除")
        }
    }

    /// 清空服务下所有 Keychain 条目
    public func clear() {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service
        ]
        SecItemDelete(query as CFDictionary)
    }

    // MARK: - 基础操作 (Data)

    /// 存储二进制数据到 Keychain
    /// - Parameters:
    ///   - key: 条目键名
    ///   - data: 二进制数据
    /// - Throws: KeychainError
    public func store(key: String, data: Data) throws {
        // 先尝试删除旧条目（避免重复）
        try? delete(key: key)

        let query = (baseQuery(key: key) as [String: Any])
            .merging([kSecValueData as String: data]) { $1 }

        let status = SecItemAdd(query as CFDictionary, nil)
        guard status == errSecSuccess else {
            throw KeychainError.unexpectedStatus(status, "存储")
        }
    }

    /// 从 Keychain 读取二进制数据
    /// - Parameter key: 条目键名
    /// - Returns: 二进制数据
    /// - Throws: KeychainError
    public func retrieve(key: String) throws -> Data {
        let query = (baseQuery(key: key) as [String: Any])
            .merging([kSecMatchLimit as String: kSecMatchLimitOne]) { $1 }
            .merging([kSecReturnData as String: true]) { $1 }

        var result: AnyObject?
        let status = SecItemCopyMatching(query as CFDictionary, &result)

        guard status != errSecItemNotFound else {
            throw KeychainError.itemNotFound
        }
        guard status == errSecSuccess else {
            throw KeychainError.unexpectedStatus(status, "读取")
        }
        guard let data = result as? Data else {
            throw KeychainError.decodingError
        }
        return data
    }

    /// 更新二进制数据
    /// - Parameters:
    ///   - key: 条目键名
    ///   - data: 新数据
    /// - Throws: KeychainError
    public func update(key: String, data: Data) throws {
        let query = baseQuery(key: key)
        let update: [String: Any] = [kSecValueData as String: data]

        let status = SecItemUpdate(query as CFDictionary, update as CFDictionary)
        guard status == errSecSuccess else {
            throw KeychainError.unexpectedStatus(status, "更新")
        }
    }

    // MARK: - 私有方法

    /// 构建基础 Keychain 查询字典
    private func baseQuery(key: String) -> CFDictionary {
        return [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key,
            kSecAttrAccessible as String: accessibility,
            kSecUseDataProtectionKeychain as String: true  // 使用 Data Protection API（iOS 9+）
        ] as CFDictionary
    }
}

// MARK: - 便捷扩展: DigitalKeySDK 专用

extension KeychainManager {

    /// SDK Keychain 服务标识
    static let sdkService = "com.digitalkey.sdk.keychain"

    /// API Key 在 Keychain 中的键名
    static let apiKeyKey = "sdk_api_key"

    /// 创建 SDK 专用 Keychain 管理器
    static var sdkInstance: KeychainManager {
        return KeychainManager(service: sdkService)
    }

    /// 存储 API Key
    /// - Parameter apiKey: API 密钥字符串
    func storeApiKey(_ apiKey: String) throws {
        try store(key: Self.apiKeyKey, value: apiKey)
    }

    /// 读取 API Key
    /// - Returns: API 密钥
    func retrieveApiKey() throws -> String {
        return try retrieve(key: Self.apiKeyKey)
    }

    /// 删除 API Key
    func deleteApiKey() throws {
        try delete(key: Self.apiKeyKey)
    }

    /// 检查 API Key 是否存在
    var hasApiKey: Bool {
        return contains(key: Self.apiKeyKey)
    }
}
