package com.yuledkcs.sdk.hub

/**
 * yuleDKCS SDK 错误类型
 */
sealed class YDKError : Exception() {
    object NotInitialized : YDKError()
    object NotAuthenticated : YDKError()
    data class HubError(val code: String, val msg: String) : YDKError()
    data class NetworkError(val cause: Throwable) : YDKError()
    object Timeout : YDKError()
    data class Internal(val msg: String) : YDKError()

    override val message: String get() = when (this) {
        NotInitialized -> "SDK 未初始化"
        NotAuthenticated -> "未登录，请先调用 setToken()"
        is HubError -> "[$code] $msg"
        is NetworkError -> "网络错误: ${cause.message}"
        Timeout -> "请求超时"
        is Internal -> "SDK 内部错误: $msg"
    }
}
