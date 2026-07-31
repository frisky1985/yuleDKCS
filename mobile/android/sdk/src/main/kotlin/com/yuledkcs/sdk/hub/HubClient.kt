package com.yuledkcs.sdk.hub

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import java.util.UUID
import java.util.concurrent.TimeUnit

/**
 * yuleDKCS Hub REST Gateway 客户端
 *
 * 通过 HTTP/JSON 调用 Hub REST Gateway (:8080)。
 * 依赖: OkHttp + Gson。
 *
 * 用法:
 * ```kotlin
 * val client = HubClient.create(SDKConfig(hubEndpoint = "hub.yuletech.com"))
 * client.setToken("session-token-from-oem-server")
 * val key = client.bindKey("LSV...")
 * ```
 */
class HubClient private constructor(
    private val baseURL: String,
    private val client: OkHttpClient,
    private val config: SDKConfig,
    private val gson: Gson = Gson(),
    private val logger: Logger = Logger(config.enableLogging)
) {
    private var token: String? = null

    companion object {
        suspend fun create(
            endpoint: String = "hub.yuletech.com",
            port: Int = 8080,
            config: SDKConfig = SDKConfig()
        ): HubClient = withContext(Dispatchers.IO) {
            val okHttpClient = OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .build()
            HubClient("https://$endpoint:$port/api/v1", okHttpClient, config)
        }

        suspend fun create(config: SDKConfig): HubClient =
            create(config.hubEndpoint, config.hubPort, config)
    }

    /** 设置用户 session token（从车厂 Server 获取后调用） */
    fun setToken(token: String) {
        this.token = token
    }

    /** 清除 token */
    fun clearToken() {
        this.token = null
    }

    /** 关闭客户端（释放连接池） */
    fun shutdown() {
        client.dispatcher.executorService.shutdown()
        client.connectionPool.evictAll()
    }

    // ─── 内部 HTTP 请求封装 ──────────────────────────────────

    /**
     * 发起 HTTP JSON 请求并解码响应
     */
    suspend inline fun <reified T> request(
        method: String,
        path: String,
        body: Any? = null,
        query: Map<String, String>? = null
    ): T = withContext(Dispatchers.IO) {
        val urlBuilder = okHttpUrl("$baseURL$path").newBuilder()
        query?.forEach { (k, v) -> urlBuilder.addQueryParameter(k, v) }
        val url = urlBuilder.build()

        val bodyJson = body?.let { gson.toJson(it) }
        val requestBody = bodyJson?.toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url(url)
            .method(method, requestBody)
            .addHeader("Accept", "application/json")
            .addHeader("X-SDK-Version", Version.CURRENT)
            .addHeader("X-Platform", "android")
            .apply {
                token?.let { addHeader("Authorization", "Bearer $it") }
            }
            .build()

        logger.log("→ $method $path")

        val response = client.newCall(request).execute()

        logger.log("← ${response.code}")

        val responseBody = response.body?.string() ?: ""

        if (!response.isSuccessful) {
            val hubError = try {
                gson.fromJson(responseBody, HubErrorResponse::class.java)
            } catch (_: Exception) { null }

            throw when {
                hubError?.code != null -> YDKError.HubError(hubError.code, hubError.message ?: "")
                hubError?.error != null -> YDKError.HubError(hubError.error, hubError.message ?: "")
                else -> YDKError.HttpError(response.code)
            }
        }

        // 处理空响应 (204 No Content)
        if (responseBody.isEmpty()) {
            if (T::class == Unit::class) {
                @Suppress("UNCHECKED_CAST")
                return@withContext Unit as T
            }
            throw YDKError.DecodingFailed("empty response body")
        }

        try {
            gson.fromJson(responseBody, object : TypeToken<T>() {}.type)
        } catch (e: Exception) {
            throw YDKError.DecodingFailed(e.message ?: "parse error")
        }
    }

    private fun okHttpUrl(url: String) = okhttp3.HttpUrl.parse(url)
        ?: throw YDKError.Internal("invalid URL: $url")
}

/** 日志辅助 */
class Logger(private val enabled: Boolean) {
    fun log(msg: String) {
        if (enabled) println("[HubClient] $msg")
    }
}

/** SDK 版本 */
object Version {
    const val CURRENT = "1.0.0"
}
