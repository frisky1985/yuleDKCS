// NfcManager.kt - NFC管理模块
package com.digitalkey.sdk.nfc

import android.app.Activity
import android.app.PendingIntent
import android.content.Intent
import android.content.IntentFilter
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.NfcAdapter
import android.nfc.Tag
import android.nfc.tech.IsoDep
import android.nfc.tech.Ndef
import android.nfc.tech.NfcA
import android.os.Build
import com.digitalkey.sdk.error.DkErrorCode
import com.digitalkey.sdk.logger.DkLogger
import com.digitalkey.sdk.telemetry.DkTelemetry

/**
 * NFC事件监听器
 */
interface NfcEventListener {
    fun onTagDiscovered(tag: Tag, techList: List<String>)
    fun onNdefDiscovered(message: NdefMessage)
    fun onDataReceived(tagId: String, data: ByteArray)
    fun onError(error: DkError)
}

/**
 * NFC管理器
 *
 * 支持明文和安全通道两种APDU通信模式。
 * 安全通道使用 AES-256-GCM 加密数据载荷，基于 ECDH 密钥协商建立的会话密钥。
 *
 * 使用安全通道:
 * ```kotlin
 * // 1. 建立会话（需先执行 ISO SELECT 选定应用）
 * nfcManager.establishSecureSession(tag, remotePublicKey)
 *
 * // 2. 安全读写
 * nfcManager.secureReadData(tag, offset, length)   // 自动加密/解密
 * nfcManager.secureWriteData(tag, offset, data)
 * ```
 */
class NfcManager(private val activity: Activity) {
    
    private val logger = DkLogger.getLogger("NfcManager")
    private val telemetry = DkTelemetry
    
    private var nfcAdapter: NfcAdapter? = null
    private var pendingIntent: PendingIntent? = null
    private var intentFilters: Array<IntentFilter>? = null
    private var techLists: Array<Array<String>>? = null
    
    private val listeners = mutableListOf<NfcEventListener>()
    
    /** NFC 安全通道实例 */
    private val secureChannel = NfcSecureChannel.getInstance()
    
    // AID for Digital Key
    companion object {
        const val DIGITAL_KEY_AID = "D2760000850101FF"
        const val SELECT_CMD = "00A40400"
        const val MAX_TRANSCEIVE_LENGTH = 253
    }
    
    init {
        initialize()
    }
    
    private fun initialize() {
        nfcAdapter = NfcAdapter.getDefaultAdapter(activity)
        
        // Create pending intent for NFC discovery
        val intent = Intent(activity, activity.javaClass).apply {
            addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP)
        }
        
        val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        } else {
            PendingIntent.FLAG_UPDATE_CURRENT
        }
        
        pendingIntent = PendingIntent.getActivity(activity, 0, intent, flags)
        
        // Setup intent filters for NDEF and tag discovery
        val ndefFilter = IntentFilter(NfcAdapter.ACTION_NDEF_DISCOVERED).apply {
            try {
                addDataType("*/*")
            } catch (e: IntentFilter.MalformedMimeTypeException) {
                logger.error("Malformed MIME type", e)
            }
        }
        
        val tagFilter = IntentFilter(NfcAdapter.ACTION_TAG_DISCOVERED)
        val techFilter = IntentFilter(NfcAdapter.ACTION_TECH_DISCOVERED)
        
        intentFilters = arrayOf(ndefFilter, tagFilter, techFilter)
        techLists = arrayOf(
            arrayOf(IsoDep::class.java.name),
            arrayOf(Ndef::class.java.name),
            arrayOf(NfcA::class.java.name)
        )
        
        logger.info("NFC Manager initialized, available: ${isAvailable()}")
    }
    
    // ── 监听器管理 ──────────────────────────────────────────
    
    fun addListener(listener: NfcEventListener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener)
        }
    }
    
    fun removeListener(listener: NfcEventListener) {
        listeners.remove(listener)
    }
    
    // ── 状态检查 ────────────────────────────────────────────
    
    fun isAvailable(): Boolean {
        return nfcAdapter != null && nfcAdapter?.isEnabled == true
    }
    
    fun isSupported(): Boolean {
        return nfcAdapter != null
    }
    
    // ── 前台/读卡器 调度 ────────────────────────────────────
    
    fun enableForegroundDispatch() {
        if (!isAvailable()) {
            notifyError(DkError(DkErrorCode.ERR_NFC_DISABLED, "NFC is not available"))
            return
        }
        
        try {
            nfcAdapter?.enableForegroundDispatch(activity, pendingIntent, intentFilters, techLists)
            logger.info("Foreground dispatch enabled")
        } catch (e: Exception) {
            logger.error("Failed to enable foreground dispatch", e)
            notifyError(DkError(DkErrorCode.ERR_NFC_DISABLED, cause = e))
        }
    }
    
    fun disableForegroundDispatch() {
        try {
            nfcAdapter?.disableForegroundDispatch(activity)
            logger.info("Foreground dispatch disabled")
        } catch (e: Exception) {
            logger.error("Failed to disable foreground dispatch", e)
        }
    }
    
    @Suppress("DEPRECATION")
    fun enableReaderMode(flags: Int = NfcAdapter.FLAG_READER_NFC_A or NfcAdapter.FLAG_READER_NFC_B or NfcAdapter.FLAG_READER_NFC_F or NfcAdapter.FLAG_READER_NFC_V) {
        if (!isAvailable()) {
            notifyError(DkError(DkErrorCode.ERR_NFC_DISABLED, "NFC is not available"))
            return
        }
        
        try {
            nfcAdapter?.enableReaderMode(activity, { tag ->
                handleTagDiscovered(tag)
            }, flags, null)
            logger.info("Reader mode enabled")
        } catch (e: Exception) {
            logger.error("Failed to enable reader mode", e)
            notifyError(DkError(DkErrorCode.ERR_NFC_DISABLED, cause = e))
        }
    }
    
    fun disableReaderMode() {
        try {
            nfcAdapter?.disableReaderMode(activity)
            logger.info("Reader mode disabled")
        } catch (e: Exception) {
            logger.error("Failed to disable reader mode", e)
        }
    }
    
    // ── Intent 处理 ────────────────────────────────────────
    
    fun handleIntent(intent: Intent?) {
        if (intent == null) return
        
        when (intent.action) {
            NfcAdapter.ACTION_NDEF_DISCOVERED -> handleNdefIntent(intent)
            NfcAdapter.ACTION_TECH_DISCOVERED -> handleTechIntent(intent)
            NfcAdapter.ACTION_TAG_DISCOVERED -> handleTagIntent(intent)
        }
    }
    
    // ── 明文 APDU 通信（原始接口，保持兼容）─────────────────
    
    /**
     * 发送 APDU 命令（明文）
     *
     * 这是原始接口，直接发送原始字节。对于新代码推荐使用 [secureReadData] / [secureWriteData]。
     */
    fun sendApdu(tag: Tag, command: ByteArray): ByteArray? {
        return try {
            IsoDep.get(tag)?.use { isoDep ->
                isoDep.timeout = 5000
                isoDep.transceive(command)
            }
        } catch (e: Exception) {
            logger.error("APDU command failed", e)
            telemetry.trackError(
                DkErrorCode.ERR_NFC_READ_FAILED,
                "APDU failed: ${e.message}"
            )
            null
        }
    }
    
    /**
     * 选择数字钥匙应用（明文 SELECT 命令）
     *
     * ISO 7816-4 SELECT 指令保持明文，这是协议标准要求。
     */
    fun selectDigitalKeyApp(tag: Tag): ByteArray? {
        val selectCommand = buildSelectCommand(DIGITAL_KEY_AID)
        return sendApdu(tag, selectCommand)
    }
    
    /**
     * 读取数据（明文）
     */
    fun readData(tag: Tag, offset: Int, length: Int): ByteArray? {
        val readCommand = buildReadCommand(offset, length)
        return sendApdu(tag, readCommand)
    }
    
    /**
     * 写入数据（明文）
     */
    fun writeData(tag: Tag, offset: Int, data: ByteArray): Boolean {
        val writeCommand = buildWriteCommand(offset, data)
        val response = sendApdu(tag, writeCommand)
        return response?.let { checkResponse(it) } ?: false
    }
    
    // ── 安全通道 APDU 通信（AES-256-GCM 加密）───────────────
    
    /**
     * 建立 NFC 安全会话（ECDH 密钥协商）
     *
     * 流程:
     * 1. 发送本地 ECDH 公钥（通过安全通道握手 APDU）
     * 2. 车端返回其公钥
     * 3. 双方通过 ECDH 计算出共享秘密
     * 4. 派生 AES-256 会话密钥用于后续加密通信
     *
     * @param tag 当前 NFC Tag
     * @return true 表示会话建立成功
     */
    fun establishSecureSession(tag: Tag): Boolean {
        val tagId = getTagId(tag)
        
        try {
            // 1. 获取本地公钥
            val localPublicKey = secureChannel.getLocalPublicKey()
            
            // 2. 构建并发送握手 APDU
            val handshakeApdu = secureChannel.buildHandshakeApdu(localPublicKey)
            val response = sendApdu(tag, handshakeApdu) ?: run {
                logger.error("Secure handshake failed: no response from tag=$tagId")
                return false
            }
            
            // 3. 提取响应中的远端公钥并建立会话
            //    响应格式：前 N 字节为远端公钥，后 2 字节为 0x9000 状态字
            val responseData = if (response.size >= 2) {
                response.copyOfRange(0, response.size - 2)
            } else {
                response
            }
            
            secureChannel.handleHandshakeResponse(tagId, responseData)
            
            logger.info("Secure session established for tag=$tagId")
            telemetry.track("nfc_secure_handshake", mapOf(
                "tag_id" to tagId,
                "state" to "established"
            ))
            
            return true
        } catch (e: Exception) {
            logger.error("Secure session establishment failed for tag=$tagId", e)
            telemetry.trackError(
                DkErrorCode.ERR_CRYPTO_ERROR,
                "NFC secure handshake failed: ${e.message}"
            )
            return false
        }
    }
    
    /**
     * 安全读取数据（通过加密通道）
     *
     * APDU 指令使用安全 CLA (0x8C)，数据载荷经 AES-256-GCM 加密，
     * 响应数据自动解密后返回。
     *
     * @param tag NFC Tag
     * @param offset 读取偏移
     * @param length 读取长度
     * @return 解密后的明文数据，失败返回 null
     */
    fun secureReadData(tag: Tag, offset: Int, length: Int): ByteArray? {
        val tagId = getTagId(tag)
        val state = secureChannel.getSessionState(tagId)
        
        if (state != SecureChannelState.ESTABLISHED) {
            notifyError(DkError(DkErrorCode.ERR_SESSION_EXPIRED,
                "Secure session not established for tag=$tagId"))
            return null
        }
        
        return try {
            // 构建加密的 READ 命令
            val p1 = (offset shr 8).toByte()
            val p2 = offset.toByte()
            val encryptedReadCommand = secureChannel.buildSecureWriteApdu(
                tagId, p1, p2, byteArrayOf(length.toByte())
            )
            
            // 发送
            val response = sendApdu(tag, encryptedReadCommand) ?: return null
            
            // 提取响应数据（去掉最后2字节状态字）
            val responseData = if (response.size >= 2) {
                response.copyOfRange(0, response.size - 2)
            } else {
                response
            }
            
            // 解密响应
            val plaintext = secureChannel.decryptSecureReadResponse(tagId, responseData, p1, p2)
            
            telemetry.track("nfc_secure_read", mapOf(
                "tag_id" to tagId,
                "offset" to offset,
                "length" to length,
                "decrypted_len" to plaintext.size
            ))
            
            plaintext
        } catch (e: Exception) {
            logger.error("Secure read failed for tag=$tagId", e)
            telemetry.trackError(
                DkErrorCode.ERR_NFC_READ_FAILED,
                "Secure read failed: ${e.message}"
            )
            null
        }
    }
    
    /**
     * 安全写入数据（通过加密通道）
     *
     * @param tag NFC Tag
     * @param offset 写入偏移
     * @param data 明文数据（自动加密后发送）
     * @return true 表示写入成功
     */
    fun secureWriteData(tag: Tag, offset: Int, data: ByteArray): Boolean {
        val tagId = getTagId(tag)
        val state = secureChannel.getSessionState(tagId)
        
        if (state != SecureChannelState.ESTABLISHED) {
            notifyError(DkError(DkErrorCode.ERR_SESSION_EXPIRED,
                "Secure session not established for tag=$tagId"))
            return false
        }
        
        return try {
            val p1 = (offset shr 8).toByte()
            val p2 = offset.toByte()
            
            // 构建加密的 WRITE 命令
            val encryptedWriteCommand = secureChannel.buildSecureWriteApdu(tagId, p1, p2, data)
            
            // 发送
            val response = sendApdu(tag, encryptedWriteCommand)
            
            val success = response?.let { checkResponse(it) } ?: false
            
            if (success) {
                telemetry.track("nfc_secure_write", mapOf(
                    "tag_id" to tagId,
                    "offset" to offset,
                    "length" to data.size
                ))
            }
            
            success
        } catch (e: Exception) {
            logger.error("Secure write failed for tag=$tagId", e)
            telemetry.trackError(
                DkErrorCode.ERR_NFC_WRITE_FAILED,
                "Secure write failed: ${e.message}"
            )
            false
        }
    }
    
    /**
     * 关闭安全会话
     */
    fun closeSecureSession(tagId: String) {
        secureChannel.closeSession(tagId)
        logger.info("Secure session closed for tag=$tagId")
    }
    
    /**
     * 检查安全通道状态
     */
    fun getSecureChannelState(tagId: String): SecureChannelState {
        return secureChannel.getSessionState(tagId)
    }
    
    // ── Tag 信息 ───────────────────────────────────────────
    
    fun getTagId(tag: Tag): String {
        return tag.id.toHexString()
    }
    
    fun getTagTechList(tag: Tag): List<String> {
        return tag.techList.toList()
    }
    
    // ── 释放资源 ───────────────────────────────────────────
    
    fun release() {
        disableForegroundDispatch()
        disableReaderMode()
        secureChannel.closeAllSessions()
        listeners.clear()
    }
    
    // ── 私有方法 ───────────────────────────────────────────
    
    private fun handleTagDiscovered(tag: Tag) {
        val tagId = getTagId(tag)
        val techList = getTagTechList(tag)
        
        logger.info("Tag discovered: $tagId, techs: $techList")
        
        telemetry.track("nfc_tap", mapOf(
            "tag_id" to tagId,
            "tech_list" to techList.joinToString(",")
        ))
        
        listeners.forEach { it.onTagDiscovered(tag, techList) }
        
        // Try to read NDEF if available
        try {
            Ndef.get(tag)?.use { ndef ->
                ndef.connect()
                if (ndef.isConnected) {
                    val message = ndef.ndefMessage
                    if (message != null) {
                        listeners.forEach { it.onNdefDiscovered(message) }
                    }
                }
            }
        } catch (e: Exception) {
            logger.debug("NDEF not available: ${e.message}")
        }
    }
    
    private fun handleNdefIntent(intent: Intent) {
        val rawMessages = intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)
        if (rawMessages != null) {
            for (raw in rawMessages) {
                val message = raw as NdefMessage
                logger.info("NDEF message received, records: ${message.records.size}")
                listeners.forEach { it.onNdefDiscovered(message) }
            }
        }
    }
    
    private fun handleTechIntent(intent: Intent) {
        val tag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG, Tag::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG)
        }
        
        tag?.let { handleTagDiscovered(it) }
    }
    
    private fun handleTagIntent(intent: Intent) {
        val tag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG, Tag::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG)
        }
        
        tag?.let { handleTagDiscovered(it) }
    }
    
    private fun buildSelectCommand(aid: String): ByteArray {
        val aidBytes = aid.hexToBytes()
        return byteArrayOf(
            0x00, 0xA4, 0x04, 0x00  // SELECT command
        ) + byteArrayOf(aidBytes.size.toByte()) + aidBytes
    }
    
    private fun buildReadCommand(offset: Int, length: Int): ByteArray {
        return byteArrayOf(
            0x00, 0xB0,  // READ BINARY
            (offset shr 8).toByte(),
            offset.toByte(),
            length.toByte()
        )
    }
    
    private fun buildWriteCommand(offset: Int, data: ByteArray): ByteArray {
        return byteArrayOf(
            0x00, 0xD6,  // UPDATE BINARY
            (offset shr 8).toByte(),
            offset.toByte(),
            data.size.toByte()
        ) + data
    }
    
    private fun checkResponse(response: ByteArray): Boolean {
        // Check status bytes (last 2 bytes)
        if (response.size >= 2) {
            val status = (response[response.size - 2].toInt() shl 8) or response[response.size - 1].toInt()
            return status == 0x9000 // Success status
        }
        return false
    }
    
    private fun notifyError(error: DkError) {
        logger.error(error.message, error)
        telemetry.trackError(error.code, error.message)
        listeners.forEach { it.onError(error) }
    }
    
    // Extension functions
    
    private fun ByteArray.toHexString(): String {
        return joinToString("") { "%02X".format(it) }
    }
    
    private fun String.hexToBytes(): ByteArray {
        return chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    }
    
    private fun Int.toInt(): Int = this
}

// Extension for NdefRecord parsing
fun NdefRecord.toHexString(): String {
    return payload.toHexString()
}

private fun ByteArray.toHexString(): String {
    return joinToString("") { "%02X".format(it) }
}
