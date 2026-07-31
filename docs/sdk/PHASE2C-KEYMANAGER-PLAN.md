# yuleDKCS SDK Phase 2c — KeyManager 实现计划

> **平台**: iOS (Swift) + Android (Kotlin)  
> **依赖**: Phase 2a HubClient（已完成）  
> **工时**: ~2 天 (双平台并行)

---

## 架构

```
Hub REST Gateway
    ↑ listKeys() ↓
HubClient (Phase 2a)
    ↑ sync ↓
KeyManager ──→ JSON 文件缓存 (本地)
    ↓
App (通过 delegate/callback 收到变更通知)
```

KeyManager 是一个纯本地模块，不直接调任何网络 API——它通过 HubClient 与云端同步。

---

## 核心职责

| # | 能力 | 说明 |
|:--|:-----|:------|
| 1 | **本地钥匙缓存** | JSON 文件持久化，保存 YDKKey 列表 |
| 2 | **定时同步** | 周期性调 HubClient.listKeys() 拉取最新状态 |
| 3 | **差异检测** | 对比本地缓存 vs 云端返回，发现新增/变更/删除的钥匙 |
| 4 | **Push 触发同步** | App 收到 Push 后调用 SDK，KeyManager 做增量同步 |
| 5 | **离线访问** | 无网时返回本地缓存数据 |
| 6 | **变更通知** | 当钥匙状态变更时通知 App（KVO/回调） |

---

## iOS 实现

### 项目结构

```
Sources/YDKKeyManager/
├── YDKKeyManager.swift          # 公开入口
├── YDKKeyManager+Sync.swift     # 同步逻辑 + 差异检测
├── YDKKeyManager+Push.swift     # Push 触发增量同步
└── YDKKeyCache.swift            # JSON 文件缓存
```

### 公开接口

```swift
public class YDKKeyManager {
    // 初始化（绑定 HubClient）
    public init(hubClient: YDKHubClient, cacheFileURL: URL? = nil)

    // 获取本地缓存的钥匙列表（无网可用）
    public func getLocalKeys() -> [YDKKey]

    // 手动触发同步
    // - 调 HubClient.listKeys()
    // - 检测差异
    // - 更新本地缓存
    // - 触发 delegate 回调
    @discardableResult
    public func syncFromHub() async throws -> SyncResult

    // 获取单把钥匙（优先读取缓存，可选调 Hub）
    public func getKey(keyId: String, preferCache: Bool = true) -> YDKKey?

    // 处理 Push 通知 → 触发增量同步
    @discardableResult
    public func handleKeyStatusPush(keyId: String) async throws -> Bool

    // 清除所有缓存
    public func clearCache()

    // 代理（App 实现此 protocol 接收变更通知）
    public weak var delegate: YDKKeyManagerDelegate?
}

public protocol YDKKeyManagerDelegate: AnyObject {
    func keyManager(_ manager: YDKKeyManager, didDetectChanges changes: [KeyChange])
    func keyManager(_ manager: YDKKeyManager, syncDidFailWith error: Error)
}

public struct SyncResult {
    public let added: [YDKKey]
    public let updated: [YDKKey]
    public let removed: [YDKKey]
    public let unchanged: Int
}

public struct KeyChange {
    public let keyId: String
    public let type: ChangeType  // added / updated / removed
    public let key: YDKKey?
}

public enum ChangeType { case added, updated, removed }
```

### 缓存策略

```swift
// JSON 文件存于 App 沙箱
// ~/Library/Application Support/com.yuledkcs.sdk/keys_cache.json
//
// 格式:
// {
//   "version": 1,
//   "lastSyncAt": 1700000000,
//   "keys": [ YDKKey, ... ]
// }

struct KeyCacheData: Codable {
    let version: Int
    let lastSyncAt: Int64
    let keys: [YDKKey]
}
```

### 同步流程

```
syncFromHub():
  1. hubClient.listKeys()              → 获取云端钥匙列表
  2. 读取本地缓存 KeyCacheData           → 现有钥匙
  3. diff(cloud, local)                 → added / updated / removed
  4. 更新 KeyCacheData.keys = cloud      → 写入 JSON 文件
  5. delegate?.keyManager(didDetectChanges:)  → 通知 App
  6. return SyncResult
```

---

## Android 实现

### 项目结构

```
sdk/src/main/kotlin/com/yuledkcs/sdk/keymanager/
├── KeyManager.kt          # 公开入口 + 同步逻辑
├── KeyCache.kt            # JSON 文件缓存
└── KeyChange.kt           # 数据模型
```

### 公开接口（与 iOS 对等）

```kotlin
class KeyManager(
    private val hubClient: HubClient,
    private val cacheDir: File
) {
    // 获取本地缓存
    fun getLocalKeys(): List<YDKKey>

    // 手动同步
    suspend fun syncFromHub(): SyncResult

    // Push 触发增量同步
    suspend fun handleKeyStatusPush(keyId: String): Boolean

    // 清除缓存
    fun clearCache()

    // 同步状态（App 可观察）
    val syncState: StateFlow<SyncState>

    // 钥匙列表（可观察 Flow）
    val keys: StateFlow<List<YDKKey>>
}

data class SyncResult(
    val added: List<YDKKey>,
    val updated: List<YDKKey>,
    val removed: List<YDKKey>,
    val unchanged: Int
)
```

> Android 侧用 `StateFlow` 代替 delegate，App 通过 `collect()` 监听变更。

---

## Package 依赖

### iOS

```swift
// 零外部依赖
// 使用 Foundation 的 JSONEncoder/JSONDecoder + FileManager
.target(name: "YDKKeyManager", dependencies: ["YDKHubClient"])
```

### Android

```kotlin
// 零额外外部依赖
// 使用 Gson（已在 HubClient 中引入）+ 文件 I/O
```

---

## 排期

| 天 | iOS | Android | 可并行？ |
|:-:|:----|:--------|:--------:|
| 1 | KeyCache + KeyManager 同步逻辑 + 差异检测 | 同 iOS | ✅ |
| 2 | Push 触发 + 定时器 + delegate 通知 | 同 iOS | ✅ |
