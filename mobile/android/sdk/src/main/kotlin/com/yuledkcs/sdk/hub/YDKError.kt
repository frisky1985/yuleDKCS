package com.yuledkcs.sdk.hub

/**
 * yuleDKCS SDK 错误类型
 */
sealed class YDKError : Exception() {
    object NotInitialized : YDKError()
    object NotAuthenticated : YDKError()
    data class HubError(val code: String, val msg: String) : YDKError()
    data class HttpError(val statusCode: Int) : YDKError()
    data class NetworkError(val cause: Throwable) : YDKError()
    object Timeout : YDKError()
    data class DecodingFailed(val detail: String) : YDKError()
    data class Internal(val msg: String) : YDKError()

    override val message: String get() = when (this) {
        NotInitialized -> "SDK 未初始化"
        NotAuthenticated -> "未登录，请先调用 setToken()"
        is HubError -> "[$code] $msg"
        is HttpError -> "HTTP 错误: $statusCode"
        is NetworkError -> "网络错误: ${cause.message}"
        Timeout -> "请求超时"
        is DecodingFailed -> "JSON 解析失败: $detail"
        is Internal -> "SDK 内部错误: $msg"
    }
}

/** Hub REST Gateway 错误响应格式 */
data class HubErrorResponse(
    val error: String? = null,
    val message: String? = null,
    val code: String? = null
)
