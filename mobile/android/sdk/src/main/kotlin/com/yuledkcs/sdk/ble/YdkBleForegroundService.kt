package com.yuledkcs.sdk.ble

import android.annotation.SuppressLint
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

/**
 * 数字钥匙后台 BLE 前台服务 — 2b-I (契约 AD-5 / AD-8 / B2.1 / B2.2 / AC-4)
 *
 * 平台事实:
 * - Android 8+ (API 26): 后台 BLE 扫描受限 (约 30 秒窗口)
 * - Android 12+ (API 31): 后台无法启动扫描 → 必须前台服务包裹
 * - 后台重连: BleManager.connect(autoConnect = true) 由系统维护
 *
 * 边界 (AD-8): SDK 提供能力组件, 生命周期归宿主 App 管理:
 * - 宿主 AndroidManifest.xml 声明 service + 权限 (见 docs/sdk/BLE-BACKGROUND-INTEGRATION.md)
 * - 宿主调用 [start] 启动、[stop] 停止
 *
 * 扫描结果经 [onScanResults] 静态回调传出 (宿主在 [start] 前设置); 缺省仅记录日志。
 */
@SuppressLint("MissingPermission") // 权限由 BlePermissions.checkOrThrow 运行期预检
class YdkBleForegroundService : Service() {

    private val serviceScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private var bleManager: BleManager? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        // 持有 BleManager (契约 B2.1): 复用现有扫描/连接路径, 不新造
        bleManager = BleManager(this)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                // 契约 B2.2: 停止时移除前台通知并自杀
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
            // ACTION_START 或 null (系统 sticky 重建, intent 为 null): 恢复前台 + 重启后台扫描
            else -> {
                startForeground(NOTIFICATION_ID, buildNotification())
                val timeoutMs = intent?.getLongExtra(EXTRA_TIMEOUT_MS, DEFAULT_TIMEOUT_MS)
                    ?: DEFAULT_TIMEOUT_MS
                val vehicleIds = intent?.getStringArrayListExtra(EXTRA_VEHICLE_IDS)?.toSet()
                startBackgroundScan(timeoutMs, vehicleIds)
            }
        }
        // sticky: 系统杀死后尝试重建 (重建时 intent=null 走 else 分支恢复前台, 避免 ANR)
        return START_STICKY
    }

    override fun onDestroy() {
        serviceScope.cancel()
        bleManager?.shutdown()
        bleManager = null
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    // MARK: 内部实现

    /** 启动后台扫描协程 (契约 B2.3: 复用 BleManager.startBackgroundScan) */
    private fun startBackgroundScan(timeoutMs: Long, vehicleIds: Set<String>?) {
        try {
            // 权限预检: 缺失权限抛 IllegalStateException, 服务保持前台由宿主兜底
            BlePermissions.checkOrThrow(this)
        } catch (e: IllegalStateException) {
            Log.e(TAG, "BLE 权限缺失, 后台扫描未启动: ${e.message}")
            return
        }
        serviceScope.launch {
            try {
                val results = bleManager?.startBackgroundScan(timeoutMs, vehicleIds).orEmpty()
                onScanResults?.invoke(results)
                Log.i(TAG, "后台扫描完成, 发现 ${results.size} 辆车")
            } catch (e: Exception) {
                // 扫描异常不致命: 服务保持前台, 宿主可结合推送决定重试
                Log.e(TAG, "后台扫描失败", e)
            }
        }
    }

    /** 常驻通知渠道 (API 26+ 必需); minSdk 26, 防御性保留版本判断 */
    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "数字钥匙后台服务",
                NotificationManager.IMPORTANCE_LOW // 常驻低优先级, 不打扰用户
            ).apply {
                description = "维持数字钥匙 BLE 后台扫描与连接"
            }
            getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
        }
    }

    /** 常驻通知 — 图标用系统占位, 生产需替换为宿主/品牌图标 (契约 B2.2) */
    private fun buildNotification(): Notification {
        val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
        }
        return builder
            .setSmallIcon(android.R.drawable.ic_lock_idle_lock) // 占位图标, 生产替换
            .setContentTitle("数字钥匙服务运行中")
            .setContentText("正在后台扫描车辆蓝牙信号")
            .setOngoing(true)
            .build()
    }

    companion object {
        private const val TAG = "YdkBleForegroundService"

        const val CHANNEL_ID = "yule_dkcs_ble_background"
        const val NOTIFICATION_ID = 1001

        const val ACTION_START = "com.yuledkcs.sdk.ble.action.START"
        const val ACTION_STOP = "com.yuledkcs.sdk.ble.action.STOP"
        const val EXTRA_TIMEOUT_MS = "com.yuledkcs.sdk.ble.extra.TIMEOUT_MS"
        const val EXTRA_VEHICLE_IDS = "com.yuledkcs.sdk.ble.extra.VEHICLE_IDS"

        private const val DEFAULT_TIMEOUT_MS = 30_000L

        /**
         * 后台扫描结果回调 — 宿主在 [start] 前设置。
         * 静态持有: Service 实例由系统创建, 宿主无法直接注入实例字段。
         */
        @Volatile
        var onScanResults: ((List<VehicleAdvertise>) -> Unit)? = null

        /**
         * 启动后台 BLE 前台服务。
         * @param timeoutMs 单次后台扫描超时 (毫秒)
         * @param vehicleIds 可选过滤: 仅扫描指定 vehicleId (或 MAC) 集合
         */
        fun start(context: Context, timeoutMs: Long = DEFAULT_TIMEOUT_MS, vehicleIds: Set<String>? = null) {
            val intent = Intent(context, YdkBleForegroundService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_TIMEOUT_MS, timeoutMs)
                vehicleIds?.let { putStringArrayListExtra(EXTRA_VEHICLE_IDS, ArrayList(it)) }
            }
            // minSdk 26: startForegroundService (API 26+) 可直接调用, 无需 androidx ContextCompat
            context.startForegroundService(intent)
        }

        /** 停止后台 BLE 前台服务 (走 ACTION_STOP → stopForeground + stopSelf) */
        fun stop(context: Context) {
            val intent = Intent(context, YdkBleForegroundService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }
}
