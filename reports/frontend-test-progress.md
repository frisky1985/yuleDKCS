# 前端测试诊断报告

> 生成日期: 2026-07-18
> 项目: yuleDKCS frontend

## 1. 目录结构与源文件统计

### Android SDK (`android/`)
| 指标 | 数值 |
|------|------|
| 总源文件 (.kt) | 12 |
| 单元测试 | **0** |
| UI 测试 | **0** |
| Kotlin 版本 | 1.9.22 |
| 编译 SDK | 34 |
| 最低 SDK | 26 |

**源文件列表:**
- `DigitalKeySdk.kt` — SDK 入口
- `DeviceCapabilities.kt` — 设备能力检测
- `error/DkError.kt` — 统一错误码
- `bertlv/BertlvDecoder.kt` — BER-TLV 解码器
- `bertlv/BertlvEncoder.kt` — BER-TLV 编码器
- `ble/BleManager.kt` — BLE 通信
- `key/KeyManager.kt` — 密钥管理
- `nfc/NfcManager.kt` — NFC 通信
- `nfc/NfcSecureChannel.kt` — NFC 安全通道
- `telemetry/DkTelemetry.kt` — 遥测
- `logger/DkLogger.kt` — 统一日志
- `uwb/UwbManager.kt` — UWB 通信

### Android App (`android-app/`)
| 指标 | 数值 |
|------|------|
| 总源文件 (.kt) | 31 |
| 单元测试 | **6** ✅ |
| UI 测试 | **4** ✅ |
| 测试框架 | JUnit4 + MockK + MockWebServer |

**现有测试覆盖:**
- `data/remote/ApiServiceTest.kt` — 22 个测试用例 (MockWebServer)
- `data/repository/KeyRepositoryTest.kt`
- `data/repository/VehicleRepositoryTest.kt`
- `data/model/ModelsTest.kt`
- `home/VehicleControlViewModelTest.kt`
- `key/KeyListViewModelTest.kt`
- `ui/KeyListScreenTest.kt` (AndroidTest)
- `ui/RemoteControlScreenTest.kt` (AndroidTest)
- `ui/KeyBindScreenTest.kt` (AndroidTest)
- `MainActivityUITest.kt` (AndroidTest)

### iOS SDK (`ios/`)
| 指标 | 数值 |
|------|------|
| 总源文件 (.swift) | 9 |
| 单元测试 | **0** |
| Swift 版本 | 5.9 |
| iOS 最低版本 | 14.0 |

**源文件列表:**
- `DigitalKeySDK.swift` — SDK 入口（单例 + SdkConfig）
- `Error/DkError.swift` — 统一错误码
- `Logger/DkLogger.swift` — 统一日志
- `BLE/BleManager.swift` — BLE 通信
- `KeychainManager.swift` — Keychain 安全存储
- `Bertlv/BertlvDecoder.swift` — BER-TLV 解码器
- `Bertlv/BertlvEncoder.swift` — BER-TLV 编码器
- `DeviceCapabilities.swift` — 设备能力检测
- `Telemetry/DkTelemetry.swift` — 遥测

### iOS App (`ios-app/`)
| 指标 | 数值 |
|------|------|
| 总源文件 (.swift) | 23 |
| 单元测试 (在 ios-tests/ 内) | **5** ✅ |
| UI 测试 | **1** ✅ |

**注意:** iOS App 的测试位于独立的 `ios-tests/` Xcode 项目目录，而非 `ios-app/` 下。

## 2. 测试缺口分析

### 高风险缺口

| 模块 | 源文件 | 测试数 | 风险等级 | 理由 |
|------|--------|--------|----------|------|
| **Android SDK** | 12 | **0** | 🔴 高 | 核心协议(bertlv)、通信(BLE/NFC/UWB)、错误码系统无任何测试 |
| **iOS SDK** | 9 | **0** | 🔴 高 | 同上入口类 + Keychain 安全存储无测试 |

### 已覆盖模块

| 模块 | 覆盖情况 |
|------|----------|
| Android App Repository 层 | 🟢 有测试 |
| Android App API 层 | 🟢 有测试 (MockWebServer) |
| Android App ViewModel 层 | 🟢 有测试 |
| Android App UI 层 | 🟢 有 UI 测试 |
| iOS App APIClient | 🟢 有测试 (MockURLProtocol) |
| iOS App Service 层 | 🟢 有测试 |

## 3. 测试优先级排序

```
P0 (立即)  → Android SDK: Error/DkError, Bertlv Decoder/Encoder
P0 (立即)  → iOS SDK: DigitalKeySDK 入口, DkError, KeychainManager
P1 (次日)  → Android SDK: DeviceCapabilities, DkLogger
P1 (次日)  → iOS SDK: Bertlv Decoder/Encoder, DkLogger
P2 (本周)  → Android SDK: BLE/NFC/UWB 通信层 (需 mock)
P2 (本周)  → iOS SDK: BLE, DeviceCapabilities (需 mock)
```

## 4. 开发环境检查

| 工具 | 状态 |
|------|------|
| Android SDK | ⚠️ 部分存在 (仅 cmdline-tools) |
| Java | ❌ 未安装 |
| Gradle | ❌ 未安装 |
| Xcode | ❌ 未安装 |
| iOS SDK | ❌ 未安装 |
| Kotlin (独立) | ❌ 未安装 |

**结论: 本地无完整编译环境。采用 CI-only 方案，通过 GitHub Actions 远程执行测试。**

## 5. ⚠️ 已知阻塞项: iOS SDK DigitalKeyError 命名冲突

**DigitalKeySDK.swift** 与 **DkError.swift** 在同模块下定义了同名 `DigitalKeyError` 类型：
- `DigitalKeySDK.swift:15` — `public enum DigitalKeyError: Error, LocalizedError`
- `DkError.swift:185` — `public struct DigitalKeyError: Error, CustomStringConvertible`

此冲突会导致编译失败，是 iOS 测试的 **#1 阻塞项**。建议:
1. 删除 `DigitalKeySDK.swift` 中的旧 enum，统一使用 `DkError.swift` 的 struct 版本
2. 或者将 enum 标记为 `@available(*, deprecated)` 并添加重命名 alias

## 6. 已产出文件清单

| 文件 | 类型 | 行数 | 说明 |
|------|------|------|------|
| `specs/spec-frontend-test.md` | Spec | 183 | OpenSpec 需求 (SWR-001~003) |
| `reports/frontend-test-progress.md` | 报告 | 本文件 | 诊断+行动计划 |
| `.github/workflows/frontend-test.yml` | CI | 151 | GitHub Actions 双 job |
| `android/.../error/DkErrorTest.kt` | 测试 | 228 | 12 用例 |
| `android/.../bertlv/BertlvDecoderTest.kt` | 测试 | 269 | 15 用例 |
| `android/.../bertlv/BertlvEncoderTest.kt` | 测试 | 207 | 10+ 用例 |
| `ios/.../DkErrorTests.swift` | 测试 | 185 | 10+ 用例 |
| `ios/.../KeychainManagerTests.swift` | 测试 | 185 | 12+ 用例 |
| `ios/.../DigitalKeySDKTests.swift` | 测试 | 156 | 8 用例 |
| `ios/Package.swift` | 配置 | 20 | SPM 配置（xcodebuild 需另配） |

## 7. 建议行动计划

1. **立即**: 合入 Spec，明确测试需求
2. **Day 1**: 修复 iOS SDK DigitalKeyError 命名冲突
3. **Day 1**: 提交 Android SDK 纯逻辑层测试 (DkError, Bertlv) — CI 可跑
4. **Day 1**: 提交 iOS SDK 纯逻辑层测试 (DkError, KeychainManager, SDK入口) — CI 可跑
5. **Day 2**: 提交 Android SDK Bertlv roundtrip + 编码测试
6. **Day 3**: 搭建 CI，验证全流程通过
7. **Week 2**: 补通信层 mock 测试
