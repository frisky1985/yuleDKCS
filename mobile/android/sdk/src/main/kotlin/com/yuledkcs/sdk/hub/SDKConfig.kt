package com.yuledkcs.sdk.hub

/**
 * yuleDKCS SDK 配置
 */
data class SDKConfig(
    /// Hub REST Gateway 主机地址（如 "hub.yuletech.com"）
    val hubEndpoint: String = "hub.yuletech.com",
    /// Hub REST Gateway 端口（默认 8080）
    val hubPort: Int = 8080,
    val enableLogging: Boolean = false,
    val platform: Platform = Platform.ANDROID
)

enum class Platform {
    UNSPECIFIED, IOS, ANDROID, HARMONY
}
