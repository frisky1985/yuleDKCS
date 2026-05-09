package com.yuledkcs

import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * CryptoWrapper - High-level cryptographic operations
 * 
 * Wraps platform-specific crypto with secure key management using Android Keystore.
 */
object CryptoWrapper {
    
    private const val ANDROID_KEYSTORE = "AndroidKeyStore"
    private const val ALGORITHM = KeyProperties.KEY_ALGORITHM_AES
    private const val BLOCK_MODE = KeyProperties.BLOCK_MODE_GCM
    private const val PADDING = KeyProperties.ENCRYPTION_PADDING_NONE
    private const val KEY_SIZE = 256
    private const val GCM_TAG_LENGTH = 128
    
    private val keyStore = KeyStore.getInstance(ANDROID_KEYSTORE).apply { load(null) }
    
    /**
     * Generate or retrieve a key from Android Keystore
     */
    fun getOrCreateKey(alias: String): SecretKey {
        return keyStore.getEntry(alias, null) as? KeyStore.SecretKeyEntry?.let {
            it.secretKey
        } ?: generateKey(alias)
    }
    
    /**
     * Encrypt data using AES-GCM
     * 
     * @param plaintext Data to encrypt
     * @param keyAlias Keystore alias for the encryption key
     * @return Base64 encoded encrypted data with IV prepended
     */
    fun encrypt(plaintext: ByteArray, keyAlias: String): String {
        val key = getOrCreateKey(keyAlias)
        
        val cipher = Cipher.getInstance("$ALGORITHM/$BLOCK_MODE/$PADDING")
        cipher.init(Cipher.ENCRYPT_MODE, key)
        
        val iv = cipher.iv
        val ciphertext = cipher.doFinal(plaintext)
        
        // Combine IV + ciphertext
        val combined = ByteArray(iv.size + ciphertext.size)
        System.arraycopy(iv, 0, combined, 0, iv.size)
        System.arraycopy(ciphertext, 0, combined, iv.size, ciphertext.size)
        
        return Base64.encodeToString(combined, Base64.NO_WRAP)
    }
    
    /**
     * Decrypt data using AES-GCM
     * 
     * @param encryptedData Base64 encoded encrypted data with IV prepended
     * @param keyAlias Keystore alias for the decryption key
     * @return Decrypted plaintext
     */
    fun decrypt(encryptedData: String, keyAlias: String): ByteArray {
        val combined = Base64.decode(encryptedData, Base64.NO_WRAP)
        
        // Extract IV (first 12 bytes for GCM)
        val iv = combined.copyOfRange(0, 12)
        val ciphertext = combined.copyOfRange(12, combined.size)
        
        val key = getOrCreateKey(keyAlias)
        
        val cipher = Cipher.getInstance("$ALGORITHM/$BLOCK_MODE/$PADDING")
        cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(GCM_TAG_LENGTH, iv))
        
        return cipher.doFinal(ciphertext)
    }
    
    /**
     * Encrypt a string
     */
    fun encryptString(plaintext: String, keyAlias: String): String {
        return encrypt(plaintext.toByteArray(Charsets.UTF_8), keyAlias)
    }
    
    /**
     * Decrypt to string
     */
    fun decryptToString(encryptedData: String, keyAlias: String): String {
        return String(decrypt(encryptedData, keyAlias), Charsets.UTF_8)
    }
    
    /**
     * Hash data using SHA-256 (via native lib for performance)
     */
    fun hash(data: ByteArray): ByteArray {
        return NativeLib.hashSha256(data)
    }
    
    /**
     * Generate random bytes
     */
    fun randomBytes(length: Int): ByteArray {
        return NativeLib.randomBytes(length)
    }
    
    /**
     * Delete a key from Keystore
     */
    fun deleteKey(alias: String) {
        if (keyStore.containsAlias(alias)) {
            keyStore.deleteEntry(alias)
        }
    }
    
    /**
     * Check if key exists
     */
    fun hasKey(alias: String): Boolean {
        return keyStore.containsAlias(alias)
    }
    
    private fun generateKey(alias: String): SecretKey {
        val keyGenerator = KeyGenerator.getInstance(ALGORITHM, ANDROID_KEYSTORE)
        
        val keyGenParameterSpec = KeyGenParameterSpec.Builder(
            alias,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        )
            .setBlockModes(BLOCK_MODE)
            .setEncryptionPaddings(PADDING)
            .setKeySize(KEY_SIZE)
            .setRandomizedEncryptionRequired(true)
            .build()
        
        keyGenerator.init(keyGenParameterSpec)
        return keyGenerator.generateKey()
    }
}
