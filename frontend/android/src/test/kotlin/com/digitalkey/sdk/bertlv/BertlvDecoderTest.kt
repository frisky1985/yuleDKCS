/**
 * DigitalKey SDK — BertlvDecoder 单元测试
 *
 * 验证:
 * - 短格式/长格式标签解码
 * - 短格式/长格式长度解码 (0x81, 0x82, 0x83, 0x84)
 * - 构造节点递归解码
 * - 异常路径 (数据不足, 无效标签, 无效长度)
 * - 综合 roundtrip: encode → decode → verify
 */
package com.digitalkey.sdk.bertlv

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class BertlvDecoderTest {

    // ════════════════════════════════════════════
    // 短格式标签解码
    // ════════════════════════════════════════════

    @Test
    fun `decode single byte tag primitive`() {
        // Tag=0x05(长度2 单字节), Length=0x02, Value=[0x01, 0x02]
        val data = byteArrayOf(0x05, 0x02, 0x01, 0x02)
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(0x05, nodes[0].tag)
        assertArrayEquals(byteArrayOf(0x01, 0x02), nodes[0].value)
        assertTrue(nodes[0].children.isEmpty())
    }

    @Test
    fun `decode multiple top-level nodes`() {
        // Tag=0x01 Len=1 Val=0xAA, Tag=0x02 Len=1 Val=0xBB
        val data = byteArrayOf(0x01, 0x01, 0xAA.toByte(), 0x02, 0x01, 0xBB.toByte())
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(2, nodes.size)
        assertEquals(0x01, nodes[0].tag)
        assertEquals(0x02, nodes[1].tag)
        assertArrayEquals(byteArrayOf(0xAA.toByte()), nodes[0].value)
        assertArrayEquals(byteArrayOf(0xBB.toByte()), nodes[1].value)
    }

    // ════════════════════════════════════════════
    // 长格式标签解码 (0x1F)
    // ════════════════════════════════════════════

    @Test
    fun `decode multi-byte tag with 2 bytes`() {
        // Tag=0x1F81, Len=1, Val=0x00
        // First byte 0x9F (constructed=0x20 | 0x1F), second byte 0x81
        val data = byteArrayOf(0x9F.toByte(), 0x81.toByte(), 0x01, 0x00)
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        // tag = class|constructed|0x01 = 0xA1
        assertTrue("Tag should include constructed flag", (nodes[0].tag and 0x20) != 0)
    }

    // ════════════════════════════════════════════
    // 长度解码
    // ════════════════════════════════════════════

    @Test
    fun `decode short form length`() {
        // Tag=0x01, Length=0x05 (short form), Value=5 bytes
        val data = byteArrayOf(0x01, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05)
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(5, nodes[0].value.size)
    }

    @Test
    fun `decode long form length 0x81`() {
        // Length=0x81 0xFF → 255 bytes of value
        val value = ByteArray(255) { it.toByte() }
        val data = byteArrayOf(0x01, 0x81.toByte(), 0xFF.toByte()) + value
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(255, nodes[0].value.size)
    }

    @Test
    fun `decode long form length 0x82`() {
        // Length=0x82 0x01 0x00 → 256 bytes
        val value = ByteArray(256) { it.toByte() }
        val data = byteArrayOf(0x01, 0x82.toByte(), 0x01, 0x00) + value
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(256, nodes[0].value.size)
    }

    @Test
    fun `decode long form length 0x83`() {
        // Length=0x83 0x00 0x10 0x00 → 4096 bytes
        val value = ByteArray(4096) { 0x42.toByte() }
        val data = byteArrayOf(0x01, 0x83.toByte(), 0x00, 0x10, 0x00) + value
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(4096, nodes[0].value.size)
    }

    @Test
    fun `decode long form length 0x84`() {
        // Length=0x84 0x00 0x01 0x00 0x00 → 65536 bytes
        val value = ByteArray(65536) { 0x42.toByte() }
        val data = byteArrayOf(0x01, 0x84.toByte(), 0x00, 0x01, 0x00, 0x00) + value
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(65536, nodes[0].value.size)
    }

    // ════════════════════════════════════════════
    // 构造节点递归解码
    // ════════════════════════════════════════════

    @Test
    fun `decode constructed node with children`() {
        // Constructed tag 0x30, length=6
        //   Child 0x01 length=1 val=0xAA
        //   Child 0x02 length=1 val=0xBB
        val data = byteArrayOf(
            0x30, 0x06,
            0x01, 0x01, 0xAA.toByte(),
            0x02, 0x01, 0xBB.toByte()
        )
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(0x30, nodes[0].tag)
        assertEquals(2, nodes[0].children.size)
        assertEquals(0x01, nodes[0].children[0].tag)
        assertEquals(0x02, nodes[0].children[1].tag)
        assertArrayEquals(byteArrayOf(0xAA.toByte()), nodes[0].children[0].value)
        assertArrayEquals(byteArrayOf(0xBB.toByte()), nodes[0].children[1].value)
    }

    @Test
    fun `decode nested constructed nodes`() {
        // Outer(0x30, len=8)
        //   Middle(0x31, len=4)
        //     Inner(0x01, len=1, val=0xCC)
        val data = byteArrayOf(
            0x30, 0x08,
            0x31, 0x04,
            0x01, 0x01, 0xCC.toByte()
        )
        val decoder = BertlvDecoder(data)
        val nodes = decoder.decodeAll()
        assertEquals(1, nodes.size)
        assertEquals(1, nodes[0].children.size)
        assertEquals(1, nodes[0].children[0].children.size)
        assertArrayEquals(byteArrayOf(0xCC.toByte()), nodes[0].children[0].children[0].value)
    }

    // ════════════════════════════════════════════
    // 异常路径
    // ════════════════════════════════════════════

    @Test(expected = BertlvDecodeException::class)
    fun `decode empty data throws exception`() {
        BertlvDecoder(byteArrayOf()).decodeNode()
    }

    @Test(expected = BertlvDecodeException::class)
    fun `decode insufficient data for length throws`() {
        // Claimed length 10 but only 2 bytes available
        val data = byteArrayOf(0x01, 0x0A, 0x01, 0x02)
        BertlvDecoder(data).decodeAll()
    }

    @Test
    fun `decode invalid length encoding throws`() {
        // Length byte 0x85 (unsupported long form)
        val data = byteArrayOf(0x01, 0x85.toByte(), 0x01)
        try {
            BertlvDecoder(data).decodeAll()
            fail("Should have thrown BertlvDecodeException")
        } catch (e: BertlvDecodeException) {
            assertTrue(e.message!!.contains("Invalid length"))
        }
    }

    @Test
    fun `decode truncated multi-byte tag throws`() {
        // Tag start byte 0x1F (indicates multi-byte) but no more bytes
        val data = byteArrayOf(0x1F)
        try {
            BertlvDecoder(data).decodeAll()
            fail("Should have thrown for truncated multi-byte tag")
        } catch (e: BertlvDecodeException) {
            assertTrue(e.message!!.contains("Decode error"))
        }
    }

    // ════════════════════════════════════════════
    // remaining() 辅助方法
    // ════════════════════════════════════════════

    @Test
    fun `remaining returns empty when fully consumed`() {
        val data = byteArrayOf(0x01, 0x01, 0xAA.toByte())
        val decoder = BertlvDecoder(data)
        decoder.decodeAll()
        assertArrayEquals(byteArrayOf(), decoder.remaining())
    }

    @Test
    fun `remaining returns unconsumed bytes after decode`() {
        // First node consumes 3 bytes, 2 bytes remain
        val data = byteArrayOf(0x01, 0x01, 0xAA.toByte(), 0xFF.toByte(), 0xEE.toByte())
        val decoder = BertlvDecoder(data)
        decoder.decodeAll()
        val remaining = decoder.remaining()
        assertArrayEquals(byteArrayOf(0xFF.toByte(), 0xEE.toByte()), remaining)
    }

    // ════════════════════════════════════════════
    // Roundtrip: encode → decode
    // ════════════════════════════════════════════

    @Test
    fun `encode then decode primitive node roundtrip`() {
        val original = BertlvNode(0x15, byteArrayOf(0x01, 0x02, 0x03))
        val encoded = original.toBytes()
        val decoder = BertlvDecoder(encoded)
        val decoded = decoder.decodeAll()
        assertEquals(1, decoded.size)
        assertEquals(0x15, decoded[0].tag)
        assertArrayEquals(byteArrayOf(0x01, 0x02, 0x03), decoded[0].value)
    }

    @Test
    fun `encode then decode constructed node roundtrip`() {
        val child = BertlvNode(0x10, byteArrayOf(0xAB.toByte()))
        val parent = BertlvNode(0x30, byteArrayOf(), listOf(child))
        val encoded = parent.toBytes()
        val decoder = BertlvDecoder(encoded)
        val decoded = decoder.decodeAll()
        assertEquals(1, decoded.size)
        assertEquals(1, decoded[0].children.size)
        assertEquals(0x10, decoded[0].children[0].tag)
    }

    // ════════════════════════════════════════════
    // BertlvNode equals/hashCode
    // ════════════════════════════════════════════

    @Test
    fun `BertlvNode equality uses byte content`() {
        val a = BertlvNode(0x01, byteArrayOf(0x01, 0x02))
        val b = BertlvNode(0x01, byteArrayOf(0x01, 0x02))
        val c = BertlvNode(0x01, byteArrayOf(0x01, 0x03))
        assertEquals(a, b)
        assertEquals(a.hashCode(), b.hashCode())
        assertNotEquals(a, c)
    }
}
