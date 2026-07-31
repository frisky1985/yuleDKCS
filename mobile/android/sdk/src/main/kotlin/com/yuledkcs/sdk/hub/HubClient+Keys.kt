package com.yuledkcs.sdk.hub

import com.google.gson.annotations.SerializedName
import com.yuledkcs.sdk.device.DeviceManager
import java.util.UUID

// ─── 数据模型 ──────────────────────────────────────────────

data class YDKKey(
    val keyId: String,
    val vehicleId: String,
    val deviceId: String? = null,
    val vehicleName: String? = null,
    val keyType: String? = null,
    @SerializedName("protocol") val protocolName: String? = null,
    val status: String? = null,
    val validFrom: Long = 0,
    val validUntil: Long = 0,
    val createdAt: Long = 0
)

data class YDKShare(
    val shareId: String,
    val shareCode: String? = null,
    val sharingUrl: String? = null,
    val keyId: String? = null,
    val fromUserId: String? = null,
    val toUserId: String? = null,
    val validFrom: Long = 0,
    val validUntil: Long = 0,
    val errorCode: String? = null,
    val errorMsg: String? = null
)

data class BindKeyResponse(
    val keyId: String,
    val vehicleId: String,
    val errorCode: String? = null,
    val errorMsg: String? = null
)

data class KeyListResponse(
    val keys: List<YDKKey>? = null
)

data class ControlCommandResponse(
    val cmdId: String? = null,
    val resultCode: Int = 0,
    val errorMsg: String? = null
)

// ─── 钥匙操作 ──────────────────────────────────────────────

suspend fun HubClient.bindKey(
    vehicleId: String,
    deviceId: String? = null,
    devicePubkey: String? = null
): BindKeyResponse {
    val device = DeviceManager
    val pubkey = try { device.readPublicKeyBase64() } catch (_: Exception) { "" }

    val body = mapOf(
        "vehicleId" to vehicleId,
        "deviceId" to (deviceId ?: device.getDeviceId()),
        "devicePubkey" to (devicePubkey ?: pubkey),
        "vendor" to device.detectVendor().protoValue.toString(),
        "protocol" to device.detectProtocol().protoValue.toString(),
        "keyType" to "OWNER",
        "traceId" to UUID.randomUUID().toString()
    )
    return request("POST", "/keys", body)
}

suspend fun HubClient.unbindKey(keyId: String) {
    request<Unit>("DELETE", "/keys/$keyId")
}

suspend fun HubClient.suspendKey(keyId: String, reason: String? = null) {
    request<Unit>("PUT", "/keys/$keyId/suspend", mapOf(
        "reason" to (reason ?: ""),
        "traceId" to UUID.randomUUID().toString()
    ))
}

suspend fun HubClient.resumeKey(keyId: String) {
    request<Unit>("PUT", "/keys/$keyId/resume", mapOf(
        "traceId" to UUID.randomUUID().toString()
    ))
}

suspend fun HubClient.revokeKey(keyId: String, reason: String? = null) {
    request<Unit>("PUT", "/keys/$keyId/revoke", mapOf(
        "reason" to (reason ?: ""),
        "traceId" to UUID.randomUUID().toString()
    ))
}

suspend fun HubClient.renewKey(keyId: String, validUntil: Long) {
    request<Unit>("PUT", "/keys/$keyId/renew", mapOf(
        "validUntil" to validUntil,
        "traceId" to UUID.randomUUID().toString()
    ))
}

suspend fun HubClient.getKey(keyId: String): YDKKey =
    request("GET", "/keys/$keyId")

suspend fun HubClient.listKeys(
    vehicleId: String? = null,
    status: String? = null
): List<YDKKey> {
    val query = mutableMapOf<String, String>()
    vehicleId?.let { query["vehicleId"] = it }
    status?.let { query["status"] = it }
    val resp: KeyListResponse = request("GET", "/keys", query = query.ifEmpty { null })
    return resp.keys ?: emptyList()
}
