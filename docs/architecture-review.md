# 架构评审记录 (Architecture Review Record)

> **项目**: yuleDKCS 数字钥匙系统
> **评审对象**: `docs/architecture.md` v1.0.0（架构基线）
> **日期**: 2026-08-06 | **评审人**: 小马 (Evaluator) / 小克 (Architect) | **结论**: ✅ 通过（有条件）
> **过程域**: ASPICE SWE.2.BP3 (Verify architecture)
> **关联**: `docs/design/DK-HUB-ARCHITECTURE.md`、`docs/design/HUB-DETAILED-DESIGN.md`、`docs/aspice/SWE.4-software-arch.md`

---

## 1. 评审范围

- 组件边界与职责（云端 Hub/DKCS、车端四层、Frontend）
- 接口定义完整性（9 处 include 目录 + 内部 Go 接口 + 外部协议接口）
- 数据流正确性（绑定/分享/远程控车/状态上报/吊销）
- 需求覆盖（REQ-001~040 ↔ 架构组件映射）
- 安全架构关联（SG-01~06）

## 2. 评审结论

| 评审项 | 结论 | 说明 |
|:-------|:----:|:-----|
| 组件边界清晰 | ✅ | Hub 授权决策 vs DKCS 密钥编排 vs OEM KMS 密钥材料，职责分离明确 |
| 接口定义完整 | ✅ | 外部接口（REST/gRPC/MQTT/BLE/UWB/NFC）+ 内部接口（10 组 Go 接口）+ 车端 41 头文件盘点齐全 |
| 数据流正确 | ✅ | 三层绑定/分享/控车时序与 HUB-DETAILED-DESIGN §7 一致 |
| 需求覆盖 | ✅ | 40 条 REQ 全部映射至组件 |
| 安全架构 | ✅ | SG-01~06 覆盖防中继/密钥保护/鉴权/吊销 |
| 可验证性 | ✅ | 组件与测试目录一一对应（tests/integration、tests/hil） |

## 3. 发现项跟踪（Findings → Closure）

| 编号 | 发现 | 严重度 | 处置 | 状态 |
|:-----|:-----|:------:|:-----|:----:|
| AR-01 | BSW 服务（OS/COM/DCM 等）未集成，ASIL-B 运行时隔离缺失 | 🔴 高 | 列入 ADR-04 已知缺口，BSW 三阶段集成计划推进（先 MCAL+OS，再诊断，后 ASIL-B 分区） | 🔄 跟踪中 |
| AR-02 | 三协议栈错误码风格不统一 | 🟡 中 | REQ-027 云端统一错误码已定义；车端错误码对齐列入后续 Sprint | 🔄 跟踪中 |
| AR-03 | include/ 汇总层为文档级契约头，未接入产品构建 | 🟢 低 | 有意为之（避免影响产品代码）；后续可将 dk_hal.h 与 unified_hal 实际签名对拍 | 🔄 跟踪中 |
| AR-04 | VFB 接口未定义（AUTOSAR 对齐待确认） | 🟡 中 | 待 yuleASR 集成决策后定义 | 🔄 跟踪中 |

**闭环要求**: AR-01/AR-02/AR-04 在 BSW 集成 Sprint 关闭；AR-03 在接口对拍任务关闭。跟踪记录存 `.osh/reviews/`。

## 4. 评审证据

- 评审会记录: `.osh/reviews/architecture-review-2026-08-06.md`
- 架构文档: `docs/architecture.md`
- 上游设计: `docs/design/DK-HUB-ARCHITECTURE.md`（303 行）、`docs/design/HUB-DETAILED-DESIGN.md`（1012 行）

---

*— 评审通过；发现项按上表跟踪至闭环。—*
