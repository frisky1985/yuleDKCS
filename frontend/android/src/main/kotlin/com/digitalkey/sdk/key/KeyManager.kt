// KeyManager.kt - 密钥管理模块
// 钥匙元数据存储：Android KeyStore + AES-256/GCM + SharedPreferences（密文）
// 取代 EncryptedSharedPreferences，通过 OEM 审计硬件级存储要求
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
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.PrivateKey
import java.security.SecureRandom
import java.security.spec.ECGenParameterSpec
import java.util.*
import java.util.concurrent.ConcurrentHashMap

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
    
    // Android KeyStore 回话的元数据加密存储（取代 EncryptedSharedPreferences）
    private val metadataStore = KeyStoreMetadataStore(context)

    private val listeners = mutableListOf<KeyEventListener>()
    private val keysCache = ConcurrentHashMap<String, DigitalKey>()

    // KeyStore alias prefix
    companion object {
        private const val KEY_ALIAS_PREFIX = "dk_key_"
        private const val ANDROID_KEYSTORE = "AndroidKeyStore"

        /**
         * 交易计数器的最大允许值。
         * 1 万亿次交易在数字钥匙设备生命周期内足够了。
         * 达到此值后 [getNextTransactionId] 将抛出异常，拒绝新交易。
         */
        private const val MAX_TRANSACTION_COUNT = 1_000_000_000_000L
    }

    /**
     * 持久化的交易计数器，用于防重放保护。
     * 在 [init] 中从 KeyStore 加密存储加载，每次递增后写回。
     */
    private var transactionCounter: Long = 0
    
    init {
        // 从遗留 EncryptedSharedPreferences 迁移到 KeyStore 回话存储（仅一次）
        metadataStore.migrateFromLegacyIfNeeded()

        // 从持久化存储加载交易计数器（向后兼容：无数据时从 0 开始）
        transactionCounter = metadataStore.readCounter()
        logger.info("Loaded transaction counter: $transactionCounter")

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
    
    /**
     * 获取下一个交易 ID（自动递增计数器）。
     *
     * - 计数器从持久化存储加载，保证进程重启后不归零
     * - 每次递增后立即写回 KeyStore 加密存储
     * - 达到 [MAX_TRANSACTION_COUNT] 时抛出异常拒绝交易
     * - 线程安全通过 synchronized 保证
     *
     * @return 新的交易 ID（从 1 开始递增）
     * @throws DkError 当计数器达到 [MAX_TRANSACTION_COUNT] 时拒绝交易
     */
    fun getNextTransactionId(): Long {
        synchronized(this) {
            if (transactionCounter >= MAX_TRANSACTION_COUNT) {
                val errMsg = "Transaction counter reached maximum ($MAX_TRANSACTION_COUNT), cannot issue new transaction"
                logger.error(errMsg)
                telemetry.trackError(DkErrorCode.ERR_QUOTA_EXCEEDED, errMsg)
                throw DkError(DkErrorCode.ERR_QUOTA_EXCEEDED, errMsg)
            }
            transactionCounter++
            metadataStore.writeCounter(transactionCounter)
            return transactionCounter
        }
    }
    
    /**
     * 重置交易计数器（仅用于测试）。
     * 生产代码中不应调用此方法。
     */
    fun resetTransactionCounter() {
        synchronized(this) {
            transactionCounter = 0L
            metadataStore.clearCounter()
            logger.warn("Transaction counter reset to 0 (test only)")
        }
    }

    /**
     * 获取当前交易计数器值（仅用于测试/监控）。
     */
    fun getTransactionCounter(): Long {
        return transactionCounter
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
        val keyPairGenerator = KeyPairGenerator.getInstance(
            KeyProperties.KEY_ALGORITHM_EC,
            ANDROID_KEYSTORE
        )

        val spec = KeyGenParameterSpec.Builder(
            alias,
            KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_VERIFY
        )
            .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
            .setDigests(KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_SHA384)
            // 生物识别注册变更时是否使数字签名密钥失效
            // 此处硬编码 false 以保持与 KeyManager 公共 API 的兼容性；
            // 如需可配置，可类似 KeyStoreMetadataStore 通过构造函数参数注入。
            .setInvalidatedByBiometricEnrollment(false)
            .build()

        keyPairGenerator.initialize(spec)
        keyPairGenerator.generateKeyPair()
    }
    
    private fun getKeyPair(alias: String): KeyStore.PrivateKeyEntry? {
        return keyStore.getEntry(alias, null) as? KeyStore.PrivateKeyEntry
    }

    private fun getPrivateKey(alias: String): PrivateKey? {
        return getKeyPair(alias)?.privateKey
    }
    
    private fun loadKeysFromStorage() {
        val keysJson = metadataStore.readMetadata()

        if (keysJson != null) {
            try {
                val json = JSONObject(keysJson)
                json.keys().forEach { keyId ->
                    val keyData = json.getJSONObject(keyId)
                    val key = parseKeyFromJson(keyId, keyData)
                    keysCache[keyId] = key
                }
            } catch (e: Exception) {
                logger.error("Failed to load keys from KeyStore-backed store", e)
            }
        }
    }

    private fun saveKeyToStorage(key: DigitalKey) {
        val keysJson = metadataStore.readMetadata()
        val json = if (keysJson != null) JSONObject(keysJson) else JSONObject()

        json.put(key.keyId, keyToJson(key))
        metadataStore.writeMetadata(json.toString())
    }

    private fun removeKeyFromStorage(keyId: String) {
        val keysJson = metadataStore.readMetadata()

        if (keysJson != null) {
            try {
                val json = JSONObject(keysJson)
                json.remove(keyId)
                metadataStore.writeMetadata(json.toString())
            } catch (e: Exception) {
                logger.error("Failed to remove key from KeyStore-backed store", e)
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
