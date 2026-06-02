/**
 * DigitalKey App - Retrofit API Service 接口定义
 *
 * 对应 OpenAPI 规范中的所有 REST 端点。
 * 基础路径由 ApiClient 配置。
 */
package com.digitalkey.app.data.remote

import com.digitalkey.app.data.remote.dto.*
import retrofit2.Response
import retrofit2.http.*

/**
 * yuleDKCS REST API Retrofit 接口
 *
 * 所有端点对应 OpenAPI 规范中的路径定义。
 * 认证通过 [ApiClient] 中的拦截器自动添加 Bearer Token。
 */
interface ApiService {

    // ════════════════════════════════════════════
    // 认证 (Auth)
    // ════════════════════════════════════════════

    /**
     * 登录获取 JWT Token
     */
    @POST("auth/login")
    suspend fun login(
        @Body request: LoginRequest
    ): Response<LoginResponse>

    // ════════════════════════════════════════════
    // 密钥管理 (Keys)
    // ════════════════════════════════════════════

    /**
     * 绑定密钥（为指定车辆绑定数字钥匙）
     */
    @POST("keys")
    suspend fun bindKey(
        @Body request: BindKeyRequest
    ): Response<BindKeyResponse>

    /**
     * 获取密钥列表（支持分页过滤）
     */
    @GET("keys")
    suspend fun listKeys(
        @Query("user_id") userId: String? = null,
        @Query("vehicle_id") vehicleId: String? = null,
        @Query("limit") limit: Int? = null,
        @Query("offset") offset: String? = null
    ): Response<ListKeysResponse>

    /**
     * 查询单个密钥详情
     */
    @GET("keys/{keyId}")
    suspend fun getKey(
        @Path("keyId") keyId: String
    ): Response<KeyResponse>

    /**
     * 解绑密钥
     */
    @DELETE("keys/{keyId}")
    suspend fun unbindKey(
        @Path("keyId") keyId: String
    ): Response<KeyErrorResponse>

    /**
     * 暂停密钥（临时挂起）
     */
    @PUT("keys/{keyId}/suspend")
    suspend fun suspendKey(
        @Path("keyId") keyId: String,
        @Body request: KeyActionRequest? = null
    ): Response<KeyErrorResponse>

    /**
     * 恢复密钥（取消挂起）
     */
    @PUT("keys/{keyId}/resume")
    suspend fun resumeKey(
        @Path("keyId") keyId: String
    ): Response<KeyErrorResponse>

    /**
     * 吊销密钥（永久）
     */
    @PUT("keys/{keyId}/revoke")
    suspend fun revokeKey(
        @Path("keyId") keyId: String,
        @Body request: KeyActionRequest? = null
    ): Response<KeyErrorResponse>

    /**
     * 续期密钥
     */
    @PUT("keys/{keyId}/renew")
    suspend fun renewKey(
        @Path("keyId") keyId: String,
        @Body request: RenewKeyRequest
    ): Response<RenewKeyResponse>

    // ════════════════════════════════════════════
    // 密钥分享 (Shares)
    // ════════════════════════════════════════════

    /**
     * 创建密钥分享
     */
    @POST("shares")
    suspend fun createShare(
        @Body request: CreateShareRequest
    ): Response<CreateShareResponse>

    /**
     * 接受密钥分享（使用分享码）
     */
    @POST("shares/accept")
    suspend fun acceptShare(
        @Body request: AcceptShareRequest
    ): Response<AcceptShareResponse>

    /**
     * 查询分享信息
     */
    @GET("shares/{shareId}")
    suspend fun getShare(
        @Path("shareId") shareId: String
    ): Response<ShareInfoResponse>

    /**
     * 取消密钥分享
     */
    @DELETE("shares/{shareId}")
    suspend fun cancelShare(
        @Path("shareId") shareId: String
    ): Response<ShareActionResponse>

    // ════════════════════════════════════════════
    // 车辆控制 (Vehicles)
    // ════════════════════════════════════════════

    /**
     * 发送车辆控制指令
     */
    @POST("vehicles/{vehicleId}/command")
    suspend fun sendCommand(
        @Path("vehicleId") vehicleId: String,
        @Body request: SendCommandRequest
    ): Response<SendCommandResponse>

    // ════════════════════════════════════════════
    // Token 管理 (Tokens)
    // ════════════════════════════════════════════

    /**
     * 签发授权 Token
     */
    @POST("tokens")
    suspend fun issueToken(
        @Body request: IssueTokenRequest
    ): Response<IssueTokenResponse>

    /**
     * 验证 Token
     */
    @GET("tokens/{tokenId}")
    suspend fun verifyToken(
        @Path("tokenId") tokenId: String
    ): Response<VerifyTokenResponse>

    /**
     * 吊销 Token
     */
    @DELETE("tokens/{tokenId}")
    suspend fun revokeToken(
        @Path("tokenId") tokenId: String
    ): Response<TokenActionResponse>

    /**
     * Token 换发数字钥匙
     */
    @POST("tokens/{tokenId}/exchange")
    suspend fun exchangeToken(
        @Path("tokenId") tokenId: String
    ): Response<ExchangeTokenResponse>

    /**
     * 挂起 Token
     */
    @PUT("tokens/{tokenId}/suspend")
    suspend fun suspendToken(
        @Path("tokenId") tokenId: String
    ): Response<TokenActionResponse>

    /**
     * 恢复 Token
     */
    @PUT("tokens/{tokenId}/resume")
    suspend fun resumeToken(
        @Path("tokenId") tokenId: String
    ): Response<TokenActionResponse>

    // ════════════════════════════════════════════
    // 设备管理 (Devices)
    // ════════════════════════════════════════════

    /**
     * 注册设备
     */
    @POST("devices")
    suspend fun registerDevice(
        @Body request: RegisterDeviceRequest
    ): Response<RegisterDeviceResponse>

    /**
     * 列出用户设备
     */
    @GET("devices")
    suspend fun listDevices(): Response<ListDevicesResponse>

    /**
     * 查看设备详情
     */
    @GET("devices/{deviceId}")
    suspend fun getDevice(
        @Path("deviceId") deviceId: String
    ): Response<DeviceInfo>

    /**
     * 删除设备
     */
    @DELETE("devices/{deviceId}")
    suspend fun deleteDevice(
        @Path("deviceId") deviceId: String
    ): Response<DeviceActionResponse>

    /**
     * 配钥到设备
     */
    @POST("devices/{deviceId}/provision")
    suspend fun provisionDevice(
        @Path("deviceId") deviceId: String,
        @Body request: ProvisionDeviceRequest
    ): Response<ProvisionDeviceResponse>

    /**
     * 吊销设备所有钥匙
     */
    @POST("devices/{deviceId}/revoke")
    suspend fun revokeDevice(
        @Path("deviceId") deviceId: String
    ): Response<DeviceActionResponse>

    // ════════════════════════════════════════════
    // HUB 管理 (Hub)
    // ════════════════════════════════════════════

    /**
     * 列出所有适配器状态
     */
    @GET("hub/adapters")
    suspend fun listAdapters(): Response<HubHealthResponse>

    /**
     * HUB 健康检查
     */
    @GET("hub/health")
    suspend fun hubHealthCheck(): Response<HubHealthResponse>

    // ════════════════════════════════════════════
    // 系统 (System)
    // ════════════════════════════════════════════

    /**
     * 公开健康检查（无需认证）
     */
    @GET("health")
    suspend fun healthCheck(): Response<HealthCheckResponse>
}
