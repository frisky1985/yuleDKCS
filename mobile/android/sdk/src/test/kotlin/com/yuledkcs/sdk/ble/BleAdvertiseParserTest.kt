package com.yuledkcs.sdk.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.UUID

/**
 * 2b-B 广告包解析测试 — 用构造的 AD Structure 字节数组验证解析逻辑。
 */
class BleAdvertiseParserTest {

    // ─── 测试辅助 ─────────────────────────────────────────

    /** 构造单个 AD Structure: [len][type][data...] */
    private fun ad(type: Int, vararg data: Int): ByteArray {
        val bytes = ByteArray(2 + data.size)
        bytes[0] = (data.size + 1).toByte()
        bytes[1] = type.toByte()
        data.forEachIndexed { i, v -> bytes[i + 2] = v.toByte() }
        return bytes
    }

    private fun concat(vararg arrays: ByteArray): ByteArray {
        var total = 0
        arrays.forEach { total += it.size }
        val out = ByteArray(total)
        var offset = 0
        arrays.forEach { a ->
            a.copyInto(out, offset)
            offset += a.size
        }
        return out
    }

    private fun parse(vararg ads: ByteArray) = BleAdvertiseParser.parse(concat(*ads))

    // ─── 基本解析 ─────────────────────────────────────────

    @Test
    fun `null and empty records return null`() {
        assertNull(BleAdvertiseParser.parse(null))
        assertNull(BleAdvertiseParser.parse(ByteArray(0)))
    }

    @Test
    fun `parses flags service uuids and manufacturer data`() {
        val parsed = parse(
            ad(0x01, 0x06),                          // Flags: LE General Discoverable
            ad(0x03, 0xF5, 0xFE),                    // Complete 16-bit Service UUID: 0xFEF5 (ICCOA)
            ad(0xFF, 0x23, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02) // Mfr: company 0x0123
        )
        assertNotNull(parsed)

        assertEquals(6, parsed!!.flags)
        assertEquals(listOf(0xFEF5), parsed.serviceUuids16)
        assertEquals(0x0123, parsed.manufacturerData?.companyId)
        assertEquals(8, parsed.manufacturerData?.data?.size)
        assertEquals(
            "AABBCCDDEEFF0102",
            parsed.manufacturerData?.data?.joinToString("") { "%02x".format(it) }
        )
        assertEquals(3, parsed.adStructures.size)
    }

    @Test
    fun `parses 16-bit service uuid list with multiple entries`() {
        val parsed = parse(
            ad(0x02, 0xF5, 0xFF, 0xF5, 0xFE) // Incomplete 16-bit UUID list: 0xFFF5, 0xFEF5
        )
        assertTrue(parsed!!.hasServiceUuid16(0xFFF5))
        assertTrue(parsed.hasServiceUuid16(0xFEF5))
        assertFalse(parsed.hasServiceUuid16(0x1234))
    }

    @Test
    fun `parses 128-bit service uuid`() {
        // 0000FEF5-0000-1000-8000-00805F9B34FB 的 16 字节小端编码
        val uuidBytes = intArrayOf(
            0xF5, 0xFE, 0x9B, 0x5F, 0x00, 0x00, 0x10, 0x00,
            0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
        )
        val parsed = parse(ad(0x07, *uuidBytes))

        val expected = UUID.fromString("0000FEF5-0000-1000-8000-00805F9B34FB")
        assertTrue(parsed!!.serviceUuids128.contains(expected))
        assertTrue(parsed.hasServiceUuid(BleUuids.ICCOA_SERVICE))
        assertFalse(parsed.hasServiceUuid(BleUuids.CCC_SERVICE))
    }

    @Test
    fun `hasServiceUuid matches full 128-bit uuid`() {
        val parsed = parse(ad(0x03, 0xF5, 0xFF)) // 0xFFF5 = CCC v4.0.0 DK Service (Table 19-6)
        assertTrue(parsed!!.hasServiceUuid(BleUuids.CCC_SERVICE))
        assertFalse(parsed.hasServiceUuid(BleUuids.ICCOA_SERVICE))
    }

    @Test
    fun `parseUuid128 matches android bluetooth uuid encoding`() {
        // 蓝牙 16-bit UUID 0x1234 → 00001234-0000-1000-8000-00805F9B34FB
        val bytes = byteArrayOf(
            0x34.toByte(), 0x12, 0x9B.toByte(), 0x5F,
            0x00, 0x00, 0x10, 0x00,
            0x80.toByte(), 0x00, 0x00, 0x80.toByte(),
            0x5F, 0x9B.toByte(), 0x34, 0xFB.toByte()
        )
        val uuid = BleAdvertiseParser.parseUuid128(bytes)
        assertEquals("00001234-0000-1000-8000-00805F9B34FB", uuid.toString())
    }

    // ─── 容错 ─────────────────────────────────────────────

    @Test
    fun `malformed truncated ad structure stops parsing gracefully`() {
        // 声明 10 字节但只有 1 字节数据 → 停止解析, 不抛异常
        val malformed = byteArrayOf(0x0A, 0x03, 0xF5.toByte())
        val parsed = BleAdvertiseParser.parse(malformed)
        assertNotNull(parsed)
        assertTrue(parsed!!.serviceUuids16.isEmpty())
    }

    @Test
    fun `zero length ad structure terminates parsing`() {
        // [0x00] 终止符 (类似 iBeacon 尾部填充) → 之前的结构仍解析成功
        val bytes = concat(ad(0x03, 0xF5, 0xFF), byteArrayOf(0x00), byteArrayOf(0x02, 0x01, 0x05))
        val parsed = BleAdvertiseParser.parse(bytes)
        assertTrue(parsed!!.serviceUuids16.contains(0xFFF5))
        assertEquals(1, parsed.adStructures.size) // 0x00 之后的结构被忽略
    }

    @Test
    fun `manufacturer data shorter than 2 bytes is ignored`() {
        val parsed = parse(ad(0xFF, 0x01))
        assertNull(parsed!!.manufacturerData)
    }

    @Test
    fun `manufacturer data toBytes roundtrips raw ad bytes`() {
        val mfr = BleAdvertiseParser.ManufacturerData(0x0123, byteArrayOf(0xAA.toByte(), 0xBB.toByte()))
        val raw = mfr.toBytes()
        assertEquals(4, raw.size)
        assertEquals("2301aabb", raw.joinToString("") { "%02x".format(it) })
    }
}
