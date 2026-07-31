package com.yuledkcs.sdk.ble

import java.nio.ByteBuffer

// ─── 协议适配器接口 ───────────────────────────────────────

/**
 * BLE 协议适配器 — 封装各协议（CCC/ICCOA/ICCE）的指令编解码
 */
interface BleProtocolAdapter {
    val protocolType: BleProtocolType

    /** 解析扫描记录 → 车辆信息（非本协议返回 null） */
    fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise?

    fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray
    fun buildLockCommand(keyId: String, session: SessionContext): ByteArray
    fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray
    fun parseCommandResponse(data: ByteArray): CommandResult
    fun parseVehicleStatus(data: ByteArray): VehicleStatus
}

// ─── 工厂 ─────────────────────────────────────────────────

object BleProtocolAdapterFactory {
    fun makeAdapter(type: BleProtocolType): BleProtocolAdapter = when (type) {
        BleProtocolType.CCC -> CCCBleAdapter()
        BleProtocolType.ICCOA -> ICCOABleAdapter()
        BleProtocolType.ICCE -> ICCEBleAdapter()
    }
}

// ─── CCC 适配器 ───────────────────────────────────────────

/**
 * CCC Digital Key v4.0 BLE 协议适配器
 *
 * 指令帧格式（简化骨架）:
 *   [0]     = command type
 *   [1-2]   = session handle (big endian)
 *   [3-6]   = message counter (big endian)
 *   [7...]  = payload
 */
class CCCBleAdapter : BleProtocolAdapter {

    override val protocolType = BleProtocolType.CCC

    override fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise? {
        if (scanRecord == null || scanRecord.size < 6) return null
        // 简化: 假设广告包含 service UUID 0xFFD1（真实解析需 ScanRecord 完整解析）
        return VehicleAdvertise(
            vehicleId = "ccc-" + scanRecord.take(4).joinToString("") { "%02x".format(it) },
            rssi = rssi,
            protocolType = BleProtocolType.CCC.ordinal + 1,
            supportsUwb = false,
            manufacturerData = scanRecord
        )
    }

    override fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.UNLOCK, session)

    override fun buildLockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.LOCK, session)

    override fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.ENGINE_ON, session)

    private fun buildCommand(type: BleCommandType, session: SessionContext): ByteArray {
        val buf = ByteBuffer.allocate(8)
        buf.put(type.value)
        buf.putShort(session.sessionHandle)
        buf.putInt(session.counter.toInt())
        buf.put(0x00)  // payload 占位
        return buf.array()
    }

    override fun parseCommandResponse(data: ByteArray): CommandResult {
        if (data.isEmpty()) return CommandResult(false, -1, "empty response")
        val status = data[0]
        return if (status == 0x00.toByte()) {
            CommandResult(true)
        } else {
            CommandResult(false, status.toInt() and 0xFF, "CCC command failed: 0x%02X".format(status))
        }
    }

    override fun parseVehicleStatus(data: ByteArray): VehicleStatus {
        if (data.size < 3) throw IllegalArgumentException("invalid status response")
        return VehicleStatus(
            locked = data[0] != 0x00.toByte(),
            engineOn = data[1] != 0x00.toByte(),
            batteryPct = data[2].toInt() and 0xFF
        )
    }
}

// ─── ICCOA 适配器 ─────────────────────────────────────────

class ICCOABleAdapter : BleProtocolAdapter {

    override val protocolType = BleProtocolType.ICCOA

    override fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise? {
        if (scanRecord == null || scanRecord.size < 6) return null
        return VehicleAdvertise(
            vehicleId = "iccoa-" + scanRecord.take(4).joinToString("") { "%02x".format(it) },
            rssi = rssi,
            protocolType = BleProtocolType.ICCOA.ordinal + 1,
            supportsUwb = false,
            manufacturerData = scanRecord
        )
    }

    override fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.UNLOCK, session)

    override fun buildLockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.LOCK, session)

    override fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.ENGINE_ON, session)

    private fun buildCommand(type: BleCommandType, session: SessionContext): ByteArray {
        val buf = ByteBuffer.allocate(8)
        buf.put(type.value)
        buf.putShort(session.sessionHandle)
        buf.putInt(session.counter.toInt())
        buf.put(0x00)
        return buf.array()
    }

    override fun parseCommandResponse(data: ByteArray): CommandResult {
        if (data.isEmpty()) return CommandResult(false, -1, "empty response")
        val status = data[0]
        return if (status == 0x00.toByte()) CommandResult(true)
        else CommandResult(false, status.toInt() and 0xFF, "ICCOA command failed: 0x%02X".format(status))
    }

    override fun parseVehicleStatus(data: ByteArray): VehicleStatus {
        if (data.size < 3) throw IllegalArgumentException("invalid status response")
        return VehicleStatus(
            locked = data[0] != 0x00.toByte(),
            engineOn = data[1] != 0x00.toByte(),
            batteryPct = data[2].toInt() and 0xFF
        )
    }
}

// ─── ICCE 适配器 ─────────────────────────────────────────

class ICCEBleAdapter : BleProtocolAdapter {

    override val protocolType = BleProtocolType.ICCE

    override fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise? {
        if (scanRecord == null || scanRecord.size < 6) return null
        return VehicleAdvertise(
            vehicleId = "icce-" + scanRecord.take(4).joinToString("") { "%02x".format(it) },
            rssi = rssi,
            protocolType = BleProtocolType.ICCE.ordinal + 1,
            supportsUwb = false,
            manufacturerData = scanRecord
        )
    }

    override fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.UNLOCK, session)

    override fun buildLockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.LOCK, session)

    override fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.ENGINE_ON, session)

    private fun buildCommand(type: BleCommandType, session: SessionContext): ByteArray {
        val buf = ByteBuffer.allocate(8)
        buf.put(type.value)
        buf.putShort(session.sessionHandle)
        buf.putInt(session.counter.toInt())
        buf.put(0x00)
        return buf.array()
    }

    override fun parseCommandResponse(data: ByteArray): CommandResult {
        if (data.isEmpty()) return CommandResult(false, -1, "empty response")
        val status = data[0]
        return if (status == 0x00.toByte()) CommandResult(true)
        else CommandResult(false, status.toInt() and 0xFF, "ICCE command failed: 0x%02X".format(status))
    }

    override fun parseVehicleStatus(data: ByteArray): VehicleStatus {
        if (data.size < 3) throw IllegalArgumentException("invalid status response")
        return VehicleStatus(
            locked = data[0] != 0x00.toByte(),
            engineOn = data[1] != 0x00.toByte(),
            batteryPct = data[2].toInt() and 0xFF
        )
    }
}
