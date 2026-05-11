package com.yuledkcs

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import com.yuledkcs.model.DigitalKey
import com.yuledkcs.model.KeyPermission
import com.yuledkcs.model.KeyUsageLog
import com.yuledkcs.model.Permission
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Date
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap

/**
 * Manager for digital key operations
 * 
 * Handles issuing, listing, sharing, and revoking digital keys.
 * Also manages key usage logs and offline operations.
 */
class KeyManager private constructor(private val context: Context) {
    
    companion object {
        private const val TAG = "KeyManager"
        private const val PREFS_NAME = "key_manager_prefs"
        private const val KEY_LOGS_PREFIX = "usage_logs_"
        
        @Volatile
        private var instance: KeyManager? = null
        
        fun getInstance(context: Context): KeyManager {
            return instance ?: synchronized(this) {
                instance ?: KeyManager(context.applicationContext).also { instance = it }
            }
        }
    }
    
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val keysCache = ConcurrentHashMap<String, DigitalKey>()
    private val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    
    // StateFlow for reactive UI updates
    private val _keysFlow = MutableStateFlow<List<DigitalKey>>(emptyList())
    val keysFlow: StateFlow<List<DigitalKey>> = _keysFlow
    
    private val _activeKeyFlow = MutableStateFlow<DigitalKey?>(null)
    val activeKeyFlow: StateFlow<DigitalKey?> = _activeKeyFlow

    init {
        // Load cached keys on initialization
        scope.launch {
            loadCachedKeys()
        }
    }
    
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
                        _keysFlow.value = keysCache.values.toList()
                        
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
     * Receive a shared key via QR code or deep link
     * 
     * @param shareToken The share token from QR code or link
     * @param callback Callback with Result containing the received DigitalKey
     */
    fun receiveSharedKey(
        shareToken: String,
        callback: (Result<DigitalKey>) -> Unit
    ) {
        YuleDKCS.checkInitialized()
        
        scope.launch {
            try {
                val response = YuleDKCS.api.receiveSharedKey(
                    apiKey = YuleDKCS.apiKey,
                    token = shareToken
                )
                
                if (response.isSuccessful) {
                    val key = response.body()
                    if (key != null) {
                        storeKey(key)
                        keysCache[key.id] = key
                        _keysFlow.value = keysCache.values.toList()
                        
                        withContext(Dispatchers.Main) {
                            callback(Result.success(key))
                        }
                    } else {
                        throw IllegalStateException("Empty response body")
                    }
                } else {
                    throw Exception("Failed to receive key: ${response.code()}")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error receiving shared key", e)
                withContext(Dispatchers.Main) {
                    callback(Result.failure(e))
                }
            }
        }
    }
    
    /**
     * Activate a key (bind to SE)
     * 
     * @param keyId The key identifier
     * @param callback Callback with Result
     */
    fun activateKey(
        keyId: String,
        callback: ((Result<Unit>) -> Unit)? = null
    ) {
        YuleDKCS.checkInitialized()
        
        scope.launch {
            try {
                // First activate on server
                val response = YuleDKCS.api.activateKey(
                    apiKey = YuleDKCS.apiKey,
                    keyId = keyId
                )
                
                if (response.isSuccessful) {
                    // Then store in SE
                    val key = keysCache[keyId]
                    if (key != null) {
                        YuleDKCS.securityProvider.storeKeyInSE(keyId, key.keyData)
                        
                        // Update local status
                        val updatedKey = key.copy(status = "active")
                        keysCache[keyId] = updatedKey
                        _keysFlow.value = keysCache.values.toList()
                        
                        // Update active key
                        _activeKeyFlow.value = updatedKey
                    }
                    
                    withContext(Dispatchers.Main) {
                        callback?.invoke(Result.success(Unit))
                    }
                } else {
                    throw Exception("Failed to activate key: ${response.code()}")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error activating key", e)
                withContext(Dispatchers.Main) {
                    callback?.invoke(Result.failure(e))
                }
            }
        }
    }
    
    /**
     * Get list of all keys
     * 
     * @param forceRefresh Force refresh from server
     * @return List of DigitalKey objects
     */
    fun listKeys(forceRefresh: Boolean = false): List<DigitalKey> {
        YuleDKCS.checkInitialized()
        
        if (forceRefresh || keysCache.isEmpty()) {
            refreshKeys()
        }
        
        return keysCache.values.toList()
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
    
    /**
     * Set active key for quick access
     * 
     * @param keyId The key identifier
     */
    fun setActiveKey(keyId: String?) {
        _activeKeyFlow.value = keyId?.let { keysCache[it] }
        prefs.edit().putString("active_key_id", keyId).apply()
    }
    
    /**
     * Get the currently active key
     */
    fun getActiveKey(): DigitalKey? {
        val activeId = prefs.getString("active_key_id", null)
        return activeId?.let { getKey(it) }
    }
    
    /**
     * Share a key with another user
     * 
     * @param keyId The key identifier to share
     * @param to Email or phone number of recipient
     * @param permissions List of permissions to grant
     * @param expiresInDays Expiration in days
     * @param callback Optional callback for result
     */
    fun shareKey(
        keyId: String,
        to: String,
        permissions: List<Permission>,
        expiresInDays: Int = 7,
        callback: ((Result<String>) -> Unit)? = null
    ) {
        YuleDKCS.checkInitialized()
        
        scope.launch {
            try {
                val response = YuleDKCS.api.shareKey(
                    apiKey = YuleDKCS.apiKey,
                    keyId = keyId,
                    request = ShareKeyRequest(
                        recipient = to,
                        permissions = permissions.map { it.name },
                        expiresInDays = expiresInDays
                    )
                )
                
                withContext(Dispatchers.Main) {
                    if (response.isSuccessful) {
                        val shareResult = response.body()
                        callback?.invoke(Result.success(shareResult?.shareLink ?: ""))
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
                    _keysFlow.value = keysCache.values.toList()
                    
                    // Clear active key if it was the revoked one
                    if (_activeKeyFlow.value?.id == keyId) {
                        _activeKeyFlow.value = null
                        prefs.edit().remove("active_key_id").apply()
                    }
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
     * Delete local key data only (keep server key)
     * Use this when user wants to remove key from this device only
     * 
     * @param keyId The key identifier
     */
    fun deleteLocalKey(keyId: String) {
        keysCache.remove(keyId)
        deleteStoredKey(keyId)
        _keysFlow.value = keysCache.values.toList()
        
        if (_activeKeyFlow.value?.id == keyId) {
            _activeKeyFlow.value = null
            prefs.edit().remove("active_key_id").apply()
        }
    }
    
    /**
     * Record a key usage log
     * 
     * @param keyId The key identifier
     * @param operation The operation performed
     * @param status Success or failure status
     * @param location Optional location
     */
    fun recordUsage(
        keyId: String,
        operation: String,
        status: KeyUsageLog.Status,
        location: Pair<Double, Double>? = null,
        failureReason: String? = null
    ) {
        val log = KeyUsageLog(
            id = UUID.randomUUID().toString(),
            keyId = keyId,
            operation = operation,
            status = status,
            timestamp = Date(),
            location = location?.let { KeyUsageLog.Location(it.first, it.second) },
            deviceInfo = android.os.Build.MODEL,
            failureReason = failureReason
        )
        
        // Store locally
        storeUsageLog(keyId, log)
        
        // Try to sync to server
        scope.launch {
            try {
                YuleDKCS.api.recordUsage(
                    apiKey = YuleDKCS.apiKey,
                    keyId = keyId,
                    log = log
                )
            } catch (e: Exception) {
                Log.w(TAG, "Failed to sync usage log, will retry later", e)
            }
        }
    }
    
    /**
     * Get usage logs for a key
     * 
     * @param keyId The key identifier
     * @return List of usage logs
     */
    fun getUsageLogs(keyId: String): List<KeyUsageLog> {
        return loadUsageLogs(keyId)
    }
    
    /**
     * Check if a key has a specific permission
     * 
     * @param keyId The key identifier
     * @param permission The permission to check
     * @return true if key has the permission
     */
    fun hasPermission(keyId: String, permission: Permission): Boolean {
        val key = keysCache[keyId] ?: return false
        return key.permissions.any { it.type == permission.name && it.enabled }
    }
    
    /**
     * Check if key is expired
     * 
     * @param keyId The key identifier
     * @return true if key is expired
     */
    fun isKeyExpired(keyId: String): Boolean {
        val key = keysCache[keyId] ?: return true
        key.expiresAt?.let {
            return Date().after(it)
        }
        return false
    }
    
    // Private helper methods
    
    private fun loadCachedKeys() {
        // Load from secure storage
        val keyIds = prefs.getStringSet("key_ids", emptySet()) ?: emptySet()
        keyIds.forEach { keyId ->
            loadStoredKey(keyId)?.let { keysCache[keyId] = it }
        }
        _keysFlow.value = keysCache.values.toList()
        
        // Load active key
        val activeId = prefs.getString("active_key_id", null)
        _activeKeyFlow.value = activeId?.let { keysCache[it] }
        
        // Refresh from server
        refreshKeys()
    }
    
    private fun refreshKeys() {
        scope.launch {
            try {
                val response = YuleDKCS.api.listKeys(YuleDKCS.apiKey)
                if (response.isSuccessful) {
                    response.body()?.let { keys ->
                        keysCache.clear()
                        keys.forEach { 
                            keysCache[it.id] = it
                            storeKey(it)
                        }
                        _keysFlow.value = keysCache.values.toList()
                        
                        // Update key ID set
                        prefs.edit().putStringSet("key_ids", keysCache.keys).apply()
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error refreshing keys", e)
            }
        }
    }
    
    private fun storeKey(key: DigitalKey) {
        YuleDKCS.securityProvider.storeSecureData(
            "key_${key.id}",
            key.keyData
        )
    }
    
    private fun loadStoredKey(keyId: String): DigitalKey? {
        val keyData = YuleDKCS.securityProvider.getSecureData("key_$keyId") ?: return null
        return DigitalKey(
            id = keyId,
            keyData = keyData
        )
    }
    
    private fun deleteStoredKey(keyId: String) {
        YuleDKCS.securityProvider.deleteSecureData("key_$keyId")
    }
    
    private fun storeUsageLog(keyId: String, log: KeyUsageLog) {
        val logs = loadUsageLogs(keyId).toMutableList()
        logs.add(0, log) // Add to beginning
        
        // Keep only last 100 logs
        if (logs.size > 100) {
            logs.subList(100, logs.size).clear()
        }
        
        // Serialize and store
        val json = logs.joinToString("|") { it.toJson() }
        prefs.edit().putString("$KEY_LOGS_PREFIX$keyId", json).apply()
    }
    
    private fun loadUsageLogs(keyId: String): List<KeyUsageLog> {
        val json = prefs.getString("$KEY_LOGS_PREFIX$keyId", null) ?: return emptyList()
        return json.split("|").mapNotNull { KeyUsageLog.fromJson(it) }
    }
}

// API Request/Response data classes
internal data class IssueKeyRequest(val vehicleId: String)
internal data class ShareKeyRequest(
    val recipient: String,
    val permissions: List<String>,
    val expiresInDays: Int = 7
)
