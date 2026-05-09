package com.yuledkcs.model

import com.google.gson.annotations.SerializedName

/**
 * Represents a digital key for vehicle access
 */
data class DigitalKey(
    @SerializedName("id")
    val id: String,
    
    @SerializedName("vehicle_id")
    val vehicleId: String? = null,
    
    @SerializedName("owner_id")
    val ownerId: String? = null,
    
    @SerializedName("created_at")
    val createdAt: Long? = null,
    
    @SerializedName("expires_at")
    val expiresAt: Long? = null,
    
    @SerializedName("status")
    val status: KeyStatus = KeyStatus.ACTIVE,
    
    @SerializedName("permissions")
    val permissions: List<Permission> = emptyList(),
    
    @SerializedName("encrypted_data")
    val encryptedData: ByteArray = byteArrayOf(),
    
    @SerializedName("vehicle_name")
    val vehicleName: String? = null,
    
    @SerializedName("vehicle_model")
    val vehicleModel: String? = null
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        
        other as DigitalKey
        
        if (id != other.id) return false
        if (!encryptedData.contentEquals(other.encryptedData)) return false
        
        return true
    }
    
    override fun hashCode(): Int {
        var result = id.hashCode()
        result = 31 * result + encryptedData.contentHashCode()
        return result
    }
    
    /**
     * Check if key is currently valid
     */
    fun isValid(): Boolean {
        if (status != KeyStatus.ACTIVE) return false
        expiresAt?.let {
            if (System.currentTimeMillis() / 1000 > it) return false
        }
        return true
    }
    
    /**
     * Check if key has specific permission
     */
    fun hasPermission(permission: Permission): Boolean {
        return permissions.contains(permission) || permissions.contains(Permission.ALL)
    }
}

enum class KeyStatus {
    @SerializedName("active")
    ACTIVE,
    
    @SerializedName("suspended")
    SUSPENDED,
    
    @SerializedName("revoked")
    REVOKED,
    
    @SerializedName("expired")
    EXPIRED
}
