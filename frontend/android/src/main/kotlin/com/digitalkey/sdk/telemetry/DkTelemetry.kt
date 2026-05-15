// DkTelemetry.kt - 统一埋点接口 - Android SDK端
// 三端统一埋点规范

package com.digitalkey.sdk.telemetry

import android.content.Context
import android.os.Build
import android.provider.Settings
import com.digitalkey.sdk.logger.DkLogger
import com.digitalkey.sdk.logger.LogField
import org.json.JSONObject
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

/**
 * 事件类型枚举
 */
object EventType {
    // App事件
    const val APP_LAUNCH = "app_launch"
    const val APP_BACKGROUND = "app_background"
    const val SDK_INIT = "sdk_init"
    const val SDK_ERROR = "sdk_error"
    
    // 用户事件
    const val USER_LOGIN = "user_login"
    const val USER_LOGOUT = "user_logout"
    
    // 设备事件
    const val DEVICE_BIND_START = "device_bind_start"
    const val DEVICE_BIND_SUCCESS = "device_bind_success"
    const val DEVICE_BIND_FAILED = "device_bind_failed"
    const val DEVICE_UNBIND = "device_unbind"
    
    // 密钥事件
    const val KEY_CREATE = "key_create"
    const val KEY_DELETE = "key_delete"
    const val KEY_REFRESH = "key_refresh"
    const val KEY_USE = "key_use"
    const val KEY_EXPIRED = "key_expired"
    
    // 车辆事件
    const val VEHICLE_UNLOCK = "vehicle_unlock"
    const val VEHICLE_LOCK = "vehicle_lock"
    const val VEHICLE_START = "vehicle_start"
    const val VEHICLE_STOP = "vehicle_stop"
    
    // 分享事件
    const val SHARE_CREATE = "share_create"
    const val SHARE_ACCEPT = "share_accept"
    const val SHARE_REVOKE = "share_revoke"
    
    // 通道事件
    const val BLE_SCAN = "ble_scan"
    const val BLE_CONNECT = "ble_connect"
    const val BLE_DISCONNECT = "ble_disconnect"
    const val NFC_TAP = "nfc_tap"
    const val UWB_RANGING = "uwb_ranging"
    const val CHANNEL_SWITCH = "channel_switch"
    
    // 协议事件
    const val PROTOCOL_MSG = "protocol_msg"
    
    // 安全事件
    const val SECURITY_ALERT = "security_alert"
}

/**
 * 事件来源
 */
enum class EventSource {
    SDK, TCU, CLOUD
}

/**
 * 埋点事件
 */
data class TelemetryEvent(
    val eventId: String = UUID.randomUUID().toString(),
    val eventType: String,
    val timestamp: Long = System.currentTimeMillis(),
    val source: EventSource = EventSource.SDK,
    val sessionId: String? = null,
    val userId: String? = null,
    val deviceId: String? = null,
    val vehicleId: String? = null,
    val keyId: String? = null,
    val traceId: String? = null,
    val properties: Map<String, Any?> = emptyMap(),
    val context: Map<String, Any?> = emptyMap()
) {
    fun toJson(): JSONObject = JSONObject().apply {
        put("event_id", eventId)
        put("event_type", eventType)
        put("timestamp", timestamp)
        put("source", source.name)
        sessionId?.let { put("session_id", it) }
        userId?.let { put("user_id", it) }
        deviceId?.let { put("device_id", it) }
        vehicleId?.let { put("vehicle_id", it) }
        keyId?.let { put("key_id", it) }
        traceId?.let { put("trace_id", it) }
        put("properties", JSONObject(properties.filterValues { it != null }))
        if (context.isNotEmpty()) {
            put("context", JSONObject(context))
        }
    }
    
    fun toMap(): Map<String, Any?> = buildMap {
        put("event_id", eventId)
        put("event_type", eventType)
        put("timestamp", timestamp)
        put("source", source.name)
        sessionId?.let { put("session_id", it) }
        userId?.let { put("user_id", it) }
        deviceId?.let { put("device_id", it) }
        vehicleId?.let { put("vehicle_id", it) }
        keyId?.let { put("key_id", it) }
        traceId?.let { put("trace_id", it) }
        put("properties", properties)
    }
}

/**
 * 埋点配置
 */
data class TelemetryConfig(
    val appContext: Context,
    var endpoint: String = "",
    var batchSize: Int = 10,
    var flushIntervalMs: Long = 5000,
    var enableDebug: Boolean = false,
    var sampleRate: Float = 1.0f  // 0.0-1.0
)

/**
 * 埋点接口
 */
interface Telemetry {
    fun track(eventType: String, properties: Map<String, Any?> = emptyMap())
    fun trackError(code: Int, errorMessage: String, context: Map<String, Any?> = emptyMap())
    fun setUser(userId: String?)
    fun setDevice(deviceId: String?)
    fun setVehicle(vehicleId: String?)
    fun setTraceId(traceId: String?)
    fun flush()
}

/**
 * 统一埋点实现
 */
object DkTelemetry : Telemetry {
    
    private lateinit var config: TelemetryConfig
    private var userId: String? = null
    private var deviceId: String? = null
    private var vehicleId: String? = null
    private var traceId: String? = null
    private var sessionId: String? = null
    
    private val eventQueue = ConcurrentHashMap.newKeySet<TelemetryEvent>()
    private lateinit var executor: ScheduledExecutorService
    private val eventHandlers = ConcurrentHashMap.newKeySet<EventHandler>()
    
    /**
     * 初始化
     */
    fun init(cfg: TelemetryConfig) {
        config = cfg
        
        // 生成设备ID
        deviceId = getDeviceId(cfg.appContext)
        
        // 生成会话ID
        sessionId = UUID.randomUUID().toString()
        
        // 启动定时刷新
        executor = Executors.newSingleThreadScheduledExecutor()
        executor.scheduleAtFixedRate(
            { flush() },
            cfg.flushIntervalMs,
            cfg.flushIntervalMs,
            TimeUnit.MILLISECONDS
        )
        
        DkLogger.i("TELEMETRY", "Telemetry initialized, device_id=$deviceId")
    }
    
    /**
     * 获取设备ID
     */
    private fun getDeviceId(context: Context): String {
        return Settings.Secure.getString(
            context.contentResolver,
            Settings.Secure.ANDROID_ID
        ) ?: UUID.randomUUID().toString()
    }
    
    /**
     * 埋点
     */
    override fun track(eventType: String, properties: Map<String, Any?>) {
        // 采样判断
        if (Math.random() > config.sampleRate) return
        
        val event = TelemetryEvent(
            eventType = eventType,
            source = EventSource.SDK,
            sessionId = sessionId,
            userId = userId,
            deviceId = deviceId,
            vehicleId = vehicleId,
            traceId = traceId,
            properties = properties,
            context = getCommonContext()
        )
        
        eventQueue.add(event)
        
        if (config.enableDebug) {
            DkLogger.d("TELEMETRY", "Event tracked: $eventType", 
                DkLogger.field("event_id", event.eventId))
        }
        
        // 达到批量大小立即刷新
        if (eventQueue.size >= config.batchSize) {
            flush()
        }
        
        // 通知处理器
        notifyHandlers(event)
    }
    
    /**
     * 错误埋点
     */
    override fun trackError(code: Int, errorMessage: String, context: Map<String, Any?>) {
        val props = context.toMutableMap()
        props["error_code"] = code
        props["error_code_hex"] = "0x%04X".format(code)
        props["error_message"] = errorMessage
        
        track(EventType.SDK_ERROR, props)
    }
    
    /**
     * 设置用户ID
     */
    override fun setUser(userId: String?) {
        this.userId = userId
    }
    
    /**
     * 设置设备ID
     */
    override fun setDevice(deviceId: String?) {
        this.deviceId = deviceId
    }
    
    /**
     * 设置车辆ID
     */
    override fun setVehicle(vehicleId: String?) {
        this.vehicleId = vehicleId
    }
    
    /**
     * 设置追踪ID
     */
    override fun setTraceId(traceId: String?) {
        this.traceId = traceId
    }
    
    /**
     * 刷新
     */
    override fun flush() {
        if (eventQueue.isEmpty()) return
        
        val events = eventQueue.toList()
        eventQueue.clear()
        
        // 发送到后端
        sendEvents(events)
    }
    
    /**
     * 发送事件到后端
     */
    private fun sendEvents(events: List<TelemetryEvent>) {
        if (config.endpoint.isEmpty()) {
            // 没有配置endpoint，打印日志
            if (config.enableDebug) {
                events.forEach { event ->
                    DkLogger.d("TELEMETRY", event.toJson().toString())
                }
            }
            return
        }
        
        // TODO: 实现实际的网络发送
        // 这里可以添加OkHttp/Retrofit等网络库的调用
        
        if (config.enableDebug) {
            DkLogger.i("TELEMETRY", "Sent ${events.size} events to ${config.endpoint}")
        }
    }
    
    /**
     * 获取通用上下文
     */
    private fun getCommonContext(): Map<String, Any?> {
        return buildMap {
            put("app_version", getAppVersion())
            put("os_version", Build.VERSION.RELEASE)
            put("sdk_version", getSdkVersion())
            put("device_model", Build.MODEL)
            put("manufacturer", Build.MANUFACTURER)
        }
    }
    
    private fun getAppVersion(): String {
        return try {
            if (::config.isInitialized) {
                val pm = config.appContext.packageManager
                val pkgInfo = pm.getPackageInfo(config.appContext.packageName, 0)
                pkgInfo.versionName ?: "unknown"
            } else "unknown"
        } catch (e: Exception) {
            "unknown"
        }
    }
    
    private fun getSdkVersion(): String = "1.0.0"
    
    private fun notifyHandlers(event: TelemetryEvent) {
        eventHandlers.forEach { handler ->
            try {
                handler.onEvent(event)
            } catch (e: Exception) {
                // 忽略处理器异常
            }
        }
    }
    
    //=========================================================================
    // 便捷方法
    //=========================================================================
    
    fun trackKeyUse(keyId: String, vehicleId: String, channel: String, durationMs: Long) {
        track(EventType.KEY_USE, mapOf(
            "key_id" to keyId,
            "vehicle_id" to vehicleId,
            "channel_type" to channel,
            "duration_ms" to durationMs
        ))
    }
    
    fun trackVehicleCommand(action: String, vehicleId: String, result: String, errorCode: Int?) {
        val props = mutableMapOf<String, Any?>(
            "action" to action,
            "vehicle_id" to vehicleId,
            "result" to result
        )
        errorCode?.let { props["error_code"] = it }
        
        track(EventType.PROTOCOL_MSG, props)
    }
    
    fun trackSecurityEvent(eventType: String, threatLevel: Int, details: Map<String, Any?>) {
        val props = details.toMutableMap()
        props["security_event_type"] = eventType
        props["threat_level"] = threatLevel
        
        track(EventType.SECURITY_ALERT, props)
    }
    
    fun trackBleConnect(deviceId: String, macAddress: String, mtu: Int, success: Boolean) {
        track(EventType.BLE_CONNECT, mapOf(
            "device_id" to deviceId,
            "mac_address" to macAddress,
            "mtu" to mtu,
            "success" to success
        ))
    }
    
    fun trackNfcTap(tagId: String, vehicleId: String, success: Boolean) {
        track(EventType.NFC_TAP, mapOf(
            "nfc_tag_id" to tagId,
            "vehicle_id" to vehicleId,
            "success" to success
        ))
    }
    
    fun trackUwbRanging(vehicleId: String, distanceMm: Int, accuracyMm: Int) {
        track(EventType.UWB_RANGING, mapOf(
            "vehicle_id" to vehicleId,
            "distance_mm" to distanceMm,
            "accuracy_mm" to accuracyMm
        ))
    }
    
    //=========================================================================
    // 事件处理器
    //=========================================================================
    
    interface EventHandler {
        fun onEvent(event: TelemetryEvent)
    }
    
    fun addHandler(handler: EventHandler) {
        eventHandlers.add(handler)
    }
    
    fun removeHandler(handler: EventHandler) {
        eventHandlers.remove(handler)
    }
    
    /**
     * 释放资源
     */
    fun release() {
        if (::executor.isInitialized) {
            executor.shutdown()
        }
        flush()
    }
}
