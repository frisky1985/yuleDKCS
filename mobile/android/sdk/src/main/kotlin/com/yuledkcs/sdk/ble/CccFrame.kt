package com.yuledkcs.sdk.ble

/**
 * CCC Digital Key v4.0 BLE 指令帧 (CCC-TS-101 Table 19-19)
 *
 * 线格式 (4 字节帧头 + 负载):
 *   [0]     message_header  Bit[5:0]=Message Type, Bit[7:6]=RFU(置0)
 *   [1]     payload_header  Message ID (DK_APDU_RQ=0x0B / DK_APDU_RS=0x0C)
 *   [2-3]   length          负载长度 (大端 u16)
 *   [4..]   payload
 *
 * 参考: docs/certification/ccc-ts101-ble-secure-channel.md §4 (Table 19-19)
 * 规范示例: SELECT APDU → 0x010B0013 00A404000DA000000809434343444B41763100
 *
 * 纯 JVM 实现, 可单测。
 */
object CccFrame {

    /** 帧头长度: Message Header + Payload Header + Length(2) */
    const val HEADER_SIZE = 4

    /** Message Type (Table 19-21) */
    const val MSG_TYPE_FRAMEWORK = 0
    const val MSG_TYPE_SE = 1
    const val MSG_TYPE_UWB_RANGING = 2
    const val MSG_TYPE_DK_EVENT = 3
    const val MSG_TYPE_VEHICLE_OEM_APP = 4
    const val MSG_TYPE_SUPPLEMENTARY = 5
    const val MSG_TYPE_HEAD_UNIT_PAIRING = 6

    /** Message ID (§19.3.2) */
    const val MSG_ID_DK_APDU_RQ = 0x0B
    const val MSG_ID_DK_APDU_RS = 0x0C

    /** 解析后的帧 */
    data class Frame(
        val messageType: Int,
        val messageId: Int,
        val payload: ByteArray,
        val raw: ByteArray
    ) {
        override fun equals(other: Any?): Boolean =
            other is Frame && messageType == other.messageType &&
                messageId == other.messageId && payload.contentEquals(other.payload)

        override fun hashCode(): Int = 31 * (31 * messageType + messageId) + payload.contentHashCode()
    }

    /**
     * 构造帧
     * @param messageType Message Type (Bit[5:0], Bit[7:6] RFU 置 0)
     * @param messageId   Message ID (Payload Header)
     */
    fun build(messageType: Int, messageId: Int, payload: ByteArray): ByteArray {
        require(messageType in 0..0x3F) { "messageType out of 6-bit range: $messageType" }
        require(messageId in 0..0xFF) { "messageId out of 8-bit range: $messageId" }
        require(payload.size <= 0xFFFF) { "payload too large: ${payload.size}" }

        val buf = ByteArray(HEADER_SIZE + payload.size)
        buf[0] = messageType.toByte()
        buf[1] = messageId.toByte()
        buf[2] = (payload.size ushr 8).toByte()
        buf[3] = payload.size.toByte()
        payload.copyInto(buf, HEADER_SIZE)
        return buf
    }

    /**
     * 解析帧; 长度不符 / 数据不足时返回 null (防截断/粘包)。
     */
    fun parse(data: ByteArray): Frame? {
        if (data.size < HEADER_SIZE) return null
        val declared = ((data[2].toUInt8()) shl 8) or data[3].toUInt8()
        if (data.size != HEADER_SIZE + declared) return null
        return Frame(
            messageType = data[0].toUInt8() and 0x3F,
            messageId = data[1].toUInt8(),
            payload = data.copyOfRange(HEADER_SIZE, data.size),
            raw = data
        )
    }
}
