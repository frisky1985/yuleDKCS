/**
 * DigitalKey App - 通用 DTO（错误码、通用响应结构）
 */
package com.digitalkey.app.data.remote.dto

import com.google.gson.annotations.SerializedName

/**
 * 通用错误响应
 */
data class ErrorResponse(
    val error: String = "",
    val message: String = "",
    val detail: String? = null
)

/**
 * API 错误码枚举（与 OpenAPI x-error-codes 对应）
 */
enum class ApiError(val code: String, val httpStatus: Int) {
    BAD_REQUEST("BAD_REQUEST", 400),
    AUTH_MISSING_HEADER("AUTH_MISSING_HEADER", 401),
    AUTH_INVALID_FORMAT("AUTH_INVALID_FORMAT", 401),
    AUTH_INVALID_TOKEN("AUTH_INVALID_TOKEN", 401),
    AUTH_INVALID_CREDENTIALS("AUTH_INVALID_CREDENTIALS", 401),
    FORBIDDEN("FORBIDDEN", 403),
    GRPC_NOT_FOUND("GRPC_NOT_FOUND", 404),
    GRPC_PERMISSION_DENIED("GRPC_PERMISSION_DENIED", 403),
    GRPC_INVALID_ARGUMENT("GRPC_INVALID_ARGUMENT", 400),
    GRPC_UNAVAILABLE("GRPC_UNAVAILABLE", 503),
    GRPC_INTERNAL("GRPC_INTERNAL", 500),
    INTERNAL_ERROR("INTERNAL_ERROR", 500),
    ERR_RATE_LIMIT("ERR_RATE_LIMIT", 429);

    companion object {
        fun fromCode(code: String): ApiError? = entries.find { it.code == code }
    }
}
