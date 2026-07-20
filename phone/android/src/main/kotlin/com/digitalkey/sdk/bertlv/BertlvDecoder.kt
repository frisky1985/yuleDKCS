// BertlvDecoder.kt - BER-TLV解码器
package com.digitalkey.sdk.bertlv

import com.digitalkey.sdk.error.DkError
import com.digitalkey.sdk.error.DkErrorCode
import com.digitalkey.sdk.logger.DkLogger

// BER-TLV解码异常
class BertlvDecodeException(
    val offset: Int,
    message: String
) : Exception("Decode error at offset $offset: $message")

// BER-TLV解码器
class BertlvDecoder(private val data: ByteArray) {
    
    private val logger = DkLogger.getLogger("BertlvDecoder")
    private var offset = 0
    
    // 解码所有节点
    fun decodeAll(): List<BertlvNode> {
        val nodes = mutableListOf<BertlvNode>()
        while (offset < data.size) {
            try {
                val node = decodeNode()
                nodes.add(node)
            } catch (e: BertlvDecodeException) {
                logger.error("Failed to decode at offset ${e.offset}", e)
                break
            }
        }
        return nodes
    }
    
    // 解码单个节点
    fun decodeNode(): BertlvNode {
        if (offset >= data.size) {
            throw BertlvDecodeException(offset, "No more data to decode")
        }
        
        val startOffset = offset
        
        // 解码标签
        val tag = readTag()
        
        // 解码长度
        val length = readLength()
        
        // 读取值
        if (offset + length > data.size) {
            throw BertlvDecodeException(startOffset, "Insufficient data for value: need $length, have ${data.size - offset}")
        }
        
        val value = data.copyOfRange(offset, offset + length)
        offset += length
        
        // 检查是否为构造标签
        val isConstructed = (tag and 0x20) != 0
        
        // 如果是构造标签，递归解码子节点
        val children = if (isConstructed && length > 0) {
            BertlvDecoder(value).decodeAll()
        } else {
            emptyList()
        }
        
        return BertlvNode(tag, value, children)
    }
    
    // 获取剩余数据
    fun remaining(): ByteArray {
        return if (offset < data.size) {
            data.copyOfRange(offset, data.size)
        } else {
            byteArrayOf()
        }
    }
    
    // 解码标签
    private fun readTag(): Int {
        if (offset >= data.size) {
            throw BertlvDecodeException(offset, "Unexpected end of data while reading tag")
        }
        
        val firstByte = data[offset].toInt() and 0xFF
        offset++
        
        // 检查标签类别和构造位
        val tagClass = firstByte and 0xC0
        val isConstructed = (firstByte and 0x20) != 0
        
        // 处理多字节标签
        return when {
            firstByte and 0x1F != 0x1F -> {
                // 单字节标签
                firstByte
            }
            offset >= data.size -> {
                throw BertlvDecodeException(offset - 1, "Unexpected end of data in multi-byte tag")
            }
            else -> {
                var tag = 0
                var b: Int
                do {
                    b = data[offset].toInt() and 0xFF
                    offset++
                    tag = (tag shl 7) or (b and 0x7F)
                } while (b and 0x80 != 0 && offset < data.size)
                
                if (b and 0x80 != 0) {
                    throw BertlvDecodeException(offset - 1, "Invalid multi-byte tag: high bit not cleared")
                }
                
                tagClass or (if (isConstructed) 0x20 else 0x00) or tag
            }
        }
    }
    
    // 解码长度
    private fun readLength(): Int {
        if (offset >= data.size) {
            throw BertlvDecodeException(offset, "Unexpected end of data while reading length")
        }
        
        val firstByte = data[offset].toInt() and 0xFF
        offset++
        
        return when {
            firstByte < 0x80 -> {
                // 短格式
                firstByte
            }
            firstByte == 0x81 -> {
                // 长格式：1字节长度
                if (offset >= data.size) {
                    throw BertlvDecodeException(offset - 1, "Unexpected end of data for long length")
                }
                data[offset++].toInt() and 0xFF
            }
            firstByte == 0x82 -> {
                // 长格式：2字节长度
                if (offset + 1 >= data.size) {
                    throw BertlvDecodeException(offset - 1, "Unexpected end of data for long length")
                }
                val len = (data[offset].toInt() and 0xFF) shl 8 or (data[offset + 1].toInt() and 0xFF)
                offset += 2
                len
            }
            firstByte == 0x83 -> {
                // 长格式：3字节长度
                if (offset + 2 >= data.size) {
                    throw BertlvDecodeException(offset - 1, "Unexpected end of data for long length")
                }
                val len = (data[offset].toInt() and 0xFF) shl 16 or
                        (data[offset + 1].toInt() and 0xFF) shl 8 or
                        (data[offset + 2].toInt() and 0xFF)
                offset += 3
                len
            }
            firstByte == 0x84 -> {
                // 长格式：4字节长度
                if (offset + 3 >= data.size) {
                    throw BertlvDecodeException(offset - 1, "Unexpected end of data for long length")
                }
                val len = (data[offset].toInt() and 0xFF) shl 24 or
                        (data[offset + 1].toInt() and 0xFF) shl 16 or
                        (data[offset + 2].toInt() and 0xFF) shl 8 or
                        (data[offset + 3].toInt() and 0xFF)
                offset += 4
                len
            }
            else -> {
                throw BertlvDecodeException(offset - 1, "Invalid length encoding: 0x${firstByte.toString(16)}")
            }
        }
    }
}

// BERTLV解析辅助
object BertlvParser {
    
    private val logger = DkLogger.getLogger("BertlvParser")
    
    /**
     * 解析认证响应
     */
    fun parseAuthResponse(data: ByteArray): AuthResponse? {
        return try {
            val decoder = BertlvDecoder(data)
            val nodes = decoder.decodeAll()
            
            val root = nodes.find { it.tag == BertlvTag.AUTH_RESPONSE }
                ?: nodes.firstOrNull()
            
            root?.let { parseAuthResponseNode(it) }
        } catch (e: Exception) {
            logger.error("Failed to parse auth response", e)
            null
        }
    }
    
    private fun parseAuthResponseNode(node: BertlvNode): AuthResponse {
        return AuthResponse(
            statusCode = node.getChildValue(BertlvTag.STATUS_CODE)?.firstOrNull()?.toInt() ?: 0,
            transactionId = node.getChildValue(BertlvTag.TRANSACTION_ID)?.let { bytesToLong(it) } ?: 0,
            keyId = node.getChildValue(BertlvTag.KEY_ID),
            signature = node.getChildValue(BertlvTag.SIGNATURE),
            certificate = node.getChildValue(BertlvTag.CERTIFICATE)
        )
    }
    
    /**
     * 解析车辆命令
     */
    fun parseVehicleCommand(data: ByteArray): VehicleCommand? {
        return try {
            val decoder = BertlvDecoder(data)
            val nodes = decoder.decodeAll()
            
            val root = nodes.find { it.tag == BertlvTag.VEHICLE_CMD }
                ?: nodes.firstOrNull()
            
            root?.let { parseVehicleCommandNode(it) }
        } catch (e: Exception) {
            logger.error("Failed to parse vehicle command", e)
            null
        }
    }
    
    private fun parseVehicleCommandNode(node: BertlvNode): VehicleCommand {
        return VehicleCommand(
            commandType = node.getChildValue(BertlvTag.MESSAGE_TYPE)?.firstOrNull()?.toInt() ?: 0,
            transactionId = node.getChildValue(BertlvTag.TRANSACTION_ID)?.let { bytesToLong(it) } ?: 0,
            keyId = node.getChildValue(BertlvTag.KEY_ID) ?: byteArrayOf(),
            timestamp = node.getChildValue(BertlvTag.TIMESTAMP)?.let { bytesToLong(it) }
        )
    }
    
    /**
     * 解析状态响应
     */
    fun parseStatusResponse(data: ByteArray): StatusResponse? {
        return try {
            val decoder = BertlvDecoder(data)
            val nodes = decoder.decodeAll()
            
            val root = nodes.find { it.tag == BertlvTag.VEHICLE_STATUS }
                ?: nodes.firstOrNull()
            
            root?.let {
                StatusResponse(
                    statusCode = it.getChildValue(BertlvTag.STATUS_CODE)?.firstOrNull()?.toInt() ?: 0,
                    transactionId = it.getChildValue(BertlvTag.TRANSACTION_ID)?.let { v -> bytesToLong(v) } ?: 0,
                    extraData = parseExtraData(it)
                )
            }
        } catch (e: Exception) {
            logger.error("Failed to parse status response", e)
            null
        }
    }
    
    /**
     * 解析错误响应
     */
    fun parseErrorResponse(data: ByteArray): ErrorResponse? {
        return try {
            val decoder = BertlvDecoder(data)
            val nodes = decoder.decodeAll()
            
            val root = nodes.find { it.tag == BertlvTag.ERROR_CODE || it.tag == BertlvTag.STATUS_CODE }
                ?: nodes.firstOrNull()
            
            root?.let {
                ErrorResponse(
                    errorCode = it.getChildValue(BertlvTag.ERROR_CODE)?.firstOrNull()?.toInt() ?: 0,
                    transactionId = it.getChildValue(BertlvTag.TRANSACTION_ID)?.let { v -> bytesToLong(v) } ?: 0,
                    message = it.getChildValue(BertlvTag.ERROR_MESSAGE)?.toString(Charsets.UTF_8)
                )
            }
        } catch (e: Exception) {
            logger.error("Failed to parse error response", e)
            null
        }
    }
    
    /**
     * 解析UWB测距数据
     */
    fun parseUwbRangingData(data: ByteArray): UwbRangingData? {
        return try {
            val decoder = BertlvDecoder(data)
            val nodes = decoder.decodeAll()
            
            val root = nodes.find { it.tag == BertlvTag.UWB_RANGING_DATA }
            
            root?.let {
                UwbRangingData(
                    timestamp = it.getChildValue(BertlvTag.TIMESTAMP)?.let { v -> bytesToLong(v) } ?: 0,
                    distance = it.getChildValue(BertlvTag.UWB_DISTANCE)?.let { bytesToFloat(it) },
                    azimuth = it.getChildValue(BertlvTag.UWB_AZIMUTH)?.let { bytesToFloat(it) },
                    elevation = it.getChildValue(BertlvTag.UWB_ELEVATION)?.let { bytesToFloat(it) }
                )
            }
        } catch (e: Exception) {
            logger.error("Failed to parse UWB ranging data", e)
            null
        }
    }
    
    /**
     * 查找指定标签的第一个值
     */
    fun findTag(data: ByteArray, tag: Int): ByteArray? {
        return try {
            val decoder = BertlvDecoder(data)
            decoder.decodeAll().find { it.tag == tag }?.value
        } catch (e: Exception) {
            logger.error("Failed to find tag 0x${tag.toString(16)}", e)
            null
        }
    }
    
    /**
     * 查找所有指定标签的值
     */
    fun findAllTags(data: ByteArray, tag: Int): List<ByteArray> {
        return try {
            val decoder = BertlvDecoder(data)
            decoder.decodeAll().filter { it.tag == tag }.map { it.value }
        } catch (e: Exception) {
            logger.error("Failed to find tags 0x${tag.toString(16)}", e)
            emptyList()
        }
    }
    
    // 辅助方法
    
    private fun BertlvNode.getChildValue(tag: Int): ByteArray? {
        return children.find { it.tag == tag }?.value
    }
    
    private fun parseExtraData(node: BertlvNode): Map<Int, ByteArray> {
        return node.children
            .filter { it.tag != BertlvTag.STATUS_CODE && it.tag != BertlvTag.TRANSACTION_ID }
            .associate { it.tag to it.value }
    }
    
    private fun bytesToLong(bytes: ByteArray): Long {
        var result = 0L
        for (b in bytes) {
            result = (result shl 8) or (b.toLong() and 0xFF)
        }
        return result
    }
    
    private fun bytesToFloat(bytes: ByteArray): Float {
        if (bytes.size != 4) return 0f
        val bits = (bytes[0].toInt() and 0xFF shl 24) or
                (bytes[1].toInt() and 0xFF shl 16) or
                (bytes[2].toInt() and 0xFF shl 8) or
                (bytes[3].toInt() and 0xFF)
        return java.lang.Float.intBitsToFloat(bits)
    }
}

// 数据类

data class AuthResponse(
    val statusCode: Int,
    val transactionId: Long,
    val keyId: ByteArray?,
    val signature: ByteArray?,
    val certificate: ByteArray?
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is AuthResponse) return false
        return transactionId == other.transactionId
    }
    
    override fun hashCode(): Int = transactionId.hashCode()
}

data class VehicleCommand(
    val commandType: Int,
    val transactionId: Long,
    val keyId: ByteArray,
    val timestamp: Long?
)

data class StatusResponse(
    val statusCode: Int,
    val transactionId: Long,
    val extraData: Map<Int, ByteArray>
)

data class ErrorResponse(
    val errorCode: Int,
    val transactionId: Long,
    val message: String?
) {
    fun toDkError(): DkError {
        return DkError(DkErrorCode.protocolError, message ?: "Protocol error")
    }
}

data class UwbRangingData(
    val timestamp: Long,
    val distance: Float?,
    val azimuth: Float?,
    val elevation: Float?
)

// 扩展函数

fun ByteArray.decodeBertlv(): List<BertlvNode> {
    return BertlvDecoder(this).decodeAll()
}

fun ByteArray.decodeBertlvNode(): BertlvNode {
    return BertlvDecoder(this).decodeNode()
}
