/**
 * DigitalKey App - HUB 管理 DTO
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

// ── 适配器状态 ──

data class AdapterStatusDto(
    val vendor: String? = null,
    val protocol: String? = null,
    val healthy: Boolean = false,
    @SerializedName("last_check_ms")
    val lastCheckMs: Long? = null,
    @SerializedName("error_msg")
    val errorMsg: String? = null
)

data class HubHealthResponse(
    val healthy: Boolean = false,
    val adapters: List<AdapterStatusDto>? = null
)

/**
 * 公开健康检查响应（/health）
 */
data class HealthCheckResponse(
    val status: String
)
