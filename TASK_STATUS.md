# TASK_STATUS — 量产就绪待办清单

> 关键量产代办事项，持续更新。
> 当前阶段：量产就绪收尾 — Loop C 杂项收尾（审计文档更新 / kustomize+helm 渲染 smoke / TASK_STATUS 整理）已完成；剩余仅外部依赖（TLS 证书签发、Apple 证书签名）

---

## P1-2 里程碑（已完成）

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

## Loop B/C 收尾（已完成, 2026-08-01）

| # | 事项 | 状态 | 备注 |
|:-:|:-----|:----:|:-----|
| B-1 | 离线授权回退机制（P2 语义调研 + 双端 OfflineAuthorizer） | ✅ | 见 P2 表格; `docs/sdk/OFFLINE-FALLBACK-DESIGN.md` |
| B-2 | postgres-exporter 部署（kustomize + helm 同步） | ✅ | 见 P2 表格; `deploy/k8s/README.md` |
| B-3 | iOS wire 断言补齐（transport 注入缝 + WireShapeContractTests 5 形状） | ✅ | 见 P1 iOS 编译错误项; `swiftc -parse` 通过 |
| C-1 | TEST-COVERAGE-AUDIT 过时标注更新（第 74 行受限/待补 → Batch B 已补齐 ✅） | ✅ | `docs/sdk/TEST-COVERAGE-AUDIT.md` 表 2.1 + 结论 3 + 清单 + 验证表 |
| C-2 | kustomize 全量渲染 smoke（base + staging overlay） | ✅ | `kubectl kustomize` 双 exit 0, 各 33 文档 YAML 结构合法（仅 commonLabels deprecation warning） |
| C-3 | helm 静态校验（18 模板 `{{}}` 平衡 + 119 个 `.Values.*` 引用全覆盖） | ✅ | 2 个 default 保护可选覆盖（fullnameOverride/nameOverride）属 Helm 惯例 |

---

## 量产就绪待办

> 这些是生产环境上线前必须完成的关键事项。

### P0 — 核心路径，阻塞上线

| # | 事项 | 状态 | 计划 |
|:-:|:-----|:----:|:----:|
| 🔴 | **Push 通知服务集成**（FCM/APNs）| ✅ **已完成** | 接口 + Mock/测试 + FCM/APNs 实现，环境变量配置 |
| 🔴 | Apple 开发者证书签名（macOS 桌面端）| 📋 | ⚠️ **属 yuleASR-Configurator 项目错放项**（已补记到 yuleASR TASK_STATUS）; yuleDKCS 侧移除 |
| 🔴 | **TLS 证书签发** | 🔜 | K8s 已支持（hub-tls secret，optional），生产需 cert-manager 或托管证书；证书未就绪时服务回退 HTTP 并打 WARN |

### P1 — 重要功能

| # | 事项 | 状态 | 计划 |
|:-:|:-----|:----:|:----:|
| 🟠 | **iOS 模块预存编译错误 4 处** | ✅ **已修复 (Batch A, W3 复验)** | 4.1 审计 4 处已全修（①YDKHubClient+Stream 跨文件访问 ②YDKKeyManager/YDKKeyCache 缺 import YDKHubClient ③internal YDKLogger 引用 ④YDKKeyManager+Sync 跨文件 private）; Batch A (c18e5fb) `swift build` 全绿; Batch B 补 tail: YDKHubClient transport 注入缝 + WireShapeContractTests（listKeys/getKey/unbindKey/cancelShare 5 形状）+ 修复 W1 遗留嵌套插值语法错（YDKKeyManager.swift:216）; **W3 复验 (2026-08-01): HEAD `swift build` 全绿 + WireShapeContractTests `swiftc -parse` 通过** |
| 🟠 | **MISRA C:2012 CI gate 修复** | ✅ **已完成 (2026-08-05)** | gate 从合入起就是哑的，3 层根因全修: ① baseline 前缀 c2023→c2012（cppcheck addon 实际输出 misra-c2012-*）② 11 条 rule- 前缀格式错（`misra-c2012-rule-8-4`→`misra-c2012-8.4`）suppression 从未生效 ③ 共享 baseline 跨模块 unmatchedSuppression 触发 error-exitcode=1（CI 加 `--suppress=unmatchedSuppression`）; 补 97 条遗漏 baseline; **4 模块全绿**（icce/ccc/iccoa/unified EXIT=0, 0 violations）; 附带修 icce_edge.c `now=0` 硬编码（cooldown 永远被跳过）→ `sys_tick_get_ms()` |
| 🟠 | **yuleOSH 工具链打通（MISRA C:2023 全量 0 违规 + 三层 CI 全绿）** | ✅ **已完成 (2026-08-05)** | yuleOSH-check v3.12.1 (5c4721a): mixed 语言支持 (scan_dirs/include_paths/project_language)、cppcheck 相对路径 -I + exclude normpath + scan_dirs 文件发现、parser 跳过 information 级、pytest exit 5→skip; yuleDKCS (1fcf22a): baseline +331 条 + misra-rules.yaml + ci-config.yaml; **yuleosh ci run 1/2/3 全绿** — MISRA 0 违规 (57 文件) + go build/vet/test 7 模块 + scenarios 32 过; 集成测试加环境探测 skip (无 carsim/网关时 SKIP 不 block CI); 产物治理 (carsim 二进制/store.db/*.dump gitignore) |
| 🟠 | **Java 适配器层构建修复** | ✅ **已完成 (2026-08-05)** | 5 大问题: ① executeWithRetry 嵌套 Future（adapter-core 编译失败根因）→ Callable<CompletableFuture<T>> + thenCompose ② Mockito 无法 mock JDK 26 → mockito 5.20.0 + mock-maker-subclass（4 模块）③ getAdapterByProtocol 按类名匹配失效 → 按 getAdapterName ④ 3 个 TSP Client 混用 reactor-netty/WebClient → 统一 Spring WebClient ⑤ 缺 List import; 测试 core 46 / ccc 15 / icce 10 / iccoa 10 / grpc-server 12 全绿（grpc-server 用 Docker 绕开 macOS Rosetta x86_64 插件卡死） |
| 🟠 | **多厂商并发 E2E 测试（CCC↔ICCOA↔ICCE）** | ✅ **E2E-14 已创建** | 含 Relay Mailbox 跨厂商流转: Apple→Xiaomi + Samsung→Huawei |
| 🟠 | **认证文档更新（PICS/PIXIT — Relay Server 部分）** | ✅ `docs/compliance/PICS_PIXIT_RELAY.md` | 覆盖 CCC §11.3.4 全部 6 个 RPC + 邮箱状态机 + Push |
| 🟠 | **依赖安全漏洞清零** | ✅ **已完成 (2026-08-01)** | govulncheck 三模块扫描 0 漏洞 (hub/dkcs/proto); 依赖升级 grpc 1.82.1 + x/net 0.56 + x/crypto 0.54 (CVE 修复版); 回归 1391 + 373 全绿 |
| 🟠 | **dkcs 服务 PG schema 迁移** | ✅ **已完成 (2026-08-01)** | `backend/db/migrations/0001_init.*` + `backend/dkcs/internal/migrate`（零依赖执行器，启动自动迁移）; 真实 PG docker 验证幂等 + 5/5 单测; `scripts/migrate-dkcs.sh` 手动通道 |
| 🟠 | **Helm Chart 同步 postgres** | ✅ **已完成 (2026-08-01)** | `backend/cloud/deploy/helm/dkcs` 全量 mysql→postgres 对齐 kustomize（statefulset/secret/configmap/deployment/values/pdb）; 0 mysql 残留静态验证; README 注明 kustomize 为官方路径 |

### P2 — 增强功能

| # | 事项 | 状态 |
|:-:|:-----|:----:|
| 🟡 | 离线授权回退机制 | ✅ **已完成 (2026-08-01)** | 调研: 移动端 KeyManager 离线授权裁决 (PRD 模块五/RS-007-34); 方案 A 双端 OfflineAuthorizer (fail-closed: revoked/suspended/expired/未知状态拒 + 7 天宽限期); iOS 13 + Android 17 用例; 见 `docs/sdk/OFFLINE-FALLBACK-DESIGN.md` |
| 🟡 | 插件 SDK 文档（面向第三方开发者）| 📋 | ⚠️ 疑为 yuleASR-Configurator 项目错放项（yuleDKCS 无插件体系）; 已在 yuleASR 侧处理, 建议移除 |
| 🟡 | 性能测试（大配置加载/保存）| 📋 | ⚠️ 疑为 yuleASR-Configurator 错放项（AUTOSAR 配置语境）; 建议移除 |
| 🟡 | postgres-exporter 部署 | ✅ **已完成 (2026-08-01)** | kustomize `deploy/k8s/postgres/exporter.yaml` (Deployment + Service :9187) + helm 同步; 独立 Deployment 与 StatefulSet 解耦; kubectl kustomize 33 文档渲染验证; 见 `deploy/k8s/README.md` |
| 🟡 | JWKS kid 未命中防放大 | ✅ **已完成 (2026-08-01)** | oem 级刷新冷却 30s + kid 级负缓存（上限 1024/OEM 防内存撑爆）; 单飞并发去重保留; 4 单测含 -race, 24 passed |
| 🟡 | tests/integration 预存 vet 错误 | ✅ **已完成 (2026-08-01)** | e2e_14 relay API 签名漂移修复 + e2e_11 Delete 终态语义隐性回归修复; integration **87 passed / 0 failed**, go vet exit 0 |

---

> **状态图例：** ✅ 完成 · 🔜 进行中 · 📋 待排期 · ⏸ 暂停 · ❌ 阻塞
