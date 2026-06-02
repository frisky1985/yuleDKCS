/**
 * DigitalKey App - 密钥管理 DTO
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

// ── 枚举 ──

enum class Protocol(val value: String) {
    @SerializedName("PROTOCOL_UNSPECIFIED") UNSPECIFIED("PROTOCOL_UNSPECIFIED"),
    @SerializedName("CCC_DK3") CCC_DK3("CCC_DK3"),
    @SerializedName("ICCOA_DK30") ICCOA_DK30("ICCOA_DK30"),
    @SerializedName("ICCOA_DK40") ICCOA_DK40("ICCOA_DK40"),
    @SerializedName("ICCE") ICCE("ICCE")
}

enum class PhoneVendor(val value: String) {
    @SerializedName("VENDOR_UNSPECIFIED") UNSPECIFIED("VENDOR_UNSPECIFIED"),
    @SerializedName("APPLE") APPLE("APPLE"),
    @SerializedName("SAMSUNG") SAMSUNG("SAMSUNG"),
    @SerializedName("XIAOMI") XIAOMI("XIAOMI"),
    @SerializedName("OPPO") OPPO("OPPO"),
    @SerializedName("VIVO") VIVO("VIVO"),
    @SerializedName("HUAWEI") HUAWEI("HUAWEI")
}

enum class KeyType(val value: String) {
    @SerializedName("KEY_TYPE_UNSPECIFIED") UNSPECIFIED("KEY_TYPE_UNSPECIFIED"),
    @SerializedName("OWNER") OWNER("OWNER"),
    @SerializedName("FRIEND") FRIEND("FRIEND"),
    @SerializedName("SERVICE") SERVICE("SERVICE"),
    @SerializedName("TEMPORARY") TEMPORARY("TEMPORARY")
}

enum class KeyStatusDto(val value: String) {
    @SerializedName("KEY_STATUS_UNSPECIFIED") UNSPECIFIED("KEY_STATUS_UNSPECIFIED"),
    @SerializedName("ACTIVE") ACTIVE("ACTIVE"),
    @SerializedName("SUSPENDED") SUSPENDED("SUSPENDED"),
    @SerializedName("REVOKED") REVOKED("REVOKED"),
    @SerializedName("EXPIRED") EXPIRED("EXPIRED")
}

enum class VehicleAction(val value: String) {
    @SerializedName("unlock") UNLOCK("unlock"),
    @SerializedName("lock") LOCK("lock"),
    @SerializedName("engine_on") ENGINE_ON("engine_on"),
    @SerializedName("engine_off") ENGINE_OFF("engine_off"),
    @SerializedName("trunk") TRUNK("trunk"),
    @SerializedName("climate") CLIMATE("climate"),
    @SerializedName("find") FIND("find")
}

enum class CommandSource(val value: Int) {
    @SerializedName("1") NFC(1),
    @SerializedName("2") BLE(2),
    @SerializedName("3") UWB(3),
    @SerializedName("4") REMOTE(4),
    @SerializedName("5") EDGE(5)
}

// ── 权限结构 ──

data class AccessLevel(
    val lock: Boolean = false,
    val unlock: Boolean = false,
    val engine: Boolean = false,
    val trunk: Boolean = false,
    val window: Boolean = false,
    val climate: Boolean = false,
    val find: Boolean = false,
    val seat: Boolean = false
)

data class TimeRestriction(
    val weekdays: List<Int>? = null,
    @SerializedName("start_time")
    val startTime: String? = null,
    @SerializedName("end_time")
    val endTime: String? = null
)

// ── 核心数字钥匙模型 ──

data class DigitalKeyDto(
    @SerializedName("key_id")
    val keyId: String,
    @SerializedName("vehicle_id")
    val vehicleId: String,
    @SerializedName("device_id")
    val deviceId: String? = null,
    @SerializedName("user_id")
    val userId: String? = null,
    @SerializedName("key_type")
    val keyType: KeyType? = null,
    val protocol: Protocol? = null,
    @SerializedName("access_level")
    val accessLevel: AccessLevel? = null,
    @SerializedName("distance_limit")
    val distanceLimit: Int? = null,
    @SerializedName("time_restriction")
    val timeRestriction: TimeRestriction? = null,
    @SerializedName("max_uses")
    val maxUses: Int? = null,
    @SerializedName("used_count")
    val usedCount: Int? = null,
    @SerializedName("key_version")
    val keyVersion: Int? = null,
    val status: KeyStatusDto? = null,
    @SerializedName("valid_from")
    val validFrom: Long? = null,
    @SerializedName("valid_until")
    val validUntil: Long? = null,
    @SerializedName("created_at")
    val createdAt: Long? = null
)

// ── 绑定密钥 ──

data class BindKeyRequest(
    @SerializedName("vehicle_id")
    val vehicleId: String,
    @SerializedName("device_id")
    val deviceId: String? = null,
    @SerializedName("user_id")
    val userId: String? = null,
    val vendor: PhoneVendor? = null,
    val protocol: Protocol? = null,
    @SerializedName("key_type")
    val keyType: KeyType? = null,
    @SerializedName("access_level")
    val accessLevel: AccessLevel? = null,
    @SerializedName("device_pubkey")
    val devicePubkey: String? = null,
    @SerializedName("valid_from")
    val validFrom: Long? = null,
    @SerializedName("valid_until")
    val validUntil: Long? = null,
    @SerializedName("trace_id")
    val traceId: String? = null
)

data class BindKeyResponse(
    val key: DigitalKeyDto,
    @SerializedName("vehicle_pubkey")
    val vehiclePubkey: String? = null,
    @SerializedName("shared_secret")
    val sharedSecret: String? = null,
    @SerializedName("error_code")
    val errorCode: String? = null,
    @SerializedName("error_msg")
    val errorMsg: String? = null
)

// ── 获取密钥列表 ──

data class ListKeysResponse(
    val keys: List<DigitalKeyDto>,
    @SerializedName("next_token")
    val nextToken: String? = null,
    val total: Int? = null
)

// ── 单密钥查询 / 状态变更 ──

data class KeyResponse(
    val key: DigitalKeyDto? = null,
    @SerializedName("error_code")
    val errorCode: String = ""
)

data class KeyActionRequest(
    val reason: String? = null
)

// ── 密钥续期 ──

data class RenewKeyRequest(
    @SerializedName("valid_until")
    val validUntil: Long
)

data class RenewKeyResponse(
    val key: DigitalKeyDto? = null,
    @SerializedName("error_code")
    val errorCode: String = ""
)

// ── 通用错误响应（密钥相关） ──

data class KeyErrorResponse(
    @SerializedName("error_code")
    val errorCode: String = "",
    @SerializedName("error_msg")
    val errorMsg: String? = null
)
