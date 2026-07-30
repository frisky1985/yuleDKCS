package com.yuledkcs.sdk.hub

/**
 * yuleDKCS SDK 配置
 */
data class SDKConfig(
    val enableLogging: Boolean = false,
    val platform: Platform = Platform.ANDROID
)

enum class Platform {
    UNSPECIFIED, IOS, ANDROID, HUAWEI
}
