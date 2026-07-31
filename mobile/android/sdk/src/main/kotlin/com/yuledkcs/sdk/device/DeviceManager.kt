package com.yuledkcs.sdk.device

import android.content.Context
import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.interfaces.ECPublicKey
import java.util.UUID

// ─── 手机厂商枚举（与 sdk.proto PhoneVendor 对齐） ──────────

enum class PhoneVendor(val protoValue: Int) {
    APPLE(1),
    SAMSUNG(2),
    XIAOMI(3),
    OPPO(4),
    VIVO(5),
    HUAWEI(6)
}

// ─── 协议类型（与 sdk.proto Protocol 对齐） ────────────────

enum class DigitalKeyProtocol(val protoValue: Int) {
    CCC_DK3(1),
    ICCOA_DK40(2),
    ICCE(3)
}

// ─── DeviceManager ─────────────────────────────────────────

/**
 * 设备信息管理器
 *
 * 职责:
 * 1. 生成/读取 device_id（首次生成后持久化到 SharedPreferences）
 * 2. 从 Android Keystore 读取 ECC P-256 公钥
 * 3. 检测手机厂商 (vendor)
 * 4. 检测数字钥匙协议 (protocol)
 *
 * bindKey / acceptShare 时由 HubClient 自动调用填充请求字段。
 */
object DeviceManager {

    private const val KEYSTORE = "AndroidKeyStore"
    private const val KEY_ALIAS = "yuledkcs_device_key"
    private const val PREFS = "yuledkcs_device"
    private const val DEVICE_ID_KEY = "device_id"

    private lateinit var context: Context

    /** SDK 初始化时必须调用 */
    fun init(appContext: Context) {
        context = appContext.applicationContext
    }

    private fun prefs() = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    // ─── Device ID ─────────────────────────────────────────

    /** 获取设备 ID（首次调用生成 UUID 并持久化） */
    fun getDeviceId(): String {
        val existing = prefs().getString(DEVICE_ID_KEY, null)
        if (existing != null) return existing
        val newId = UUID.randomUUID().toString()
        prefs().edit().putString(DEVICE_ID_KEY, newId).apply()
        return newId
    }

    // ─── Keystore 公钥 ─────────────────────────────────────

    /**
     * 从 Android Keystore 读取 ECC P-256 公钥（首次调用生成）
     *
     * 返回 X.509 SubjectPublicKeyInfo 编码的公钥字节。
     */
    @Throws(Exception::class)
    fun readPublicKey(): ByteArray {
        val ks = KeyStore.getInstance(KEYSTORE).apply { load(null) }
        val entry = ks.getEntry(KEY_ALIAS, null)
        if (entry is KeyStore.PrivateKeyEntry) {
            val pub = entry.certificate.publicKey as ECPublicKey
            return pub.encoded
        }
        return createAndGetPublicKey()
    }

    /** 读取公钥并返回 base64 字符串（用于 JSON 请求） */
    @Throws(Exception::class)
    fun readPublicKeyBase64(): String =
        Base64.encodeToString(readPublicKey(), Base64.NO_WRAP)

    private fun createAndGetPublicKey(): ByteArray {
        val generator = KeyPairGenerator.getInstance(
            KeyProperties.KEY_ALGORITHM_EC, KEYSTORE
        )
        val spec = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_VERIFY
        )
            .setAlgorithmParameterSpec(java.security.spec.ECGenParameterSpec("secp256r1"))
            .setDigests(KeyProperties.DIGEST_SHA256)
            .setUserAuthenticationRequired(false)  // 不要求每次指纹（绑定时不需要）
            .build()
        generator.initialize(spec)
        val keyPair = generator.generateKeyPair()
        return (keyPair.public as ECPublicKey).encoded
    }

    // ─── 厂商检测 ─────────────────────────────────────────

    /** 检测手机厂商 */
    fun detectVendor(): PhoneVendor {
        val manufacturer = Build.MANUFACTURER.lowercase()
        return when {
            manufacturer.contains("samsung") -> PhoneVendor.SAMSUNG
            manufacturer.contains("xiaomi") || manufacturer.contains("redmi") -> PhoneVendor.XIAOMI
            manufacturer.contains("oppo") || manufacturer.contains("oneplus") -> PhoneVendor.OPPO
            manufacturer.contains("vivo") || manufacturer.contains("iqoo") -> PhoneVendor.VIVO
            manufacturer.contains("huawei") || manufacturer.contains("honor") -> PhoneVendor.HUAWEI
            else -> PhoneVendor.APPLE  // 兜底（正常不会是 apple）
        }
    }

    // ─── 协议检测 ─────────────────────────────────────────

    /**
     * 检测数字钥匙协议
     *
     * - 华为/荣耀: ICCE
     * - 三星: CCC (Samsung Wallet 基于 CCC Digital Key)
     * - 小米/OPPO/vivo: ICCOA (Digital Key 4.0)
     * - 其他: 默认 ICCOA
     */
    fun detectProtocol(): DigitalKeyProtocol = when (detectVendor()) {
        PhoneVendor.HUAWEI -> DigitalKeyProtocol.ICCE
        PhoneVendor.SAMSUNG -> DigitalKeyProtocol.CCC_DK3
        else -> DigitalKeyProtocol.ICCOA_DK40
    }

    // ─── 默认访问级别 ─────────────────────────────────────

    /** 默认钥匙访问级别（锁车/解锁/启动 + 找车） */
    fun defaultAccessLevel(): Map<String, Boolean> = mapOf(
        "lock" to true,
        "unlock" to true,
        "engine" to true,
        "find" to true
    )
}
