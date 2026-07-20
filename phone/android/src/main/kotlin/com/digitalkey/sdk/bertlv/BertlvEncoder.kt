// BertlvEncoder.kt - BER-TLV编码器
package com.digitalkey.sdk.bertlv

import com.digitalkey.sdk.logger.DkLogger
import java.io.ByteArrayOutputStream

// BER-TLV标签
object BertlvTag {
    // 根级别标签 (0x00-0xFF，但遵循ISO 7816-4)
    
    // 认证命令
    const val AUTHENTICATE = 0x10
    const val AUTH_RESPONSE = 0x11
    const val GET_CHALLENGE = 0x12
    const val MUTUAL_AUTH = 0x13
    
    // 密钥管理
    const val KEY_ID = 0x20
    const val KEY_TYPE = 0x21
    const val KEY_USAGE = 0x22
    const val KEY_DATA = 0x23
    const val KEY_HASH = 0x24
    
    // 车辆命令
    const val VEHICLE_CMD = 0x30
    const val VEHICLE_UNLOCK = 0x31
    const val VEHICLE_LOCK = 0x32
    const val VEHICLE_START = 0x33
    const val VEHICLE_STOP = 0x34
    const val VEHICLE_STATUS = 0x35
    
    // 设备信息
    const val DEVICE_ID = 0x40
    const val DEVICE_TYPE = 0x41
    const val DEVICE_NAME = 0x42
    const val DEVICE_Vendor = 0x43
    
    // 时间戳
    const val TIMESTAMP = 0x50
    const val VALID_FROM = 0x51
    const val VALID_UNTIL = 0x52
    
    // 协议头
    const val PROTOCOL_VERSION = 0x60
    const val MESSAGE_TYPE = 0x61
    const val SEQUENCE_NUM = 0x62
    const val TRANSACTION_ID = 0x63
    
    // 状态码
    const val STATUS_CODE = 0x70
    const val ERROR_CODE = 0x71
    const val ERROR_MESSAGE = 0x72
    
    // 证书
    const val CERTIFICATE = 0x80
    const val CERT_CHAIN = 0x81
    const val PUBLIC_KEY = 0x82
    const val SIGNATURE = 0x83
    
    // 分享功能
    const val SHARE_CODE = 0x90
    const val SHARE_PERMISSION = 0x91
    const val SHARE_MAX_USES = 0x92
    const val SHARE_REMAINING = 0x93
    
    // 加密数据
    const val ENCRYPTED_DATA = 0xA0
    const val IV_COUNTER = 0xA1
    const val AUTH_DATA = 0xA2
    
    // UWB数据
    const val UWB_SESSION = 0xB0
    const val UWB_RANGING_DATA = 0xB1
    const val UWB_DISTANCE = 0xB2
    const val UWB_AZIMUTH = 0xB3
    const val UWB_ELEVATION = 0xB4
    
    // NFC数据
    const val NFC_TAG_ID = 0xC0
    const val NFC_ANTENNA_POWER = 0xC1
    
    // BLE数据
    const val BLE_DEVICE_ADDR = 0xC8
    const val BLE_RSSI = 0xC9
    const val BLE_MTU = 0xCA
    
    // 车辆信息
    const val VEHICLE_ID = 0xD0
    const val VEHICLE_VENDOR = 0xD1
    const val VEHICLE_MODEL = 0xD2
    const val VEHICLE_YEAR = 0xD3
    
    // 用户信息
    const val USER_ID = 0xE0
    const val USER_NAME = 0xE1
    const val USER_EMAIL = 0xE2
    
    // 自定义/厂商特定
    const val VENDOR_SPECIFIC = 0xF0
    const val VENDOR_CMD_1 = 0xF1
    const val VENDOR_CMD_2 = 0xF2
    
    // 构造/解析标志
    const val CONSTRUCTED = 0x80.toByte()
    
    // 多字节标签标志
    const val TAG_MASK = 0x1F
    const val MORE_OCTETS = 0x80.toByte()
    const val CLASS_MASK = 0xC0.toByte()
    const val FORM_MASK = 0x20.toByte()
}

// BER-TLV T-L-V结构
data class BertlvNode(
    val tag: Int,
    val value: ByteArray,
    val children: List<BertlvNode> = emptyList()
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is BertlvNode) return false
        return toBytes().contentEquals(other.toBytes())
    }
    
    override fun hashCode(): Int = toBytes().contentHashCode()
    
    fun toBytes(): ByteArray {
        val encoder = BertlvEncoder()
        encoder.encodeNode(this)
        return encoder.toByteArray()
    }
    
    fun toHexString(): String {
        return toBytes().toHexString()
    }
    
    private fun ByteArray.toHexString(): String {
        return joinToString("") { "%02X".format(it) }
    }
}

// BER-TLV编码器
class BertlvEncoder {
    
    private val logger = DkLogger.getLogger("BertlvEncoder")
    private val stream = ByteArrayOutputStream()
    
    fun encodeNode(node: BertlvNode) {
        // Write tag
        writeTag(node.tag)
        
        // Write length
        val valueBytes = if (node.children.isEmpty()) {
            node.value
        } else {
            val childEncoder = BertlvEncoder()
            node.children.forEach { childEncoder.encodeNode(it) }
            childEncoder.toByteArray()
        }
        
        writeLength(valueBytes.size)
        
        // Write value
        stream.writeBytes(valueBytes)
    }
    
    fun encode(tag: Int, value: ByteArray): BertlvEncoder {
        encodeNode(BertlvNode(tag, value))
        return this
    }
    
    fun encodePrimitive(tag: Int, value: ByteArray): BertlvEncoder {
        return encode(tag, value)
    }
    
    fun encodeConstructed(tag: Int, vararg children: BertlvNode): BertlvEncoder {
        val encoder = BertlvEncoder()
        children.forEach { encoder.encodeNode(it) }
        encodeNode(BertlvNode(tag, encoder.toByteArray(), children.toList()))
        return this
    }
    
    fun encodeInt(tag: Int, value: Int): BertlvEncoder {
        return encode(tag, intToBytes(value))
    }
    
    fun encodeLong(tag: Int, value: Long): BertlvEncoder {
        return encode(tag, longToBytes(value))
    }
    
    fun encodeString(tag: Int, value: String): BertlvEncoder {
        return encode(tag, value.toByteArray(Charsets.UTF_8))
    }
    
    fun encodeBoolean(tag: Int, value: Boolean): BertlvEncoder {
        return encode(tag, byteArrayOf(if (value) 0x01 else 0x00))
    }
    
    fun append(other: BertlvEncoder): BertlvEncoder {
        stream.writeBytes(other.toByteArray())
        return this
    }
    
    fun toByteArray(): ByteArray = stream.toByteArray()
    
    fun toHexString(): String = toByteArray().toHexString()
    
    private fun ByteArray.toHexString(): String {
        return joinToString("") { "%02X".format(it) }
    }
    
    private fun writeTag(tag: Int) {
        when {
            tag < 0x80 -> {
                // Single byte tag
                stream.write(tag)
            }
            tag < 0x4000 -> {
                // Two byte tag
                stream.write((0x80 or (tag shr 8)))
                stream.write(tag and 0xFF)
            }
            tag < 0x200000 -> {
                // Three byte tag
                stream.write(0xC0 or (tag shr 16))
                stream.write((tag shr 8) and 0xFF)
                stream.write(tag and 0xFF)
            }
            else -> {
                // Four byte tag
                stream.write(0xC0 or (tag shr 24))
                stream.write((tag shr 16) and 0xFF)
                stream.write((tag shr 8) and 0xFF)
                stream.write(tag and 0xFF)
            }
        }
    }
    
    private fun writeLength(length: Int) {
        when {
            length < 0x80 -> {
                // Single byte length
                stream.write(length)
            }
            length < 0x100 -> {
                // Two byte length
                stream.write(0x81)
                stream.write(length)
            }
            length < 0x10000 -> {
                // Three byte length
                stream.write(0x82)
                stream.write((length shr 8) and 0xFF)
                stream.write(length and 0xFF)
            }
            length < 0x1000000 -> {
                // Four byte length
                stream.write(0x83)
                stream.write((length shr 16) and 0xFF)
                stream.write((length shr 8) and 0xFF)
                stream.write(length and 0xFF)
            }
            else -> {
                // Five byte length
                stream.write(0x84)
                stream.write((length shr 24) and 0xFF)
                stream.write((length shr 16) and 0xFF)
                stream.write((length shr 8) and 0xFF)
                stream.write(length and 0xFF)
            }
        }
    }
    
    private fun intToBytes(value: Int): ByteArray {
        return when {
            value < 0x100 -> byteArrayOf(value.toByte())
            value < 0x10000 -> byteArrayOf(
                (value shr 8).toByte(),
                value.toByte()
            )
            else -> byteArrayOf(
                (value shr 24).toByte(),
                (value shr 16).toByte(),
                (value shr 8).toByte(),
                value.toByte()
            )
        }
    }
    
    private fun longToBytes(value: Long): ByteArray {
        val bytes = ByteArray(8)
        for (i in 0..7) {
            bytes[7 - i] = ((value shr (i * 8)) and 0xFF).toByte()
        }
        return bytes.takeWhile { it == 0x00.toByte() }.drop(1).toByteArray().let {
            if (it.isEmpty()) byteArrayOf(0x00) else it
        }
    }
}

// BERTLV构建辅助
object BertlvBuilder {
    
    fun authenticate(keyId: ByteArray, challenge: ByteArray): ByteArray {
        return BertlvEncoder()
            .encodeConstructed(
                BertlvTag.AUTHENTICATE,
                BertlvNode(BertlvTag.KEY_ID, keyId),
                BertlvNode(BertlvTag.AUTH_DATA, challenge)
            )
            .toByteArray()
    }
    
    fun vehicleCommand(command: Int, keyId: ByteArray, transactionId: Long): ByteArray {
        return BertlvEncoder()
            .encodeConstructed(
                BertlvTag.VEHICLE_CMD,
                BertlvNode(BertlvTag.MESSAGE_TYPE, byteArrayOf(command.toByte())),
                BertlvNode(BertlvTag.KEY_ID, keyId),
                BertlvNode(BertlvTag.TRANSACTION_ID, longToBytes(transactionId)),
                BertlvNode(BertlvTag.TIMESTAMP, longToBytes(System.currentTimeMillis()))
            )
            .toByteArray()
    }
    
    fun vehicleUnlock(keyId: ByteArray, transactionId: Long): ByteArray {
        return vehicleCommand(BertlvTag.VEHICLE_UNLOCK, keyId, transactionId)
    }
    
    fun vehicleLock(keyId: ByteArray, transactionId: Long): ByteArray {
        return vehicleCommand(BertlvTag.VEHICLE_LOCK, keyId, transactionId)
    }
    
    fun vehicleStart(keyId: ByteArray, transactionId: Long): ByteArray {
        return vehicleCommand(BertlvTag.VEHICLE_START, keyId, transactionId)
    }
    
    fun statusResponse(statusCode: Int, transactionId: Long, extraData: Map<Int, ByteArray>? = null): ByteArray {
        val encoder = BertlvEncoder()
            .encodeConstructed(
                BertlvTag.VEHICLE_STATUS,
                BertlvNode(BertlvTag.STATUS_CODE, byteArrayOf(statusCode.toByte())),
                BertlvNode(BertlvTag.TRANSACTION_ID, longToBytes(transactionId))
            )
        
        extraData?.forEach { (tag, data) ->
            encoder.encode(tag, data)
        }
        
        return encoder.toByteArray()
    }
    
    fun errorResponse(errorCode: Int, transactionId: Long, message: String? = null): ByteArray {
        val encoder = BertlvEncoder()
            .encodeConstructed(
                BertlvTag.STATUS_CODE,
                BertlvNode(BertlvTag.ERROR_CODE, byteArrayOf(errorCode.toByte())),
                BertlvNode(BertlvTag.TRANSACTION_ID, longToBytes(transactionId))
            )
        
        if (message != null) {
            encoder.encodeString(BertlvTag.ERROR_MESSAGE, message)
        }
        
        return encoder.toByteArray()
    }
    
    fun rangingData(distance: Float, azimuth: Float?, elevation: Float?): ByteArray {
        val encoder = BertlvEncoder()
            .encode(BertlvTag.UWB_DISTANCE, floatToBytes(distance))
        
        if (azimuth != null) {
            encoder.encode(BertlvTag.UWB_AZIMUTH, floatToBytes(azimuth))
        }
        
        if (elevation != null) {
            encoder.encode(BertlvTag.UWB_ELEVATION, floatToBytes(elevation))
        }
        
        return BertlvEncoder()
            .encodeConstructed(
                BertlvTag.UWB_RANGING_DATA,
                BertlvNode(BertlvTag.TIMESTAMP, longToBytes(System.currentTimeMillis())),
                BertlvNode(0x00, encoder.toByteArray()) // Anonymous constructed
            )
            .toByteArray()
    }
    
    fun shareCodeResponse(code: String, validUntil: Long): ByteArray {
        return BertlvEncoder()
            .encodeConstructed(
                BertlvTag.SHARE_CODE,
                BertlvNode(BertlvTag.SHARE_CODE, code.toByteArray(Charsets.UTF_8)),
                BertlvNode(BertlvTag.VALID_UNTIL, longToBytes(validUntil))
            )
            .toByteArray()
    }
    
    private fun longToBytes(value: Long): ByteArray {
        val bytes = mutableListOf<Byte>()
        var v = value
        while (v > 0) {
            bytes.add(0, (v and 0xFF).toByte())
            v = v shr 8
        }
        return if (bytes.isEmpty()) byteArrayOf(0) else bytes.toByteArray()
    }
    
    private fun floatToBytes(value: Float): ByteArray {
        val bits = java.lang.Float.floatToIntBits(value)
        return byteArrayOf(
            (bits shr 24).toByte(),
            (bits shr 16).toByte(),
            (bits shr 8).toByte(),
            bits.toByte()
        )
    }
}
