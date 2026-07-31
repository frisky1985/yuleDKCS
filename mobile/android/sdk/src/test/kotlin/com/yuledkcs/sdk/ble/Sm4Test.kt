package com.yuledkcs.sdk.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * SM4 算法测试 — 2b-F
 *
 * 标准测试向量 (GM/T 0002-2012 附录 A):
 *   密钥:  0123456789ABCDEFFEDCBA9876543210
 *   明文:  0123456789ABCDEFFEDCBA9876543210
 *   密文:  681EDF34D206965E86B3E94F536E4246
 */
class Sm4Test {

    // ─── 测试辅助 ─────────────────────────────────────────

    private fun hexToBytes(hex: String): ByteArray =
        ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }

    private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it) }

    private val standardKey = hexToBytes("0123456789ABCDEFFEDCBA9876543210")
    private val standardPlain = hexToBytes("0123456789ABCDEFFEDCBA9876543210")
    private val standardCipher = hexToBytes("681EDF34D206965E86B3E94F536E4246")

    // ─── ECB 标准向量 ─────────────────────────────────────

    @Test
    fun `ecb encrypt matches GM-T 0002-2012 standard vector`() {
        val cipher = Sm4.ecbEncrypt(standardKey, standardPlain)
        assertEquals("681edf34d206965e86b3e94f536e4246", cipher.toHex())
        assertArrayEquals(standardCipher, cipher)
    }

    @Test
    fun `ecb decrypt roundtrips`() {
        val plain = Sm4.ecbDecrypt(standardKey, standardCipher)
        assertArrayEquals(standardPlain, plain)
    }

    @Test
    fun `ecb encrypt multiple blocks`() {
        val plain = standardPlain + standardPlain
        val cipher = Sm4.ecbEncrypt(standardKey, plain)
        assertEquals(32, cipher.size)
        // ECB 模式下两个相同分组产生相同密文
        assertEquals(cipher.copyOfRange(0, 16).toHex(), cipher.copyOfRange(16, 32).toHex())
        assertArrayEquals(standardPlain, Sm4.ecbDecrypt(standardKey, cipher))
    }

    @Test
    fun `ecb requires multiple of 16 bytes`() {
        assertThrows(IllegalArgumentException::class.java) {
            Sm4.ecbEncrypt(standardKey, ByteArray(15))
        }
        assertThrows(IllegalArgumentException::class.java) {
            Sm4.ecbDecrypt(standardKey, ByteArray(0))
        }
    }

    @Test
    fun `key must be 16 bytes`() {
        assertThrows(IllegalArgumentException::class.java) {
            Sm4.ecbEncrypt(ByteArray(15), standardPlain)
        }
    }

    // ─── CBC ──────────────────────────────────────────────

    @Test
    fun `cbc encrypt decrypt roundtrip`() {
        val plain = hexToBytes("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F")
        val iv = Sm4.ZERO_IV
        val cipher = Sm4.cbcEncrypt(standardKey, iv, plain)
        assertEquals(32, cipher.size)
        assertArrayEquals(plain, Sm4.cbcDecrypt(standardKey, iv, cipher))
    }

    @Test
    fun `cbc first block equals ecb when iv is zero`() {
        val block1 = hexToBytes("000102030405060708090A0B0C0D0E0F")
        val block2 = hexToBytes("101112131415161718191A1B1C1D1E1F")
        val plain = block1 + block2
        val iv = Sm4.ZERO_IV

        val cbc = Sm4.cbcEncrypt(standardKey, iv, plain)
        val ecb1 = Sm4.ecbEncrypt(standardKey, block1)

        // 第一块: C1 = E(P1 ⊕ IV) = E(P1) 当 IV 全零
        assertEquals(ecb1.toHex(), cbc.copyOfRange(0, 16).toHex())
    }

    @Test
    fun `cbc chaining makes second block depend on first`() {
        val block2 = hexToBytes("101112131415161718191A1B1C1D1E1F")
        val plain = hexToBytes("000102030405060708090A0B0C0D0E0F") + block2
        val iv = Sm4.ZERO_IV

        val cbc = Sm4.cbcEncrypt(standardKey, iv, plain)
        val ecb2 = Sm4.ecbEncrypt(standardKey, block2)

        // 第二块: C2 = E(P2 ⊕ C1) ≠ E(P2)
        assertTrue(cbc.copyOfRange(16, 32).toHex() != ecb2.toHex())
    }

    @Test
    fun `cbc requires 16-byte iv`() {
        assertThrows(IllegalArgumentException::class.java) {
            Sm4.cbcEncrypt(standardKey, ByteArray(8), standardPlain)
        }
    }

    // ─── PKCS#7 填充 ──────────────────────────────────────

    @Test
    fun `pkcs7 pad and unpad roundtrip`() {
        val data = hexToBytes("000102030405060708090A0B0C0D0E0F")
        val padded = Sm4.pkcs7Pad(data)
        assertEquals(32, padded.size)
        assertEquals(0x10, padded[31].toInt() and 0xFF)
        assertArrayEquals(data, Sm4.pkcs7Unpad(padded))
    }

    @Test
    fun `pkcs7 pad empty data produces full block`() {
        val padded = Sm4.pkcs7Pad(ByteArray(0))
        assertEquals(16, padded.size)
        padded.forEach { assertEquals(0x10, it.toInt() and 0xFF) }
        assertEquals(0, Sm4.pkcs7Unpad(padded).size)
    }

    @Test
    fun `pkcs7 unpad rejects invalid padding`() {
        // 填充长度 0 非法
        assertThrows(IllegalArgumentException::class.java) {
            Sm4.pkcs7Unpad(ByteArray(16))
        }
        // 填充字节不一致非法
        val bad = ByteArray(16) { 0x00 }
        bad[15] = 0x02
        assertThrows(IllegalArgumentException::class.java) {
            Sm4.pkcs7Unpad(bad)
        }
    }
}
