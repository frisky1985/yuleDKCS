package com.yuledkcs.sdk.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

/**
 * 2b-D 扫描结果处理测试 — 验证广播字节 → 2b-B 解析 → 车辆信息 的完整链路。
 */
class ScanResultProcessorTest {

    private val processor = ScanResultProcessor()

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

    /** ICCOA 广播: FEF5 服务 + 厂商数据 (company 0x0100, vehicle_id 6B, ver, capability) */
    private fun iccoaAdvertise(capability: Int = 0x01): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xF5, 0xFE),
        ad(0xFF, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, capability)
    )

    /** CCC 广播: FFD1 服务 + 厂商数据 */
    private fun cccAdvertise(): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xD1, 0xFF),
        ad(0xFF, 0x00, 0x01, 0x11, 0x22, 0x33, 0x44)
    )

    /** ICCE 广播: FEFA 服务 + 厂商数据 */
    private fun icceAdvertise(): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xFA, 0xFE),
        ad(0xFF, 0x00, 0x01, 0x55, 0x66, 0x77, 0x88)
    )

    // ─── 测试 ─────────────────────────────────────────────

    @Test
    fun `process parses iccoa advertisement`() {
        val vehicle = processor.process(iccoaAdvertise(), rssi = -55)
        assertNotNull(vehicle)
        assertEquals("iccoa-aabbccddeeff", vehicle!!.vehicleId)
        assertEquals(BleProtocolType.ICCOA.ordinal + 1, vehicle.protocolType)
        assertEquals(-55, vehicle.rssi)
        // capability bit0 = UWB 支持
        assertEquals(true, vehicle.supportsUwb)
    }

    @Test
    fun `process parses iccoa advertisement without uwb capability`() {
        val vehicle = processor.process(iccoaAdvertise(capability = 0x00), rssi = -60)
        assertNotNull(vehicle)
        assertEquals(false, vehicle!!.supportsUwb)
    }

    @Test
    fun `process parses ccc advertisement`() {
        val vehicle = processor.process(cccAdvertise(), rssi = -70)
        assertNotNull(vehicle)
        assertEquals("ccc-11223344", vehicle!!.vehicleId)
        assertEquals(BleProtocolType.CCC.ordinal + 1, vehicle.protocolType)
    }

    @Test
    fun `process parses icce advertisement`() {
        val vehicle = processor.process(icceAdvertise(), rssi = -80)
        assertNotNull(vehicle)
        assertEquals("icce-55667788", vehicle!!.vehicleId)
        assertEquals(BleProtocolType.ICCE.ordinal + 1, vehicle.protocolType)
    }

    @Test
    fun `process returns null for non-vehicle advertisement`() {
        // 只有 Flags, 无任何数字钥匙服务
        val unrelated = ad(0x01, 0x06)
        assertNull(processor.process(unrelated, rssi = -40))
    }

    @Test
    fun `process returns null for garbage and empty input`() {
        assertNull(processor.process(ByteArray(0), rssi = -40))
        assertNull(processor.process(null, rssi = -40))
        assertNull(processor.process(byteArrayOf(0x7F, 0x7F, 0x7F, 0x7F), rssi = -40))
    }
}
