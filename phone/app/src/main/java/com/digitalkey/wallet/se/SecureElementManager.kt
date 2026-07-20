package com.digitalkey.wallet.se

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Log
import java.security.*
import java.security.spec.ECGenParameterSpec

/**
 * 安全元件(SE)管理器
 * 使用Android Keystore / StrongBox存储密钥
 * 密钥不出设备，所有签名操作在SE内完成
 */
class SecureElementManager(private val context: Context) {

    companion object {
        private const val TAG = "SEManager"
        private const val ANDROID_KEYSTORE = "AndroidKeyStore"
        private const val KEY_ALIAS_PREFIX = "dk_key_"
        private const val CURVE = "secp256r1"
    }

    /**
     * 生成密钥对 (存储在SE/StrongBox中)
     * 用于数字钥匙绑定阶段
     */
    fun generateKeyPair(keyId: String): KeyPair? {
        return try {
            val keyStore = KeyStore.getInstance(ANDROID_KEYSTORE)
            keyStore.load(null)

            val alias = KEY_ALIAS_PREFIX + keyId
            val keyPairGenerator = KeyPairGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_EC,
                ANDROID_KEYSTORE
            )

            val spec = KeyGenParameterSpec.Builder(
                alias,
                KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_AGREE_KEY
            )
                .setAlgorithmParameterSpec(ECGenParameterSpec(CURVE))
                .setDigests(KeyProperties.DIGEST_SHA256)
                .setUserAuthenticationRequired(false)  // 数字钥匙不需要用户认证
                .setIsStrongBoxBacked(true)  // 优先使用StrongBox
                .build()

            keyPairGenerator.initialize(spec)
            val keyPair = keyPairGenerator.generateKeyPair()

            Log.i(TAG, "Key pair generated: alias=$alias curve=$CURVE")
            keyPair
        } catch (e: Exception) {
            Log.e(TAG, "Failed to generate key pair", e)
            // Fallback: try without StrongBox
            generateKeyPairFallback(keyId)
        }
    }

    /**
     * Fallback: 不使用StrongBox
     */
    private fun generateKeyPairFallback(keyId: String): KeyPair? {
        return try {
            val alias = KEY_ALIAS_PREFIX + keyId
            val keyPairGenerator = KeyPairGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_EC,
                ANDROID_KEYSTORE
            )

            val spec = KeyGenParameterSpec.Builder(
                alias,
                KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_AGREE_KEY
            )
                .setAlgorithmParameterSpec(ECGenParameterSpec(CURVE))
                .setDigests(KeyProperties.DIGEST_SHA256)
                .setUserAuthenticationRequired(false)
                .build()

            keyPairGenerator.initialize(spec)
            keyPairGenerator.generateKeyPair()
        } catch (e: Exception) {
            Log.e(TAG, "Fallback key generation also failed", e)
            null
        }
    }

    /**
     * ECDSA签名 (在SE内执行，私钥不离开SE)
     */
    fun sign(keyId: String, data: ByteArray): ByteArray? {
        return try {
            val keyStore = KeyStore.getInstance(ANDROID_KEYSTORE)
            keyStore.load(null)

            val alias = KEY_ALIAS_PREFIX + keyId
            val entry = keyStore.getEntry(alias, null) as? KeyStore.PrivateKeyEntry
                ?: return null

            val signature = Signature.getInstance("SHA256withECDSA")
            signature.initSign(entry.privateKey)
            signature.update(data)
            signature.sign()
        } catch (e: Exception) {
            Log.e(TAG, "Sign failed", e)
            null
        }
    }

    /**
     * ECDH密钥协商 (在SE内执行)
     */
    fun agreeSharedSecret(keyId: String, vehiclePublicKey: PublicKey): ByteArray? {
        return try {
            val keyStore = KeyStore.getInstance(ANDROID_KEYSTORE)
            keyStore.load(null)

            val alias = KEY_ALIAS_PREFIX + keyId
            val entry = keyStore.getEntry(alias, null) as? KeyStore.PrivateKeyEntry
                ?: return null

            // Use KeyAgreement for ECDH
            val keyAgreement = KeyAgreement.getInstance("ECDH")
            keyAgreement.init(entry.privateKey)
            keyAgreement.doPhase(vehiclePublicKey, true)
            keyAgreement.generateSecret()
        } catch (e: Exception) {
            Log.e(TAG, "ECDH failed", e)
            null
        }
    }

    /**
     * 获取公钥
     */
    fun getPublicKey(keyId: String): ByteArray? {
        return try {
            val keyStore = KeyStore.getInstance(ANDROID_KEYSTORE)
            keyStore.load(null)

            val alias = KEY_ALIAS_PREFIX + keyId
            val entry = keyStore.getEntry(alias, null) as? KeyStore.PrivateKeyEntry
                ?: return null

            entry.certificate.publicKey.encoded
        } catch (e: Exception) {
            null
        }
    }

    /**
     * 删除密钥 (解绑时调用)
     */
    fun deleteKey(keyId: String): Boolean {
        return try {
            val keyStore = KeyStore.getInstance(ANDROID_KEYSTORE)
            keyStore.load(null)

            val alias = KEY_ALIAS_PREFIX + keyId
            keyStore.deleteEntry(alias)
            Log.i(TAG, "Key deleted: $alias")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Delete key failed", e)
            false
        }
    }
}
