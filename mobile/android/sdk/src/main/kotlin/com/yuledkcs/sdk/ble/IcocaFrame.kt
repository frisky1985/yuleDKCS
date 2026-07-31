package com.yuledkcs.sdk.ble

/**
 * ICCOA DK 3.0 协议帧编解码 — 2b-F
 *
 * 帧格式 (来源: embedded/iccoa_protocol/docs/module_design.md §3.1.4,
 * ICCOA/T 002-2024 数字车钥匙3.0 BLE 章节):
 *
 *   [0]     SOP       起始符 0xAA
 *   [1]     CMD_ID    命令 ID
 *   [2-3]   SEQ_NUM   序列号 (大端 u16)
 *   [4-5]   LEN       payload 长度 (大端 u16)
 *   [6..]   PAYLOAD   负载 (DK 3.0 为明文或 SM4 加密负载)
 *   [..]    CHECKSUM  XOR 校验和 (SOP..PAYLOAD 全部字节异或)
 *   [..]    EOP       结束符 0x55
 *
 * DK 3.0 命令 ID:
 *   0x01 BIND_REQUEST / 0x02 BIND_RESPONSE / 0x03 UNBIND_REQUEST / 0x04 UNBIND_RESPONSE
 *   0x10 AUTH_REQUEST / 0x11 AUTH_RESPONSE
 *   0x20 CTRL_REQUEST / 0x21 CTRL_RESPONSE
 *   0x30 STATUS_NOTIFY / 0x40 KEY_SHARE / 0x41 KEY_SHARE_ACK / 0xFF ERROR
 *
 * 纯 JVM 实现, 可单测。
 */
object IcocaFrame {

    /** 起始符 */
    const val SOP = 0xAA
    /** 结束符 */
    const val EOP = 0x55

    /** 帧头长度: SOP + CMD + SEQ(2) + LEN(2) */
    const val HEADER_SIZE = 6
    /** 帧尾长度: CHECKSUM + EOP */
    const val TRAILER_SIZE = 2

    // DK 3.0 命令 ID
    const val CMD_BIND_REQUEST = 0x01
    const val CMD_BIND_RESPONSE = 0x02
    const val CMD_UNBIND_REQUEST = 0x03
    const val CMD_UNBIND_RESPONSE = 0x04
    const val CMD_AUTH_REQUEST = 0x10
    const val CMD_AUTH_RESPONSE = 0x11
    const val CMD_CTRL_REQUEST = 0x20
    const val CMD_CTRL_RESPONSE = 0x21
    const val CMD_STATUS_NOTIFY = 0x30
    const val CMD_KEY_SHARE = 0x40
    const val CMD_KEY_SHARE_ACK = 0x41
    const val CMD_ERROR = 0xFF

    /** 解析后的帧 */
    data class Frame(
        val cmdId: Int,
        val seqNum: Int,
        val payload: ByteArray,
        val raw: ByteArray
    ) {
        override fun equals(other: Any?): Boolean =
            other is Frame && cmdId == other.cmdId && seqNum == other.seqNum && payload.contentEquals(other.payload)

        override fun hashCode(): Int = 31 * (31 * cmdId + seqNum) + payload.contentHashCode()
    }

    /**
     * 构造帧: SOP + CMD + SEQ + LEN + PAYLOAD + XOR 校验 + EOP
     * @param cmdId  命令 ID (0..255)
     * @param seqNum 序列号 (0..65535)
     */
    fun build(cmdId: Int, seqNum: Int, payload: ByteArray): ByteArray {
        require(cmdId in 0..0xFF) { "cmdId out of 8-bit range: $cmdId" }
        require(seqNum in 0..0xFFFF) { "seqNum out of 16-bit range: $seqNum" }
        require(payload.size <= 0xFFFF) { "payload too large: ${payload.size}" }

        val total = HEADER_SIZE + payload.size + TRAILER_SIZE
        val buf = ByteArray(total)
        buf[0] = SOP.toByte()
        buf[1] = cmdId.toByte()
        buf[2] = (seqNum ushr 8).toByte()
        buf[3] = seqNum.toByte()
        buf[4] = (payload.size ushr 8).toByte()
        buf[5] = payload.size.toByte()
        payload.copyInto(buf, HEADER_SIZE)
        val checksumIndex = HEADER_SIZE + payload.size
        buf[checksumIndex] = checksum(buf, 0, checksumIndex)
        buf[checksumIndex + 1] = EOP.toByte()
        return buf
    }

    /**
     * XOR 校验和: 对 [from, until) 区间内所有字节异或
     * (帧校验约定: 覆盖 SOP..PAYLOAD, 不含 CHECKSUM 与 EOP)
     */
    fun checksum(bytes: ByteArray, from: Int, until: Int): Byte {
        var acc = 0
        for (i in from until until) acc = acc xor bytes[i].toUInt8()
        return acc.toByte()
    }

    /**
     * 解析帧; 校验失败 / 长度不符 / 标识不符时返回 null。
     * 兼容: 输入可含尾部填充 (解析从首字节起按帧长截取)。
     */
    fun parse(data: ByteArray): Frame? {
        if (data.size < HEADER_SIZE + TRAILER_SIZE) return null
        if (data[0].toUInt8() != SOP || data[data.size - 1].toUInt8() != EOP) return null

        val payloadLen = ((data[4].toUInt8()) shl 8) or data[5].toUInt8()
        if (data.size != HEADER_SIZE + payloadLen + TRAILER_SIZE) return null

        val checksumIndex = HEADER_SIZE + payloadLen
        if (data[checksumIndex] != checksum(data, 0, checksumIndex)) return null

        return Frame(
            cmdId = data[1].toUInt8(),
            seqNum = ((data[2].toUInt8()) shl 8) or data[3].toUInt8(),
            payload = data.copyOfRange(HEADER_SIZE, checksumIndex),
            raw = data
        )
    }
}
