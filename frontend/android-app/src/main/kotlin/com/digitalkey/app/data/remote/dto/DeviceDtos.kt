/**
 * DigitalKey App - 设备管理 DTO
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

// ── 注册设备 ──

data class RegisterDeviceRequest(
    val platform: String,
    val model: String,
    @SerializedName("os_version")
    val osVersion: String? = null,
    @SerializedName("app_version")
    val appVersion: String? = null,
    val ble: Boolean? = null,
    val uwb: Boolean? = null,
    val nfc: Boolean? = null,
    @SerializedName("secure_element")
    val secureElement: Boolean? = null
)

data class RegisterDeviceResponse(
    @SerializedName("device_id")
    val deviceId: String,
    val platform: String? = null,
    val model: String? = null,
    val ble: Boolean? = null,
    val uwb: Boolean? = null,
    val nfc: Boolean? = null,
    @SerializedName("max_devices")
    val maxDevices: Int? = null
)

// ── 设备详情 ──

data class DeviceInfo(
    @SerializedName("device_id")
    val deviceId: String,
    val platform: String? = null,
    val model: String? = null,
    @SerializedName("os_version")
    val osVersion: String? = null,
    @SerializedName("app_version")
    val appVersion: String? = null,
    val ble: Boolean? = null,
    val uwb: Boolean? = null,
    val nfc: Boolean? = null,
    @SerializedName("secure_element")
    val secureElement: Boolean? = null,
    @SerializedName("last_seen")
    val lastSeen: Long? = null,
    @SerializedName("registered_at")
    val registeredAt: Long? = null
)

data class ListDevicesResponse(
    val devices: List<DeviceInfo>
)

data class DeviceActionResponse(
    @SerializedName("device_id")
    val deviceId: String? = null,
    val status: String? = null,
    @SerializedName("keys_revoked")
    val keysRevoked: Int? = null
)

// ── 配钥 ──

data class ProvisionDeviceRequest(
    @SerializedName("vehicle_id")
    val vehicleId: String
)

data class ProvisionDeviceResponse(
    @SerializedName("key_id")
    val keyId: String? = null,
    @SerializedName("device_id")
    val deviceId: String? = null,
    @SerializedName("vehicle_id")
    val vehicleId: String? = null,
    val status: String? = null,
    @SerializedName("bound_at")
    val boundAt: Long? = null,
    val note: String? = null
)
