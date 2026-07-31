package com.yuledkcs.sdk.ble

/**
 * SM4 分组密码算法 — 2b-F (GM/T 0002-2012 / GB/T 32907-2016)
 *
 * 纯 Kotlin 实现, 移植自仓库内已实现并验证的
 * embedded/icce_protocol/src/crypto/sm4.c (NXP KW47A + SE050 平台)。
 * 128 位密钥 / 128 位分组 / 32 轮 Feistel 结构, 无外部依赖。
 *
 * 标准测试向量 (GM/T 0002-2012 附录 A):
 *   密钥:  0123456789ABCDEFFEDCBA9876543210
 *   明文:  0123456789ABCDEFFEDCBA9876543210
 *   密文:  681EDF34D206965E86B3E94F536E4246
 *
 * 模式: ECB / CBC (PKCS#7 填充由调用方通过 [pkcs7Pad] 应用)。
 */
object Sm4 {

    const val KEY_SIZE = 16
    const val BLOCK_SIZE = 16

    /** 全零 IV — 仅用于无 IV 协商的调试/默认路径, 生产环境应由密钥协商提供 */
    val ZERO_IV = ByteArray(BLOCK_SIZE)

    // ─── S 盒 (16×16, 与 sm4.c 一致) ───────────────────────────────
    private val SBOX = intArrayOf(
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
    )

    /** 系统参数 FK */
    private val FK = intArrayOf(
        0xA3B1BAC6.toInt(), 0x56AA3350, 0x677D9197, 0xB27022DC.toInt()
    )

    /** 轮常数 CK (32 个) */
    private val CK = intArrayOf(
        0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269,
        0x70777E85, 0x8C939AA1, 0xA8AFB6BD, 0xC4CBD2D9,
        0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249,
        0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9,
        0xC0C7CED5, 0xDCE3EAF1, 0xF8FF060D, 0x141B2229,
        0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299,
        0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209,
        0x10171E25, 0x2C333A41, 0x484F565D, 0x646B7279
    )

    /** SM4 密钥上下文 (32 轮轮密钥) */
    class Key private constructor(internal val rk: IntArray) {
        companion object {
            /**
             * 密钥扩展:
             *   K[0..3] = MK ⊕ FK
             *   rk[i] = K[i+4] = K[i] ⊕ T'(K[i+1] ⊕ K[i+2] ⊕ K[i+3] ⊕ CK[i])
             */
            fun from(key: ByteArray): Key {
                require(key.size == KEY_SIZE) { "SM4 key must be $KEY_SIZE bytes, got ${key.size}" }
                val k = IntArray(36)
                k[0] = loadBe32(key, 0) xor FK[0]
                k[1] = loadBe32(key, 4) xor FK[1]
                k[2] = loadBe32(key, 8) xor FK[2]
                k[3] = loadBe32(key, 12) xor FK[3]
                val rk = IntArray(32)
                for (i in 0 until 32) {
                    k[i + 4] = k[i] xor tp(k[i + 1] xor k[i + 2] xor k[i + 3] xor CK[i])
                    rk[i] = k[i + 4]
                }
                return Key(rk)
            }
        }
    }

    // ─── 内部变换 ───────────────────────────────────────────────────

    private fun rotl(x: Int, n: Int): Int = Integer.rotateLeft(x, n)

    /** S 盒替换: 32 位输入逐字节查表 */
    private fun sbox(x: Int): Int {
        val b0 = SBOX[(x ushr 24) and 0xFF]
        val b1 = SBOX[(x ushr 16) and 0xFF]
        val b2 = SBOX[(x ushr 8) and 0xFF]
        val b3 = SBOX[x and 0xFF]
        return (b0 shl 24) or (b1 shl 16) or (b2 shl 8) or b3
    }

    /** L(B) = B ⊕ (B ≪ 2) ⊕ (B ≪ 10) ⊕ (B ≪ 18) ⊕ (B ≪ 24) */
    private fun l(b: Int): Int = b xor rotl(b, 2) xor rotl(b, 10) xor rotl(b, 18) xor rotl(b, 24)

    /** L'(B) = B ⊕ (B ≪ 13) ⊕ (B ≪ 23) */
    private fun lp(b: Int): Int = b xor rotl(b, 13) xor rotl(b, 23)

    /** T = L(S_box(B)) */
    private fun t(x: Int): Int = l(sbox(x))

    /** T' = L'(S_box(B)) */
    private fun tp(x: Int): Int = lp(sbox(x))

    private fun loadBe32(b: ByteArray, off: Int): Int =
        ((b[off].toInt() and 0xFF) shl 24) or ((b[off + 1].toInt() and 0xFF) shl 16) or
            ((b[off + 2].toInt() and 0xFF) shl 8) or (b[off + 3].toInt() and 0xFF)

    private fun storeBe32(b: ByteArray, off: Int, v: Int) {
        b[off] = (v ushr 24).toByte()
        b[off + 1] = (v ushr 16).toByte()
        b[off + 2] = (v ushr 8).toByte()
        b[off + 3] = v.toByte()
    }

    // ─── 单块加解密 (32 轮 Feistel, 反序输出) ───────────────────────

    /** 单块加密: X[i+4] = X[i] ⊕ T(X[i+1] ⊕ X[i+2] ⊕ X[i+3] ⊕ rk[i]); 输出 (X35,X34,X33,X32) */
    fun encryptBlock(key: Key, input: ByteArray, inOff: Int, output: ByteArray, outOff: Int) {
        val x = IntArray(36)
        x[0] = loadBe32(input, inOff)
        x[1] = loadBe32(input, inOff + 4)
        x[2] = loadBe32(input, inOff + 8)
        x[3] = loadBe32(input, inOff + 12)
        for (i in 0 until 32) {
            x[i + 4] = x[i] xor t(x[i + 1] xor x[i + 2] xor x[i + 3] xor key.rk[i])
        }
        storeBe32(output, outOff, x[35])
        storeBe32(output, outOff + 4, x[34])
        storeBe32(output, outOff + 8, x[33])
        storeBe32(output, outOff + 12, x[32])
    }

    /** 单块解密: 轮密钥反序使用 */
    fun decryptBlock(key: Key, input: ByteArray, inOff: Int, output: ByteArray, outOff: Int) {
        val x = IntArray(36)
        x[0] = loadBe32(input, inOff)
        x[1] = loadBe32(input, inOff + 4)
        x[2] = loadBe32(input, inOff + 8)
        x[3] = loadBe32(input, inOff + 12)
        for (i in 0 until 32) {
            x[i + 4] = x[i] xor t(x[i + 1] xor x[i + 2] xor x[i + 3] xor key.rk[31 - i])
        }
        storeBe32(output, outOff, x[35])
        storeBe32(output, outOff + 4, x[34])
        storeBe32(output, outOff + 8, x[33])
        storeBe32(output, outOff + 12, x[32])
    }

    // ─── ECB 模式 ───────────────────────────────────────────────────

    /** ECB 加密; 明文长度必须是 16 的倍数 (调用方先用 [pkcs7Pad] 填充) */
    fun ecbEncrypt(key: ByteArray, plain: ByteArray): ByteArray {
        require(plain.isNotEmpty() && plain.size % BLOCK_SIZE == 0) {
            "SM4 ECB requires plaintext length to be a multiple of $BLOCK_SIZE, got ${plain.size}"
        }
        val k = Key.from(key)
        val out = ByteArray(plain.size)
        for (i in plain.indices step BLOCK_SIZE) encryptBlock(k, plain, i, out, i)
        return out
    }

    /** ECB 解密; 密文长度必须是 16 的倍数 */
    fun ecbDecrypt(key: ByteArray, cipher: ByteArray): ByteArray {
        require(cipher.isNotEmpty() && cipher.size % BLOCK_SIZE == 0) {
            "SM4 ECB requires ciphertext length to be a multiple of $BLOCK_SIZE, got ${cipher.size}"
        }
        val k = Key.from(key)
        val out = ByteArray(cipher.size)
        for (i in cipher.indices step BLOCK_SIZE) decryptBlock(k, cipher, i, out, i)
        return out
    }

    // ─── CBC 模式 ───────────────────────────────────────────────────

    /** CBC 加密; 明文长度必须是 16 的倍数 */
    fun cbcEncrypt(key: ByteArray, iv: ByteArray, plain: ByteArray): ByteArray {
        require(iv.size == BLOCK_SIZE) { "SM4 CBC IV must be $BLOCK_SIZE bytes, got ${iv.size}" }
        require(plain.isNotEmpty() && plain.size % BLOCK_SIZE == 0) {
            "SM4 CBC requires plaintext length to be a multiple of $BLOCK_SIZE, got ${plain.size}"
        }
        val k = Key.from(key)
        val out = ByteArray(plain.size)
        val block = iv.copyOf()
        for (i in plain.indices step BLOCK_SIZE) {
            for (j in 0 until BLOCK_SIZE) {
                block[j] = (block[j].toInt() xor plain[i + j].toInt()).toByte()
            }
            encryptBlock(k, block, 0, out, i)
            System.arraycopy(out, i, block, 0, BLOCK_SIZE)
        }
        return out
    }

    /** CBC 解密; 密文长度必须是 16 的倍数 */
    fun cbcDecrypt(key: ByteArray, iv: ByteArray, cipher: ByteArray): ByteArray {
        require(iv.size == BLOCK_SIZE) { "SM4 CBC IV must be $BLOCK_SIZE bytes, got ${iv.size}" }
        require(cipher.isNotEmpty() && cipher.size % BLOCK_SIZE == 0) {
            "SM4 CBC requires ciphertext length to be a multiple of $BLOCK_SIZE, got ${cipher.size}"
        }
        val k = Key.from(key)
        val out = ByteArray(cipher.size)
        val prev = iv.copyOf()
        val block = ByteArray(BLOCK_SIZE)
        for (i in cipher.indices step BLOCK_SIZE) {
            decryptBlock(k, cipher, i, block, 0)
            for (j in 0 until BLOCK_SIZE) {
                out[i + j] = (block[j].toInt() xor prev[j].toInt()).toByte()
            }
            System.arraycopy(cipher, i, prev, 0, BLOCK_SIZE)
        }
        return out
    }

    // ─── PKCS#7 填充 ────────────────────────────────────────────────

    /** PKCS#7 填充: 补足到 blockSize 倍数, 填充字节 = 填充长度 (1..blockSize) */
    fun pkcs7Pad(data: ByteArray, blockSize: Int = BLOCK_SIZE): ByteArray {
        require(blockSize in 1..255) { "blockSize must be in 1..255" }
        val pad = blockSize - (data.size % blockSize)
        val out = data.copyOf(data.size + pad)
        for (i in data.size until out.size) out[i] = pad.toByte()
        return out
    }

    /** PKCS#7 去填充; 填充非法时抛 [IllegalArgumentException] */
    fun pkcs7Unpad(data: ByteArray, blockSize: Int = BLOCK_SIZE): ByteArray {
        require(blockSize in 1..255) { "blockSize must be in 1..255" }
        require(data.isNotEmpty() && data.size % blockSize == 0) { "padded data must be a non-empty multiple of blockSize" }
        val pad = data[data.size - 1].toInt() and 0xFF
        require(pad in 1..blockSize) { "invalid PKCS#7 padding length: $pad" }
        for (i in data.size - pad until data.size) {
            require((data[i].toInt() and 0xFF) == pad) { "invalid PKCS#7 padding bytes" }
        }
        return data.copyOf(data.size - pad)
    }
}
