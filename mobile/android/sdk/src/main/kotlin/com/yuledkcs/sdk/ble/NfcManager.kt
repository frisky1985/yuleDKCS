package com.yuledkcs.sdk.ble

import android.app.Activity
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import android.nfc.Tag
import android.nfc.tech.IsoDep
import android.nfc.tech.MifareClassic
import android.nfc.tech.Ndef
import android.os.Build
import kotlinx.coroutines.CancellableContinuation
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import java.nio.charset.StandardCharsets
import kotlin.coroutines.resume

/**
 * NFC 车辆信息
 */
data class NfcVehicleInfo(
    val vehicleId: String,
    val tagId: String,
    val protocolType: Int
)

/**
 * NFC 指令类型
 */
enum class NfcCommandType(val value: Byte) {
    UNLOCK(0x01),
    LOCK(0x02),
    START_ENGINE(0x03)
}

/**
 * NFC 管理器接口
 *
 * 实现说明:
 * - Android: `AndroidNfcManager` — NfcAdapter + ISO-DEP 真实实现 (编译级)
 * - iOS: `YDKCoreNFCManager` — CoreNFC (见 YDKNFCManager.swift)
 * - 真机依赖: NFC 硬件 + `android.permission.NFC`; 无硬件走明确异常
 */
interface NfcManager {
    /** 读取车辆 NFC 标签 */
    suspend fun readVehicleTag(): NfcVehicleInfo

    /** 通过 NFC 通道发送指令 */
    suspend fun sendCommandViaNfc(command: NfcCommandType)
}

// ─────────────────────────────────────────────────────────────────────────────
// NFC 异常
// ─────────────────────────────────────────────────────────────────────────────

/** 设备无 NFC 硬件（NfcAdapter 为 null） */
class NfcUnavailableException(message: String) : Exception(message)

/** NFC 已关闭, 需用户在系统设置开启 */
class NfcDisabledException(message: String) : Exception(message)

/** 标签不支持 ISO-DEP / MifareClassic 技术 */
class NfcTagNotSupportedException(message: String) : Exception(message)

/** 车辆 NFC 指令执行失败（SW 非 0x9000 / 通信错误） */
class NfcCommandException(message: String) : Exception(message)

// ─────────────────────────────────────────────────────────────────────────────
// 指令构建（纯逻辑, 不依赖 android.*, JVM 单测可跑; 与 iOS YDKNFCApdu 字节一致）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * NFC APDU / 命令字节构建与解析 — 双端一致契约
 *
 * 与 iOS `YDKNFCApdu` 保持同一字节映射（Python 交叉验证）。
 * 车辆标签安全模块约定的专有指令 (ISO 7816-4):
 *   [CLA=0x80][INS=0xD2][P1=指令码][P2=0x00][Lc=0x00][Le=0x00]
 * 响应末两字节为 SW1SW2, 0x9000 表示成功。
 */
object NfcCommandBuilder {

    /** 成功状态字 SW1SW2 = 0x9000 */
    const val SW_SUCCESS: Int = 0x9000

    /** 构建车辆控制指令 APDU（P1 携带指令码, 与 `NfcCommandType` 对齐） */
    fun buildCommand(command: NfcCommandType): ByteArray =
        byteArrayOf(0x80.toByte(), 0xD2.toByte(), command.value, 0x00, 0x00, 0x00)

    /** 构建读取车辆记录 APDU（ISO 7816-4 READ BINARY, 读 64 字节） */
    fun buildReadVehicleRecord(): ByteArray =
        byteArrayOf(0x00, 0xB0.toByte(), 0x00, 0x00, 0x40)

    /** 构建 MiFare 读取 APDU（READ block 4, NDEF 起始块） */
    fun buildReadMifareBlock(): ByteArray =
        byteArrayOf(0x30, 0x04)

    /** tagId 十六进制格式化（大写, 无分隔符; 与 iOS `YDKNFCApdu.tagIdHex` 一致） */
    fun tagIdHex(id: ByteArray): String =
        id.joinToString("") { b -> "%02X".format(b.toInt() and 0xFF) }

    /** 响应成功判定: 末两字节 == 0x90 0x00 */
    fun isSuccess(response: ByteArray): Boolean =
        response.size >= 2 &&
            (response[response.size - 2].toInt() and 0xFF) == 0x90 &&
            (response[response.size - 1].toInt() and 0xFF) == 0x00

    /** 从响应解析 vehicleId: 去 SW1SW2 + 去尾零/空白后按 UTF-8 解码 */
    fun parseVehicleId(response: ByteArray): String? {
        var bytes = response.toMutableList()
        if (bytes.size >= 2) {
            bytes = bytes.subList(0, bytes.size - 2).toMutableList() // 去掉状态字 SW1SW2
        }
        while (bytes.isNotEmpty() && (bytes.last() == 0x00.toByte() || bytes.last() == 0x20.toByte())) {
            bytes.removeAt(bytes.size - 1)
        }
        if (bytes.isEmpty()) return null
        val text = String(bytes.toByteArray(), StandardCharsets.UTF_8).trim()
        return text.ifEmpty { null }
    }

    /** 供前台调度注册的 tech-list（manifest tech-filter XML 见 NFC-INTEGRATION.md） */
    val TECH_LIST: Array<String> = arrayOf(
        "android.nfc.tech.IsoDep",
        "android.nfc.tech.Ndef",
        "android.nfc.tech.MifareClassic",
        "android.nfc.tech.NfcA"
    )
}

// ─────────────────────────────────────────────────────────────────────────────
// AndroidNfcManager — NfcAdapter + ISO-DEP 真实实现（编译级）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Android 真实 NFC 管理器 — 备用解锁通道
 *
 * 平台事实 (Android 官方):
 * - `NfcAdapter.getDefaultAdapter(context)`: 无 NFC 硬件返回 null; `isEnabled` 为 false 表示系统 NFC 关闭
 * - 标签获取两条路径:
 *   1. 前台调度: `enableForegroundDispatch(activity)` — 标签经宿主 `onNewIntent` 到达,
 *      宿主调用 `onTagDispatched(intent)` 交给 SDK
 *   2. Reader 模式: `enableReaderMode(activity, ...)` — 回调直接携带 `Tag` 对象
 * - 指令通道: `IsoDep.transceive(apdu)` (ISO 14443-4 / ISO-DEP); MiFare Classic 兜底
 * - NDEF 读取: `Ndef.get(tag).ndefMessage`; 文本记录按 NFC Forum Text RTD 解码
 *
 * 使用示例见 docs/sdk/NFC-INTEGRATION.md; 无硬件/未开启 NFC 抛明确异常。
 */
class AndroidNfcManager(private val context: Context) : NfcManager {

    private val adapter: NfcAdapter? = NfcAdapter.getDefaultAdapter(context)

    /** 设备是否有 NFC 硬件 */
    val isNfcAvailable: Boolean get() = adapter != null

    /** 系统 NFC 是否开启 */
    val isNfcEnabled: Boolean get() = adapter?.isEnabled == true

    /** 等待中的标签续体（同一时刻只支持一个等待方） */
    private var tagWaiter: CancellableContinuation<Tag>? = null

    /** 最后一个收到的标签（非等待场景供同步查询） */
    @Volatile
    private var lastTag: Tag? = null

    // ── NfcManager 接口 ──

    /** 读取车辆 NFC 标签: 等待一次标签到达 → NDEF/记录解析 → vehicleId 兜底 tagId */
    override suspend fun readVehicleTag(): NfcVehicleInfo {
        requireEnabledAdapter()
        val tag = awaitTag()
        return withContext(Dispatchers.IO) { readVehicleInfo(tag) }
    }

    /** 通过 NFC 通道发送指令: 等待一次标签到达 → ISO-DEP transceive → SW 校验 */
    override suspend fun sendCommandViaNfc(command: NfcCommandType) {
        requireEnabledAdapter()
        val tag = awaitTag()
        withContext(Dispatchers.IO) {
            val response = transceive(tag, NfcCommandBuilder.buildCommand(command))
            if (!NfcCommandBuilder.isSuccess(response)) {
                val sw = response.takeLast(2).joinToString("") { "%02X".format(it.toInt() and 0xFF) }
                throw NfcCommandException("车辆 NFC 指令执行失败, SW=$sw")
            }
        }
    }

    // ── 前台调度 / Reader 模式 ──

    /**
     * 启用 Reader 模式（推荐: 回调直达 Tag, 无需 intent 路由）。
     * 宿主在 `onResume` 调用, `onPause` 调用 [disableReaderMode]。
     */
    fun enableReaderMode(
        activity: Activity,
        flags: Int = NfcAdapter.FLAG_READER_NFC_A or
            NfcAdapter.FLAG_READER_NFC_B or
            NfcAdapter.FLAG_READER_NFC_F or
            NfcAdapter.FLAG_READER_NFC_V
    ) {
        requireEnabledAdapter().enableReaderMode(activity, readerCallback, flags, null)
    }

    fun disableReaderMode(activity: Activity) {
        adapter?.disableReaderMode(activity)
    }

    /**
     * 启用前台调度（宿主需在 `onNewIntent`/`onResume` 调用 [onTagDispatched] 路由标签）。
     * 宿主在 `onResume` 调用, `onPause` 调用 [disableForegroundDispatch]。
     */
    fun enableForegroundDispatch(activity: Activity) {
        val nfc = requireEnabledAdapter()
        val pendingIntent = PendingIntent.getActivity(
            activity, 0,
            Intent(activity, activity.javaClass).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            pendingIntentFlags()
        )
        val filters = arrayOf(
            IntentFilter(NfcAdapter.ACTION_NDEF_DISCOVERED).apply { addDataType("*/*") },
            IntentFilter(NfcAdapter.ACTION_TECH_DISCOVERED),
            IntentFilter(NfcAdapter.ACTION_TAG_DISCOVERED)
        )
        nfc.enableForegroundDispatch(activity, pendingIntent, filters, arrayOf(NfcCommandBuilder.TECH_LIST))
    }

    fun disableForegroundDispatch(activity: Activity) {
        adapter?.disableForegroundDispatch(activity)
    }

    /**
     * 标签入口（intent 路由）: 宿主在 `onNewIntent` / `onResume` 中调用。
     * 有等待方（readVehicleTag/sendCommandViaNfc 挂起中）→ 消费并返回 null;
     * 无等待方 → 同步解析返回 NfcVehicleInfo（宿主主动读）。
     */
    fun onTagDispatched(intent: Intent): NfcVehicleInfo? {
        val tag = tagFromIntent(intent) ?: return null
        return handleTag(tag)
    }

    /**
     * 标签入口（Reader 模式回调）: 本类内部已注册 [readerCallback];
     * 宿主若自建 ReaderCallback 也可调用此方法。
     */
    fun onTagDiscovered(tag: Tag): NfcVehicleInfo? = handleTag(tag)

    /** 同步解析 intent 中的标签（不等待） */
    fun readTagFromIntent(intent: Intent): NfcVehicleInfo? {
        val tag = tagFromIntent(intent) ?: return null
        return readVehicleInfo(tag)
    }

    // ── 内部 ──

    private val readerCallback = NfcAdapter.ReaderCallback { tag -> onTagDiscovered(tag) }

    private fun handleTag(tag: Tag): NfcVehicleInfo? {
        lastTag = tag
        val waiter = tagWaiter
        if (waiter != null) {
            tagWaiter = null
            waiter.resume(tag) // 被 readVehicleTag / sendCommandViaNfc 消费
            return null
        }
        return readVehicleInfo(tag)
    }

    /** 挂起直到一个标签到达（Reader 模式回调或前台调度 intent 路由） */
    private suspend fun awaitTag(): Tag = suspendCancellableCoroutine { cont ->
        tagWaiter = cont
        cont.invokeOnCancellation { tagWaiter = null }
        // 若已有标签先到达（非等待场景）则立即交付
        lastTag?.let { tag ->
            tagWaiter = null
            cont.resume(tag)
        }
    }

    /** 从 Tag 解析车辆信息: NDEF 文本记录优先, 兜底 tagId */
    private fun readVehicleInfo(tag: Tag): NfcVehicleInfo {
        val tagId = NfcCommandBuilder.tagIdHex(tag.id)
        val vehicleId = readNdefVehicleId(tag) ?: tagId
        // protocolType: 1 = ISO 14443-4 (ISO-DEP) / 2 = MiFare Classic
        return NfcVehicleInfo(vehicleId = vehicleId, tagId = tagId, protocolType = 1)
    }

    /** NDEF 读取 + NFC Forum Text RTD 解码; 失败返回 null */
    private fun readNdefVehicleId(tag: Tag): String? = try {
        val ndef = Ndef.get(tag) ?: return null
        ndef.connect()
        try {
            val message: NdefMessage = ndef.ndefMessage ?: return null
            val payload = message.records.firstOrNull()?.payload ?: return null
            val text = decodeNdefText(payload) ?: String(payload, StandardCharsets.UTF_8)
            text.trim().ifEmpty { null }
        } finally {
            runCatching { ndef.close() }
        }
    } catch (e: Exception) {
        null
    }

    /**
     * NFC Forum Text RTD 解码:
     * 首字节 bit7 = UTF-16 标志, bit0-5 = 语言码长度; 随后为语言码 + 文本。
     */
    private fun decodeNdefText(payload: ByteArray): String? {
        if (payload.isEmpty()) return null
        val status = payload[0].toInt() and 0xFF
        val langLen = status and 0x3F
        if (payload.size < 1 + langLen) return null
        val textBytes = payload.copyOfRange(1 + langLen, payload.size)
        val utf16 = (status and 0x80) != 0
        return if (utf16) String(textBytes, StandardCharsets.UTF_16) else String(textBytes, StandardCharsets.UTF_8)
    }

    /** ISO-DEP transceive; MiFare Classic 兜底; 均不支持抛明确异常 */
    private fun transceive(tag: Tag, apdu: ByteArray): ByteArray {
        val isoDep = IsoDep.get(tag)
        if (isoDep != null) {
            isoDep.connect()
            try {
                return isoDep.transceive(apdu)
            } finally {
                runCatching { isoDep.close() }
            }
        }
        val mifare = MifareClassic.get(tag)
        if (mifare != null) {
            mifare.connect()
            try {
                return mifare.transceive(apdu)
            } finally {
                runCatching { mifare.close() }
            }
        }
        throw NfcTagNotSupportedException("标签不支持 ISO-DEP / MifareClassic, techList=${tag.techList.joinToString()}")
    }

    /** 解析 intent 中的 Tag（API 33+ 走类型化 getParcelableExtra） */
    @Suppress("DEPRECATION")
    private fun tagFromIntent(intent: Intent): Tag? =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG, Tag::class.java)
        } else {
            intent.getParcelableExtra(NfcAdapter.EXTRA_TAG)
        }

    private fun pendingIntentFlags(): Int {
        var flags = PendingIntent.FLAG_UPDATE_CURRENT
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            flags = flags or PendingIntent.FLAG_MUTABLE
        }
        return flags
    }

    /** 无硬件/未开启 NFC 抛明确异常 */
    private fun requireEnabledAdapter(): NfcAdapter {
        val nfc = adapter ?: throw NfcUnavailableException("设备无 NFC 硬件 (NfcAdapter 为 null)")
        if (!nfc.isEnabled) throw NfcDisabledException("NFC 已关闭, 请在系统设置中开启")
        return nfc
    }
}
