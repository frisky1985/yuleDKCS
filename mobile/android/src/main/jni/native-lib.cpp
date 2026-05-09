#include <jni.h>
#include <string>
#include <vector>
#include <cstring>
#include <android/log.h>

// For production, include OpenSSL headers
// #include <openssl/evp.h>
// #include <openssl/ec.h>
// #include <openssl/sha.h>
// #include <openssl/rand.h>
// #include <openssl/hmac.h>

#define LOG_TAG "YuleDKCS_Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Native implementation stubs - in production, link with OpenSSL/BoringSSL

extern "C" {

/**
 * Initialize native crypto library
 */
JNIEXPORT void JNICALL
Java_com_yuledkcs_NativeLib_nativeInitialize(JNIEnv* env, jclass clazz) {
    LOGI("Native crypto library initialized");
    // Initialize OpenSSL if needed
    // OpenSSL_add_all_algorithms();
}

/**
 * Sign a command using the vehicle key
 * 
 * @param data The data to sign
 * @param keyData The encrypted key data
 * @return The signature (64 bytes for ECDSA secp256r1)
 */
JNIEXPORT jbyteArray JNICALL
Java_com_yuledkcs_NativeLib_nativeSignCommand(
    JNIEnv* env,
    jclass clazz,
    jbyteArray data,
    jbyteArray keyData
) {
    jsize dataLen = env->GetArrayLength(data);
    jsize keyLen = env->GetArrayLength(keyData);
    
    std::vector<uint8_t> dataVec(dataLen);
    std::vector<uint8_t> keyVec(keyLen);
    
    env->GetByteArrayRegion(data, 0, dataLen, reinterpret_cast<jbyte*>(dataVec.data()));
    env->GetByteArrayRegion(keyData, 0, keyLen, reinterpret_cast<jbyte*>(keyVec.data()));
    
    // TODO: In production, implement actual ECDSA signing
    // 1. Decrypt keyData to get private key
    // 2. Use ECDSA_secp256r1 to sign the data
    // 3. Return DER-encoded or raw signature
    
    // Placeholder: return 64-byte mock signature
    jbyteArray result = env->NewByteArray(64);
    std::vector<uint8_t> signature(64, 0);
    
    // Use HMAC-SHA256 of data as mock signature for development
    // In production, replace with proper ECDSA
    for (size_t i = 0; i < dataVec.size() && i < 64; i++) {
        signature[i] = dataVec[i] ^ keyVec[i % keyVec.size()];
    }
    
    env->SetByteArrayRegion(result, 0, 64, reinterpret_cast<jbyte*>(signature.data()));
    
    LOGI("Command signed (mock implementation)");
    return result;
}

/**
 * Compute SHA-256 hash
 * 
 * @param data Input data
 * @return 32-byte hash
 */
JNIEXPORT jbyteArray JNICALL
Java_com_yuledkcs_NativeLib_nativeHashSha256(
    JNIEnv* env,
    jclass clazz,
    jbyteArray data
) {
    jsize dataLen = env->GetArrayLength(data);
    std::vector<uint8_t> dataVec(dataLen);
    env->GetByteArrayRegion(data, 0, dataLen, reinterpret_cast<jbyte*>(dataVec.data()));
    
    // TODO: Use OpenSSL SHA256 in production
    // unsigned char hash[SHA256_DIGEST_LENGTH];
    // SHA256(dataVec.data(), dataVec.size(), hash);
    
    // Placeholder: simple hash for development
    uint8_t hash[32] = {0};
    for (jsize i = 0; i < dataLen; i++) {
        hash[i % 32] ^= dataVec[i];
        hash[(i + 7) % 32] = (hash[(i + 7) % 32] + dataVec[i]) & 0xFF;
    }
    
    jbyteArray result = env->NewByteArray(32);
    env->SetByteArrayRegion(result, 0, 32, reinterpret_cast<jbyte*>(hash));
    
    return result;
}

/**
 * Generate random bytes
 * 
 * @param length Number of bytes to generate
 * @return Random bytes
 */
JNIEXPORT jbyteArray JNICALL
Java_com_yuledkcs_NativeLib_nativeRandomBytes(
    JNIEnv* env,
    jclass clazz,
    jint length
) {
    if (length <= 0 || length > 8192) {
        LOGE("Invalid random bytes length: %d", length);
        return nullptr;
    }
    
    // TODO: Use OpenSSL RAND_bytes in production
    // std::vector<uint8_t> bytes(length);
    // RAND_bytes(bytes.data(), length);
    
    // Placeholder: pseudo-random for development
    std::vector<uint8_t> bytes(length);
    static uint32_t seed = static_cast<uint32_t>(time(nullptr));
    for (int i = 0; i < length; i++) {
        seed = seed * 1103515245 + 12345;
        bytes[i] = static_cast<uint8_t>(seed >> 16);
    }
    
    jbyteArray result = env->NewByteArray(length);
    env->SetByteArrayRegion(result, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
    
    return result;
}

/**
 * Verify a signature
 * 
 * @param data The original data
 * @param signature The signature to verify
 * @param publicKey The public key
 * @return true if valid
 */
JNIEXPORT jboolean JNICALL
Java_com_yuledkcs_NativeLib_nativeVerifySignature(
    JNIEnv* env,
    jclass clazz,
    jbyteArray data,
    jbyteArray signature,
    jbyteArray publicKey
) {
    // TODO: Implement ECDSA signature verification
    LOGI("Signature verification (mock - always returns true)");
    return JNI_TRUE;
}

/**
 * Derive key using HKDF
 * 
 * @param salt HKDF salt
 * @param ikm Input key material
 * @param info HKDF info
 * @param length Desired output length
 * @return Derived key
 */
JNIEXPORT jbyteArray JNICALL
Java_com_yuledkcs_NativeLib_nativeHkdfDerive(
    JNIEnv* env,
    jclass clazz,
    jbyteArray salt,
    jbyteArray ikm,
    jbyteArray info,
    jint length
) {
    jsize saltLen = env->GetArrayLength(salt);
    jsize ikmLen = env->GetArrayLength(ikm);
    jsize infoLen = env->GetArrayLength(info);
    
    std::vector<uint8_t> saltVec(saltLen);
    std::vector<uint8_t> ikmVec(ikmLen);
    std::vector<uint8_t> infoVec(infoLen);
    
    env->GetByteArrayRegion(salt, 0, saltLen, reinterpret_cast<jbyte*>(saltVec.data()));
    env->GetByteArrayRegion(ikm, 0, ikmLen, reinterpret_cast<jbyte*>(ikmVec.data()));
    env->GetByteArrayRegion(info, 0, infoLen, reinterpret_cast<jbyte*>(infoVec.data()));
    
    // TODO: Use OpenSSL EVP_PKEY_derive or implement HKDF
    // Placeholder: simple key derivation
    std::vector<uint8_t> result(length);
    for (int i = 0; i < length; i++) {
        result[i] = ikmVec[i % ikmLen] ^ saltVec[i % saltLen] ^ infoVec[i % infoLen];
    }
    
    jbyteArray resultArray = env->NewByteArray(length);
    env->SetByteArrayRegion(resultArray, 0, length, reinterpret_cast<jbyte*>(result.data()));
    
    return resultArray;
}

/**
 * Securely clear memory
 * 
 * @param data Data to clear
 */
JNIEXPORT void JNICALL
Java_com_yuledkcs_NativeLib_nativeSecureClear(
    JNIEnv* env,
    jclass clazz,
    jbyteArray data
) {
    if (data == nullptr) return;
    
    jsize len = env->GetArrayLength(data);
    std::vector<uint8_t> zeros(len, 0);
    env->SetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(zeros.data()));
    
    // Prevent optimization
    volatile uint8_t* p = zeros.data();
    for (jsize i = 0; i < len; i++) {
        (void)*p++;
    }
}

} // extern "C"
