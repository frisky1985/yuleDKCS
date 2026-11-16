package com.yuledkcs.sdk.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/**
 * CCC Digital Key v4.0 帧 — 跨语言固定字节向量测试
 *
 * 向量单一来源: `embedded/tests/protocol_vectors/ccc_frame_v4.h`
 * 车端 (test_ccc_frame.c) / iOS (CCCCommandFrameVectorsTests) / Android (本文件)
 * 必须使用完全相同的字节序列. 改动任一端都需要同步更新该头文件与三端测试.
 */
class CccFrameVectorsTest {

    // 向量 1: SELECT APDU (规范 §19.3 示例)
    private val selectApduPayload = byteArrayOf(
        0x00, 0xA4, 0x04, 0x00, 0x0D, 0xA0.toByte(), 0x00, 0x00,
        0x00, 0x80.toByte(), 0x94.toByte(), 0x34, 0x34, 0x34, 0x34, 0x44,
        0x4B, 0x41, 0x76, 0x31, 0x00
    )
    private val selectApduFrame = byteArrayOf(
        0x01, 0x0B, 0x00, 0x15,
        0x00, 0xA4.toByte(), 0x04, 0x00, 0x0D, 0xA0.toByte(), 0x00, 0x00,
        0x00, 0x80.toByte(), 0x94.toByte(), 0x34, 0x34, 0x34, 0x34, 0x44,
        0x4B, 0x41, 0x76, 0x31, 0x00
    )

    // 向量 2: DK Event Notification (车 → 手机)
    private val dkEventFrame = byteArrayOf(0x03, 0x0C, 0x00, 0x03, 0x01, 0x00, 0x45)

    // 负向向量
    private val invalidRfuFrame = byteArrayOf(
        0xC1.toByte(), 0x0B, 0x00, 0x04, 0xAA.toByte(), 0xBB.toByte(), 0xCC.toByte(), 0xDD.toByte()
    )
    private val truncatedFrame = byteArrayOf(
        0x01, 0x0B, 0x00, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05
    )
    private val stuffedFrame = byteArrayOf(
        0x01, 0x0B, 0x00, 0x02, 0x01, 0x02,
        0xAA.toByte(), 0xBB.toByte(), 0xCC.toByte(), 0xDD.toByte(), 0xEE.toByte()
    )

    @Test
    fun buildMatchesCanonicalSelectApduVector() {
        val frame = CccFrame.build(CccFrame.MSG_TYPE_SE, CccFrame.MSG_ID_DK_APDU_RQ, selectApduPayload)
        assertArrayEquals(selectApduFrame, frame)
    }

    @Test
    fun parseCanonicalSelectApduVector() {
        val frame = CccFrame.parse(selectApduFrame)!!
        assertEquals(CccFrame.MSG_TYPE_SE, frame.messageType)
        assertEquals(CccFrame.MSG_ID_DK_APDU_RQ, frame.messageId)
        assertArrayEquals(selectApduPayload, frame.payload)
    }

    @Test
    fun parseCanonicalDkEventVector() {
        val frame = CccFrame.parse(dkEventFrame)!!
        assertEquals(CccFrame.MSG_TYPE_DK_EVENT, frame.messageType)
        assertEquals(CccFrame.MSG_ID_DK_APDU_RS, frame.messageId)
        assertArrayEquals(byteArrayOf(0x01, 0x00, 0x45), frame.payload)
    }

    @Test
    fun parseRejectsNonZeroRfuBits() {
        assertNull(CccFrame.parse(invalidRfuFrame))
    }

    @Test
    fun parseRejectsTruncatedPayload() {
        assertNull(CccFrame.parse(truncatedFrame))
    }

    @Test
    fun parseRejectsStuffedPayload() {
        assertNull(CccFrame.parse(stuffedFrame))
    }

    @Test
    fun parseRejectsShortHeader() {
        assertNull(CccFrame.parse(byteArrayOf(0x01, 0x0B, 0x00)))
    }

    @Test(expected = IllegalArgumentException::class)
    fun buildRejectsMessageTypeOutOfRange() {
        CccFrame.build(0x40, CccFrame.MSG_ID_DK_APDU_RQ, ByteArray(0))
    }
}
