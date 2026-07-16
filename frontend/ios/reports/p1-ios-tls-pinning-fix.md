# P1-3: iOS TLS Pinning 实现报告

## 概述

专家评审指出 iOS 端 Keychain-first 架构是标杆，但缺少明确的 TLS Pinning 配置。本次实现为 iOS SDK 添加了完整的 TLS Pinning 支持，使用 `URLSessionDelegate` 实现证书/公钥锁定。

## 问题定位

原有 `DigitalKeySDK.swift` 引用了 `ApiClient` 类，但网络层代码未实现（`apiClient` 属性声明了但类不存在）。所有 API 请求依赖系统默认的 TLS 验证，无法防止 CA 劫持或中间人攻击。

## 修改清单

### 新增文件（3 个）

| 文件 | 用途 |
|------|------|
| `Sources/DigitalKeySDK/Network/TlsPinningDelegate.swift` | TLS Pinning Delegate 核心实现 |
| `Sources/DigitalKeySDK/Network/ApiClient.swift` | 集成 Pinning 的 API 客户端 |
| `reports/p1-ios-tls-pinning-fix.md` | 本报告 |

### 目录结构

```
Sources/DigitalKeySDK/
├── Network/
│   ├── TlsPinningDelegate.swift   ← 新增
│   └── ApiClient.swift            ← 新增
├── DigitalKeySDK.swift            ← (无修改，已引用 ApiClient)
├── DkError.swift                  ← (无修改)
├── ...其余文件...
```

## 实现细节

### 1. 公钥锁定 (Public Key Pinning) — 推荐

位置：`TlsPinningDelegate.swift` — `validatePublicKeys()`

- 提取服务器证书链中每张证书的公钥
- 按 RFC 7469 规范构造 SubjectPublicKeyInfo (SPKI)
- 计算 SHA-256 哈希并与预置哈希列表比对
- 支持 RSA 和 ECDSA (P-256/P-384) 密钥

```swift
// 使用方式
let delegate = TlsPinningDelegate.publicKeyPinning(
    host: "api.digitalkey.cn",
    hashes: ["WoiWRyIOVNa9ihaBciRSC7XHjliYS9VwUGOIud4PB18="],
    isDebug: false
)
```

公钥锁定的优势：证书轮换时只要公私钥不变，客户端无需更新，后端只需重新签发证书。

### 2. 证书锁定 (Certificate Pinning) — 更严格

位置：`TlsPinningDelegate.swift` — `validateCertificates()`

- 直接比对完整 DER 编码证书的 SHA-256 哈希
- 适合对安全要求极高的场景

```swift
let delegate = TlsPinningDelegate.certificatePinning(
    host: "api.digitalkey.cn",
    hashes: ["AAAA...", "BBBB..."],  // 旧 + 新证书哈希
    isDebug: false
)
```

### 3. 多证书/多公钥回退

两种策略都接受 `[String]` 数组，支持同时配置多个哈希：

```swift
.publicKey(hashes: [
    "current_key_hash...",   // 当前公钥
    "backup_key_hash...",    // 备用公钥（轮换过渡期）
    "another_backup...",     // 更多备用
])
```

当某个公钥即将过期时，提前在 `hashes` 中添加新公钥的哈希，发布新版本 SDK。证书正式轮换后，旧公钥可从列表中移除。

### 4. Pinning 失败安全处理 — 不静默降级

位置：`TlsPinningDelegate.swift` — `urlSession(_:didReceive:completionHandler:)`

```
Release 模式:
  Pinning 通过 → .useCredential (放行)
  Pinning 失败 → .cancelAuthenticationChallenge (拒绝连接)

Debug 模式:
  Pinning 通过 → .useCredential (放行)
  Pinning 失败 → 记录日志 + 上报安全事件 + 仍放行 (便于开发调试)
```

Pinning 失败时触发三件事：
1. **连接被拒绝** — 返回 `URLError` 给调用方
2. **安全事件上报** — 调用 `DkTelemetry.shared.trackSecurityEvent(eventType: "tls_pinning_failed", ...)`
3. **日志记录** — 通过 `DkLogger.shared.secE()` 记录错误详情

### 5. 集成到 API 客户端

`ApiClient.swift` 在初始化时自动创建 `TlsPinningDelegate`，注入 URLSession：

```swift
init(config: SdkConfig) {
    // ...
    let pinning = TlsPinningDelegate(
        pinnedHosts: [serverHost: .publicKey(hashes: [])],
        isDebug: isDebug
    )
    self.session = URLSession(configuration: config, delegate: pinning, delegateQueue: nil)
}
```

所有 HTTP 请求自动通过 Pinning 验证：
- GET/POST/PUT/DELETE 全覆盖
- 自动从 Keychain 读取 API Key 进行 Bearer 认证
- 统一的错误码和埋点上报

### 6. 上线前操作

`ApiClient.swift` 中的 `hashes: []` 是占位空数组，上线前需：

```bash
# 提取服务器公钥 SHA-256 (Base64)
openssl s_client -connect api.digitalkey.cn:443 </dev/null 2>/dev/null \
  | openssl x509 -pubkey -noout \
  | openssl rsa -pubin -outform der 2>/dev/null \
  | openssl dgst -sha256 -binary \
  | openssl enc -base64

# 输出例如: WoiWRyIOVNa9ihaBciRSC7XHjliYS9VwUGOIud4PB18=
```

将输出填入 `ApiClient.swift` 的 `hashes` 数组即可。

### 7. 动态更新 Pinning 哈希

`ApiClient.updatePinningHashes(host:hashes:)` 提供了运行时更新接口，
服务器端可下发新的 Pinning 配置，客户端热更新（需配合 SDK 版本轮换策略）。

### 8. 通配符域名支持

`findMatchingStrategy(for:)` 支持 `*.example.com` 通配符匹配，方便测试环境和子域部署。

## 错误码扩展

在 Transport Errors (0x08XX) 范围内预留了 TLS Pinning 相关错误码：

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0x0809 | TLS_PINNING_FAILED | Pinning 校验未通过 |
| 0x080A | TLS_PINNING_CONFIG_MISSING | Pinning 配置缺失 |
| 0x080B | TLS_CERT_EXPIRED | 服务器证书过期 |
| 0x080C | TLS_CERT_REVOKED | 服务器证书已吊销 |

## 安全设计原则

1. **最大防御** — 所有生产环境 API 请求强制执行 Pinning
2. **不静默降级** — Pinning 失败 = 连接取消 + 安全事件上报
3. **Debug 模式可放行** — 不影响开发环境调试
4. **证书链验证** — 先验证系统信任链，再执行 Pinning
5. **最小权限** — 使用 `URLSessionConfiguration.ephemeral` 不持久化 cookie/cache
6. **Keychain 集成** — API Key 从 Keychain 读取，不保留在内存

## 依赖

- `Security.framework` — 证书链提取、公钥操作（已集成）
- `CryptoKit.framework` — SHA-256 哈希计算（已集成，iOS 13+）

无需额外依赖。

## 测试建议

1. **单元测试** — 使用本地自签名证书验证 Pinning 逻辑
2. **集成测试** — 用测试环境域名验证通过场景
3. **负面测试** — 修改哈希验证失败场景的日志和事件上报
4. **证书轮换测试** — 模拟新旧证书过渡期的多哈希场景

## 遗留问题

1. **`DigitalKeyError` 命名冲突** — `DigitalKeySDK.swift` 中定义了 `public enum DigitalKeyError`，`DkError.swift` 中定义了 `public struct DigitalKeyError`。两者在同一个 module 中会导致编译错误。建议保留 `DkError.swift` 中的 `struct` 版本，删除 `DigitalKeySDK.swift` 中的 `enum` 版本并统一引用。
2. **`ApiClient` 初始化参数** — `DigitalKeySDK.swift` 中 `lazy var apiClient: ApiClient = ApiClient(config: config)` 需要 `init(config:)` 构造器，已在 `ApiClient.swift` 中实现。
3. **上线前需注入 Pinning 哈希** — `ApiClient` 中 `hashes: []` 为占位值。
