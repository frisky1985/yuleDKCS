package com.yuledkcs.sdk.ble

import java.util.UUID

/**
 * 数字钥匙相关 BLE Service/Characteristic UUID — 多协议支持
 *
 * CCC UUID 按 CCC-TS-101 v4.0.0:
 * - DK Service 0xFFF5 (Table 19-6) — 参考实现 0xFFD1 为 R3.0 遗留
 * - SPSM / DK Version 特征 (Table 19-7..19-12)
 * - CCCServiceDataIntent UUID (Table 19-2 广告 Service Data)
 */
object BleUuids {

    // CCC Digital Key Service (0xFFF5, CCC-TS-101 v4.0.0 Table 19-6)
    val CCC_SERVICE = UUID.fromString("0000FFF5-0000-1000-8000-00805F9B34FB")
    /** UUID_SPSM — 读取 DK Service SPSM (Table 19-7/8) */
    val CCC_CHAR_SPSM = UUID.fromString("D3B5A130-9E23-4B3A-8BE4-6B1EE5F980A3")
    /** UUID_SPSM_DK_VERSION — 车辆 SPSM+版本 (Table 19-9/10) */
    val CCC_CHAR_SPSM_DK_VERSION = UUID.fromString("AE285B91-6D23-23F1-CA12-6B1EE5B780A3")
    /** UUID_DEVICE_DK_VERSION — 设备所选版本 (Table 19-11/12) */
    val CCC_CHAR_DEVICE_DK_VERSION = UUID.fromString("BD4B9502-3F54-11EC-B919-0242AC120005")
    /** CCCServiceDataIntent UUID — 广告 Service Data (Table 19-2) */
    val CCC_SERVICE_DATA_INTENT = UUID.fromString("5810BBC0-B499-11E9-A2A3-2A2AE2DBCCE4")

    // ICCE Digital Key Service (T/CA 110-2020: 0xFEFA)
    val ICCE_SERVICE = UUID.fromString("0000FEFA-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_KEY_STATUS = UUID.fromString("0000FEFB-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_RANGING_DATA = UUID.fromString("0000FEFC-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_AUTH_CHALLENGE = UUID.fromString("0000FEFD-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_CONTROL_CMD = UUID.fromString("0000FEFE-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_SESSION_KEY = UUID.fromString("0000FEFF-0000-1000-8000-00805F9B34FB")

    // ICCOA Digital Key Service (0xFEF5)
    val ICCOA_SERVICE = UUID.fromString("0000FEF5-0000-1000-8000-00805F9B34FB")

    val CLIENT_CHARACTERISTIC_CONFIG = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")

    fun serviceForProtocol(protocol: BleProtocolType): UUID = protocol.serviceUuid
}
