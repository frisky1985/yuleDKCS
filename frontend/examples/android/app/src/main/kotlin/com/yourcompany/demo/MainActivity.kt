package com.yourcompany.demo

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.lifecycle.lifecycleScope
import com.yuledkcs.sdk.ble.AndroidNfcManager
import com.yuledkcs.sdk.ble.BleManager
import com.yuledkcs.sdk.ble.BlePermissions
import com.yuledkcs.sdk.ble.MockUwbManager
import com.yuledkcs.sdk.ble.NfcCommandType
import com.yuledkcs.sdk.ble.UwbManager
import com.yuledkcs.sdk.ble.YdkBleForegroundService
import com.yuledkcs.sdk.hub.HubClient
import com.yuledkcs.sdk.hub.SDKConfig
import com.yuledkcs.sdk.hub.acceptShare
import com.yuledkcs.sdk.hub.bindKey
import com.yuledkcs.sdk.hub.cancelShare
import com.yuledkcs.sdk.hub.createShare
import com.yuledkcs.sdk.hub.getKey
import com.yuledkcs.sdk.hub.getShare
import com.yuledkcs.sdk.hub.listKeys
import com.yuledkcs.sdk.hub.remoteLock
import com.yuledkcs.sdk.hub.remoteStart
import com.yuledkcs.sdk.hub.remoteStop
import com.yuledkcs.sdk.hub.remoteUnlock
import com.yuledkcs.sdk.hub.renewKey
import com.yuledkcs.sdk.hub.resumeKey
import com.yuledkcs.sdk.hub.revokeKey
import com.yuledkcs.sdk.hub.suspendKey
import com.yuledkcs.sdk.hub.unbindKey
import com.yuledkcs.sdk.keymanager.KeyManager
import kotlinx.coroutines.launch
import java.io.File

/**
 * yuleDKCS SDK Android 集成示例（最小骨架）
 *
 * 只展示 API 调用面，不复制 SDK 内部逻辑。
 * 所有调用与 mobile/android/sdk/src/main/kotlin 下的公开 API 签名一一对应。
 * 验证: 静态审查（AndroidManifest 权限 + API 签名对照）。
 */
class MainActivity : ComponentActivity() {

    private lateinit var hubClient: HubClient
    private lateinit var keyManager: KeyManager
    private lateinit var bleManager: BleManager
    private var nfc: AndroidNfcManager? = null
    private lateinit var logView: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        logView = TextView(this)
        setContentView(logView)

        // NFC 管理器（Reader 模式在 onResume/onPause 配对启用）
        nfc = AndroidNfcManager(this)

        // API 31+ 蓝牙扫描/连接为运行时权限；API 30- 需定位权限
        val perms = BlePermissions.requiredPermissions()
        if (perms.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }) {
            requestPermissions(perms, REQ_CODE_BLE)
        }

        lifecycleScope.launch {
            // ── 初始化 ──
            hubClient = HubClient.create(
                SDKConfig(hubEndpoint = "hub.yuletech.com", hubPort = 8080, enableLogging = true)
            )
            hubClient.setToken("session-token-from-oem-server")   // 车厂 Server 签发

            keyManager = KeyManager(hubClient, File(filesDir, "dkcs_cache"))
            keyManager.startAutoSync(intervalMs = 5 * 60 * 1000)  // 5 分钟自动同步

            bleManager = BleManager(this@MainActivity)
            bleManager.connectionChangeHandler = { state ->
                // 0=disconnected 1=scanning 2=connecting 3=connected
                log("BLE 连接状态: $state")
            }

            demo()
        }
    }

    override fun onResume() {
        super.onResume()
        nfc?.enableReaderMode(this)      // Reader 模式: 贴卡回调直达 Tag
    }

    override fun onPause() {
        super.onPause()
        nfc?.disableReaderMode(this)     // 必须与 enableReaderMode 配对
    }

    private suspend fun demo() {
        val vehicleId = "LSVX0000000000001"

        // ── 钥匙管理 ──
        val bindResp = hubClient.bindKey(vehicleId = vehicleId)
        log("绑定成功 keyId=${bindResp.keyId}")

        val keys = hubClient.listKeys(status = "ACTIVE")
        log("有效钥匙 ${keys.size} 把")
        val key = hubClient.getKey(bindResp.keyId)
        log("钥匙状态: ${key.status}")

        hubClient.suspendKey(bindResp.keyId, reason = "用户挂起")
        hubClient.resumeKey(bindResp.keyId)
        hubClient.revokeKey(bindResp.keyId, reason = "丢失")
        hubClient.renewKey(bindResp.keyId, validUntil = 1_900_000_000_000L)

        // ── 远程控车 ──
        val resp = hubClient.remoteUnlock(vehicleId = vehicleId)
        if (resp.resultCode == 0) log("远程解锁已受理 cmdId=${resp.cmdId}")
        hubClient.remoteLock(vehicleId = vehicleId)
        hubClient.remoteStart(vehicleId = vehicleId)
        hubClient.remoteStop(vehicleId = vehicleId)

        // ── BLE 扫描 / 连接 / 本地解锁 ──
        val vehicles = bleManager.scanVehicles(timeoutMs = 10_000)
        val nearby = vehicles.firstOrNull() ?: run {
            log("未扫描到车辆")
            return
        }
        log("发现车辆 ${nearby.vehicleId} rssi=${nearby.rssi}")

        val connectResult = bleManager.connect(address = nearby.vehicleId, autoConnect = false)
        if (connectResult.success) {
            // 离线授权裁决（Hub 不可达时 BLE 解锁的前置检查）
            val verdict = keyManager.authorizeOfflineUse(keyId = bindResp.keyId)
            if (verdict != null && verdict.allowed) {
                bleManager.unlock(nearby.vehicleId)
                bleManager.lock(nearby.vehicleId)
                bleManager.startEngine(nearby.vehicleId)
                val status = bleManager.readVehicleStatus(nearby.vehicleId)
                log("车辆状态 locked=${status.locked} battery=${status.batteryPct}%")
            } else {
                log("离线解锁被拒: ${verdict?.reason}")
            }
            bleManager.disconnect()
        }

        // ── 分享: Hub 分享码 ──
        val share = hubClient.createShare(
            keyId = bindResp.keyId,
            toVendor = "XIAOMI",
            validUntil = 1_900_000_000_000L,
            maxUses = 0
        )
        share.shareCode?.let { log("分享码: $it") }

        // 接收方接受分享
        val sharedKey = hubClient.acceptShare(shareCode = "123456")
        log("接受分享成功 keyId=${sharedKey.keyId}")

        // 取消 / 查询
        hubClient.cancelShare(shareId = share.shareId)
        val detail = hubClient.getShare(shareId = share.shareId)
        log("分享详情 errorCode=${detail.errorCode ?: "无"}")

        // ── Push 触发的增量同步 ──
        keyManager.handleKeyStatusPush(keyId = bindResp.keyId)

        // ── UWB 测距（API 34+ 真机; 低版本降级 Mock）──
        val uwb: UwbManager = try {
            com.yuledkcs.sdk.ble.AndroidUwbManager(this).also { manager ->
                manager.rangingResultHandler = { m ->
                    log("UWB 距离 ${m.distance}m")
                }
            }
        } catch (e: IllegalStateException) {
            MockUwbManager()   // API < 34 或硬件缺失
        }
        uwb.rangingResultHandler = { m -> log("UWB 测距: ${m.distance}m") }
        uwb.startRanging(vehicleId)
        uwb.stopRanging()

        // ── NFC 备用解锁（Reader 模式已在 onResume 启用）──
        val nfcManager = nfc ?: return
        if (nfcManager.isNfcAvailable && nfcManager.isNfcEnabled) {
            val tagInfo = nfcManager.readVehicleTag()
            log("NFC 标签 vehicleId=${tagInfo.vehicleId} tagId=${tagInfo.tagId}")
            nfcManager.sendCommandViaNfc(NfcCommandType.UNLOCK)   // LOCK / START_ENGINE
        } else {
            log("NFC 不可用: available=${nfcManager.isNfcAvailable} enabled=${nfcManager.isNfcEnabled}")
        }

        // ── 后台 BLE 前台服务（可选）──
        YdkBleForegroundService.onScanResults = { found ->
            log("后台发现车辆: ${found.map { it.vehicleId }}")
        }
        YdkBleForegroundService.start(this, timeoutMs = 30_000, vehicleIds = setOf(vehicleId))
        YdkBleForegroundService.stop(this)

        // ── 登出清理 ──
        hubClient.unbindKey(bindResp.keyId)
        hubClient.clearToken()
        keyManager.stopAutoSync()
        keyManager.clearCache()
        bleManager.shutdown()
        hubClient.shutdown()
    }

    private fun log(msg: String) {
        logView.append("$msg\n")
        logView.append("─────────────────\n")
    }

    private companion object {
        const val REQ_CODE_BLE = 1001
    }
}
