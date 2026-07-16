# Production Audit: Spec 合规 + 文档完整性审计

> **审计范围**: yuleDKCS spec-contract.md, 所有核心文档, 与代码的双向覆盖
> **审计人**: 小马 (质量架构师)  
> **日期**: 2026-07-07  
> **版本**: 1.0.0  

---

## 目录

1. [Spec 覆盖度分析](#1-spec-覆盖度分析)
2. [文档完整性矩阵](#2-文档完整性矩阵)
3. [Spec→文档一致性分析](#3-spec文档一致性分析)
4. [验收标准评估](#4-验收标准评估)
5. [缺陷汇总（P0/P1/P2）](#5-缺陷汇总-p0p1p2)
6. [修复建议](#6-修复建议)

---

## 1. Spec 覆盖度分析

### 1.1 Spec 模块 vs 代码功能对照

| Spec 模块 | SHALL 数 | 代码实现路径 | 覆盖状态 |
|:----------|:---------|:-------------|:---------|
| KL 密钥生命周期 | 10 | `backend/dkcs/internal/service/key_service.go`, `embedded/*/keymgmt/`, `frontend/*/key/*` | ✅ |
| PE 被动解锁 | 10 | `embedded/*/ble/* uwb/*`, `frontend/*/ble/* uwb/*` | ✅ |
| NF NFC 刷卡 | 6 | `embedded/*/nfc/*`, `frontend/*/nfc/*` | ✅ |
| RC 远程控车 | 9 | `backend/dkcs/internal/service/command_service.go` | ✅ |
| ES 发动机启动 | 6 | `embedded/*/ble/*`, `command_service.go` | ✅ |
| KS 钥匙分享 | 9 | `backend/dkcs/internal/service/key_service.go` | ✅ |
| KR 钥匙吊销 | 6 | `backend/dkcs/internal/service/key_service.go` | ✅ |
| RA 防中继攻击 | 10 | `embedded/*/security/*`, `frontend/*/anti_relay*` | ✅ |
| KS-SEC 密钥安全 | 10 | `embedded/*/security/* se050/*`, `frontend/*/keychain*` | ✅ |
| CM 通信安全 | 10 | 全三端通信层 | ✅ |
| OT OTA 升级 | 4 | `embedded/*/ota*` | ✅ |
| UA 用户认证 | 6 | `backend/dkcs/internal/middleware/*`, `frontend/*/auth*` | ✅ |
| AL 审计日志 | 7 | `backend/dkcs/internal/repository/event_repo.go`, `frontend/*/telemetry/*` | ✅ |
| DP 双协议支持 | 6 | `embedded/icce_protocol/`, `embedded/ccc_protocol/`, `embedded/iccoa_protocol/` | ✅ |
| OM 离线模式 | 4 | `embedded/*/nfc/*` | ✅ |

**结论**: 所有 15 个 Spec 模块在代码中有对应实现路径。Spec 覆盖度 **完整**。

### 1.2 代码中存在但 Spec 未覆盖的功能

| # | 功能 | 代码位置 | Spec 缺失说明 | 严重度 |
|:-:|:-----|:---------|:--------------|:-------|
| 1 | **Telemetry 遥测埋点框架** | `frontend/*/telemetry/*` — 全套事件类型(APP_LAUNCH, BLE_CONNECT, NFC_TAP, UWB_RANGING, SECURITY_ALERT 等)+批量上报+采样 | Spec 仅有 AL-SHALL-06 (Kafka 审计日志), 未定义 App 端遥测 vs 后端审计日志分层体系。DkTelemetry 实际是独立于审计日志的运营分析通道 | **P2** |
| 2 | **Device Capabilities 设备能力上报** | `frontend/*/DeviceCapabilities.*` — 检测 BLE/UWB/NFC/SE/OS 能力并上报云端 | 仅有 spec-multi-device.md 提到"设备注册时上报能力", spec-contract.md 无对应 SHALL | **P2** |
| 3 | **8-bit 权限位图模型** | `docs/PERMISSION_MODEL.md` — 完整的 8 位权限位定义 + 角色组合示例 | Spec 引用 "RBAC + ABAC" (UA-SHALL-05) 但未定义权限位图实现细节。权限模型在独立的 PERMISSION_MODEL.md 中 | **P2** |
| 4 | **命令集扩展** (TrunkOpen, Panic, 寻车) | `command_service.go` — Go 实现 trunk_open, panic, find_vehicle | RC-SHALL-02 只列出 6 种操作(解锁/上锁/启动/停止/闪灯鸣笛/空调/车窗)。`TrunkOpen` 和 `Panic` 未明确列出 | **P2** |
| 5 | **Idempotency 幂等性保证** | `key_service.go` [M-07] — idempotency key 去重机制 | Spec 无任何幂等性要求。重复创建钥匙/吊销请求的理论安全边界未定义 | **P2** |
| 6 | **FRP (Factory Reset Protection)** | 未找到明确代码, 但嵌入式 ICCE 协议涉及 | Spec 和代码均未覆盖 | **P3** |

### 1.3 Spec 中潜在的过时/不可实现需求

| ID | 问题 | 分析 |
|:---|:-----|:-----|
| DP-SHALL-NOT-01/02 | ICCE 禁用国际算法, CCC 禁用国密算法 | **技术上可行但未来协议可能混合**。ICCOA DK4.0 开始探索混合算法栈。建议改为 SHOULD NOT 并标注"除非 ICCOA 协议协商"例外 |
| KS-SHALL-02 | 分享支持四种方式(二维码/链接/NFC/手机号推送) | **实现上可行**, 但 NFC 碰一碰分享需要 iPhone 侧支持——iOS 端实际只有 CoreNFC reader mode, 主动 NFC 写入需额外认证。建议改 Android/iOS 分端标注 |
| RA-SHALL-04 | 解锁指令响应超 ~3μs 拒绝 | **3μs 是 IEEE 802.15.4z 物理层阈值**, 但软件层堆栈延迟远超 3μs。实际判决阈值需在 @3~5ms 范围。3μs 可能是协议层误用。需与 embedded 团队确认 |

### 1.4 Spec 覆盖度结论

整体覆盖率 **~90%**（代码主要功能 > spec 覆盖）。需补充：
- Telemetry 分层规范
- DeviceCapabilities 注册流程
- Permissions 位图规范
- 幂等性保障需求

---

## 2. 文档完整性矩阵

### 2.1 必需文档检查

| 文档 | 路径 | 行数 | 状态 | 完整性 | 备注 |
|:-----|:-----|:----:|:----:|:------:|:-----|
| ✅ 产品需求 PRD | `docs/design/PRD.md` | 924 | ✅ 存在 | ✅ 完整 | V1.0, 正式版 |
| ✅ 系统架构 | `docs/SYSTEM_ARCHITECTURE.md` | 640 | ✅ 存在 | ✅ 完整 | V1.0 |
| ✅ API 参考 | `docs/API_REFERENCE.md` | 782 | ✅ 存在 | ✅ 完整 | REST + gRPC |
| ✅ 安全指南 | `docs/SECURITY_GUIDE.md` | 490 | ✅ 存在 | ✅ 完整 | |
| ✅ 部署指南 | `docs/DEPLOYMENT_GUIDE.md` | 721 | ✅ 存在 | ✅ 完整 | K8s + Docker |
| ✅ API 契约 | `docs/design/API-CONTRACT.md` | 2682 | ✅ 存在 | ✅ 完整 | BLE + NFC + REST + gRPC |
| ✅ 测试计划 | `docs/design/TEST-PLAN.md` | 562 | ✅ 存在 | ✅ 完整 | 三端测试矩阵 |
| ✅ 开发指南 (×3) | `docs/design/*-DEV-GUIDE.md` | 4810 | ✅ 存在 | ✅ 完整 | 3份详细指南 |
| ✅ 代码审查 | `docs/design/CODE-REVIEW-V2.md` | 415 | ✅ 存在 | ✅ 完整 | V2 通过 |
| ➕ 安全白皮书 | `docs/SECURITY_WHITEPAPER.md` | 979 | ✅ 存在 | ✅ 完整 | 额外文档 |
| ➕ 权限模型 | `docs/PERMISSION_MODEL.md` | 80 | ✅ 存在 | ✅ 完整 | 额外文档 |
| ➕ DK Hub 架构 | `docs/design/DK-HUB-ARCHITECTURE.md` | 303 | ✅ 存在 | ✅ 完整 | 额外文档 |
| ➕ OpenAPI Spec | `docs/api/openapi.yaml` | 1777 | ✅ 存在 | ✅ 完整 | YAML 契约 |

### 2.2 缺失文档

| 文档类型 | 是否缺失 | 路径建议 | 说明 |
|:---------|:--------:|:---------|:-----|
| **用户手册 / 集成指南** | **❌ 缺失** | `docs/USER_GUIDE.md` | 面向车厂集成商/第三方开发者的集成步骤指南。PRD 1.3 提了"提供标准化 SDK 供第三方集成"但无对应指南 |
| **运维手册 (Runbook)** | **❌ 缺失** | `docs/RUNBOOK.md` | 生产环境运维流程: 健康检查、告警、日志采集、备份恢复、扩容缩容、故障处理 SOP |
| **FAQ / 常见问题** | **❌ 缺失** | `docs/FAQ.md` | 面向集成商/用户的常见技术问题解答 |
| **变更日志** | **❌ 缺失** | `CHANGELOG.md` | 项目根目录无 CHANGELOG。CODE_OF_CONDUCT.md 和 COMMUNITY.md 存在但不能替代 |
| **版本兼容性矩阵** | **❌ 缺失** | `docs/COMPATIBILITY_MATRIX.md` | 需记录: 各 SDK 版本兼容的最低 OS / 协议版本 / 硬件要求 |
| **发布说明** | **❌ 缺失** | `RELEASE_NOTES.md` | 无版本发布信息。PRD 附录仅有一个"修订记录" |

### 2.3 文档状态汇总

```
已存在文档: 13 份 (12 必需 + 3 额外)
缺失文档:   6 份 (用户手册/集成指南, Runbook, FAQ, CHANGELOG, 兼容性矩阵, 发布说明)

完整性:     ⚠️ 68% (13/19 文档清单中存在, 但 6 份缺失较严重)
```

---

## 3. Spec→文档一致性分析

### 3.1 ASIL 等级 — Spec vs Safety-Concept 一致性

| Safety Concept SG | Spec 映射 | ASIL(safety-concept) | ASIL(spec) | 一致? |
|:------------------|:----------|:--------------------:|:----------:|:-----:|
| SG-01 非预期解锁防护 | PE-SHALL-01~04, PE-SHALL-NOT-01/02 | **ASIL-B** | **ASIL-B** | ✅ |
| SG-02 非预期启动防护 | ES-SHALL-01~04, ES-SHALL-NOT-01/02 | **ASIL-B** | **ASIL-B** | ✅ |
| SG-03 中继攻击防护 | RA-SHALL-01~07, RA-SHALL-NOT-01/02 | **ASIL-B(D)** | **ASIL-B(D)** | ✅ |
| SG-04 密钥防提取/克隆 | KS-SHALL-01/02/03/04/KS-NOT-01 | **ASIL-B** | **ASIL-B** | ✅ |
| SG-05 远程指令密码学认证 | RC-SHALL-01/05, RC-SHALL-NOT-01 | **ASIL-A** | **ASIL-A** | ✅ |
| SG-06 吊销时效 | KR-SHALL-01/02/03, KR-SHALL-NOT-01 | **ASIL-A** | **ASIL-A** | ✅ |

**🔴 P1 — ASIL 等级不一致:**

| 项目 | safety-concept.md | spec-contract.md | 问题 |
|:----|:-----------------|:-----------------|:-----|
| SE050 EAL | SG-04 标注 **EAL6+**" | KS-SHALL-04 标注 **EAL5+** | ⚠️ 不一致 |
| SG-03 FTTI | safety-concept 标注 FTTI <100ms | Spec 对应 RA 无 FTTI 约束 | ⚠️ safety-concept 更严格 |
| SG-01 FTTI | safety-concept 标注 <500ms | Spec PE-SHALL-01 要求 ≤1s | ⚠️ safety-concept (500ms) vs spec(1s) 冲突 |
| KS-SEC-03 HKDF 派生规范 | safety-concept 无引用 | Spec KS-SHALL-05 明确 4 层密钥派生 | 🔵 spec 更详细, safety-concept 需要更新 |

### 3.2 SHALL vs PRD 功能一致性

| # | Spec SHALL ID | PRD 映射段 | 一致性 | 问题 |
|:-:|:--------------|:-----------|:------:|:-----|
| 1 | PE-SHALL-01 (≤1s) | PRD §3.1.2 UWB/BLE | ✅ | 一致 |
| 2 | PE-SHALL-07 (灯/鸣笛) | PRD §3.1.4 | ✅ | 一致 |
| 3 | RC-SHALL-02 (6种操作) | PRD §3.3.1 | ⚠️ **缺失** | PRD 还列出了 TrunkOpen, Panic 但 spec 未覆盖 |
| 4 | KS-SHALL-01 (3级) | PRD §3.2.2 | ✅ | 一致 |
| 5 | UA-SHALL-04 (MFA) | PRD §3.2.2 | ⚠️ **缺失细节** | PRD 提到"人脸识别"但 spec 只说"生物识别" |
| 6 | DP-SHALL-04 (VIN 自动识别) | PRD §3.1.3 | ✅ | 一致 |
| 7 | KL-SHALL-04 (≤10把) | PRD §6.2 (≥10把) | ⚠️ **不一致** | Spec 说 ≤10, PRD 说 ≥10, 逻辑方向相反! PRD 是容量要求, Spec 是限制 |

### 3.3 PRD 有但 Spec 无覆盖的功能

| PRD 节 | 功能 | Spec 覆盖 | 严重度 |
|:-------|:-----|:----------|:-------|
| §1.2 | UBI 保险/分时租赁等商业模式 | ❌ 未覆盖 | P3 — 业务模式非需求 |
| §3.1.2 | UWB 4~6 天线阵列 | ❌ 未覆盖 | P3 — 硬件实现细节 |
| §3.2.2 | 人脸识别/FaceID 生物认证 | ⚠️ 仅"生物识别"概括 | P2 — MFA 具体方式需对齐 |
| §3.3.1 | 远程控车: 后备箱控制 | ❌ 代码有但 spec 无 | P2 — 功能遗漏 |
| §7.1 | 数据合规(GDPR/CCPA/个保法) | ❌ 未覆盖 | P2 — 合规需求 |
| §7.1 | 数据本地化处理 | ❌ 未覆盖 | P2 — 合规需求 |
| §7.2 | 多 Region 部署 | ❌ 未覆盖 | P3 — 架构级, 可通过部署指南覆盖 |
| §1.4 | FIDO 认证协议 | ❌ 未覆盖 | P3 — UA-SHALL-02 已说 OAuth 2.0/OIDC |
| 附录 B | OCSP 证书校验 | ❌ 未覆盖 | P3 — 可归入 PKI 基础设施 |

### 3.4 其他交叉文档一致性

| 检查项 | 文档 A | 文档 B | 一致? |
|:-------|:-------|:-------|:-----:|
| BLE MTU ≥ 512 | CM-SHALL-04 | API-CONTRACT §1.2.3 MTU ≥ 512 | ✅ |
| ICCE AID | NF-SHALL-03 | API-CONTRACT §2.1 | ✅ |
| 钥匙状态机 | KL-SHALL-01 | API-CONTRACT §3 | ✅ | 
| JWT Token 有效期 | CM-SHALL-07 (≤1h/≤7d) | API-CONTRACT §3.2 | ✅ |
| 防中继 UWB | RA-SHALL-01~07 | TEST-PLAN §2.1.2 | ✅ |
| SE050 EAL | KS-SHALL-04 (EAL5+) | PRD §3.1.4 (EAL5+) | ✅ (safety-concept EAL6+ 不一致) |
| CAN FD 指令 | PE-SHALL-06 | API-CONTRACT §4 | ✅ |

---

## 4. 验收标准评估

### 4.1 验收项盘点

Spec §4 验收判定矩阵共 **43 项**（多于声明的 35 项），按功能域：

| 功能域 | 项数 | 可测量? | 可验证? | 评估 |
|:-------|:----:|:-------:|:-------:|:-----|
| 密钥生命周期 | 3 | ✅ | ✅ | 状态机/计时/注入测试 |
| 被动解锁 | 3 | ✅ | ✅ | 计时/场景/距离注入 |
| NFC 解锁 | 2 | ✅ | ✅ | 场景/APDU 追踪 |
| 远程控车 | 3 | ✅ | ✅ | E2E 计时/安全测试/RBAC |
| 发动机启动 | 1 | ✅ | ✅ | 场景测试 |
| 钥匙分享 | 4 | ✅ | ✅ | 计时/T+延时/撤销后/定位 |
| 钥匙吊销 | 2 | ✅ | ✅ | TTL/离线场景 |
| 防中继攻击 | 4 | ✅ | ✅ | 中继模拟/协议分析/注入 |
| 密钥安全 | 4 | ✅ | ✅ | 渗透/安全审查/ENV 切换 |
| 通信安全 | 4 | ✅ | ✅ | 抓包/协议分析/时效/认证 |
| OTA | 1 | ✅ | ✅ | 签名篡改测试 |
| 审计日志 | 2 | ✅ | ✅ | 操作→日志验证/配置检查 |
| 双协议 | 3 | ✅ | ✅ | 算法/场景 |
| 离线模式 | 2 | ✅ | ✅ | 场景/网络恢复 |
| 用户认证 | 2 | ✅ | ✅ | 集成/场景 |
| 性能 | 3 | ✅ | ✅ | k6 压力/监控/极限测试 |

**总评**: 43 项中 **100% 可测量、可验证** ✅

### 4.2 缺失的关键验收项

| # | 缺失验收项 | 关联需求 | 严重度 | 建议验收方法 |
|:-:|:----------|:---------|:------:|:------------|
| 1 | **安全事件检测** (RA-SHALL-07) | 中继攻击检测后告警推送 | **P1** | 中继攻击模拟→验证安全事件日志出现+告警推送 |
| 2 | **云端吊销同步到车端** (KR-SHALL-02) | 车端离线后恢复联网时同步 | **P1** | 离线→吊销→联网→验证车端吊销缓存同步 |
| 3 | **Service Mesh / gRPC TLS** | CM-SHALL-02 | **P2** | 抓包验证 gRPC over TLS |
| 4 | **App root/jailbreak 检测** | SHOULD-05 | **P2** | root 模拟→拒绝敏感功能 |
| 5 | **密钥过期前端行为** | OM-SHALL-NOT-01 | **P2** | 创建过期钥匙→验证 App/车端拒绝 |
| 6 | **多设备同时连接 >8** | PE-SHALL-08 | **P3** | 9 台设备同时连接→第 9 台等待 |
| 7 | **多 Region 部署验证** | PRD §6.2 | **P3** | 跨 region 延迟测试 |
| 8 | **钥匙转让/车辆所有权转移** | PRD §2.2.1 | **P2** | Spec 无此需求, 但 PRD 角色定义中提到 |
| 9 | **数据合规验证 (GDPR/个保法)** | PRD §7.1 | **P2** | 数据分类/删除/可移植性测试 |

### 4.3 验收判定方法评估

| 评价项 | 结果 |
|:-------|:-----|
| 判定方法多样性 | ✅ 6 种类型(状态机/计时/场景/安全/渗透/压力) |
| 自动化程度 | ⚠️ 未标注哪些可 CI 自动化 |
| 边界/异常路径覆盖 | ⚠️ 缺少 FTTI 超时边界测试 |
| 性能指标 | ✅ k6 压力测试已有 |
| 安全验证 | ✅ 注入/中继模拟/渗透/协议分析 |
| 兼容性/合规测试 | ⚠️ 缺少"
---

## 5. 缺陷汇总（P0/P1/P2）

### P0 — 发布阻断 (0 项)

| ID | 类别 | 描述 |
|:---|:-----|:-----|
| — | — | 当前无 P0 缺陷 |

### P1 — 严重 (3 项)

| ID | 类别 | 描述 | 影响文档/位置 | 修复优先级 |
|:---|:-----|:-----|:-------------|:----------|
| **DF-01** | ASIL 不一致 | Spec KS-SHALL-04 说 SE050 EAL5+, safety-concept SG-04 说 EAL6+ | `spec-contract.md` vs `safety-concept.md` | **高** |
| **DF-02** | FTTI 冲突 | safety-concept SG-01 FTTI <500ms vs Spec PE-SHALL-01 ≤1s | `safety-concept.md` SG-01 vs `spec-contract.md` PE-SHALL-01 | **高** |
| **DF-03** | 接收标准缺失 | 无 RA-SHALL-07 安全事件告警的对应验收项; 无 KR-SHALL-02 车端离线同步验收项 | `spec-contract.md` §4 | **高** |

### P2 — 中等 (10 项)

| ID | 类别 | 描述 | 影响位置 |
|:---|:-----|:------|:---------|
| **DF-04** | 文档缺失 | ❌ 无 **用户手册/集成指南** | `docs/USER_GUIDE.md` 需创建 |
| **DF-05** | 文档缺失 | ❌ 无 **运维手册 (Runbook)** | `docs/RUNBOOK.md` 需创建 |
| **DF-06** | 文档缺失 | ❌ 无 **变更日志** | `CHANGELOG.md` 需创建 |
| **DF-07** | 文档缺失 | ❌ 无 **版本兼容性矩阵** | `docs/COMPATIBILITY_MATRIX.md` 需创建 |
| **DF-08** | 文档缺失 | ❌ 无 **发布说明** | `RELEASE_NOTES.md` 需创建 |
| **DF-09** | 文档缺失 | ❌ 无 **FAQ** | `docs/FAQ.md` 需创建 |
| **DF-10** | Spec 不完整 | Telemetry 遥测埋点框架在代码中存在但 spec 无规范 | 需在 AL 模块增加 SHALL |
| **DF-11** | Spec 不完整 | DeviceCapabilities 设备能力上报无 spec 需求 | 需在 UA/DP 模块增加 |
| **DF-12** | Spec 不完整 | Idempotency 幂等性保障无对应 SHALL | 需在 KL 模块增加 |
| **DF-13** | Spec 过时 | DP-SHALL-NOT-01/02 绝对化, ICCE/CCC 算法隔离规则需协商例外 | 改为 SHOULD NOT + 例外标注 |

### P3 — 低优先级 (5 项)

| ID | 类别 | 描述 | 影响位置 |
|:---|:-----|:------|:---------|
| **DF-14** | Spec 缺失 | 数据合规(GDPR/个保法)无 spec 对应 | 需增加 UA 合规 SHALL |
| **DF-15** | Spec 缺失 | Panic/Alarm/Trunk 命令未明确列出 | RC-SHALL-02 需扩展列 |
| **DF-16** | 验收缺失 | 多用户并发场景、所有权转移无验收项 | 需补充 |
| **DF-17** | 验收缺失 | App root/jailbreak 检测无验收项 | 需补充 (关联 SHOULD-05) |
| **DF-18** | Spec 过时 | RA-SHALL-04 响应阈值 3μs 可能不合理 | 需嵌入式团队复核 |

### 缺陷分级统计

```
P0: 0  (发布阻断)
P1: 3  (严重 — 必须修复)
P2: 10 (中等 — 建议修复)
P3: 5  (低优先级 — 后续迭代)
---
总计: 18 项
```

---

## 6. 修复建议

### 6.1 紧急修复 (P1, 投产前完成)

| # | 建议 | 操作 | 负责人 |
|:-:|:-----|:-----|:-------|
| 1 | **对齐 ASIL 等级**: 统一 SE050 为 EAL5+(或 EAL6+, 但需文档一致) | 修改 safety-concept SG-04 或 spec KS-SHALL-04 | 安全工程师 + 架构师 |
| 2 | **对齐 FTTI**: safety-concept SG-01 FTTI <1000ms 或 spec PE-SHALL-01 改 500ms。建议统一为 1000ms(验收可测量) | 修改 safety-concept 或 spec 确认 | 安全工程师 + 架构师 |
| 3 | **补齐验收项**: 增加 RA-SHALL-07(安全事件告警)和 KR-SHALL-02(车端离线同步)的验收标准 | 修改 spec-contract.md §4 | QA |

### 6.2 建议修复 (P2, 首次发布前完成)

| # | 建议 | 操作 |
|:-:|:-----|:-----|
| 4 | **创建缺失文档**: 按 2.2 清单创建 6 份缺失文档 | PM + 各端负责人 |
| 5 | **补充 Telemetry 规范**: 新增 AL-SHALL-07/08, 定义 App 端遥测 vs 后端审计日志分层 | QA + 架构师 |
| 6 | **补充 DeviceCapabilities**: 新增 UA-SHALL-06, 要求设备注册时上报能力 | 架构师 |
| 7 | **补充 Idempotency**: 新增 KL-SHALL-09, 定义幂等性保障, TTL 等 | 架构师 |
| 8 | **DP-SHALL-NOT 宽松化**: 改为 SHOULD NOT + ICCOA 协议例外标注 | 架构师 |
| 9 | **补齐 Permissions 位图到 Spec**: 整合 PERMISSION_MODEL.md 到 spec 或从 spec 引用 | 架构师 |

### 6.3 改进建议 (P3, 后续迭代)

| # | 建议 | 操作 |
|:-:|:-----|:-----|
| 10 | **数据合规要求**: 增加数据本地化、GDPR/个保法合规 SHALL | PM + 安全工程师 |
| 11 | **命令集完整列出**: RC-SHALL-02 追加 Panic/Trunk/FindVehicle | 架构师 |
| 12 | **RA-SHALL-04 阈值确认**: 与 embedded 团队复核 3μs 是否合理, 修正 | Embedded 架构师 |

---

## 附录 A: 审计方法说明

| 检查项 | 方法 |
|:-------|:-----|
| Spec 覆盖度 | 遍历 spec-contract.md 所有 15 个模块 → 代码路径映射 |
| 文档完整性 | 文件存在性 + 内容行数 + 要素检查 |
| ASIL 一致性 | 逐条比对 safety-concept.md SG 表与 spec ASIL 列 |
| SHALL↔PRD 一致性 | Spec §1 SHALL ID → PRD §3 功能段落对照 |
| 验收标准评估 | Spec §4 每项检查 measurable + verifiable |
| 代码→Spec 反向检查 | 扫描嵌入式/App/后端代码特征, 标题注释, 接口定义 |

## 附录 B: 涉及文件清单

| 文件 | 本次审计用途 |
|:-----|:------------|
| `~/.yuleosh/spec-contract.md` | Spec 主体(540 行, 43 验收项) |
| `~/.yuleosh/safety-concept.md` | ASIL 等级映射源 |
| `docs/design/PRD.md` | 产品需求对照(924 行) |
| `docs/SYSTEM_ARCHITECTURE.md` | 架构设计核对 |
| `docs/design/API-CONTRACT.md` | API 契约核对(2682 行) |
| `docs/design/TEST-PLAN.md` | 测试验收方法核对(562 行) |
| `docs/PERMISSION_MODEL.md` | 权限模型核对 |
| `docs/spec/spec-multi-device.md` | 多设备配钥 spec |
| `frontend/*/telemetry/*` | 遥测埋点代码 |
| `frontend/*/DeviceCapabilities.*` | 设备能力检测代码 |
| `backend/dkcs/internal/service/key_service.go` | 密钥服务 + 幂等性代码 |
| `backend/dkcs/internal/service/command_service.go` | 命令服务代码 |

---

*审计完成时间: 2026-07-07 17:11 CST | 审计人: 小马 (质量架构师)*
