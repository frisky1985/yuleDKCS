package com.yuledkcs.api

import com.yuledkcs.model.DigitalKey
import com.yuledkcs.model.KeyUsageLog
import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.DELETE
import retrofit2.http.GET
import retrofit2.http.Header
import retrofit2.http.POST
import retrofit2.http.Path
import retrofit2.http.Query

/**
 * Retrofit API interface for YuleDKCS backend
 */
interface YuleDKCSApi {
    
    // ========== Auth ==========
    @POST("auth/login")
    suspend fun login(
        @Body request: LoginRequest
    ): Response<LoginResponse>
    
    @POST("auth/register")
    suspend fun register(
        @Body request: RegisterRequest
    ): Response<AuthResponse>
    
    @POST("auth/refresh")
    suspend fun refreshToken(
        @Header("Authorization") token: String
    ): Response<TokenResponse>
    
    @GET("user/profile")
    suspend fun getProfile(
        @Header("Authorization") token: String
    ): Response<UserProfileResponse>
    
    // ========== Keys ==========
    @POST("keys/issue")
    suspend fun issueKey(
        @Header("Authorization") token: String,
        @Body request: IssueKeyRequest
    ): Response<DigitalKey>
    
    @GET("keys")
    suspend fun listKeys(
        @Header("Authorization") token: String,
        @Query("page") page: Int? = null,
        @Query("page_size") pageSize: Int? = null
    ): Response<KeysListResponse>
    
    @GET("keys/{keyId}")
    suspend fun getKeyDetail(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String
    ): Response<DigitalKey>
    
    @POST("keys/{keyId}/activate")
    suspend fun activateKey(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String
    ): Response<KeyActionResponse>
    
    @POST("keys/{keyId}/deactivate")
    suspend fun deactivateKey(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String
    ): Response<KeyActionResponse>
    
    @POST("keys/{keyId}/share")
    suspend fun shareKey(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String,
        @Body request: ShareKeyRequest
    ): Response<ShareKeyResponse>
    
    @DELETE("keys/{keyId}")
    suspend fun revokeKey(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String
    ): Response<ApiResponse>
    
    @GET("keys/{keyId}/logs")
    suspend fun getKeyLogs(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String,
        @Query("page") page: Int? = null,
        @Query("page_size") pageSize: Int? = null
    ): Response<KeyLogsResponse>
    
    @GET("keys/shared/list")
    suspend fun getSharedKeys(
        @Header("Authorization") token: String
    ): Response<KeysListResponse>
    
    @GET("keys/{keyId}/shares")
    suspend fun getKeyShares(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String
    ): Response<SharesListResponse>
    
    @DELETE("keys/shares/{shareId}")
    suspend fun revokeShare(
        @Header("Authorization") token: String,
        @Path("shareId") shareId: String
    ): Response<ApiResponse>
    
    @PUT("keys/{keyId}/permissions")
    suspend fun updatePermissions(
        @Header("Authorization") token: String,
        @Path("keyId") keyId: String,
        @Body request: PermissionsRequest
    ): Response<ApiResponse>
    
    // ========== Vehicles ==========
    @POST("vehicles")
    suspend fun registerVehicle(
        @Header("Authorization") token: String,
        @Body request: VehicleRegistrationRequest
    ): Response<RegisterVehicleResponse>

    @GET("vehicles")
    suspend fun listVehicles(
        @Header("Authorization") token: String,
        @Query("page") page: Int? = null,
        @Query("page_size") pageSize: Int? = null
    ): Response<VehiclesListResponse>

    @GET("vehicles/{vehicleId}")
    suspend fun getVehicleDetail(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String
    ): Response<VehicleDetailResponse>

    @POST("vehicles/{vehicleId}/commands")
    suspend fun sendCommand(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String,
        @Body command: SendCommandRequest
    ): Response<CommandActionResponse>

    @GET("vehicles/{vehicleId}/commands/{commandId}")
    suspend fun getCommandStatus(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String,
        @Path("commandId") commandId: String
    ): Response<CommandStatusResponse>

    @GET("vehicles/{vehicleId}/status")
    suspend fun getVehicleStatus(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String
    ): Response<VehicleStatusResponse>

    // ========== OTA / Firmware ==========
    @GET("firmware")
    suspend fun listFirmware(
        @Header("Authorization") token: String
    ): Response<FirmwareListResponse>

    @POST("firmware")
    suspend fun uploadFirmware(
        @Header("Authorization") token: String,
        @Body request: FirmwareUploadRequest
    ): Response<FirmwareUploadResponse>

    @GET("firmware/{firmwareId}")
    suspend fun getFirmwareDetail(
        @Header("Authorization") token: String,
        @Path("firmwareId") firmwareId: String
    ): Response<FirmwareDetailResponse>

    @POST("vehicles/{vehicleId}/ota/check")
    suspend fun checkOTAUpdate(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String
    ): Response<OTAUpdateCheckResponse>

    @POST("vehicles/{vehicleId}/ota/confirm")
    suspend fun confirmOTAUpdate(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String
    ): Response<ApiResponse>

    @GET("vehicles/{vehicleId}/ota/status")
    suspend fun getOTAStatus(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String
    ): Response<OTAStatusResponse>

    @GET("vehicles/{vehicleId}/ota/history")
    suspend fun getOTAudHistory(
        @Header("Authorization") token: String,
        @Path("vehicleId") vehicleId: String
    ): Response<OTAudHistoryResponse>
}

// ========== Request/Response Data Classes ==========

data class LoginRequest(
    val username: String,
    val password: String
)

data class RegisterRequest(
    val username: String,
    val email: String,
    val password: String,
    val phone: String? = null
)

data class LoginResponse(
    val code: Int,
    val message: String,
    val data: LoginData?
)

data class LoginData(
    val token: String,
    val type: String,
    val user: UserData
)

data class UserData(
    val id: String,
    val username: String,
    val email: String,
    val role: String
)

data class AuthResponse(
    val code: Int,
    val message: String,
    val data: UserData?
)

data class TokenResponse(
    val token: String,
    val type: String
)

data class UserProfileResponse(
    val code: Int,
    val data: UserProfileData?
)

data class UserProfileData(
    val id: String,
    val username: String,
    val email: String,
    val phone: String?,
    val role: String,
    val created_at: String
)

data class KeysListResponse(
    val code: Int,
    val message: String,
    val data: KeysData?
)

data class KeysData(
    val list: List<DigitalKey>,
    val total: Int,
    val page: Int,
    val page_size: Int
)

data class KeyActionResponse(
    val code: Int,
    val message: String,
    val data: DigitalKey?
)

data class ShareKeyRequest(
    val user_id: Long,
    val expires_at: String?,
    val permissions: Map<String, Boolean>
)

data class ShareKeyResponse(
    val code: Int,
    val message: String,
    val data: ShareData?
)

data class ShareData(
    val share_id: String,
    val qr_code_url: String,
    val share_link: String,
    val expires_at: String
)

data class KeyLogsResponse(
    val code: Int,
    val message: String,
    val data: LogsData?
)

data class LogsData(
    val list: List<KeyUsageLog>,
    val total: Int,
    val page: Int,
    val page_size: Int
)

data class SharesListResponse(
    val code: Int,
    val message: String,
    val data: List<ShareRecord>?
)

data class ShareRecord(
    val id: String,
    val key_id: String,
    val shared_to: UserData,
    val status: String,
    val created_at: String,
    val expires_at: String?
)

data class PermissionsRequest(
    val permissions: Map<String, Boolean>
)

data class ApiResponse(
    val code: Int,
    val message: String
)

data class VehicleStatusResponse(
    val vehicleId: String,
    val isOnline: Boolean,
    val lastSeen: Long,
    val batteryLevel: Int?
)

data class IssueKeyRequest(
    val vehicle_id: String,
    val type: String,
    val protocol: String,
    val name: String?,
    val description: String?,
    val permissions: Map<String, Boolean>?
)

// ========== Vehicle Endpoint Data Classes ==========

data class VehicleRegistrationRequest(
    val vin: String,
    val plate: String?,
    val model: String?,
    val manufacturer: String?,
    val year: Int?,
    val color: String?
)

data class RegisterVehicleResponse(
    val code: Int,
    val message: String,
    val data: VehicleData?
)

data class VehiclesListResponse(
    val code: Int,
    val message: String,
    val data: VehiclesListData?
)

data class VehiclesListData(
    val vehicles: List<VehicleData>,
    val total: Int,
    val page: Int,
    val page_size: Int
)

data class VehicleDetailResponse(
    val code: Int,
    val message: String,
    val data: VehicleData?
)

data class VehicleData(
    val id: String,
    val vin: String,
    val plate: String?,
    val model: String?,
    val manufacturer: String?,
    val year: Int?,
    val color: String?,
    val status: String,
    val is_online: Boolean,
    val last_seen: Long?,
    val battery_level: Int?,
    val created_at: String,
    val updated_at: String?
)

data class SendCommandRequest(
    val command: String,
    val key_id: String?,
    val params: Map<String, String>?
)

data class CommandActionResponse(
    val code: Int,
    val message: String,
    val data: CommandData?
)

data class CommandStatusResponse(
    val code: Int,
    val message: String,
    val data: CommandData?
)

data class CommandData(
    val id: String,
    val vehicle_id: String,
    val command: String,
    val status: String,
    val result: String?,
    val created_at: String,
    val completed_at: String?
)

// ========== OTA / Firmware Data Classes ==========

data class FirmwareListResponse(
    val code: Int,
    val message: String,
    val data: FirmwareListData?
)

data class FirmwareListData(
    val list: List<FirmwareItem>,
    val total: Int
)

data class FirmwareItem(
    val id: Int,
    val name: String,
    val version: String,
    val size: Int,
    val md5: String,
    val description: String?,
    val created_at: String
)

data class FirmwareUploadRequest(
    val name: String,
    val version: String,
    val file_url: String,
    val md5: String,
    val description: String?
)

data class FirmwareUploadResponse(
    val code: Int,
    val message: String,
    val data: FirmwareItem?
)

data class FirmwareDetailResponse(
    val code: Int,
    val message: String,
    val data: FirmwareDetailData?
)

data class FirmwareDetailData(
    val id: Int,
    val name: String,
    val version: String,
    val size: Int,
    val md5: String,
    val description: String?,
    val created_at: String,
    val updated_at: String?
)

data class OTAUpdateCheckResponse(
    val code: Int,
    val message: String,
    val data: OTAUpdateCheckData?
)

data class OTAUpdateCheckData(
    val has_update: Boolean,
    val firmware: FirmwareItem?,
    val latest_version: String?
)

data class OTAStatusResponse(
    val code: Int,
    val message: String,
    val data: OTAStatusData?
)

data class OTAStatusData(
    val status: String,
    val progress: Int?,
    val firmware: FirmwareItem?,
    val started_at: String?,
    val completed_at: String?
)

data class OTAudHistoryResponse(
    val code: Int,
    val message: String,
    val data: OTAudHistoryData?
)

data class OTAudHistoryData(
    val list: List<OTAudHistoryItem>,
    val total: Int
)

data class OTAudHistoryItem(
    val id: Int,
    val firmware: FirmwareItem,
    val status: String,
    val progress: Int?,
    val started_at: String,
    val completed_at: String?
)
