# TASK_STATUS — 量产就绪待办清单

> 关键量产代办事项，持续更新。
> 当前阶段：Relay Server Phase B — 分享集成

---

## 当前阶段：Phase 2c — KeyManager 钥匙缓存与同步

| # | 任务 | 状态 | 备注 |
|:-:|:-----|:----:|:-----|
| 2c-1 | iOS KeyCache (JSON 文件缓存) | ✅ | |
| 2c-2 | iOS KeyManager (同步 + 差异检测 + Delegate) | ✅ | |
| 2c-3 | iOS 自动定时同步 + Push 触发 | ✅ | |
| 2c-4 | Android KeyCache (JSON 文件缓存) | ✅ | |
| 2c-5 | Android KeyManager (同步 + 差异检测 + StateFlow) | ✅ | |
| 2c-6 | Android 自动定时同步 + Push 触发 | ✅ | |


## Phase A（已完成）

- ✅ Proto 定义 + gRPC 生成
- ✅ Mailbox 状态机 + CRUD
- ✅ TTL 过期 GC
- ✅ Service 层（gRPC handlers）
- ✅ 单元测试（7/7 绿）
- ✅ Relay Server 分享集成（Phase B）
- ✅ Push 通知服务集成（FCM/APNs）
- ✅ Phase 2a: HubClient (HTTP/JSON → REST Gateway)
- ✅ Phase 2c: KeyManager (本地缓存 + 同步 + Push触发)

---

## 量产就绪待办

> 这些是生产环境上线前必须完成的关键事项。

### P0 — 核心路径，阻塞上线

| # | 事项 | 状态 | 计划 |
|:-:|:-----|:----:|:----:|
| 🔴 | **Push 通知服务集成**（FCM/APNs）| ✅ **已完成** | 接口 + Mock/测试 + FCM/APNs 实现，环境变量配置 |
| 🔴 | Apple 开发者证书签名（macOS 桌面端）| 📋 | 需 `MAC_CSC_LINK` + `MAC_CSC_KEY_PASSWORD` |

### P1 — 重要功能

| # | 事项 | 状态 | 计划 |
|:-:|:-----|:----:|:----:|
| 🟠 | 多厂商并发 E2E 测试（CCC↔ICCOA↔ICCE）| 📋 | 含 Relay Mailbox 跨厂商流转 |
| 🟠 | 认证文档更新（PICS/PIXIT — Relay Server 部分）| 📋 | 覆盖 CCC §11.3.4 |
| 🟠 | 依赖安全漏洞清零 | 📋 | 持续跟踪 |

### P2 — 增强功能

| # | 事项 | 状态 |
|:-:|:-----|:----:|
| 🟡 | 离线授权回退机制 | 📋 |
| 🟡 | 插件 SDK 文档（面向第三方开发者）| 📋 |
| 🟡 | 性能测试（大配置加载/保存）| 📋 |

---

> **状态图例：** ✅ 完成 · 🔜 进行中 · 📋 待排期 · ⏸ 暂停 · ❌ 阻塞
