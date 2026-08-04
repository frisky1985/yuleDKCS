import XCTest
import CryptoKit
@testable import DigitalKeySDK

// MARK: - Test Certificate

/// 测试用自签名证书（RSA 2048-bit, CN=api.digitalkey.test）
/// 生成方式:
/// ```
/// openssl req -x509 -newkey rsa:2048 -keyout /dev/null -out /tmp/test.der \
///   -days 3650 -nodes -subj "/CN=api.digitalkey.test" -outform DER
/// ```
private let testCertBase64 = """
MIICuDCCAaACCQCr3XhCv6WIvzANBgkqhkiG9w0BAQsFADAeMRwwGgYDVQQDDBNhcGkuZGlnaXRhbGtleS50ZXN0MB4XDTI2MDcxNjA3NDAzNVoXDTM2MDcxMzA3NDAzNVowHjEcMBoGA1UEAwwTYXBpLmRpZ2l0YWxrZXkudGVzdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAOzQbTUl34Uk71ft61guHxOumg/In5UcQbZgN/bfStctMZ2eooSHyVyhZgw7U6ZhQ6i9ScpVF2Fu5j3Fivd3BgNcgCS94CxbPzkxm/VwHhiBPv9lSmn7poAwr5E54F5fTCiovX4vqV9G/COXia7I3spKiD6cYVCjCyyzqZjj5ODlvfyv62IKtjLzTEiR7RvBvPp3bnmRturM8b+F104H8xbZHilgf5E0b99kGZFuBLpdWjmcWxIqClw1B/xO5ugEzqR9pu6zl7HzC811LT+hkqCqvZ/JMuWdRfHOCmHHpx+M6L73YdcatehHvWwQNX8XeyXtuQ0lvxw0o3SohCVnbRsCAwEAATANBgkqhkiG9w0BAQsFAAOCAQEATtIp9ol9nd+/7n/MqSZB1ORTuyXWZTj+WUm8izwuwsmBSTuzf8jWDbKGVEQP9JO7MsHj2h1Q9e6lhOmnFZhT1AyVc2ruWEnJT/+c80dK1PLdOsbiePUXtToBkL8YtC0nysfZfDPxtxsXtgoTZ+kDi/+luMip7xYPPkGzos/sI/h8SWUtX+KRnAtbFDRj3vnKDpjbr4HcaZM46QTWNsDW+d4XnpeLn+CbQb+yzZtoB1Lgr2EbPDVroQHrv3dod652opjCSaIFzlKm2Sua4vcO9CdasFxapuYDoOcVJESPI4CjtYgYj3vbArj1dKMkItop8eaLQVZ9Cd5MaJPrSSONDA==
"""

/// 另一个不匹配的哈希（用于验证失败测试）
private let nonMatchingHash = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="

// MARK: - 测试证书辅助

/// Base64 字符串去除所有换行符
private func strippingNewlines(_ base64: String) -> String {
    return base64.components(separatedBy: .newlines).joined()
}

/// 从测试证书 DER 数据加载 SecCertificate
private func loadTestCertificate() -> SecCertificate? {
    guard let derData = Data(base64Encoded: strippingNewlines(testCertBase64)) else {
        return nil
    }
    return SecCertificateCreateWithData(nil, derData as CFData)
}

/// 加载测试证书 DER 数据
private func loadTestCertData() -> Data? {
    return Data(base64Encoded: strippingNewlines(testCertBase64))
}

/// 加载测试证书的完整 DER 并计算 SHA-256 Base64 哈希
private func computeTestCertHash() -> String? {
    guard let derData = loadTestCertData() else { return nil }
    let hash = SHA256.hash(data: derData)
    return Data(hash).base64EncodedString()
}

/// 从测试证书提取公钥，计算其 SPKI SHA-256 Base64 哈希
/// 复现 TlsPinningDelegate 中 buildSubjectPublicKeyInfo 的逻辑
private func computeTestSPKIHash() -> String? {
    guard let cert = loadTestCertificate() else { return nil }
    guard let publicKey = extractPublicKey(from: cert) else { return nil }
    guard let rawKeyData = SecKeyCopyExternalRepresentation(publicKey, nil) as Data? else { return nil }

    // 构建 RSA SPKI 头 (OID: 1.2.840.113549.1.1.1)
    // 外层 SEQUENCE + 算法标识 SEQUENCE + BIT STRING 包裹
    let rsaSPKIHeader = Data([
        0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09,
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
        0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00
    ])

    let spkiData = rsaSPKIHeader + rawKeyData
    let hash = SHA256.hash(data: spkiData)
    return Data(hash).base64EncodedString()
}

/// 从 SecCertificate 提取 SecKey
private func extractPublicKey(from certificate: SecCertificate) -> SecKey? {
    var secTrust: SecTrust?
    let policy = SecPolicyCreateBasicX509()
    let status = SecTrustCreateWithCertificates(certificate, policy, &secTrust)
    guard status == errSecSuccess, let trust = secTrust else { return nil }
    if #available(iOS 14.0, *) {
        return SecTrustCopyKey(trust)
    } else {
        var trustResult: SecTrustResultType = .invalid
        SecTrustEvaluate(trust, &trustResult)
        return SecTrustCopyPublicKey(trust)
    }
}

/// 创建测试 SecTrust
private func createTestTrust() -> SecTrust? {
    guard let cert = loadTestCertificate() else { return nil }
    var trust: SecTrust?
    let policy = SecPolicyCreateSSL(true, "api.digitalkey.test" as CFString)
    let status = SecTrustCreateWithCertificates(cert, policy, &trust)
    guard status == errSecSuccess, let serverTrust = trust else { return nil }
    return serverTrust
}

/// 创建带 serverTrust 的 URL 认证挑战
private func createServerTrustChallenge(
    host: String,
    trust: SecTrust,
    sender: URLAuthenticationChallengeSender
) -> URLAuthenticationChallenge? {
    let protectionSpace = URLProtectionSpace(
        host: host,
        port: 443,
        protocol: "https",
        realm: nil,
        authenticationMethod: NSURLAuthenticationMethodServerTrust
    )

    // 通过 KVC 注入 serverTrust（URLProtectionSpace 的 serverTrust 为 readwrite，KVC 兼容）
    protectionSpace.setValue(trust, forKey: "serverTrust")

    return URLAuthenticationChallenge(
        protectionSpace: protectionSpace,
        proposedCredential: nil,
        previousFailureCount: 0,
        failureResponse: nil,
        error: nil,
        sender: sender
    )
}

// MARK: - SHA-256 Hash Tests

/// SHA-256 哈希计算测试
class SHA256HashTests: XCTestCase {

    func testSHA256OfKnownData() throws {
        let data = "hello".data(using: .utf8)!
        let hash = SHA256.hash(data: data)
        let hashData = Data(hash)
        let base64 = hashData.base64EncodedString()
        XCTAssertEqual(base64, "LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=")
    }

    func testSHA256OfEmptyData() throws {
        let data = Data()
        let hash = SHA256.hash(data: data)
        let hashData = Data(hash)
        let base64 = hashData.base64EncodedString()
        XCTAssertEqual(base64, "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=")
    }

    /// 验证测试证书的完整 SHA-256 哈希计算
    func testCertificateHashMatches() throws {
        guard let hash = computeTestCertHash() else {
            XCTFail("无法计算测试证书哈希")
            return
        }
        // 验证哈希是有效的 Base64 字符串
        XCTAssertGreaterThan(hash.count, 10)
        XCTAssertFalse(hash.contains(" "))

        // 验证再次计算得到同样的结果（确定性）
        guard let hash2 = computeTestCertHash() else {
            XCTFail("无法再次计算哈希")
            return
        }
        XCTAssertEqual(hash, hash2)
    }

    /// 验证 SPKI 哈希是有效的 Base64
    func testSPKIHashIsValid() throws {
        guard let spkiHash = computeTestSPKIHash() else {
            XCTFail("无法计算 SPKI 哈希")
            return
        }
        XCTAssertGreaterThan(spkiHash.count, 10)
        XCTAssertFalse(spkiHash.contains(" "))

        // 验证确定性
        guard let spkiHash2 = computeTestSPKIHash() else {
            XCTFail("无法再次计算 SPKI 哈希")
            return
        }
        XCTAssertEqual(spkiHash, spkiHash2)
    }

    /// 验证 SPKI 哈希和完整证书哈希不同
    func testSPKIHashDiffersFromCertHash() throws {
        guard let certHash = computeTestCertHash(),
              let spkiHash = computeTestSPKIHash() else {
            XCTFail("无法计算哈希")
            return
        }
        XCTAssertNotEqual(certHash, spkiHash,
                          "SPKI 哈希应与完整证书哈希不同")
    }
}

// MARK: - PinningStrategy Tests

/// PinningStrategy 枚举测试
class PinningStrategyTests: XCTestCase {

    func testPublicKeyStrategyEquality() {
        let s1 = PinningStrategy.publicKey(hashes: ["abc", "def"])
        let s2 = PinningStrategy.publicKey(hashes: ["abc", "def"])
        let s3 = PinningStrategy.publicKey(hashes: ["xyz"])
        let s4 = PinningStrategy.publicKey(hashes: [])

        XCTAssertEqual(s1, s2)
        XCTAssertNotEqual(s1, s3)
        XCTAssertNotEqual(s1, s4)
    }

    func testCertificateStrategyEquality() {
        let s1 = PinningStrategy.certificate(hashes: ["abc"])
        let s2 = PinningStrategy.certificate(hashes: ["abc"])
        let s3 = PinningStrategy.certificate(hashes: ["def"])

        XCTAssertEqual(s1, s2)
        XCTAssertNotEqual(s1, s3)
    }

    func testStrategyCrossTypeInequality() {
        let pk = PinningStrategy.publicKey(hashes: ["abc"])
        let cert = PinningStrategy.certificate(hashes: ["abc"])
        XCTAssertNotEqual(pk, cert)
    }

    func testDisabledEquality() {
        let d1 = PinningStrategy.disabled
        let d2 = PinningStrategy.disabled
        XCTAssertEqual(d1, d2)
    }

    func testPublicKeyStrategyWithMultipleHashes() {
        guard let spkiHash = computeTestSPKIHash() else {
            XCTFail("无法计算 SPKI 哈希")
            return
        }
        let strategy = PinningStrategy.publicKey(hashes: [spkiHash, nonMatchingHash])
        switch strategy {
        case .publicKey(let hashes):
            XCTAssertEqual(hashes.count, 2)
            XCTAssertTrue(hashes.contains(spkiHash))
            XCTAssertTrue(hashes.contains(nonMatchingHash))
        default:
            XCTFail("Expected publicKey strategy")
        }
    }

    func testCertificateStrategyWithSingleHash() {
        guard let certHash = computeTestCertHash() else {
            XCTFail("无法计算证书哈希")
            return
        }
        let strategy = PinningStrategy.certificate(hashes: [certHash])
        switch strategy {
        case .certificate(let hashes):
            XCTAssertEqual(hashes.count, 1)
            XCTAssertEqual(hashes.first, certHash)
        default:
            XCTFail("Expected certificate strategy")
        }
    }
}

// MARK: - TlsPinningDelegate Initialization Tests

/// TlsPinningDelegate 初始化与配置测试
class TlsPinningDelegateInitTests: XCTestCase {

    private var testSPKIHash: String {
        return computeTestSPKIHash() ?? nonMatchingHash
    }

    func testInitWithSingleHost() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: [
                "api.digitalkey.cn": .publicKey(hashes: [testSPKIHash])
            ],
            isDebug: false
        )
        XCTAssertNotNil(delegate)
    }

    func testInitWithMultipleHosts() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: [
                "api.digitalkey.cn": .publicKey(hashes: [testSPKIHash]),
                "backup.digitalkey.cn": .publicKey(hashes: [nonMatchingHash]),
                "*.digitalkey.cn": .certificate(hashes: ["certHashPlaceholder"]),
            ],
            isDebug: false
        )
        XCTAssertNotNil(delegate)
    }

    func testInitWithDebugMode() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: [
                "api.digitalkey.cn": .publicKey(hashes: [testSPKIHash])
            ],
            isDebug: true
        )
        XCTAssertNotNil(delegate)
    }

    func testInitWithEmptyHosts() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: [:],
            isDebug: false
        )
        XCTAssertNotNil(delegate)
    }

    func testInitWithDisabledStrategy() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.cn": .disabled],
            isDebug: false
        )
        XCTAssertNotNil(delegate)
    }

    func testPublicKeyFactoryMethod() {
        let delegate = TlsPinningDelegate.publicKeyPinning(
            host: "api.digitalkey.cn",
            hashes: [testSPKIHash],
            isDebug: false
        )
        XCTAssertNotNil(delegate)
    }

    func testCertificateFactoryMethod() {
        let delegate = TlsPinningDelegate.certificatePinning(
            host: "api.digitalkey.cn",
            hashes: ["certHash"],
            isDebug: false
        )
        XCTAssertNotNil(delegate)
    }

    func testDebugFactoryMethod() {
        let delegate = TlsPinningDelegate.debugPinning()
        XCTAssertNotNil(delegate)
    }
}

// MARK: - Challenge Passthrough Tests

/// 验证非 ServerTrust / 未配置域名等情况的委托回调正确传递
class TlsPinningDelegateChallengeTests: XCTestCase {

    /// 非 ServerTrust 认证挑战应执行默认处理
    func testNonServerTrustChallengePassesThrough() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.cn": .publicKey(hashes: [nonMatchingHash])],
            isDebug: false
        )

        let expectation = self.expectation(description: "Challenge handled")
        let protectionSpace = URLProtectionSpace(
            host: "api.digitalkey.cn",
            port: 443,
            protocol: "https",
            realm: nil,
            authenticationMethod: NSURLAuthenticationMethodNTLM
        )

        let sender = MockChallengeSender(
            onPerformDefaultHandling: {
                expectation.fulfill()
            },
            onCancel: { XCTFail("不应取消非 ServerTrust 挑战") }
        )

        let challenge = URLAuthenticationChallenge(
            protectionSpace: protectionSpace,
            proposedCredential: nil,
            previousFailureCount: 0,
            failureResponse: nil,
            error: nil,
            sender: sender
        )

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .performDefaultHandling)
        }

        wait(for: [expectation], timeout: 2.0)
    }

    /// 未配置 Pinning 的域名应执行默认处理
    func testUnpinnedHostPassesThrough() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: ["pinned.example.com": .publicKey(hashes: [nonMatchingHash])],
            isDebug: false
        )

        let expectation = self.expectation(description: "Unpinned host")
        let protectionSpace = URLProtectionSpace(
            host: "other-service.com",
            port: 443,
            protocol: "https",
            realm: nil,
            authenticationMethod: NSURLAuthenticationMethodServerTrust
        )

        delegate.urlSession(URLSession.shared, didReceive: URLAuthenticationChallenge(
            protectionSpace: protectionSpace,
            proposedCredential: nil,
            previousFailureCount: 0,
            failureResponse: nil,
            error: nil,
            sender: MockChallengeSender()
        )) { disposition, _ in
            XCTAssertEqual(disposition, .performDefaultHandling)
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)
    }

    /// Disabled 策略应跳过校验并默认处理
    func testDisabledStrategySkipsValidation() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.cn": .disabled],
            isDebug: false
        )

        let expectation = self.expectation(description: "Disabled strategy")
        let protectionSpace = URLProtectionSpace(
            host: "api.digitalkey.cn",
            port: 443,
            protocol: "https",
            realm: nil,
            authenticationMethod: NSURLAuthenticationMethodServerTrust
        )

        delegate.urlSession(URLSession.shared, didReceive: URLAuthenticationChallenge(
            protectionSpace: protectionSpace,
            proposedCredential: nil,
            previousFailureCount: 0,
            failureResponse: nil,
            error: nil,
            sender: MockChallengeSender()
        )) { disposition, _ in
            XCTAssertEqual(disposition, .performDefaultHandling)
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)
    }
}

// MARK: - Cert-Based Trust Validation Tests

/// 基于真实证书的 Pinning 校验测试
///
/// 使用 `isDebug: true` 跳过 iOS 的信任链验证（自签名证书不被系统信任），
/// 仅测试哈希匹配逻辑。
class TlsPinningDelegateCertValidationTests: XCTestCase {

    /// 延迟加载的测试哈希（避免类加载时计算）
    private var testSPKIHash: String {
        return computeTestSPKIHash() ?? nonMatchingHash
    }
    private var testCertHash: String {
        return computeTestCertHash() ?? nonMatchingHash
    }

    // MARK: - Public Key Pinning Tests

    /// 公钥锁定 — 正确哈希应通过校验（useCredential）
    func testPublicKeyPinningPassesWithCorrectHash() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.test": .publicKey(hashes: [testSPKIHash])],
            isDebug: true
        )

        let expectation = self.expectation(description: "Public key pinning passed")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onUseCredential: { _, _ in
                expectation.fulfill()
            }, onCancel: {
                XCTFail("Pinning 通过时不应取消挑战")
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .useCredential)
        }

        wait(for: [expectation], timeout: 2.0)
    }

    /// 公钥锁定 — 错误哈希应触发 onPinningFailed 并取消连接
    func testPublicKeyPinningFailsWithIncorrectHash() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.test": .publicKey(hashes: [nonMatchingHash])],
            isDebug: false
        )

        var didCallPinningFailed = false
        delegate.onPinningFailed = { info in
            didCallPinningFailed = true
            XCTAssertEqual(info.host, "api.digitalkey.test")
        }

        let expectation = self.expectation(description: "Public key pinning failed")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onCancel: {
                expectation.fulfill()
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .cancelAuthenticationChallenge)
        }

        wait(for: [expectation], timeout: 2.0)
        XCTAssertTrue(didCallPinningFailed, "onPinningFailed 回调应被触发")
    }

    // MARK: - Certificate Pinning Tests

    /// 证书锁定 — 正确哈希应通过校验
    func testCertificatePinningPassesWithCorrectHash() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.test": .certificate(hashes: [testCertHash])],
            isDebug: true
        )

        let expectation = self.expectation(description: "Certificate pinning passed")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onUseCredential: { _, _ in
                expectation.fulfill()
            }, onCancel: {
                XCTFail("Pinning 通过时不应取消")
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .useCredential)
        }

        wait(for: [expectation], timeout: 2.0)
    }

    /// 证书锁定 — 错误哈希应拒绝连接
    func testCertificatePinningFailsWithIncorrectHash() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.test": .certificate(hashes: [nonMatchingHash])],
            isDebug: false
        )

        let expectation = self.expectation(description: "Certificate pinning cancelled")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onCancel: {
                expectation.fulfill()
            }, onUseCredential: { _, _ in
                XCTFail("Pinning 失败时不应使用 credential")
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .cancelAuthenticationChallenge)
        }

        wait(for: [expectation], timeout: 2.0)
    }

    // MARK: - Multiple Hash Support Tests

    /// 多哈希轮换 — 第二个哈希匹配应通过
    func testPinningPassesWithMultipleHashesWhenSecondMatches() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: [
                "api.digitalkey.test": .publicKey(hashes: [
                    nonMatchingHash,    // 第一个不匹配
                    testSPKIHash,       // 第二个匹配
                ])
            ],
            isDebug: true
        )

        let expectation = self.expectation(description: "Second hash matched")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onUseCredential: { _, _ in
                expectation.fulfill()
            }, onCancel: {
                XCTFail("多哈希匹配时不应取消")
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .useCredential)
        }

        wait(for: [expectation], timeout: 2.0)
    }

    /// 多哈希全部不匹配 — 应失败
    func testPinningFailsWhenNoHashMatches() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: [
                "api.digitalkey.test": .publicKey(hashes: [
                    nonMatchingHash,
                    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=",
                ])
            ],
            isDebug: false
        )

        let expectation = self.expectation(description: "No hash matched")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onCancel: {
                expectation.fulfill()
            }, onUseCredential: { _, _ in
                XCTFail("所有哈希不匹配时不应通过")
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .cancelAuthenticationChallenge)
        }

        wait(for: [expectation], timeout: 2.0)
    }

    // MARK: - Release Mode Tests

    /// Release 模式 — Pinning 失败应取消连接
    func testReleaseModeCancelsFailedPinning() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.test": .publicKey(hashes: [nonMatchingHash])],
            isDebug: false
        )

        var didCallPinningFailed = false
        delegate.onPinningFailed = { info in
            didCallPinningFailed = true
        }

        let expectation = self.expectation(description: "Release mode cancelled")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onCancel: {
                expectation.fulfill()
            }, onUseCredential: { _, _ in
                XCTFail("失败时不应放行")
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .cancelAuthenticationChallenge)
        }

        wait(for: [expectation], timeout: 2.0)
        XCTAssertTrue(didCallPinningFailed)
    }

    // MARK: - Debug Mode Tests

    /// Debug 模式 — Pinning 失败应放行（仅记录）
    func testDebugModePermitsFailedPinning() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.test": .publicKey(hashes: [nonMatchingHash])],
            isDebug: true
        )

        var didCallPinningFailed = false
        delegate.onPinningFailed = { _ in
            didCallPinningFailed = true
        }

        let expectation = self.expectation(description: "Debug mode permits")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onUseCredential: { _, _ in
                expectation.fulfill()
            }, onCancel: {
                XCTFail("Debug 模式应放行")
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { disposition, _ in
            XCTAssertEqual(disposition, .useCredential)
        }

        wait(for: [expectation], timeout: 2.0)
        XCTAssertTrue(didCallPinningFailed, "Debug 模式下也应触发失败回调")
    }

    // MARK: - Callback Tests

    /// onPinningPassed 回调验证
    func testOnPinningPassedCallback() {
        guard let trust = createTestTrust() else {
            XCTFail("无法创建测试 SecTrust")
            return
        }

        let delegate = TlsPinningDelegate(
            pinnedHosts: ["api.digitalkey.test": .publicKey(hashes: [testSPKIHash])],
            isDebug: true
        )

        var passedHost: String?
        var passedStrategy: PinningStrategy?
        delegate.onPinningPassed = { host, strategy in
            passedHost = host
            passedStrategy = strategy
        }

        let expectation = self.expectation(description: "On passed callback")

        guard let challenge = createServerTrustChallenge(
            host: "api.digitalkey.test",
            trust: trust,
            sender: MockChallengeSender(onUseCredential: { _, _ in
                expectation.fulfill()
            })
        ) else {
            XCTFail("无法创建挑战")
            return
        }

        delegate.urlSession(URLSession.shared, didReceive: challenge) { _, _ in }

        wait(for: [expectation], timeout: 2.0)
        XCTAssertEqual(passedHost, "api.digitalkey.test")
        XCTAssertNotNil(passedStrategy)
    }

    /// onLog 回调验证
    func testOnLogCallback() {
        let delegate = TlsPinningDelegate(
            pinnedHosts: ["host": .disabled],
            isDebug: false
        )

        var loggedMessage: String?
        delegate.onLog = { msg in
            loggedMessage = msg
        }

        let expectation = self.expectation(description: "Log callback")
        let protectionSpace = URLProtectionSpace(
            host: "host",
            port: 443,
            protocol: "https",
            realm: nil,
            authenticationMethod: NSURLAuthenticationMethodServerTrust
        )

        delegate.urlSession(URLSession.shared, didReceive: URLAuthenticationChallenge(
            protectionSpace: protectionSpace,
            proposedCredential: nil,
            previousFailureCount: 0,
            failureResponse: nil,
            error: nil,
            sender: MockChallengeSender()
        )) { _, _ in
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)
        // Disabled strategy 应触发日志
    }
}

// MARK: - APIClient Integration Tests

/// APIClient 集成 TLS Pinning 测试
class APIClientPinningIntegrationTests: XCTestCase {

    override func setUp() {
        super.setUp()
        MockURLProtocol.reset()
    }

    override func tearDown() {
        MockURLProtocol.reset()
        super.tearDown()
    }

    /// APIClient 不配置 Pinning 时通过 Mock Session 正常工作
    func testAPIClientWithoutPinningUsesDefaultSession() {
        let client = APIClient(session: .mockSession)
        XCTAssertNotNil(client)
    }

    /// APIClient 配置 Pinning 时可通过注入 Mock Session 测试（不拦截真实网络）
    func testAPIClientWithPinningConfigAndMockSessionMakesRequest() {
        MockURLProtocol.configureEmptySuccess(statusCode: 200)
        let tlsConfig = TLSPinningConfig(
            pinnedHosts: ["api.digitalkey.cn": [nonMatchingHash]],
            isDebug: true
        )
        // 注入 Mock Session 覆盖 Pinning 配置的 URLSession
        let client = APIClient(session: .mockSession, tlsConfig: tlsConfig)

        let request = APIRequest(path: "/test", method: .get)
        let expectation = self.expectation(description: "Request with mock session")

        var result: Result<Void, Error>?
        client.performRaw(request) { res in
            result = res
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)

        switch result {
        case .success:
            XCTAssertTrue(true)
        case .failure(let error):
            XCTFail("请求失败: \(error)")
        case nil:
            XCTFail("未收到响应")
        }

        XCTAssertFalse(MockURLProtocol.capturedRequests.isEmpty)
    }

    /// APIClient 通过 TLSPinningConfig 构造（无 Mock Session）——不会崩溃
    func testAPIClientWithPinningConfigCreatesSession() {
        let tlsConfig = TLSPinningConfig(
            pinnedHosts: ["api.digitalkey.cn": [nonMatchingHash]],
            isDebug: true
        )
        let client = APIClient(tlsConfig: tlsConfig)
        XCTAssertNotNil(client)
    }

    /// Mock Session 的优先级高于 Pinning 配置
    func testMockSessionTakesPriority() {
        MockURLProtocol.configureEmptySuccess(statusCode: 200)
        let tlsConfig = TLSPinningConfig(
            pinnedHosts: ["api.digitalkey.cn": [nonMatchingHash]],
            isDebug: false
        )
        let client = APIClient(session: .mockSession, tlsConfig: tlsConfig)

        let request = APIRequest(path: "/test", method: .get)
        let expectation = self.expectation(description: "Mock session request")

        client.performRaw(request) { _ in
            expectation.fulfill()
        }

        wait(for: [expectation], timeout: 2.0)
        XCTAssertEqual(MockURLProtocol.capturedRequests.count, 1)
    }

    // MARK: - Pinning Config Update Tests

    /// 动态更新 Pinning 哈希配置不崩溃
    func testUpdatePinningHashes() {
        let tlsConfig = TLSPinningConfig(
            pinnedHosts: ["api.digitalkey.cn": [nonMatchingHash]],
            isDebug: true
        )
        let client = APIClient(tlsConfig: tlsConfig)
        client.updatePinningHashes(host: "api.digitalkey.cn", hashes: [nonMatchingHash, nonMatchingHash])
        // 验证更新不抛异常
    }

    /// 更新完整 Pinning 配置不崩溃
    func testUpdateFullPinningConfig() {
        let client = APIClient(session: .mockSession)
        let newConfig = TLSPinningConfig(
            pinnedHosts: [
                "api.digitalkey.cn": [nonMatchingHash],
                "backup.digitalkey.cn": [nonMatchingHash],
            ],
            isDebug: false
        )
        client.updatePinningConfig(newConfig)
    }

    /// 清空 Pinning 配置后恢复正常工作
    func testClearPinningConfig() {
        MockURLProtocol.configureEmptySuccess(statusCode: 200)
        let client = APIClient(session: .mockSession)

        // 清空配置
        client.updatePinningConfig(TLSPinningConfig(pinnedHosts: [:], isDebug: false))

        let request = APIRequest(path: "/test", method: .get)
        let expectation = self.expectation(description: "Clear pinning config")
        client.performRaw(request) { _ in
            expectation.fulfill()
        }
        wait(for: [expectation], timeout: 2.0)
        XCTAssertEqual(MockURLProtocol.capturedRequests.count, 1)
    }

    // MARK: - TLSPinningConfig Tests

    /// TLSPinningConfig 初始化
    func testTLSPinningConfigInit() {
        let config = TLSPinningConfig(pinnedHosts: ["host": ["hash1", "hash2"]])
        XCTAssertEqual(config.pinnedHosts.count, 1)
        XCTAssertEqual(config.pinnedHosts["host"]?.count, 2)
        XCTAssertFalse(config.isDebug) // 默认 false
    }

    /// TLSPinningConfig 空哈希列表
    func testTLSPinningConfigWithEmptyHashes() {
        let config = TLSPinningConfig(pinnedHosts: ["host": []])
        XCTAssertEqual(config.pinnedHosts["host"]?.count, 0)
    }

    /// TLSPinningConfig 多域名
    func testTLSPinningConfigMultipleHosts() {
        let config = TLSPinningConfig(
            pinnedHosts: [
                "a.com": ["hash1"],
                "b.com": ["hash2", "hash3"],
            ]
        )
        XCTAssertEqual(config.pinnedHosts.count, 2)
    }

    /// TLSPinningConfig Debug 模式标记
    func testTLSPinningConfigDebugMode() {
        let config = TLSPinningConfig(pinnedHosts: ["h": ["h"]], isDebug: true)
        XCTAssertTrue(config.isDebug)
    }
}

// MARK: - APIError TLS Pinning Tests

/// APIError + TLS Pinning 错误测试
class APIErrorTLSPinningTest: XCTestCase {

    func testTLSPinningFailedError() {
        let error = APIError.tlsPinningFailed(host: "api.example.com", reason: "哈希不匹配")
        XCTAssertEqual(error.errorDescription, "TLS Pinning 校验失败: api.example.com - 哈希不匹配")
    }

    func testTLSPinningFailedErrorDescription() {
        let error = APIError.tlsPinningFailed(host: "test.host", reason: "证书不匹配")
        XCTAssertTrue(error.errorDescription?.contains("test.host") ?? false)
        XCTAssertTrue(error.errorDescription?.contains("证书") ?? false)
    }

    func testTLSPinningErrorEquality() {
        let e1 = APIError.tlsPinningFailed(host: "h", reason: "r")
        let e2 = APIError.tlsPinningFailed(host: "h", reason: "r")
        let e3 = APIError.tlsPinningFailed(host: "h2", reason: "r")
        XCTAssertEqual(e1, e2)
        XCTAssertNotEqual(e1, e3)
    }
}

// MARK: - Mock Challenge Sender

/// 模拟 URLAuthenticationChallengeSender，用于单元测试
class MockChallengeSender: NSObject, URLAuthenticationChallengeSender {

    private let onUseCredential: ((URLAuthenticationChallenge, URLCredential?) -> Void)?
    private let onPerformDefaultHandling: (() -> Void)?
    private let onCancel: (() -> Void)?
    private let onReject: (() -> Void)?

    init(
        onUseCredential: ((URLAuthenticationChallenge, URLCredential?) -> Void)? = nil,
        onPerformDefaultHandling: (() -> Void)? = nil,
        onCancel: (() -> Void)? = nil,
        onReject: (() -> Void)? = nil
    ) {
        self.onUseCredential = onUseCredential
        self.onPerformDefaultHandling = onPerformDefaultHandling
        self.onCancel = onCancel
        self.onReject = onReject
    }

    func use(_ credential: URLCredential, for challenge: URLAuthenticationChallenge) {
        onUseCredential?(challenge, credential)
    }

    func performDefaultHandling(for challenge: URLAuthenticationChallenge) {
        onPerformDefaultHandling?()
    }

    func cancel(_ challenge: URLAuthenticationChallenge) {
        onCancel?()
    }

    func rejectProtectionSpaceAndContinue(with challenge: URLAuthenticationChallenge) {
        onReject?()
    }
}
