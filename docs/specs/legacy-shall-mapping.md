# 遗留 SHALL ID → 现 REQ 体系映射（证据索引过滤）

> **用途**: 供 `yuleosh evidence pack` / `yuleosh traceability matrix` 索引过滤。
> 遗留 SHALL 编号（`KL-SHALL-01` 等，源自旧 OpenSpec 契约层 `spec-contract.md` 与旧 spec 文件）已被
> `docs/software-requirements.md` 的 `REQ-xxx` 唯一标识体系取代。本表将遗留条目映射到现需求，
> 工具据此将 **superseded（已废弃）** 条目从证据索引中剔除、将 **mapped（已映射）** 条目并入目标 REQ，
> 避免遗留 ID 因无测试映射而拖垮覆盖率门禁（历史问题：184 条索引中 140 条遗留 ID 无映射 → 覆盖率 24% FAIL）。
>
> **状态语义**:
> - `superseded` — 该组遗留 ID 已被现 REQ 体系整体取代，从证据索引剔除（对应 REQ 已在索引中且 40/40 有测试追溯）。
> - `mapped` — 条目并入目标 REQ ID（与既有 REQ 条目去重合并，不重复计数）。

---

## 一、遗留 SHALL 前缀组（superseded，源自旧契约层 spec-contract.md）

| 遗留 ID 模式 | 状态 | 现需求 ID | 说明 |
|:-------------|:-----|:----------|:-----|
| KL-SHALL-* | superseded | REQ-010~014 | 密钥生命周期管理 → 密钥绑定/解绑/撤销/列表/分享 |
| PE-SHALL-* | superseded | REQ-009, REQ-030 | 被动无感解锁/上锁 → 用户体验 / UWB 测距层 |
| NF-SHALL-* | superseded | REQ-028, REQ-007 | NFC 刷卡解锁 → NFC 通信层 / 离线能力 |
| RC-SHALL-* | superseded | REQ-015 | 远程控车 → 车辆控制指令 |
| ES-SHALL-* | superseded | REQ-015 | 发动机启动 → 车辆控制指令（11 动作含 Engine） |
| KS-SHALL-* | superseded | REQ-014 | 钥匙分享 → 分享创建 |
| KR-SHALL-* | superseded | REQ-012 | 钥匙吊销 → 密钥撤销 |
| RA-SHALL-* | superseded | REQ-006 | 防中继攻击 → 安全性需求（UWB Secure Ranging） |
| KSS-SHALL-* | superseded | REQ-006, REQ-010 | 密钥存储与安全 → 安全性需求 / 密钥绑定 |
| CM-SHALL-* | superseded | REQ-006 | 通信安全 → 安全性需求（TLS/加密/防重放） |
| OT-SHALL-* | superseded | REQ-035 | OTA 升级 → 安全启动（签名校验/安全升级） |
| UA-SHALL-* | superseded | REQ-001, REQ-006 | 用户认证与会话 → 设备注册认证 / MFA |
| AL-SHALL-* | superseded | REQ-006 | 审计日志 → 安全性需求（审计 ≥3 年） |
| DP-SHALL-* | superseded | REQ-008 | 双协议支持 → 协议兼容性（ICCE+CCC 双栈） |
| OM-SHALL-* | superseded | REQ-007 | 离线模式 → 离线能力 |

## 二、系统需求条目（mapped，RS-xxx 下放为软件需求 REQ-xxx）

> `specs/requirements-index.md` 的 RS-xxx 为系统级需求上游，逐条下放为软件需求 REQ-xxx；
> REQ 的测试追溯即覆盖对应 RS。故 RS 条目并入 REQ，不单独计数。

| 遗留 ID 模式 | 状态 | 现需求 ID | 说明 |
|:-------------|:-----|:----------|:-----|
| 用户设备注册 | mapped | REQ-001 | RS-001 → 设备注册 |
| 多设备配钥 | mapped | REQ-002 | RS-002 → 多设备配钥 |
| 多设备管理 | mapped | REQ-003 | RS-003 → 多设备管理 |
| 性能指标 | mapped | REQ-004 | RS-004 → 性能指标 |
| 可用性需求 | mapped | REQ-005 | RS-005 → 可用性需求 |
| 安全性需求 | mapped | REQ-006 | RS-006 → 安全性需求 |
| 离线能力 | mapped | REQ-007 | RS-007 → 离线能力 |
| 协议兼容性 | mapped | REQ-008 | RS-008 → 协议兼容性 |
| 用户体验 | mapped | REQ-009 | RS-009 → 用户体验 |

## 三、旧 spec 文件散条目（mapped，并入对应 REQ）

> 旧 spec 文件（`spec-fix-p0.md`、`spec-fix-kni.md`、`spec-embedded-c.md`、`spec-frontend-test.md`、
> `spec-cmd-test.md`、`spec-multi-device.md`）中无编号的散 SHALL 条目，按主题并入现 REQ。

| 遗留 ID 模式 | 状态 | 现需求 ID | 说明 |
|:-------------|:-----|:----------|:-----|
| 设备注册 → RS-001 | mapped | REQ-001 | 旧 spec 条目 |
| 多设备按需配钥 → RS-002 | mapped | REQ-002 | 旧 spec 条目 |
| 多设备管理 → RS-003 | mapped | REQ-003 | 旧 spec 条目 |
| dkcs 入口测试 | mapped | REQ-010 | DKCS 入口 → 密钥绑定域 |
| yuledkcs 统一入口测试 | mapped | REQ-001 | 统一入口 → 系统级 |
| hub 入口测试 | mapped | REQ-019 | Hub 入口 → Registry 规范化 |
| Registry 大小写不敏感匹配缺失（P1 🟡）→ SWR-HUB-001 | mapped | REQ-019 | Hub 缺陷项 |
| ICCOACodec.Encode nil pointer dereference（P0 🔴）→ SWR-HUB-001, SWR-HUB-002 | mapped | REQ-020 | nil 指针安全检查 |
| strings.ToLower 定义了但未调用（P1 🟡）→ SWR-HUB-002 | mapped | REQ-020 | nil 指针/健壮性 |
| hub/service 补测试 → SWR-HUB-003 | mapped | REQ-021 | Hub 单元测试覆盖 |
| hub/logger 补测试 → SWR-HUB-003 | mapped | REQ-021 | Hub 单元测试覆盖 |
| 覆盖率门禁 → SWR-HUB-004 | mapped | REQ-022 | CI 覆盖率门禁 |
| CI 分层 L1/L2/L3 → SWR-HUB-005 | mapped | REQ-023 | CI 分层机制 |
| 集成测试 CI 化 → SWR-HUB-005 | mapped | REQ-023 | CI 分层机制 |
| SAST 安全扫描 → SWR-HUB-005 | mapped | REQ-022 | CI 覆盖率门禁/安全门禁 |
| C 单元测试框架引入 | mapped | REQ-021 | 嵌入式测试 → Hub 单测覆盖 |
| CI 中集成 C 单元测试 | mapped | REQ-023 | CI 分层机制 |
| ICCE 协议栈单元测试 | mapped | REQ-032 | ICCE 协议栈 |
| CCC 协议栈单元测试 | mapped | REQ-008 | 协议兼容性（CCC DK 3.0） |
| ICCOA 协议栈单元测试 | mapped | REQ-031 | ICCOA 协议栈 |
| Android SDK API 单元测试 | mapped | REQ-036 | Android SDK |
| iOS SDK API 单元测试 | mapped | REQ-037 | iOS SDK |
| 测试编译 CI 集成 | mapped | REQ-023 | CI 分层机制 |

---

*— 本表由 yuleDKCS ASPICE CL2 证据补齐 Sprint 维护；变更须同步 `docs/software-requirements.md` 追溯关系。—*
