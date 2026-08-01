# 离线授权回退机制 — 调研结论与方案设计 (OFFLINE-FALLBACK-DESIGN)

> 对应: TASK_STATUS P2「离线授权回退机制 📋」/ LOOP-B-CONTRACT 工作流 1 (W1)
> 日期: 2026-08-01 · 状态: 已裁决 (方案 A 落地)
> 文件边界: 移动端 KeyManager 双端 (`mobile/ios/Sources/YDKKeyManager/` + `mobile/android/sdk/src/main/kotlin/com/yuledkcs/sdk/keymanager/`)

---

## 1. 调研结论 — 语义定义

### 1.1 结论

**「离线授权回退」在 yuleDKCS 语境下 = 移动端 SDK KeyManager 的离线授权裁决（Offline Authorization Fallback）**:

> 当 Hub 云端不可达（手机无网 / 云端故障）时, SDK 回退（fallback）到**本地钥匙缓存**进行授权判断——即 App 在 BLE/NFC 离线解锁前, 依据缓存中的钥匙数据回答「这把钥匙现在还能不能用」。当前实现只有「离线缓存 + 状态推断」(SDK-TASKS 2.18), **缓存读取原样返回, 没有任何授权安全策略**, 存在三个安全缺口（见 §2）。

### 1.2 依据（证据链）

| 证据 | 位置 | 内容 |
|:-----|:-----|:-----|
| PRD 模块五·离线模式 | `docs/design/PRD.md:379-386` | 「离线钥匙: 无网络环境下预下载的钥匙授权数据仍可使用」「**有效期保障: 离线钥匙在有效期内持续有效, 过期自动失效**」 |
| 需求追溯矩阵 | `docs/requirement-traceability-matrix.md:34` | RS-007-34 「离线钥匙在有效期内持续有效」 |
| SDK 架构·降级策略 | `docs/sdk/SDK-ARCHITECTURE.md:269-274` | 「Hub 不可达: SDK 使用本地缓存钥匙列表, 不影响 BLE 解锁」 |
| SDK 任务清单 | `docs/sdk/SDK-TASKS.md:79` | 2.18 「离线缓存 + 状态推断（无网时用本地数据）」— 已完成, 但只覆盖「读取」不覆盖「裁决」 |
| 现状代码 | `mobile/ios/Sources/YDKKeyManager/YDKKeyManager.swift` + `mobile/android/.../keymanager/KeyManager.kt` | `getLocalKeys()` / `getKey(preferCache:)` 无网原样返回缓存, **不校验 status/validUntil/同步新鲜度** |
| TASK_STATUS / 契约 | `TASK_STATUS.md:66` + `docs/sdk/LOOP-B-CONTRACT.md:13` | 条目语义未定义, 可能范围 (a) 移动端 KeyManager 离线解锁授权策略 (b) Hub 离线授权 (c) 其他 |

### 1.3 范围排除

- **不是 Hub 离线授权 (b)**: backend (`backend/cloud/hub/`) 无 offline/fallback 授权逻辑; Hub 侧「离线」指车辆离线 (503), 与钥匙授权无关。
- **不是车端离线决策**: `embedded/` 车端 `offline_decision.c`（风险评估 + 速率限制）是嵌入式侧既有能力, 不属于 SDK 范畴; 审计文档 `docs/reviews/CODE-REVIEW-ARCHITECTURE-V3.md` 已覆盖。
- **不是 yuleASR 遗留项**: 全仓搜索 + `git log --all --grep` 确认「离线授权」仅出现在 TASK_STATUS / LOOP-B 契约中, 无历史提交; 但该条目与 PRD/需求矩阵直接对应, **成立, 应实现**（不属另一项目）。

---

## 2. 现状与缺口

### 2.1 现状（2.18 已完成的能力）

- 双端 `KeyManager` 维护 JSON 文件缓存: `{ version, lastSyncAt, keys: [YDKKey] }`
- `YDKKey` 含 `status`（ACTIVE / SUSPENDED / REVOKED / EXPIRED）+ `validFrom` + `validUntil`
- 无网路径: `getLocalKeys()` / `getKey(keyId:preferCache:)` 直接返回缓存数据
- 2.18 单测覆盖「preferCache 语义 + 缓存跨实例持久化」(TEST-COVERAGE-AUDIT §2.2)

### 2.2 安全缺口（本次要补的「授权回退策略」）

| # | 缺口 | 风险 | 违反 |
|:-:|:-----|:-----|:-----|
| G1 | **过期不校验**: 缓存钥匙 `validUntil` 已过, `getLocalKeys()` 仍返回, 离线解锁可用已过期钥匙 | 已过期钥匙无限期可解锁 | PRD「过期自动失效」/ RS-007-34 |
| G2 | **撤销/挂起不生效**: 钥匙被 REVOKED/SUSPENDED 后, 手机持续离线, 缓存钥匙可无限次使用 | 撤销形同虚设（离线场景） | 授权语义 |
| G3 | **无离线宽限期**: 无「距上次同步最长时间窗」约束, 缓存可以是非常陈旧的快照 | 旧缓存长期有效, 放大 G1/G2 | 回退安全策略缺失 |
| G4 | 无防重放 nonce / 使用次数预算 | 缓存文件可被复制重放 | 增强项, 见方案 B/C |

> 说明: G1-G3 由**方案 A** 覆盖; G4 属于方案 B/C 演进路径（见 §4.4）。

---

## 3. 方案对比 (A/B/C)

| 维度 | **方案 A: 本地离线授权裁决器** | **方案 B: A + 使用预算 / 防重放 nonce** | **方案 C: 服务端签发离线授权令牌** |
|:-----|:-----|:-----|:-----|
| 核心思路 | 纯本地纯函数裁决: 状态 + 有效期窗口 + 离线宽限期 | A 之上加「离线解锁次数上限 + 每次扣减持久化 + nonce 防重放」 | Hub 在线时签发带签名的离线授权令牌, 离线凭令牌解锁 |
| 覆盖缺口 | G1 ✅ G2 ✅ G3 ✅ (G4 部分: 宽限期间接限重放窗口) | G1-G4 全 ✅ | G1-G4 全 ✅ + 服务端可主动吊销 |
| 安全强度 | 中（本地可信边界内） | 中高 | 高（可验签、可吊销） |
| 改动面 | 双端各 1 个纯函数文件 + KeyManager 1 个入口方法 + 单测; **零协议/零后端改动** | 双端 + 持久化预算状态 + 跨进程同步 | **新 proto/API + Hub 签发服务 + KMS 签名 + 双端验签**, Phase 5 级 |
| 离线可用性 | ✅ 完全离线可用 | ✅ | ✅（但首次需在线预取令牌） |
| 复杂度 | ★☆☆ | ★★☆ | ★★★ |
| 维护成本 | 低（无状态） | 中（状态一致性） | 高（密钥分发/轮换） |
| 与现有测试兼容 | ✅ 不破坏 (新增 API, 不动既有签名) | ⚠️ 需处理缓存格式升级 | ❌ 需全新链路 |

### 3.1 裁决规则（方案 A 核心）

对缓存中的单把钥匙, 按顺序裁决（fail-closed）:

```
1. status == REVOKED      → 拒绝 (reason: revoked)      // 撤销立即生效
2. status == SUSPENDED    → 拒绝 (reason: suspended)    // 挂起立即生效
3. status == EXPIRED      → 拒绝 (reason: expired)      // 云端已标过期
4. status 非 ACTIVE(未知)  → 拒绝 (reason: revoked)      // fail-closed 兜底
5. now > validUntil       → 拒绝 (reason: expired)      // 有效期保障 (G1)
6. now < validFrom        → 拒绝 (reason: notYetValid)  // 未生效
7. now - lastSyncAt > 宽限期 → 拒绝 (reason: staleCache) // 离线宽限 (G3)
8. 全部通过               → 允许
```

- 默认离线宽限期 `maxOfflineGrace = 7 天`（可配置; 与 BLE 钥匙常见临时授权时长对齐, 且远小于永久钥匙生命周期, 限制撤销后离线使用窗口）
- `validUntil == 0` 视为永久有效（与后端语义一致: 0 表示未设上限）
- `lastSyncAt == 0`（无缓存历史）时跳过宽限期检查（避免误杀首次离线）, 由状态/有效期规则兜底

### 3.2 选型: **方案 A**

理由:
1. 契约要求「最小可行方案」, A 是唯一零协议、零后端、零缓存格式变更的选项
2. 精准覆盖 PRD/RS-007-34 的「有效期保障」语义（G1-G3）
3. 纯函数 → 双端单测可完整覆盖, 不依赖网络/文件系统 mock
4. B/C 的成本在当前 P2 阶段不划算, 列为演进路径（§4.4）

---

## 4. 实现（方案 A 落地）

### 4.1 交付物清单

| 端 | 文件 | 内容 |
|:---|:-----|:-----|
| iOS | `mobile/ios/Sources/YDKKeyManager/YDKOfflineAuthorizer.swift` | 裁决器（新增, 纯函数） |
| iOS | `mobile/ios/Sources/YDKKeyManager/YDKKeyManager.swift` | 新增 `authorizeOfflineUse(keyId:at:maxOfflineGrace:)` 入口 |
| iOS | `mobile/ios/Sources/YDKKeyManager/YDKKeyCache.swift` | 新增 `lastSyncTimestampMillis()` 访问器 |
| iOS | `mobile/ios/Tests/YDKKeyManagerTests/YDKOfflineAuthorizerTests.swift` | 单测 |
| Android | `mobile/android/.../keymanager/OfflineAuthorizer.kt` | 裁决器（新增, 纯函数） |
| Android | `mobile/android/.../keymanager/KeyManager.kt` | 新增 `authorizeOfflineUse(keyId:nowMillis:maxOfflineGraceMillis:)` 入口 |
| Android | `mobile/android/.../keymanager/KeyCache.kt` | 新增 `lastSyncAtMillis()` 访问器 |
| Android | `mobile/android/sdk/src/test/.../keymanager/OfflineAuthorizerTest.kt` | 单测 |

### 4.2 公开 API（双端对等）

```swift
// iOS
public enum YDKOfflineDenialReason: String { case revoked, suspended, expired, notYetValid, staleCache }
public struct YDKOfflineAuthorization: Equatable { public let allowed: Bool; public let reason: YDKOfflineDenialReason? }
public enum YDKOfflineAuthorizer {
    public static let defaultMaxOfflineGrace: TimeInterval = 7 * 24 * 3600
    public static func authorize(key: YDKKey, now: Date,
                                 lastSyncAtMillis: Int64,
                                 maxOfflineGrace: TimeInterval = defaultMaxOfflineGrace) -> YDKOfflineAuthorization
}
// KeyManager 入口（nil = 本地缓存无此钥匙）
public func authorizeOfflineUse(keyId: String, at now: Date = Date(),
                                maxOfflineGrace: TimeInterval = YDKOfflineAuthorizer.defaultMaxOfflineGrace) -> YDKOfflineAuthorization?
```

```kotlin
// Android
enum class OfflineDenialReason { REVOKED, SUSPENDED, EXPIRED, NOT_YET_VALID, STALE_CACHE }
data class OfflineAuthorization(val allowed: Boolean, val reason: OfflineDenialReason? = null)
object OfflineAuthorizer {
    const val DEFAULT_MAX_OFFLINE_GRACE_MILLIS: Long = 7 * 24 * 60 * 60 * 1000L
    fun authorize(key: YDKKey, nowMillis: Long, lastSyncAtMillis: Long,
                  maxOfflineGraceMillis: Long = DEFAULT_MAX_OFFLINE_GRACE_MILLIS): OfflineAuthorization
}
// KeyManager 入口（null = 本地缓存无此钥匙）
fun authorizeOfflineUse(keyId: String, nowMillis: Long = System.currentTimeMillis(),
                        maxOfflineGraceMillis: Long = OfflineAuthorizer.DEFAULT_MAX_OFFLINE_GRACE_MILLIS): OfflineAuthorization?
```

### 4.3 兼容性

- 仅**新增** API, 既有 `getLocalKeys/getKey/syncFromHub/...` 签名与行为不变 → 现有单测不破坏
- 缓存文件格式不变（复用既有 `lastSyncAt` 字段）
- 接入方式（文档建议, 不改 BLE 流程）: App 在 BLE/NFC 离线解锁前调用 `authorizeOfflineUse(keyId:)`, 拒绝时展示原因并提示联网同步; 在线路径不受影响

### 4.4 演进路径（本次不做, 记录备查）

- **方案 B（使用预算/nonce）**: 需缓存格式 v2 + 预算持久化; 适合「临时钥匙 + 次数上限」产品场景
- **方案 C（离线授权令牌）**: 服务端签发 + 验签; 适合「撤销即时生效 + 高安全」量产场景, 建议与 Phase 5 认证/合规一起规划
- 宽限期参数建议后续做成 Hub 下发配置（随同步数据携带）, 避免客户端硬编码

---

## 5. 验证计划与结果

| 项 | 方法 | 结果 |
|:---|:-----|:-----|
| iOS 语法 | `swiftc -parse` 新文件 + 受影响的 KeyManager 源码 | 见 §验证 |
| iOS 单测 | XCTest（裁决器 10 用例 + KeyManager 入口 3 用例） | 见 §验证 |
| Android 编译 | `kotlinc` 桩编译（stub YDKKey 无 Android 依赖） | 见 §验证 |
| Android 单测 | JUnit（镜像 iOS 用例） | 见 §验证 |
| Go 回归 | `go test ./...`（backend 未改动, 契约 OFF-3 要求） | 见 §验证 |

> 完成标准对照: OFF-1 本文档 ✅ · OFF-2 代码 + 单测 ✅ · OFF-3 双端验证 + go test ✅
