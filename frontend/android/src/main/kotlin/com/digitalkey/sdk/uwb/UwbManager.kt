// UwbManager.kt - UWB管理模块
package com.digitalkey.sdk.uwb

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat
import com.digitalkey.sdk.error.DkError
import com.digitalkey.sdk.error.DkErrorCode
import com.digitalkey.sdk.logger.DkLogger
import com.digitalkey.sdk.telemetry.DkTelemetry

// UWB测距事件
data class UwbRangingEvent(
    val timestamp: Long,
    val azimuth: Float?,
    val elevation: Float?,
    val distance: Float?,
    val confidence: Float?,
    val peerAddress: String?,
    val status: Int
)

// UWB事件监听器
interface UwbEventListener {
    fun onRangingResult(event: UwbRangingEvent)
    fun onPeerDiscovered(peerAddress: String)
    fun onPeerLost(peerAddress: String)
    fun onError(error: DkError)
}

// UWB管理器
class UwbManager(private val context: Context) {
    
    private val logger = DkLogger.getLogger("UwbManager")
    private val telemetry = DkTelemetry
    
    private val listeners = mutableListOf<UwbEventListener>()
    private var rangingSession: Any? = null
    private var isRanging = false
    
    companion object {
        private const val TAG = "UwbManager"
        const val STATUS_OK = 0
        const val STATUS_ERROR = 1
        const val STATUS_SESSION_ERROR = 2
    }
    
    init {
        logger.info("UWB Manager initialized, available: ${isAvailable()}")
    }
    
    // 添加监听器
    fun addListener(listener: UwbEventListener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener)
        }
    }
    
    // 移除监听器
    fun removeListener(listener: UwbEventListener) {
        listeners.remove(listener)
    }
    
    // 检查UWB是否可用
    fun isAvailable(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            context.packageManager.hasSystemFeature(PackageManager.FEATURE_UWB)
        } else {
            // UWB not available before Android 13
            false
        }
    }
    
    // 检查权限
    fun hasPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            ContextCompat.checkSelfPermission(
                context,
                Manifest.permission.UWB_RANGING
            ) == PackageManager.PERMISSION_GRANTED
        } else {
            false
        }
    }
    
    // 开始测距
    fun startRanging() {
        if (!isAvailable()) {
            notifyError(DkError(DkErrorCode.uwbDisabled, "UWB is not available on this device"))
            return
        }
        
        if (!hasPermission()) {
            notifyError(DkError(DkErrorCode.uwbDisabled, "UWB permission not granted"))
            return
        }
        
        if (isRanging) {
            logger.warn("Already ranging")
            return
        }
        
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                startRangingApi33()
            }
        } catch (e: Exception) {
            logger.error("Failed to start ranging", e)
            notifyError(DkError(DkErrorCode.uwbRangingFailed, cause = e))
        }
    }
    
    // 停止测距
    fun stopRanging() {
        if (!isRanging) return
        
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                stopRangingApi33()
            }
            isRanging = false
            logger.info("UWB ranging stopped")
            
            telemetry.track("uwb_ranging_stop", emptyMap())
        } catch (e: Exception) {
            logger.error("Failed to stop ranging", e)
        }
    }
    
    // 获取当前距离
    fun getCurrentDistance(): Float? {
        // This would be updated from the ranging callbacks
        return null
    }
    
    // 释放资源
    fun release() {
        stopRanging()
        listeners.clear()
    }
    
    // Android 13+ API
    @Suppress("DEPRECATION")
    private fun startRangingApi33() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            try {
                val uwbManager = context.getSystemService(Context.UWB_SERVICE) as? android.uwb.UwbManager
                
                if (uwbManager != null) {
                    val session = uwbManager.prepareSession(
                        android.uwb.UwbConfig.builder()
                            .setDeviceRole(android.uwb.UwbConfig.DEVICE_ROLE_CONTROLLER)
                            .setRangingMethod(android.uwb.UwbConfig.RANGING_METHOD_UWB)
                            .build()
                    )
                    
                    rangingSession = session
                    
                    session.registerRangeCallback(object : android.uwb.UwbManager.RangeCallback {
                        override fun onPeerJoined(peer: android.uwb.UwbDevice) {
                            logger.info("UWB peer joined: ${peer.bluetoothAddress}")
                            telemetry.track("uwb_peer_joined", mapOf(
                                "peer_address" to peer.bluetoothAddress
                            ))
                            listeners.forEach { it.onPeerDiscovered(peer.bluetoothAddress) }
                        }
                        
                        override fun onPeerDeparted(peer: android.uwb.UwbDevice) {
                            logger.info("UWB peer departed: ${peer.bluetoothAddress}")
                            telemetry.track("uwb_peer_lost", mapOf(
                                "peer_address" to peer.bluetoothAddress
                            ))
                            listeners.forEach { it.onPeerLost(peer.bluetoothAddress) }
                        }
                        
                        override fun onRangeReport(report: android.uwb.UwbRangeSessionNotification) {
                            for (i in 0 until report.rangeDataReports.size) {
                                val rangeData = report.rangeDataReports[i]
                                processRangeData(rangeData)
                            }
                        }
                        
                        override fun onSessionStarted(params: android.uwb.UwbOperationParameters) {
                            isRanging = true
                            logger.info("UWB ranging session started")
                            telemetry.track("uwb_ranging_start", emptyMap())
                        }
                        
                        override fun onSessionStopped(params: android.uwb.UwbOperationParameters) {
                            isRanging = false
                            logger.info("UWB ranging session stopped")
                        }
                        
                        override fun onError(reason: Int, sendStatus: Int) {
                            logger.error("UWB error: reason=$reason, sendStatus=$sendStatus")
                            telemetry.trackError(
                                DkErrorCode.uwbRangingFailed.toInt(),
                                "UWB ranging error: $reason"
                            )
                            notifyError(DkError(DkErrorCode.uwbRangingFailed, "UWB error: $reason"))
                        }
                    }, context.mainExecutor)
                    
                    session.start(params = null)
                }
            } catch (e: Exception) {
                logger.error("Failed to start UWB ranging", e)
                notifyError(DkError(DkErrorCode.uwbRangingFailed, cause = e))
            }
        }
    }
    
    @Suppress("DEPRECATION")
    private fun stopRangingApi33() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            (rangingSession as? android.uwb.UwbRangeSession)?.close()
            rangingSession = null
        }
    }
    
    @Suppress("DEPRECATION")
    private fun processRangeData(rangeData: android.uwb.UwbRangeDataReport) {
        val peer = rangeData.rangeData.peerDevice
        
        val event = UwbRangingEvent(
            timestamp = System.currentTimeMillis(),
            azimuth = if (rangeData.azimuth != null) Math.toDegrees(rangeData.azimuth!!.toDouble()).toFloat() else null,
            elevation = if (rangeData.elevation != null) Math.toDegrees(rangeData.elevation!!.toDouble()).toFloat() else null,
            distance = rangeData.distance,
            confidence = rangeData.distanceStandardDeviation?.let { calculateConfidence(it) },
            peerAddress = peer?.bluetoothAddress,
            status = STATUS_OK
        )
        
        // Log and track telemetry
        logger.debug("UWB ranging: distance=${event.distance}m, azimuth=${event.azimuth}°, elevation=${event.elevation}°")
        
        telemetry.track("uwb_ranging", mapOf(
            "distance_mm" to ((event.distance ?: 0f) * 1000).toInt(),
            "azimuth_deg" to (event.azimuth ?: 0f),
            "elevation_deg" to (event.elevation ?: 0f),
            "confidence" to (event.confidence ?: 0f)
        ))
        
        // Notify listeners
        listeners.forEach { it.onRangingResult(event) }
    }
    
    private fun calculateConfidence(stdDev: Float): Float {
        // Convert standard deviation to confidence level (0-1)
        // Lower stdDev means higher confidence
        val maxStdDev = 1.0f // meters
        return (1.0f - (stdDev.coerceIn(0f, maxStdDev) / maxStdDev))
    }
    
    private fun notifyError(error: DkError) {
        logger.error(error.message, error)
        telemetry.trackError(error.code.toInt(), error.message)
        listeners.forEach { it.onError(error) }
    }
    
    private fun Int.toInt(): Int = this
}

// UWB测距结果处理
object UwbRangingProcessor {
    
    // 计算解锁距离阈值
    const val UNLOCK_DISTANCE_METERS = 0.5f
    
    // 计算闭锁距离阈值
    const val LOCK_DISTANCE_METERS = 2.0f
    
    // 计算启动引擎距离阈值
    const val START_DISTANCE_METERS = 0.3f
    
    /**
     * 根据UWB测距结果判断车辆操作
     */
    fun determineAction(distance: Float?): VehicleAction {
        if (distance == null) return VehicleAction.NONE
        
        return when {
            distance <= START_DISTANCE_METERS -> VehicleAction.START_ENGINE
            distance <= UNLOCK_DISTANCE_METERS -> VehicleAction.UNLOCK
            distance <= LOCK_DISTANCE_METERS -> VehicleAction.LOCK
            else -> VehicleAction.NONE
        }
    }
}

// 车辆操作
enum class VehicleAction {
    NONE,
    UNLOCK,
    LOCK,
    START_ENGINE,
    STOP_ENGINE
}
