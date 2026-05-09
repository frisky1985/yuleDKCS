import Foundation
import CommonCrypto

/// 加密错误类型
public enum CryptoError: Error, LocalizedError {
    case keyNotFound
    case invalidKey
    case encryptionFailed
    case decryptionFailed
    case invalidData
    case algorithmNotSupported
    
    public var errorDescription: String? {
        switch self {
        case .keyNotFound:
            return "密钥未找到"
        case .invalidKey:
            return "无效的密钥"
        case .encryptionFailed:
            return "加密失败"
        case .decryptionFailed:
            return "解密失败"
        case .invalidData:
            return "无效的数据"
        case .algorithmNotSupported:
            return "不支持的加密算法"
        }
    }
}

/// 加密算法
public enum CryptoAlgorithm {
    case aes256GCM
    case chacha20Poly1305
    
    var keySize: Int {
        switch self {
        case .aes256GCM:
            return 32
        case .chacha20Poly1305:
            return 32
        }
    }
    
    var nonceSize: Int {
        switch self {
        case .aes256GCM:
            return 12
        case .chacha20Poly1305:
            return 12
        }
    }
    
    var tagSize: Int {
        return 16
    }
}

/// 加密包装器 - 提供加密操作接口
public final class CryptoWrapper {
    
    // MARK: - Singleton
    
    public static let shared = CryptoWrapper()
    
    // MARK: - Properties
    
    private let queue = DispatchQueue(label: "com.yuledkcs.crypto", qos: .default)
    private var keyStore: [String: Data] = [:]
    private let keychain = CryptoKeychain()
    
    // MARK: - Initialization
    
    private init() {}
    
    // MARK: - Public Methods
    
    /// 存储密钥
    /// - Parameters:
    ///   - key: 密钥数据
    ///   - keyId: 密钥标识符
    public func storeKey(_ key: Data, keyId: String) throws {
        try keychain.save(key: key, identifier: keyId)
        keyStore[keyId] = key
    }
    
    /// 获取密钥
    /// - Parameter keyId: 密钥标识符
    /// - Returns: 密钥数据
    public func getKey(keyId: String) -> Data? {
        if let key = keyStore[keyId] {
            return key
        }
        
        // 从 Keychain 获取
        if let key = try? keychain.load(identifier: keyId) {
            keyStore[keyId] = key
            return key
        }
        
        return nil
    }
    
    /// 删除密钥
    /// - Parameter keyId: 密钥标识符
    public func deleteKey(keyId: String) throws {
        try keychain.delete(identifier: keyId)
        keyStore.removeValue(forKey: keyId)
    }
    
    /// 加密数据
    /// - Parameters:
    ///   - data: 要加密的数据
    ///   - keyId: 密钥标识符
    ///   - algorithm: 加密算法
    /// - Returns: 加密结果
    public func encrypt(
        data: Data,
        keyId: String,
        algorithm: CryptoAlgorithm = .aes256GCM
    ) -> Result<Data, Error> {
        return queue.sync {
            guard let key = getKey(keyId: keyId) else {
                return .failure(CryptoError.keyNotFound)
            }
            
            do {
                switch algorithm {
                case .aes256GCM:
                    let encrypted = try aesGCMEncrypt(data: data, key: key)
                    return .success(encrypted)
                case .chacha20Poly1305:
                    // 通过 FFI 调用底层 C 库
                    let result = FFIBridge.shared.encrypt(
                        data: data,
                        key: key,
                        algorithm: .chacha20Poly1305
                    )
                    return result
                }
            } catch {
                return .failure(error)
            }
        }
    }
    
    /// 解密数据
    /// - Parameters:
    ///   - data: 要解密的数据
    ///   - keyId: 密钥标识符
    ///   - algorithm: 加密算法
    /// - Returns: 解密结果
    public func decrypt(
        data: Data,
        keyId: String,
        algorithm: CryptoAlgorithm = .aes256GCM
    ) -> Result<Data, Error> {
        return queue.sync {
            guard let key = getKey(keyId: keyId) else {
                return .failure(CryptoError.keyNotFound)
            }
            
            do {
                switch algorithm {
                case .aes256GCM:
                    let decrypted = try aesGCMDecrypt(data: data, key: key)
                    return .success(decrypted)
                case .chacha20Poly1305:
                    // 通过 FFI 调用底层 C 库
                    let result = FFIBridge.shared.decrypt(
                        data: data,
                        key: key,
                        algorithm: .chacha20Poly1305
                    )
                    return result
                }
            } catch {
                return .failure(error)
            }
        }
    }
    
    /// 生成随机密钥
    /// - Parameter size: 密钥大小（字节）
    /// - Returns: 随机密钥
    public func generateRandomKey(size: Int = 32) -> Data {
        var bytes = [UInt8](repeating: 0, count: size)
        _ = SecRandomCopyBytes(kSecRandomDefault, size, &bytes)
        return Data(bytes)
    }
    
    /// 计算 HMAC
    /// - Parameters:
    ///   - data: 输入数据
    ///   - key: 密钥
    /// - Returns: HMAC 值
    public func hmac(data: Data, key: Data) -> Data {
        var result = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        CCHmac(CCHmacAlgorithm(kCCHmacAlgSHA256),
               (key as NSData).bytes, key.count,
               (data as NSData).bytes, data.count,
               &result)
        return Data(result)
    }
    
    /// 计算 SHA256 哈希
    /// - Parameter data: 输入数据
    /// - Returns: 哈希值
    public func sha256(data: Data) -> Data {
        var hash = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        data.withUnsafeBytes {
            _ = CC_SHA256($0.baseAddress, CC_LONG(data.count), &hash)
        }
        return Data(hash)
    }
    
    // MARK: - Private Methods
    
    private func aesGCMEncrypt(data: Data, key: Data) throws -> Data {
        guard key.count == CryptoAlgorithm.aes256GCM.keySize else {
            throw CryptoError.invalidKey
        }
        
        // 生成随机 nonce
        let nonce = generateRandomKey(size: CryptoAlgorithm.aes256GCM.nonceSize)
        
        // 使用 FFI 调用底层加密库
        let result = FFIBridge.shared.encryptAESGCM(
            plaintext: data,
            key: key,
            nonce: nonce
        )
        
        switch result {
        case .success(let ciphertext):
            // 组合: nonce + ciphertext + tag
            var output = Data()
            output.append(nonce)
            output.append(ciphertext)
            return output
        case .failure(let error):
            throw error
        }
    }
    
    private func aesGCMDecrypt(data: Data, key: Data) throws -> Data {
        guard key.count == CryptoAlgorithm.aes256GCM.keySize else {
            throw CryptoError.invalidKey
        }
        
        let nonceSize = CryptoAlgorithm.aes256GCM.nonceSize
        let tagSize = CryptoAlgorithm.aes256GCM.tagSize
        
        guard data.count > nonceSize + tagSize else {
            throw CryptoError.invalidData
        }
        
        let nonce = data.prefix(nonceSize)
        let ciphertext = data.suffix(from: nonceSize)
        
        let result = FFIBridge.shared.decryptAESGCM(
            ciphertext: ciphertext,
            key: key,
            nonce: Data(nonce)
        )
        
        switch result {
        case .success(let plaintext):
            return plaintext
        case .failure(let error):
            throw error
        }
    }
}

// MARK: - Crypto Keychain

private final class CryptoKeychain {
    
    func save(key: Data, identifier: String) throws {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: identifier,
            kSecValueData as String: key,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        ]
        
        // 删除已存在的
        SecItemDelete(query as CFDictionary)
        
        let status = SecItemAdd(query as CFDictionary, nil)
        guard status == errSecSuccess else {
            throw CryptoError.encryptionFailed
        }
    }
    
    func load(identifier: String) throws -> Data {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: identifier,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        
        guard status == errSecSuccess, let data = result as? Data else {
            throw CryptoError.keyNotFound
        }
        
        return data
    }
    
    func delete(identifier: String) throws {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: identifier
        ]
        
        let status = SecItemDelete(query as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw CryptoError.encryptionFailed
        }
    }
}