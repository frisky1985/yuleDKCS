/**
 * DigitalKey App - 密钥分享 DTO
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

// ── 创建分享 ──

data class CreateShareRequest(
    @SerializedName("key_id")
    val keyId: String,
    @SerializedName("from_user_id")
    val fromUserId: String? = null,
    @SerializedName("to_vendor")
    val toVendor: PhoneVendor? = null,
    @SerializedName("to_user_id")
    val toUserId: String? = null,
    @SerializedName("access_level")
    val accessLevel: AccessLevel? = null,
    @SerializedName("valid_from")
    val validFrom: Long? = null,
    @SerializedName("valid_until")
    val validUntil: Long? = null,
    @SerializedName("max_uses")
    val maxUses: Int? = null,
    @SerializedName("trace_id")
    val traceId: String? = null
)

data class CreateShareResponse(
    @SerializedName("share_id")
    val shareId: String,
    @SerializedName("share_code")
    val shareCode: String? = null,
    @SerializedName("error_code")
    val errorCode: String = ""
)

// ── 接受分享 ──

data class AcceptShareRequest(
    @SerializedName("share_code")
    val shareCode: String,
    @SerializedName("device_id")
    val deviceId: String? = null,
    @SerializedName("user_id")
    val userId: String? = null,
    val vendor: PhoneVendor? = null,
    @SerializedName("device_pubkey")
    val devicePubkey: String? = null,
    @SerializedName("trace_id")
    val traceId: String? = null
)

data class AcceptShareResponse(
    val key: DigitalKeyDto? = null,
    @SerializedName("shared_secret")
    val sharedSecret: String? = null,
    @SerializedName("error_code")
    val errorCode: String = ""
)

// ── 查询分享 ──

data class ShareInfoResponse(
    @SerializedName("share_id")
    val shareId: String? = null,
    @SerializedName("key_id")
    val keyId: String? = null,
    @SerializedName("from_user_id")
    val fromUserId: String? = null,
    @SerializedName("access_level")
    val accessLevel: AccessLevel? = null,
    @SerializedName("valid_from")
    val validFrom: Long? = null,
    @SerializedName("valid_until")
    val validUntil: Long? = null,
    @SerializedName("error_code")
    val errorCode: String = ""
)

// ── 取消分享通用响应 ──

data class ShareActionResponse(
    @SerializedName("error_code")
    val errorCode: String = ""
)
