package com.yuledkcs.sdk.ble

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build

/**
 * BLE 权限预检 — 2b-I 后台 BLE (契约 AD-6 / B2.5 / AC-6)
 *
 * Android 权限矩阵 (宿主 App 需在 AndroidManifest.xml 声明, SDK 侧做运行期预检):
 * - API 31+ (Android 12+): BLUETOOTH_SCAN + BLUETOOTH_CONNECT (运行时权限)
 * - API 30 及以下 (Android 6..11): ACCESS_FINE_LOCATION (扫描/连接依赖定位权限;
 *   API 25- 无运行时权限概念, minSdk 26 已排除该区间, 分支仍保留供单测覆盖)
 *
 * 用法:
 * - 启动 [YdkBleForegroundService] 前调用 [checkOrThrow]
 * - [BleManager.startBackgroundScan] 内部自动预检
 */
object BlePermissions {

    /**
     * 按 API level 返回所需权限列表 — 纯逻辑, 可单测。
     *
     * 保持 internal: 仅库内与测试可见, 不进入公开 API 面。
     */
    internal fun permissionsForApiLevel(apiLevel: Int): List<String> =
        if (apiLevel >= Build.VERSION_CODES.S) {
            listOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            listOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    /** 当前设备需要的权限列表 */
    fun requiredPermissions(): Array<String> =
        permissionsForApiLevel(Build.VERSION.SDK_INT).toTypedArray()

    /**
     * 权限预检 — 任一权限未授予则抛 [IllegalStateException] 并列出缺失项。
     *
     * @throws IllegalStateException 缺失权限时, message 包含缺失权限名列表
     */
    fun checkOrThrow(context: Context) {
        val missing = permissionsForApiLevel(Build.VERSION.SDK_INT).filter {
            context.checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isNotEmpty()) {
            throw IllegalStateException("缺少 BLE 权限: ${missing.joinToString(", ")}")
        }
    }
}
