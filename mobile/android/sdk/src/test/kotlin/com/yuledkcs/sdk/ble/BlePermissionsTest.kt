package com.yuledkcs.sdk.ble

import android.os.Build
import org.junit.Assert.assertEquals
import org.junit.Test

/**
 * 2b-I 权限预检测试 (契约 B2.5 / AC-6)
 *
 * 说明 (纯 JVM 边界, 与 BleStubTest 同一约束):
 * [BlePermissions.checkOrThrow] 依赖 Context.checkSelfPermission,
 * android.jar 桩方法在本地单测抛 "not mocked", 无法实例化 Context;
 * 因此本测试覆盖纯逻辑分支 [BlePermissions.permissionsForApiLevel]。
 * (Manifest.permission.* / Build.VERSION_CODES.S 均为编译期常量, 内联后 JVM 安全)
 * checkOrThrow 的授予/缺失异常路径需 Robolectric 或仪器测试 (CI 环境执行)。
 */
class BlePermissionsTest {

    // ─── 分支: API 31+ (Android 12+) ──────────────────────

    @Test
    fun `api 31 and above require bluetooth scan and connect`() {
        val expected = listOf(
            "android.permission.BLUETOOTH_SCAN",
            "android.permission.BLUETOOTH_CONNECT"
        )
        assertEquals(expected, BlePermissions.permissionsForApiLevel(31))
        // 与 Android 官方常量等价 (Build.VERSION_CODES.S == 31)
        assertEquals(expected, BlePermissions.permissionsForApiLevel(Build.VERSION_CODES.S))
        assertEquals(expected, BlePermissions.permissionsForApiLevel(32))
        assertEquals(expected, BlePermissions.permissionsForApiLevel(35)) // 当前 compileSdk
    }

    // ─── 分支: API 26-30 (Android 8..11, minSdk 区间) ─────

    @Test
    fun `api 26 to 30 require fine location`() {
        val expected = listOf("android.permission.ACCESS_FINE_LOCATION")
        assertEquals(expected, BlePermissions.permissionsForApiLevel(26))
        assertEquals(expected, BlePermissions.permissionsForApiLevel(28))
        assertEquals(expected, BlePermissions.permissionsForApiLevel(30))
    }

    // ─── 分支: API 25 及以下 (低于 minSdk, 逻辑保留) ───────

    @Test
    fun `api 25 and below require fine location`() {
        val expected = listOf("android.permission.ACCESS_FINE_LOCATION")
        assertEquals(expected, BlePermissions.permissionsForApiLevel(25))
        assertEquals(expected, BlePermissions.permissionsForApiLevel(0))
    }

    // ─── requiredPermissions 与当前设备分支一致 ────────────

    @Test
    fun `requiredPermissions matches current api branch`() {
        val expected = BlePermissions.permissionsForApiLevel(Build.VERSION.SDK_INT)
        assertEquals(expected, BlePermissions.requiredPermissions().toList())
    }
}
