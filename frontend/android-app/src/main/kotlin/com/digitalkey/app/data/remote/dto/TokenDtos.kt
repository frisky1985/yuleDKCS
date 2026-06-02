/**
 * DigitalKey App - Token 管理 DTO
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

// ── 签发 Token ──

data class IssueTokenRequest(
    @SerializedName("subject_id")
    val subjectId: String,
    @SerializedName("vehicle_id")
    val vehicleId: String,
    val permissions: List<String>? = null,
    val duration: String? = null,
    @SerializedName("max_uses")
    val maxUses: Int? = null
)

data class IssueTokenResponse(
    @SerializedName("token_id")
    val tokenId: String,
    @SerializedName("expires_at")
    val expiresAt: Long,
    val signature: String
)

// ── 验证 Token ──

data class VerifyTokenResponse(
    val valid: Boolean = false,
    @SerializedName("owner_id")
    val ownerId: String? = null,
    @SerializedName("subject_id")
    val subjectId: String? = null,
    @SerializedName("vehicle_id")
    val vehicleId: String? = null,
    val permissions: List<String>? = null,
    @SerializedName("expires_at")
    val expiresAt: Long? = null,
    @SerializedName("use_count")
    val useCount: Int? = null
)

// ── Token 操作响应 ──

data class TokenActionResponse(
    @SerializedName("token_id")
    val tokenId: String? = null,
    val status: String? = null
)

// ── Token 换发 ──

data class ExchangeTokenResponse(
    val exchanged: Boolean = false,
    @SerializedName("token_id")
    val tokenId: String? = null,
    @SerializedName("key_id")
    val keyId: String? = null,
    val subject: String? = null,
    val vehicle: String? = null,
    val note: String? = null
)
