package com.yuledkcs

import android.util.Log
import com.yuledkcs.model.DigitalKey
import com.yuledkcs.model.Permission
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.concurrent.ConcurrentHashMap

/**
 * Manager for digital key operations
 * 
 * Handles issuing, listing, sharing, and revoking digital keys.
 */
class KeyManager private constructor() {
    
    companion object {
        private const val TAG = "KeyManager"
        
        @Volatile
        private var instance: KeyManager? = null
        
        fun getInstance(): KeyManager {
            return instance ?: synchronized(this) {
                instance ?: KeyManager().also { instance = it }
            }
        }
    }
    
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val keysCache = ConcurrentHashMap<String, DigitalKey>()
    
    /**
     * Issue a new digital key for a vehicle
     * 
     * @param vehicleId The vehicle identifier
     * @param callback Callback with Result containing the issued DigitalKey
     */
    fun issueKey(
        vehicleId: String,
        callback: (Result<DigitalKey>) -> Unit
    ) {
        YuleDKCS.checkInitialized()
        
        scope.launch {
            try {
                val response = YuleDKCS.api.issueKey(
                    apiKey = YuleDKCS.apiKey,
                    request = IssueKeyRequest(vehicleId)
                )
                
                if (response.isSuccessful) {
                    val key = response.body()
                    if (key != null) {
                        // Store key securely
                        storeKey(key)
                        keysCache[key.id] = key
                        
                        withContext(Dispatchers.Main) {
                            callback(Result.success(key))
                        }
                    } else {
                        throw IllegalStateException("Empty response body")
                    }
                } else {
                    throw Exception("Failed to issue key: ${response.code()}")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error issuing key", e)
                withContext(Dispatchers.Main) {
                    callback(Result.failure(e))
                }
            }
        }
    }
    
    /**
     * Get list of all keys
     * 
     * @return List of DigitalKey objects
     */
    fun listKeys(): List<DigitalKey> {
        YuleDKCS.checkInitialized()
        
        // Return cached keys, refresh from server if needed
        if (keysCache.isEmpty()) {
            refreshKeys()
        }
        
        return keysCache.values.toList()
    }
    
    /**
     * Share a key with another user
     * 
     * @param keyId The key identifier to share
     * @param to Email or phone number of recipient
     * @param permissions List of permissions to grant
     * @param callback Optional callback for result
     */
    fun shareKey(
        keyId: String,
        to: String,
        permissions: List<Permission>,
        callback: ((Result<Unit>) -> Unit)? = null
    ) {
        YuleDKCS.checkInitialized()
        
        scope.launch {
            try {
                val response = YuleDKCS.api.shareKey(
                    apiKey = YuleDKCS.apiKey,
                    keyId = keyId,
                    request = ShareKeyRequest(to, permissions.map { it.name })
                )
                
                withContext(Dispatchers.Main) {
                    if (response.isSuccessful) {
                        callback?.invoke(Result.success(Unit))
                    } else {
                        callback?.invoke(
                            Result.failure(
                                Exception("Failed to share key: ${response.code()}")
                            )
                        )
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error sharing key", e)
                withContext(Dispatchers.Main) {
                    callback?.invoke(Result.failure(e))
                }
            }
        }
    }
    
    /**
     * Revoke a key
     * 
     * @param keyId The key identifier to revoke
     * @param callback Optional callback for result
     */
    fun revokeKey(
        keyId: String,
        callback: ((Result<Unit>) -> Unit)? = null
    ) {
        YuleDKCS.checkInitialized()
        
        scope.launch {
            try {
                val response = YuleDKCS.api.revokeKey(
                    apiKey = YuleDKCS.apiKey,
                    keyId = keyId
                )
                
                if (response.isSuccessful) {
                    keysCache.remove(keyId)
                    deleteStoredKey(keyId)
                }
                
                withContext(Dispatchers.Main) {
                    if (response.isSuccessful) {
                        callback?.invoke(Result.success(Unit))
                    } else {
                        callback?.invoke(
                            Result.failure(
                                Exception("Failed to revoke key: ${response.code()}")
                            )
                        )
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error revoking key", e)
                withContext(Dispatchers.Main) {
                    callback?.invoke(Result.failure(e))
                }
            }
        }
    }
    
    /**
     * Get a specific key by ID
     * 
     * @param keyId The key identifier
     * @return DigitalKey or null if not found
     */
    fun getKey(keyId: String): DigitalKey? {
        return keysCache[keyId] ?: loadStoredKey(keyId)
    }
    
    private fun refreshKeys() {
        scope.launch {
            try {
                val response = YuleDKCS.api.listKeys(YuleDKCS.apiKey)
                if (response.isSuccessful) {
                    response.body()?.let { keys ->
                        keysCache.clear()
                        keys.forEach { keysCache[it.id] = it }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error refreshing keys", e)
            }
        }
    }
    
    private fun storeKey(key: DigitalKey) {
        // Store encrypted key data using SecurityProvider
        YuleDKCS.securityProvider.storeSecureData(
            "key_${key.id}",
            key.encryptedData
        )
    }
    
    private fun loadStoredKey(keyId: String): DigitalKey? {
        val encryptedData = YuleDKCS.securityProvider.getSecureData("key_$keyId")
        return if (encryptedData != null) {
            DigitalKey(
                id = keyId,
                encryptedData = encryptedData
            )
        } else null
    }
    
    private fun deleteStoredKey(keyId: String) {
        YuleDKCS.securityProvider.deleteSecureData("key_$keyId")
    }
}

// API Request/Response data classes
internal data class IssueKeyRequest(val vehicleId: String)
internal data class ShareKeyRequest(
    val recipient: String,
    val permissions: List<String>
)
