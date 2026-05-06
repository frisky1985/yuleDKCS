// DkLogger.kt - 统一日志接口 - Android SDK端
// 三端统一日志规范

package com.digitalkey.sdk.logger

import android.util.Log
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap

/**
 * 日志级别
 */
enum class LogLevel(val priority: Int, val tag: String) {
    VERBOSE(Log.VERBOSE, "V"),
    DEBUG(Log.DEBUG, "D"),
    INFO(Log.INFO, "I"),
    WARN(Log.WARN, "W"),
    ERROR(Log.ERROR, "E")
}

/**
 * 日志模块标签
 */
object LogTag {
    const val INIT = "INIT"
    const val KEYMGR = "KEYMGR"
    const val AUTH = "AUTH"
    const val BLE = "BLE"
    const val NFC = "NFC"
    const val UWB = "UWB"
    const val SEC = "SEC"
    const val TRANSPORT = "TRANSPORT"
    const val PROTO = "PROTO"
    const val SERVICE = "SERVICE"
    const val ADAPTER = "ADAPTER"
}

/**
 * 日志配置
 */
data class LoggerConfig(
    var minLevel: LogLevel = LogLevel.DEBUG,
    var enableJson: Boolean = true,
    var enableTimestamp: Boolean = true,
    var enableTraceId: Boolean = true,
    var enableFileLog: Boolean = false,
    var logFilePath: String? = null
)

/**
 * 日志字段
 */
data class LogField(
    val key: String,
    val value: Any?
)

/**
 * 统一日志器
 */
object DkLogger {
    
    private var config = LoggerConfig()
    private var traceId: String? = null
    private val subscribers = ConcurrentHashMap.newKeySet<LogSubscriber>()
    
    /**
     * 配置日志器
     */
    fun configure(cfg: LoggerConfig) {
        config = cfg
    }
    
    /**
     * 设置追踪ID
     */
    fun setTraceId(tid: String?) {
        traceId = tid
    }
    
    /**
     * 获取追踪ID
     */
    fun getTraceId(): String? = traceId
    
    /**
     * 生成追踪ID
     */
    fun generateTraceId(): String {
        val tid = "SDK-${System.currentTimeMillis()}-${(1000..9999).random()}"
        traceId = tid
        return tid
    }
    
    //=========================================================================
    // 基础日志方法
    //=========================================================================
    
    fun v(tag: String, message: String, vararg fields: LogField) {
        log(LogLevel.VERBOSE, tag, message, fields.toList())
    }
    
    fun d(tag: String, message: String, vararg fields: LogField) {
        log(LogLevel.DEBUG, tag, message, fields.toList())
    }
    
    fun i(tag: String, message: String, vararg fields: LogField) {
        log(LogLevel.INFO, tag, message, fields.toList())
    }
    
    fun w(tag: String, message: String, vararg fields: LogField) {
        log(LogLevel.WARN, tag, message, fields.toList())
    }
    
    fun e(tag: String, message: String, throwable: Throwable? = null, vararg fields: LogField) {
        val allFields = fields.toList().toMutableList()
        throwable?.let {
            allFields.add(LogField("error", it.message ?: ""))
            allFields.add(LogField("stack_trace", it.stackTraceToString()))
        }
        log(LogLevel.ERROR, tag, message, allFields)
    }
    
    private fun log(level: LogLevel, tag: String, message: String, fields: List<LogField>) {
        if (level.priority < config.minLevel.priority) return
        
        val timestamp = if (config.enableTimestamp) {
            SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS", Locale.US).format(Date())
        } else null
        
        if (config.enableJson) {
            outputJson(level, tag, message, timestamp, fields)
        } else {
            outputText(level, tag, message, timestamp, fields)
        }
        
        // 通知订阅者
        notifySubscribers(level, tag, message, fields)
    }
    
    private fun outputJson(level: LogLevel, tag: String, message: String, 
                          timestamp: String?, fields: List<LogField>) {
        val json = JSONObject().apply {
            timestamp?.let { put("timestamp", it) }
            put("level", level.tag)
            put("tag", tag)
            put("message", message)
            config.enableTraceId && traceId?.let { put("trace_id", it) }
            if (fields.isNotEmpty()) {
                val fieldsObj = JSONObject()
                fields.forEach { fieldsObj.put(it.key, it.value ?: "null") }
                put("fields", fieldsObj)
            }
        }
        
        // Android Logcat
        Log.println(level.priority, "DK_$tag", json.toString())
    }
    
    private fun outputText(level: LogLevel, tag: String, message: String,
                          timestamp: String?, fields: List<LogField>) {
        val sb = StringBuilder()
        timestamp?.let { sb.append("[$it]") }
        sb.append("[${level.tag}][$tag]")
        config.enableTraceId && traceId?.let { sb.append("[T:$it]") }
        sb.append(" $message")
        fields.forEach { sb.append(" ${it.key}=${it.value}") }
        
        Log.println(level.priority, "DK_$tag", sb.toString())
    }
    
    private fun notifySubscribers(level: LogLevel, tag: String, message: String, 
                                 fields: List<LogField>) {
        subscribers.forEach { subscriber ->
            try {
                subscriber.onLog(level, tag, message, fields, traceId)
            } catch (e: Exception) {
                // 忽略订阅者异常
            }
        }
    }
    
    //=========================================================================
    // 便捷扩展方法
    //=========================================================================
    
    fun keyMgr(message: String, vararg fields: LogField) = i(LogTag.KEYMGR, message, *fields)
    fun auth(message: String, vararg fields: LogField) = i(LogTag.AUTH, message, *fields)
    fun ble(message: String, vararg fields: LogField) = i(LogTag.BLE, message, *fields)
    fun nfc(message: String, vararg fields: LogField) = i(LogTag.NFC, message, *fields)
    fun sec(message: String, vararg fields: LogField) = i(LogTag.SEC, message, *fields)
    fun proto(message: String, vararg fields: LogField) = d(LogTag.PROTO, message, *fields)
    
    // 错误便捷方法
    fun keyMgrE(message: String, err: Throwable? = null, vararg fields: LogField) = 
        e(LogTag.KEYMGR, message, err, *fields)
    fun authE(message: String, err: Throwable? = null, vararg fields: LogField) = 
        e(LogTag.AUTH, message, err, *fields)
    fun bleE(message: String, err: Throwable? = null, vararg fields: LogField) = 
        e(LogTag.BLE, message, err, *fields)
    fun secE(message: String, err: Throwable? = null, vararg fields: LogField) = 
        e(LogTag.SEC, message, err, *fields)
    
    //=========================================================================
    // 字段构建器
    //=========================================================================
    
    companion object {
        fun field(key: String, value: Any?) = LogField(key, value)
        fun traceId(value: String) = LogField("trace_id", value)
        fun errorCode(code: Int) = LogField("error_code", "0x%04X".format(code))
        fun duration(ms: Long) = LogField("duration_ms", ms)
        fun keyId(id: String) = LogField("key_id", id)
        fun vehicleId(id: String) = LogField("vehicle_id", id)
        fun userId(id: String) = LogField("user_id", id)
        fun deviceId(id: String) = LogField("device_id", id)
        fun channel(type: String) = LogField("channel_type", type)
    }
    
    //=========================================================================
    // 订阅者接口
    //=========================================================================
    
    interface LogSubscriber {
        fun onLog(level: LogLevel, tag: String, message: String, 
                 fields: List<LogField>, traceId: String?)
    }
    
    fun subscribe(subscriber: LogSubscriber) {
        subscribers.add(subscriber)
    }
    
    fun unsubscribe(subscriber: LogSubscriber) {
        subscribers.remove(subscriber)
    }
}

// 扩展函数，便于使用
fun keyMgr(message: String, vararg fields: Pair<String, Any?>) =
    DkLogger.keyMgr(message, *fields.map { DkLogger.field(it.first, it.second) }.toTypedArray())

fun auth(message: String, vararg fields: Pair<String, Any?>) =
    DkLogger.auth(message, *fields.map { DkLogger.field(it.first, it.second) }.toTypedArray())

fun ble(message: String, vararg fields: Pair<String, Any?>) =
    DkLogger.ble(message, *fields.map { DkLogger.field(it.first, it.second) }.toTypedArray())

fun sec(message: String, vararg fields: Pair<String, Any?>) =
    DkLogger.sec(message, *fields.map { DkLogger.field(it.first, it.second) }.toTypedArray())

fun keyMgrE(message: String, err: Throwable? = null, vararg fields: Pair<String, Any?>) =
    DkLogger.keyMgrE(message, err, *fields.map { DkLogger.field(it.first, it.second) }.toTypedArray())

fun authE(message: String, err: Throwable? = null, vararg fields: Pair<String, Any?>) =
    DkLogger.authE(message, err, *fields.map { DkLogger.field(it.first, it.second) }.toTypedArray())
