package com.yuledkcs.sdk.ble

/**
 * NFC 车辆信息
 */
data class NfcVehicleInfo(
    val vehicleId: String,
    val tagId: String,
    val protocolType: Int
)

/**
 * NFC 指令类型
 */
enum class NfcCommandType(val value: Byte) {
    UNLOCK(0x01),
    LOCK(0x02),
    START_ENGINE(0x03)
}

/**
 * NFC 管理器接口
 *
 * 实现说明:
 * - Android: HCE (Host Card Emulation) + 标签读取
 * - 本期提供接口 + 说明，真实实现需硬件环境
 */
interface NfcManager {
    /** 读取车辆 NFC 标签 */
    suspend fun readVehicleTag(): NfcVehicleInfo

    /** 通过 NFC 通道发送指令 */
    suspend fun sendCommandViaNfc(command: NfcCommandType)
}
