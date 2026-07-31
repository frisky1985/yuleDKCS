package com.yuledkcs.sdk.ble

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

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
 * - 依赖 UWB 硬件 (FiRa 兼容 / Android 12+ UWB)
 * - 本期提供接口 + Mock 实现，真实集成需硬件环境
 */
interface UwbManager {
    suspend fun startRanging(vehicleId: String)
    fun stopRanging()
    var rangingResultHandler: ((UwbMeasurement) -> Unit)?
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
