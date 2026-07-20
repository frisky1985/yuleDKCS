package com.digitalkey.wallet.uwb

import android.content.Context
import android.os.Build
import android.util.Log
import androidx.annotation.RequiresApi
import com.digitalkey.wallet.Models

/**
 * UWB测距管理器
 * 使用Android UWB API (Android 13+) 与车端NCJ29D6进行FiRa测距
 * 自动触发: BLE连接后启动UWB测距 → 距离分区 → 触发解锁/闭锁
 */
@RequiresApi(Build.VERSION_CODES.S)
class UwbManager(private val context: Context) {

    companion object {
        private const val TAG = "UwbManager"

        // UWB Channels
        const val CHANNEL_5 = 5
        const val CHANNEL_6 = 6
        const val CHANNEL_8 = 8
        const val CHANNEL_9 = 9

        // Preamble codes
        const val PREAMBLE_CODE_9  = 9
        const val PREAMBLE_CODE_10 = 10
        const val PREAMBLE_CODE_11 = 11
        const val PREAMBLE_CODE_12 = 12

        // Distance zones (mm)
        const val ZONE_FAR_THRESHOLD      = 20000   // 20m
        const val ZONE_MID_THRESHOLD      = 10000   // 10m
        const val ZONE_NEAR_THRESHOLD     = 3000    // 3m
        const val ZONE_VICINITY_THRESHOLD = 1000    // 1m
    }

    // Zone classification
    enum class Zone {
        NONE, FAR, MID, NEAR, VICINITY, INTERIOR
    }

    var onRangingResult: ((RangingResult) -> Unit)? = null
    var onZoneChanged: ((Zone, Zone) -> Unit)? = null  // (oldZone, newZone)

    private var currentZone = Zone.NONE
    private var isRanging = false

    data class RangingResult(
        val sessionId: Int,
        val distanceMm: Int,
        val angleAzimuth: Float,
        val angleElevation: Float,
        val quality: Int  // 0-100
    )

    /**
     * 启动UWB测距会话
     * BLE连接成功后调用，传入车端提供的UWB配置
     */
    fun startRanging(
        sessionId: Int,
        channel: Int = CHANNEL_9,
        preambleCode: Int = PREAMBLE_CODE_11,
        macMode: Int = 1  // SP3
    ) {
        if (isRanging) {
            Log.w(TAG, "Ranging already active")
            return
        }

        Log.i(TAG, "Starting UWB ranging: session=$sessionId ch=$channel preamble=$preambleCode")

        // Android UWB API (simplified):
        // val uwbManager = context.getSystemService(Context.UWB_SERVICE) as UwbManager
        // val session = uwbManager.controllerSessionScope()
        //     .createRangingSession(config)
        // session.startRanging()

        isRanging = true
        currentZone = Zone.NONE
    }

    /**
     * 停止UWB测距
     */
    fun stopRanging() {
        if (!isRanging) return
        Log.i(TAG, "Stopping UWB ranging")
        isRanging = false
        currentZone = Zone.NONE
    }

    /**
     * 处理测距结果 (从Android UWB API回调)
     */
    fun handleRangingData(distanceMm: Int, azimuth: Float, elevation: Float, quality: Int) {
        val result = RangingResult(
            sessionId = 0,
            distanceMm = distanceMm,
            angleAzimuth = azimuth,
            angleElevation = elevation,
            quality = quality
        )

        onRangingResult?.invoke(result)

        // Zone classification
        val newZone = classifyZone(distanceMm)
        if (newZone != currentZone) {
            Log.i(TAG, "Zone changed: $currentZone → $newZone (distance=${distanceMm}mm)")
            onZoneChanged?.invoke(currentZone, newZone)
            currentZone = newZone
        }
    }

    /**
     * 距离分区
     */
    fun classifyZone(distanceMm: Int): Zone = when {
        distanceMm < 0          -> Zone.NONE
        distanceMm < ZONE_VICINITY_THRESHOLD -> Zone.INTERIOR
        distanceMm < ZONE_NEAR_THRESHOLD     -> Zone.VICINITY
        distanceMm < ZONE_MID_THRESHOLD      -> Zone.NEAR
        distanceMm < ZONE_FAR_THRESHOLD      -> Zone.MID
        distanceMm < 50000                   -> Zone.FAR
        else                                 -> Zone.NONE
    }

    /**
     * 根据分区自动触发操作
     */
    fun getAutoAction(newZone: Zone): Models.ControlAction? = when (newZone) {
        Zone.VICINITY -> Models.ControlAction.UNLOCK
        Zone.MID -> Models.ControlAction.LOCK
        Zone.INTERIOR -> Models.ControlAction.ENGINE_ON
        else -> null
    }

    fun getCurrentZone(): Zone = currentZone
    fun isRanging(): Boolean = isRanging
}
