package com.yuledkcs

/**
 * Native Library wrapper for JNI calls
 * 
 * Bridges Kotlin code to native C++ crypto implementation.
 */
internal object NativeLib {
    
    init {
        System.loadLibrary("native-lib")
    }
    
    /**
     * Initialize native crypto library
     */
    @JvmStatic
    external fun nativeInitialize()
    
    /**
     * Sign a command using the vehicle key
     */
    @JvmStatic
    external fun nativeSignCommand(data: ByteArray, keyData: ByteArray): ByteArray
    
    /**
     * Compute SHA-256 hash
     */
    @JvmStatic
    external fun nativeHashSha256(data: ByteArray): ByteArray
    
    /**
     * Generate random bytes
     */
    @JvmStatic
    external fun nativeRandomBytes(length: Int): ByteArray
    
    /**
     * Verify a signature
     */
    @JvmStatic
    external fun nativeVerifySignature(
        data: ByteArray,
        signature: ByteArray,
        publicKey: ByteArray
    ): Boolean
    
    /**
     * Derive key using HKDF
     */
    @JvmStatic
    external fun nativeHkdfDerive(
        salt: ByteArray,
        ikm: ByteArray,
        info: ByteArray,
        length: Int
    ): ByteArray
    
    /**
     * Securely clear memory
     */
    @JvmStatic
    external fun nativeSecureClear(data: ByteArray)
    
    // Convenience wrappers
    
    fun initialize() {
        nativeInitialize()
    }
    
    fun signCommand(data: ByteArray, keyData: ByteArray): ByteArray {
        return nativeSignCommand(data, keyData)
    }
    
    fun hashSha256(data: ByteArray): ByteArray {
        return nativeHashSha256(data)
    }
    
    fun randomBytes(length: Int): ByteArray {
        return nativeRandomBytes(length)
    }
    
    fun verifySignature(data: ByteArray, signature: ByteArray, publicKey: ByteArray): Boolean {
        return nativeVerifySignature(data, signature, publicKey)
    }
    
    fun hkdfDerive(salt: ByteArray, ikm: ByteArray, info: ByteArray, length: Int): ByteArray {
        return nativeHkdfDerive(salt, ikm, info, length)
    }
    
    fun secureClear(data: ByteArray) {
        nativeSecureClear(data)
    }
}
