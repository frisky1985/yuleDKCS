/**
 * DigitalKey App - Retrofit 客户端配置
 *
 * 提供 [ApiService] 的单例实例，配置：
 * - 基础 URL（默认生产环境，可覆盖）
 * - Gson 序列化/反序列化
 * - OkHttp 日志拦截器
 * - Bearer Token 认证拦截器
 * - 连接/读/写超时
 */
package com.digitalkey.app.data.remote

import com.google.gson.Gson
import com.google.gson.GsonBuilder
import okhttp3.Interceptor
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.util.concurrent.TimeUnit

/**
 * API 客户端配置
 *
 * 使用示例：
 * ```
 * // 获取默认实例（生产环境）
 * val api = ApiClient.instance.apiService
 *
 * // 自定义 Token
 * ApiClient.instance.setAuthToken("eyJhbGciOiJIUzI1NiIs...")
 * ```
 */
class ApiClient private constructor(
    private val baseUrl: String,
    private val enableLogging: Boolean,
    private val connectTimeoutSeconds: Long,
    private val readTimeoutSeconds: Long,
    private val writeTimeoutSeconds: Long
) {

    // 当前认证 Token
    @Volatile
    private var authToken: String? = null

    // Gson 实例（自定义反序列化配置）
    private val gson: Gson = GsonBuilder()
        .setLenient()
        .create()

    // 认证拦截器：自动添加 Bearer Token
    private val authInterceptor = Interceptor { chain ->
        val original = chain.request()
        val token = authToken
        val request = if (token != null) {
            original.newBuilder()
                .header("Authorization", "Bearer $token")
                .build()
        } else {
            original
        }
        chain.proceed(request)
    }

    // 日志拦截器
    private val loggingInterceptor = HttpLoggingInterceptor().apply {
        level = if (enableLogging) {
            HttpLoggingInterceptor.Level.BODY
        } else {
            HttpLoggingInterceptor.Level.NONE
        }
    }

    // OkHttp 客户端
    private val okHttpClient: OkHttpClient = OkHttpClient.Builder()
        .addInterceptor(authInterceptor)
        .addInterceptor(loggingInterceptor)
        .connectTimeout(connectTimeoutSeconds, TimeUnit.SECONDS)
        .readTimeout(readTimeoutSeconds, TimeUnit.SECONDS)
        .writeTimeout(writeTimeoutSeconds, TimeUnit.SECONDS)
        .build()

    // Retrofit 实例
    private val retrofit: Retrofit = Retrofit.Builder()
        .baseUrl(baseUrl)
        .client(okHttpClient)
        .addConverterFactory(GsonConverterFactory.create(gson))
        .build()

    /** API Service 实例 */
    val apiService: ApiService = retrofit.create(ApiService::class.java)

    /**
     * 设置 Bearer Token（线程安全）
     */
    fun setAuthToken(token: String?) {
        authToken = token
    }

    /**
     * 获取当前 Token
     */
    fun getAuthToken(): String? = authToken

    /**
     * 清除 Token（登出时调用）
     */
    fun clearAuthToken() {
        authToken = null
    }

    companion object {
        // 默认环境
        private const val DEFAULT_BASE_URL = "https://api.yuledkcs.com/v1/"
        private const val STAGING_BASE_URL = "https://staging.api.yuledkcs.com/v1/"
        private const val LOCAL_BASE_URL = "http://localhost:8080/api/v1/"

        private const val DEFAULT_CONNECT_TIMEOUT = 15L
        private const val DEFAULT_READ_TIMEOUT = 30L
        private const val DEFAULT_WRITE_TIMEOUT = 30L

        // 单例锁
        @Volatile
        private var instance: ApiClient? = null

        /**
         * 获取默认 ApiClient 实例（生产环境）
         */
        @JvmStatic
        fun getInstance(): ApiClient {
            return instance ?: synchronized(this) {
                instance ?: createDefault().also { instance = it }
            }
        }

        /**
         * 创建生产环境默认客户端
         */
        @JvmStatic
        fun createDefault(): ApiClient {
            return ApiClient(
                baseUrl = DEFAULT_BASE_URL,
                enableLogging = true,
                connectTimeoutSeconds = DEFAULT_CONNECT_TIMEOUT,
                readTimeoutSeconds = DEFAULT_READ_TIMEOUT,
                writeTimeoutSeconds = DEFAULT_WRITE_TIMEOUT
            )
        }

        /**
         * 创建预发布环境客户端
         */
        @JvmStatic
        fun createStaging(): ApiClient {
            return ApiClient(
                baseUrl = STAGING_BASE_URL,
                enableLogging = true,
                connectTimeoutSeconds = DEFAULT_CONNECT_TIMEOUT,
                readTimeoutSeconds = DEFAULT_READ_TIMEOUT,
                writeTimeoutSeconds = DEFAULT_WRITE_TIMEOUT
            )
        }

        /**
         * 创建本地开发环境客户端
         */
        @JvmStatic
        fun createLocal(): ApiClient {
            return ApiClient(
                baseUrl = LOCAL_BASE_URL,
                enableLogging = true,
                connectTimeoutSeconds = DEFAULT_CONNECT_TIMEOUT,
                readTimeoutSeconds = DEFAULT_READ_TIMEOUT,
                writeTimeoutSeconds = DEFAULT_WRITE_TIMEOUT
            )
        }

        /**
         * 完全自定义的构建方法
         */
        @JvmStatic
        fun build(
            baseUrl: String,
            enableLogging: Boolean = true,
            connectTimeoutSeconds: Long = DEFAULT_CONNECT_TIMEOUT,
            readTimeoutSeconds: Long = DEFAULT_READ_TIMEOUT,
            writeTimeoutSeconds: Long = DEFAULT_WRITE_TIMEOUT
        ): ApiClient {
            return ApiClient(
                baseUrl = baseUrl,
                enableLogging = enableLogging,
                connectTimeoutSeconds = connectTimeoutSeconds,
                readTimeoutSeconds = readTimeoutSeconds,
                writeTimeoutSeconds = writeTimeoutSeconds
            )
        }

        /**
         * 重置单例（测试用）
         */
        @JvmStatic
        fun resetInstance() {
            instance = null
        }
    }
}
