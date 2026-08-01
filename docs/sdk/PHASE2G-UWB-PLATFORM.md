# PHASE2G: UWB 测距真实化 — 平台差异 / Token 交换 / 真机联调

> 对应契约: `docs/sdk/PHASE2B-GH-P1-CONTRACT.md` 工作流 1 (2b-G UWB)。
> 交付代码:
> - iOS: `mobile/ios/Sources/YDKBLEManager/YDKUWBManager.swift` → `YDKNIUWBManager` (NearbyInteraction)
> - Android: `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/ble/UwbManager.kt` → `AndroidUwbManager` (android.uwb, API 34+)
> - 模拟测: `mobile/ios/Tests/YDKBLEManagerTests/YDKUWBManagerTests.swift` + `mobile/android/sdk/src/test/kotlin/com/yuledkcs/sdk/ble/UwbManagerTest.kt`

---

## 1. 平台差异总览

| 维度 | iOS | Android |
|:-----|:----|:--------|
| 框架 | NearbyInteraction (`NISession`) | `android.uwb` 原生 API (FiRa 兼容) |
| 最低版本 | iOS 14.0+ (需 U1/U2 chip) | Android 14 / API 34+ |
| 系统服务 | 无 (框架直连) | `Context.getSystemService(UwbManager::class.java)` |
| 对端标识 | `NIDiscoveryToken` (经 BLE 交换) | `UwbAddress` + `sessionId` (经 BLE 协商) |
| 测距参数 | `NINearbyPeerConfiguration(peerDiscoveryToken:)` | `RangingParameters` (channel/preamble/sessionId/slotDuration/UwbConfigType) |
| 结果单位 | distance: 米 (Float); azimuth/elevation: 弧度 (已转度) | distanceMm → 米; azimuth/elevation: 度 (原生) |
| 前后台 | 前台有效; 退后台挂起 (iOS 15+) 或失效 (iOS 14) | 前台有效; 退后台会话被系统暂停/关闭 |
| 降级路径 | `YDKMockUWBManager` (编译期由调用方选择) | `MockUwbManager` (API < 34 捕获 `IllegalStateException` 后切换) |

**接口 (双端对齐, FiRa 抽象)**: `startRanging(vehicleId)` / `stopRanging()` / `rangingResultHandler(UWBMeasurement)`。
`UWBMeasurement { vehicleId, distance(m), azimuth(°), elevation(°), timestamp(ms) }` — 双端语义一致。

---

## 2. Token 交换流程 (关键前置)

UWB 测距必须"双端互相知道对方身份"才能建立会话:

### iOS 侧 (`NIDiscoveryToken`)
1. **本端 token 获取**: `session.discoveryToken` (NISession 属性, 本机唯一标识)。
2. **车端 token 注入**: 车端 TCU 的 token 经 **BLE 通道** (既有 CCC 指令链路) 传输到手机后,
   调用 `YDKNIUWBManager.injectPeerDiscoveryToken(data:)` 注入 (iOS 16+ 支持 `NIDiscoveryToken(data:)` 解析)。
3. **车端获得本端 token**: 手机把 `session.discoveryToken.data` 经 BLE 上行写入车端 (车端据此 `run` 会话)。
4. **建立会话**: `startRanging(vehicleId:)` → `NINearbyPeerConfiguration(peerDiscoveryToken: 车端token)` → `session.run(config)`。

> ⚠️ iOS 14/15: `NIDiscoveryToken` 无公开 Data 构造器,
> 需走 `session(_:didGenerateShareableConfigurationData:for:)` 路径双端直接交换 token 实例
> (或要求车端固件支持 iOS 16+ 设备)。代码中 `injectPeerDiscoveryToken(data:)` 在 <iOS 16 抛
> `YDKUWBError.peerTokenBelowIOS16`, 由上层决定是否降级 Mock。

### Android 侧 (`UwbAddress` + `sessionId`)
1. **本端地址**: `RangingParameters.deviceAddress = null` 时由系统分配; 也可显式指定 `UwbAddress(bytes)`。
2. **车端地址**: 车端 UWB 地址 (EUI-64) 经 BLE 协商后注入 `destinationAddress` (当前代码置 null, 联调时替换)。
3. **会话参数**: `sessionId = 0x444B4353` ("DKCS" 固定值) + `UwbComplexChannel(9, 11)` + `UwbConfigType.UWB_CONFIG_1`
   必须与车端固件约定一致, 否则 `onOpenFailed`/`onStartFailed`。
4. **建立会话**: `adapter.openRangingSession(parameters, executor, callback)` → `onOpened` 回调中 `session.start()`。

> 车端 TCU 固件需同时支持 FiRa UWB 会话 (channel 9 / preamble 11 / sessionId "DKCS"),
> 本端与车端参数不一致是联调最常见失败点。

---

## 3. App 前后台限制 (真机联调必读)

### iOS
| 场景 | 行为 | 处理 |
|:-----|:-----|:-----|
| 前台 | 正常测距 | — |
| 退后台 (iOS 15+) | `sessionWasSuspended` (测距暂停) | `sessionSuspensionHandler(true)` 通知上层暂停 UI; 回前台 `sessionSuspensionEnded` 自动恢复 |
| 退后台 (iOS 14) | 会话直接失效 `didInvalidateWithError` | `sessionInvalidatedHandler` 通知上层, 回前台重新 `startRanging` |
| 用户拒绝授权 | `NISession.Error.userDidNotAllow` | 提示用户开启权限 (Info.plist key 见 §4) |
| 同时多会话 | `NISession.Error.activeSessionLimitReached` | 一次只允许一个 NISession, 先 `stopRanging` 再开新会话 |

> 关键: `NISession.delegate` 为 weak — 调用方必须**强持有** `YDKNIUWBManager` 实例,
> 否则 delegate 被释放、回调静默丢失。

### Android
| 场景 | 行为 | 处理 |
|:-----|:-----|:-----|
| 前台 | 正常测距 | — |
| 退后台 | 系统暂停/关闭 UWB 会话 | 回前台重新 `startRanging` |
| 无定位权限 | `onOpenFailed` (权限前置) | 先申请 `ACCESS_FINE_LOCATION` + `UWB_RANGING` |
| 无 UWB 硬件 | `getSystemService` 返回 null | 抛 `IllegalStateException`, 上层降级 Mock |

---

## 4. 权限 / 配置清单

### iOS (Info.plist)
```xml
<!-- iOS 15+: 一次性授权描述 (推荐) -->
<key>NSNearbyInteractionAllowOnceUsageDescription</key>
<string>需要 UWB 测距以完成靠近解锁</string>
<!-- iOS 14: 长期授权描述 (二选一) -->
<key>NSNearbyInteractionUsageDescription</key>
<string>需要 UWB 测距以完成靠近解锁</string>
```

### Android (AndroidManifest.xml)
```xml
<!-- 硬件特性 (可选: 用于 Play 过滤) -->
<uses-feature android:name="android.hardware.uwb" android:required="false" />
<!-- 运行时权限: UWB 测距 (API 33+) -->
<uses-permission android:name="android.permission.UWB_RANGING" />
<!-- 前置权限: 定位 (UWB 测距强制要求) -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

---

## 5. 真机联调清单

### 前置条件
- [ ] iOS 真机: iPhone 11+ (U1) 或 iPhone 15/16 (U2); 系统 iOS 14+ (建议 16+ 走 Data token 路径)
- [ ] Android 真机: Android 14+ 且支持 UWB (Pixel 6 Pro / Galaxy S21+ 等 FiRa 认证机型)
- [ ] 车端 TCU 固件支持 FiRa UWB 会话 (channel 9 / preamble 11 / sessionId "DKCS")
- [ ] BLE 链路可承载 token/地址交换 (既有 CCC 指令链路)

### iOS 联调步骤
1. [ ] Info.plist 已加 §4 描述 key, 首次弹窗允许授权
2. [ ] 上层强持有 `YDKNIUWBManager`, 注册 `rangingResultHandler` / `sessionInvalidatedHandler` / `sessionSuspensionHandler`
3. [ ] 与车端交换 `NIDiscoveryToken` (BLE 下行车端 token → `injectPeerDiscoveryToken(data:)`; 上行本端 `session.discoveryToken.data`)
4. [ ] `startRanging(vehicleId:)` 后观察 delegate `didUpdate` → `rangingResultHandler` 收到距离/角度
5. [ ] 验证退后台/回前台: iOS 15+ 挂起恢复; iOS 14 失效后重新 start
6. [ ] 验证停止: `stopRanging()` 后无回调、可再次 start (无 `activeSessionLimitReached`)

### Android 联调步骤
1. [ ] manifest 已加 §4 权限; 运行时申请 `ACCESS_FINE_LOCATION` + `UWB_RANGING`
2. [ ] API < 34 设备: 确认捕获 `IllegalStateException` 并降级 `MockUwbManager`
3. [ ] 与车端协商 `UwbAddress` (EUI-64) 后注入 `destinationAddress` (替换代码中的 null)
4. [ ] 确认 `sessionId` / channel / preamble 与车端一致, 否则 `onOpenFailed`/`onStartFailed`
5. [ ] `startRanging(vehicleId:)` 后观察 `onReportReceived` → `rangingResultHandler` 收到距离 (mm→m)
6. [ ] 验证退后台/回前台: 回前台重新 start; 验证 `stopRanging()` 后 `onStopped`/`onClosed` 正常

### 常见失败排查
| 现象 | 根因 | 处理 |
|:-----|:-----|:-----|
| `didInvalidateWithError` (iOS) | 未授权 / token 不匹配 / 会话超限 | 检查权限 key、token 交换、是否重复 start |
| `onOpenFailed`/`onStartFailed` (Android) | sessionId/channel/preamble 与车端不一致 / 无权限 | 核对 RangingParameters 与车端约定 |
| 收不到回调 (双端) | 车端未在测距 / 车端 token 未注入 / 参数不匹配 | 按 §2 逐项核对交换流程 |
| 后台断连 | 平台前后台限制 | 回前台重新 start (见 §3) |

---

## 6. 模拟测覆盖 (无硬件)

| 测试 | 覆盖契约 | 位置 |
|:-----|:---------|:-----|
| Mock start → 回调 (vehicleId/距离校验) | U-4 | iOS `testMockStartRangingEmitsMeasurement` / Android `mockStartRangingEmitsMeasurement` |
| Mock stop → 无回调 | U-4 | iOS `testMockStopRangingHaltsCallbacks` / Android `mockStopRangingHaltsCallbacks` |
| handler 注入/置空 | U-3 | iOS `testRangingResultHandlerInjection` / Android `mockRangingResultHandlerInjection` |
| 幂等 start/stop | U-4 | iOS `testMockRestartIsIdempotent` |
| 平台能力查询不崩溃 | U-1 辅助 | iOS `testPlatformCapabilityQueryNeverThrows` |
| NI 无 token → 抛错 | U-1 | iOS `testNIUWBManagerFailsWithoutPeerToken` (仅 iOS 宿主编译) |
| NI 空 token 数据 → 抛错 | U-1 | iOS `testNIUWBManagerRejectsInvalidTokenData` (仅 iOS 宿主编译) |
| 版本降级分支 (API < 34 抛错) | U-2 | Android `versionPolicyRequireSupportedBelow34Throws` / `versionPolicySupportsApi34Plus` |
| 数据契约默认值 | U-3 | Android `uwbMeasurementDefaults` |

> 真机测距链路 (真实距离/角度) 不纳入模拟测, 按 §5 联调清单执行。
