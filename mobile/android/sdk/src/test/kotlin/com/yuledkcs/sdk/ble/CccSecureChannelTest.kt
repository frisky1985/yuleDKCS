package com.yuledkcs.sdk.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * CCC 2b-E 安全通道 + 帧格式测试
 *
 * 向量来源:
 * - RFC4493 CMAC-AES-128 标准向量 (与 cryptography 官方库交叉验证)
 * - RFC5869 HKDF-SHA256 标准向量
 * - CCC-TS-101 v4.0.0 Table 19-19 帧格式 + §19.3 示例
 */
class CccSecureChannelTest {

    private val cmacKey = byteArrayOf(
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    )

    // ─── RFC4493 CMAC ───────────────────────────────────────────────

    @Test
    fun `subkeys match RFC4493`() {
        val (k1, k2) = CccAesCmac.subkeys(cmacKey)
        assertEquals("fbeed618357133667c85e08f7236a8de", k1.toHexString())
        // K2 = K1<<1 XOR Rb (MSB(K1)=1)
        assertEquals("f7ddac306ae266ccf90bc11ee46d513b", k2.toHexString())
    }

    @Test
    fun `cmac empty message`() {
        val mac = CccAesCmac.cmac(cmacKey, ByteArray(0))
        assertEquals("bb1d6929e95937287fa37d129b756746", mac.toHexString())
    }

    @Test
    fun `cmac one block`() {
        val msg = byteArrayOf(
            0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
            0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
        )
        val mac = CccAesCmac.cmac(cmacKey, msg)
        assertEquals("070a16b46b4d4144f79bdd9dd04a287c", mac.toHexString())
    }

    @Test
    fun `cmac forty bytes`() {
        val msg = byteArrayOf(
            0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
            0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
            0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
            0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
            0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
        )
        val mac = CccAesCmac.cmac(cmacKey, msg)
        assertEquals("dfa66747de9ae63030ca32611497c827", mac.toHexString())
    }

    @Test
    fun `cmac sixty four bytes`() {
        val msg = byteArrayOf(
            0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
            0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
            0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
            0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
            0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
            0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
            0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
            0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
        )
        val mac = CccAesCmac.cmac(cmacKey, msg)
        assertEquals("51f0bebf7e3b9d92fc49741779363cfe", mac.toHexString())
    }

    @Test
    fun `mac8 truncation`() {
        val mac8 = CccAesCmac.mac8(cmacKey, byteArrayOf(1, 2, 3))
        assertEquals(8, mac8.size)
        assertArrayEquals(CccAesCmac.cmac(cmacKey, byteArrayOf(1, 2, 3)).copyOf(8), mac8)
    }

    // ─── RFC5869 HKDF ───────────────────────────────────────────────

    @Test
    fun `hkdf test case 1`() {
        val ikm = ByteArray(22) { 0x0b }
        val salt = byteArrayOf(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c)
        val info = byteArrayOf(0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9)
        val prk = CccSystemKeyDerivation.extract(salt, ikm)
        assertEquals("077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5", prk.toHexString())
        val okm = CccSystemKeyDerivation.expand(prk, info, 42)
        assertEquals(
            "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
            okm.toHexString()
        )
    }

    @Test
    fun `system keys derivation`() {
        val sk = ByteArray(32) { 0x42 }
        val keys = CccSystemKeyDerivation.deriveSystemKeys(sk)
        assertEquals(16, keys.kenc.size)
        assertEquals(16, keys.kmac.size)
        assertEquals(16, keys.krmac.size)
        assertEquals(16, keys.longTermSharedSecret.size)
        assertFalse(keys.kenc.contentEquals(keys.kmac))
        assertFalse(keys.kmac.contentEquals(keys.krmac))
        assertFalse(keys.krmac.contentEquals(keys.longTermSharedSecret))
        // 确定性
        val keys2 = CccSystemKeyDerivation.deriveSystemKeys(sk)
        assertEquals(keys, keys2)
    }

    // ─── 帧格式 (CCC-TS-101 Table 19-19) ───────────────────────────

    @Test
    fun `frame header is four bytes`() {
        assertEquals(4, CccFrame.HEADER_SIZE)
    }

    @Test
    fun `select apdu wire example`() {
        val apdu = byteArrayOf(
            0x00, 0xA4, 0x04, 0x00, 0x0D,
            0xA0, 0x00, 0x00, 0x08, 0x09, 0x43, 0x43, 0x43, 0x44, 0x4B, 0x41, 0x76, 0x31,
            0x00
        )
        assertEquals(19, apdu.size)
        val frame = CccFrame.build(CccFrame.MSG_TYPE_SE, CccFrame.MSG_ID_DK_APDU_RQ, apdu)
        assertEquals("010b001300a404000da000000809434343444b41763100", frame.toHexString())
    }

    @Test
    fun `frame round trip`() {
        val frame = CccFrame.build(1, 0x0B, byteArrayOf(0xDE.toByte(), 0xAD.toByte(), 0xBE.toByte(), 0xEF.toByte()))
        val parsed = CccFrame.parse(frame)
        assertTrue(parsed != null)
        assertEquals(1, parsed!!.messageType)
        assertEquals(0x0B, parsed.messageId)
        assertArrayEquals(byteArrayOf(0xDE.toByte(), 0xAD.toByte(), 0xBE.toByte(), 0xEF.toByte()), parsed.payload)
    }

    @Test
    fun `payload length mismatch rejected`() {
        val bad = byteArrayOf(0x01, 0x0B, 0x00, 0x10, 0x01, 0x02, 0x03)
        assertNull(CccFrame.parse(bad))
    }

    @Test
    fun `rfu bits masked`() {
        val frame = CccFrame.build(0x81, 0x0B, ByteArray(0))
        assertEquals(0x01, frame[0].toUInt8())
    }

    // ─── 安全通道 (SCP03 风格) ─────────────────────────────────────

    @Test
    fun `command encrypt then response decrypt`() {
        val keys = CccSystemKeyDerivation.deriveSystemKeys(ByteArray(32) { 0x11 })
        val channel = CccSecureChannel(keys)
        val (ciphertext, mac) = channel.encryptCommand("unlock command".toByteArray())

        assertEquals(0, ciphertext.size % 16) // 块对齐
        assertEquals(8, mac.size)             // C-MAC 8 字节

        // 响应 (payload 模拟 APDU 响应: SW1=0x90)
        val responsePlain = byteArrayOf(0x90, 0x00)
        val iv = ByteArray(16).also { it[0] = 0x80.toByte(); it[15] = 0x01 }
        val responseCipher = CccAesCrypto.cbcEncrypt(keys.kenc, iv, CccPadding.pad(responsePlain))
        val rmac = CccAesCmac.mac8(keys.krmac, ByteArray(16) + responseCipher)

        val decrypted = channel.decryptResponse(responseCipher, rmac)
        assertArrayEquals(responsePlain, decrypted)
    }

    @Test
    fun `tampered rmac rejected`() {
        val keys = CccSystemKeyDerivation.deriveSystemKeys(ByteArray(32) { 0x11 })
        val channel = CccSecureChannel(keys)
        channel.encryptCommand("unlock".toByteArray())

        val iv = ByteArray(16).also { it[0] = 0x80.toByte(); it[15] = 0x01 }
        val responseCipher = CccAesCrypto.cbcEncrypt(keys.kenc, iv, CccPadding.pad(byteArrayOf(0x90, 0x00)))
        assertThrows(IllegalStateException::class.java) {
            channel.decryptResponse(responseCipher, ByteArray(8) { 0xAA.toByte() })
        }
    }

    @Test
    fun `mac chaining changes`() {
        val keys = CccSystemKeyDerivation.deriveSystemKeys(ByteArray(32) { 0x11 })
        val channel = CccSecureChannel(keys)
        val (_, mac1) = channel.encryptCommand("first".toByteArray())
        val (_, mac2) = channel.encryptCommand("second".toByteArray())
        assertNotEquals(mac1.toHexString(), mac2.toHexString())
    }
}
