# BLE 后台集成指南 (2b-I)

> 宿主 App 集成文档。双端分写: iOS 部分 (W1) / Android 部分 (W2), 主导合并。
> 配套裁决文档: `docs/certification/ble-background-runtime.md`。
> NFC 备用解锁通道（2b-H）配置见: `docs/sdk/NFC-INTEGRATION.md`。

---

## iOS 部分

### 1. Info.plist 声明（宿主 App）

```xml
<key>UIBackgroundModes</key>
<array>
    <string>bluetooth-central</string>
</array>
```

> ⚠️ 未声明 `bluetooth-central` 时, restore identifier 不产生后台保持效果（SDK 无法替宿主声明）。

### 2. SDK API 用法

```swift
// 启用后台支持: 传入 restore identifier, 系统杀 App 后重建 CBCentralManager 并恢复
let manager = YDKBLEManager(enableLogging: true,
                            backgroundRestoreIdentifier: "com.yourcompany.dkcs.ble")

// 恢复流程: 系统重建后, 已连接/扫描中的外设经 onRestoreState 交回,
// 宿主/车厂 App 可据此调用 connectVehicle 重连（或等待系统自动 didConnect 接管）
manager.connectionChangeHandler = { state in
    // 0=disconnected 1=scanning 2=connecting 3=connected
}
```

### 3. 行为说明

- **连接唤醒**: SDK 连接时自动传 `CBConnectPeripheralOptionNotifyOnConnectionKey` + `NotifyOnDisconnectionKey`,
  系统在后台连接/断开时唤醒 App（无需宿主配置）。
- **后台扫描**: 保持 `AllowDuplicates=false`（后台时系统忽略该选项, 扫描发现率低属正常）。
- **恢复 ≠ 已连接**: `willRestoreState` 交回的外设不代表已连接, SDK 记录并复位状态,
  重连走 `connectVehicle`（`peripheralByVehicleId` 命中 / `retrieveConnectedPeripherals` 回退）。
- **真机验证**: 状态恢复仅在 App 被系统终止后发生, 需真机验证。

---

## Android 部分

### 1. AndroidManifest.xml 声明 (宿主 App)

SDK 为 library, 组件/权限由宿主 App 的 manifest 合并声明:

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <!-- 蓝牙 (API 31+) -->
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN"
        android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

    <!-- 定位 (API 30 及以下扫描/连接依赖; 若声明 neverForLocation 仍建议保留) -->
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

    <!-- 前台服务 (API 34+ 必需) -->
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE" />

    <application>
        <!-- 前台服务声明: connectedDevice 类型 (API 29+; API 34+ 必需) -->
        <service
            android:name="com.yuledkcs.sdk.ble.YdkBleForegroundService"
            android:exported="false"
            android:foregroundServiceType="connectedDevice" />
    </application>
</manifest>
```

要点:
- `BLUETOOTH_SCAN` 建议加 `neverForLocation` 标志 (Android 12+), 但 `ACCESS_FINE_LOCATION` 对 API 30- 设备仍必需。
- API 31+ 权限为运行时权限, 宿主需在启动服务前动态申请 (可用 SDK `BlePermissions.requiredPermissions()` 获取清单)。
- API 34+ (Android 14) 启动 connectedDevice 前台服务还要求 `FOREGROUND_SERVICE_CONNECTED_DEVICE` 权限, 否则抛 `ForegroundServiceStartNotAllowedException` / `SecurityException`。

### 2. 运行时权限申请示例 (宿主)

```kotlin
// 宿主 Activity/Fragment 中
val perms = BlePermissions.requiredPermissions()
if (perms.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }) {
    requestPermissions(perms, REQ_CODE_BLE)
}
```

### 3. 启动/停止后台服务示例

```kotlin
// 1) 设置扫描结果回调 (可选; 缺省仅 SDK 内部日志)
YdkBleForegroundService.onScanResults = { vehicles ->
    Log.i("HostApp", "后台发现车辆: ${vehicles.map { it.vehicleId }}")
}

// 2) 启动: 30 秒后台扫描, 只关注指定车辆 (vehicleId 或 MAC)
YdkBleForegroundService.start(context, timeoutMs = 30_000, vehicleIds = setOf("VH-2026-0001"))

// 3) 停止 (移除常驻通知)
YdkBleForegroundService.stop(context)
```

### 4. 后台重连示例

```kotlin
// 前台服务存活期间, 对已知车辆启用系统后台重连 (autoConnect = true)
scope.launch {
    val result = bleManager.connect(address = "AA:BB:CC:DD:EE:FF", autoConnect = true)
    if (result.success) {
        // 已连接; 断线后系统自动重连, 无需宿主干预
    }
}
```

### 5. 平台限制说明

- 前台服务停止后即受后台扫描限制 (API 26-30: 约 30 秒窗口; API 31+: 禁止启动扫描)。
- 常驻通知不可被用户划掉 (setOngoing(true)), 用户需经"停止服务"或系统设置停止。
- 通知图标当前为系统占位 (android.R.drawable.ic_lock_idle_lock), 生产版需替换为品牌图标。
- 长时间驻留建议: 宿主按需调用 `startBackgroundScan` 单次窗口 + 结合推送触发, 而非无限循环扫描。
