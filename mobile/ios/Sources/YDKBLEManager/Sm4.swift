import Foundation

// MARK: - SM4 分组密码 (GM/T 0002-2012 / GB/T 32907-2016)

/// SM4 分组密码算法 — 2b-F (ICCE 会话加密用, 裁决 AD-7)
///
/// 纯 Swift 实现, 移植自已通过标准测试向量的
/// `mobile/android/sdk/.../ble/Sm4.kt` (与 embedded/icce_protocol/src/crypto/sm4.c 同源)。
/// 128 位密钥 / 128 位分组 / 32 轮 Feistel 结构, 无第三方依赖。
///
/// 标准测试向量 (GM/T 0002-2012 附录 A):
///   密钥:  0123456789ABCDEFFEDCBA9876543210
///   明文:  0123456789ABCDEFFEDCBA9876543210
///   密文:  681EDF34D206965E86B3E94F536E4246
///
/// 模式: ECB / CBC (PKCS#7 填充由调用方通过 [pkcs7Pad] 应用)。
public enum Sm4 {

    /// 密钥长度 (字节)
    public static let keySize = 16
    /// 分组长度 (字节)
    public static let blockSize = 16

    /// 全零 IV — 仅用于无 IV 协商的调试/默认路径, 生产环境应由密钥协商提供
    public static let zeroIV: [UInt8] = [UInt8](repeating: 0, count: blockSize)

    // ─── S 盒 (16×16, 与 sm4.c / Sm4.kt 一致) ─────────────────────────
    private static let sboxTable: [UInt8] = [
        0xD6, 0x90, 0xE9, 0xFE, 0xCC, 0xE1, 0x3D, 0xB7, 0x16, 0xB6, 0x14, 0xC2, 0x28, 0xFB, 0x2C, 0x05,
        0x2B, 0x67, 0x9A, 0x76, 0x2A, 0xBE, 0x04, 0xC3, 0xAA, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
        0x9C, 0x42, 0x50, 0xF4, 0x91, 0xEF, 0x98, 0x7A, 0x33, 0x54, 0x0B, 0x43, 0xED, 0xCF, 0xAC, 0x62,
        0xE4, 0xB3, 0x1C, 0xA9, 0xC9, 0x08, 0xE8, 0x95, 0x80, 0xDF, 0x94, 0xFA, 0x75, 0x8F, 0x3F, 0xA6,
        0x47, 0x07, 0xA7, 0xFC, 0xF3, 0x73, 0x17, 0xBA, 0x83, 0x59, 0x3C, 0x19, 0xE6, 0x85, 0x4F, 0xA8,
        0x68, 0x6B, 0x81, 0xB2, 0x71, 0x64, 0xDA, 0x8B, 0xF8, 0xEB, 0x0F, 0x4B, 0x70, 0x56, 0x9D, 0x35,
        0x1E, 0x24, 0x0E, 0x5E, 0x63, 0x58, 0xD1, 0xA2, 0x25, 0x22, 0x7C, 0x3B, 0x01, 0x21, 0x78, 0x87,
        0xD4, 0x00, 0x46, 0x57, 0x9F, 0xD3, 0x27, 0x52, 0x4C, 0x36, 0x02, 0xE7, 0xA0, 0xC4, 0xC8, 0x9E,
        0xEA, 0xBF, 0x8A, 0xD2, 0x40, 0xC7, 0x38, 0xB5, 0xA3, 0xF7, 0xF2, 0xCE, 0xF9, 0x61, 0x15, 0xA1,
        0xE0, 0xAE, 0x5D, 0xA4, 0x9B, 0x34, 0x1A, 0x55, 0xAD, 0x93, 0x32, 0x30, 0xF5, 0x8C, 0xB1, 0xE3,
        0x1D, 0xF6, 0xE2, 0x2E, 0x82, 0x66, 0xCA, 0x60, 0xC0, 0x29, 0x23, 0xAB, 0x0D, 0x53, 0x4E, 0x6F,
        0xD5, 0xDB, 0x37, 0x45, 0xDE, 0xFD, 0x8E, 0x2F, 0x03, 0xFF, 0x6A, 0x72, 0x6D, 0x6C, 0x5B, 0x51,
        0x8D, 0x1B, 0xAF, 0x92, 0xBB, 0xDD, 0xBC, 0x7F, 0x11, 0xD9, 0x5C, 0x41, 0x1F, 0x10, 0x5A, 0xD8,
        0x0A, 0xC1, 0x31, 0x88, 0xA5, 0xCD, 0x7B, 0xBD, 0x2D, 0x74, 0xD0, 0x12, 0xB8, 0xE5, 0xB4, 0xB0,
        0x89, 0x69, 0x97, 0x4A, 0x0C, 0x96, 0x77, 0x7E, 0x65, 0xB9, 0xF1, 0x09, 0xC5, 0x6E, 0xC6, 0x84,
        0x18, 0xF0, 0x7D, 0xEC, 0x3A, 0xDC, 0x4D, 0x20, 0x79, 0xEE, 0x5F, 0x3E, 0xD7, 0xCB, 0x39, 0x48
    ]

    /// 系统参数 FK
    private static let fk: [UInt32] = [
        0xA3B1BAC6, 0x56AA3350, 0x677D9197, 0xB27022DC
    ]

    /// 轮常数 CK (32 个)
    private static let ck: [UInt32] = [
        0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269,
        0x70777E85, 0x8C939AA1, 0xA8AFB6BD, 0xC4CBD2D9,
        0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249,
        0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9,
        0xC0C7CED5, 0xDCE3EAF1, 0xF8FF060D, 0x141B2229,
        0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299,
        0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209,
        0x10171E25, 0x2C333A41, 0x484F565D, 0x646B7279
    ]

    // ─── 错误 ──────────────────────────────────────────────────────

    /// SM4 使用错误
    public enum Sm4Error: Error, Equatable {
        case invalidKeySize(Int)
        case invalidIvSize(Int)
        case invalidBlockLength(Int)
        case invalidPadding
    }

    // ─── 密钥上下文 (32 轮轮密钥) ─────────────────────────────────

    /// SM4 密钥上下文 (32 轮轮密钥)
    public struct Key {
        let rk: [UInt32]

        /// 密钥扩展:
        ///   K[0..3] = MK ⊕ FK
        ///   rk[i] = K[i+4] = K[i] ⊕ T'(K[i+1] ⊕ K[i+2] ⊕ K[i+3] ⊕ CK[i])
        public init(key: [UInt8]) throws {
            guard key.count == keySize else {
                throw Sm4Error.invalidKeySize(key.count)
            }
            var k = [UInt32](repeating: 0, count: 36)
            k[0] = loadBe32(key, 0) ^ fk[0]
            k[1] = loadBe32(key, 4) ^ fk[1]
            k[2] = loadBe32(key, 8) ^ fk[2]
            k[3] = loadBe32(key, 12) ^ fk[3]
            var rk = [UInt32](repeating: 0, count: 32)
            for i in 0..<32 {
                k[i + 4] = k[i] ^ tp(k[i + 1] ^ k[i + 2] ^ k[i + 3] ^ ck[i])
                rk[i] = k[i + 4]
            }
            self.rk = rk
        }
    }

    // ─── 内部变换 ─────────────────────────────────────────────────

    private static func rotl(_ x: UInt32, _ n: Int) -> UInt32 {
        (x << n) | (x >> (32 - n))
    }

    /// S 盒替换: 32 位输入逐字节查表
    private static func sbox(_ x: UInt32) -> UInt32 {
        let b0 = UInt32(sboxTable[Int((x >> 24) & 0xFF)])
        let b1 = UInt32(sboxTable[Int((x >> 16) & 0xFF)])
        let b2 = UInt32(sboxTable[Int((x >> 8) & 0xFF)])
        let b3 = UInt32(sboxTable[Int(x & 0xFF)])
        return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3
    }

    /// L(B) = B ⊕ (B ≪ 2) ⊕ (B ≪ 10) ⊕ (B ≪ 18) ⊕ (B ≪ 24)
    private static func l(_ b: UInt32) -> UInt32 {
        b ^ rotl(b, 2) ^ rotl(b, 10) ^ rotl(b, 18) ^ rotl(b, 24)
    }

    /// L'(B) = B ⊕ (B ≪ 13) ⊕ (B ≪ 23)
    private static func lp(_ b: UInt32) -> UInt32 {
        b ^ rotl(b, 13) ^ rotl(b, 23)
    }

    /// T = L(S_box(B))
    private static func t(_ x: UInt32) -> UInt32 { l(sbox(x)) }

    /// T' = L'(S_box(B))
    private static func tp(_ x: UInt32) -> UInt32 { lp(sbox(x)) }

    private static func loadBe32(_ b: [UInt8], _ off: Int) -> UInt32 {
        (UInt32(b[off]) << 24) | (UInt32(b[off + 1]) << 16) |
            (UInt32(b[off + 2]) << 8) | UInt32(b[off + 3])
    }

    private static func storeBe32(_ b: inout [UInt8], _ off: Int, _ v: UInt32) {
        b[off] = UInt8((v >> 24) & 0xFF)
        b[off + 1] = UInt8((v >> 16) & 0xFF)
        b[off + 2] = UInt8((v >> 8) & 0xFF)
        b[off + 3] = UInt8(v & 0xFF)
    }

    // ─── 单块加解密 (32 轮 Feistel, 反序输出) ───────────────────────

    /// 单块加密: X[i+4] = X[i] ⊕ T(X[i+1] ⊕ X[i+2] ⊕ X[i+3] ⊕ rk[i]); 输出 (X35,X34,X33,X32)
    private static func encryptBlock(_ key: Key, _ input: [UInt8], _ inOff: Int,
                                     _ output: inout [UInt8], _ outOff: Int) {
        var x = [UInt32](repeating: 0, count: 36)
        x[0] = loadBe32(input, inOff)
        x[1] = loadBe32(input, inOff + 4)
        x[2] = loadBe32(input, inOff + 8)
        x[3] = loadBe32(input, inOff + 12)
        for i in 0..<32 {
            x[i + 4] = x[i] ^ t(x[i + 1] ^ x[i + 2] ^ x[i + 3] ^ key.rk[i])
        }
        storeBe32(&output, outOff, x[35])
        storeBe32(&output, outOff + 4, x[34])
        storeBe32(&output, outOff + 8, x[33])
        storeBe32(&output, outOff + 12, x[32])
    }

    /// 单块解密: 轮密钥反序使用
    private static func decryptBlock(_ key: Key, _ input: [UInt8], _ inOff: Int,
                                     _ output: inout [UInt8], _ outOff: Int) {
        var x = [UInt32](repeating: 0, count: 36)
        x[0] = loadBe32(input, inOff)
        x[1] = loadBe32(input, inOff + 4)
        x[2] = loadBe32(input, inOff + 8)
        x[3] = loadBe32(input, inOff + 12)
        for i in 0..<32 {
            x[i + 4] = x[i] ^ t(x[i + 1] ^ x[i + 2] ^ x[i + 3] ^ key.rk[31 - i])
        }
        storeBe32(&output, outOff, x[35])
        storeBe32(&output, outOff + 4, x[34])
        storeBe32(&output, outOff + 8, x[33])
        storeBe32(&output, outOff + 12, x[32])
    }

    // ─── ECB 模式 ───────────────────────────────────────────────────

    /// ECB 加密; 明文长度必须是 16 的倍数 (调用方先用 [pkcs7Pad] 填充)
    public static func ecbEncrypt(key: [UInt8], plain: [UInt8]) throws -> [UInt8] {
        guard !plain.isEmpty, plain.count % blockSize == 0 else {
            throw Sm4Error.invalidBlockLength(plain.count)
        }
        let k = try Key(key: key)
        var out = [UInt8](repeating: 0, count: plain.count)
        var i = 0
        while i < plain.count {
            encryptBlock(k, plain, i, &out, i)
            i += blockSize
        }
        return out
    }

    /// ECB 解密; 密文长度必须是 16 的倍数
    public static func ecbDecrypt(key: [UInt8], cipher: [UInt8]) throws -> [UInt8] {
        guard !cipher.isEmpty, cipher.count % blockSize == 0 else {
            throw Sm4Error.invalidBlockLength(cipher.count)
        }
        let k = try Key(key: key)
        var out = [UInt8](repeating: 0, count: cipher.count)
        var i = 0
        while i < cipher.count {
            decryptBlock(k, cipher, i, &out, i)
            i += blockSize
        }
        return out
    }

    // ─── CBC 模式 ───────────────────────────────────────────────────

    /// CBC 加密; 明文长度必须是 16 的倍数 (调用方先用 [pkcs7Pad] 填充)
    public static func cbcEncrypt(key: [UInt8], iv: [UInt8], plain: [UInt8]) throws -> [UInt8] {
        guard iv.count == blockSize else { throw Sm4Error.invalidIvSize(iv.count) }
        guard !plain.isEmpty, plain.count % blockSize == 0 else {
            throw Sm4Error.invalidBlockLength(plain.count)
        }
        let k = try Key(key: key)
        var out = [UInt8](repeating: 0, count: plain.count)
        var block = iv
        var i = 0
        while i < plain.count {
            for j in 0..<blockSize {
                block[j] = block[j] ^ plain[i + j]
            }
            encryptBlock(k, block, 0, &out, i)
            for j in 0..<blockSize {
                block[j] = out[i + j]
            }
            i += blockSize
        }
        return out
    }

    /// CBC 解密; 密文长度必须是 16 的倍数
    public static func cbcDecrypt(key: [UInt8], iv: [UInt8], cipher: [UInt8]) throws -> [UInt8] {
        guard iv.count == blockSize else { throw Sm4Error.invalidIvSize(iv.count) }
        guard !cipher.isEmpty, cipher.count % blockSize == 0 else {
            throw Sm4Error.invalidBlockLength(cipher.count)
        }
        let k = try Key(key: key)
        var out = [UInt8](repeating: 0, count: cipher.count)
        var prev = iv
        var block = [UInt8](repeating: 0, count: blockSize)
        var i = 0
        while i < cipher.count {
            decryptBlock(k, cipher, i, &block, 0)
            for j in 0..<blockSize {
                out[i + j] = block[j] ^ prev[j]
            }
            for j in 0..<blockSize {
                prev[j] = cipher[i + j]
            }
            i += blockSize
        }
        return out
    }

    // ─── PKCS#7 填充 ────────────────────────────────────────────────

    /// PKCS#7 填充: 补足到 blockSize 倍数, 填充字节 = 填充长度 (1..blockSize)
    public static func pkcs7Pad(_ data: [UInt8], blockSize: Int = blockSize) throws -> [UInt8] {
        guard (1...255).contains(blockSize) else { throw Sm4Error.invalidPadding }
        let pad = blockSize - (data.count % blockSize)
        var out = data
        out.append(contentsOf: [UInt8](repeating: UInt8(pad), count: pad))
        return out
    }

    /// PKCS#7 去填充; 填充非法时抛 [Sm4Error.invalidPadding]
    public static func pkcs7Unpad(_ data: [UInt8], blockSize: Int = blockSize) throws -> [UInt8] {
        guard (1...255).contains(blockSize) else { throw Sm4Error.invalidPadding }
        guard !data.isEmpty, data.count % blockSize == 0 else {
            throw Sm4Error.invalidBlockLength(data.count)
        }
        let pad = Int(data[data.count - 1])
        guard pad >= 1, pad <= blockSize, pad <= data.count else { throw Sm4Error.invalidPadding }
        guard data.suffix(pad).allSatisfy({ Int($0) == pad }) else { throw Sm4Error.invalidPadding }
        return Array(data.dropLast(pad))
    }
}
