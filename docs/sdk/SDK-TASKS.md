# yuleDKCS Mobile SDK + 移动端 开发任务分解

> **版本**: v1.0  
> **更新日期**: 2026-07-30  
> **文档**: SDK-ARCHITECTURE.md、api/sdk/v1/sdk.proto

---

## Phase 1: SDK 接口定型（当前阶段）

| # | 任务 | 文件 | 工时 | 状态 |
|:-:|:-----|:----|:----:|:----:|
| 1.1 | SDK 架构文档 | `docs/sdk/SDK-ARCHITECTURE.md` | ✅ 完成 | ✅ |
| 1.2 | SDK proto 接口定义 | `api/sdk/v1/sdk.proto` | ✅ 完成 | ✅ |
| 1.3 | Hub proto Review — SDK 需要的 Hub RPC 是否齐全 | `api/v1/hub.proto` | 小半天 | 🔜 |

**1.3 的检查项**:
- [ ] SDK `BindKey` 需要 Hub 支持 → ✅ 已有 `BindKey` RPC
- [ ] SDK `ListKeys` 需要 Hub 支持 → ✅ 已有 `ListKeys` RPC
- [ ] SDK `RemoteLock/Unlock` 需要 Hub 支持 → ✅ 已有 `SendCommand`
- [ ] SDK `HandlePushNotification` 需要 Hub deliver Push payload → ⚠️ 需确认
- [ ] SDK → MailboxClient Relay Server 的 CCC 6 个 API → ✅ relay.proto 已全

---

## Phase 2: SDK 核心实现（Native Library）

SDK 以原生库形式嵌入车厂 App，**不是一个独立服务**。

### 2a: HubClient 实现

| # | 任务 | 说明 | 工时 |
|:-:|:-----|:-----|:----:|
| 2.1 | 从 proto 生成 iOS Swift gRPC stub | `protoc` → Swift 代码 | 0.5天 |
| 2.2 | 从 proto 生成 Android Java gRPC stub | `protoc` → Java 代码 | 0.5天 |
| 2.3 | HubClient 初始化模块（gRPC 连接管理、TLS、reconnect） | iOS + Android | 1天 |
| 2.4 | Session token 注入（App 传入，SDK 附加到每个 gRPC metadata） | iOS + Android | 0.5天 |
| 2.5 | BindKey / UnbindKey 实现（调 Hub gRPC） | iOS + Android | 1天 |
| 2.6 | ListKeys / GetKey 实现 | iOS + Android | 0.5天 |
| 2.7 | CreateShare / AcceptShare / CancelShare 实现 | iOS + Android | 1天 |
| 2.8 | RemoteLock / RemoteUnlock / RemoteStart 实现 | iOS + Android | 1天 |
| 2.9 | **Push 回调处理** — App 收到 Push 后传给 SDK，SDK 解析并触发内部状态更新 | iOS + Android | 1天 |

### 2b: BLEProtocol 实现

这是最复杂的模块。需要实现 BLE/UWB 通信协议栈。

**现状核对（2026-07-31）**: 骨架完整（iOS 903 行 + Android 649 行），但实现大量为占位——
广告解析占位（`CCCBleAdapter.swift:29,43`）、扫描占位（`YDKBLEManager.swift:120`）、
CCC 指令加密占位（`CCCBleAdapter.swift:66`）、SM4 加密占位（`ICCOABleAdapter.swift:51`、
Android `BleProtocolAdapter.kt:74`）、UWB/NFC 为 60 行级骨架。

**拆解表（W1=iOS worker / W2=Android worker / 独立=硬件项）**:

| # | 任务 | 现状证据 | 完成标准 | Worker |
|:-:|:-----|:-----|:-----|:---:|
| 2b-A | 广告包解析 iOS | `CCCBleAdapter.swift:29,43` 占位 | 真实解析 manufacturer data → vehicleId/协议；构造字节单测 | W1 |
| 2b-B | 广告包解析 Android | `BleProtocolAdapter.kt` 占位 | 同上（Kotlin）| W2 |
| 2b-C | BLE 扫描 iOS | `YDKBLEManager.swift:120` 占位 | CoreBluetooth 扫描/过滤 + mock 测试 | W1 |
| 2b-D | BLE 扫描 Android | `BleManager.kt` | BluetoothLeScanner + mock 测试 | W2 |
| 2b-E | **CCC 指令帧 + 加密签名** | `CCCSecureChannel.swift/kt` + `CccFrame.kt` + `CCCCommandFrame.swift` | ✅ **完成 (2026-07-31)**: 规范 v4.0.0 原文裁决 (AES-128+CMAC-AES-128/SCP03), 见 `docs/certification/ccc-ts101-ble-secure-channel.md`; iOS 16/16 测试通过, Android 测试就位 | W1 |
| 2b-F | ICCOA/ICCE 指令帧 SM4 | `ICCOABleAdapter.swift`、`BleProtocolAdapter.kt` | ✅ **完成 (2026-08-01)**: 以车端参考实现裁决 (dk30.c/iccoa_digital_key.h/module_design.md), 见 `docs/certification/iccoa-icce-ble-command-frames.md`; 关键裁决: ICCOA 应用层无 SM4 (链路层 LE SC 加密), CTRL payload=[cmd][param], 帧 SEQ/LEN 小端 + checksum 不含 SOP, 枚举映射 (unlock→0x02 防锁/解颠倒), ICCE HMAC-SHA256 真实化 + SM4-CBC; iOS 42/42 断言 + 类型检查, Android 测试更新 CI 执行 | W2 |
| 2b-G | UWB 测距 | iOS/Android 60 行骨架 | ⚠️ 真机依赖: 代码+接口+模拟测，真机联调单列 | 独立 |
| 2b-H | NFC 备用解锁 | iOS/Android 34 行骨架 | ⚠️ 真机依赖: 同上 | 独立 |
| 2b-I | 后台 BLE | 未开始 | iOS background mode + Android foreground service | W1/W2 尾 |

**防幻觉原则**:
1. 协议指令真实化（2b-E/F）必须先研读规范/参考实现再写，禁止凭印象编帧格式
2. 硬件项（2b-G/H）无真机不许标 ✅——完成标准只到"代码 + 模拟测试"，真机联调单独列项
3. 任务间文件隔离（iOS/Android 目录分离），并行 worker 零冲突
4. 每个任务完成标准 = 编译/语法检查通过 + 单测代码就位（本环境无完整移动端工具链时，测试文件写出、由 CI/真机构建执行）

### 2c: KeyManager

| # | 任务 | 说明 | 工时 |
|:-:|:-----|:-----|:----:|
| 2.16 | 本地钥匙缓存（SQLite / Realm） | iOS + Android | 0.5天 |
| 2.17 | 钥匙状态同步（定时调 Hub `ListKeys` 对比差异） | iOS + Android | 1天 |
| 2.18 | 离线缓存 + 状态推断（无网时用本地数据） | iOS + Android | 0.5天 |
| 2.19 | Push 触发增量同步 | iOS + Android | 0.5天 |

### 2d: MailboxClient

| # | 任务 | 说明 | 工时 |
|:-:|:-----|:-----|:----:|
| 2.20 | Mailbox gRPC 客户端（CCC 分享用） | iOS + Android | 1天 |
| 2.21 | Secret fragment 安全处理（不泄露到日志/URL 请求） | iOS + Android | 0.5天 |

**Phase 2 总计: ~17 天**（iOS + Android 并行，可压缩到 ~10 日历天）

---

## Phase 3: 移动 App 集成（车厂做）

| # | 任务 | 说明 | 依赖 |
|:-:|:-----|:-----|:----:|
| 3.1 | 车厂 App 集成 yuleDKCS SDK | pod / SPM / gradle | Phase 2 |
| 3.2 | 用户登录 → 车厂 Server | 车厂自己的后端 | 无 |
| 3.3 | 将 token 传入 SDK | `SDK.setToken()` | 3.2 |
| 3.4 | 钱包层集成 | | |
| 3.4a | iOS: Apple CarKey API (`PKPassLibrary.addCarKeyPass`) | Apple Developer 文档 | Phase 2.5 返回 walletKeyData |
| 3.4b | Samsung: Samsung Wallet SDK 集成 | Samsung 合作伙伴协议 | Phase 2.5 |
| 3.4c | 小米: 小米钱包 SDK 集成 | 小米开放平台 | Phase 2.5 |
| 3.4d | 华为: 华为钱包 SDK 集成 | 华为 HMS | Phase 2.5 |
| 3.5 | BLE 权限声明 (Info.plist / AndroidManifest.xml) | 系统配置 | 无 |
| 3.6 | Push Notification 权限 | APNs / FCM 配置 | 无 |
| 3.7 | UICC BLE 解锁界面 | App UI 开发 | Phase 2.10-2.12 |
| 3.8 | 远程控车界面 | App UI 开发 | Phase 2.8 |
| 3.9 | 分享管理界面（我的钥匙 → 分享 → 好友接受） | App UI 开发 | Phase 2.7 |

---

## Phase 4: 集成测试

| # | 任务 | 说明 | 依赖 | 并行性 |
|:-:|:-----|:-----|:----:|:----:|
| 4.1 | SDK 单元测试（Mock Hub、Mock TCU） | iOS XCTest + Android JUnit | 无 | ✅ 可与 2b 并行 |
| 4.2 | SDK × Hub 集成测试（真实 gRPC 调用） | 复用现有 E2E 测试框架；REST 层已实测打通（2026-07-31） | 无 | ✅ 可与 2b 并行 |
| 4.3 | BLE 桩测试（模拟车辆广播） | iOS + Android 模拟器 | 2b-A/B/C/D | ✅ **桩就位 (2026-07-31)**: iOS FakeCentral/FakePeripheral + Android FakeBleScanEngine; 存量测试同步修复; iOS wire 级独立验证 9/9 + 加密 16/16; Android 测试 CI 执行; 真机联调单列 |
| 4.4 | CCC 分享全链路 E2E（两台手机 ↔ Hub ↔ Relay） | 物理机测试 | 分享链路: 无；BLE 解锁: 2b | ✅ **分享链路就位 (2026-07-31)**: 双端高层编排层 (ShareFlow: Sender/Receiver/取消流) + URL 生成/解析; iOS 核心逻辑 7/7 独立验证 + 类型检查; Android 16 wire 级用例就位; 物理机 E2E 单列 |
| 4.5 | ICCOA/ICCE S2S 分享全链路 E2E | 通过 Hub mock S2S | 无 | ✅ 可与 2b 并行 |

---

## 优先级建议

```
Week 1-2:  Phase 2a (HubClient) — 最核心，决定 App 能否调通云端
Week 3-4:  Phase 2c (KeyManager) + Phase 2d (MailboxClient)
           — 钥匙状态管理和 CCC 分享
Week 5-7:  Phase 2b (BLEProtocol) — 最复杂，但不影响云端功能测试
Week 8+:   Phase 3 (App 集成) + Phase 4 (集成测试)
           — 需要车厂配合
```

**建议先完成 Phase 2a**（Hub gRPC 客户端），这样就能从手机端调用 Hub 的绑钥匙/分享/远程控车功能。BLE 部分可以和 App UI 开发并行。

---

## 依赖关系图

```
Phase 1 (proto 定型)
    ↓
Phase 2a (HubClient gRPC) ─────→ Phase 3.1 (App 集成 SDK)
    ↓                                  ↓
Phase 2c (KeyManager)            Phase 3.4 (钱包层集成)
    ↓
Phase 2d (MailboxClient)
    ↓
Phase 2b (BLEProtocol) ─────────→ Phase 3.7 (BLE 解锁 UI)
    ↓                                  ↓
Phase 4 (E2E 测试) ←───────────── 全部
```
