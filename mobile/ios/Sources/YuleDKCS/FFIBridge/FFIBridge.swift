import Foundation

/// FFI 桥接 - 连接 Swift 与 C 库
public final class FFIBridge {
    
    // MARK: - Singleton
    
    public static let shared = FFIBridge()
    
    // MARK: - Properties
    
    private var isInitialized = false
    private let queue = DispatchQueue(label: "com.yuledkcs.ffi", qos: .userInitiated)
    
    // MARK: - Initialization
    
    private init() {}
    
    // MARK: - Public Methods
    
    /// 初始化 FFI 库
    public func initialize() {
        queue.sync {
            guard !isInitialized else { return }
            
            // 调用 C 库初始化函数
            ydkcs_init()
            
            isInitialized = true
            print("[YuleDKCS] FFI Bridge initialized")
        }
    }
    
    /// 生成数字钥匙
    /// - Parameter vehicleId: 车辆 ID
    /// - Returns: 钥匙数据
    public func generateKey(vehicleId: String) -> Result<Data, Error> {
        return queue.sync {
            var result = ydkcs_key_result_t()
            
            vehicleId.withCString { cVehicleId in
                ydkcs_generate_key(cVehicleId, &result)
            }
            
            if result.success == 0 {
                let errorMsg = String(cString: result.error_msg)
                return .failure(YuleDKCSError.ffiError(errorMsg))
            }
            
            let keyData = Data(bytes: result.key_data, count: Int(result.key_len))
            
            // 释放 C 内存
            ydkcs_free_key_result(&result)
            
            return .success(keyData)
        }
    }
    
    /// 加密数据 (ChaCha20-Poly1305)
    /// - Parameters:
    ///   - data: 明文数据
    ///   - key: 加密密钥
    ///   - algorithm: 加密算法
    /// - Returns: 密文数据 (nonce + ciphertext + tag)
    public func encrypt(
        data: Data,
        key: Data,
        algorithm: CryptoAlgorithm
    ) -> Result<Data, Error> {
        return queue.sync {
            var result = ydkcs_crypto_result_t()
            
            data.withUnsafeBytes { dataPtr in
                key.withUnsafeBytes { keyPtr in
                    ydkcs_encrypt(
                        dataPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        data.count,
                        keyPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        key.count,
                        YDKCS_ALGO_CHACHA20_POLY1305,
                        &result
                    )
                }
            }
            
            if result.success == 0 {
                let errorMsg = String(cString: result.error_msg)
                return .failure(YuleDKCSError.ffiError(errorMsg))
            }
            
            let encryptedData = Data(bytes: result.data, count: Int(result.data_len))
            
            // 释放 C 内存
            ydkcs_free_crypto_result(&result)
            
            return .success(encryptedData)
        }
    }
    
    /// 解密数据 (ChaCha20-Poly1305)
    /// - Parameters:
    ///   - data: 密文数据 (nonce + ciphertext + tag)
    ///   - key: 解密密钥
    ///   - algorithm: 加密算法
    /// - Returns: 明文数据
    public func decrypt(
        data: Data,
        key: Data,
        algorithm: CryptoAlgorithm
    ) -> Result<Data, Error> {
        return queue.sync {
            var result = ydkcs_crypto_result_t()
            
            data.withUnsafeBytes { dataPtr in
                key.withUnsafeBytes { keyPtr in
                    ydkcs_decrypt(
                        dataPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        data.count,
                        keyPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        key.count,
                        YDKCS_ALGO_CHACHA20_POLY1305,
                        &result
                    )
                }
            }
            
            if result.success == 0 {
                let errorMsg = String(cString: result.error_msg)
                return .failure(YuleDKCSError.ffiError(errorMsg))
            }
            
            let decryptedData = Data(bytes: result.data, count: Int(result.data_len))
            
            // 释放 C 内存
            ydkcs_free_crypto_result(&result)
            
            return .success(decryptedData)
        }
    }
    
    /// 使用 AES-GCM 加密
    /// - Parameters:
    ///   - plaintext: 明文
    ///   - key: 密钥
    ///   - nonce: 随机数
    /// - Returns: 密文 + 认证标签
    public func encryptAESGCM(
        plaintext: Data,
        key: Data,
        nonce: Data
    ) -> Result<Data, Error> {
        return queue.sync {
            var result = ydkcs_crypto_result_t()
            
            plaintext.withUnsafeBytes { plainPtr in
                key.withUnsafeBytes { keyPtr in
                    nonce.withUnsafeBytes { noncePtr in
                        ydkcs_encrypt_aes_gcm(
                            plainPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            plaintext.count,
                            keyPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            key.count,
                            noncePtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            nonce.count,
                            &result
                        )
                    }
                }
            }
            
            if result.success == 0 {
                let errorMsg = String(cString: result.error_msg)
                return .failure(YuleDKCSError.ffiError(errorMsg))
            }
            
            let encryptedData = Data(bytes: result.data, count: Int(result.data_len))
            
            ydkcs_free_crypto_result(&result)
            
            return .success(encryptedData)
        }
    }
    
    /// 使用 AES-GCM 解密
    /// - Parameters:
    ///   - ciphertext: 密文 + 认证标签
    ///   - key: 密钥
    ///   - nonce: 随机数
    /// - Returns: 明文
    public func decryptAESGCM(
        ciphertext: Data,
        key: Data,
        nonce: Data
    ) -> Result<Data, Error> {
        return queue.sync {
            var result = ydkcs_crypto_result_t()
            
            ciphertext.withUnsafeBytes { cipherPtr in
                key.withUnsafeBytes { keyPtr in
                    nonce.withUnsafeBytes { noncePtr in
                        ydkcs_decrypt_aes_gcm(
                            cipherPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            ciphertext.count,
                            keyPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            key.count,
                            noncePtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            nonce.count,
                            &result
                        )
                    }
                }
            }
            
            if result.success == 0 {
                let errorMsg = String(cString: result.error_msg)
                return .failure(YuleDKCSError.ffiError(errorMsg))
            }
            
            let decryptedData = Data(bytes: result.data, count: Int(result.data_len))
            
            ydkcs_free_crypto_result(&result)
            
            return .success(decryptedData)
        }
    }
    
    /// 验证钥匙签名
    /// - Parameters:
    ///   - keyData: 钥匙数据
    ///   - signature: 签名
    ///   - publicKey: 公钥
    /// - Returns: 验证结果
    public func verifyKeySignature(
        keyData: Data,
        signature: Data,
        publicKey: Data
    ) -> Bool {
        return queue.sync {
            var result: Int32 = 0
            
            keyData.withUnsafeBytes { keyPtr in
                signature.withUnsafeBytes { sigPtr in
                    publicKey.withUnsafeBytes { pubPtr in
                        result = ydkcs_verify_signature(
                            keyPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            keyData.count,
                            sigPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            signature.count,
                            pubPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            publicKey.count
                        )
                    }
                }
            }
            
            return result == 1
        }
    }
    
    /// 派生会话密钥
    /// - Parameters:
    ///   - masterKey: 主密钥
    ///   - context: 上下文信息
    /// - Returns: 派生密钥
    public func deriveSessionKey(
        masterKey: Data,
        context: String
    ) -> Result<Data, Error> {
        return queue.sync {
            var result = ydkcs_key_result_t()
            
            masterKey.withUnsafeBytes { keyPtr in
                context.withCString { ctxPtr in
                    ydkcs_derive_key(
                        keyPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        masterKey.count,
                        ctxPtr,
                        &result
                    )
                }
            }
            
            if result.success == 0 {
                let errorMsg = String(cString: result.error_msg)
                return .failure(YuleDKCSError.ffiError(errorMsg))
            }
            
            let derivedKey = Data(bytes: result.key_data, count: Int(result.key_len))
            
            ydkcs_free_key_result(&result)
            
            return .success(derivedKey)
        }
    }
    
    /// 清理资源
    public func cleanup() {
        queue.sync {
            guard isInitialized else { return }
            ydkcs_cleanup()
            isInitialized = false
        }
    }
    
    deinit {
        cleanup()
    }
}

// MARK: - C 数据结构定义

/// 钥匙结果结构体（对应 C 结构）
struct ydkcs_key_result_t {
    var success: Int32
    var key_data: UnsafePointer<UInt8>?
    var key_len: Int
    var error_msg: UnsafePointer<CChar>?
    
    init() {
        self.success = 0
        self.key_data = nil
        self.key_len = 0
        self.error_msg = nil
    }
}

/// 加密结果结构体（对应 C 结构）
struct ydkcs_crypto_result_t {
    var success: Int32
    var data: UnsafePointer<UInt8>?
    var data_len: Int
    var error_msg: UnsafePointer<CChar>?
    
    init() {
        self.success = 0
        self.data = nil
        self.data_len = 0
        self.error_msg = nil
    }
}

/// 加密算法类型（对应 C 枚举）
enum ydkcs_algorithm_t: Int32 {
    case YDKCS_ALGO_AES256_GCM = 0
    case YDKCS_ALGO_CHACHA20_POLY1305 = 1
}

// 常量定义
let YDKCS_ALGO_AES256_GCM: Int32 = 0
let YDKCS_ALGO_CHACHA20_POLY1305: Int32 = 1