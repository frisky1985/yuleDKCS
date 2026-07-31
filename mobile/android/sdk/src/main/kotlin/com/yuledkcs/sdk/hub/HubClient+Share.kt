package com.yuledkcs.sdk.hub

import com.yuledkcs.sdk.device.DeviceManager
import java.util.UUID

suspend fun HubClient.createShare(
    keyId: String,
    toVendor: String,
    toUserId: String? = null,
    validFrom: Long = 0,
    validUntil: Long = 0,
    maxUses: Int = 0
): YDKShare {
    val body = mapOf(
        "keyId" to keyId,
        "toVendor" to toVendor,
        "toUserId" to (toUserId ?: ""),
        "validFrom" to validFrom,
        "validUntil" to validUntil,
        "maxUses" to maxUses,
        "traceId" to UUID.randomUUID().toString()
    )
    return request("POST", "/shares", body)
}

suspend fun HubClient.acceptShare(shareCode: String): YDKKey {
    val device = DeviceManager
    val pubkey = try { device.readPublicKeyBase64() } catch (_: Exception) { "" }

    val body = mapOf(
        "shareCode" to shareCode,
        "deviceId" to device.getDeviceId(),
        "devicePubkey" to pubkey,
        "vendor" to device.detectVendor().protoName,
        "traceId" to UUID.randomUUID().toString()
    )
    return request("POST", "/shares/accept", body)
}

suspend fun HubClient.cancelShare(shareId: String) {
    request<Unit>("DELETE", "/shares/$shareId")
}

suspend fun HubClient.getShare(shareId: String): YDKShare =
    request("GET", "/shares/$shareId")
