# TASK_STATUS — 量产就绪待办清单

> 关键量产代办事项，持续更新。
> 当前阶段：P1 — TLS + K8s 部署编排（已完成，待证书签发）

---

## 当前阶段：P1-2 — 用户体系对接（已完成）+ SDK↔Hub 打通验证（已完成）

| # | 任务 | 状态 | 备注 |
|:-:|:-----|:----:|:-----|
| P1-2 | 用户体系对接（车厂后端鉴权） | ✅ | 双轨令牌: admin HS256 (iss=dkcs-admin) + OEM RS256/ES256 (JWKS, iss=<oem>); fail-closed 管理员; 修复 main.go 缺 WithJWTSecret 的 DOA bug; 1365 测试通过 |
| SDK-1 | SDK↔Hub 全链路打通（E2E 实测） | ✅ | login→bindKey→getKey→listKeys→mailbox 全通; 修复: ① main.go 缺 WithGRPCConn (管理 API 全 503) ② SDK 枚举数字字符串→枚举名 (iOS+Android) ③ GetKey/ListKeys service 空壳返回空 |

---

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
- ✅ Phase 2d: MailboxClient (CCC 分享 HTTP 客户端 + Backend REST 路由)
- ✅ 量产 P1: 多厂商 E2E 测试 + PICS/PIXIT 认证文档 + 依赖检查
- ✅ Phase 2b: BLEProtocol (协议层 + iOS/Android BLE + UWB/NFC 抽象)
- ✅ P0: PostgreSQL 持久化存储（KeyStore + MailboxStore 落盘）
- ✅ P0: SDK DeviceManager（iOS SE / Android Keystore + vendor/protocol 检测 + bindKey/acceptShare 自动填充）
- ✅ P1-1: TLS + K8s 部署编排（Gateway HTTPS + postgres StatefulSet + kustomize base/overlay 拆分）
- ✅ P1-2: 用户体系对接（JWKS 双轨令牌 + fail-closed 管理员 + DOA bug 修复）
- ✅ SDK↔Hub 打通: 枚举名契约 + GetKey/ListKeys 实现 + gRPC 转发接线（E2E 实测全通）

---

## 量产就绪待办

> 这些是生产环境上线前必须完成的关键事项。

### P0 — 核心路径，阻塞上线

| # | 事项 | 状态 | 计划 |
|:-:|:-----|:----:|:----:|
| 🔴 | **Push 通知服务集成**（FCM/APNs）| ✅ **已完成** | 接口 + Mock/测试 + FCM/APNs 实现，环境变量配置 |
| 🔴 | Apple 开发者证书签名（macOS 桌面端）| 📋 | 需 `MAC_CSC_LINK` + `MAC_CSC_KEY_PASSWORD` |
| 🔴 | **TLS 证书签发** | 🔜 | K8s 已支持（hub-tls secret，optional），生产需 cert-manager 或托管证书；证书未就绪时服务回退 HTTP 并打 WARN |

### P1 — 重要功能

| # | 事项 | 状态 | 计划 |
|:-:|:-----|:----:|:----:|
| 🟠 | **多厂商并发 E2E 测试（CCC↔ICCOA↔ICCE）** | ✅ **E2E-14 已创建** | 含 Relay Mailbox 跨厂商流转: Apple→Xiaomi + Samsung→Huawei |
| 🟠 | **认证文档更新（PICS/PIXIT — Relay Server 部分）** | ✅ `docs/compliance/PICS_PIXIT_RELAY.md` | 覆盖 CCC §11.3.4 全部 6 个 RPC + 邮箱状态机 + Push |
| 🟠 | **依赖安全漏洞清零** | ✅ `go mod tidy` + `go vet` 通过 | 需安装 `govulncheck` 做 CVE 深度扫描 |
| 🟠 | **dkcs 服务 PG schema 迁移** | 📋 | dkcs 用 lib/pq 但无自带迁移；旧 `db/schema.sql` 是 MySQL 方言，需转 PG 方言并纳入迁移 |
| 🟠 | **Helm Chart 同步 postgres** | 📋 | `helm/dkcs` 仍引用 mysql-statefulset + mysql-password；现行部署走 kustomize，helm 需跟进或标记废弃 |

### P2 — 增强功能

| # | 事项 | 状态 |
|:-:|:-----|:----:|
| 🟡 | 离线授权回退机制 | 📋 |
| 🟡 | 插件 SDK 文档（面向第三方开发者）| 📋 |
| 🟡 | 性能测试（大配置加载/保存）| 📋 |
| 🟡 | postgres-exporter 部署 | 📋 | Prometheus 采集已指向 `postgres-exporter:9187`，需部署 exporter（可用 postgres_exporter sidecar 或独立 Deployment） |
| 🟡 | JWKS kid 未命中防放大 | 📋 | 恶意令牌随机 kid 会触发重复拉取；建议 kid miss 负缓存或 30s 冷却后再刷新（评审 MINOR #4） |
| 🟡 | tests/integration 预存 vet 错误 | 📋 | `scenarios/e2e_14_cross_vendor_mailbox_share_test.go` relay API 签名漂移（与本次改动无关，预存） |

---

> **状态图例：** ✅ 完成 · 🔜 进行中 · 📋 待排期 · ⏸ 暂停 · ❌ 阻塞
