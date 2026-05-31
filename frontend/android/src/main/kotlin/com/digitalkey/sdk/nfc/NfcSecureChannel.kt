// NfcSecureChannel.kt - NFC安全通道层
// 使用 AES-256-GCM 对 APDU 数据载荷进行会话加密，
// 基于已建立的 ECDH 会话密钥，提供机密性、完整性保护及重放防护。
package com.digitalkey.sdk.nfc

import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import com.digitalkey.sdk.error.DkErrorCode
import com.digitalkey.sdk.logger.DkLogger
import java.security.*
import java.security.spec.ECGenParameterSpec
import java.security.spec.X509EncodedKeySpec
import java.util.concurrent.ConcurrentHashMap
import javax.crypto.Cipher
import javax.crypto.KeyAgreement
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

// ═══════════════════════════════════════════════════════════
// APDU 安全通道状态 & 会话
// ═══════════════════════════════════════════════════════════

/**
 * NFC 安全通道状态
 */
enum class SecureChannelState {
    /** 初始状态 / 明文传输 */
    PLAINTEXT,
    /** 会话密钥协商中 */
    HANDSHAKING,
    /** 安全通道已建立 */
    ESTABLISHED,
    /** 会话已过期 / 需要重新协商 */
    EXPIRED,
    /** 错误状态 */
    ERROR
}

/**
 * Tag 会话信息
 *
 * 每个 NFC Tag 对应一个独立的加密会话，用 Tag ID 索引。
 */
data class SecureSession(
    /** 会话密钥（AES-256-GCM） */
    val sessionKey: SecretKey,
    /** 发送计数器（写入时递增） */
    var sendCounter: Long,
    /** 接收计数器（读取时递增） */
    var recvCounter: Long,
    /** 会话建立时间戳 */
    val establishedAt: Long = System.currentTimeMillis(),
    /** 会话有效期（毫秒） */
    val sessionTtlMs: Long = 300_000L,  // 默认5分钟
    /** 最大传输次数（防滥用） */
    val maxTransfers: Int = 100
) {
    /** 会话是否未过期 */
    val isValid: Boolean
        get() = (System.currentTimeMillis() - establishedAt) < sessionTtlMs &&
                sendCounter < maxTransfers &&
                recvCounter < maxTransfers

    /** 获取本次写入用的 IV（发送方向 counter 编码为 12 字节） */
    fun nextSendIv(): ByteArray = counterToIv(sendCounter++)

    /** 获取本次读取用的 IV（接收方向 counter 编码为 12 字节） */
    fun nextRecvIv(): ByteArray = counterToIv(recvCounter++)

    private fun counterToIv(counter: Long): ByteArray {
        val iv = ByteArray(12)  // GCM 标准 12 字节 nonce
        // 将 counter 写为 big-endian 填充到 IV 前 8 字节
        for (i in 7 downTo 0) {
            iv[7 - i] = ((counter shr (i * 8)) and 0xFF).toByte()
        }
        return iv
    }
}

// ═══════════════════════════════════════════════════════════
// ISO 7816-4 APDU CLA/INS 指令
// ═══════════════════════════════════════════════════════════

/**
 * APDU 安全通道指令扩展
 *
 * 使用保留的 CLA 字节 0x8C 标识安全通道指令，
 * 与普通 0x00 CLA 的 SELECT / READ / WRITE 区分。
 */
object SecureApduIns {
    /** 安全通道 CLA 标记 */
    const val CLA_SECURE: Byte = 0x8C.toByte()

    /** 安全通道：发送加密数据 (安全 WRITE) */
    const val INS_SECURE_WRITE: Byte = 0xD6.toByte()
    /** 安全通道：接收加密数据 (安全 READ) */
    const val INS_SECURE_READ: Byte = 0xB0.toByte()
    /** 安全通道：会话握手 (ECC 公钥交换) */
    const val INS_HANDSHAKE: Byte = 0xE0.toByte()
    /** 安全通道：会话验证 (Challenge-Response) */
    const val INS_CHALLENGE: Byte = 0xE1.toByte()

    /** 加密数据 APDU 最大 PDU 长度 */
    const val MAX_ENCRYPTED_LEN: Int = 240
}

// ═══════════════════════════════════════════════════════════
// NFC 安全通道管理器
// ═══════════════════════════════════════════════════════════

/**
 * NFC 安全通道管理器
 *
 * 职责:
 * 1. 通过 ECDH 密钥协商建立会话密钥（secp256r1）
 * 2. 使用 AES-256-GCM 加密 APDU 数据载荷
 * 3. 每次传输携带递增 counter 防重放
 * 4. 每个 Tag 独立会话，过期自动清理
 *
 * 使用方式:
 * ```kotlin
 * val secureChannel = NfcSecureChannel.getInstance()
 * // 建立会话
 * secureChannel.establishSession(tagId)
 * // 加密写入
 * val ciphertext = secureChannel.encryptForWrite(tagId, plainData)
 * // 解密读取
 * val plainData = secureChannel.decryptFromRead(tagId, ciphertext)
 * ```
 */
class NfcSecureChannel private constructor() {

    private val logger = DkLogger.getLogger("NfcSecureChannel")
    private val sessions = ConcurrentHashMap<String, SecureSession>()
    private val secureRandom = SecureRandom()
    private val keyStore: KeyStore = KeyStore.getInstance(ANDROID_KEYSTORE).apply {
        load(null)
    }

    companion object {
        private const val ANDROID_KEYSTORE = "AndroidKeyStore"
        private const val KEY_ALIAS_ECDH = "dk_nfc_ecdh_key"

        /** 密钥协商算法 */
        private const val KEY_EXCHANGE_ALG = "ECDH"
        /** session key 导出算法 */
        private const val SESSION_KEY_ALG = "AES"
        /** session key 导出长度（位） */
        private const val SESSION_KEY_LENGTH = 256
        /** key agreement 使用的曲线 */
        private const val EC_CURVE = "secp256r1"
        /** AES-GCM 加密算法字符串 */
        private const val AES_GCM_TRANSFORM = "AES/GCM/NoPadding"

        /** GCM 认证标签长度（字节） */
        private const val GCM_TAG_LEN = 16   // 128-bit tag
        /** nonce/IV 长度（字节） */
        private const val GCM_IV_LEN = 12    // 96-bit standard

        @Volatile
        private var _instance: NfcSecureChannel? = null

        /** 获取单例 */
        fun getInstance(): NfcSecureChannel {
            return _instance ?: synchronized(this) {
                _instance ?: NfcSecureChannel().also { _instance = it }
            }
        }
    }

    // ── 公/私钥管理 ─────────────────────────────────────────

    /**
     * 获取本地 ECDH 公钥（从 Android KeyStore 自动生成/加载）
     */
    fun getLocalPublicKey(): PublicKey {
        return loadOrGenerateKeyPair().public
    }

    /**
     * 建立安全会话（ECDH 密钥协商）
     *
     * @param tagId 目标 NFC Tag ID
     * @param remotePublicKeyBytes 远端（车端 TCU）公钥 DER 编码
     * @return 建立的会话
     * @throws DigitalKeyException 协商失败时抛出
     */
    fun establishSession(tagId: String, remotePublicKeyBytes: ByteArray): SecureSession {
        try {
            // 1. 加载本地密钥对
            val keyPair = loadOrGenerateKeyPair()
            val localPrivateKey = keyPair.private

            // 2. 解析远端公钥 (X.509 SubjectPublicKeyInfo / 原始 EC 点)
            val remotePubKey = parseEcPublicKey(remotePublicKeyBytes)

            // 3. ECDH 密钥协商
            val keyAgreement = KeyAgreement.getInstance(KEY_EXCHANGE_ALG)
            keyAgreement.init(localPrivateKey)
            keyAgreement.doPhase(remotePubKey, true)
            val sharedSecret = keyAgreement.generateSecret()

            // 4. 从共享秘密派生 AES-256 会话密钥 (SHA-256 裁剪)
            val digest = MessageDigest.getInstance("SHA-256")
            val derivedKeyBytes = digest.digest(sharedSecret)
            val sessionKey = SecretKeySpec(derivedKeyBytes, SESSION_KEY_ALG)

            // 5. 创建会话状态
            val session = SecureSession(
                sessionKey = sessionKey,
                sendCounter = 0L,
                recvCounter = 0L
            )

            // 6. 清理旧会话，存入新会话
            sessions.remove(tagId)
            sessions[tagId] = session

            logger.info("Secure session established for tag=$tagId")
            return session
        } catch (e: Exception) {
            logger.error("Session establishment failed for tag=$tagId", e)
            throw DkError(DkErrorCode.ERR_CRYPTO_ERROR,
                "Secure channel handshake failed: ${e.message}", cause = e)
        }
    }

    /**
     * 关闭指定 tag 的会话
     */
    fun closeSession(tagId: String) {
        sessions.remove(tagId)
        logger.info("Session closed for tag=$tagId")
    }

    /**
     * 关闭所有会话
     */
    fun closeAllSessions() {
        sessions.clear()
        logger.info("All NFC secure sessions cleared")
    }

    /**
     * 获取指定 tag 的会话状态
     */
    fun getSessionState(tagId: String): SecureChannelState {
        val session = sessions[tagId] ?: return SecureChannelState.PLAINTEXT
        return if (session.isValid) SecureChannelState.ESTABLISHED
               else SecureChannelState.EXPIRED
    }

    // ── 加密/解密 ───────────────────────────────────────────

    /**
     * 加密待发送的 APDU 数据载荷
     *
     * 格式: [12字节 IV (有效 counter)] [密文] [16字节 GCM Tag]
     *
     * @param tagId 目标 NFC Tag ID
     * @param plaintext 明文载荷
     * @param aad 附加认证数据 (AAD)，比如 APDU CLA/INS/P1/P2
     * @return 密文（含 IV + GCM Tag）
     */
    fun encryptForWrite(tagId: String, plaintext: ByteArray, aad: ByteArray = byteArrayOf()): ByteArray {
        val session = sessions[tagId]
            ?: throw DkError(DkErrorCode.ERR_SESSION_EXPIRED,
                "No secure session for tag=$tagId")

        if (!session.isValid) {
            closeSession(tagId)
            throw DkError(DkErrorCode.ERR_SESSION_EXPIRED,
                "Secure session expired for tag=$tagId")
        }

        try {
            val iv = session.nextSendIv()
            val cipher = Cipher.getInstance(AES_GCM_TRANSFORM)
            val spec = GCMParameterSpec(GCM_TAG_LEN * 8, iv)
            cipher.init(Cipher.ENCRYPT_MODE, session.sessionKey, spec)

            // 设置 AAD
            if (aad.isNotEmpty()) {
                cipher.updateAAD(aad)
            }

            val ciphertext = cipher.doFinal(plaintext)

            // 输出结构: IV(12) + ciphertext(含GCM Tag末尾16字节)
            return iv + ciphertext
        } catch (e: Exception) {
            logger.error("AES-GCM encrypt failed for tag=$tagId", e)
            throw DkError(DkErrorCode.ERR_CRYPTO_ERROR,
                "Encryption failed: ${e.message}", cause = e)
        }
    }

    /**
     * 解密从 APDU 收到的数据载荷
     *
     * 期望格式: [12字节 IV] [密文] [16字节 GCM Tag]
     *
     * @param tagId 来源 NFC Tag ID
     * @param ciphertextWithIv IV+密文（含GCM Tag）
     * @param aad AAD（需与加密时一致）
     * @return 解密后明文
     */
    fun decryptFromRead(tagId: String, ciphertextWithIv: ByteArray, aad: ByteArray = byteArrayOf()): ByteArray {
        val session = sessions[tagId]
            ?: throw DkError(DkErrorCode.ERR_SESSION_EXPIRED,
                "No secure session for tag=$tagId")

        if (!session.isValid) {
            closeSession(tagId)
            throw DkError(DkErrorCode.ERR_SESSION_EXPIRED,
                "Secure session expired for tag=$tagId")
        }

        if (ciphertextWithIv.size < GCM_IV_LEN + GCM_TAG_LEN) {
            throw DkError(DkErrorCode.ERR_CRYPTO_ERROR,
                "Ciphertext too short: ${ciphertextWithIv.size} bytes")
        }

        try {
            // 使用接收方向 counter 产生独立的 IV
            val iv = session.nextRecvIv()
            val cipher = Cipher.getInstance(AES_GCM_TRANSFORM)
            val spec = GCMParameterSpec(GCM_TAG_LEN * 8, iv)
            cipher.init(Cipher.DECRYPT_MODE, session.sessionKey, spec)

            if (aad.isNotEmpty()) {
                cipher.updateAAD(aad)
            }

            // 跳过自带的 IV（我们使用自己的 counter IV）
            val actualCiphertext = ciphertextWithIv.copyOfRange(GCM_IV_LEN, ciphertextWithIv.size)
            return cipher.doFinal(actualCiphertext)
        } catch (e: AEADBadTagException) {
            logger.error("GCM authentication failed for tag=$tagId", e)
            throw DkError(DkErrorCode.ERR_CRYPTO_ERROR,
                "GCM authentication failed: data may be tampered", cause = e)
        } catch (e: Exception) {
            logger.error("AES-GCM decrypt failed for tag=$tagId", e)
            throw DkError(DkErrorCode.ERR_CRYPTO_ERROR,
                "Decryption failed: ${e.message}", cause = e)
        }
    }

    /**
     * 构建加密的 APDU WRITE 命令
     *
     * @param tagId Tag ID
     * @param p1 P1 参数（如 offset high byte）
     * @param p2 P2 参数（如 offset low byte）
     * @param data 待写入的明文数据
     * @return 完整 APDU 命令字节
     */
    fun buildSecureWriteApdu(tagId: String, p1: Byte, p2: Byte, data: ByteArray): ByteArray {
        // AAD = CLA || INS || P1 || P2
        val aad = byteArrayOf(SecureApduIns.CLA_SECURE, SecureApduIns.INS_SECURE_WRITE, p1, p2)
        val encryptedPayload = encryptForWrite(tagId, data, aad)

        return buildApdu(
            cla = SecureApduIns.CLA_SECURE,
            ins = SecureApduIns.INS_SECURE_WRITE,
            p1 = p1,
            p2 = p2,
            data = encryptedPayload
        )
    }

    /**
     * 构建安全通道 READ APDU
     * [CR-2 fix] 独立于 WRITE 的方法，使用正确的 INS 字节
     *
     * @param tagId Tag ID
     * @param p1 P1 参数（如偏移量高字节）
     * @param p2 P2 参数（如偏移量低字节）
     * @param length 期望读取的字节数
     * @return 完整的读命令 APDU
     */
    fun buildSecureReadApdu(tagId: String, p1: Byte, p2: Byte, length: Int): ByteArray {
        val aad = byteArrayOf(SecureApduIns.CLA_SECURE, SecureApduIns.INS_SECURE_READ, p1, p2)
        val encryptedPayload = encryptForWrite(tagId, byteArrayOf(length.toByte()), aad)
        return buildApdu(
            cla = SecureApduIns.CLA_SECURE,
            ins = SecureApduIns.INS_SECURE_READ,
            p1 = p1,
            p2 = p2,
            data = encryptedPayload
        )
    }

    /**
     * 解密安全通道 READ 响应
     *
     * @param tagId Tag ID
     * @param responseData APDU 响应数据（不含状态字 0x9000）
     * @param p1 读取命令的 P1
     * @param p2 读取命令的 P2
     * @return 解密后的明文数据
     */
    fun decryptSecureReadResponse(tagId: String, responseData: ByteArray, p1: Byte, p2: Byte): ByteArray {
        val aad = byteArrayOf(SecureApduIns.CLA_SECURE, SecureApduIns.INS_SECURE_READ, p1, p2)
        return decryptFromRead(tagId, responseData, aad)
    }

    /**
     * 构建安全通道握手 APDU（发送本地公钥）
     *
     * @return SELECT + HANDSHAKE 命令对（先发送 SELECT AID，再发送公钥）
     */
    fun buildHandshakeApdu(localPublicKey: PublicKey): ByteArray {
        val pubKeyEncoded = localPublicKey.encoded  // X.509 SubjectPublicKeyInfo
        // 指令: 8C E0 00 00 Lc || publicKeyBytes
        return buildApdu(
            cla = SecureApduIns.CLA_SECURE,
            ins = SecureApduIns.INS_HANDSHAKE,
            p1 = 0x00,
            p2 = 0x00,
            data = pubKeyEncoded
        )
    }

    /**
     * 验证握手响应并建立会话
     *
     * @param tagId Tag ID
     * @param responseData 握手响应数据（含远端公钥）
     */
    fun handleHandshakeResponse(tagId: String, responseData: ByteArray) {
        // 响应: [远端EC公钥, 作为 session 建立]
        if (responseData.isEmpty()) {
            throw DkError(DkErrorCode.ERR_CRYPTO_ERROR,
                "Empty handshake response for tag=$tagId")
        }
        // 远端在响应中返回了其 ECC 公钥
        establishSession(tagId, responseData)
    }

    // ── 私有工具方法 ─────────────────────────────────────────

    /**
     * 构建标准 ISO 7816-4 APDU
     */
    private fun buildApdu(cla: Byte, ins: Byte, p1: Byte, p2: Byte, data: ByteArray): ByteArray {
        val lc = data.size
        require(lc <= SecureApduIns.MAX_ENCRYPTED_LEN) {
            "APDU data length $lc exceeds max ${SecureApduIns.MAX_ENCRYPTED_LEN}"
        }
        if (lc == 0) {
            return byteArrayOf(cla, ins, p1, p2)
        }
        return byteArrayOf(cla, ins, p1, p2, lc.toByte()) + data
    }

    /**
     * 加载或生成 ECDH 密钥对（Android KeyStore 持久化）
     */
    private fun loadOrGenerateKeyPair(): KeyPair {
        if (keyStore.containsAlias(KEY_ALIAS_ECDH)) {
            val privateKey = keyStore.getKey(KEY_ALIAS_ECDH, null) as? PrivateKey
            val certificate = keyStore.getCertificate(KEY_ALIAS_ECDH)
            if (privateKey != null && certificate != null) {
                return KeyPair(certificate.publicKey, privateKey)
            }
        }

        val generator = KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, ANDROID_KEYSTORE)
        val spec = KeyGenParameterSpec.Builder(KEY_ALIAS_ECDH, KeyProperties.PURPOSE_AGREE_KEY)
            .setAlgorithmParameterSpec(ECGenParameterSpec(EC_CURVE))
            .setDigests(KeyProperties.DIGEST_SHA256)
            .setKeySize(SESSION_KEY_LENGTH)
            .build()
        generator.initialize(spec)
        return generator.generateKeyPair()
    }

    /**
     * 解析远端 EC 公钥
     */
    private fun parseEcPublicKey(encoded: ByteArray): PublicKey {
        return try {
            // 尝试 X.509 SubjectPublicKeyInfo 格式
            val keyFactory = KeyFactory.getInstance("EC")
            keyFactory.generatePublic(X509EncodedKeySpec(encoded))
        } catch (e: Exception) {
            throw DkError(DkErrorCode.ERR_CRYPTO_ERROR,
                "Failed to parse remote EC public key", cause = e)
        }
    }
}

// ═══════════════════════════════════════════════════════════
// DkError 补充 — 供 NfcManager 使用
// ═══════════════════════════════════════════════════════════

/**
 * 轻量错误包装，兼容 NfcManager 引用。
 *
 * DkErrorCode 中的常量已使用 `ERR_*` 命名，此处映射到 NfcManager 期望的简写。
 * 实际错误编码与 [com.digitalkey.sdk.error.DkErrorCode] 中定义的一致。
 */
class DkError(
    val code: Int,
    override val message: String,
    val cause: Throwable? = null
) : Exception(message, cause) {

    override fun toString(): String {
        return "DkError(code=0x%04X, message='$message')".format(code)
    }
}
