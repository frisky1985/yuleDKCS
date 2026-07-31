package com.yuledkcs.sdk.ble

import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * 4.3 BLE 桩测试 — 模拟车辆广播驱动 SDK 扫描/指令链路 (contract B2/B4)
 *
 * 桩设计 (W2):
 * - [FakeBleScanEngine]: 实现 [BleScanEngine] 的扫描桩, 记录 start/stop 调用
 *   并保存 ScanCallback, 测试可经 [FakeBleScanEngine.emitScanResult] /
 *   [FakeBleScanEngine.emitScanFailed] 直接驱动回调, 无需真实蓝牙硬件。
 * - 发现链路: 复用 BleManager.scanVehicles 内部同一处理逻辑
 *   ([ScanResultProcessor] + 各协议适配器广告解析 + [BleScanFilterFactory] 过滤),
 *   以三种协议桩广播样本验证"扫描发现 → 广告解析 → vehicleId"。
 * - 指令链路: [CCCBleAdapter] wire 级断言 — 规范 4 字节帧头
 *   (CCC-TS-101 v4.0.0 Table 19-19): Message Type(SE) + Message ID(DK_APDU_RQ/RS) + length BE。
 *
 * 说明 (纯 JVM 边界): BleManager 构造依赖 Android Context (getSystemService),
 * android.jar 桩方法在本地单测中抛 "not mocked", 无法实例化;
 * 因此全链路在三个接缝上验证: 注入点 (BleScanEngine 桩) +
 * 发现处理 (ScanResultProcessor, 即 scanVehicles.onScanResult 内部逻辑) +
 * 指令层 (CCCBleAdapter)。对应 iOS FakeCentral 注入职责。
 * 桩样本只经 AD Structure 字节构造, 不调用 android 实例方法 (仅类型引用/构造)。
 */
class BleStubTest {

    // ─── 测试辅助: 构造 AD Structure ───────────────────────

    private fun ad(type: Int, vararg data: Int): ByteArray {
        val bytes = ByteArray(2 + data.size)
        bytes[0] = (data.size + 1).toByte()
        bytes[1] = type.toByte()
        data.forEachIndexed { i, v -> bytes[i + 2] = v.toByte() }
        return bytes
    }

    private fun concat(vararg arrays: ByteArray): ByteArray {
        var total = 0
        arrays.forEach { total += it.size }
        val out = ByteArray(total)
        var offset = 0
        arrays.forEach { a ->
            a.copyInto(out, offset)
            offset += a.size
        }
        return out
    }

    /** CCC 桩广播: 0xFFF5 service (CCC-TS-101 v4.0.0 Table 19-6) + 厂商数据 */
    private fun cccStubAdvertise(): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xF5, 0xFF),
        ad(0xFF, 0x00, 0x01, 0x11, 0x22, 0x33, 0x44)
    )

    /** ICCOA 桩广播: 0xFEF5 service + 厂商数据 (vehicle_id 6B + ver + capability) */
    private fun iccoaStubAdvertise(): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xF5, 0xFE),
        ad(0xFF, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x01)
    )

    /** ICCE 桩广播: 0xFEFA service + 厂商数据 */
    private fun icceStubAdvertise(): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xFA, 0xFE),
        ad(0xFF, 0x00, 0x01, 0x55, 0x66, 0x77, 0x88)
    )

    // ─── B2: 扫描桩 (FakeBleScanEngine 注入点) ──────────────

    @Test
    fun `fake scan engine captures callback and reports start result`() {
        val engine = FakeBleScanEngine()
        val callback = RecordingScanCallback()

        val started = engine.startScan(emptyList(), callback)

        assertTrue(started)
        assertTrue(engine.started)
        assertEquals(1, engine.startCount)
        assertEquals(0, engine.lastFilters!!.size)
        assertSame(callback, engine.lastCallback)
    }

    @Test
    fun `fake scan engine propagates start failure`() {
        val engine = FakeBleScanEngine(startResult = false)

        assertFalse(engine.startScan(emptyList(), RecordingScanCallback()))
        assertTrue(engine.started) // 调用已发生, 返回值模拟系统拒绝启动扫描
    }

    @Test
    fun `fake scan engine drives scan result callback`() {
        val engine = FakeBleScanEngine()
        val callback = RecordingScanCallback()
        engine.startScan(emptyList(), callback)

        // ScanResult 公共构造器 (device=null): 仅作回调参数传递, 不读取其字段
        val stubResult = ScanResult(null, 1, 0, 0, -1, -127, -70, 0, null, 0L)
        engine.emitScanResult(1, stubResult)

        assertEquals(1, callback.scanResults.size)
        assertSame(stubResult, callback.scanResults[0])
    }

    @Test
    fun `fake scan engine drives scan failure callback`() {
        val engine = FakeBleScanEngine()
        val callback = RecordingScanCallback()
        engine.startScan(emptyList(), callback)

        engine.emitScanFailed(3) // ScanCallback.SCAN_FAILED_INTERNAL_ERROR

        assertEquals(3, callback.failedCode)
    }

    @Test
    fun `fake scan engine stop is tracked and repeatable`() {
        val engine = FakeBleScanEngine()
        val callback = RecordingScanCallback()
        engine.startScan(emptyList(), callback)

        engine.stopScan(callback)
        engine.stopScan(callback)

        assertTrue(engine.stopped)
        assertEquals(2, engine.stopCount)
    }

    // ─── B2/B3: 发现链路 (BleManager.scanVehicles 同款处理) ──

    private val processor = ScanResultProcessor()

    @Test
    fun `scan pipeline discovers ccc stub broadcast`() {
        val vehicle = processor.process(cccStubAdvertise(), rssi = -70)

        assertNotNull(vehicle)
        assertEquals("ccc-11223344", vehicle!!.vehicleId)
        assertEquals(BleProtocolType.CCC.ordinal + 1, vehicle.protocolType)
        assertEquals(-70, vehicle.rssi)
    }

    @Test
    fun `scan pipeline discovers iccoa and icce stub broadcasts`() {
        val iccoa = processor.process(iccoaStubAdvertise(), rssi = -55)
        assertNotNull(iccoa)
        assertEquals("iccoa-aabbccddeeff", iccoa!!.vehicleId)
        assertEquals(BleProtocolType.ICCOA.ordinal + 1, iccoa.protocolType)
        assertTrue(iccoa.supportsUwb)

        val icce = processor.process(icceStubAdvertise(), rssi = -80)
        assertNotNull(icce)
        assertEquals("icce-55667788", icce!!.vehicleId)
        assertEquals(BleProtocolType.ICCE.ordinal + 1, icce.protocolType)
    }

    @Test
    fun `scan pipeline filters non-protocol broadcast`() {
        // 只有 Flags, 无任何数字钥匙 service
        assertNull(processor.process(ad(0x01, 0x06), rssi = -40))
        // 无关 16-bit service (0x1234)
        assertNull(processor.process(concat(ad(0x01, 0x06), ad(0x03, 0x34, 0x12)), rssi = -40))
    }

    // ─── B4: 指令构建 wire 级 (CCC 4B 帧头, 对照规范) ────────

    @Test
    fun `ccc unlock command uses spec 4-byte frame header`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)
        val session = SessionContext(keyId = "key-1", vehicleId = "VH-1", sessionHandle = 0x0102, counter = 3)

        val command = adapter.buildUnlockCommand("key-1", session)

        // [0]=Message Type SE(0x01) [1]=Message ID DK_APDU_RQ(0x0B) [2-3]=length BE
        assertEquals(CccFrame.MSG_TYPE_SE, command[0].toInt() and 0xFF)
        assertEquals(CccFrame.MSG_ID_DK_APDU_RQ, command[1].toInt() and 0xFF)
        val declaredLen = ((command[2].toInt() and 0xFF) shl 8) or (command[3].toInt() and 0xFF)
        assertEquals(13, declaredLen)
        assertEquals(CccFrame.HEADER_SIZE + 13, command.size)

        // 帧解析往返
        val frame = CccFrame.parse(command)
        assertNotNull(frame)
        assertEquals(CccFrame.MSG_TYPE_SE, frame!!.messageType)
        assertEquals(CccFrame.MSG_ID_DK_APDU_RQ, frame.messageId)

        // 透传载荷 (CccNullMessageSecurity): [0]=cmdType [1-2]=handle BE [3-6]=counter BE [7]=keyIdLen [8..]=keyId
        val p = frame.payload
        assertEquals(13, p.size)
        assertEquals(BleCommandType.UNLOCK.value.toInt() and 0xFF, p[0].toInt() and 0xFF)
        assertEquals(0x01, p[1].toInt() and 0xFF)
        assertEquals(0x02, p[2].toInt() and 0xFF)
        assertEquals(0x00, p[3].toInt() and 0xFF)
        assertEquals(0x00, p[4].toInt() and 0xFF)
        assertEquals(0x00, p[5].toInt() and 0xFF)
        assertEquals(0x03, p[6].toInt() and 0xFF)
        assertEquals(5, p[7].toInt() and 0xFF)
        assertArrayEquals("key-1".toByteArray(), p.copyOfRange(8, 13))
    }

    @Test
    fun `ccc lock and engine commands carry correct command type`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)
        val session = SessionContext(keyId = "k", vehicleId = "VH-1", counter = 1)

        val lock = CccFrame.parse(adapter.buildLockCommand("k", session))
        assertNotNull(lock)
        assertEquals(CccFrame.MSG_TYPE_SE, lock!!.messageType)
        assertEquals(CccFrame.MSG_ID_DK_APDU_RQ, lock.messageId)
        assertEquals(BleCommandType.LOCK.value.toInt() and 0xFF, lock.payload[0].toInt() and 0xFF)

        val engine = CccFrame.parse(adapter.buildStartEngineCommand("k", session))
        assertNotNull(engine)
        assertEquals(BleCommandType.ENGINE_ON.value.toInt() and 0xFF, engine!!.payload[0].toInt() and 0xFF)
    }

    @Test
    fun `ccc response parses SE DK_APDU_RS frame`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)

        // 规范响应: SE + DK_APDU_RS (0x0C), payload 首字节 SW1=0x90 → 成功
        val successFrame = CccFrame.build(
            CccFrame.MSG_TYPE_SE, CccFrame.MSG_ID_DK_APDU_RS,
            byteArrayOf(0x90.toByte(), 0x00)
        )
        val success = adapter.parseCommandResponse(successFrame)
        assertTrue(success.success)

        // SW1 != 0x90/0x00 → 失败, errorCode = SW1
        val failureFrame = CccFrame.build(
            CccFrame.MSG_TYPE_SE, CccFrame.MSG_ID_DK_APDU_RS,
            byteArrayOf(0x6A.toByte(), 0x86.toByte())
        )
        val failure = adapter.parseCommandResponse(failureFrame)
        assertFalse(failure.success)
        assertEquals(0x6A, failure.errorCode)
    }
}

/**
 * FakeBleScanEngine — 实现 [BleScanEngine] 的扫描桩 (contract B2.3/B2.4 注入点)。
 *
 * 记录 start/stop 调用与过滤器, 保存 ScanCallback 供测试驱动;
 * [startResult] 可配置以模拟系统拒绝启动扫描。
 * 纯 JVM 安全: 只保存/返回参数, 不调用任何 android 实例方法。
 */
class FakeBleScanEngine(
    /** startScan 返回值, false 模拟系统拒绝启动 */
    var startResult: Boolean = true
) : BleScanEngine {

    var started: Boolean = false
        private set
    var stopped: Boolean = false
        private set
    var startCount: Int = 0
        private set
    var stopCount: Int = 0
        private set
    var lastFilters: List<ScanFilter>? = null
        private set
    var lastCallback: ScanCallback? = null
        private set

    override fun startScan(filters: List<ScanFilter>, callback: ScanCallback): Boolean {
        started = true
        startCount++
        lastFilters = filters
        lastCallback = callback
        return startResult
    }

    override fun stopScan(callback: ScanCallback) {
        stopped = true
        stopCount++
    }

    /** 驱动保存的 ScanCallback — 触发一次扫描结果回调 */
    fun emitScanResult(callbackType: Int, result: ScanResult) {
        lastCallback?.onScanResult(callbackType, result)
    }

    /** 驱动保存的 ScanCallback — 触发一次扫描失败回调 */
    fun emitScanFailed(errorCode: Int) {
        lastCallback?.onScanFailed(errorCode)
    }
}

/**
 * RecordingScanCallback — 记录回调事件的 [ScanCallback] 子类。
 * 只保存回调参数, 不调用 android 桩方法 (纯 JVM 可测)。
 */
private class RecordingScanCallback : ScanCallback() {
    val scanResults = mutableListOf<ScanResult>()
    var failedCode: Int? = null

    override fun onScanResult(callbackType: Int, result: ScanResult) {
        scanResults.add(result)
    }

    override fun onScanFailed(errorCode: Int) {
        failedCode = errorCode
    }
}
