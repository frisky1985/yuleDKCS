# iOS TLS Pinning 实现报告

## 概述

本项目已按照安全审计要求，在 iOS 客户端（App 层）实现了 TLS Pinning（证书固定/公钥固定）机制，防止中间人攻击（MITM）。

## 架构

本项目存在两层网络客户端：

| 层次 | 文件 | 状态 |
|------|------|------|
| **SDK 层** | `ios/Sources/DigitalKeySDK/Network/ApiClient.swift` | ✅ 已有 TLS Pinning |
| **SDK Delegate** | `ios/Sources/DigitalKeySDK/Network/TlsPinningDelegate.swift` | ✅ 已有的 Pinning 委托 |
| **App 层** | `ios-app/Service/APIClient.swift` | ⚠️ **本次修复** |

原始问题：App 层的 `APIClient` 使用 `URLSessionConfiguration.default` 创建 URLSession，**没有设置 URLSessionDelegate**，因此没有任何证书固定逻辑。

## 修改文件

### 1. `ios-app/Service/APIClient.swift`

**改动内容：**

1. **新增 `import DigitalKeySDK`** — 导入 SDK 模块以使用 `TlsPinningDelegate`
2. **新增 `TLSPinningConfig` 结构体** — 封装 Pinning 配置：
   - `pinnedHosts: [String: [String]]` — 域名 → 公钥 SHA-256 Base64 哈希列表
   - `isDebug: Bool` — Debug 模式下 Pinning 失败仅记录不阻断
3. **重构 `init` 方法** — 支持 `tlsConfig` 参数：
   - 注入 Mock Session → 使用注入的 Session（测试场景）
   - 有 Pinning 配置 → 使用 `TlsPinningDelegate` 创建带 Delegate 的 URLSession
   - 无配置 → 回退到默认 URLSession
4. **新增 `APIError.tlsPinningFailed(host:reason:)`** — Pinning 失败错误类型
5. **新增 `updatePinningConfig(_:)`** — 运行时热更新 Pinning 配置
6. **新增 `updatePinningHashes(host:hashes:)`** — 动态更新特定域名的公钥哈希
7. **新增 `handlePinningFailed(errorInfo:)`** — Pinning 失败处理（日志 + 通知）
8. **新增 `Notification.Name.tlsPinningFailed`** — 供 UI 层监听 Pinning 失败事件
9. **增强 `mapNetworkError`** — 识别 TLS 证书相关的 `URLError` 并映射为 Pinning 错误

### 2. `ios-tests/DigitalKeyAppTests/TLSPinningDelegateTests.swift`（新增）

**测试覆盖：**

| 测试类别 | 测试方法 | 说明 |
|----------|----------|------|
| **SHA-256 哈希** | `testSHA256OfKnownData` | 验证已知数据哈希值 |
| | `testSHA256OfEmptyData` | 验证空数据哈希 |
| | `testCertificateHashMatches` | 验证证书哈希确定性 |
| | `testSPKIHashIsValid` | 验证 SPKI 哈希计算 |
| | `testSPKIHashDiffersFromCertHash` | SPKI ≠ Cert Hash |
| **PinningStrategy** | 6 个测试 | 策略相等性、类型、多哈希 |
| **Delegate 初始化** | 8 个测试 | 单域名、多域名、Debug、Disabled、工厂方法 |
| **挑战传递** | `testNonServerTrustChallengePassesThrough` | NTLM 认证走默认处理 |
| | `testUnpinnedHostPassesThrough` | 未配置域名走默认处理 |
| | `testDisabledStrategySkipsValidation` | Disabled 策略跳过校验 |
| **公钥锁定** | `testPublicKeyPinningPassesWithCorrectHash` | ✅ 正确哈希通过 |
| | `testPublicKeyPinningFailsWithIncorrectHash` | ❌ 错误哈希拒绝 |
| **证书锁定** | `testCertificatePinningPassesWithCorrectHash` | ✅ 正确哈希通过 |
| | `testCertificatePinningFailsWithIncorrectHash` | ❌ 错误哈希拒绝 |
| **多哈希轮换** | `testPinningPassesWithMultipleHashesWhenSecondMatches` | 多哈希任一个匹配即可 |
| | `testPinningFailsWhenNoHashMatches` | 全部不匹配 → 拒绝 |
| **Release/Debug** | `testReleaseModeCancelsFailedPinning` | Release 模式拒绝连接 |
| | `testDebugModePermitsFailedPinning` | Debug 模式放行+记录 |
| **回调** | `testOnPinningPassedCallback` | 验证成功回调 |
| | `testOnLogCallback` | 验证日志回调 |
| **APIClient 集成** | 8 个测试 | 配置、Mock、更新、清空 |
| **APIError** | 3 个测试 | Pinning 错误描述、相等性 |

**测试证书：** 测试使用通过 openssl 生成的 RSA 2048-bit 自签名证书（CN=api.digitalkey.test），通过 Security + CryptoKit 框架在测试运行时动态计算哈希值。

## 安全原则

1. **绝不静默降级** — Release 构建中 Pinning 校验不通过 = 连接取消
2. **Debug 构建** — 仅记录失败不阻断（方便开发调试）
3. **多哈希轮换** — 支持配置多个公钥哈希，证书轮换时无缝切换
4. **SPKI 规范** — 公钥哈希按 RFC 7469 标准通过 SubjectPublicKeyInfo 计算
5. **安全事件上报** — onPinningFailed 回调通知应用层，可触发埋点上报

## 上线前配置

在上线前，需要通过以下方式获取并设置正确的公钥哈希：

```bash
# 获取服务器公钥 SPKI SHA-256 Base64 哈希
openssl s_client -connect your-server.com:443 </dev/null 2>/dev/null \
  | openssl x509 -pubkey -noout \
  | openssl rsa -pubin -outform der 2>/dev/null \
  | openssl dgst -sha256 -binary \
  | openssl enc -base64
```

然后将输出填入 `TLSPinningConfig`：

```swift
let tlsConfig = TLSPinningConfig(
    pinnedHosts: [
        "api.digitalkey.cn": ["<spki_hash_base64>"],
        "backup.digitalkey.cn": ["<backup_spki_hash>"],
    ],
    isDebug: false  // Release 构建必须为 false
)

let client = APIClient(tlsConfig: tlsConfig)
```

## SDK 层说明

SDK 层的 `ApiClient` 已独立实现 TLS Pinning 支持（通过 `TlsPinningDelegate`），与 App 层互不干扰。App 层通过引入 SDK 模块复用 `TlsPinningDelegate` 实现。

## 文件清单

| 文件 | 操作 |
|------|------|
| `ios-app/Service/APIClient.swift` | ✅ 修改（添加 TLS Pinning） |
| `ios/Sources/DigitalKeySDK/Network/TlsPinningDelegate.swift` | ✅ 无需修改（已有） |
| `ios/Sources/DigitalKeySDK/Network/ApiClient.swift` | ✅ 无需修改（已有） |
| `ios-tests/DigitalKeyAppTests/TLSPinningDelegateTests.swift` | ✨ 新增（33个测试用例） |
| `ios-tests/DigitalKeyAppTests/MockURLProtocol.swift` | ✅ 无需修改（已有） |
| `reports/ios-tls-pinning-report.md` | ✨ 本报告 |
