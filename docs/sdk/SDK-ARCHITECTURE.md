# yuleDKCS Mobile SDK 架构文档

> **版本**: v1.0  
> **更新日期**: 2026-07-30  
> **适用**: iOS / Android App 集成  
> **参考**: hub.proto、relay.proto、CCC-TS-101 v4.0、ICCOA DK 4.0、ICCE

---

## 1. 架构总览

### 1.1 三层模型

```
┌──────────────────────────────────────────────────────────────────┐
│  Layer 1: 车厂 App（车厂自己写）                                  │
│                                                                  │
│  ┌──────────────────────┐  ┌────────────────────────────────┐  │
│  │ 用户层                │  │ 钱包层（SDK管不了）             │  │
│  │ - 用户登录/注册       │  │ - Apple CarKey API (iOS)       │  │
│  │ - session token 管理  │  │ - Samsung Wallet SDK           │  │
│  │ - 车辆状态界面        │  │ - 小米/OPPO/vivo 钱包 SDK      │  │
│  │ - 分享/钥匙管理 UI    │  │ - 华为钱包 SDK (ICCE)          │  │
│  │ - 远程控车按钮        │  │ - Push 通知处理 (FCM/APNs)     │  │
│  └──────────────────────┘  └────────────────────────────────┘  │
│                         ↕                                        │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Layer 2: yuleDKCS SDK（我们写）                           │  │
│  │                                                          │  │
│  │  ┌────────────────┐ ┌────────────┐ ┌───────────────┐   │  │
│  │  │ HubClient      │ │ BLEProtocol│ │ KeyManager    │   │  │
│  │  │ (gRPC→Hub)     │ │ (BLE/UWB)  │ │ (状态管理)     │   │  │
│  │  └────────────────┘ └────────────┘ └───────────────┘   │  │
│  └──────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### 1.2 职责边界

| 层 | 职责 | 谁写 |
|:---|:-----|:-----|
| **Layer 1: 用户层** | 登录、UI、session token 管理 | 车厂 |
| **Layer 1: 钱包层** | 调用 Apple/Samsung/小米/华为钱包 API | 车厂（各平台 SDK 不同）|
| **Layer 2: yuleDKCS SDK** | Hub 通信、BLE/UWB 协议、钥匙状态管理 | **我们** |

## 2. SDK 职责

### 2.1 SDK 做的事

```
yuleDKCS SDK 提供：
├── HubClient（gRPC）
│   ├── BindKey(token, vehicleId)           → 绑定钥匙到手机 SE
│   ├── UnbindKey(keyId)                    → 解绑钥匙
│   ├── CreateShare(keyId, toUser)          → 创建分享
│   ├── AcceptShare(shareCode)              → 接受分享
│   ├── CancelShare(shareId)                → 取消分享
│   ├── GetKey(keyId) / ListKeys()          → 查询钥匙
│   ├── RemoteLock(vehicleId)               → 远程锁车
│   ├── RemoteUnlock(vehicleId)             → 远程解锁
│   ├── RemoteStart(vehicleId)              → 远程启动
│   └── SyncKeyStatus()                     → 同步钥匙状态
│
├── BLEProtocol（BLE/UWB 通信协议栈）
│   ├── ScanForVehicles()                   → 扫描周围车辆
│   ├── ConnectToVehicle(vehicleId)          → 连接指定车辆
│   ├── Unlock(vehicleId)                   → 靠近解锁
│   ├── Lock(vehicleId)                     → 靠近锁车
│   ├── StartEngine(vehicleId)              → 启动引擎
│   ├── ReadVehicleStatus(vehicleId)        → 读取车辆状态
│   └── 全部使用手机 SE 中的钥匙进行端到端加密
│
├── KeyManager（钥匙生命周期管理）
│   ├── 本地缓存钥匙列表 + 状态
│   ├── 定期向 Hub 同步状态差异
│   ├── TTL 过期自动标记
│   └── Push 通知回调处理（钥匙被撤销/挂起）
│
└── MailboxClient（CCC 协议专用）
    ├── ReadDisplayInfo(url)                → 读取分享邮箱展示信息
    ├── UpdateMailbox(mailboxId, payload)   → 更新邮箱内容
    ├── DeleteMailbox(mailboxId)            → 删除邮箱
    └── 仅在 CCC 协议分享时由 SDK 内部使用
```

### 2.2 SDK 不做的事

```
✗ 用户登录/注册
✗ session token 的获取（由车厂 App 从车厂 Server 获取后传入 SDK）
✗ UI 界面（按钮、列表、弹窗）
✗ 调用手机钱包 API（Apple CarKey / Samsung Wallet 等）
✗ Push 通知的接收（由系统 Push 直达 App，SDK 处理回调）
```

## 3. 通信架构

### 3.1 协议矩阵

```
┌──────────────┬──────────────────┬───────────────────┬──────────────┐
│  场景         │ 协议              │ 端点               │ SDK 接口      │
├──────────────┼──────────────────┼───────────────────┼──────────────┤
│ 绑钥匙        │ gRPC → Hub       │ Hub :9090         │ BindKey()    │
│ 解绑/撤销     │ gRPC → Hub       │ Hub :9090         │ UnbindKey()  │
│ 分享(Create) │ gRPC → Hub       │ Hub :9090         │ CreateShare()│
│ 分享(Accept) │ gRPC → Hub       │ Hub :9090         │ AcceptShare()│
│ 远程控车      │ gRPC → Hub       │ Hub :9090         │ RemoteXxx()  │
│ BLE 解锁      │ BLE (本地)        │ 直连 TCU          │ Unlock()     │
│ CCC 邮箱读取  │ gRPC → Relay Svr  │ Mailbox URL       │ MailboxClient│
│ 状态同步      │ gRPC → Hub       │ Hub :9090         │ SyncKeys()   │
└──────────────┴──────────────────┴───────────────────┴──────────────┘
```

### 3.2 数据流

**绑钥匙流程（完整）：**
```
App User           yuleDKCS SDK           Hub              车厂Server         车辆TCU
  │                    │                    │                    │              │
  │  login()           │                    │                    │              │
  │───────────────────→│                    │                    │              │
  │  token              │                    │                    │              │
  │←───────────────────│                    │                    │              │
  │                    │                    │                    │              │
  │ bindKey(token, VIN)│                    │                    │              │
  │───────────────────→│                    │                    │              │
  │                    │ BindKey(请求)       │                    │              │
  │                    │───────────────────→│                    │              │
  │                    │                    │ → 验证token(问车厂) │              │
  │                    │                    │←─── 确认用户有权限 ─→              │
  │                    │                    │                    │              │
  │                    │                    │ → 调 adapter       │              │
  │                    │                    │    (CCC/ICCOA/ICCE)│              │
  │                    │                    │                    │              │
  │                    │ ←─────────────────│ 车端公钥+共享密钥    │              │
  │                    │                    │                    │              │
  │ ←── 车端公钥 ──│                    │                    │              │
  │                    │                    │                    │              │
  │ [ECDH 协商]        │                    │                    │              │
  │ [写入 SE]          │                    │                    │              │
  │ [添加至钱包]       │                    │                    │              │
  │                    │                    │                    │              │
```

**注意**: 绑钥匙成功后，App 需要额外调用 **钱包层 API** 将钥匙添加到 Apple Wallet / Samsung Wallet。这一步 SDK 做不了。

**日常用车流程（靠近解锁）：**
```
手机 SE (钥匙)  ←── BLE/UWB/NFC ──→ 车辆 TCU (验证)
     ↑ 端到端加密                          ↑
     不走任何云服务器                      TCU 本地验证
```

**分享流程（CCC 协议）：**
```
发送方手机               yuleDKCS SDK            Hub           Relay Server   接收方手机
  │─CreateShare()→│                    │                    │              │
  │               │─CreateShare──────→│                    │              │
  │               │                   │──CreateMailbox───→│              │
  │               │                   │←Sharing URL──────│              │
  │               │←── shareCode ────│                    │              │
  │←ShareCode────│                    │                    │              │
  │               │                    │                    │              │
  │[发送分享码给好友]│                    │                    │              │
  │               │                    │                    │              │
  │               │                    │                    │ 好友手机     │
  │               │                    │                    │←AcceptShare │
  │               │                    │←─AcceptShare───────│         │
  │               │                    │                    │              │
  │               │                    │ ←签名 → 调 S2S     │              │
  │               │                    │                    │              │
  │               │                    │──UpdateMailbox───→│              │
  │               │                    │──DeleteMailbox───→│              │
```

## 4. 安全架构

### 4.1 密钥分层

```
┌────────────────────────────────────────────┐
│  App Sandbox                                │
│                                             │
│  ┌─────────────────────────────────┐        │
│  │  yuleDKCS SDK                   │        │
│  │  - Hub gRPC 连接 (TLS)          │        │
│  │  - BLE 协议栈                   │        │
│  │  - 钥匙元数据缓存               │        │
│  └─────────────────────────────────┘        │
│                                             │
│  ┌─────────────────────────────────┐        │
│  │  手机 Secure Element (SE)       │        │
│  │  - 数字钥匙私钥 (不可导出)       │        │
│  │  - CCC/ICCOA/ICCE 凭证          │        │
│  │  - Apple CarKey / Samsung Wallet│        │
│  └─────────────────────────────────┘        │
└────────────────────────────────────────────┘
```

### 4.2 安全要点

- **钥匙私钥永不离开 SE** — SDK 只能通过操作系统 API 使用钥匙，不能读取
- **Hub 通信使用 TLS** — gRPC 内置传输层加密
- **Mailbox 零知识** — Secret 在 URL fragment，SDK 负责追加，不发送到 Relay Server
- **Session token 由车厂 App 管理** — SDK 只使用、不存储 token

## 5. 平台差异

| 能力 | iOS | Android |
|:-----|:---:|:-------:|
| 钥匙存储 | Secure Enclave + Apple Wallet | StrongBox + 各厂商钱包 |
| BLE 后台扫描 | ✅ 支持 | ✅ 支持 |
| UWB 测距 | ✅ (U1/U2 芯片) | ⚠️ (部分厂商支持) |
| NFC 备用 | ✅ Apple Pay 通道 | ✅ 各厂商钱包 |
| gRPC 客户端 | ✅ Swift gRPC | ✅ gRPC-Android |
| 后台 Push | ✅ APNs | ✅ FCM |

## 6. SDK 接口分类

### 6.1 生命周期接口（App 必须调用）

```
// App 启动时
SDK.initialize(config)           → 初始化 SDK
SDK.setToken(sessionToken)       → 设置用户 token（从车厂Server 获取后传入）

// App 退出时
SDK.destroy()                    → 清理资源
```

### 6.2 核心业务接口

```
// 钥匙管理
hub.bindKey(vehicleId)           → 绑定钥匙
hub.unbindKey(keyId)             → 解绑钥匙
hub.listKeys()                   → 钥匙列表
hub.getKey(keyId)                → 钥匙详情

// 分享
hub.createShare(keyId, opts)     → 创建分享
hub.acceptShare(shareCode)       → 接受分享
hub.cancelShare(shareId)         → 取消分享

// 远程控车
hub.remoteLock(vehicleId)        → 远程锁车
hub.remoteUnlock(vehicleId)      → 远程解锁
hub.remoteStart(vehicleId)       → 远程启动

// BLE 本地控制
ble.scan()                       → 扫描周围车辆
ble.connect(vehicleId)           → 连接
ble.unlock()                     → 靠近解锁
ble.lock()                       → 靠近锁车
ble.getStatus()                  → 读取车辆状态
```

### 6.3 回调接口

```
// Push 通知 → App 接收
onKeyRevoked(keyId, reason)      → 钥匙被撤销
onKeySuspended(keyId)            → 钥匙被挂起
onShareReceived(shareInfo)       → 收到分享
onVehicleAlert(alertInfo)        → 车辆告警
```

## 7. Reliability & Graceful Degradation

- **离线模式**: BLE 解锁不依赖网络，钥匙在 SE 中可离线使用
- **远程控车断网**: SDK 返回错误，App 显示友好提示
- **Hub 不可达**: SDK 使用本地缓存钥匙列表，不影响 BLE 解锁
- **Push 未到达**: SDK 的定时同步机制兜底（与 Hub 的轮询策略一致）

## 8. 相关文档

- [Hub API Proto](../api/v1/hub.proto) — Hub gRPC 服务定义
- [Relay Server Proto](../api/relay/v1/relay.proto) — CCC Mailbox API
- [SDK Proto](../api/sdk/v1/sdk.proto) — SDK gRPC 接口定义
- [Hub 架构图](../docs/architecture/hub-architecture.svg.html)
