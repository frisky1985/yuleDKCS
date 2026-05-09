package com.yuledkcs.api

import com.yuledkcs.model.DigitalKey
import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.DELETE
import retrofit2.http.GET
import retrofit2.http.Header
import retrofit2.http.POST
import retrofit2.http.Path

/**
 * Retrofit API interface for YuleDKCS backend
 */
interface YuleDKCSApi {
    
    @POST("keys/issue")
    suspend fun issueKey(
        @Header("Authorization") apiKey: String,
        @Body request: com.yuledkcs.IssueKeyRequest
    ): Response<DigitalKey>
    
    @GET("keys")
    suspend fun listKeys(
        @Header("Authorization") apiKey: String
    ): Response<List<DigitalKey>>
    
    @POST("keys/{keyId}/share")
    suspend fun shareKey(
        @Header("Authorization") apiKey: String,
        @Path("keyId") keyId: String,
        @Body request: com.yuledkcs.ShareKeyRequest
    ): Response<Unit>
    
    @DELETE("keys/{keyId}")
    suspend fun revokeKey(
        @Header("Authorization") apiKey: String,
        @Path("keyId") keyId: String
    ): Response<Unit>
    
    @GET("vehicles/{vehicleId}/status")
    suspend fun getVehicleStatus(
        @Header("Authorization") apiKey: String,
        @Path("vehicleId") vehicleId: String
    ): Response<VehicleStatusResponse>
}

data class VehicleStatusResponse(
    val vehicleId: String,
    val isOnline: Boolean,
    val lastSeen: Long,
    val batteryLevel: Int?
)
