package com.yuledkcs.sdk.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking

/**
 * 2b-G UWB 模拟测 — 接口契约 + 版本降级分支 (contract U-2/U-3/U-4)
 *
 * 说明 (纯 JVM 边界, 同 BleStubTest 约定):
 * - AndroidUwbManager 构造依赖 Context (getSystemService), android.jar 桩方法
 *   在本地单测抛 "not mocked", 无法实例化; 真实实现按"静态审查 (API 名称/签名)
 *   + 版本策略纯逻辑"验证, 硬件链路属真机联调范畴。
 * - [UwbVersionPolicy] 为纯逻辑对象: 版本分支 (API < 34 → IllegalStateException
 *   降级指引) 可完整单测。
 * - [MockUwbManager] 契约 (start/stop/回调注入) 不依赖硬件, 全量覆盖。
 */
class UwbManagerTest {

    // ─── U-3/U-4: Mock 接口契约 ────────────────────────────────────────

    /** U-4: startRanging 后产生测距回调 (vehicleId 透传 + 距离范围) */
    @Test
    fun mockStartRangingEmitsMeasurement() = runBlocking {
        val manager = MockUwbManager()
        var received: UwbMeasurement? = null
        manager.rangingResultHandler = { received = it }

        manager.startRanging("VH-001")

        // 等第一个回调 (Mock 周期 1s)
        repeat(30) {
            if (received != null) return@repeat
            delay(100)
        }
        manager.stopRanging()

        assertNotNull("startRanging 后应产生测距回调", received)
        assertEquals("VH-001", received!!.vehicleId)
        assertTrue("距离应在 (0, 5] 米", received!!.distance > 0.0 && received!!.distance <= 5.0)
    }

    /** U-4: stopRanging 后不再产生回调 (状态机终止) */
    @Test
    fun mockStopRangingHaltsCallbacks() = runBlocking {
        val manager = MockUwbManager()
        var count = 0
        manager.rangingResultHandler = { count++ }

        manager.startRanging("VH-001")
        delay(1100) // 至少一个回调
        manager.stopRanging()
        val afterStop = count
        delay(1200) // 停止后再等一个周期

        assertEquals("stopRanging 后不得再产生回调", afterStop, count)
        assertTrue("停止前应至少收到一个回调", afterStop >= 1)
    }

    /** U-3: 回调 handler 可注入、替换、置空 */
    @Test
    fun mockRangingResultHandlerInjection() = runBlocking {
        val manager = MockUwbManager()
        assertNull("初始 handler 应为空", manager.rangingResultHandler)

        var count = 0
        manager.rangingResultHandler = { count++ }
        assertNotNull(manager.rangingResultHandler)

        manager.startRanging("VH-001")
        delay(1100)
        manager.stopRanging()
        assertTrue("运行期间应至少收到一次回调", count >= 1)

        manager.rangingResultHandler = null
        assertNull("handler 可置空", manager.rangingResultHandler)
    }

    // ─── U-2: 版本降级分支 (纯逻辑) ────────────────────────────────────

    @Test
    fun versionPolicySupportsApi34Plus() {
        assertFalse("API 33 (Android 13) 不支持", UwbVersionPolicy.isSupported(33))
        assertTrue("API 34 (Android 14) 支持", UwbVersionPolicy.isSupported(34))
        assertTrue("API 35+ 支持", UwbVersionPolicy.isSupported(35))
    }

    @Test
    fun versionPolicyRequireSupportedBelow34Throws() {
        // API < 34: 必须抛 IllegalStateException 且信息含降级指引
        try {
            UwbVersionPolicy.requireSupported(33)
            fail("API 33 应抛 IllegalStateException")
        } catch (e: IllegalStateException) {
            assertTrue(
                "错误信息应含降级指引, 实际: ${e.message}",
                e.message.orEmpty().contains("UWB requires Android 14+")
            )
        }

        // API 34+ 不抛
        UwbVersionPolicy.requireSupported(34)
        UwbVersionPolicy.requireSupported(100)
    }

    /** 边界: 恰好 34 为支持边界 (含) */
    @Test
    fun versionPolicyBoundaryIsInclusive() {
        assertTrue(UwbVersionPolicy.isSupported(UwbVersionPolicy.MIN_API_LEVEL))
    }

    // ─── UwbMeasurement 数据契约 ───────────────────────────────────────

    @Test
    fun uwbMeasurementDefaults() {
        val m = UwbMeasurement(vehicleId = "V", distance = 1.0, timestamp = 0L)
        assertNull(m.azimuth)
        assertNull(m.elevation)
        assertEquals("V", m.vehicleId)
        assertEquals(1.0, m.distance, 1e-9)
    }
}
