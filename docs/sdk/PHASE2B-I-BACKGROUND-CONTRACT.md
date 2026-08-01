# Sprint Contract: 2b-I 后台 BLE（iOS background + Android foreground service）

> 依据: 老板钦定规则 — 开发用 harness 流程, Contract 先定 done 标准。
> 任务来源: SDK-TASKS.md 2b-I（未开始）。
> 约束: yuleDKCS 是 SDK 库（嵌入车厂 App）, 后台能力由 SDK 提供 API + 宿主 App 配置声明。
> 裁决日期: 2026-08-01

---

## 1. Scope

### What
为 SDK 增加 BLE 后台能力: iOS 系统级后台扫描/连接（state preservation & restoration）,
Android 前台服务（Foreground Service）包裹扫描/连接 + 后台重连。

### In Scope
- iOS: `YDKBLEManager` 支持 restore identifier + 连接唤醒选项; `YDKCoreBluetoothCentral`
  实现 `willRestoreState`; 测试就位
- Android: SDK 提供 `YdkBleForegroundService`（前台服务 + 通知渠道）+ `BleManager`
  后台扫描 API + `connectGatt` autoConnect 支持; 测试/文档就位
- 集成文档: `docs/sdk/BLE-BACKGROUND-INTEGRATION.md`（Info.plist UIBackgroundModes /
  AndroidManifest service+权限 / 平台限制说明）
- 裁决文档: `docs/certification/ble-background-runtime.md`（平台行为依据, 引用官方文档）

### Out of Scope
- 真机验证（单列, 需物理车/真机）
- 推送唤醒后的 BLE 快速连接编排（Push→BLE 联动, 后续任务）
- 后台自动解锁策略/安全决策（产品层）
- iOS 蓝牙外设模式（peripheral mode, SDK 是 central）

---

## 2. Architecture Decision

### 2.1 平台事实（architect-lead 会签）

| # | 裁决 | 依据（Apple/Android 官方文档） | 结论 |
|:-:|:-----|:-----|:-----|
| AD-1 | **iOS 后台 BLE = UIBackgroundModes bluetooth-central + CBCentralManager state restoration** | Apple: "Core Bluetooth background execution" / "State Preservation and Restoration" | 宿主 App 在 Info.plist 声明 `bluetooth-central`; SDK 用 `CBCentralManagerOptionRestoreIdentifierKey` + `willRestoreState` 恢复 |
| AD-2 | **iOS 后台扫描限制: 无 AllowDuplicates、扫描结果稀疏、发现后短暂执行** | Apple 文档: 后台扫描默认去重且频率低 | SDK 保持 AllowDuplicates=false; 文档注明后台扫描发现率差异 |
| AD-3 | **iOS 连接唤醒: `CBConnectPeripheralOptionNotifyOnConnectionKey` + `NotifyOnDisconnectionKey`** | Apple: 后台连接/断开系统唤醒 App | `connectVehicle` 时 options 传入两个通知键 |
| AD-4 | **iOS `willRestoreState` 恢复扫描/连接** | Apple: 系统杀死 App 后重建 CBCentralManager 并恢复 | `YDKCoreBluetoothCentral` 增加 restore 回调; `YDKBLEManager` 恢复时重连已连接外设（retrieveConnectedPeripherals 已有回退） |
| AD-5 | **Android 后台扫描 = Foreground Service 必需** | Android 8+ (API 26): 后台扫描受限; Android 12+ (API 31): 后台无法启动扫描 | SDK 提供 `YdkBleForegroundService`（前台服务 + 常驻通知）, 宿主 App 声明 service + 权限 |
| AD-6 | **Android 权限矩阵** | Android 12+: BLUETOOTH_SCAN/CONNECT/BLUETOOTH_ADVERTISE; Android 6+: 定位 (ACCESS_FINE_LOCATION) | 文档提供 AndroidManifest 完整声明; SDK 代码做权限预检 |
| AD-7 | **Android 后台重连: `connectGatt(context, autoConnect=true, callback)`** | Android 官方: autoConnect 让系统维护连接/重连 | `BleManager.connect` 增加 autoConnect 参数（默认 false 保持现有行为） |
| AD-8 | **SDK 库边界: 后台生命周期由宿主 App 管理** | SDK 提供能力/组件, 宿主 App 负责启动 service/声明权限 | iOS: SDK 提供 `enableBackgroundSupport(restoreIdentifier:)`; Android: 宿主启动 `YdkBleForegroundService`, SDK 提供 `BleManager` 绑定接口 |

### 2.2 文件边界

| Specialist | 职责 | 文件隔离 |
|:---|:---|:---|
| W1 (iOS worker) | YDKBLEManager + YDKCoreBluetoothCentral 后台支持 + 测试 | `mobile/ios/Sources/YDKBLEManager/` + `mobile/ios/Tests/YDKBLEManagerTests/` |
| W2 (Android worker) | YdkBleForegroundService + BleManager 后台 API + 权限预检 + 测试 | `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/` + test |

文档（集成指南 + 裁决文档）由双 worker 各写自己平台部分, 主导合并。

---

## 3. Testable Behaviors

### iOS（W1）
- [ ] B1.1: `YDKBLEManager(enableBackgroundSupport:)` 生产 init 创建 central 时传
       `[CBCentralManagerOptionRestoreIdentifierKey: id]` | Owner: W1
- [ ] B1.2: fake central 能捕获 options 并断言 restore identifier 存在 | Owner: W1
- [ ] B1.3: `YDKCoreBluetoothCentral` 实现 `centralManager(_:willRestoreState:)`,
       恢复的外设经 `restoredPeripherals` 回调传出; fake 驱动断言 | Owner: W1
- [ ] B1.4: `connectVehicle` 传 `CBConnectPeripheralOptionNotifyOnConnectionKey: true` +
       `CBConnectPeripheralOptionNotifyOnDisconnectionKey: true`; fake central 断言 options | Owner: W1
- [ ] B1.5: 恢复场景: 收到 restore 回调后 `retrieveConnectedPeripherals` 重连逻辑可触发（已有代码路径, 补测试） | Owner: W1

### Android（W2）
- [ ] B2.1: `YdkBleForegroundService` 继承 Service; onCreate 建 NotificationChannel (API 26+);
       onStartCommand 启动前台 (startForeground) + 持有/创建 BleManager | Owner: W2
- [ ] B2.2: 通知含常驻文案 + 图标占位; 停止服务时 stopForeground + 释放 BleManager | Owner: W2
- [ ] B2.3: `BleManager` 新增 `startBackgroundScan(timeoutMs, vehicleIds)`（供 service 调用,
       内部复用 scanVehicles 逻辑, 不新造扫描路径） | Owner: W2
- [ ] B2.4: `BleManager.connect` 新增 `autoConnect: Boolean = false` 参数,
       传给 `connectGatt(context, autoConnect, callback)`; 现有调用不变 | Owner: W2
- [ ] B2.5: 权限预检 `BlePermissions.checkOrThrow(context)`（BLUETOOTH_SCAN/CONNECT API 31+,
       ACCESS_FINE_LOCATION API 30-）; 单测覆盖各 API level 分支 | Owner: W2
- [ ] B2.6: AndroidManifest 合并说明写进集成文档（service + foregroundService 权限 + 蓝牙权限） | Owner: W2

### 文档
- [ ] B3.1: `docs/certification/ble-background-runtime.md` — 平台行为 + 限制（引用官方文档章节） | Owner: 双 worker 分写
- [ ] B3.2: `docs/sdk/BLE-BACKGROUND-INTEGRATION.md` — 宿主 App 集成指南（iOS Info.plist / Android Manifest / 启动代码示例） | Owner: 双 worker 分写

---

## 4. Acceptance Criteria

| ID | Criterion | Pass Condition | Fail Condition | Priority | Owner |
|:---|:----------|:---------------|:---------------|:--------:|:-----:|
| AC-1 | iOS restore identifier 传递 | fake central 断言 options 含 RestoreIdentifierKey | 未传 options | P0 | W1 |
| AC-2 | iOS willRestoreState 回调链 | fake 触发 restore → 回调到 manager → 重连路径可达 | 无回调或断链 | P0 | W1 |
| AC-3 | iOS 连接唤醒选项 | connect options 含 NotifyOnConnection/Disconnection | 缺选项 | P0 | W1 |
| AC-4 | Android ForegroundService 结构 | service 类可编译; startForeground + channel + stop 释放完整 | 缺组件 | P0 | W2 |
| AC-5 | Android autoConnect 参数 | connect() 签名含 autoConnect 且传给 connectGatt; 默认 false 不破坏现有 | 未传/默认破坏 | P0 | W2 |
| AC-6 | Android 权限预检 | checkOrThrow 按 API level 分支正确; 单测覆盖 | 分支错/无测试 | P0 | W2 |
| AC-7 | 编译/语法检查 | iOS: swiftc -parse/类型检查; Android: 测试/静态验证 | 编译失败 | P0 | W1+W2 |
| AC-8 | 集成文档 | 双端配置声明 + 启动示例 + 限制说明齐全 | 缺文档 | P1 | 主导 |
| AC-9 | 不破坏现有行为 | 现有测试（BleStubTests / BleProtocolAdapterTest 等）不回归 | 回归 | P0 | W1+W2 |

---

## 5. Responsibility Matrix

| Criterion | Responsible | Fallback |
|:----------|:------------|:---------|
| AC-1..AC-3 iOS 后台 | W1 | 主导 |
| AC-4..AC-6 Android 后台 | W2 | 主导 |
| AC-7 编译检查 | W1+W2 | 主导 |
| AC-8 文档 | 主导 | W1/W2 提供素材 |
| AC-9 不回归 | W1+W2 | 主导 |

---

## 6. Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 8 项裁决基于 Apple/Android 官方后台 BLE 文档; SDK 库边界清晰（能力提供方, 生命周期归宿主） |
| R2 | architect-lead | APPROVE_ARCHITECTURE | 文件隔离; 现有抽象层（YDKCentralManaging / BleScanEngine）可直接扩展, 无侵入性重构 |
| R3 | Evaluator | APPROVE_TESTABILITY | fake central 可断言 options/restore; Android 权限分支可单测; 无真机依赖 |
| R4 | 老板 | 确认 | 继续 |
| R5 | Evaluator | APPROVE | 双端完成: iOS 16/16 断言 + typecheck 零错误 (fake central 驱动 options/restore/唤醒); Android 权限分支 41 API level 穷举 PASS + 静态审查 (无 gradle 工具链, 测试由 CI 执行) |
| R6 | 主导 | CLOSE | 文档合并: ble-background-runtime.md 补 Android 段 (W1 覆盖冲突已解决), BLE-BACKGROUND-INTEGRATION.md 补 iOS 段 |

---

## 7. 交付物清单

- `mobile/ios/Sources/YDKBLEManager/YDKBLEManager.swift`（后台支持扩展）
- `mobile/ios/Sources/YDKBLEManager/YDKCoreBluetoothCentral.swift` 或同文件内扩展（willRestoreState）
- `mobile/ios/Tests/YDKBLEManagerTests/`（新增后台测试）
- `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/YdkBleForegroundService.kt`（新增）
- `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/BlePermissions.kt`（新增）
- `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/BleManager.kt`（后台 API + autoConnect）
- `mobile/android/sdk/src/test/kotlin/com/yuledkcs/sdk/ble/`（权限/后台测试）
- `docs/certification/ble-background-runtime.md`（新增）
- `docs/sdk/BLE-BACKGROUND-INTEGRATION.md`（新增）
- `docs/sdk/SDK-TASKS.md`（2b-I 状态更新）
