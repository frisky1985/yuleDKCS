import Foundation
import Security
#if canImport(UIKit)
import UIKit
#endif

// MARK: - 手机厂商枚举（与 sdk.proto PhoneVendor 对齐）

public enum PhoneVendor: String, Codable {
    case apple = "apple"
    case samsung = "samsung"
    case xiaomi = "xiaomi"
    case oppo = "oppo"
    case vivo = "vivo"
    case huawei = "huawei"

    /// 对应 hub.proto PhoneVendor 枚举名（protojson 反序列化要求传枚举名字符串）
    public var protoName: String {
        switch self {
        case .apple: return "APPLE"
        case .samsung: return "SAMSUNG"
        case .xiaomi: return "XIAOMI"
        case .oppo: return "OPPO"
        case .vivo: return "VIVO"
        case .huawei: return "HUAWEI"
        }
    }

    /// 对应 sdk.proto PhoneVendor 枚举值
    public var protoValue: Int {
        switch self {
        case .apple: return 1
        case .samsung: return 2
        case .xiaomi: return 3
        case .oppo: return 4
        case .vivo: return 5
        case .huawei: return 6
        }
    }
}

// MARK: - 协议类型（与 sdk.proto Protocol 对齐）

public enum DigitalKeyProtocol: String, Codable {
    case ccc = "ccc_dk3"
    case iccoa = "iccoa_dk40"
    case icce = "icce"

    /// 对应 hub.proto Protocol 枚举名（protojson 反序列化要求传枚举名字符串；
    /// sdk.proto 与 hub.proto 的数字值有漂移，但枚举名一致且稳定）
    public var protoName: String {
        switch self {
        case .ccc: return "CCC_DK3"
        case .iccoa: return "ICCOA_DK40"
        case .icce: return "ICCE"
        }
    }

    /// 对应 sdk.proto Protocol 枚举值
    public var protoValue: Int {
        switch self {
        case .ccc: return 1
        case .iccoa: return 2
        case .icce: return 3
        }
    }
}

// MARK: - DeviceManager

/// 设备信息管理器
///
/// 职责:
/// 1. 生成/读取 device_id（首次生成后持久化）
/// 2. 从 Secure Enclave 读取 ECC P-256 公钥
/// 3. 检测手机厂商 (vendor)
/// 4. 检测数字钥匙协议 (protocol)
///
/// bindKey / acceptShare 时由 HubClient 自动调用填充请求字段。
public final class YDKDeviceManager {

    public static let shared = YDKDeviceManager()

    private let defaults: UserDefaults
    private let keyTag = "com.yuledkcs.sdk.device.key"
    private let deviceIdKey = "com.yuledkcs.sdk.device.id"

    private init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    // MARK: - Device ID

    /// 获取设备 ID（首次调用生成 UUID 并持久化）
    public func getDeviceId() -> String {
        if let existing = defaults.string(forKey: deviceIdKey) {
            return existing
        }
        let newId = UUID().uuidString
        defaults.set(newId, forKey: deviceIdKey)
        return newId
    }

    // MARK: - Secure Enclave 公钥

    /// 从 Secure Enclave 读取 ECC P-256 公钥（首次调用创建）
    ///
    /// 返回 X.509 DER 编码的公钥，发送到 Hub 前需 base64。
    public func readPublicKey() throws -> Data {
        // 已存在则返回
        if let existing = loadExistingPublicKey() {
            return existing
        }
        return try createAndReturnPublicKey()
    }

    /// 读取公钥并返回 base64 字符串（用于 JSON 请求）
    public func readPublicKeyBase64() throws -> String {
        try readPublicKey().base64EncodedString()
    }

    private func loadExistingPublicKey() -> Data? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassKey,
            kSecAttrApplicationTag as String: keyTag.data(using: .utf8)!,
            kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
            kSecReturnData as String: true,
        ]
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        guard status == errSecSuccess, let data = result as? Data else {
            return nil
        }
        return data
    }

    private func createAndReturnPublicKey() throws -> Data {
        let access = SecAccessControlCreateWithFlags(
            nil,
            kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
            [.privateKeyUsage],
            nil
        )!

        let attributes: [String: Any] = [
            kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
            kSecAttrKeySizeInBits as String: 256,
            kSecAttrTokenID as String: kSecAttrTokenIDSecureEnclave,
            kSecAttrApplicationTag as String: keyTag.data(using: .utf8)!,
            kSecAttrAccessControl as String: access,
            kSecAttrLabel as String: "yuleDKCS Device Key",
        ]

        var error: Unmanaged<CFError>?
        guard let privateKey = SecKeyCreateRandomKey(attributes as CFDictionary, &error) else {
            throw YDKError.internal_("Secure Enclave key creation failed: \(error.debugDescription)")
        }

        guard let publicKey = SecKeyCopyPublicKey(privateKey),
              let publicKeyData = SecKeyCopyExternalRepresentation(publicKey, &error) as Data? else {
            throw YDKError.internal_("Failed to read public key: \(error.debugDescription)")
        }

        // 保存公钥以便后续读取（SE 私钥不可导出，公钥单独存）
        let saveQuery: [String: Any] = [
            kSecClass as String: kSecClassKey,
            kSecAttrApplicationTag as String: keyTag.data(using: .utf8)!,
            kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
            kSecValueData as String: publicKeyData,
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
        ]
        SecItemDelete(saveQuery as CFDictionary)
        let status = SecItemAdd(saveQuery as CFDictionary, nil)
        guard status == errSecSuccess else {
            throw YDKError.internal_("Failed to persist public key: \(status)")
        }

        return publicKeyData
    }

    // MARK: - 厂商检测

    /// 检测手机厂商
    public func detectVendor() -> PhoneVendor {
        #if targetEnvironment(simulator)
        return .apple
        #elseif canImport(UIKit)
        let model = UIDevice.current.model
        if model.localizedCaseInsensitiveContains("iPhone") {
            return .apple
        }
        // 非 iOS 设备（SDK 运行在 iOS，正常只会是 apple）
        return .apple
        #else
        // 非 iOS 宿主（如 macOS 开发/CI 环境，无 UIKit）: 默认返回 apple，与 iOS 行为一致
        return .apple
        #endif
    }

    // MARK: - 协议检测

    /// 检测数字钥匙协议
    ///
    /// - iOS 设备: 支持 CCC Digital Key 4.0（Apple CarKey 基于 CCC）
    /// - 后续可按系统版本/能力细化（iOS 18+ 支持 UWB 等）
    public func detectProtocol() -> DigitalKeyProtocol {
        .ccc
    }

    // MARK: - 默认访问级别

    /// 默认钥匙访问级别（锁车/解锁/启动 + 找车）
    public func defaultAccessLevel() -> [String: Bool] {
        [
            "lock": true,
            "unlock": true,
            "engine": true,
            "find": true,
        ]
    }
}
