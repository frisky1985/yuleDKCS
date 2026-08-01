# yuleDKCS SDK 集成指南

> **版本**: v1.0
> **更新日期**: 2026-08-01
> **适用对象**: 车厂 App 客户端工程（iOS / Android）
> **配套文档**:
> - [SDK-ARCHITECTURE.md](./SDK-ARCHITECTURE.md) — 三层模型与职责边界
> - [BLE-BACKGROUND-INTEGRATION.md](./BLE-BACKGROUND-INTEGRATION.md) — BLE 后台集成细节
> - [NFC-INTEGRATION.md](./NFC-INTEGRATION.md) — NFC 备用解锁集成细节
> - [OFFLINE-FALLBACK-DESIGN.md](./OFFLINE-FALLBACK-DESIGN.md) — 离线授权回退设计
> - [LOOP-C-CONTRACT.md](./LOOP-C-CONTRACT.md) — 本批次契约（W1）

---

## 目录

1. [概述与集成全景](#1-概述与集成全景)
2. [iOS 集成](#2-ios-集成)
   - 2.1 环境要求
   - 2.2 Swift Package Manager 接入
   - 2.3 初始化
   - 2.4 钥匙管理（绑定/解绑/挂起/恢复/撤销/续期/查询）
   - 2.5 远程控车
   - 2.6 BLE 扫描 / 连接 / 本地解锁
   - 2.7 钥匙分享（Hub 分享码 + CCC Mailbox ShareFlow）
   - 2.8 离线授权
   - 2.9 UWB 精确测距
   - 2.10 NFC 备用解锁
   - 2.11 Info.plist / Entitlements 声明汇总
3. [Android 集成](#3-android-集成)
   - 3.1 环境要求
   - 3.2 Gradle 接入
   - 3.3 初始化
   - 3.4 钥匙管理 / 远程控车 / BLE / 分享 / 离线授权 / UWB / NFC 示例
   - 3.5 AndroidManifest 权限汇总
   - 3.6 前台服务（ForegroundService）用法
4. [集成 Checklist（车厂侧配置项）](#4-集成-checklist车厂侧配置项)
5. [错误处理与降级策略](#5-错误处理与降级策略)
6. [最小示例工程](#6-最小示例工程)

---

## 1. 概述与集成全景

### 1.1 SDK 形态

yuleDKCS SDK 采用**三层模型**（详见架构文档）：

```
┌────────────────────────────────────────────────────────────┐
│ Layer 1: 车厂 App（登录/UI/session token/钱包层，车厂自写） │
├────────────────────────────────────────────────────────────┤
│ Layer 2: yuleDKCS SDK（本指南集成对象）                     │
│   iOS:    YDKHubClient / YDKKeyManager / YDKBLEManager     │
│           (+ YDKShareFlow / YDKUWBManager / YDKNFCManager) │
│   Android: HubClient / KeyManager / BleManager             │
│           (+ ShareFlow / UwbManager / NfcManager)          │
├────────────────────────────────────────────────────────────┤
│ Layer 3: Hub REST Gateway (:8080) / 车辆 TCU (BLE/UWB/NFC) │
└────────────────────────────────────────────────────────────┘
```

### 1.2 SDK 边界（重要）

| SDK 做 | SDK 不做（车厂自建） |
|:-------|:---------------------|
| Hub gRPC/REST 通信、钥匙生命周期、分享编排、BLE/UWB/NFC 协议 | 用户登录注册、session token 获取与刷新 |
| 钥匙状态本地缓存 + 定时同步 + 离线授权裁决 | 钱包层 API（Apple CarKey / Samsung Wallet / 小米华为钱包） |
| Push 回调处理（KeyManager.handleKeyStatusPush） | Push 通道本身的接收（APNs/FCM 由宿主配置） |
| 错误类型标准化（YDKError） | UI（按钮/列表/弹窗） |

> ⚠️ 绑钥匙成功后，**添加至手机钱包**（Apple Wallet / 厂商钱包）必须由车厂 App 调用钱包层 SDK 完成，yuleDKCS SDK 不包含钱包能力。

### 1.3 双端模块/包对照

| 能力 | iOS 产物（import） | Android 包（import） |
|:-----|:-------------------|:---------------------|
| Hub 通信 + 钥匙/分享/远程控车 | `YDKHubClient` | `com.yuledkcs.sdk.hub` |
| CCC Mailbox 分享编排 | `YDKHubClient`（YDKShareFlow） | `com.yuledkcs.sdk.mailbox` |
| 钥匙状态管理 + 离线授权 | `YDKKeyManager` | `com.yuledkcs.sdk.keymanager` |
| BLE 扫描/连接/控制 + 前台服务 | `YDKBLEManager` | `com.yuledkcs.sdk.ble` |
| UWB 测距 | `YDKBLEManager`（YDKNIUWBManager / YDKMockUWBManager） | `com.yuledkcs.sdk.ble`（AndroidUwbManager / MockUwbManager） |
| NFC 备用解锁 | `YDKBLEManager`（YDKCoreNFCManager） | `com.yuledkcs.sdk.ble`（AndroidNfcManager） |
| 设备信息/密钥（SDK 内部自动使用） | `YDKHubClient`（YDKDeviceManager，internal） | `com.yuledkcs.sdk.device` |

---

## 2. iOS 集成

### 2.1 环境要求

- iOS 15.0+（SDK `Package.swift` 声明 `.iOS(.v15)`）
- Xcode 15+（Swift 5.9+）
- 真机能力要求：BLE 解锁需 iPhone 6s+；UWB 需 U1/U2 芯片机型（iPhone 11+）；NFC 需 iPhone 7+（iOS 11+）且需 entitlement
- 证书：Apple Developer Program 付费账号（NFC Tag Reading entitlement 需在开发者后台开通）

### 2.2 Swift Package Manager 接入

**方式 A — 本地路径依赖（开发/示例，见 examples/ios/DemoApp）**：

```swift
// 车厂 App 的 Package.swift（或 Xcode → File → Add Package Dependency → Local）
dependencies: [
    .package(name: "yuleDKCS-SDK", path: "path/to/yuleDKCS/mobile/ios"),
],
targets: [
    .target(
        name: "OEMApp",
        dependencies: [
            .product(name: "YDKHubClient", package: "yuleDKCS-SDK"),
            .product(name: "YDKKeyManager", package: "yuleDKCS-SDK"),
            .product(name: "YDKBLEManager", package: "yuleDKCS-SDK"),
        ]
    ),
]
```

**方式 B — Git 依赖（发布后）**：

```swift
.package(url: "https://github.com/yuletech/yuleDKCS-SDK.git", from: "1.0.0"),
```

> SDK 对外导出 3 个 library 产物：`YDKHubClient`、`YDKKeyManager`、`YDKBLEManager`。
> `YDKShareFlow` / `YDKMailboxClient` 位于 `YDKHubClient` 目标内，`YDKUWBManager` / `YDKNFCManager` 位于 `YDKBLEManager` 目标内——按上表 import 对应产物即可，无需额外声明依赖。

### 2.3 初始化

```swift
import YDKHubClient
import YDKKeyManager
import YDKBLEManager

// ── 1. Hub 客户端（App 启动时创建一次，全局持有）──
let config = SDKConfig(
    hubEndpoint: "hub.yuletech.com",   // 车厂部署的 Hub REST Gateway 地址
    hubPort: 8080,
    platform: .iOS,
    enableLogging: true                // 生产建议 false
)
let hubClient = YDKHubClient(config: config)

// ── 2. 登录后注入 session token（token 由车厂 Server 签发，SDK 不存储）──
hubClient.setToken("session-token-from-oem-server")

// ── 3. 钥匙状态管理器（本地缓存 + 定时同步）──
let keyManager = YDKKeyManager(hubClient: hubClient, enableLogging: true)
keyManager.delegate = self   // 实现 YDKKeyManagerDelegate 接收变更通知

// ── 4. BLE 管理器（后台场景传入 restore identifier，见 2.11 权限）──
let bleManager = YDKBLEManager(
    enableLogging: true,
    backgroundRestoreIdentifier: "com.yourcompany.dkcs.ble"  // 可选，启用系统状态恢复
)
bleManager.connectionChangeHandler = { state in
    // 0=disconnected 1=scanning 2=connecting 3=connected
    print("BLE connection state: \(state)")
}
```

**登出/退出**：

```swift
hubClient.clearToken()      // 清除 token
keyManager.clearCache()     // 清除本地钥匙缓存（可选）
hubClient.shutdown()        // 关闭 URLSession
bleManager.disconnect()     // 断开 BLE（如有连接）
```

### 2.4 钥匙管理

```swift
// 绑定钥匙（SDK 自动填充 deviceId/公钥/vendor/protocol/keyType）
let bindResp = try await hubClient.bindKey(vehicleId: "LSVX0000000000001")
print("绑定成功 keyId=\(bindResp.keyId)")

// 查询
let keys = try await hubClient.listKeys()                 // 全部钥匙
let ownerKeys = try await hubClient.listKeys(status: "ACTIVE")
let key = try await hubClient.getKey(keyId: bindResp.keyId)

// 生命周期操作
try await hubClient.suspendKey(keyId: bindResp.keyId, reason: "用户挂起")
try await hubClient.resumeKey(keyId: bindResp.keyId)
try await hubClient.revokeKey(keyId: bindResp.keyId, reason: "丢失")
try await hubClient.renewKey(keyId: bindResp.keyId, validUntil: 1_900_000_000_000)
try await hubClient.unbindKey(keyId: bindResp.keyId)

// Push 触发增量同步（App 收到「钥匙被撤销/挂起」Push 后调用）
let changed = try await keyManager.handleKeyStatusPush(keyId: "key-xxx")
```

**KeyManager 变更通知**（实现 `YDKKeyManagerDelegate`）：

```swift
extension ViewController: YDKKeyManagerDelegate {
    func keyManager(_ manager: YDKKeyManager, didDetectChanges changes: [KeyChange]) {
        for change in changes {
            // change.type: .added / .updated / .removed
            print("钥匙变更 \(change.keyId): \(change.type.rawValue)")
        }
    }
    func keyManager(_ manager: YDKKeyManager, syncDidFailWith error: Error) {
        print("同步失败: \(error.localizedDescription)")
    }
}
```

### 2.5 远程控车

```swift
// 远程指令走 Hub（需网络）；返回值含 cmdId，可用于后续状态轮询
let resp = try await hubClient.remoteUnlock(vehicleId: "LSVX0000000000001")
if resp.resultCode == 0 {
    print("远程解锁指令已受理 cmdId=\(resp.cmdId ?? "")")
}

try await hubClient.remoteLock(vehicleId: "LSVX0000000000001")
try await hubClient.remoteStart(vehicleId: "LSVX0000000000001")
try await hubClient.remoteStop(vehicleId: "LSVX0000000000001")
// 可传 keyId 指定使用的钥匙（多钥匙场景）：remoteUnlock(vehicleId:keyId:)
```

### 2.6 BLE 扫描 / 连接 / 本地解锁

```swift
// 扫描（默认 10s 超时；按 CCC/ICCOA/ICCE 协议 service UUID 过滤 + 广告解析）
let vehicles = try await bleManager.scanVehicles(timeout: 10)
for v in vehicles {
    print("发现车辆 \(v.vehicleId) rssi=\(v.rssi) supportsUWB=\(v.supportsUWB)")
}

// 连接（从扫描结果按 vehicleId 匹配；支持系统已连接外设回退）
try await bleManager.connectVehicle(vehicleId: vehicles.first!.vehicleId)

// 本地控制（BLE 通道，离线可用；SDK 使用手机 SE 中的钥匙做端到端加密）
try await bleManager.unlock(vehicleId: vehicleId)
try await bleManager.lock(vehicleId: vehicleId)
try await bleManager.startEngine(vehicleId: vehicleId)

// 读取车辆状态
let status = try await bleManager.readVehicleStatus(vehicleId: vehicleId)
print("locked=\(status.locked) engineOn=\(status.engineOn) battery=\(status.batteryPct)%")

// 断开
try await bleManager.disconnect()
```

> **离线解锁推荐流程**：先 `keyManager.authorizeOfflineUse(keyId:)` 做离线授权裁决，允许后再走 BLE unlock（见 2.8）。

### 2.7 钥匙分享

**方式 A — 分享码（Hub 直连，通用）**：

```swift
// 发送方：创建分享（toVendor 必传，如 "APPLE"/"XIAOMI"；toUserId 空则生成 6 位分享码）
let share = try await hubClient.createShare(
    keyId: "key-xxx",
    toVendor: "XIAOMI",
    validUntil: 1_900_000_000_000,   // 0 = 无期限
    maxUses: 0                        // 0 = 不限次数
)
let shareCode = share.shareCode        // 通过短信/微信分享给好友

// 接收方：输入分享码接受
let newKey = try await hubClient.acceptShare(shareCode: shareCode)
print("接受成功 keyId=\(newKey.keyId)")

// 发送方取消 / 查询
try await hubClient.cancelShare(shareId: share.shareId)
let shareDetail = try await hubClient.getShare(shareId: share.shareId)
```

**方式 B — CCC Mailbox 分享（CCC 协议专用，高安全）**：

```swift
import YDKHubClient

let flow = YDKShareFlow(hubEndpoint: "hub.yuletech.com", port: 8080)

// 发送方：创建 Mailbox，返回分享 URL（https://host/api/v1/mailbox/{id}#{secret}）
let sharingURL = try await flow.shareKeyViaMailbox(
    payload: encryptedKeyData,          // 钱包层生成的加密钥匙材料
    displayInfo: brandIconData,         // 可选：展示信息（品牌/车型）
    senderVendor: "APPLE",
    senderDeviceId: "iphone-001",
    host: "hub.yuletech.com:8080",
    expirationSeconds: 86400,           // 默认 24h
    maxUpdates: 10
)

// 接收方：解析 URL → 读展示信息 → 读密文 → 回写 KeySigning/Import（钱包层材料）
let content = try await flow.acceptSharedKeyViaMailbox(
    urlString: sharingURL,
    updaterDeviceId: "xiaomi-001",
    keySigningPayload: signedKeyData,   // 钱包层签名结果，务必传真实数据
    importPayload: importAckData        // 钱包层导入确认
)

// 取消（按角色）
try await flow.senderCancelMailboxShare(mailboxId: "mb-xxx", updaterDeviceId: "iphone-001")
try await flow.receiverCancelMailboxShare(mailboxId: "mb-xxx", updaterDeviceId: "xiaomi-001")
```

> secret 位于 URL fragment，SDK 只追加不发送到 Relay Server（零知识）；URL 需通过安全渠道（端到端加密聊天）传递。

### 2.8 离线授权

BLE/NFC 解锁不依赖网络，但钥匙是否可用需本地裁决（fail-closed）：

```swift
// 离线解锁前调用；nil = 本地无此钥匙
if let verdict = keyManager.authorizeOfflineUse(keyId: "key-xxx") {
    if verdict.allowed {
        try await bleManager.unlock(vehicleId: vehicleId)
    } else {
        // verdict.reason: .revoked / .suspended / .expired / .notYetValid / .staleCache
        print("离线解锁被拒: \(verdict.reason?.rawValue ?? "unknown")")
    }
}
```

也可直接用纯函数裁决器（KeyManager 内部即调用它）：

```swift
let verdict = YDKOfflineAuthorizer.authorize(
    key: key,
    now: Date(),
    lastSyncAtMillis: lastSyncMillis,
    maxOfflineGrace: 7 * 24 * 3600   // 默认 7 天
)
```

### 2.9 UWB 精确测距

```swift
import YDKBLEManager

// 真机（iOS 14+，U1/U2 芯片）：
let uwb = YDKNIUWBManager()
uwb.rangingResultHandler = { m in
    print("距离 \(m.vehicleId): \(m.distance)m 方位 \(m.azimuth ?? 0)°")
}
uwb.sessionInvalidatedHandler = { error in
    // 用户拒绝授权 / 系统限制 / 退后台失效
}
uwb.sessionSuspensionHandler = { suspended in
    // true=已挂起（退后台），false=已恢复（回前台）
}

// token 交换：车端 token 经 BLE 通道下发后注入
try uwb.injectPeerDiscoveryToken(data: peerTokenDataFromBLE)
let localToken = try uwb.exportLocalDiscoveryToken()   // 上行写入车端

try await uwb.startRanging(vehicleId: vehicleId)
// ... 使用完毕后
uwb.stopRanging()

// 无硬件 / 调试环境：Mock 管理器（每秒产生模拟测距）
let mock = YDKMockUWBManager()
mock.rangingResultHandler = { m in print("mock 距离 \(m.distance)m") }
try await mock.startRanging(vehicleId: vehicleId)
```

### 2.10 NFC 备用解锁

```swift
import YDKBLEManager

// expectedTagId 可选：绑定标签 ID，防错贴
let nfc = YDKCoreNFCManager(expectedTagId: "04A1B2C3D4E5F6")

// 读取车辆标签（返回 vehicleId + tagId）
let info = try await nfc.readVehicleTag()

// 发送指令（用户需将 iPhone 靠近车辆 NFC 标签）
try await nfc.sendCommandViaNFC(command: .unlock)   // .lock / .startEngine

// 错误处理：YDKNFCError 覆盖 无硬件/未授权/未检测到/标签不匹配/指令被拒/超时/用户取消
do { try await nfc.sendCommandViaNFC(command: .lock) }
catch let e as YDKNFCError {
    print("NFC 失败: \(e.errorDescription ?? "")")
}
```

### 2.11 Info.plist / Entitlements 声明汇总

**Info.plist**（车厂 App 工程）：

```xml
<!-- 蓝牙：后台 BLE（状态恢复必需） -->
<key>UIBackgroundModes</key>
<array>
    <string>bluetooth-central</string>
</array>

<!-- NFC 读取（备用解锁） -->
<key>NFCReaderUsageDescription</key>
<string>用于在手机无网络/无电场景下通过车辆 NFC 标签执行备用解锁</string>

<!-- UWB 测距（iOS 15+；iOS 14 用 NSNearbyInteractionUsageDescription） -->
<key>NSNearbyInteractionAllowOnceUsageDescription</key>
<string>用于靠近车辆时提供精确测距解锁</string>

<!-- 网络（ATS：如 Hub 使用自签证书需配置例外，生产建议正规 CA） -->
<key>NSAppTransportSecurity</key>
<dict>
    <key>NSAllowsArbitraryLoads</key>
    <false/>
</dict>
```

**Entitlements**（`.entitlements` 文件 / Xcode → Signing & Capabilities）：

```xml
<!-- NFC Tag Reading（真机必需；需在 Apple Developer 后台开通该 capability） -->
<key>com.apple.developer.nfc.readersession.formats</key>
<array>
    <string>NDEF</string>
    <string>TAG</string>
</array>
```

> ⚠️ 缺少 NFC entitlement 时 `NFCTagReaderSession` 立即失效（`YDKNFCError.sessionCreationFailed`）。
> 未声明 `bluetooth-central` 时，`backgroundRestoreIdentifier` 不产生后台保持效果。
> UWB 无 entitlement 要求，但需 usage description，否则首次 `session.run` 即失效。

---

## 3. Android 集成

### 3.1 环境要求

- Android 8.0+（`minSdk 26`）；compileSdk 35
- JDK 17、AGP 8.7.3+、Kotlin 2.0.21+
- 真机能力：BLE 扫描需 API 31+ 运行时权限；UWB 需 Android 14+（API 34）且硬件支持；NFC 需硬件（普通权限）
- 签名：release keystore（正式发布）

### 3.2 Gradle 接入

`settings.gradle.kts`（示例见 examples/android）：

```kotlin
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories { google(); mavenCentral() }
}
rootProject.name = "OEMApp"
include(":app")
include(":sdk")
project(":sdk").projectDir = File(rootDir, "../yuleDKCS/mobile/android/sdk") // 本地路径
```

`app/build.gradle.kts`：

```kotlin
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.yourcompany.oemapp"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.yourcompany.oemapp"
        minSdk = 26
        targetSdk = 35
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation(project(":sdk"))                    // yuleDKCS SDK
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.0")  // lifecycleScope
}
```

> 发布后也可切换为 Maven 坐标：`implementation("com.yuledkcs:sdk:1.0.0")`。

### 3.3 初始化

```kotlin
package com.yourcompany.oemapp

import android.content.Context
import com.yuledkcs.sdk.ble.BleManager
import com.yuledkcs.sdk.hub.HubClient
import com.yuledkcs.sdk.hub.SDKConfig
import com.yuledkcs.sdk.keymanager.KeyManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

class DkcsManager(private val appContext: Context) {

    lateinit var hubClient: HubClient
        private set
    lateinit var keyManager: KeyManager
        private set
    lateinit var bleManager: BleManager
        private set

    /** App 启动 / 登录后调用（suspend：HubClient.create 走 IO） */
    suspend fun init(sessionToken: String) {
        hubClient = HubClient.create(
            SDKConfig(hubEndpoint = "hub.yuletech.com", hubPort = 8080, enableLogging = true)
        )
        hubClient.setToken(sessionToken)

        keyManager = KeyManager(hubClient, File(appContext.filesDir, "dkcs_cache"))
        keyManager.startAutoSync(intervalMs = 5 * 60 * 1000)   // 5 分钟自动同步

        bleManager = BleManager(appContext)
        bleManager.connectionChangeHandler = { state ->
            // 0=disconnected 1=scanning 2=connecting 3=connected
        }
    }

    /** 登出 */
    fun logout() {
        hubClient.clearToken()
        keyManager.stopAutoSync()
        keyManager.clearCache()
        bleManager.shutdown()
        hubClient.shutdown()
    }
}
```

### 3.4 功能示例（与 iOS 一一对应）

**钥匙管理**：

```kotlin
import com.yuledkcs.sdk.hub.*   // HubClient + 扩展函数 + YDKKey/YDKShare

val bindResp = hubClient.bindKey(vehicleId = "LSVX0000000000001")
val keys = hubClient.listKeys(status = "ACTIVE")
val key = hubClient.getKey(bindResp.keyId)

hubClient.suspendKey(bindResp.keyId, reason = "用户挂起")
hubClient.resumeKey(bindResp.keyId)
hubClient.revokeKey(bindResp.keyId, reason = "丢失")
hubClient.renewKey(bindResp.keyId, validUntil = 1_900_000_000_000L)
hubClient.unbindKey(bindResp.keyId)

// Push 触发增量同步
keyManager.handleKeyStatusPush(keyId = "key-xxx")
// 观察钥匙列表：keyManager.keys（StateFlow）
```

**远程控车**：

```kotlin
val resp = hubClient.remoteUnlock(vehicleId = "LSVX0000000000001")
if (resp.resultCode == 0) { /* 已受理 */ }
hubClient.remoteLock(vehicleId = "LSVX0000000000001")
hubClient.remoteStart(vehicleId = "LSVX0000000000001")
hubClient.remoteStop(vehicleId = "LSVX0000000000001")
```

**BLE 扫描 / 连接 / 本地解锁**：

```kotlin
import com.yuledkcs.sdk.ble.BleManager

// 扫描（10s 超时；可按 vehicleId/MAC 过滤）
val vehicles = bleManager.scanVehicles(timeoutMs = 10_000, vehicleIds = setOf("VH-2026-0001"))
val vehicle = vehicles.first()

// 连接（address 传 vehicleId 或 MAC；autoConnect=true 启用系统后台重连）
val result = bleManager.connect(address = vehicle.vehicleId, autoConnect = false)
if (result.success) {
    bleManager.unlock(vehicle.vehicleId)     // 返回 LocalControlResponse
    bleManager.lock(vehicle.vehicleId)
    bleManager.startEngine(vehicle.vehicleId)
    val status = bleManager.readVehicleStatus(vehicle.vehicleId)
}
bleManager.disconnect()
```

**分享（分享码）**：

```kotlin
val share = hubClient.createShare(
    keyId = "key-xxx",
    toVendor = "XIAOMI",
    validUntil = 1_900_000_000_000L,
    maxUses = 0
)
val code = share.shareCode                  // 分享给好友

val newKey = hubClient.acceptShare(shareCode = code)   // 接收方
hubClient.cancelShare(shareId = share.shareId)
val detail = hubClient.getShare(shareId = share.shareId)
```

**分享（CCC Mailbox）**：

```kotlin
import com.yuledkcs.sdk.mailbox.*

val mailbox = MailboxClient(hubEndpoint = "hub.yuletech.com", port = 8080)

// 发送方
val url = mailbox.shareKeyViaMailbox(
    payload = encryptedKeyData,
    senderVendor = "APPLE",
    senderDeviceId = "iphone-001",
    host = "hub.yuletech.com:8080"
)

// 接收方
val content = mailbox.acceptSharedKeyViaMailbox(
    urlString = url,
    updaterDeviceId = "xiaomi-001",
    keySigningPayload = signedKeyData,
    importPayload = importAckData
)

// 取消
mailbox.senderCancelMailboxShare(mailboxId = "mb-xxx", updaterDeviceId = "iphone-001")
```

**离线授权**：

```kotlin
val verdict = keyManager.authorizeOfflineUse(keyId = "key-xxx") ?: return
if (verdict.allowed) {
    bleManager.unlock(vehicleId)
} else {
    // verdict.reason: REVOKED / SUSPENDED / EXPIRED / NOT_YET_VALID / STALE_CACHE
}
```

**UWB 测距**（Android 14+）：

```kotlin
import com.yuledkcs.sdk.ble.AndroidUwbManager
import com.yuledkcs.sdk.ble.MockUwbManager
import com.yuledkcs.sdk.ble.UwbManager

val uwb: UwbManager = try {
    AndroidUwbManager(context).also { manager ->
        manager.rangingResultHandler = { m ->
            runOnUiThread { tv.text = "距离 ${m.distance}m" }
        }
    }
} catch (e: IllegalStateException) {
    MockUwbManager()   // API < 34 或硬件缺失时降级
}

uwb.rangingResultHandler = { m -> /* 测距结果 */ }
// 注意：AndroidUwbManager.startRanging 内部会 UwbVersionPolicy.requireSupported(API 34+)
// 会话仅前台有效；退后台后回前台需重新 startRanging
```

**NFC 备用解锁**（推荐 Reader 模式）：

```kotlin
import com.yuledkcs.sdk.ble.AndroidNfcManager
import com.yuledkcs.sdk.ble.NfcCommandType

val nfc = AndroidNfcManager(context)

override fun onResume() {
    super.onResume()
    nfc.enableReaderMode(this)              // onPause 配对 disableReaderMode
}

lifecycleScope.launch {
    val info = nfc.readVehicleTag()          // 贴卡后返回 vehicleId + tagId
    nfc.sendCommandViaNfc(NfcCommandType.UNLOCK)   // LOCK / START_ENGINE
}
// 异常：NfcUnavailableException（无硬件）/ NfcDisabledException（未开启）
//        / NfcTagNotSupportedException / NfcCommandException
```

### 3.5 AndroidManifest 权限汇总

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <!-- 网络：Hub REST Gateway -->
    <uses-permission android:name="android.permission.INTERNET" />

    <!-- 蓝牙 (API 31+)：运行时权限 -->
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN"
        android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

    <!-- 定位 (API 30 及以下扫描/连接依赖) -->
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

    <!-- 前台服务 (API 34+ 必需 FOREGROUND_SERVICE_CONNECTED_DEVICE) -->
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE" />

    <!-- NFC（普通权限，无需运行时申请） -->
    <uses-permission android:name="android.permission.NFC" />
    <uses-feature android:name="android.hardware.nfc" android:required="false" />
    <uses-feature android:name="android.hardware.uwb" android:required="false" />

    <application>
        <!-- yuleDKCS 后台 BLE 前台服务（connectedDevice 类型） -->
        <service
            android:name="com.yuledkcs.sdk.ble.YdkBleForegroundService"
            android:exported="false"
            android:foregroundServiceType="connectedDevice" />
    </application>
</manifest>
```

**运行时权限申请**（API 31+ 蓝牙扫描/连接）：

```kotlin
import com.yuledkcs.sdk.ble.BlePermissions

val perms = BlePermissions.requiredPermissions()   // 按 API level 返回 BLUETOOTH_SCAN/CONNECT 或 ACCESS_FINE_LOCATION
if (perms.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }) {
    requestPermissions(perms, REQ_CODE_BLE)
}
```

### 3.6 前台服务（ForegroundService）用法

Android 8+ 后台 BLE 扫描受限（约 30 秒窗口），Android 12+ 后台禁止启动扫描——SDK 提供 `YdkBleForegroundService` 包裹后台扫描（见 [BLE-BACKGROUND-INTEGRATION.md](./BLE-BACKGROUND-INTEGRATION.md)）：

```kotlin
import com.yuledkcs.sdk.ble.YdkBleForegroundService

// 1) 可选：设置扫描结果回调（缺省仅日志）
YdkBleForegroundService.onScanResults = { vehicles ->
    Log.i("OEMApp", "后台发现车辆: ${vehicles.map { it.vehicleId }}")
}

// 2) 启动：30 秒后台扫描，可过滤指定车辆（vehicleId 或 MAC）
YdkBleForegroundService.start(
    context = this,
    timeoutMs = 30_000,
    vehicleIds = setOf("VH-2026-0001")
)

// 3) 停止（移除常驻通知）
YdkBleForegroundService.stop(context = this)
```

**后台重连**（前台服务存活期间）：

```kotlin
scope.launch {
    val result = bleManager.connect(address = "AA:BB:CC:DD:EE:FF", autoConnect = true)
    // autoConnect=true：断线后系统自动重连，无需宿主干预
}
```

---

## 4. 集成 Checklist（车厂侧配置项）

### 4.1 通用

- [ ] Hub REST Gateway 地址（`hub.yuletech.com:8080`）已与 yuletech 确认并走 HTTPS（TLS）
- [ ] 车厂 Server 与 Hub 的 session token 签发/校验联调完成（SDK 只透传 token）
- [ ] 绑定流程联调：`bindKey` → 车厂钱包层添加钥匙 → 钱包 UI 展示
- [ ] Push 通道就绪：APNs（iOS）/ FCM（Android），`handleKeyStatusPush` 接入

### 4.2 iOS 专属

| 类别 | 配置项 | 位置 | 缺失后果 |
|:-----|:-------|:-----|:---------|
| 权限 | `UIBackgroundModes = [bluetooth-central]` | Info.plist | 后台 BLE 状态恢复失效 |
| 权限 | `NFCReaderUsageDescription` | Info.plist | 读 NFC 直接崩溃/拒绝 |
| 权限 | `NSNearbyInteractionAllowOnceUsageDescription`（iOS 15+，或 iOS 14 的 `NSNearbyInteractionUsageDescription`） | Info.plist | UWB 首跑即失效 |
| Entitlement | `com.apple.developer.nfc.readersession.formats = [NDEF, TAG]` | .entitlements + Developer 后台 | NFC 会话创建失败 |
| 证书 | Apple Developer 付费账号 + 该 capability 开通 | Developer Portal | 无法签名真机包 |
| 证书 | APNs 证书（.p8/.p12）用于钥匙撤销等 Push | Developer Portal | 撤销通知收不到（KeyManager 同步兜底） |
| 后台声明 | `backgroundRestoreIdentifier` 传入 YDKBLEManager | 代码 | 无系统级状态恢复 |
| 钱包层 | Apple CarKey entitlement（若启用 Apple 钱包钥匙） | Developer Portal（Apple 侧审核） | 无法添加 Apple Wallet |

### 4.3 Android 专属

| 类别 | 配置项 | 位置 | 缺失后果 |
|:-----|:-------|:-----|:---------|
| 权限 | `BLUETOOTH_SCAN`（`neverForLocation`）+ `BLUETOOTH_CONNECT` | Manifest + 运行时申请 | 扫描/连接抛 SecurityException |
| 权限 | `ACCESS_FINE_LOCATION`（API 30-） | Manifest + 运行时申请 | API 30- 设备扫描失败 |
| 权限 | `FOREGROUND_SERVICE` + `FOREGROUND_SERVICE_CONNECTED_DEVICE` | Manifest | API 34+ 启动前台服务抛异常 |
| 权限 | `NFC` + `uses-feature android.hardware.nfc` | Manifest | 无 NFC 设备 Crash |
| 组件 | `<service com.yuledkcs.sdk.ble.YdkBleForegroundService foregroundServiceType="connectedDevice">` | Manifest | 前台服务启动失败 |
| 签名 | release keystore（正式发布签名） | Gradle signingConfig | 无法上架/升级 |
| Push | FCM（google-services.json + 依赖） | 工程 + Firebase 控制台 | 撤销/分享通知收不到 |
| 钱包层 | 各厂商钱包 SDK 资质（Samsung/小米/华为等） | 厂商申请 | 无法添加厂商钱包钥匙 |
| UWB | `android.hardware.uwb` feature 声明（required=false）+ 车端 UWB 地址协商 | Manifest + 联调 | 无 UWB 时降级 BLE |

### 4.4 联调验收（双端）

- [ ] 真机验证 BLE 扫描/连接/解锁/锁车/启动（含锁屏与后台）
- [ ] 真机验证 NFC 贴卡读取 + 三指令（SW=9000）
- [ ] 真机验证 UWB 测距数据回传（iOS U1/U2；Android 14+ 设备）
- [ ] 离线场景：断网后 BLE 解锁 + `authorizeOfflineUse` 裁决正确
- [ ] 分享链路：分享码与 Mailbox URL 两条路径均验证（含取消）
- [ ] 撤销/挂起 Push 到达后钥匙状态即时变化（KeyManager 同步兜底验证）

---

## 5. 错误处理与降级策略

| 场景 | SDK 行为 | App 建议 |
|:-----|:---------|:---------|
| Hub 不可达 | 钥匙操作抛 `YDKError.networkError` / `timeout`；KeyManager 返回本地缓存 | 展示友好提示；BLE 解锁不受影响 |
| 断网 + BLE | 离线解锁可用（SE 钥匙 + 本地授权裁决） | 解锁前先 `authorizeOfflineUse` |
| BLE 未授权/关闭 | `YDKBLEManager.state != .poweredOn`，操作抛 `YDKError.internal_` | 引导用户开蓝牙/授权 |
| 钥匙被撤销 | 云端状态变更 → Push/同步 → KeyManager 变更通知 | 移除钱包钥匙 + UI 更新 |
| NFC 无硬件/未授权 | `YDKNFCError.hardwareUnavailable` / `sessionCreationFailed` | 隐藏 NFC 入口或提示 |
| UWB 不支持 | `YDKUWBError.unsupportedPlatform` / `missingPeerDiscoveryToken` | 降级 BLE 测距或隐藏入口 |
| 远程控车失败 | `ControlCommandResponse.resultCode != 0` 或抛错 | 按 resultCode/errorMsg 提示重试 |

---

## 6. 最小示例工程

仓库内已提供最小可编译骨架（只展示 API 调用面，不复制 SDK 逻辑）：

| 工程 | 路径 | 说明 |
|:-----|:-----|:-----|
| iOS DemoApp | `examples/ios/DemoApp/` | Swift Package，path 依赖本地 SDK；`main.swift` 演示初始化/扫描/解锁/分享/离线授权 |
| Android DemoApp | `examples/android/` | Gradle 工程，`settings.gradle` 引用 SDK module；`MainActivity.kt` 演示对应调用 |

集成步骤速查：

```bash
# iOS：打开 examples/ios/DemoApp 或用 Xcode 添加本地包
cd examples/ios/DemoApp && swift build    # 或 swiftc -parse Sources/DemoApp/main.swift 做语法验证

# Android：用 Android Studio 打开 examples/android，或命令行静态审查
cd examples/android
```
