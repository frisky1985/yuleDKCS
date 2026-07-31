package com.yuledkcs.sdk.ble

import java.util.UUID

/**
 * BLE 广播数据 (Scan Record) 解析器 — 2b-B
 *
 * 按 Bluetooth Core Spec Vol 3 Part C §11 (AD Structures) 解析原始广播字节:
 *   每个 AD Structure = [Length(1B)] [Type(1B)] [Data...]
 *
 * 支持的 AD Type:
 *   - 0x01 Flags
 *   - 0x02/0x03 不完整/完整 16-bit Service UUID 列表 (LE u16)
 *   - 0x04/0x05 不完整/完整 32-bit Service UUID 列表 (LE u32)
 *   - 0x06/0x07 不完整/完整 128-bit Service UUID 列表
 *   - 0x16 16-bit Service Data
 *   - 0xFF Manufacturer Specific Data (LE u16 company id + data)
 *
 * 解析行为与 Android 系统 ScanRecord.parseFromBytes 一致:
 * 遇到 0 长度结构或截断结构时停止解析, 不抛异常。
 *
 * 纯 JVM 实现, 可用构造的字节数组进行单元测试。
 */
object BleAdvertiseParser {

    const val AD_TYPE_FLAGS = 0x01
    const val AD_TYPE_SERVICE_UUIDS_16_INCOMPLETE = 0x02
    const val AD_TYPE_SERVICE_UUIDS_16_COMPLETE = 0x03
    const val AD_TYPE_SERVICE_UUIDS_32_INCOMPLETE = 0x04
    const val AD_TYPE_SERVICE_UUIDS_32_COMPLETE = 0x05
    const val AD_TYPE_SERVICE_UUIDS_128_INCOMPLETE = 0x06
    const val AD_TYPE_SERVICE_UUIDS_128_COMPLETE = 0x07
    const val AD_TYPE_SERVICE_DATA_16 = 0x16
    const val AD_TYPE_MANUFACTURER_SPECIFIC = 0xFF

    /** Bluetooth Base UUID: 00000000-0000-1000-8000-00805F9B34FB */
    val BLE_BASE_UUID = UUID.fromString("00000000-0000-1000-8000-00805F9B34FB")

    /** 单个 AD Structure */
    data class AdStruct(val type: Int, val data: ByteArray) {
        override fun equals(other: Any?): Boolean =
            other is AdStruct && type == other.type && data.contentEquals(other.data)

        override fun hashCode(): Int = 31 * type + data.contentHashCode()
    }

    /** 厂商特定数据: companyId (LE u16) + data */
    data class ManufacturerData(val companyId: Int, val data: ByteArray) {
        /** 还原为 AD 原始字节: companyId(LE, 2B) + data */
        fun toBytes(): ByteArray {
            val out = ByteArray(2 + data.size)
            out[0] = (companyId and 0xFF).toByte()
            out[1] = ((companyId ushr 8) and 0xFF).toByte()
            data.copyInto(out, 2)
            return out
        }

        override fun equals(other: Any?): Boolean =
            other is ManufacturerData && companyId == other.companyId && data.contentEquals(other.data)

        override fun hashCode(): Int = 31 * companyId + data.contentHashCode()
    }

    /** 解析后的广播记录 */
    data class ParsedScanRecord(
        val adStructures: List<AdStruct>,
        val flags: Int?,
        val serviceUuids16: List<Int>,
        val serviceUuids32: List<Int>,
        val serviceUuids128: List<UUID>,
        val manufacturerData: ManufacturerData?
    ) {
        /** 16-bit Service UUID 是否出现 */
        fun hasServiceUuid16(uuid16: Int): Boolean = serviceUuids16.contains(uuid16 and 0xFFFF)

        /**
         * 指定 Service UUID (128-bit 标准形式) 是否出现。
         * 兼容三种广播形式: 完整 128-bit 列表、16-bit 列表 (取低 16 位)、
         * 以及 128-bit 列表中符合 BLE Base UUID 结构的低 16 位。
         */
        fun hasServiceUuid(uuid: UUID): Boolean {
            if (serviceUuids128.contains(uuid)) return true
            val low16 = (uuid.mostSignificantBits ushr 32).toInt() and 0xFFFF
            if (serviceUuids16.contains(low16)) return true
            return serviceUuids128.any { isBleBaseUuid(it) && ((it.mostSignificantBits ushr 32).toInt() and 0xFFFF) == low16 }
        }
    }

    /** 判断 UUID 是否基于 BLE Base UUID (0000xxxx-0000-1000-8000-00805F9B34FB) */
    private fun isBleBaseUuid(uuid: UUID): Boolean =
        uuid.leastSignificantBits == BLE_BASE_UUID.leastSignificantBits &&
            (uuid.mostSignificantBits and 0xFFFF0000L) == 0L

    /**
     * 解析完整 Scan Record。
     * @return null 当输入为 null / 空
     */
    fun parse(scanRecord: ByteArray?): ParsedScanRecord? {
        if (scanRecord == null || scanRecord.isEmpty()) return null

        val structs = parseAdStructures(scanRecord)

        var flags: Int? = null
        val uuids16 = mutableListOf<Int>()
        val uuids32 = mutableListOf<Int>()
        val uuids128 = mutableListOf<UUID>()
        var mfr: ManufacturerData? = null

        for (s in structs) {
            when (s.type) {
                AD_TYPE_FLAGS -> if (flags == null && s.data.isNotEmpty()) flags = s.data[0].toUInt8()
                AD_TYPE_SERVICE_UUIDS_16_INCOMPLETE,
                AD_TYPE_SERVICE_UUIDS_16_COMPLETE -> parseU16List(s.data, uuids16)
                AD_TYPE_SERVICE_UUIDS_32_INCOMPLETE,
                AD_TYPE_SERVICE_UUIDS_32_COMPLETE -> parseU32List(s.data, uuids32)
                AD_TYPE_SERVICE_UUIDS_128_INCOMPLETE,
                AD_TYPE_SERVICE_UUIDS_128_COMPLETE -> parseUuid128List(s.data, uuids128)
                AD_TYPE_MANUFACTURER_SPECIFIC -> if (s.data.size >= 2) {
                    val companyId = (s.data[0].toUInt8()) or (s.data[1].toUInt8() shl 8)
                    mfr = ManufacturerData(companyId, s.data.copyOfRange(2, s.data.size))
                }
            }
        }

        return ParsedScanRecord(structs, flags, uuids16, uuids32, uuids128, mfr)
    }

    /**
     * 解析 AD Structures 列表。
     * 与 Android ScanRecord 一致: 0 长度或截断的结构导致停止解析。
     */
    fun parseAdStructures(bytes: ByteArray): List<AdStruct> {
        val out = mutableListOf<AdStruct>()
        var offset = 0
        while (offset + 1 < bytes.size) {
            val length = bytes[offset].toUInt8()
            if (length == 0) break
            if (offset + 1 + length > bytes.size) break
            val type = bytes[offset + 1].toUInt8()
            val data = bytes.copyOfRange(offset + 2, offset + 1 + length)
            out.add(AdStruct(type, data))
            offset += 1 + length
        }
        return out
    }

    private fun parseU16List(data: ByteArray, out: MutableList<Int>) {
        var i = 0
        while (i + 1 < data.size) {
            out.add((data[i].toUInt8()) or (data[i + 1].toUInt8() shl 8))
            i += 2
        }
    }

    private fun parseU32List(data: ByteArray, out: MutableList<Int>) {
        var i = 0
        while (i + 3 < data.size) {
            out.add(
                (data[i].toUInt8()) or (data[i + 1].toUInt8() shl 8) or
                    (data[i + 2].toUInt8() shl 16) or (data[i + 3].toUInt8() shl 24)
            )
            i += 4
        }
    }

    private fun parseUuid128List(data: ByteArray, out: MutableList<UUID>) {
        var i = 0
        while (i + 15 < data.size) {
            out.add(parseUuid128(data, i))
            i += 16
        }
    }

    /**
     * 按 Android BluetoothUuid.parseUuidFrom 规则解析 128-bit UUID (小端):
     *   time_low  = LE u32 @ [0..3]
     *   time_mid  = LE u16 @ [4..5]
     *   time_hi   = LE u16 @ [6..7]
     *   clock_seq =      u16 @ [8..9]
     *   node      = BE      @ [10..15]
     */
    fun parseUuid128(bytes: ByteArray, offset: Int = 0): UUID {
        require(offset + 16 <= bytes.size) { "need 16 bytes for a 128-bit UUID" }
        val timeLow =
            (bytes[offset].toUInt8()) or (bytes[offset + 1].toUInt8() shl 8) or
                (bytes[offset + 2].toUInt8() shl 16) or (bytes[offset + 3].toUInt8() shl 24)
        val timeMid = (bytes[offset + 4].toUInt8()) or (bytes[offset + 5].toUInt8() shl 8)
        val timeHi = (bytes[offset + 6].toUInt8()) or (bytes[offset + 7].toUInt8() shl 8)
        val clockSeq = (bytes[offset + 8].toUInt8() shl 8) or (bytes[offset + 9].toUInt8())
        var node = 0L
        for (i in 10 until 16) node = (node shl 8) or bytes[offset + i].toUInt8().toLong()
        val msb = (timeLow.toLong() shl 32) or (timeMid.toLong() shl 16) or timeHi.toLong()
        val lsb = (clockSeq.toLong() shl 48) or node
        return UUID(msb, lsb)
    }
}

/** 无符号字节值 (0..255) */
internal fun Byte.toUInt8(): Int = toInt() and 0xFF

/** 小写十六进制字符串 (每字节 2 位) */
internal fun ByteArray.toHexString(): String = joinToString("") { "%02x".format(it) }

