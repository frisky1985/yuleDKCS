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
import com.digitalkey.sdk.error.DkError
import com.digitalkey.sdk.error.DkErrorCode
import com.digitalkey.sdk.logger.DkLogger
import com.digitalkey.sdk.telemetry.DkTelemetry

// NFC事件监听器
interface NfcEventListener {
    fun onTagDiscovered(tag: Tag, techList: List<String>)
    fun onNdefDiscovered(message: NdefMessage)
    fun onDataReceived(tagId: String, data: ByteArray)
    fun onError(error: DkError)
}

// NFC管理器
class NfcManager(private val activity: Activity) {
    
    private val logger = DkLogger.getLogger("NfcManager")
    private val telemetry = DkTelemetry
    
    private var nfcAdapter: NfcAdapter? = null
    private var pendingIntent: PendingIntent? = null
    private var intentFilters: Array<IntentFilter>? = null
    private var techLists: Array<Array<String>>? = null
    
    private val listeners = mutableListOf<NfcEventListener>()
    
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
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE
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
    
    // 添加监听器
    fun addListener(listener: NfcEventListener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener)
        }
    }
    
    // 移除监听器
    fun removeListener(listener: NfcEventListener) {
        listeners.remove(listener)
    }
    
    // 检查NFC是否可用
    fun isAvailable(): Boolean {
        return nfcAdapter != null && nfcAdapter?.isEnabled == true
    }
    
    // 检查NFC是否支持
    fun isSupported(): Boolean {
        return nfcAdapter != null
    }
    
    // 启用前台调度
    fun enableForegroundDispatch() {
        if (!isAvailable()) {
            notifyError(DkError(DkErrorCode.nfcDisabled, "NFC is not available"))
            return
        }
        
        try {
            nfcAdapter?.enableForegroundDispatch(activity, pendingIntent, intentFilters, techLists)
            logger.info("Foreground dispatch enabled")
        } catch (e: Exception) {
            logger.error("Failed to enable foreground dispatch", e)
            notifyError(DkError(DkErrorCode.nfcDisabled, cause = e))
        }
    }
    
    // 禁用前台调度
    fun disableForegroundDispatch() {
        try {
            nfcAdapter?.disableForegroundDispatch(activity)
            logger.info("Foreground dispatch disabled")
        } catch (e: Exception) {
            logger.error("Failed to disable foreground dispatch", e)
        }
    }
    
    // 启用Beam/读卡模式
    @Suppress("DEPRECATION")
    fun enableReaderMode(flags: Int = NfcAdapter.FLAG_READER_NFC_A or NfcAdapter.FLAG_READER_NFC_B or NfcAdapter.FLAG_READER_NFC_F or NfcAdapter.FLAG_READER_NFC_V) {
        if (!isAvailable()) {
            notifyError(DkError(DkErrorCode.nfcDisabled, "NFC is not available"))
            return
        }
        
        try {
            nfcAdapter?.enableReaderMode(activity, { tag ->
                handleTagDiscovered(tag)
            }, flags, null)
            logger.info("Reader mode enabled")
        } catch (e: Exception) {
            logger.error("Failed to enable reader mode", e)
            notifyError(DkError(DkErrorCode.nfcDisabled, cause = e))
        }
    }
    
    // 禁用读卡模式
    fun disableReaderMode() {
        try {
            nfcAdapter?.disableReaderMode(activity)
            logger.info("Reader mode disabled")
        } catch (e: Exception) {
            logger.error("Failed to disable reader mode", e)
        }
    }
    
    // 处理Intent
    fun handleIntent(intent: Intent?) {
        if (intent == null) return
        
        when (intent.action) {
            NfcAdapter.ACTION_NDEF_DISCOVERED -> handleNdefIntent(intent)
            NfcAdapter.ACTION_TECH_DISCOVERED -> handleTechIntent(intent)
            NfcAdapter.ACTION_TAG_DISCOVERED -> handleTagIntent(intent)
        }
    }
    
    // 发送APDU命令
    fun sendApdu(tag: Tag, command: ByteArray): ByteArray? {
        return try {
            IsoDep.get(tag)?.use { isoDep ->
                isoDep.timeout = 5000
                isoDep.transceive(command)
            }
        } catch (e: Exception) {
            logger.error("APDU command failed", e)
            telemetry.trackError(
                DkErrorCode.nfcReadFailed.toInt(),
                "APDU failed: ${e.message}"
            )
            null
        }
    }
    
    // 选择数字钥匙应用
    fun selectDigitalKeyApp(tag: Tag): ByteArray? {
        val selectCommand = buildSelectCommand(DIGITAL_KEY_AID)
        return sendApdu(tag, selectCommand)
    }
    
    // 读取数据
    fun readData(tag: Tag, offset: Int, length: Int): ByteArray? {
        val readCommand = buildReadCommand(offset, length)
        return sendApdu(tag, readCommand)
    }
    
    // 写入数据
    fun writeData(tag: Tag, offset: Int, data: ByteArray): Boolean {
        val writeCommand = buildWriteCommand(offset, data)
        val response = sendApdu(tag, writeCommand)
        return response?.let { checkResponse(it) } ?: false
    }
    
    // 获取标签ID
    fun getTagId(tag: Tag): String {
        return tag.id.toHexString()
    }
    
    // 获取标签技术
    fun getTagTechList(tag: Tag): List<String> {
        return tag.techList.toList()
    }
    
    // 释放资源
    fun release() {
        disableForegroundDispatch()
        disableReaderMode()
        listeners.clear()
    }
    
    // 私有方法
    
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
        telemetry.trackError(error.code.toInt(), error.message)
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
