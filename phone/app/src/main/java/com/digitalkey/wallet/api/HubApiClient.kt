package com.digitalkey.wallet.api

import com.digitalkey.wallet.Models.*
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * HUB Cloud API Client
 * 手机App与云端HUB的通信层
 */
class HubApiClient(private val baseUrl: String) {

    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .writeTimeout(30, TimeUnit.SECONDS)
        .addInterceptor(AuthInterceptor())
        .build()

    private val gson = Gson()
    private val jsonType = "application/json; charset=utf-8".toMediaType()

    // ── 密钥管理 ──

    suspend fun bindKey(request: BindKeyRequest): Result<BindKeyResponse> = withContext(Dispatchers.IO) {
        post("/api/v1/keys", request)
    }

    suspend fun unbindKey(keyId: String): Result<ApiResponse> = withContext(Dispatchers.IO) {
        delete("/api/v1/keys/$keyId")
    }

    suspend fun suspendKey(keyId: String): Result<ApiResponse> = withContext(Dispatchers.IO) {
        put("/api/v1/keys/$keyId/suspend", emptyMap<String, Any>())
    }

    suspend fun resumeKey(keyId: String): Result<ApiResponse> = withContext(Dispatchers.IO) {
        put("/api/v1/keys/$keyId/resume", emptyMap())
    }

    suspend fun revokeKey(keyId: String, reason: String): Result<ApiResponse> = withContext(Dispatchers.IO) {
        put("/api/v1/keys/$keyId/revoke", mapOf("reason" to reason))
    }

    suspend fun listKeys(vehicleId: String? = null): Result<ListKeysResponse> = withContext(Dispatchers.IO) {
        val path = if (vehicleId != null) "/api/v1/keys?vehicle_id=$vehicleId" else "/api/v1/keys"
        get(path)
    }

    // ── 密钥分享 ──

    suspend fun createShare(request: CreateShareRequest): Result<CreateShareResponse> = withContext(Dispatchers.IO) {
        post("/api/v1/shares", request)
    }

    suspend fun acceptShare(shareCode: String, deviceId: String): Result<AcceptShareResponse> = withContext(Dispatchers.IO) {
        post("/api/v1/shares/accept", mapOf(
            "share_code" to shareCode,
            "device_id" to deviceId
        ))
    }

    suspend fun cancelShare(shareId: String): Result<ApiResponse> = withContext(Dispatchers.IO) {
        delete("/api/v1/shares/$shareId")
    }

    // ── 车辆控制 ──

    suspend fun sendCommand(vehicleId: String, action: ControlAction, keyId: String): Result<CommandResponse> =
        withContext(Dispatchers.IO) {
            post("/api/v1/vehicles/$vehicleId/command", mapOf(
                "action" to action.name.lowercase(),
                "key_id" to keyId,
                "source" to 4  // Remote
            ))
        }

    suspend fun getVehicleStatus(vehicleId: String): Result<VehicleStatus> = withContext(Dispatchers.IO) {
        get("/api/v1/vehicles/$vehicleId/status")
    }

    // ── HTTP helpers ──

    private inline fun <reified T> get(path: String): Result<T> {
        val request = Request.Builder().url("$baseUrl$path").get().build()
        return execute(request)
    }

    private inline fun <reified T> post(path: String, body: Any): Result<T> {
        val json = gson.toJson(body)
        val request = Request.Builder()
            .url("$baseUrl$path")
            .post(json.toRequestBody(jsonType))
            .build()
        return execute(request)
    }

    private inline fun <reified T> put(path: String, body: Any): Result<T> {
        val json = gson.toJson(body)
        val request = Request.Builder()
            .url("$baseUrl$path")
            .put(json.toRequestBody(jsonType))
            .build()
        return execute(request)
    }

    private inline fun <reified T> delete(path: String): Result<T> {
        val request = Request.Builder().url("$baseUrl$path").delete().build()
        return execute(request)
    }

    private inline fun <reified T> execute(request: Request): Result<T> {
        return try {
            val response = client.newCall(request).execute()
            val responseBody = response.body?.string() ?: return Result.failure(IOException("Empty response"))

            if (!response.isSuccessful) {
                return Result.failure(IOException("HTTP ${response.code}: $responseBody"))
            }

            val type = object : TypeToken<T>() {}.type
            val result = gson.fromJson<T>(responseBody, type)
            Result.success(result)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    // ── Request/Response models ──

    data class BindKeyRequest(
        val vehicle_id: String,
        val device_id: String,
        val vendor: String,
        val protocol: String,
        val key_type: String,
        val access_level: Map<String, Boolean>,
        val device_pubkey: String,  // Base64
        val valid_from: Long,
        val valid_until: Long
    )

    data class BindKeyResponse(
        val key: DigitalKey?,
        val vehicle_pubkey: String?,  // Base64
        val shared_secret: String?,   // Base64
        val error_code: String?,
        val error_msg: String?
    )

    data class CreateShareRequest(
        val key_id: String,
        val from_user_id: String,
        val to_vendor: String?,
        val to_user_id: String?,
        val access_level: Map<String, Boolean>,
        val valid_from: Long,
        val valid_until: Long,
        val max_uses: Int?
    )

    data class CreateShareResponse(
        val share_id: String,
        val share_code: String?,
        val error_code: String?
    )

    data class AcceptShareResponse(
        val key: DigitalKey?,
        val shared_secret: String?,
        val error_code: String?
    )

    data class ListKeysResponse(
        val keys: List<DigitalKey>,
        val total: Int
    )

    data class CommandResponse(
        val cmd_id: String,
        val result_code: Int,
        val error_msg: String?
    )

    data class ApiResponse(
        val error_code: String?,
        val message: String?
    )

    // ── Auth interceptor ──
    private class AuthInterceptor : Interceptor {
        override fun intercept(chain: Interceptor.Chain): Response {
            val request = chain.request().newBuilder()
                .addHeader("Authorization", "Bearer <user_token>")  // TODO: from secure storage
                .addHeader("X-Device-Id", "<device_id>")
                .addHeader("X-App-Version", "1.0.0")
                .build()
            return chain.proceed(request)
        }
    }
}
