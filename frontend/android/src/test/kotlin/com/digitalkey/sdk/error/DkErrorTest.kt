/**
 * DigitalKey SDK — DkError 单元测试
 *
 * 验证:
 * - 所有错误码常量的值正确性
 * - ErrorCategory 编解码
 * - DigitalKeyError 属性计算与工厂方法
 * - 链式构造（details, traceId, cause）
 * - 序列化 (toMap)
 */
package com.digitalkey.sdk.error

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class DkErrorTest {

    // ════════════════════════════════════════════
    // ErrorCategory
    // ════════════════════════════════════════════

    @Test
    fun `ErrorCategory fromCode maps known codes correctly`() {
        val cases = mapOf(
            0x0000 to ErrorCategory.SUCCESS,
            0x0100 to ErrorCategory.REQUEST,
            0x0200 to ErrorCategory.AUTH,
            0x0300 to ErrorCategory.KEY,
            0x0400 to ErrorCategory.VEHICLE,
            0x0500 to ErrorCategory.SHARE,
            0x0600 to ErrorCategory.DEVICE,
            0x0700 to ErrorCategory.VENDOR,
            0x0800 to ErrorCategory.TRANSPORT,
            0x0900 to ErrorCategory.SYSTEM,
            0xAC00 to ErrorCategory.TCU,
        )
        cases.forEach { (code, expected) ->
            assertEquals("Code 0x${code.toString(16)} should map to ${expected.name}",
                expected, ErrorCategory.fromCode(code))
        }
    }

    @Test
    fun `ErrorCategory fromCode unknown code maps to SYSTEM`() {
        assertEquals(ErrorCategory.SYSTEM, ErrorCategory.fromCode(0xFFFF))
        assertEquals(ErrorCategory.SYSTEM, ErrorCategory.fromCode(0x0A00))
        assertEquals(ErrorCategory.SYSTEM, ErrorCategory.fromCode(0x0000))
    }

    @Test
    fun `ErrorCategory has correct value for each entry`() {
        assertEquals(0x00u, ErrorCategory.SUCCESS.value)
        assertEquals(0x01u, ErrorCategory.REQUEST.value)
        assertEquals(0x02u, ErrorCategory.AUTH.value)
        assertEquals(0x03u, ErrorCategory.KEY.value)
        assertEquals(0x04u, ErrorCategory.VEHICLE.value)
        assertEquals(0x05u, ErrorCategory.SHARE.value)
        assertEquals(0x06u, ErrorCategory.DEVICE.value)
        assertEquals(0x07u, ErrorCategory.VENDOR.value)
        assertEquals(0x08u, ErrorCategory.TRANSPORT.value)
        assertEquals(0x09u, ErrorCategory.SYSTEM.value)
        assertEquals(0xACu, ErrorCategory.TCU.value)
    }

    // ════════════════════════════════════════════
    // DkErrorCode 常量值
    // ════════════════════════════════════════════

    @Test
    fun `DkErrorCode success values are correct`() {
        assertEquals(0x0000, DkErrorCode.SUCCESS)
        assertEquals(0x0001, DkErrorCode.SUCCESS_ASYNC)
        assertEquals(0x0002, DkErrorCode.SUCCESS_PARTIAL)
    }

    @Test
    fun `DkErrorCode key error values are correct`() {
        assertEquals(0x0301, DkErrorCode.ERR_KEY_NOT_FOUND)
        assertEquals(0x0302, DkErrorCode.ERR_KEY_EXISTS)
        assertEquals(0x0303, DkErrorCode.ERR_KEY_EXPIRED)
        assertEquals(0x0304, DkErrorCode.ERR_KEY_REVOKED)
        assertEquals(0x030D, DkErrorCode.ERR_KEY_BIND_FAILED)
    }

    @Test
    fun `DkErrorCode transport error values are correct`() {
        assertEquals(0x0801, DkErrorCode.ERR_NETWORK_ERROR)
        assertEquals(0x0805, DkErrorCode.ERR_MQTT_DISCONNECTED)
        assertEquals(0x0807, DkErrorCode.ERR_GRPC_UNAVAILABLE)
    }

    // ════════════════════════════════════════════
    // DigitalKeyError 基础属性
    // ════════════════════════════════════════════

    @Test
    fun `DigitalKeyError SUCCESS factory creates success error`() {
        val err = DigitalKeyError.SUCCESS
        assertEquals(DkErrorCode.SUCCESS, err.code)
        assertEquals(ErrorCategory.SUCCESS, err.category)
        assertEquals("0x0000", err.hexCode)
        assertTrue(err.isSuccess)
    }

    @Test
    fun `DigitalKeyError has correct hexCode format`() {
        val err = DigitalKeyError.fromCode(DkErrorCode.ERR_KEY_NOT_FOUND)
        assertEquals("0x0301", err.hexCode)
    }

    @Test
    fun `DigitalKeyError category is derived from code`() {
        assertEquals(ErrorCategory.AUTH, DigitalKeyError.fromCode(0x0201).category)
        assertEquals(ErrorCategory.VEHICLE, DigitalKeyError.fromCode(0x0401).category)
        assertEquals(ErrorCategory.TCU, DigitalKeyError.fromCode(0xAC01).category)
    }

    @Test
    fun `DigitalKeyError fromCode creates error with correct message`() {
        val err = DigitalKeyError.fromCode(DkErrorCode.ERR_KEY_NOT_FOUND)
        assertEquals("密钥不存在", err.message)
    }

    @Test
    fun `DigitalKeyError fromCode with unknown code returns generic message`() {
        val err = DigitalKeyError.fromCode(0x9999)
        assertEquals("Unknown error", err.message)
    }

    @Test
    fun `DigitalKeyError isSuccess is false for non-success codes`() {
        assertFalse(DigitalKeyError.fromCode(DkErrorCode.ERR_INTERNAL_ERROR).isSuccess)
        assertFalse(DigitalKeyError.fromCode(DkErrorCode.ERR_KEY_BIND_FAILED).isSuccess)
    }

    // ════════════════════════════════════════════
    // 链式构造
    // ════════════════════════════════════════════

    @Test
    fun `DigitalKeyError withTraceId preserves code and adds traceId`() {
        val err = DigitalKeyError.fromCode(DkErrorCode.ERR_COMMAND_FAILED)
            .withTraceId("trace-abc-123")
        assertEquals(DkErrorCode.ERR_COMMAND_FAILED, err.code)
        assertEquals("trace-abc-123", err.traceId)
    }

    @Test
    fun `DigitalKeyError withDetails adds details map`() {
        val err = DigitalKeyError.fromCode(DkErrorCode.ERR_BATTERY_LOW)
            .withDetails("vehicle_id" to "VH-001", "battery_level" to 5)
        assertEquals("VH-001", err.details?.get("vehicle_id"))
        assertEquals(5, err.details?.get("battery_level"))
    }

    @Test
    fun `DigitalKeyError withCause preserves code and adds cause`() {
        val cause = RuntimeException("connection reset")
        val err = DigitalKeyError.fromCode(DkErrorCode.ERR_NETWORK_ERROR)
            .withCause(cause)
        assertEquals(DkErrorCode.ERR_NETWORK_ERROR, err.code)
        assertNotNull(err.cause)
        assertEquals("connection reset", err.cause!!.message)
    }

    @Test
    fun `DigitalKeyError full chain preserves all fields`() {
        val cause = RuntimeException("timeout")
        val err = DigitalKeyError.fromCode(DkErrorCode.ERR_SERVER_UNREACHABLE)
            .withTraceId("trace-xyz")
            .withDetails("host" to "api.example.com")
            .withCause(cause)
        assertEquals("trace-xyz", err.traceId)
        assertEquals("api.example.com", err.details?.get("host"))
        assertSame(cause, err.cause)
        assertEquals("0x0803", err.hexCode)
    }

    // ════════════════════════════════════════════
    // toMap 序列化
    // ════════════════════════════════════════════

    @Test
    fun `DigitalKeyError toMap contains all fields`() {
        val err = DigitalKeyError.fromCode(DkErrorCode.ERR_KEY_BIND_FAILED)
            .withTraceId("trace-001")
            .withDetails("vehicle_id" to "VH-001")
        val map = err.toMap()
        assertEquals(DkErrorCode.ERR_KEY_BIND_FAILED, map["code"])
        assertEquals("0x030D", map["code_hex"])
        assertEquals("ERR_KEY_BIND_FAILED", map["name"])
        assertEquals("密钥绑定失败", map["message"])
        assertEquals("KEY", map["category"])
        assertEquals("trace-001", map["trace_id"])
        assertNotNull(map["details"])
    }

    // ════════════════════════════════════════════
    // getErrorName / getErrorMessage 辅助函数
    // ════════════════════════════════════════════

    @Test
    fun `getErrorName returns correct name for known codes`() {
        assertEquals("ERR_KEY_NOT_FOUND", getErrorName(DkErrorCode.ERR_KEY_NOT_FOUND))
        assertEquals("ERR_NETWORK_ERROR", getErrorName(DkErrorCode.ERR_NETWORK_ERROR))
        assertEquals("ERR_TCU_BLE_ERROR", getErrorName(DkErrorCode.ERR_TCU_BLE_ERROR))
    }

    @Test
    fun `getErrorName returns ERR_UNKNOWN for unknown codes`() {
        assertEquals("ERR_UNKNOWN", getErrorName(0xFFFF))
    }

    @Test
    fun `getErrorMessage returns correct message for known codes`() {
        assertEquals("密钥不存在", getErrorMessage(DkErrorCode.ERR_KEY_NOT_FOUND))
        assertEquals("网络错误", getErrorMessage(DkErrorCode.ERR_NETWORK_ERROR))
        assertEquals("门锁错误码覆盖下边缘", "车门未关闭", getErrorMessage(DkErrorCode.ERR_DOOR_OPEN))
    }

    @Test
    fun `getErrorMessage returns default for unknown codes`() {
        assertEquals("Unknown error", getErrorMessage(0xFFFF))
    }
}
