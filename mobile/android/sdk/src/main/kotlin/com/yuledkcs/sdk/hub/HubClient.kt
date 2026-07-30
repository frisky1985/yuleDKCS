package com.yuledkcs.sdk.hub

import io.grpc.ManagedChannel
import io.grpc.ManagedChannelBuilder
import io.grpc.Metadata
import io.grpc.stub.MetadataUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.UUID
import java.util.concurrent.TimeUnit

/**
 * yuleDKCS Hub gRPC 客户端
 *
 * 用法:
 * ```kotlin
 * val client = HubClient.create("hub.example.com", 9090)
 * client.setToken("session-token-from-oem-server")
 * val key = client.bindKey("LSV...")
 * ```
 */
class HubClient private constructor(
    private val channel: ManagedChannel,
    private val config: SDKConfig
) {
    private var token: String? = null

    private val stubInterceptor = MetadataUtils.newAttachHeadersInterceptor(
        Metadata().apply {
            put(Keys.SDK_VERSION, "1.0.0")
            put(Keys.PLATFORM, "android")
        }
    )

    // gRPC stubs — 由 proto 代码生成 (Phase 2a 编译 proto 后可用)
    // private val keyStub = KeyManagementServiceGrpcKt.KeyManagementServiceCoroutineStub(channel)
    // private val shareStub = KeyShareServiceGrpcKt.KeyShareServiceCoroutineStub(channel)
    // private val vehicleStub = VehicleControlServiceGrpcKt.VehicleControlServiceCoroutineStub(channel)

    companion object {
        suspend fun create(
            endpoint: String,
            port: Int = 9090,
            config: SDKConfig = SDKConfig()
        ): HubClient = withContext(Dispatchers.IO) {
            val channel = ManagedChannelBuilder
                .forAddress(endpoint, port)
                .useTransportSecurity()
                .keepAliveTime(30, TimeUnit.SECONDS)
                .keepAliveTimeout(10, TimeUnit.SECONDS)
                .build()
            HubClient(channel, config)
        }
    }

    /** 设置用户 session token（从车厂 Server 获取后调用） */
    fun setToken(token: String) {
        this.token = token
    }

    /** 清除 token */
    fun clearToken() {
        this.token = null
    }

    /** 关闭连接 */
    suspend fun shutdown() = withContext(Dispatchers.IO) {
        channel.shutdown().awaitTermination(5, TimeUnit.SECONDS)
    }

    private object Keys {
        val AUTHORIZATION = Metadata.Key.of("authorization", Metadata.ASCII_STRING_MARSHALLER)
        val SDK_VERSION = Metadata.Key.of("x-sdk-version", Metadata.ASCII_STRING_MARSHALLER)
        val PLATFORM = Metadata.Key.of("x-platform", Metadata.ASCII_STRING_MARSHALLER)
    }
}
