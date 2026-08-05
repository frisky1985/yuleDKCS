// KeyStoreMetadataStore.kt
// Android KeyStore 回话钥匙元数据 AES 加密存储层
//
// 取代 EncryptedSharedPreferences：
// - AES-256/GCM 密钥由 Android KeyStore 硬件级生成并持久化
// - 密文（IV + ciphertext）经 Base64 编码后存入普通 SharedPreferences
// - 解密时从 KeyStore 取密钥
// - 自动迁移遗留 EncryptedSharedPreferences 数据（使用旧 API 解密后重加密）
//
// API 设计：KeyManager 无感替换，接口不变

package com.digitalkey.sdk.key

import android.content.Context
import android.content.SharedPreferences
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import com.digitalkey.sdk.logger.DkLogger
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * 基于 Android KeyStore 的元数据加密存储层。
 *
 * 钥匙元数据序列化为 JSON 后，用 KeyStore 中的 AES-256/GCM 密钥加密，
 * 密文写入普通 [SharedPreferences]。每次读取时从 KeyStore 解密。
 *
 * 运行时零 EncryptedSharedPreferences 依赖——仅迁移路径使用旧 API。
 *
 * == Biometric Enrollment Invalidation ==
 *
 * Android KeyStore 支持 `setInvalidatedByBiometricEnrollment()`，用于控制
 * 当用户添加/移除生物识别（指纹、面部等）时，是否自动失效已生成的密钥。
 *
 *   - `false`（默认）: 密钥不受生物识别注册变更影响。
 *     优点：用户更新生物识别后无需重新生成密钥，钥匙元数据始终可访问。
 *     缺点：设备失窃后，攻击者更新生物识别仍可访问旧密钥。
 *
 *   - `true`: 任何生物识别注册变更（添加/删除指纹或面部）都会使密钥失效。
 *     优点：提升安全性——生物识别变更视为安全事件，旧密钥自动废弃。
 *     缺点：用户每次修改生物识别后，应用必须重新保护钥匙元数据（可能需重新配车）。
 *
 * 对车辆数字钥匙场景的安全权衡：
 * - 数字钥匙元数据本身不包含私钥材料（私钥独立存储在 Android KeyStore 中），
 *   因此元数据的泄漏风险有限。
 * - 但元数据包含钥匙 ID、授权车辆、有效期等敏感信息，攻击者可能利用其进行重放攻击。
 * - 强烈建议对 **高安全等级车辆**（如共享汽车、企业车队）开启 true。
 * - 对 **消费级车辆**（家庭用车），false 提供更好的用户体验。
 *
 * @param context Android 上下文（Application 级别）
 * @param prefsName SharedPreferences 文件名（默认 "yuledkcs_keys"）
 * @param keyAlias  KeyStore 中 AES 密钥的别名（默认 "yuledkcs_metadata_key"）
 * @param invalidateOnBiometricEnrollment
 *   是否在生物识别注册变更时自动失效 AES 加密密钥。
 *   - `false`（默认）: 密钥持久，用户换指纹不丢失钥匙数据。
 *   - `true`: 更安全，任何生物识别变更后需重新加密钥匙元数据。
 *   具体安全权衡见上方说明。
 */
class KeyStoreMetadataStore(
    private val context: Context,
    private val prefsName: String = DEFAULT_PREFS_NAME,
    private val keyAlias: String = DEFAULT_KEY_ALIAS,
    private val invalidateOnBiometricEnrollment: Boolean = false
) {

    private val logger = DkLogger.getLogger("KeyStoreMetadataStore")
    private val prefs: SharedPreferences =
        context.getSharedPreferences(prefsName, Context.MODE_PRIVATE)

    companion object {
        private const val DEFAULT_PREFS_NAME = "yuledkcs_keys"
        private const val DEFAULT_KEY_ALIAS = "yuledkcs_metadata_key"
        private const val ANDROID_KEYSTORE = "AndroidKeyStore"
        private const val CIPHER_TRANSFORMATION = "AES/GCM/NoPadding"
        private const val GCM_TAG_LENGTH_BITS = 128
        private const val GCM_IV_LENGTH = 12

        // SharedPreferences 中密文的键名
        private const val SP_KEY_ENCRYPTED = "keys_encrypted"

        // 交易计数器持久化 — 同一 AES 密钥，独立 SharedPreferences 键
        private const val SP_KEY_COUNTER = "tx_counter_encrypted"

        // 迁移标志 — 旧 EncryptedSharedPreferences 数据已迁移
        private const val SP_KEY_MIGRATED = "migration_complete"
        private const val LEGACY_PREFS_NAME = "digital_keys_encrypted"
    }

    // ---------------------------------------------------------------
    // 公开 API
    // ---------------------------------------------------------------

    /**
     * 读取已存储的钥匙元数据 JSON。如果尚未存储或解密失败返回 null。
     */
    fun readMetadata(): String? {
        val base64 = prefs.getString(SP_KEY_ENCRYPTED, null) ?: return null
        return try {
            val decoded = Base64.decode(base64, Base64.NO_WRAP)
            val (iv, ciphertext) = extractIvAndCiphertext(decoded)
            val secretKey = getOrCreateSecretKey()
            decrypt(secretKey, iv, ciphertext)
        } catch (e: Exception) {
            logger.error("Failed to decrypt metadata from KeyStore-backed store", e)
            null
        }
    }

    /**
     * 写入钥匙元数据 JSON。内部加密后存入 SharedPreferences。
     */
    fun writeMetadata(plaintextJson: String) {
        try {
            val secretKey = getOrCreateSecretKey()
            val (iv, ciphertext) = encrypt(secretKey, plaintextJson.toByteArray(Charsets.UTF_8))
            val combined = iv + ciphertext
            val base64 = Base64.encodeToString(combined, Base64.NO_WRAP)
            prefs.edit().putString(SP_KEY_ENCRYPTED, base64).apply()
        } catch (e: Exception) {
            logger.error("Failed to encrypt metadata for KeyStore-backed store", e)
            throw e
        }
    }

    /**
     * 删除所有元数据（清空 SharedPreferences，包括计数器）。
     */
    fun clearMetadata() {
        prefs.edit().clear().apply()
    }

    /**
     * 读取持久化的交易计数器值。
     *
     * 使用同一 AES-256/GCM 密钥加密存储，与钥匙元数据共享相同的 KeyStore 密钥
     * 和 SharedPreferences 文件，但使用独立的键名（"tx_counter_encrypted"）。
     *
     * 向后兼容：如果从未存储过计数器值（老版本数据），返回 0。
     */
    fun readCounter(): Long {
        val base64 = prefs.getString(SP_KEY_COUNTER, null) ?: return 0L
        return try {
            val decoded = Base64.decode(base64, Base64.NO_WRAP)
            val (iv, ciphertext) = extractIvAndCiphertext(decoded)
            val secretKey = getOrCreateSecretKey()
            val plaintext = decrypt(secretKey, iv, ciphertext)
            plaintext.toLongOrNull() ?: 0L
        } catch (e: Exception) {
            logger.error("Failed to decrypt transaction counter from KeyStore-backed store", e)
            0L
        }
    }

    /**
     * 持久化交易计数器值。
     *
     * 加密后写入 SharedPreferences，键名为 "tx_counter_encrypted"。
     * 每次交易计数器递增后调用此方法写回。
     */
    fun writeCounter(counter: Long) {
        try {
            val plaintext = counter.toString()
            val secretKey = getOrCreateSecretKey()
            val (iv, ciphertext) = encrypt(secretKey, plaintext.toByteArray(Charsets.UTF_8))
            val combined = iv + ciphertext
            val base64 = Base64.encodeToString(combined, Base64.NO_WRAP)
            prefs.edit().putString(SP_KEY_COUNTER, base64).apply()
        } catch (e: Exception) {
            logger.error("Failed to encrypt transaction counter for KeyStore-backed store", e)
            throw e
        }
    }

    /**
     * 删除持久化的交易计数器（清空对应 SharedPreferences 键）。
     */
    fun clearCounter() {
        prefs.edit().remove(SP_KEY_COUNTER).apply()
    }

    /**
     * 迁移遗留计数器值（如有）。
     *
     * 当前无遗留计数器格式，此方法为将来预留。
     */
    fun migrateCounterFromLegacyIfNeeded() {
        // 无遗留计数器需要迁移
    }

    /**
     * 从遗留 EncryptedSharedPreferences 迁移数据到新 KeyStore 回话存储。
     *
     * 仅在新存储为空且旧存储有数据时执行一次。迁移完成后清理旧文件。
     * 运行时路径不透支 EncryptedSharedPreferences——仅此方法引用旧 API。
     */
    fun migrateFromLegacyIfNeeded() {
        if (prefs.getBoolean(SP_KEY_MIGRATED, false)) {
            return // 已经迁移过
        }
        if (prefs.contains(SP_KEY_ENCRYPTED)) {
            // 新存储已有数据，无需迁移，标记即可
            prefs.edit().putBoolean(SP_KEY_MIGRATED, true).apply()
            return
        }

        // 使用 EncryptedSharedPreferences 读取遗留数据（需要旧 MasterKey）
        val legacyData = try {
            val masterKey = MasterKey.Builder(context)
                .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                .build()
            val legacyPrefs = EncryptedSharedPreferences.create(
                context,
                LEGACY_PREFS_NAME,
                masterKey,
                EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
            )
            legacyPrefs.getString("keys", null)
        } catch (e: Exception) {
            logger.warn("Legacy EncryptedSharedPreferences not accessible (first run or already cleaned up): ${e.message}")
            null
        }

        if (legacyData == null) {
            // 旧存储也没有数据，直接标记完成
            prefs.edit().putBoolean(SP_KEY_MIGRATED, true).apply()
            return
        }

        logger.info("Migrating key metadata from EncryptedSharedPreferences to KeyStore-backed store")
        try {
            writeMetadata(legacyData)
            prefs.edit().putBoolean(SP_KEY_MIGRATED, true).apply()
            // 清理遗留 SharedPreferences 文件（释放空间）
            context.getSharedPreferences(LEGACY_PREFS_NAME, Context.MODE_PRIVATE)
                .edit().clear().apply()
            logger.info("Key metadata migration completed successfully")
        } catch (e: Exception) {
            logger.error("Key metadata migration failed", e)
            // 不标记 migrated，下次初始化会重试
        }
    }

    // ---------------------------------------------------------------
    // KeyStore 内部方法
    // ---------------------------------------------------------------

    /**
     * 获取或生成 AES-256/GCM 密钥，硬件级持久化于 Android KeyStore。
     * 密钥仅在首次调用时生成，后续从 KeyStore 加载。
     */
    private fun getOrCreateSecretKey(): SecretKey {
        val keyStore = KeyStore.getInstance(ANDROID_KEYSTORE).apply { load(null) }

        if (keyStore.containsAlias(keyAlias)) {
            return (keyStore.getEntry(keyAlias, null) as KeyStore.SecretKeyEntry).secretKey
        }

        // 生成新密钥 —— 硬件 backed，密钥材料永不离开安全环境
        val keyGenerator = KeyGenerator.getInstance(
            KeyProperties.KEY_ALGORITHM_AES,
            ANDROID_KEYSTORE
        )
        val spec = KeyGenParameterSpec.Builder(
            keyAlias,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        )
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            // 生物识别注册变更时是否使密钥失效
            //   false = 密钥持久（默认，用户体验优先）
            //   true  = 更安全，生物识别变更后需重新加密
            .setInvalidatedByBiometricEnrollment(invalidateOnBiometricEnrollment)
            .build()

        keyGenerator.init(spec)
        return keyGenerator.generateKey()
    }

    /**
     * 使用 AES/GCM 加密明文，返回 (IV, ciphertext)。
     */
    private fun encrypt(secretKey: SecretKey, plaintext: ByteArray): Pair<ByteArray, ByteArray> {
        val cipher = Cipher.getInstance(CIPHER_TRANSFORMATION)
        cipher.init(Cipher.ENCRYPT_MODE, secretKey)
        val ciphertext = cipher.doFinal(plaintext)
        val iv = cipher.iv
        return Pair(iv, ciphertext)
    }

    /**
     * 使用 AES/GCM 解密，IV 和 ciphertext 分开传入。
     */
    private fun decrypt(secretKey: SecretKey, iv: ByteArray, ciphertext: ByteArray): String {
        val cipher = Cipher.getInstance(CIPHER_TRANSFORMATION)
        val spec = GCMParameterSpec(GCM_TAG_LENGTH_BITS, iv)
        cipher.init(Cipher.DECRYPT_MODE, secretKey, spec)
        val plaintext = cipher.doFinal(ciphertext)
        return String(plaintext, Charsets.UTF_8)
    }

    /**
     * 从拼接字节数组中分离 IV 和密文。
     * 存储格式：[IV (12 bytes)][ciphertext (rest)]
     */
    private fun extractIvAndCiphertext(combined: ByteArray): Pair<ByteArray, ByteArray> {
        val iv = combined.copyOfRange(0, GCM_IV_LENGTH)
        val ciphertext = combined.copyOfRange(GCM_IV_LENGTH, combined.size)
        return Pair(iv, ciphertext)
    }
}
