# BLE 后台运行 — 平台行为裁决知识库（2b-I）

> 用途: 移动端后台 BLE 实现的唯一依据（双平台行为差异裁决）。
> 双 worker 分写: 本文件 iOS 部分由 W1 完成; Android 部分由 W2 追加。
> 契约: `docs/sdk/PHASE2B-I-BACKGROUND-CONTRACT.md`（AD-1..AD-8）。
> 集成指南: `docs/sdk/BLE-BACKGROUND-INTEGRATION.md`（宿主 App 配置声明, 由主导合并）。

---

# iOS 部分（W1）

## 1. 平台事实（Apple 官方文档依据）

| # | 裁决 | Apple 文档出处 | 结论 |
|:-:|:-----|:-----|:-----|
| AD-1 | **后台 BLE = UIBackgroundModes bluetooth-central + state restoration** | [Core Bluetooth background execution](https://developer.apple.com/documentation/corebluetooth/core-bluetooth-background-execution-for-ios-apps)（"Implementing State Preservation and Restoration" 章节）; `CBCentralManagerOptionRestoreIdentifierKey` | 宿主 App 在 Info.plist 声明 `UIBackgroundModes = [bluetooth-central]`; SDK 用 restore identifier 创建 `CBCentralManager`, 实现 `centralManager(_:willRestoreState:)` 恢复被杀前的扫描/连接 |
| AD-2 | **后台扫描限制: 无 AllowDuplicates、扫描结果稀疏、发现后短暂执行** | 同上一篇（"Background Scanning" 章节: 后台扫描默认去重、低频率; 仅在 `UIBackgroundModes` 含 bluetooth-central 且扫描在后台持续时偶尔推送结果） | SDK 保持 `AllowDuplicates=false` 不变; 文档注明后台扫描发现率差异, 不依赖后台扫描做关键路径 |
| AD-3 | **连接唤醒: `CBConnectPeripheralOptionNotifyOnConnectionKey` + `NotifyOnDisconnectionKey`** | `CBConnectPeripheralOptionNotifyOnConnectionKey` / `CBConnectPeripheralOptionNotifyOnDisconnectionKey`（Core Bluetooth Constants） | `connect(_:options:)` 传入两个键, 后台连接/断开时系统唤醒宿主 App |
| AD-4 | **`willRestoreState` 恢复扫描/连接** | `centralManager(_:willRestoreState:)`; `CBCentralManagerRestoredStatePeripheralsKey`（dict 值类型 `[CBPeripheral]`） | `YDKCoreBluetoothCentral` 实现 delegate 方法, 恢复外设经 `onRestoreState` 交回上层; `YDKBLEManager` 记录外设并复位连接状态, 重连走 `connectVehicle` 现有回退路径 |
| AD-8 | **SDK 库边界: 后台生命周期由宿主 App 管理** | —（SDK 设计裁决） | SDK 只提供能力/API; 宿主 App 负责 Info.plist 声明 + 在合适的时机用 restore identifier 创建 manager |

> 注意（真实行为, 与直觉相反）: `willRestoreState` 在 `centralManagerDidUpdateState(_:)` **之前**触发,
> 且触发时蓝牙可能尚未 poweredOn; 恢复的外设**不代表当前已连接**, 只是系统重建后交回的已知外设。
> 因此 SDK 恢复处理只做"记录 + 复位连接状态", 实际连接由 `connectVehicle` 显式发起
> （`retrieveConnectedPeripherals(withServices:)` 回退按 identifier/name 匹配, 2b-I AD-4 复用）。

## 2. 系统行为时序（App 被杀后重建）

```
宿主 App 被系统终止（后台）
   │
   ▼ 宿主 App 重新启动（用户/推送/蓝牙事件唤醒）
   ├─ SDK: YDKBLEManager(backgroundRestoreIdentifier: "xxx") 创建
   │     └─ CBCentralManager(options: [RestoreIdentifierKey: "xxx"])  ← AD-1
   ├─ 系统: centralManager(_:willRestoreState:)  ← AD-4
   │     └─ dict[CBCentralManagerRestoredStatePeripheralsKey] = [CBPeripheral]（被杀前已知外设）
   │           └─ SDK: onRestoreState → handleRestoredState: 按 name/identifier 存入
   │              peripheralByVehicleId, 连接状态复位 .disconnected
   ├─ 系统: centralManagerDidUpdateState(.poweredOn)
   │     └─ SDK: state = .poweredOn, 状态机照常
   └─ 宿主 App: connectVehicle(vehicleId)  ← 重连路径
         ├─ ① peripheralByVehicleId 命中（恢复时已按 name/identifier 占位）
         └─ ② 回退: retrieveConnectedPeripherals(withServices:) 按 identifier/name 匹配
              └─ connect(options: [NotifyOnConnection: true, NotifyOnDisconnection: true])  ← AD-3
```

## 3. 关键 API 事实核对

| API | 事实 | 出处 |
|:----|:-----|:-----|
| `CBCentralManagerOptionRestoreIdentifierKey` | String 键; 值为唯一标识（如 bundle id + 后缀）。**必须与宿主 App 的 UIBackgroundModes 声明配合**, 否则系统不会在后台保持 BLE 会话 | `CBCentralManager` 初始化选项 |
| `CBCentralManagerRestoredStatePeripheralsKey` | `willRestoreState` dict 键; 值为 `[CBPeripheral]`（被杀前已连接或正在连接的外设） | `centralManager(_:willRestoreState:)` 文档 |
| `CBCentralManagerRestoredStateScanOptionsKey` | dict 键; 值为被杀前扫描的 options（`CBCentralManagerScanOptionAllowDuplicatesKey` 等） | 同上（SDK 当前不依赖, 恢复后由宿主重新发起扫描） |
| `CBConnectPeripheralOptionNotifyOnConnectionKey` | Bool; true 时**后台**连接完成可唤醒 App | `connect(_:options:)` 文档 |
| `CBConnectPeripheralOptionNotifyOnDisconnectionKey` | Bool; true 时**后台**断开可唤醒 App | 同上 |
| 后台扫描 | `AllowDuplicates` 在后台被系统忽略; 扫描结果低频推送; 发现后仅短暂执行窗口 | "Background Scanning" 章节 |

## 4. SDK 实现落点（W1, 2026-08-01）

| 文件 | 修改 |
|:-----|:-----|
| `mobile/ios/Sources/YDKBLEManager/YDKBLEManager.swift` | `YDKCentralManaging` 新增 `onRestoreState`; `YDKCoreBluetoothCentral` 实现 `centralManager(_:willRestoreState:)`（取 `RestoredStatePeripheralsKey` → wrap → `onRestoreState`）; 生产 init 支持 `backgroundRestoreIdentifier`（非 nil 时传 `[RestoreIdentifierKey: id]`）; `wireCallbacks` 增加 restore 处理（`handleRestoredState`: 按 name/identifier 存 `peripheralByVehicleId` + 复位连接状态）; `connect(_:)` 传连接唤醒 options |
| `mobile/ios/Tests/YDKBLEManagerTests/BackgroundTests.swift` | 新增: options 传递断言（工厂捕获）、connect 唤醒选项断言、restore 回调链 + 重连断言（fake 驱动） |
| `mobile/ios/Tests/YDKBLEManagerTests/BleStubTests.swift` | `FakeCentral` 同步协议变更: `onRestoreState`、`connectOptions` 记录、`connectedSystemPeripherals`（retrieveConnectedPeripherals 回退）、`simulateRestoreState` |

## 5. 限制与边界（W1 声明）

1. **真机验证单列**: 状态恢复只有在 App 被系统终止后才发生, 无法在单元测试/模拟器验证, 需真机 + 物理车。
2. **恢复 ≠ 已连接**: 恢复的外设需要重新 `connectVehicle`; 系统可能已自动重连（此时 `didConnect` 会触发, SDK 正常流程接管）。
3. **后台扫描不是关键路径**: 后台扫描发现率低且无重复推送, SDK 保持 `AllowDuplicates=false`; 后台解锁编排（Push→BLE 联动）在 2b-I 范围外。
4. **info.plist 是宿主责任**: SDK 无法替宿主声明 `UIBackgroundModes`; 未声明时 restore identifier 不产生后台保持效果。

---

<!-- ANDROID-SECTION -->

## Android 部分（W2）

### 1. 平台事实（Android 官方文档依据）

| # | 平台事实 | 依据 | SDK 落点 |
|:-:|:-----|:-----|:-----|
| A-1 | Android 8+ (API 26) 后台 BLE 扫描受限（约 30 秒窗口） | Android 官方 "BLE scanning background limits" | 后台扫描必须前台服务包裹 |
| A-2 | Android 12+ (API 31) 后台无法启动扫描 | Android 官方 "Background BLE scanning" | `YdkBleForegroundService` 提供前台上下文 |
| A-3 | Foreground Service 需通知渠道 (API 26+) + `startForeground` | Android 官方 "Foreground services" | `YdkBleForegroundService.onCreate` 建 `NotificationChannel` (IMPORTANCE_LOW) |
| A-4 | 前台服务类型: `foregroundServiceType="connectedDevice"` (API 29+); API 34+ 需 `FOREGROUND_SERVICE_CONNECTED_DEVICE` 权限 | Android 官方 "Foreground service types" | 集成文档 manifest 示例 |
| A-5 | 权限矩阵: API 31+ `BLUETOOTH_SCAN`+`BLUETOOTH_CONNECT`; API 30- `ACCESS_FINE_LOCATION` | Android 官方 "Bluetooth permissions" | `BlePermissions.permissionsForApiLevel` |
| A-6 | `connectGatt(context, autoConnect, callback)`: autoConnect=true 时系统维护后台重连 | Android 官方 BluetoothDevice.connectGatt | `BleManager.connect(address, autoConnect)` |

### 2. 系统行为时序（前台服务启动后台扫描）

```
宿主 App (前台)                     YdkBleForegroundService         系统
      │  startForegroundService()          │                          │
      ├────────────────────────────────────►│  onCreate: channel + BleManager
      │  onStartCommand(ACTION_START)       │                          │
      ├────────────────────────────────────►│  startForeground(notify) │
      │                                     │  startBackgroundScan()   │
      │                                     ├─────── BLE 扫描 ────────►│
      │                                     │◄────── 结果 ─────────────│
      │◄── onScanResults(静态回调) ─────────│                          │
      │                                     │                          │
      │  stop(context)                      │                          │
      ├────────────────────────────────────►│  stopForeground + stopSelf│
```

### 3. 关键 API 事实核对

- `context.startForegroundService(intent)`（minSdk 26 安全, SDK 无 androidx 依赖, 不用 ContextCompat）
- `Notification.Builder(context, CHANNEL_ID)`（原生, 不用 NotificationCompat）
- `stopForeground(STOP_FOREGROUND_REMOVE)`（API 24+）
- `START_STICKY`（系统杀死后重建, intent=null 走恢复分支避免 ANR）
- 扫描结果经 `@Volatile onScanResults` 静态回调（Service 实例由系统创建, 宿主无法注入实例字段）

### 4. SDK 实现落点（W2, 2026-08-01）

| 文件 | 修改 |
|:-----|:-----|
| `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/BlePermissions.kt` | 新增: `permissionsForApiLevel` (internal 纯逻辑) + `requiredPermissions()` + `checkOrThrow(context)` |
| `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/YdkBleForegroundService.kt` | 新增: 前台服务 + 通知渠道 + 后台扫描协程 + 静态结果回调 |
| `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/BleManager.kt` | `connect(address, autoConnect=false)`; `startBackgroundScan(timeoutMs, vehicleIds)` (权限预检 + 复用 scanVehicles 路径) |
| `mobile/android/sdk/src/test/kotlin/com/yuledkcs/sdk/ble/BlePermissionsTest.kt` | 新增: API 31+/26-30/25- 三分支断言 |

### 5. 限制与边界（W2 声明）

1. **真机验证单列**: 前台服务行为需真机验证（通知显示、后台扫描、系统杀死重建）。
2. **生命周期归宿主**: SDK 提供 service 组件, 宿主负责 manifest 声明 + 启动/停止时机。
3. **SDK 无 androidx**: 用原生 API 等价实现, 注释已说明。
4. **checkOrThrow 异常路径**: 依赖 Context, 单测用纯逻辑 `permissionsForApiLevel` 覆盖, 完整路径需 Robolectric/仪器测试。
5. **通知图标**: 用系统图标占位, 生产需替换为宿主品牌图标。
