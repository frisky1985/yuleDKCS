import Foundation
import CommonCrypto

// MARK: - CCC 安全通道 (2b-E)
//
// 依据: docs/certification/ccc-ts101-ble-secure-channel.md
//   - §18.4.9  Listing 18-9  系统密钥派生 (HKDF-SHA256, Info="SystemKeys")
//   - §18.4.12 Listing 18-10 命令加密与认证 (GPC_SPE_014 §6.2.6 / SCP03)
//   - §18.4.13 Listing 18-11 响应加密与认证 (GPC_SPE_014 §6.2.7)
// 算法族: AES-128 + CMAC-AES-128 (RFC4493)。既不是 AES-CCM 也不是 AES-256-GCM。

// MARK: - HKDF-SHA256 (RFC5869)

enum CCCSystemKeyDerivation {

    /// HKDF-Extract (RFC5869 §2.2)
    /// - Parameters:
    ///   - salt: 规范要求 NULL → 用 32 字节零串
    ///   - ikm:  输入密钥材料 (SPAKE2+ 共享密钥 SK)
    static func extract(salt: Data? = nil, ikm: Data) -> Data {
        let saltData = salt ?? Data(repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        return hmacSHA256(key: saltData, data: ikm)
    }

    /// HKDF-Expand (RFC5869 §2.3)
    static func expand(prk: Data, info: Data, outputLength: Int) -> Data {
        let hashLen = Int(CC_SHA256_DIGEST_LENGTH)
        precondition(outputLength <= 255 * hashLen, "HKDF-Expand output too long")
        var output = Data()
        var t = Data()
        var counter: UInt8 = 1
        while output.count < outputLength {
            var input = t
            input.append(info)
            input.append(counter)
            t = hmacSHA256(key: prk, data: input)
            output.append(t)
            counter += 1
        }
        return output.prefix(outputLength)
    }

    /// 完整 HKDF-SHA256: Info="SystemKeys", L=64 (无 BLE intro 密钥)
    /// 输出: [0:16]=Kenc [16:16]=Kmac [32:16]=Krmac [48:16]=LONG_TERM_SHARED_SECRET
    static func deriveSystemKeys(sk: Data) -> SystemKeySet {
        let prk = extract(salt: nil, ikm: sk)
        let okm = expand(prk: prk, info: Data("SystemKeys".utf8), outputLength: 64)
        return SystemKeySet(
            kenc: okm.subdata(in: 0..<16),
            kmac: okm.subdata(in: 16..<32),
            krmac: okm.subdata(in: 32..<48),
            longTermSharedSecret: okm.subdata(in: 48..<64)
        )
    }

    static func hmacSHA256(key: Data, data: Data) -> Data {
        var mac = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        key.withUnsafeBytes { keyPtr in
            data.withUnsafeBytes { dataPtr in
                CCHmac(CCHmacAlgorithm(kCCHmacAlgSHA256),
                       keyPtr.baseAddress, key.count,
                       dataPtr.baseAddress, data.count,
                       &mac)
            }
        }
        return Data(mac)
    }
}

/// 系统密钥组 (各 16 字节)
struct SystemKeySet: Equatable {
    let kenc: Data
    let kmac: Data
    let krmac: Data
    let longTermSharedSecret: Data
}

// MARK: - AES-128 原语 (CommonCrypto)

enum CCCAESCrypto {

    /// AES-128-CBC 加密 (无填充, 调用方负责 block 对齐)
    static func cbcEncrypt(key: Data, iv: Data, plaintext: Data) throws -> Data {
        try crypt(op: CCOperation(kCCEncrypt), key: key, iv: iv, input: plaintext)
    }

    /// AES-128-CBC 解密 (无填充)
    static func cbcDecrypt(key: Data, iv: Data, ciphertext: Data) throws -> Data {
        try crypt(op: CCOperation(kCCDecrypt), key: key, iv: iv, input: ciphertext)
    }

    /// AES-128-ECB 单块加密 (CMAC 子密钥生成用)
    static func ecbEncryptBlock(key: Data, block: Data) throws -> Data {
        precondition(block.count == kCCBlockSizeAES128, "CMAC block must be 16 bytes")
        var outLen = 0
        var out = [UInt8](repeating: 0, count: kCCBlockSizeAES128)
        let status = key.withUnsafeBytes { keyPtr in
            block.withUnsafeBytes { blockPtr in
                CCCrypt(CCOperation(kCCEncrypt),
                        CCAlgorithm(kCCAlgorithmAES),
                        CCOptions(kCCOptionECBMode),
                        keyPtr.baseAddress, key.count,
                        nil,
                        blockPtr.baseAddress, block.count,
                        &out, out.count, &outLen)
            }
        }
        guard status == kCCSuccess, outLen == kCCBlockSizeAES128 else {
            throw CCCryptoError.aesFailure(status: status)
        }
        return Data(out)
    }

    private static func crypt(op: CCOperation, key: Data, iv: Data, input: Data) throws -> Data {
        precondition(key.count == kCCKeySizeAES128, "AES-128 key must be 16 bytes")
        precondition(iv.count == kCCBlockSizeAES128, "IV must be 16 bytes")
        precondition(input.count % kCCBlockSizeAES128 == 0, "input must be block aligned")
        var outLen = 0
        var out = [UInt8](repeating: 0, count: input.count)
        let status = key.withUnsafeBytes { keyPtr in
            iv.withUnsafeBytes { ivPtr in
                input.withUnsafeBytes { inPtr in
                    CCCrypt(op,
                            CCAlgorithm(kCCAlgorithmAES),
                            CCOptions(kCCOptionPKCS7Padding & 0), // 无填充
                            keyPtr.baseAddress, key.count,
                            ivPtr.baseAddress,
                            inPtr.baseAddress, input.count,
                            &out, out.count, &outLen)
                }
            }
        }
        guard status == kCCSuccess else {
            throw CCCryptoError.aesFailure(status: status)
        }
        return Data(out.prefix(outLen))
    }
}

enum CCCryptoError: Error, Equatable {
    case aesFailure(status: Int32)
    case invalidData(String)
}

// MARK: - CMAC-AES-128 (RFC4493)

enum CCCAesCmac {

    /// RFC4493 子密钥生成: L=AES_K(0^128); K1/K2
    static func subkeys(key: Data) throws -> (k1: Data, k2: Data) {
        let zero = Data(repeating: 0, count: kCCBlockSizeAES128)
        let l = try CCCAESCrypto.ecbEncryptBlock(key: key, block: zero)
        let k1 = leftShiftOne(l)
        let k2 = leftShiftOne(k1)
        return (k1, k2)
    }

    private static func leftShiftOne(_ input: Data) -> Data {
        // 若最高位为 0: 左移 1; 否则左移 1 后 XOR 0x87
        let rb: UInt8 = 0x87
        var out = [UInt8](repeating: 0, count: kCCBlockSizeAES128)
        let msb = input[0] & 0x80
        var carry: UInt8 = 0
        for i in stride(from: kCCBlockSizeAES128 - 1, through: 0, by: -1) {
            let b = input[i]
            out[i] = (b << 1) | carry
            carry = (b & 0x80) >> 7
        }
        if msb != 0 { out[kCCBlockSizeAES128 - 1] ^= rb }
        return Data(out)
    }

    /// RFC4493 §2.4 CMAC 计算, 输出 16 字节
    static func cmac(key: Data, message: Data) throws -> Data {
        let (k1, k2) = try subkeys(key: key)
        let blockSize = kCCBlockSizeAES128
        let n = max(1, (message.count + blockSize - 1) / blockSize)

        // 最后一块处理: 完整用 K1, 不完整 padding 后 XOR K2
        let lastStart = (n - 1) * blockSize
        var lastBlock: Data
        let isComplete = message.count > 0 && (message.count % blockSize == 0)
        if isComplete && message.count >= blockSize {
            lastBlock = message.subdata(in: lastStart..<message.count)
            lastBlock = xor(lastBlock, k1)
        } else {
            var padded = Data(message.subdata(in: lastStart..<message.count))
            padded.append(0x80)
            while padded.count < blockSize { padded.append(0x00) }
            lastBlock = xor(padded, k2)
        }

        // CBC-MAC: 前 n-1 块与最后一块
        var x = Data(repeating: 0, count: blockSize)
        for i in 0..<(n - 1) {
            let block = message.subdata(in: i * blockSize..<(i + 1) * blockSize)
            x = try CCCAESCrypto.ecbEncryptBlock(key: key, block: xor(x, block))
        }
        let finalBlock = xor(x, lastBlock)
        return try CCCAESCrypto.ecbEncryptBlock(key: key, block: finalBlock)
    }

    /// C-MAC / R-MAC: CMAC 输出截断为 8 字节 (规范 §18.4.12/13)
    static func mac8(key: Data, message: Data) throws -> Data {
        try cmac(key: key, message: message).prefix(8)
    }

    private static func xor(_ a: Data, _ b: Data) -> Data {
        precondition(a.count == b.count)
        var out = [UInt8](repeating: 0, count: a.count)
        for i in 0..<a.count { out[i] = a[i] ^ b[i] }
        return Data(out)
    }
}

// MARK: - ISO/IEC 9797-1 Padding Method 2 (SCP03 填充)

enum CccPadding {
    /// 0x80 后补零到 16 字节边界 (SCP03 标准填充)
    static func pad(_ data: Data) -> Data {
        var out = data
        out.append(0x80)
        while out.count % kCCBlockSizeAES128 != 0 { out.append(0x00) }
        return out
    }

    /// 去除 0x80 填充
    static func unpad(_ data: Data) throws -> Data {
        guard data.count % kCCBlockSizeAES128 == 0, !data.isEmpty else {
            throw CCCryptoError.invalidData("unpad: bad length")
        }
        var idx = data.count - 1
        while idx >= 0 && data[idx] == 0x00 { idx -= 1 }
        guard idx >= 0, data[idx] == 0x80 else {
            throw CCCryptoError.invalidData("unpad: missing 0x80 marker")
        }
        return data.prefix(idx)
    }
}

// MARK: - CCC 安全通道 (GPC_SPE_014 SCP03 风格)

/// CCC Secure Channel — 命令加密 + 响应解密/验证。
///
/// 状态: 维护命令计数器 (01h 起) 与 MAC Chaining Value (16 字节)。
/// 线程安全: 一个实例对应一条 L2CAP 连接, 按序使用。
final class CCCSecureChannel {

    private let keys: SystemKeySet
    private var commandCounter: UInt8 = 0x01
    private var macChainingValue = Data(repeating: 0, count: 16)
    private var lastCommandCounter: UInt8 = 0x00

    init(keys: SystemKeySet) {
        self.keys = keys
    }

    /// 加密并认证命令 (Listing 18-10)
    /// - Returns: (密文载荷, C-MAC 8 字节)
    func encryptCommand(_ plaintext: Data) throws -> (ciphertext: Data, mac: Data) {
        // 1. Padded Counter Block: 0000...00 || counter
        var iv = Data(repeating: 0, count: 15)
        iv.append(commandCounter)

        // 2. S-ENC: AES-128-CBC(Kenc, ICV, pad(payload))
        let padded = CccPadding.pad(plaintext)
        let ciphertext = try CCCAESCrypto.cbcEncrypt(key: keys.kenc, iv: iv, plaintext: padded)

        // 3. S-MAC: C-MAC = CMAC(Kmac, MAC_Chaining_Value || ciphertext)[0:8]
        var macInput = macChainingValue
        macInput.append(ciphertext)
        let mac = try CCCAesCmac.mac8(key: keys.kmac, message: macInput)

        // 4. 更新 chaining: 完整 16 字节 CMAC 输出
        macChainingValue = try CCCAesCmac.cmac(key: keys.kmac, message: macInput)

        lastCommandCounter = commandCounter
        if commandCounter < 0xFF { commandCounter += 1 }
        return (ciphertext, mac)
    }

    /// 解密并验证响应 (Listing 18-11)
    /// - Parameters:
    ///   - ciphertext: 响应的加密载荷
    ///   - rmac: R-MAC (8 字节)
    /// - Returns: 明文 (已去填充)
    func decryptResponse(ciphertext: Data, rmac: Data) throws -> Data {
        // 1. Padded Counter Block: 8000...00 || command counter
        var iv = Data(repeating: 0, count: 16)
        iv[0] = 0x80
        iv[15] = lastCommandCounter

        // 2. 验证 R-MAC = CMAC(Krmac, command MAC_Chaining_Value || ciphertext)[0:8]
        var macInput = Data(repeating: 0, count: 16) // 响应用命令的 MAC Chaining Value (首条命令为全零)
        macInput.append(ciphertext)
        let expected = try CCCAesCmac.mac8(key: keys.krmac, message: macInput)
        guard rmac == expected else {
            throw CCCryptoError.invalidData("R-MAC verification failed")
        }

        // 3. S-ENC: AES-128-CBC 解密
        let padded = try CCCAESCrypto.cbcDecrypt(key: keys.kenc, iv: iv, ciphertext: ciphertext)
        return try CccPadding.unpad(padded)
    }

    /// 重置通道状态 (新连接/新会话)
    func reset() {
        commandCounter = 0x01
        macChainingValue = Data(repeating: 0, count: 16)
        lastCommandCounter = 0x00
    }
}
