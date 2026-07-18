/**
 * DigitalKey SDK — BertlvEncoder 单元测试
 *
 * 验证:
 * - 编码 primitive / constructed 节点
 * - encodeInt / encodeLong / encodeString / encodeBoolean 辅助方法
 * - BertlvBuilder 工厂方法
 * - toByteArray / toHexString 输出
 */
package com.digitalkey.sdk.bertlv

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class BertlvEncoderTest {

    // ════════════════════════════════════════════
    // 基础编码
    // ════════════════════════════════════════════

    @Test
    fun `encode primitive node produces correct bytes`() {
        val encoder = BertlvEncoder()
        encoder.encodeNode(BertlvNode(0x05, byteArrayOf(0x01, 0x02)))
        val bytes = encoder.toByteArray()
        assertArrayEquals(byteArrayOf(0x05, 0x02, 0x01, 0x02), bytes)
    }

    @Test
    fun `encode constructed node with children`() {
        val child = BertlvNode(0x10, byteArrayOf(0xAB.toByte()))
        val parent = BertlvNode(0x30, byteArrayOf(), listOf(child))
        val encoder = BertlvEncoder()
        encoder.encodeNode(parent)
        val bytes = encoder.toByteArray()
        // 0x30, len=3, child tag=0x10, len=1, val=0xAB
        assertEquals(5, bytes.size)
        assertEquals(0x30.toByte(), bytes[0])
        assertEquals(0x03, bytes[1])
        assertEquals(0x10.toByte(), bytes[2])
        assertEquals(0x01, bytes[3])
        assertEquals(0xAB.toByte(), bytes[4])
    }

    @Test
    fun `encode and append multiple nodes`() {
        val encoder = BertlvEncoder()
        encoder.encode(0x01, byteArrayOf(0xAA.toByte()))
        encoder.encode(0x02, byteArrayOf(0xBB.toByte()))
        val bytes = encoder.toByteArray()
        assertArrayEquals(
            byteArrayOf(0x01, 0x01, 0xAA.toByte(), 0x02, 0x01, 0xBB.toByte()),
            bytes
        )
    }

    // ════════════════════════════════════════════
    // 辅助方法
    // ════════════════════════════════════════════

    @Test
    fun `encodeInt produces correct TLV`() {
        val encoder = BertlvEncoder().encodeInt(0x01, 255)
        val bytes = encoder.toByteArray()
        // Tag=0x01, Len=1, Val=0xFF
        assertArrayEquals(byteArrayOf(0x01, 0x01, 0xFF.toByte()), bytes)
    }

    @Test
    fun `encodeInt with multi-byte value`() {
        val encoder = BertlvEncoder().encodeInt(0x02, 0x0102)
        val bytes = encoder.toByteArray()
        assertArrayEquals(byteArrayOf(0x02, 0x02, 0x01, 0x02), bytes)
    }

    @Test
    fun `encodeLong produces correct big-endian bytes`() {
        val encoder = BertlvEncoder().encodeLong(0x10, 0x0102030405060708)
        val bytes = encoder.toByteArray()
        // Tag=0x10, Len=8
        assertEquals(10, bytes.size)
        assertEquals(0x10.toByte(), bytes[0])
        assertEquals(0x08, bytes[1])
        assertEquals(0x01, bytes[2])
        assertEquals(0x08, bytes[9])
    }

    @Test
    fun `encodeString produces UTF-8 encoded value`() {
        val encoder = BertlvEncoder().encodeString(0x20, "Hello")
        val bytes = encoder.toByteArray()
        assertEquals(0x20.toByte(), bytes[0])
        assertEquals(0x05, bytes[1])
        assertEquals("Hello".toByteArray(Charsets.UTF_8).size, 5)
    }

    @Test
    fun `encodeBoolean true`() {
        val encoder = BertlvEncoder().encodeBoolean(0x01, true)
        assertArrayEquals(byteArrayOf(0x01, 0x01, 0x01), encoder.toByteArray())
    }

    @Test
    fun `encodeBoolean false`() {
        val encoder = BertlvEncoder().encodeBoolean(0x01, false)
        assertArrayEquals(byteArrayOf(0x01, 0x01, 0x00), encoder.toByteArray())
    }

    // ════════════════════════════════════════════
    // BertlvBuilder 工厂方法
    // ════════════════════════════════════════════

    @Test
    fun `builder authenticate produces valid TLV`() {
        val keyId = byteArrayOf(0x01, 0x02, 0x03)
        val challenge = byteArrayOf(0xAA.toByte(), 0xBB.toByte())
        val bytes = BertlvBuilder.authenticate(keyId, challenge)
        // Decode and verify
        val decoder = BertlvDecoder(bytes)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(BertlvTag.AUTHENTICATE, nodes[0].tag)
        assertEquals(2, nodes[0].children.size)
        assertEquals(BertlvTag.KEY_ID, nodes[0].children[0].tag)
        assertEquals(BertlvTag.AUTH_DATA, nodes[0].children[1].tag)
    }

    @Test
    fun `builder vehicleCommand produces valid TLV`() {
        val keyId = byteArrayOf(0x01)
        val bytes = BertlvBuilder.vehicleCommand(BertlvTag.VEHICLE_UNLOCK, keyId, 1000L)
        val decoder = BertlvDecoder(bytes)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(BertlvTag.VEHICLE_CMD, nodes[0].tag)
        // Should have message_type, key_id, transaction_id, timestamp
        assertTrue(nodes[0].children.size >= 3)
    }

    @Test
    fun `builder vehicleUnlock and vehicleLock produce different commands`() {
        val keyId = byteArrayOf(0x01)
        val unlockBytes = BertlvBuilder.vehicleUnlock(keyId, 1L)
        val lockBytes = BertlvBuilder.vehicleLock(keyId, 2L)
        assertFalse("Unlock and Lock should differ", unlockBytes.contentEquals(lockBytes))
    }

    @Test
    fun `builder errorResponse produces valid TLV`() {
        val bytes = BertlvBuilder.errorResponse(0x0401, 100L, "test error")
        val decoder = BertlvDecoder(bytes)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
    }

    // ════════════════════════════════════════════
    // 标签编码 (Tag 长度)
    // ════════════════════════════════════════════

    @Test
    fun `encode single byte tag`() {
        val bytes = BertlvEncoder().encode(0x05, byteArrayOf(0x00)).toByteArray()
        assertEquals(0x05.toByte(), bytes[0])
    }

    @Test
    fun `encode multi-byte tag`() {
        val bytes = BertlvEncoder().encode(0x0100, byteArrayOf(0x00)).toByteArray()
        // Tag should be 2 bytes: 0x81 0x00
        assertEquals(4, bytes.size)
        assertEquals(0x81.toByte(), bytes[0])
    }

    // ════════════════════════════════════════════
    // toHexString
    // ════════════════════════════════════════════

    @Test
    fun `toHexString prints hex representation`() {
        val encoder = BertlvEncoder().encode(0x01, byteArrayOf(0xFF.toByte()))
        assertEquals("0101FF", encoder.toHexString())
    }

    @Test
    fun `BertlvNode toHexString works`() {
        val node = BertlvNode(0x01, byteArrayOf(0xAB.toByte()))
        assertEquals("0101AB", node.toHexString())
    }

    // ════════════════════════════════════════════
    // append
    // ════════════════════════════════════════════

    @Test
    fun `append combines encoders`() {
        val a = BertlvEncoder().encode(0x01, byteArrayOf(0xAA.toByte()))
        val b = BertlvEncoder().encode(0x02, byteArrayOf(0xBB.toByte()))
        val combined = BertlvEncoder().append(a).append(b)
        assertArrayEquals(
            byteArrayOf(0x01, 0x01, 0xAA.toByte(), 0x02, 0x01, 0xBB.toByte()),
            combined.toByteArray()
        )
    }
}
