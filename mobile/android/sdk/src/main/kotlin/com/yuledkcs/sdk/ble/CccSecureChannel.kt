package com.yuledkcs.sdk.ble

import javax.crypto.Cipher
import javax.crypto.Mac
import javax.crypto.spec.IvParameterSpec
import javax.crypto.spec.SecretKeySpec

/**
 * CCC 安全通道 (2b-E) — GPC_SPE_014 (SCP03 风格)
 *
 * 依据: docs/certification/ccc-ts101-ble-secure-channel.md
 *   - §18.4.9  Listing 18-9  系统密钥派生 (HKDF-SHA256, Info="SystemKeys")
 *   - §18.4.12 Listing 18-10 命令加密与认证 (GPC_SPE_014 §6.2.6)
 *   - §18.4.13 Listing 18-11 响应加密与认证 (GPC_SPE_014 §6.2.7)
 * 算法族: AES-128 + CMAC-AES-128 (RFC4493)。既不是 AES-CCM 也不是 AES-256-GCM。
 *
 * 纯 JVM 实现 (javax.crypto), 可单测。
 */
object CccSystemKeyDerivation {

    /** HKDF-Extract (RFC5869 §2.2): salt 为 null 时用 32 字节零串 (规范要求 NULL) */
    fun extract(salt: ByteArray? = null, ikm: ByteArray): ByteArray {
        val saltData = salt ?: ByteArray(32)
        return hmacSha256(saltData, ikm)
    }

    /** HKDF-Expand (RFC5869 §2.3) */
    fun expand(prk: ByteArray, info: ByteArray, outputLength: Int): ByteArray {
        require(outputLength <= 255 * 32) { "HKDF-Expand output too long: $outputLength" }
        val out = java.io.ByteArrayOutputStream()
        var t = ByteArray(0)
        var counter = 1
        while (out.size() < outputLength) {
            val input = t + info + byteArrayOf(counter.toByte())
            t = hmacSha256(prk, input)
            out.write(t)
            counter++
        }
        return out.toByteArray().copyOf(outputLength)
    }

    /** 完整 HKDF-SHA256: Info="SystemKeys", L=64 → Kenc/Kmac/Krmac/LTSS (各 16 字节) */
    fun deriveSystemKeys(sk: ByteArray): SystemKeySet {
        val prk = extract(salt = null, ikm = sk)
        val okm = expand(prk, "SystemKeys".toByteArray(Charsets.UTF_8), 64)
        return SystemKeySet(
            kenc = okm.copyOfRange(0, 16),
            kmac = okm.copyOfRange(16, 32),
            krmac = okm.copyOfRange(32, 48),
            longTermSharedSecret = okm.copyOfRange(48, 64)
        )
    }

    private fun hmacSha256(key: ByteArray, data: ByteArray): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(key, "HmacSHA256"))
        return mac.doFinal(data)
    }
}

/** 系统密钥组 (各 16 字节) */
data class SystemKeySet(
    val kenc: ByteArray,
    val kmac: ByteArray,
    val krmac: ByteArray,
    val longTermSharedSecret: ByteArray
) {
    override fun equals(other: Any?): Boolean =
        other is SystemKeySet && kenc.contentEquals(other.kenc) && kmac.contentEquals(other.kmac) &&
            krmac.contentEquals(other.krmac) && longTermSharedSecret.contentEquals(other.longTermSharedSecret)

    override fun hashCode(): Int =
        31 * (31 * (31 * kenc.contentHashCode() + kmac.contentHashCode()) + krmac.contentHashCode()) +
            longTermSharedSecret.contentHashCode()
}

/** AES-128 原语 (javax.crypto) */
object CccAesCrypto {

    /** AES-128-CBC 加密 (无填充, 调用方负责 block 对齐) */
    fun cbcEncrypt(key: ByteArray, iv: ByteArray, plaintext: ByteArray): ByteArray {
        require(key.size == 16) { "AES-128 key must be 16 bytes" }
        require(iv.size == 16) { "IV must be 16 bytes" }
        require(plaintext.size % 16 == 0) { "input must be block aligned" }
        val cipher = Cipher.getInstance("AES/CBC/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(key, "AES"), IvParameterSpec(iv))
        return cipher.doFinal(plaintext)
    }

    /** AES-128-CBC 解密 (无填充) */
    fun cbcDecrypt(key: ByteArray, iv: ByteArray, ciphertext: ByteArray): ByteArray {
        require(key.size == 16) { "AES-128 key must be 16 bytes" }
        require(iv.size == 16) { "IV must be 16 bytes" }
        require(ciphertext.size % 16 == 0) { "input must be block aligned" }
        val cipher = Cipher.getInstance("AES/CBC/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, SecretKeySpec(key, "AES"), IvParameterSpec(iv))
        return cipher.doFinal(ciphertext)
    }

    /** AES-128-ECB 单块加密 (CMAC 子密钥生成用) */
    fun ecbEncryptBlock(key: ByteArray, block: ByteArray): ByteArray {
        require(block.size == 16) { "CMAC block must be 16 bytes" }
        val cipher = Cipher.getInstance("AES/ECB/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(key, "AES"))
        return cipher.doFinal(block)
    }
}

/** CMAC-AES-128 (RFC4493) */
object CccAesCmac {

    /** RFC4493 子密钥生成: L=AES_K(0^128); K1/K2 */
    fun subkeys(key: ByteArray): Pair<ByteArray, ByteArray> {
        val l = CccAesCrypto.ecbEncryptBlock(key, ByteArray(16))
        val k1 = leftShiftOne(l)
        val k2 = leftShiftOne(k1)
        return k1 to k2
    }

    private fun leftShiftOne(input: ByteArray): ByteArray {
        // 若最高位为 0: 左移 1; 否则左移 1 后 XOR 0x87 (最低字节)
        val rb: Int = 0x87
        val out = ByteArray(16)
        val msb = input[0].toUInt8() and 0x80
        var carry = 0
        for (i in 15 downTo 0) {
            val b = input[i].toUInt8()
            out[i] = ((b shl 1) or carry).toByte()
            carry = (b and 0x80) shr 7
        }
        if (msb != 0) out[15] = (out[15].toUInt8() xor rb).toByte()
        return out
    }

    /** RFC4493 §2.4 CMAC 计算, 输出 16 字节 */
    fun cmac(key: ByteArray, message: ByteArray): ByteArray {
        val (k1, k2) = subkeys(key)
        val blockSize = 16
        val n = maxOf(1, (message.size + blockSize - 1) / blockSize)
        val lastStart = (n - 1) * blockSize
        val isComplete = message.isNotEmpty() && message.size % blockSize == 0

        val lastBlock: ByteArray
        if (isComplete && message.size >= blockSize) {
            lastBlock = xor(message.copyOfRange(lastStart, message.size), k1)
        } else {
            val padded = message.copyOfRange(lastStart, message.size) + byteArrayOf(0x80.toByte())
            var p = padded
            while (p.size < blockSize) p = p + byteArrayOf(0)
            lastBlock = xor(p, k2)
        }

        var x = ByteArray(blockSize)
        for (i in 0 until (n - 1)) {
            val block = message.copyOfRange(i * blockSize, (i + 1) * blockSize)
            x = CccAesCrypto.ecbEncryptBlock(key, xor(x, block))
        }
        return CccAesCrypto.ecbEncryptBlock(key, xor(x, lastBlock))
    }

    /** C-MAC / R-MAC: CMAC 输出截断为 8 字节 (规范 §18.4.12/13) */
    fun mac8(key: ByteArray, message: ByteArray): ByteArray = cmac(key, message).copyOf(8)

    private fun xor(a: ByteArray, b: ByteArray): ByteArray {
        require(a.size == b.size)
        return ByteArray(a.size) { (a[it].toUInt8() xor b[it].toUInt8()).toByte() }
    }
}

/** ISO/IEC 9797-1 Padding Method 2 (SCP03 填充) */
object CccPadding {
    /** 0x80 后补零到 16 字节边界 */
    fun pad(data: ByteArray): ByteArray {
        var out = data + byteArrayOf(0x80.toByte())
        while (out.size % 16 != 0) out = out + byteArrayOf(0)
        return out
    }

    /** 去除 0x80 填充 */
    fun unpad(data: ByteArray): ByteArray {
        require(data.isNotEmpty() && data.size % 16 == 0) { "unpad: bad length" }
        var idx = data.size - 1
        while (idx >= 0 && data[idx].toUInt8() == 0) idx--
        require(idx >= 0 && data[idx].toUInt8() == 0x80) { "unpad: missing 0x80 marker" }
        return data.copyOf(idx)
    }
}

/**
 * CCC Secure Channel — 命令加密 + 响应解密/验证 (GPC_SPE_014 SCP03 风格)
 *
 * 状态: 维护命令计数器 (01h 起) 与 MAC Chaining Value (16 字节)。
 * 一个实例对应一条 L2CAP 连接, 按序使用。
 */
class CccSecureChannel(private val keys: SystemKeySet) {

    private var commandCounter: Int = 0x01
    private var macChainingValue = ByteArray(16)
    private var lastCommandCounter: Int = 0x00

    /** 加密并认证命令 (Listing 18-10) */
    fun encryptCommand(plaintext: ByteArray): Pair<ByteArray, ByteArray> {
        // 1. Padded Counter Block: 0000...00 || counter
        val iv = ByteArray(16)
        iv[15] = commandCounter.toByte()

        // 2. S-ENC: AES-128-CBC(Kenc, ICV, pad(payload))
        val padded = CccPadding.pad(plaintext)
        val ciphertext = CccAesCrypto.cbcEncrypt(keys.kenc, iv, padded)

        // 3. S-MAC: C-MAC = CMAC(Kmac, MAC_Chaining_Value || ciphertext)[0:8]
        val macInput = macChainingValue + ciphertext
        val mac = CccAesCmac.mac8(keys.kmac, macInput)

        // 4. 更新 chaining: 完整 16 字节 CMAC 输出
        macChainingValue = CccAesCmac.cmac(keys.kmac, macInput)

        lastCommandCounter = commandCounter
        if (commandCounter < 0xFF) commandCounter++
        return ciphertext to mac
    }

    /** 解密并验证响应 (Listing 18-11) */
    fun decryptResponse(ciphertext: ByteArray, rmac: ByteArray): ByteArray {
        // 1. Padded Counter Block: 8000...00 || command counter
        val iv = ByteArray(16)
        iv[0] = 0x80.toByte()
        iv[15] = lastCommandCounter.toByte()

        // 2. 验证 R-MAC = CMAC(Krmac, command MAC_Chaining_Value || ciphertext)[0:8]
        val macInput = ByteArray(16) + ciphertext
        val expected = CccAesCmac.mac8(keys.krmac, macInput)
        check(expected.contentEquals(rmac)) { "R-MAC verification failed" }

        // 3. S-ENC: AES-128-CBC 解密
        return CccPadding.unpad(CccAesCrypto.cbcDecrypt(keys.kenc, iv, ciphertext))
    }

    /** 重置通道状态 (新连接/新会话) */
    fun reset() {
        commandCounter = 0x01
        macChainingValue = ByteArray(16)
        lastCommandCounter = 0x00
    }
}

/**
 * CCC 消息安全提供者 — 控制指令载荷的加密/签名接缝。
 *
 * 真实实现: [CccSecureChannel] (GPC_SPE_014 SCP03: AES-128 + CMAC-AES-128),
 * 依据 docs/certification/ccc-ts101-ble-secure-channel.md §5 (2026-07-31 规范裁决)。
 */
interface CccMessageSecurity {
    /** 加密控制载荷 (含完整性保护) */
    fun encrypt(plaintext: ByteArray): ByteArray
    /** 解密控制载荷 (含完整性校验) */
    fun decrypt(ciphertext: ByteArray): ByteArray
    /** 对载荷签名 */
    fun sign(data: ByteArray): ByteArray
    /** 验签 */
    fun verify(data: ByteArray, signature: ByteArray): Boolean
}

/** 透传安全提供者 — 仅用于单元测试与联调。明文直通、验签恒真。禁止用于生产。 */
class CccNullMessageSecurity : CccMessageSecurity {
    override fun encrypt(plaintext: ByteArray): ByteArray = plaintext
    override fun decrypt(ciphertext: ByteArray): ByteArray = ciphertext
    override fun sign(data: ByteArray): ByteArray = ByteArray(0)
    override fun verify(data: ByteArray, signature: ByteArray): Boolean = true
}
