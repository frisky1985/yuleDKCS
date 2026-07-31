package com.yuledkcs.sdk.hub

import java.util.UUID

suspend fun HubClient.remoteLock(vehicleId: String, keyId: String? = null): ControlCommandResponse =
    sendCommand(vehicleId, "lock", keyId)

suspend fun HubClient.remoteUnlock(vehicleId: String, keyId: String? = null): ControlCommandResponse =
    sendCommand(vehicleId, "unlock", keyId)

suspend fun HubClient.remoteStart(vehicleId: String, keyId: String? = null): ControlCommandResponse =
    sendCommand(vehicleId, "engine_on", keyId)

suspend fun HubClient.remoteStop(vehicleId: String, keyId: String? = null): ControlCommandResponse =
    sendCommand(vehicleId, "engine_off", keyId)

private suspend fun HubClient.sendCommand(
    vehicleId: String,
    action: String,
    keyId: String?
): ControlCommandResponse {
    val body = mapOf(
        "action" to action,
        "keyId" to (keyId ?: ""),
        "traceId" to UUID.randomUUID().toString()
    )
    return request("POST", "/vehicles/$vehicleId/command", body)
}
