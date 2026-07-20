// KeyManager.kt - 密钥管理模块
package com.digitalkey.sdk.key

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import com.digitalkey.sdk.error.DkError
import com.digitalkey.sdk.error.DkErrorCode
import com.digitalkey.sdk.logger.DkLogger
import com.digitalkey.sdk.telemetry.DkTelemetry
import kotlinx.coroutines.*
import org.json.JSONObject
import java.security.KeyStore
import java.security.SecureRandom
import java.util.*
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

// 密钥类型
enum class KeyType(val value: Int) {
    PRIMARY(0x01),      // 主钥匙
    SECONDARY(0x02),    // 副钥匙
    SHARE(0x03),        // 分享钥匙
    TEMPORARY(0x04)     // 临时钥匙
}

// 密钥状态
enum class KeyStatus(val value: Int) {
    PENDING(0x00),      // 待激活
    ACTIVE(0x01),      // 已激活
    SUSPENDED(0x02),   // 已挂起
    REVOKED(0x03),     // 已撤销
    EXPIRED(0x04)      // 已过期
}

// 数字钥匙信息
data class DigitalKey(
    val keyId: String,
    val vehicleId: String,
    val userId: String,
    val keyType: KeyType,
    val status: KeyStatus,
    val validFrom: Long,
    val validUntil: Long,
    val maxUses: Int?,
    val usedCount: Int,
    val shareCode: String?,
    val issuerId: String,
    val vendor: String,
    val protocolVersion: String,
    val createdAt: Long,
    val updatedAt: Long
) {
    fun isValid(): Boolean {
        val now = System.currentTimeMillis()
        return status == KeyStatus.ACTIVE &&
                now >= validFrom &&
                now <= validUntil &&
                (maxUses == null || usedCount < maxUses)
    }
    
    fun remainingUses(): Int? {
        return maxUses?.let { it - usedCount }
    }
}

// 密钥事件监听器
interface KeyEventListener {
    fun onKeyCreated(key: DigitalKey)
    fun onKeyActivated(keyId: String)
    fun onKeyDeleted(keyId: String)
    fun onKeyExpired(keyId: String)
    fun onKeyStatusChanged(keyId: String, oldStatus: KeyStatus, newStatus: KeyStatus)
    fun onError(error: DkError)
}

// 密钥管理器
class KeyManager(private val context: Context) {
    
    private val logger = DkLogger.getLogger("KeyManager")
    private val telemetry = DkTelemetry
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    
    private val keyStore = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
    private val secureRandom = SecureRandom()
    
    private val listeners = mutableListOf<KeyEventListener>()
    private val keysCache = mutableMapOf<String, DigitalKey>()
    
    // KeyStore alias prefix
    companion object {
        private const val KEY_ALIAS_PREFIX = "dk_key_"
        private const val TRANSACTION_ID_ALIAS = "dk_transaction_counter"
        private const val AES_GCM_TAG_LENGTH = 128
        private const val ANDROID_KEYSTORE = "AndroidKeyStore"
        
        // Transaction counter for replay protection
        @Volatile
        private var transactionCounter: Long = 0
    }
    
    init {
        loadKeysFromStorage()
        logger.info("Key Manager initialized, loaded ${keysCache.size} keys")
    }
    
    // 添加监听器
    fun addListener(listener: KeyEventListener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener)
        }
    }
    
    // 移除监听器
    fun removeListener(listener: KeyEventListener) {
        listeners.remove(listener)
    }
    
    // 创建新钥匙
    suspend fun createKey(
        vehicleId: String,
        userId: String,
        keyType: KeyType,
        validFrom: Long,
        validUntil: Long,
        maxUses: Int? = null,
        issuerId: String = "default",
        vendor: String = "CCC",
        protocolVersion: String = "3.0"
    ): DkResult<DigitalKey> = withContext(Dispatchers.IO) {
        try {
            // Generate key ID
            val keyId = generateKeyId()
            
            // Store in KeyStore
            val keyAlias = "$KEY_ALIAS_PREFIX$keyId"
            generateKeyPair(keyAlias)
            
            // Create key object
            val key = DigitalKey(
                keyId = keyId,
                vehicleId = vehicleId,
                userId = userId,
                keyType = keyType,
                status = KeyStatus.PENDING,
                validFrom = validFrom,
                validUntil = validUntil,
                maxUses = maxUses,
                usedCount = 0,
                shareCode = null,
                issuerId = issuerId,
                vendor = vendor,
                protocolVersion = protocolVersion,
                createdAt = System.currentTimeMillis(),
                updatedAt = System.currentTimeMillis()
            )
            
            // Save to storage
            saveKeyToStorage(key)
            keysCache[keyId] = key
            
            logger.info("Key created: $keyId, type: $keyType")
            telemetry.track("key_create", mapOf(
                "key_id" to keyId,
                "vehicle_id" to vehicleId,
                "key_type" to keyType.name
            ))
            
            listeners.forEach { it.onKeyCreated(key) }
            
            DkResult.success(key)
        } catch (e: Exception) {
            logger.error("Failed to create key", e)
            telemetry.trackError(DkErrorCode.keyBindFailed.toInt(), "Key creation failed: ${e.message}")
            DkResult.failure(DkError(DkErrorCode.keyBindFailed, cause = e))
        }
    }
    
    // 激活钥匙
    suspend fun activateKey(keyId: String): DkResult<Unit> = withContext(Dispatchers.IO) {
        val key = keysCache[keyId] ?: return@withContext DkResult.failure(
            DkError(DkErrorCode.keyNotFound, "Key not found: $keyId")
        )
        
        val oldStatus = key.status
        val activatedKey = key.copy(
            status = KeyStatus.ACTIVE,
            updatedAt = System.currentTimeMillis()
        )
        
        saveKeyToStorage(activatedKey)
        keysCache[keyId] = activatedKey
        
        logger.info("Key activated: $keyId")
        telemetry.track("key_activate", mapOf("key_id" to keyId))
        
        listeners.forEach { it.onKeyActivated(keyId) }
        
        DkResult.success(Unit)
    }
    
    // 删除钥匙
    suspend fun deleteKey(keyId: String): DkResult<Unit> = withContext(Dispatchers.IO) {
        val key = keysCache[keyId] ?: return@withContext DkResult.failure(
            DkError(DkErrorCode.keyNotFound, "Key not found: $keyId")
        )
        
        // Remove from KeyStore
        val keyAlias = "$KEY_ALIAS_PREFIX$keyId"
        keyStore.deleteEntry(keyAlias)
        
        // Remove from storage
        removeKeyFromStorage(keyId)
        keysCache.remove(keyId)
        
        logger.info("Key deleted: $keyId")
        telemetry.track("key_delete", mapOf("key_id" to keyId))
        
        listeners.forEach { it.onKeyDeleted(keyId) }
        
        DkResult.success(Unit)
    }
    
    // 使用钥匙
    suspend fun useKey(keyId: String): DkResult<DigitalKey> = withContext(Dispatchers.IO) {
        val key = keysCache[keyId] ?: return@withContext DkResult.failure(
            DkError(DkErrorCode.keyNotFound, "Key not found: $keyId")
        )
        
        if (!key.isValid()) {
            if (key.status == KeyStatus.ACTIVE && System.currentTimeMillis() > key.validUntil) {
                val expiredKey = key.copy(status = KeyStatus.EXPIRED)
                keysCache[keyId] = expiredKey
                saveKeyToStorage(expiredKey)
                listeners.forEach { it.onKeyExpired(keyId) }
            }
            return@withContext DkResult.failure(
                DkError(DkErrorCode.keyNotActive, "Key is not valid")
            )
        }
        
        // Update usage count
        val usedKey = key.copy(
            usedCount = key.usedCount + 1,
            updatedAt = System.currentTimeMillis()
        )
        
        // Check if max uses reached
        if (usedKey.maxUses != null && usedKey.usedCount >= usedKey.maxUses) {
            val revokedKey = usedKey.copy(status = KeyStatus.REVOKED)
            keysCache[keyId] = revokedKey
            saveKeyToStorage(revokedKey)
        } else {
            keysCache[keyId] = usedKey
            saveKeyToStorage(usedKey)
        }
        
        telemetry.track("key_use", mapOf(
            "key_id" to keyId,
            "vehicle_id" to key.vehicleId,
            "used_count" to usedKey.usedCount
        ))
        
        DkResult.success(usedKey)
    }
    
    // 获取钥匙
    fun getKey(keyId: String): DigitalKey? {
        return keysCache[keyId]
    }
    
    // 获取用户所有钥匙
    fun getUserKeys(userId: String): List<DigitalKey> {
        return keysCache.values.filter { it.userId == userId }
    }
    
    // 获取车辆所有钥匙
    fun getVehicleKeys(vehicleId: String): List<DigitalKey> {
        return keysCache.values.filter { it.vehicleId == vehicleId }
    }
    
    // 获取钥匙列表
    fun getAllKeys(): List<DigitalKey> {
        return keysCache.values.toList()
    }
    
    // 获取有效钥匙
    fun getActiveKeys(): List<DigitalKey> {
        return keysCache.values.filter { it.isValid() }
    }
    
    // 创建分享
    suspend fun createShare(keyId: String, shareType: KeyType, validUntil: Long, maxUses: Int?): DkResult<String> = withContext(Dispatchers.IO) {
        val key = keysCache[keyId] ?: return@withContext DkResult.failure(
            DkError(DkErrorCode.keyNotFound, "Key not found: $keyId")
        )
        
        if (!key.isValid()) {
            return@withContext DkResult.failure(
                DkError(DkErrorCode.keyNotActive, "Key is not valid")
            )
        }
        
        // Generate share code
        val shareCode = generateShareCode()
        
        // Create shared key metadata
        val shareKey = key.copy(
            keyId = generateKeyId(),
            keyType = shareType,
            status = KeyStatus.PENDING,
            validFrom = System.currentTimeMillis(),
            validUntil = validUntil,
            maxUses = maxUses,
            usedCount = 0,
            shareCode = shareCode,
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis()
        )
        
        saveKeyToStorage(shareKey)
        keysCache[shareKey.keyId] = shareKey
        
        telemetry.track("share_create", mapOf(
            "original_key_id" to keyId,
            "share_key_id" to shareKey.keyId,
            "share_code" to shareCode
        ))
        
        DkResult.success(shareCode)
    }
    
    // 接受分享
    suspend fun acceptShare(shareCode: String, userId: String): DkResult<DigitalKey> = withContext(Dispatchers.IO) {
        val shareKey = keysCache.values.find { it.shareCode == shareCode }
            ?: return@withContext DkResult.failure(
                DkError(DkErrorCode.shareNotFound, "Share not found")
            )
        
        if (shareKey.status != KeyStatus.PENDING) {
            return@withContext DkResult.failure(
                DkError(DkErrorCode.shareNotAllowed, "Share already accepted or expired")
            )
        }
        
        if (System.currentTimeMillis() > shareKey.validUntil) {
            return@withContext DkResult.failure(
                DkError(DkErrorCode.shareExpired, "Share code expired")
            )
        }
        
        // Activate shared key
        val acceptedKey = shareKey.copy(
            userId = userId,
            status = KeyStatus.ACTIVE,
            shareCode = null,
            updatedAt = System.currentTimeMillis()
        )
        
        saveKeyToStorage(acceptedKey)
        keysCache[acceptedKey.keyId] = acceptedKey
        
        telemetry.track("share_accept", mapOf(
            "share_key_id" to shareKey.keyId,
            "user_id" to userId
        ))
        
        DkResult.success(acceptedKey)
    }
    
    // 撤销钥匙
    suspend fun revokeKey(keyId: String): DkResult<Unit> = withContext(Dispatchers.IO) {
        val key = keysCache[keyId] ?: return@withContext DkResult.failure(
            DkError(DkErrorCode.keyNotFound, "Key not found: $keyId")
        )
        
        val oldStatus = key.status
        val revokedKey = key.copy(
            status = KeyStatus.REVOKED,
            updatedAt = System.currentTimeMillis()
        )
        
        saveKeyToStorage(revokedKey)
        keysCache[keyId] = revokedKey
        
        telemetry.track("key_revoke", mapOf("key_id" to keyId))
        
        listeners.forEach { it.onKeyStatusChanged(keyId, oldStatus, KeyStatus.REVOKED) }
        
        DkResult.success(Unit)
    }
    
    // 挂起钥匙
    suspend fun suspendKey(keyId: String): DkResult<Unit> = withContext(Dispatchers.IO) {
        val key = keysCache[keyId] ?: return@withContext DkResult.failure(
            DkError(DkErrorCode.keyNotFound, "Key not found: $keyId")
        )
        
        val oldStatus = key.status
        val suspendedKey = key.copy(
            status = KeyStatus.SUSPENDED,
            updatedAt = System.currentTimeMillis()
        )
        
        saveKeyToStorage(suspendedKey)
        keysCache[keyId] = suspendedKey
        
        telemetry.track("key_suspend", mapOf("key_id" to keyId))
        
        listeners.forEach { it.onKeyStatusChanged(keyId, oldStatus, KeyStatus.SUSPENDED) }
        
        DkResult.success(Unit)
    }
    
    // 恢复钥匙
    suspend fun resumeKey(keyId: String): DkResult<Unit> = withContext(Dispatchers.IO) {
        val key = keysCache[keyId] ?: return@withContext DkResult.failure(
            DkError(DkErrorCode.keyNotFound, "Key not found: $keyId")
        )
        
        val oldStatus = key.status
        val resumedKey = key.copy(
            status = KeyStatus.ACTIVE,
            updatedAt = System.currentTimeMillis()
        )
        
        saveKeyToStorage(resumedKey)
        keysCache[keyId] = resumedKey
        
        telemetry.track("key_resume", mapOf("key_id" to keyId))
        
        listeners.forEach { it.onKeyStatusChanged(keyId, oldStatus, KeyStatus.ACTIVE) }
        
        DkResult.success(Unit)
    }
    
    // 获取下一个交易ID
    fun getNextTransactionId(): Long {
        return ++transactionCounter
    }
    
    // 释放资源
    fun release() {
        scope.cancel()
        listeners.clear()
    }
    
    // 私有方法
    
    private fun generateKeyId(): String {
        val bytes = ByteArray(16)
        secureRandom.nextBytes(bytes)
        return UUID.nameUUIDFromBytes(bytes).toString()
    }
    
    private fun generateShareCode(): String {
        val bytes = ByteArray(6)
        secureRandom.nextBytes(bytes)
        return Base64.getEncoder().encodeToString(bytes).take(8)
    }
    
    private fun generateKeyPair(alias: String) {
        val keyGen = KeyGenerator.getInstance(
            KeyProperties.KEY_ALGORITHM_AES,
            ANDROID_KEYSTORE
        )
        
        val spec = KeyGenParameterSpec.Builder(
            alias,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        )
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            .setRandomizedEncryptionRequired(true)
            .build()
        
        keyGen.init(spec)
        keyGen.generateKey()
    }
    
    private fun getSecretKey(alias: String): SecretKey? {
        return (keyStore.getEntry(alias, null) as? KeyStore.SecretKeyEntry)?.secretKey
    }
    
    private fun encrypt(data: ByteArray, key: SecretKey): ByteArray? {
        return try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, key)
            val iv = cipher.iv
            val encrypted = cipher.doFinal(data)
            iv + encrypted
        } catch (e: Exception) {
            logger.error("Encryption failed", e)
            null
        }
    }
    
    private fun decrypt(encryptedData: ByteArray, key: SecretKey): ByteArray? {
        return try {
            val iv = encryptedData.copyOfRange(0, 12)
            val data = encryptedData.copyOfRange(12, encryptedData.size)
            
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            val spec = GCMParameterSpec(AES_GCM_TAG_LENGTH, iv)
            cipher.init(Cipher.DECRYPT_MODE, key, spec)
            cipher.doFinal(data)
        } catch (e: Exception) {
            logger.error("Decryption failed", e)
            null
        }
    }
    
    private fun loadKeysFromStorage() {
        // Load keys from SharedPreferences
        val prefs = context.getSharedPreferences("digital_keys", Context.MODE_PRIVATE)
        val keysJson = prefs.getString("keys", null)
        
        if (keysJson != null) {
            try {
                val json = JSONObject(keysJson)
                json.keys().forEach { keyId ->
                    val keyData = json.getJSONObject(keyId)
                    val key = parseKeyFromJson(keyId, keyData)
                    keysCache[keyId] = key
                }
            } catch (e: Exception) {
                logger.error("Failed to load keys", e)
            }
        }
    }
    
    private fun saveKeyToStorage(key: DigitalKey) {
        val prefs = context.getSharedPreferences("digital_keys", Context.MODE_PRIVATE)
        val keysJson = prefs.getString("keys", null)
        val json = if (keysJson != null) JSONObject(keysJson) else JSONObject()
        
        json.put(key.keyId, keyToJson(key))
        prefs.edit().putString("keys", json.toString()).apply()
    }
    
    private fun removeKeyFromStorage(keyId: String) {
        val prefs = context.getSharedPreferences("digital_keys", Context.MODE_PRIVATE)
        val keysJson = prefs.getString("keys", null)
        
        if (keysJson != null) {
            try {
                val json = JSONObject(keysJson)
                json.remove(keyId)
                prefs.edit().putString("keys", json.toString()).apply()
            } catch (e: Exception) {
                logger.error("Failed to remove key", e)
            }
        }
    }
    
    private fun keyToJson(key: DigitalKey): JSONObject {
        return JSONObject().apply {
            put("vehicleId", key.vehicleId)
            put("userId", key.userId)
            put("keyType", key.keyType.value)
            put("status", key.status.value)
            put("validFrom", key.validFrom)
            put("validUntil", key.validUntil)
            put("maxUses", key.maxUses ?: JSONObject.NULL)
            put("usedCount", key.usedCount)
            put("shareCode", key.shareCode ?: JSONObject.NULL)
            put("issuerId", key.issuerId)
            put("vendor", key.vendor)
            put("protocolVersion", key.protocolVersion)
            put("createdAt", key.createdAt)
            put("updatedAt", key.updatedAt)
        }
    }
    
    private fun parseKeyFromJson(keyId: String, json: JSONObject): DigitalKey {
        return DigitalKey(
            keyId = keyId,
            vehicleId = json.getString("vehicleId"),
            userId = json.getString("userId"),
            keyType = KeyType.entries.find { it.value == json.getInt("keyType") } ?: KeyType.PRIMARY,
            status = KeyStatus.entries.find { it.value == json.getInt("status") } ?: KeyStatus.PENDING,
            validFrom = json.getLong("validFrom"),
            validUntil = json.getLong("validUntil"),
            maxUses = if (json.isNull("maxUses")) null else json.getInt("maxUses"),
            usedCount = json.getInt("usedCount"),
            shareCode = if (json.isNull("shareCode")) null else json.getString("shareCode"),
            issuerId = json.getString("issuerId"),
            vendor = json.getString("vendor"),
            protocolVersion = json.getString("protocolVersion"),
            createdAt = json.getLong("createdAt"),
            updatedAt = json.getLong("updatedAt")
        )
    }
    
    private fun Int.toInt(): Int = this
}

// 结果封装
sealed class DkResult<out T> {
    data class success<T>(val value: T) : DkResult<T>()
    data class failure(val error: DkError) : DkResult<Nothing>()
    
    fun isSuccess(): Boolean = this is success
    fun isFailure(): Boolean = this is failure
    
    fun getOrNull(): T? = (this as? success)?.value
    fun errorOrNull(): DkError? = (this as? failure)?.error
}
