// TlsPinningDelegate.swift
// 数字钥匙SDK — TLS 证书/公钥锁定
//
// 实现 URLSessionDelegate 的证书锁定策略：
// 1. 公钥锁定（推荐，更灵活 — 证书轮换时只需更新公钥哈希）
// 2. 证书锁定（更严格 — 绑定特定证书）
// 3. 多证书回退（应对证书轮换）
// 4. Pinning 失败时不再静默降级，直接取消连接并上报安全事件
//
// 安全原则:
// - 绝不静默降级：校验不通过 = 连接取消
// - Debug 构建可在开发环境放行，Release 构建严格校验
// - 所有异常通过 DkTelemetry 上报 security_alert 事件

import Foundation
import Security
import CryptoKit

// MARK: - Pinning 策略

/// TLS Pinning 策略
public enum PinningStrategy: Equatable {
    /// 公钥锁定 — 比对服务器证书的公钥 SHA-256 哈希
    /// 优点：证书轮换时只需重新签发证书，公钥可以不变
    case publicKey(hashes: [String])
    
    /// 证书锁定 — 比对完整证书数据 SHA-256 哈希
    /// 优点：最严格，防止任何中间人伪造
    case certificate(hashes: [String])
    
    /// 禁用锁定 — 仅用于 Debug 或开发环境
    case disabled
}

// MARK: - Pinning 结果

/// Pinning 校验结果
public enum PinningResult {
    case passed
    case failed(reason: String)
    case skipped(reason: String)
}

// MARK: - Pinning 错误

/// TLS Pinning 错误信息
public struct PinningErrorInfo {
    public let host: String
    public let strategy: PinningStrategy
    public let reason: String
    public let leafCertificateSummary: String?
}

// MARK: - Pinning Delegate

/// TLS Pinning URLSession Delegate
///
/// 使用方式:
/// ```swift
/// let pinning = TlsPinningDelegate(
///     hosts: [
///         "api.digitalkey.cn": .publicKey(hashes: ["SHA256+base64..."]),
///     ],
///     isDebug: false
/// )
/// let session = URLSession(configuration: config, delegate: pinning, delegateQueue: nil)
/// ```
public class TlsPinningDelegate: NSObject, URLSessionDelegate {
    
    // MARK: - Properties
    
    /// 域名 → Pinning 策略映射
    private let pinnedHosts: [String: PinningStrategy]
    
    /// 是否为 Debug 构建
    private let isDebug: Bool
    
    /// Pinning 失败回调（用于上报安全事件）
    public var onPinningFailed: ((PinningErrorInfo) -> Void)?
    
    /// Pinning 成功回调
    public var onPinningPassed: ((String, PinningStrategy) -> Void)?
    
    /// 日志回调
    public var onLog: ((String) -> Void)?
    
    // MARK: - Init
    
    /// 初始化 Pinning Delegate
    /// - Parameters:
    ///   - pinnedHosts: 需要锁定校验的域名列表及对应策略
    ///   - isDebug: 是否为 Debug 构建（Debug 下 pinning 失败仅警告、不阻断）
    public init(
        pinnedHosts: [String: PinningStrategy],
        isDebug: Bool = false
    ) {
        self.pinnedHosts = pinnedHosts
        self.isDebug = isDebug
    }
    
    // MARK: - URLSessionDelegate
    
    public func urlSession(
        _ session: URLSession,
        didReceive challenge: URLAuthenticationChallenge,
        completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void
    ) {
        // 只处理服务器信任验证
        guard challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust,
              let serverTrust = challenge.protectionSpace.serverTrust else {
            completionHandler(.performDefaultHandling, nil)
            return
        }
        
        let host = challenge.protectionSpace.host
        
        // 查找该域名是否配置了 Pinning
        guard let strategy = findMatchingStrategy(for: host) else {
            // 未配置 Pinning 的域名，走系统默认验证
            completionHandler(.performDefaultHandling, nil)
            return
        }
        
        // 禁用状态：跳过校验
        if case .disabled = strategy {
            log("[TLS Pinning] \(host) — Pinning disabled, skipping validation")
            completionHandler(.performDefaultHandling, nil)
            return
        }
        
        // 执行 Pinning 校验
        let result = validate(serverTrust: serverTrust, host: host, strategy: strategy)
        
        switch result {
        case .passed:
            log("[TLS Pinning] ✅ \(host) — validation passed (\(describe(strategy)))")
            onPinningPassed?(host, strategy)
            // 创建 URLCredential 通过验证
            let credential = URLCredential(trust: serverTrust)
            completionHandler(.useCredential, credential)
            
        case .failed(let reason):
            let info = buildErrorInfo(host: host, strategy: strategy, reason: reason, serverTrust: serverTrust)
            log("[TLS Pinning] ❌ \(host) — validation FAILED: \(reason)")
            onPinningFailed?(info)
            
            if isDebug {
                // Debug 模式：记录失败但放行（用于开发调试）
                log("[TLS Pinning] ⚠️ \(host) — Debug mode: permitting despite pinning failure")
                let credential = URLCredential(trust: serverTrust)
                completionHandler(.useCredential, credential)
            } else {
                // Release 模式：拒绝连接
                completionHandler(.cancelAuthenticationChallenge, nil)
            }
            
        case .skipped(let reason):
            log("[TLS Pinning] ⏭️ \(host) — skipped: \(reason)")
            completionHandler(.performDefaultHandling, nil)
        }
    }
    
    // MARK: - Pinning 校验核心逻辑
    
    /// 执行 Pinning 校验
    /// - Parameters:
    ///   - serverTrust: 服务器信任对象
    ///   - host: 域名
    ///   - strategy: Pinning 策略
    /// - Returns: 校验结果
    private func validate(
        serverTrust: SecTrust,
        host: String,
        strategy: PinningStrategy
    ) -> PinningResult {
        // 验证信任链有效性（系统默认验证）
        var trustResult: CFError?
        let isTrusted = SecTrustEvaluateWithError(serverTrust, &trustResult)
        
        if !isDebug && !isTrusted {
            // 非 Debug 下信任链无效直接失败
            let reason = trustResult?.localizedDescription ?? "Trust evaluation failed"
            return .failed(reason: "证书信任链无效: \(reason)")
        }
        
        // 获取证书链
        guard let certificateChain = getCertificateChain(from: serverTrust) else {
            return .failed(reason: "无法获取服务器证书链")
        }
        
        guard !certificateChain.isEmpty else {
            return .failed(reason: "服务器未提供证书")
        }
        
        switch strategy {
        case .publicKey(let pinnedHashes):
            return validatePublicKeys(
                certificateChain: certificateChain,
                pinnedHashes: pinnedHashes
            )
            
        case .certificate(let pinnedHashes):
            return validateCertificates(
                certificateChain: certificateChain,
                pinnedHashes: pinnedHashes
            )
            
        case .disabled:
            return .skipped(reason: "Pinning disabled")
        }
    }
    
    // MARK: - 公钥锁定
    
    /// 公钥锁定校验
    /// 提取证书链中每张证书的公钥，计算 SHA-256 哈希，
    /// 与预置的哈希列表比对（支持多公钥回退）。
    private func validatePublicKeys(
        certificateChain: [SecCertificate],
        pinnedHashes: [String]
    ) -> PinningResult {
        var lastError: String?
        
        for cert in certificateChain {
            guard let publicKey = copyPublicKey(from: cert) else {
                lastError = "无法提取公钥"
                continue
            }
            
            guard let publicKeyHash = hashPublicKey(publicKey) else {
                lastError = "无法计算公钥哈希"
                continue
            }
            
            if pinnedHashes.contains(publicKeyHash) {
                return .passed
            }
            
            lastError = "公钥哈希不匹配 (got: \(publicKeyHash))"
        }
        
        return .failed(reason: lastError ?? "所有证书公钥均未匹配 Pinned Hashes")
    }
    
    // MARK: - 证书锁定
    
    /// 证书锁定校验
    /// 计算完整证书数据的 SHA-256 哈希，
    /// 与预置的哈希列表比对（支持多证书回退）。
    private func validateCertificates(
        certificateChain: [SecCertificate],
        pinnedHashes: [String]
    ) -> PinningResult {
        var lastError: String?
        
        for cert in certificateChain {
            guard let certData = SecCertificateCopyData(cert) as Data? else {
                lastError = "无法读取证书数据"
                continue
            }
            
            let certHash = sha256(data: certData)
            
            if pinnedHashes.contains(certHash) {
                return .passed
            }
            
            lastError = "证书哈希不匹配"
        }
        
        return .failed(reason: lastError ?? "所有证书均未匹配 Pinned Hashes")
    }
    
    // MARK: - 辅助方法
    
    /// 为域名查找匹配的 Pinning 策略（支持通配符）
    private func findMatchingStrategy(for host: String) -> PinningStrategy? {
        // 精确匹配优先
        if let exact = pinnedHosts[host] {
            return exact
        }
        
        // 通配符匹配 *.example.com
        let components = host.split(separator: ".")
        guard components.count >= 2 else { return nil }
        
        let wildcardHost = "*." + components.suffix(from: max(1, components.count - 2)).joined(separator: ".")
        return pinnedHosts[wildcardHost]
    }
    
    /// 从 SecureTransport 信任对象中提取证书链
    private func getCertificateChain(from trust: SecTrust) -> [SecCertificate]? {
        if #available(iOS 15.0, *) {
            return (SecTrustCopyCertificateChain(trust) as? [SecCertificate]) ?? []
        } else {
            let count = SecTrustGetCertificateCount(trust)
            guard count > 0 else { return [] }
            
            var certs: [SecCertificate] = []
            for i in 0..<count {
                if let cert = SecTrustGetCertificateAtIndex(trust, i) {
                    certs.append(cert)
                }
            }
            return certs
        }
    }
    
    /// 从证书中提取公钥
    private func copyPublicKey(from certificate: SecCertificate) -> SecKey? {
        var secTrust: SecTrust?
        let policy = SecPolicyCreateBasicX509()
        
        let status = SecTrustCreateWithCertificates(certificate, policy, &secTrust)
        guard status == errSecSuccess, let trust = secTrust else {
            return nil
        }
        
        if #available(iOS 14.0, *) {
            return SecTrustCopyKey(trust)
        } else {
            // iOS 14 以下回退方案
            // 先评估信任
            var trustResult: SecTrustResultType = .invalid
            SecTrustEvaluate(trust, &trustResult)
            return SecTrustCopyPublicKey(trust)
        }
    }
    
    /// 计算公钥的 SHA-256 哈希（Base64 编码）
    private func hashPublicKey(_ publicKey: SecKey) -> String? {
        guard let publicKeyData = SecKeyCopyExternalRepresentation(publicKey, nil) as Data? else {
            return nil
        }
        
        // X.509 SubjectPublicKeyInfo 格式
        // 对于 RSA 和 EC 密钥，需要按规范构造 SPKI
        let spkiData = buildSubjectPublicKeyInfo(rawPublicKey: publicKeyData, key: publicKey)
        let hash = sha256(data: spkiData)
        return hash
    }
    
    /// 构建 SubjectPublicKeyInfo (SPKI) 数据
    /// 用于公钥哈希的标准计算方法（RFC 7469 规范）
    private func buildSubjectPublicKeyInfo(rawPublicKey: Data, key: SecKey) -> Data {
        // 获取密钥属性以判断算法
        guard let attributes = SecKeyCopyAttributes(key) as? [String: Any] else {
            // 回退：直接对原始公钥数据做哈希
            return rawPublicKey
        }
        
        let keyType = attributes[kSecAttrKeyType as String] as? String
        let keySize = attributes[kSecAttrKeySizeInBits as String] as? Int ?? 256
        
        var spkiHeader: Data
        
        switch keyType {
        case kSecAttrKeyTypeRSA as String:
            // RSA 公钥 SPKI 头 (OID: 1.2.840.113549.1.1.1)
            spkiHeader = Data([
                0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09,
                0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
                0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00
            ])
            
        case kSecAttrKeyTypeECSECPrimeRandom as String:
            // EC 公钥 SPKI 头 (OID: 1.2.840.10045.2.1)
            switch keySize {
            case 256:
                spkiHeader = Data([
                    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
                    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a,
                    0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
                    0x42, 0x00
                ])
            case 384:
                spkiHeader = Data([
                    0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86,
                    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b,
                    0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00
                ])
            default:
                return rawPublicKey
            }
            
        default:
            // 未知算法，直接返回原始数据
            return rawPublicKey
        }
        
        return spkiHeader + rawPublicKey
    }
    
    /// SHA-256 哈希计算（Base64 编码）
    /// 使用 CryptoKit（iOS 13+）替代 CommonCrypto，无需额外桥接
    private func sha256(data: Data) -> String {
        let hash = SHA256.hash(data: data)
        return Data(hash).base64EncodedString()
    }
    
    /// 构建 Pinning 错误信息
    private func buildErrorInfo(
        host: String,
        strategy: PinningStrategy,
        reason: String,
        serverTrust: SecTrust
    ) -> PinningErrorInfo {
        let summary: String? = {
            guard let chain = getCertificateChain(from: serverTrust),
                  let first = chain.first,
                  let data = SecCertificateCopyData(first) as Data? else {
                return nil
            }
            let hash = sha256(data: data)
            return "leaf_cert_sha256: \(hash)"
        }()
        
        return PinningErrorInfo(
            host: host,
            strategy: strategy,
            reason: reason,
            leafCertificateSummary: summary
        )
    }
    
    /// Pinning 策略描述
    private func describe(_ strategy: PinningStrategy) -> String {
        switch strategy {
        case .publicKey(let hashes):
            return "PublicKey pinning (\(hashes.count) hashes)"
        case .certificate(let hashes):
            return "Certificate pinning (\(hashes.count) hashes)"
        case .disabled:
            return "Disabled"
        }
    }
    
    // MARK: - 日志
    
    private func log(_ message: String) {
        onLog?(message)
        #if DEBUG
        print(message)
        #endif
    }
}

// MARK: - 便捷工厂方法

extension TlsPinningDelegate {
    
    /// 为单个域名创建公钥锁定 Delegate
    /// - Parameters:
    ///   - host: 目标服务器域名
    ///   - publicKeyHashes: 公钥 SHA-256 Base64 哈希列表（支持轮换）
    ///   - isDebug: 是否 Debug 模式
    public static func publicKeyPinning(
        host: String,
        hashes: [String],
        isDebug: Bool = false
    ) -> TlsPinningDelegate {
        return TlsPinningDelegate(
            pinnedHosts: [host: .publicKey(hashes: hashes)],
            isDebug: isDebug
        )
    }
    
    /// 为单个域名创建证书锁定 Delegate
    /// - Parameters:
    ///   - host: 目标服务器域名
    ///   - certificateHashes: 证书 SHA-256 Base64 哈希列表
    ///   - isDebug: 是否 Debug 模式
    public static func certificatePinning(
        host: String,
        hashes: [String],
        isDebug: Bool = false
    ) -> TlsPinningDelegate {
        return TlsPinningDelegate(
            pinnedHosts: [host: .certificate(hashes: hashes)],
            isDebug: isDebug
        )
    }
    
    /// 创建一个调试 Delegate（不校验任何域名）
    public static func debugPinning() -> TlsPinningDelegate {
        return TlsPinningDelegate(
            pinnedHosts: [:],
            isDebug: true
        )
    }
}

// MARK: - CommonCrypto 桥接

// 注意: SHA-256 使用 CryptoKit 框架计算（iOS 13+），已在 project.yml 中链接
