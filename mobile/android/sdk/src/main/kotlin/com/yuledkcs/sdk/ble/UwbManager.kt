package com.yuledkcs.sdk.ble

import android.content.Context
import android.os.Build
import android.uwb.RangingParameters
import android.uwb.RangingReport
import android.uwb.RangingSession
import android.uwb.UwbAdapter
import android.uwb.UwbComplexChannel
import android.uwb.UwbConfigType
// 别名导入: 避免与本文档 FiRa 抽象 interface UwbManager 同名冲突 (Kotlin Conflicting import)
import android.uwb.UwbManager as AndroidUwbManagerService
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.util.concurrent.Executor
import java.util.concurrent.Executors

/**
 * UWB 测距结果
 */
data class UwbMeasurement(
    val vehicleId: String,
    val distance: Double,
    val azimuth: Double? = null,
    val elevation: Double? = null,
    val timestamp: Long
)

/**
 * UWB 管理器接口 (FiRa 标准抽象)
 *
 * 实现说明:
 * - 真实实现: [AndroidUwbManager] (android.uwb 原生 API, Android 14 / API 34+)
 * - 降级/调试: [MockUwbManager]
 */
interface UwbManager {
    suspend fun startRanging(vehicleId: String)
    fun stopRanging()
    var rangingResultHandler: ((UwbMeasurement) -> Unit)?
}

/**
 * UWB 平台版本策略 (纯逻辑, 便于 JVM 单测 — 见 UwbManagerTest)
 *
 * Android 14 (API 34) 起 android.uwb.UwbManager 才作为公开系统服务开放
 * (Context.getSystemService(UwbManager::class.java)); 更低版本调用
 * [requireSupported] 抛 IllegalStateException, 上层可捕获后降级 [MockUwbManager]。
 */
object UwbVersionPolicy {
    /** android.uwb 公开系统服务所需最低 API 级别 */
    const val MIN_API_LEVEL = 34

    fun isSupported(apiLevel: Int): Boolean = apiLevel >= MIN_API_LEVEL

    fun requireSupported(apiLevel: Int) {
        if (!isSupported(apiLevel)) {
            throw IllegalStateException(
                "UWB requires Android 14+ (API $MIN_API_LEVEL), current API $apiLevel"
            )
        }
    }
}

/**
 * Mock UWB 管理器（开发调试用）
 */
class MockUwbManager : UwbManager {
    override var rangingResultHandler: ((UwbMeasurement) -> Unit)? = null
    private var job: Job? = null
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    override suspend fun startRanging(vehicleId: String) {
        stopRanging()
        job = scope.launch {
            while (isActive) {
                delay(1000)
                val measurement = UwbMeasurement(
                    vehicleId = vehicleId,
                    distance = 0.5 + Math.random() * 4.5,
                    azimuth = -45 + Math.random() * 90,
                    elevation = -10 + Math.random() * 20,
                    timestamp = System.currentTimeMillis()
                )
                rangingResultHandler?.invoke(measurement)
            }
        }
    }

    override fun stopRanging() {
        job?.cancel()
        job = null
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 真实实现: android.uwb 原生 API (Android 14 / API 34+)
//
// 平台事实 (官方, 详见 docs/sdk/PHASE2G-UWB-PLATFORM.md):
// 1. Android 14 (API 34) 起 android.uwb.UwbManager 为公开系统服务, 经
//    Context.getSystemService(UwbManager::class.java) 获取, 底层为 FiRa 兼容
//    UWB 硬件 (manifest 需 android.hardware.uwb feature + UWB_RANGING 权限 +
//    前台运行 + 定位权限)。
// 2. 测距流程: UwbManager(系统服务, 别名 AndroidUwbManagerService).adapter →
//    adapter.openRangingSession(RangingParameters, Executor, RangingSession.Callback)
//    → 回调 onOpened 后调用 session.start()
//    → onReportReceived 携带 RangingReport (距离 mm / 方位角 deg / 仰角 deg)。
// 3. 会话需前台运行: App 退后台会话被系统暂停/关闭, 回前台需重新 startRanging。
// 4. 版本降级: API < 34 时 [UwbVersionPolicy.requireSupported] 抛
//    IllegalStateException("UWB requires Android 14+ ..."), 上层捕获后降级 [MockUwbManager]。
// 5. 回调在传入的 [executor] 线程执行; 更新 UI 请自行切主线程。
//
// @param context 应用上下文 (getSystemService 需要)
// @param executor 系统回调执行器, 默认单线程
// ─────────────────────────────────────────────────────────────────────────────
class AndroidUwbManager(
    private val context: Context,
    private val executor: Executor = Executors.newSingleThreadExecutor()
) : UwbManager {

    override var rangingResultHandler: ((UwbMeasurement) -> Unit)? = null

    private var rangingSession: RangingSession? = null
    private var activeVehicleId: String? = null

    /** 测距会话回调 (android.uwb, API 34 签名: 失败回调携带 RangingParameters) */
    private val sessionCallback = object : RangingSession.Callback() {

        override fun onOpened(session: RangingSession) {
            session.start()
        }

        override fun onOpenFailed(reason: Int, parameters: RangingParameters) {
            rangingSession = null
        }

        override fun onStarted(session: RangingSession) {
            // 测距已开始; 等待 onReportReceived
        }

        override fun onStartFailed(reason: Int, parameters: RangingParameters) {
            rangingSession?.close()
            rangingSession = null
        }

        override fun onStopped(session: RangingSession, reason: Int) {
            // 停止完成
        }

        override fun onClosed(session: RangingSession, reason: Int, parameters: RangingParameters) {
            rangingSession = null
        }

        override fun onRangingFailure(session: RangingSession, reason: Int) {
            // 测距中断 (如会话超时); 上层可重新 startRanging
        }

        override fun onReportReceived(session: RangingSession, report: RangingReport) {
            val vehicleId = activeVehicleId ?: return
            report.measurements.firstOrNull()?.let { measurement ->
                rangingResultHandler?.invoke(
                    UwbMeasurement(
                        vehicleId = vehicleId,
                        distance = measurement.distanceMm / 1000.0,          // mm → m
                        azimuth = measurement.azimuthDegrees,                // 度
                        elevation = measurement.elevationDegrees,            // 度
                        timestamp = measurement.elapsedRealtimeNanos / 1_000_000 // ns → ms
                    )
                )
            }
        }
    }

    override suspend fun startRanging(vehicleId: String) {
        // 版本降级: API < 34 抛明确错误, 上层捕获后回退 MockUwbManager
        UwbVersionPolicy.requireSupported(Build.VERSION.SDK_INT)

        stopRanging()
        activeVehicleId = vehicleId

        val uwbManager: AndroidUwbManagerService = context.getSystemService(AndroidUwbManagerService::class.java)
            ?: throw IllegalStateException("UWB manager service unavailable (no FiRa UWB hardware)")
        val adapter: UwbAdapter = uwbManager.adapter

        val parameters = RangingParameters(
            deviceAddress = null,                 // 由系统分配本地 UWB 地址
            destinationAddress = null,            // 车端地址: 2b-G 联调时经 BLE 协商后注入
            complexChannel = UwbComplexChannel(9, 11), // channel 9 / preamble 11 (与车端约定)
            sessionId = intArrayOf(0x444B4353),   // "DKCS" 固定会话 ID (FiRa 32-bit, 与车端约定)
            rangingRssi = null,
            slotDuration = 960,                   // 960 µs (UWB_CONFIG_1 默认)
            uwbConfigType = UwbConfigType.UWB_CONFIG_1
        )
        rangingSession = adapter.openRangingSession(parameters, executor, sessionCallback)
        // 会话在 onOpened 回调中 start()
    }

    override fun stopRanging() {
        activeVehicleId = null
        rangingSession?.stop()
        rangingSession?.close()
        rangingSession = null
    }
}
