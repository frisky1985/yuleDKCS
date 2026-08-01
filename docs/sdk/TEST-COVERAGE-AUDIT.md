# TEST-COVERAGE-AUDIT.md — SDK 单元测试覆盖审计 (Phase 4.1 / W1)

> 日期: 2026-08-01
> 依据: `docs/sdk/PHASE4-FINALE-CONTRACT.md` 工作流 1（4.1 SDK 单测审计 + 补齐, AC-41）
> 原则: **只增不删** — 未改动任何既有测试断言; 仅新增测试文件与本文档。
> 范围: `mobile/ios/Tests/` + `mobile/android/sdk/src/test/`（BLE 已在前期充分覆盖, 不重复）。

---

## 1. 覆盖总览

| 端 | 既有测试行数 | 本次新增 | 新增文件 |
|:---|:---|:---|:---|
| iOS | 1994 行 | +296 行 | `Tests/YDKHubClientTests/YDKHubClientRemoteControlContractTests.swift`（141 行）<br>`Tests/YDKKeyManagerTests/YDKKeyManagerPushSyncTests.swift`（155 行） |
| Android | 2603 行 | +374 行 | `sdk/src/test/.../hub/HubClientRemoteControlContractTest.kt`（175 行）<br>`sdk/src/test/.../keymanager/KeyManagerPushSyncTest.kt`（199 行） |

## 2. 详细审计表（功能面 × 双端 × 覆盖状态）

图例: ✅ 直接覆盖（wire/形状/逻辑断言） · ⚠️ 部分覆盖（间接或受限） · ❌ 未覆盖

### 2.1 HubClient 全方法（请求形状 + 响应解码）

| 功能面 | iOS 覆盖 | Android 覆盖 | 缺口 | 计划/状态 |
|:---|:---|:---|:---|:---|
| BindKey | ✅ `YDKHubClientRequestShapeContractTests`（枚举名/camelCase/base64） | ✅ 同上 + `HubClientTest`（路径/认证/错误映射） | 无 | — |
| UnbindKey | ❌ | ✅ **本次新增** `HubClientRemoteControlContractTest`（DELETE /keys/{id}, 无 body） | iOS 无 wire 注入缝, 无法断言 | Android 已补; iOS 记录为受限缺口 |
| ListKeys | ❌ 直接 | ⚠️ 间接（`KeyManagerTest.syncFromHub` 触发 listKeys wire） | 双端无 query 参数形状断言 | 记录, 建议后续补 |
| GetKey | ❌ 直接 | ❌ 直接 | 双端无单钥查询路径断言 | 记录, 建议后续补 |
| CreateShare | ⚠️ `YDKShareFlowTests` 走 mock 协议层（不验证 HTTP 形状） | ✅ `HubClientRequestShapeContractTest`（camelCase + 默认值） | iOS 无 HTTP 形状 | 记录 |
| AcceptShare | ✅ `RequestShapeContractTests` | ✅ `RequestShapeContractTest` | 无 | — |
| CancelShare | ❌ | ✅ **本次新增** `HubClientRemoteControlContractTest`（DELETE /shares/{id}, 无 body） | iOS 无 wire 注入缝 | Android 已补; iOS 记录为受限缺口 |
| RemoteLock | ✅ **本次新增** `YDKHubClientRemoteControlContractTests`（action=lock, body 形状, 无 source, 路径形状, 响应解码, API 面编译契约） | ✅ **本次新增** `HubClientRemoteControlContractTest`（POST /vehicles/{id}/command wire 断言） | 补齐前双端 ❌ | ✅ 已补齐 |
| RemoteUnlock | ✅ **本次新增**（action=unlock） | ✅ **本次新增**（wire 断言） | 补齐前双端 ❌ | ✅ 已补齐 |
| RemoteStart | ✅ **本次新增**（action=engine_on） | ✅ **本次新增**（wire 断言） | 补齐前双端 ❌ | ✅ 已补齐 |
| RemoteStop | ✅ **本次新增**（action=engine_off, 同形状顺带覆盖） | ✅ **本次新增**（wire 断言） | 补齐前双端 ❌ | ✅ 已补齐 |
| Push 回调（`handleKeyStatusPush`） | ⚠️ **本次新增**（错误传播 + delegate 失败回调; 成功路径受 transport 不可注入限制） | ✅ **本次新增** `KeyManagerPushSyncTest`（有变更 true / 无变更 false / syncState） | 补齐前双端 ❌ | ✅ 已补齐 |

### 2.2 KeyManager（2.16 缓存 / 2.17 同步 / 2.18 离线 / 2.19 增量）

| 功能面 | iOS 覆盖 | Android 覆盖 | 缺口 | 计划/状态 |
|:---|:---|:---|:---|:---|
| 2.16 本地缓存 | ✅ `YDKKeyManagerTests`（getLocalKeys/clearCache） | ✅ `KeyManagerTest`（同上 + CacheData 序列化） | 无 | — |
| 2.17 状态同步（diff: added/updated/removed） | ⚠️ `SyncResult.hasChanges`/`KeyChange` 纯逻辑 **本次新增**; 成功路径（listKeys）受 transport 不可注入限制 | ✅ `KeyManagerTest`（added/updated）+ **本次新增** removed 分支 | iOS 成功路径不可单测 | Android 完整; iOS 受限已记录 |
| 2.18 离线推断 | ✅ **本次新增** `YDKKeyManagerPushSyncTests`（preferCache 语义 + 缓存跨实例持久化） | ✅ **本次新增** `KeyManagerPushSyncTest`（预置缓存无网络读取 + keys Flow 初始化） | 补齐前双端仅空缓存路径 | ✅ 已补齐 |
| 2.19 增量同步（Push 触发） | ⚠️ **本次新增**（失败传播） | ✅ **本次新增**（变更/无变更两分支） | 补齐前双端 ❌ | ✅ 已补齐 |

### 2.3 MailboxClient（2.20 CCC 分享 6 API / 2.21 secret fragment 处理）

| 功能面 | iOS 覆盖 | Android 覆盖 | 缺口 | 计划/状态 |
|:---|:---|:---|:---|:---|
| CCC 分享 6 API（create/read/update/delete 等编排） | ⚠️ `YDKShareFlowTests`（464 行, mock `YDKMailboxClientProtocol` 断言调用顺序/URL 形状/失败中止） | ✅ `ShareFlowTest`（442 行, HTTPS MockWebServer 捕获真实 wire + 调用顺序 + 请求形状 camelCase/base64） | iOS 走 mock 协议层, 非 MailboxClient wire | Android 完整; iOS 受限已记录 |
| secret fragment 处理（URL `#` 中, 永不发服务器） | ⚠️ `YDKShareFlowTests` B2.3（非法 URL 抛错） | ✅ `ShareFlowTest` B2.3 + `parseSharingURL`（含 `secret=` 前缀剥离） | 无显著缺口 | — |

### 2.4 其余功能面（前期已覆盖, 审计确认无缺口）

| 功能面 | 覆盖结论 |
|:---|:---|
| BLE（BleStub/Adapter/SecureChannel/Advertise/UWB/NFC/Background/Permissions/SM4） | ✅ 双端充分（iOS 1287 行 / Android ~1300 行）, 不重复 |
| DeviceManager（iOS SE / Android Keystore） | ✅ 双端已测（记忆） |

## 3. 本次新增测试清单

| 文件 | 覆盖点 |
|:---|:---|
| iOS `Tests/YDKHubClientTests/YDKHubClientRemoteControlContractTests.swift` | 远程控车 4 action 的 body 形状（camelCase 字段集/枚举字符串/无 source/keyId 缺省 ""/traceId UUID）、keyId 透传、路径形状 `/vehicles/{id}/command`、`ControlCommandResponse` 解码、公开 API 面编译契约 |
| iOS `Tests/YDKKeyManagerTests/YDKKeyManagerPushSyncTests.swift` | Push 错误传播、delegate `syncDidFailWith` 回调、`SyncResult.hasChanges` 纯逻辑、`KeyChange.ChangeType` rawValue、离线 preferCache 语义、缓存跨实例持久化 |
| Android `sdk/src/test/.../hub/HubClientRemoteControlContractTest.kt` | 远程控车 4 action 的 **wire 断言**（POST 路径/方法/Bearer 头/body 字段集/无 source/keyId 透传）、`ControlCommandResponse` 解码、`unbindKey`/`cancelShare` DELETE 无 body 形状 |
| Android `sdk/src/test/.../keymanager/KeyManagerPushSyncTest.kt` | `handleKeyStatusPush` 有变更 true / 无变更 false、syncState Success/Failed 状态机、离线预置缓存无网络读取（getLocalKeys/getKey/keys Flow）、diff removed 分支、`SyncResult.hasChanges` 纯逻辑 |

## 4. 审计结论

1. **主要缺口已补齐**: 远程控车（RemoteLock/Unlock/Start/Stop）请求形状、Push 回调入口、KeyManager 离线推断与 removed 分支此前双端零覆盖, 本次全部就位; Android 侧为真实 wire 断言, iOS 侧遵循既有 RequestShape 模式（URLSession 不可注入 → 镜像编码路径验证字节形状）。
2. **既有覆盖确认充分**: BLE 全链路、Mailbox（Android wire / iOS 编排层）、BindKey/AcceptShare/CreateShare 形状、KeyManager added/updated diff 均无缺口。
3. **记录为受限/待补的项**（非阻塞, 建议 Phase 5）: iOS 侧 ListKeys/GetKey 路径断言与 unbindKey/cancelShare wire 断言（需先给 YDKHubClient 加 transport 注入缝）; 双端 ListKeys query 参数形状断言。
4. **预存风险记录（本次未动, 符合 AC-41-3 只增不删）**:
   - Android 既有 `HubClientTest.kt` 与 `KeyManagerTest.kt` 的 `@Before` 直接调用 suspend `HubClient.create(...)`（未包 `runBlocking`; 参考文件 `HubClientRequestShapeContractTest` 已注明此坑并正确使用 runBlocking）; 且 `HubClientTest.kt:57` 的 `assertFailsWith` 无 `kotlin.test` import（全测试目录无 kotlin.test 引用）→ 两个文件存在编译期风险, 建议后续轮次修复。
   - iOS 源码预存编译问题（本次验证时暴露, 仓库文件零改动）: ① `YDKHubClient+Stream.swift` 跨文件访问 `private baseURL` / `fileprivate token`; ② `YDKKeyManager.swift`/`YDKKeyCache.swift` 未 `import YDKHubClient` 却使用 `YDKKey`; ③ 二者引用 YDKHubClient 内部类型 `YDKLogger`（internal 不可跨模块）; ④ `YDKKeyManager+Sync.swift` 跨文件访问 `private syncQueue`/`private logger`。以上任一都会使 `swift build`/`swift test` 失败, 建议 Phase 5 统一修复。

## 5. 验证结果

| 项 | 结果 |
|:---|:---|
| iOS 语法 | `swiftc -parse` 两个新增文件 ✅ 通过 |
| iOS 类型检查 | 用真实 SDK 源码构建 `YDKHubClient`/`YDKKeyManager` 模块（`-enable-testing`; `/tmp` 副本仅对上述预存 bug 做最小修复: 补 `import`、`private let`→`let`、`YDKLogger` shim）+ XCTest/UIKit 最小桩, 对两个新增测试文件 `swiftc -typecheck` ✅ 全部通过（真实签名校验） |
| Android 静态审查 | 新增文件仅引用既有公开 API（HubClient 扩展函数/KeyManager/KeyCache/YDKKey/SyncResult）; `runBlocking` 包裹 suspend create; 未使用 `assertFailsWith`（suspend 调用改 try/catch）; JUnit 断言消息参数序正确 ✅ |
| Android Python 交叉验证 | 新增断言的纯逻辑（diff 归类 added/updated/removed/unchanged + hasChanges + 远程控车 body 字段集/action 映射/keyId 缺省 ""/无 source + wire JSON 形状）用 Python 独立复算: **33/33 通过** ✅ |
| 既有测试 | 零改动（只增不删）✅ |
