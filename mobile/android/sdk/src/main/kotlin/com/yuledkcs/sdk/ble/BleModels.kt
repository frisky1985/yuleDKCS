package com.yuledkcs.sdk.ble

import java.util.UUID

// ─── BLE 协议类型 ─────────────────────────────────────────

enum class BleProtocolType(val serviceUuid: UUID) {
    CCC(UUID.fromString("0000FFD1-0000-1000-8000-00805F9B34FB")),
    ICCOA(UUID.fromString("0000FEF5-0000-1000-8000-00805F9B34FB")),
    ICCE(UUID.fromString("0000FEFA-0000-1000-8000-00805F9B34FB"))
}

// ─── BLE 状态 ─────────────────────────────────────────────

enum class BleState { UNKNOWN, RESETTING, UNSUPPORTED, UNAUTHORIZED, POWERED_OFF, POWERED_ON }

enum class ConnectionState { DISCONNECTED, SCANNING, CONNECTING, CONNECTED, DISCOVERING, DISCONNECTING }

// ─── 车辆广播信息 ─────────────────────────────────────────

data class VehicleAdvertise(
    val vehicleId: String,
    val rssi: Int,
    val protocolType: Int,
    val supportsUwb: Boolean,
    val manufacturerData: ByteArray? = null
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is VehicleAdvertise) return false
        return vehicleId == other.vehicleId
    }

    override fun hashCode(): Int = vehicleId.hashCode()
}

// ─── 连接结果 ─────────────────────────────────────────────

data class ConnectResponse(val success: Boolean, val error: String? = null)
data class LocalControlResponse(val success: Boolean, val error: String? = null)

// ─── 车辆状态 ─────────────────────────────────────────────

data class VehicleStatus(
    val locked: Boolean,
    val engineOn: Boolean,
    val batteryPct: Int,
    val error: String? = null
)

// ─── 命令结果 ─────────────────────────────────────────────

data class CommandResult(
    val success: Boolean,
    val errorCode: Int = 0,
    val errorMessage: String? = null
)

// ─── 会话上下文 ───────────────────────────────────────────

data class SessionContext(
    val keyId: String,
    val vehicleId: String,
    var sessionHandle: Short = 0,
    var counter: Long = 0,
    /** 用户 ID (ICCE control_command_t.user_id, 大端 u32) */
    var userId: Int = 0,
    /** SM4 会话密钥 (16 字节) — 由绑定/认证流程的密钥协商产生; null 表示未协商(仅调试/预绑定明文帧) */
    var sessionKey: ByteArray? = null,
    /** SM4-CBC 初始向量 (16 字节); null 时使用全零 IV */
    var sessionIv: ByteArray? = null
)

// ─── 指令类型 ─────────────────────────────────────────────

enum class BleCommandType(val value: Byte) {
    UNLOCK(0x01),
    LOCK(0x02),
    ENGINE_ON(0x03),
    ENGINE_OFF(0x04),
    STATUS(0x05)
}
