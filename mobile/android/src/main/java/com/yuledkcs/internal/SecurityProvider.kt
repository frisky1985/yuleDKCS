package com.yuledkcs.internal

import android.content.Context
import android.content.SharedPreferences
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import com.yuledkcs.CryptoWrapper

/**
 * Internal security provider for secure data storage
 * 
 * Uses AndroidX Security library for encrypted preferences.
 */
internal class SecurityProvider(context: Context) {
    
    companion object {
        private const val PREFS_FILE = "yuledkcs_secure_prefs"
        private const val MASTER_KEY_ALIAS = "yuledkcs_master_key"
    }
    
    private val masterKey: MasterKey = MasterKey.Builder(context, MASTER_KEY_ALIAS)
        .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
        .build()
    
    private val encryptedPrefs: EncryptedSharedPreferences = EncryptedSharedPreferences.create(
        context,
        PREFS_FILE,
        masterKey,
        EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
        EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
    ) as EncryptedSharedPreferences
    
    /**
     * Store data securely
     */
    fun storeSecureData(key: String, data: ByteArray) {
        val encrypted = CryptoWrapper.encrypt(data, MASTER_KEY_ALIAS)
        encryptedPrefs.edit().putString(key, encrypted).apply()
    }
    
    /**
     * Retrieve secure data
     */
    fun getSecureData(key: String): ByteArray? {
        val encrypted = encryptedPrefs.getString(key, null) ?: return null
        return try {
            CryptoWrapper.decrypt(encrypted, MASTER_KEY_ALIAS)
        } catch (e: Exception) {
            null
        }
    }
    
    /**
     * Delete secure data
     */
    fun deleteSecureData(key: String) {
        encryptedPrefs.edit().remove(key).apply()
    }
    
    /**
     * Store string securely
     */
    fun storeSecureString(key: String, value: String) {
        val encrypted = CryptoWrapper.encryptString(value, MASTER_KEY_ALIAS)
        encryptedPrefs.edit().putString(key, encrypted).apply()
    }
    
    /**
     * Retrieve secure string
     */
    fun getSecureString(key: String): String? {
        val encrypted = encryptedPrefs.getString(key, null) ?: return null
        return try {
            CryptoWrapper.decryptToString(encrypted, MASTER_KEY_ALIAS)
        } catch (e: Exception) {
            null
        }
    }
    
    /**
     * Clear all stored data
     */
    fun clearAll() {
        encryptedPrefs.edit().clear().apply()
    }
}
