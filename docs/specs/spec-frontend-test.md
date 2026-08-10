# Spec: 前端 SDK 单元测试

## 概述

本 Spec 定义 yuleDKCS 前端 SDK 单元测试需求，覆盖 Android 和 iOS 平台的 SDK 核心模块。

## 范围

- Android SDK (`android/`) — 纯逻辑层（无 Android 依赖）
- iOS SDK (`ios/Sources/DigitalKeySDK/`) — 纯逻辑层（无 UI 依赖）
- CI 集成 — GitHub Actions

## 架构约束

- 测试不得依赖真机或模拟器硬件（BLE/NFC/UWB）
- Android 独立单元测试使用 Robolectric + MockK
- iOS 测试使用 XCTest + URLProtocol mock
- CI 跑在 ubuntu-latest（Android）和 macos-latest（iOS）runner 上

---

## SWR-001: Android SDK API 单元测试

### SHALL

1. Android SDK module SHALL include unit tests for `error/DkError.kt` covering:
   - 所有错误码常量的值正确性
   - `ErrorCategory.fromCode()` 的编解码正确性（覆盖所有 11 个分类）
   - `DigitalKeyError` 的工厂方法、属性计算（category, hexCode, isSuccess, toMap）
   - 带有 details, traceId, cause 的链式构造
   - 边界情况：未知错误码 → SYSTEM 分类

2. Android SDK module SHALL include unit tests for `bertlv/BertlvDecoder.kt` covering:
   - 短格式标签解码（单字节 < 0x1F）
   - 长格式标签解码（多字节，最多 4 字节）
   - 短格式长度解码（< 0x80）
   - 长格式长度解码（0x81, 0x82, 0x83, 0x84）
   - 构造节点递归解码（constructed tag 含子节点）
   - 异常路径：数据不足、无效标签、无效长度
   - 综合用例：编解码 roundtrip 一致性

3. Android SDK module SHALL include unit tests for `bertlv/BertlvEncoder.kt` covering:
   - 编码单个 primitive 节点
   - 编码 constructed 节点（含子节点）
   - `encodeInt()` / `encodeLong()` / `encodeString()` / `encodeBoolean()` 辅助方法
   - `BertlvBuilder` 工厂方法：authenticate, vehicleCommand, statusResponse, errorResponse

4. Android SDK module SHALL include unit tests for `logger/DkLogger.kt` covering:
   - 日志级别过滤（minLevel）
   - JSON 格式输出
   - 文本格式输出
   - 订阅者通知
   - 便捷方法（keyMgr, auth, ble 等）

5. Android SDK module SHALL include unit tests for `DeviceCapabilities` covering:
   - `detect()` 方法返回结构正确
   - 各能力检测方法在 mock Context 下正确响应

### Reason

错误码系统是整个 SDK 的一致性和可调试性的基础，必须通过测试保证三端（Cloud/SDK/Vehicle）统一。
BER-TLV 是数字钥匙协议的核心编码格式，编解码一致性直接影响车辆通信安全。
日志系统影响运维排障，需要验证过滤和格式正确。

---

## SWR-002: iOS SDK API 单元测试

### SHALL

1. iOS SDK module SHALL include unit tests for `DigitalKeySDK.swift` covering:
   - `configure()` 参数验证：空串拒绝、非法 URL
   - 单例模式：`shared` 在配置前应 fatalError
   - `reset()` 后 isConfigured 应为 false
   - `retrieveApiKey()` 从 Keychain 读取的正确路径

2. iOS SDK module SHALL include unit tests for `Error/DkError.swift` covering:
   - 所有错误码常量的值正确性（与 Android 端一致）
   - `DigitalKeyError` 的工厂方法、属性（category, hexCode, name, isSuccess, toMap）
   - `ErrorCategory.from()` 编解码
   - `withTraceId()` / `withDetails()` 链式构造
   - `description` 格式化输出

3. iOS SDK module SHALL include unit tests for `KeychainManager.swift` covering:
   - store → retrieve roundtrip（String）
   - store → retrieve roundtrip（Data）
   - 已存在条目更新（update）
   - 条目删除（delete）
   - 条目存在检查（contains）
   - 条目不存在时 retrieve 抛出 itemNotFound
   - clear() 清空服务下所有条目
   - SDK 便捷扩展：storeApiKey, retrieveApiKey, deleteApiKey, hasApiKey

4. iOS SDK module SHALL include unit tests for `BertlvDecoder.swift` covering:
   - 短格式/长格式标签解码
   - 短格式/长格式长度解码
   - 构造类子节点递归解码
   - 便捷方法：decodeFirstNode, decodeValue, decodeInteger, decodeString
   - 异常路径：数据不足、无效标签、无效长度
   - `validate()` 方法正确性
   - `bytesToInteger()` 大端转换（含负数处理）

5. iOS SDK module SHALL include unit tests for `BertlvEncoder.swift` covering:
   - 编码单个节点 / constructed 节点
   - encodeInteger / encodeString / encodeBoolean
   - `BertlvBuilder` 工厂方法

### Reason

iOS SDK 入口方法的安全校验直接影响 API Key 存储安全，必须通过测试保证 Keychain 集成正确。
错误码系统与 Android SDK 完全对应，跨平台一致性需要自动化验证。
BER-TLV 编解码是跨平台共享协议，两端测试既是质量保证也是协议合规验证。

---

## SWR-003: 测试编译 CI 集成

### SHALL

1. Repository SHALL have a GitHub Actions workflow at `.github/workflows/frontend-test.yml` that:
   - Triggers on `push` and `pull_request` to main branch
   - Has two jobs: `android-sdk-test` and `ios-sdk-test`
   - `android-sdk-test` SHALL run on `ubuntu-latest` with setup-java + Gradle cache
   - `ios-sdk-test` SHALL run on `macos-latest` with Xcode 15
   - Both jobs SHALL report test results as annotations (JUnit XML)
   - Both jobs SHALL publish test reports as build artifacts

2. Android SDK test job SHALL:
   - Checkout source
   - Setup JDK 17 (temurin)
   - Cache Gradle dependencies
   - Run `./gradlew :sdk:testDebugUnitTest`
   - Upload test results XML

3. iOS SDK test job SHALL:
   - Checkout source
   - Select Xcode 15.0+
   - Run `xcodebuild test -project ios/project.yml` or `swift test`
   - Upload test results

### Reason

零本地环境运行时，CI 是唯一的质量门禁。自动化 CI 确保每次提交不会破坏现有测试。
Android 和 iOS 使用不同 runner 类型优化构建速度（macOS 比 Linux 慢且贵，因此只用 macOS 跑 iOS 测试）。

---

## 附录 A: 测试框架版本

| 平台 | 框架 | 版本 | 备注 |
|------|------|------|------|
| Android | JUnit | 4.13.2 | 已存在 |
| Android | MockK | 1.13.9 | 已存在 |
| Android | kotlinx-coroutines-test | 1.7.3 | 已存在 |
| Android | Robolectric | 4.11.1 | 需添加 |
| iOS | XCTest | 内置 | 系统框架 |
| iOS | URLProtocol Mock | 内置 | 自定义 MockURLProtocol |

## 附录 B: 测试目标覆盖矩阵

| SDK 模块 | 测试文件 | 最简用例数 | 优先级 |
|----------|----------|-----------|--------|
| `android/error/DkError.kt` | `DkErrorTest.kt` | 12 | P0 |
| `android/bertlv/BertlvDecoder.kt` | `BertlvDecoderTest.kt` | 15 | P0 |
| `android/bertlv/BertlvEncoder.kt` | `BertlvEncoderTest.kt` | 10 | P0 |
| `android/logger/DkLogger.kt` | `DkLoggerTest.kt` | 8 | P1 |
| `android/DeviceCapabilities.kt` | `DeviceCapabilitiesTest.kt` | 5 | P1 |
| `ios/DigitalKeySDK.swift` | `DigitalKeySDKTests.swift` | 8 | P0 |
| `ios/Error/DkError.swift` | `DkErrorTests.swift` | 10 | P0 |
| `ios/KeychainManager.swift` | `KeychainManagerTests.swift` | 12 | P0 |
| `ios/Bertlv/*.swift` | `BertlvDecoderTests.swift` | 15 | P0 |
| `ios/Bertlv/*.swift` | `BertlvEncoderTests.swift` | 8 | P1 |
| **合计** | | **~103** | |

## 附录 C: 关键决策记录

1. **Decision**: Android SDK 测试用 Robolectric 而非纯 JVM
   - **Rationale**: SDK 部分模块依赖 Android SDK API（如 `android.util.Log`, `PackageManager`），Robolectric 提供 shadow 支持
   - **Alternative**: 纯 JVM + 接口抽象，需要重构代码

2. **Decision**: iOS SDK 测试放在 Package.swift 而非 Xcode project
   - **Rationale**: 便于 CI 用 `swift test` 运行，减少 Xcode 依赖
   - **Scope**: 仅 SDK 模块（`ios/Sources/`），App 层仍用 Xcode project
