/**
 * DigitalKey App - 认证相关 DTO
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

/**
 * 登录请求
 */
data class LoginRequest(
    @SerializedName("user_id")
    val userId: String,
    val password: String
)

/**
 * 登录响应
 */
data class LoginResponse(
    val token: String,
    @SerializedName("token_type")
    val tokenType: String,
    @SerializedName("expires_in")
    val expiresIn: Int
)
