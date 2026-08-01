package com.yuledkcs.sdk.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * ICCOA DK 3.0 帧编解码测试 — 2b-F
 *
 * 帧格式 (embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c:114-139):
 *   SOP(0xAA) | CMD_ID | SEQ(LE u16) | LEN(LE u16) | PAYLOAD | XOR校验(不含SOP) | EOP(0x55)
 */
class IcocaFrameTest {

    @Test
    fun `build produces expected frame layout`() {
        val payload = byteArrayOf(0x01, 0x02, 0x03)
        val frame = IcocaFrame.build(cmdId = 0x20, seqNum = 0x1234, payload = payload)

        // 6 (header) + 3 (payload) + 2 (trailer) = 11
        assertEquals(11, frame.size)

        // SOP / CMD
        assertEquals(0xAA, frame[0].toInt() and 0xFF)
        assertEquals(0x20, frame[1].toInt() and 0xFF)
        // SEQ 小端 LE (dk30.c:120): 0x1234 → [0x34, 0x12]
        assertEquals(0x34, frame[2].toInt() and 0xFF)
        assertEquals(0x12, frame[3].toInt() and 0xFF)
        // LEN 小端 LE (dk30.c:121): 3 → [0x03, 0x00]
        assertEquals(0x03, frame[4].toInt() and 0xFF)
        assertEquals(0x00, frame[5].toInt() and 0xFF)

        // payload
        assertArrayEquals(payload, frame.copyOfRange(6, 9))

        // XOR checksum 不含 SOP (dk30.c:131-132: raw+1 起 4+len 字节):
        //   0x20^0x34^0x12^0x03^0x00^0x01^0x02^0x03 = 0x05
        assertEquals(0x05, frame[9].toInt() and 0xFF)

        // EOP
        assertEquals(0x55, frame[10].toInt() and 0xFF)
    }

    @Test
    fun `build empty payload frame`() {
        val frame = IcocaFrame.build(cmdId = IcocaFrame.CMD_CTRL_RESPONSE, seqNum = 1, payload = ByteArray(0))
        assertEquals(8, frame.size)
        assertEquals(0xAA, frame[0].toInt() and 0xFF)
        assertEquals(0x55, frame[7].toInt() and 0xFF)
        // 校验不含 SOP = 0x21 ^ 0x01(seq lo) ^ 0x00(seq hi) ^ 0x00(len lo) ^ 0x00(len hi) = 0x20
        assertEquals(0x20, frame[6].toInt() and 0xFF)
    }

    @Test
    fun `parse roundtrips build`() {
        val payload = byteArrayOf(0x01, 0x02, 0x03, 0x04)
        val frame = IcocaFrame.build(cmdId = 0x20, seqNum = 0xABCD, payload = payload)

        val parsed = IcocaFrame.parse(frame)
        assertTrue(parsed != null)
        assertEquals(0x20, parsed!!.cmdId)
        assertEquals(0xABCD, parsed.seqNum)
        assertArrayEquals(payload, parsed.payload)
        assertArrayEquals(frame, parsed.raw)
    }

    @Test
    fun `parse rejects corrupted checksum`() {
        val frame = IcocaFrame.build(cmdId = 0x20, seqNum = 1, payload = byteArrayOf(0x01))
        frame[6] = (frame[6].toInt() xor 0xFF).toByte() // 破坏校验和
        assertNull(IcocaFrame.parse(frame))
    }

    @Test
    fun `parse rejects wrong eop`() {
        val frame = IcocaFrame.build(cmdId = 0x20, seqNum = 1, payload = byteArrayOf(0x01))
        frame[frame.size - 1] = 0x54
        assertNull(IcocaFrame.parse(frame))
    }

    @Test
    fun `parse rejects truncated frame`() {
        val frame = IcocaFrame.build(cmdId = 0x20, seqNum = 1, payload = byteArrayOf(0x01, 0x02, 0x03))
        val truncated = frame.copyOf(frame.size - 1)
        assertNull(IcocaFrame.parse(truncated))
        assertNull(IcocaFrame.parse(ByteArray(0)))
        assertNull(IcocaFrame.parse(ByteArray(7)))
    }

    @Test
    fun `parse rejects length mismatch`() {
        val frame = IcocaFrame.build(cmdId = 0x20, seqNum = 1, payload = byteArrayOf(0x01))
        frame[4] = 0x05 // 声称 5 字节 payload 但实际只有 1 字节 (LEN 低字节, LE)
        assertNull(IcocaFrame.parse(frame))
    }

    @Test
    fun `build rejects out of range arguments`() {
        assertThrows(IllegalArgumentException::class.java) {
            IcocaFrame.build(cmdId = 0x100, seqNum = 1, payload = ByteArray(0))
        }
        assertThrows(IllegalArgumentException::class.java) {
            IcocaFrame.build(cmdId = 0x20, seqNum = 0x10000, payload = ByteArray(0))
        }
    }

    @Test
    fun `checksum excludes sop`() {
        // dk30.c:131-132: 校验和从 raw+1 起 (4+len) 字节 — SOP 不参与
        val payload = byteArrayOf(0x01, 0x02)
        val frame = IcocaFrame.build(cmdId = 0x30, seqNum = 0x0001, payload = payload)
        val expected = (0x30 xor 0x01 xor 0x00 xor 0x02 xor 0x00 xor 0x01 xor 0x02)
        assertEquals(expected, frame[8].toInt() and 0xFF)
    }

    @Test
    fun `checksum excludes sop even when payload absent`() {
        val frame = IcocaFrame.build(cmdId = 0x30, seqNum = 0x0001, payload = ByteArray(0))
        // 0x30 ^ 0x01 ^ 0x00 ^ 0x00 ^ 0x00 = 0x31 (SOP 0xAA 不参与)
        assertEquals(0x31, frame[6].toInt() and 0xFF)
    }
}
