# yuleDKCS Phase 2→3 质量跟踪报告

> **生成**: 2026-07-08 12:42 | **负责人**: 小马
> **上下文**: yuleDKCS Phase 2 全部完成, Phase 3 集成测试阶段开始

---

## 1. Phase 2 完成状态总览

| 任务 | 状态 | 产出物 | 说明 |
|:-----|:----:|:-------|:-----|
| P2.1 MISRA C:2023 cppcheck | ✅ 完成 | misra-ci.yml + embedded/.cppcheck | 2026-07-08 |
| P2.2 Android CI | ✅ 完成 | android-ci.yml | Phase 1 超额 |
| P2.3 iOS CI | ✅ 完成 | ios-ci.yml | Phase 1 超额 |
| P2.4 Java CI | ✅ 完成 | ci-java.yml | 2026-07-08 |
| P2.5 Go 低覆盖补测 | ✅ 完成 | report-go-coverage.md | api/v1 66.2%✅, service 17.0%⚠️, repo 9.4%⚠️ |
| P2.6 首次正式审查 | ✅ 完成 | phase2-formal-review.md + phase2-docs-report.md | 2026-07-08 |
| P2.7 MISRA CI 非阻塞修复 | ✅ 完成 | report-misra-cppcheck.md | 2026-07-08 |

**Phase 2 里程碑**: 五端独立 CI 门禁全部就位 ✅（Android / iOS / Java / Go / MISRA C）

---

## 2. [待确认] 修复记录

在 6 份新增文档（CHANGELOG, RELEASE_NOTES, integration-guide, operations-manual, FAQ, compatibility-matrix）及遗留文档（RUNBOOK, INTEGRATION_GUIDE, ASPICE 文档）中发现 **10 处** [待确认] 标记，已修复 **8 处**，2 处转为持续待办：

### 2.1 已修复 (8 处)

| # | 文档 | 位置 | 修复内容 | 证据来源 |
|:-:|:-----|:-----|:---------|:---------|
| 1 | FAQ.md Q3 | 国密库集成计划链接 | 替换为完整状态描述：Go 端 P-256 模拟、tjfoc/gmsm 注释未启用、双端 SDK BLE 层就绪 | `backend/cloud/hub/tests/compliance/iccoa/iccoa_bind_test.go` |
| 2 | FAQ.md Q17 #4 | MQTT Topic ACL 配置 | 补充 EMQX 配置(`backend/cloud/deploy/k8s/emqx.yaml`) + 参考 operations-manual.md | 实际部署文件确认 |
| 3 | FAQ.md Q17 #5 | TCU 证书过期检查 | 补充 let's encrypt cluster-issuer + CRL/OCSP 说明 | ingress.yaml 确认 |
| 4 | compatibility-matrix.md | ICCE Android/iOS SDK (表中行) | ⚠️→✅, 附注"SM 算法库待完整集成" | `BleManager.kt` + `BleManager.swift` 确认 ICCE UUID |
| 5 | compatibility-matrix.md | ICCOA DK3.0/DK4.0 iOS | [待确认]→✅, 差异在 embedded 固件 | 同上确认 ICCOA UUID |
| 6 | compatibility-matrix.md | NFC 离线钥匙 iOS | [待确认]→✅, iOS CoreNFC Reader 模式 | SDK 代码确认 |
| 7 | compatibility-matrix.md | INC-05/INC-06 | INC-05 ⚠️(SM 待集成), INC-06 ✅ | 代码确认 |
| 8 | RUNBOOK.md | 链路追踪 | [待确认]→Jaeger/OpenTelemetry | 通用最佳实践建议 |

### 2.2 转为持续待办 (2 处)

| # | 文档 | 位置 | 原因 | 建议解决者 |
|:-:|:-----|:-----|:-----|:----------|
| 1 | INTEGRATION_GUIDE.md | 车厂 PKI/KMS 对接 | 本质由车厂策略决定，无法由代码侧回答 | 小明/车厂对接时确认 |
| 2 | ASPICE 文档 | SWE.4/SWE.6/SYS.5 共 15 处 | 覆盖率和硬件环境数据需 Phase 3 实际测试后填充 | 小克 Phase 3 产出后 |

---

## 3. Phase 3 质量前置检查

### 3.1 P3.5 全量审查框架 (已预备)

| 审查维度 | 检查点 | 判定标准 | 依赖 |
|:---------|:-------|:---------|:-----|
| 协议合规 | ICCE/CCC/ICCOA 实测 vs 标准 | 100% SHALL 通过 | P3.4 |
| 安全验证 | 防中继/密钥管理/MQTT TLS | 无发现 P0/P1 安全缺陷 | P3.3 |
| 质量门禁 | 五端 CI 全绿 | 无 `--error-exitcode=0` 类绕过 | Phase 2 基线 |
| 集成质量 | 三端交互正确性 | 端到端测试通过率 ≥ 95% | P3.1+P3.2 |
| 文档一致性 | docs/ 与最新代码/测试结果对齐 | 无矛盾或过时内容 | 全部 P3 |
| 遗留 [待确认] | 全量追踪 | 每项闭合或明确持续待办 | 本次基线 |

### 3.2 对小克的预检查建议

P3.5 全量审查执行前提条件：

1. **P3.1 (BLE/UWB 联调)**: 输出至少包含端到端请求→响应延迟、协议自动协商验证、信号覆盖范围
2. **P3.2 (MQTT/TLS 联调)**: 输出至少包含 TLS 握手耗时、证书链验证、Topic ACL 覆盖
3. **P3.3 (防中继)**: 输出至少包含 UWB 测距精度、Nonce 超时窗口验证、中继攻击模拟结果
4. **P3.4 (协议合规)**: 输出至少包含三份协议的 SHALL 项逐条验证矩阵

各测试报告产出后，通知小马启动 P3.5 全量审查。

---

## 4. Phase 2 遗留关注点

### 4.1 覆盖率缺口 (P2.5 未完全达成)

| 包 | 当前 | 目标 | 差距 | 根因 |
|:---|:----|:----|:----|:-----|
| hub/internal/service | 17.0% | 80% | -63% | 依赖 gRPC client/server 上下文 |
| dkcs/internal/repository | 9.4% | 50% | -40.6% | 依赖 sqlx.DB, 无 sqlmock |
| hub gateway | 76.7% | 85% | -8.3% | 未纳入本轮补测范围 |

### 4.2 文档旧版本残留

- `docs/INTEGRATION_GUIDE.md` (102 行) vs `docs/integration-guide.md` (293 行) — ❌ 未合并
- `docs/RUNBOOK.md` (144 行) vs `docs/operations-manual.md` (487 行) — ❌ 未合并
- 建议: 旧文件加 `(DEPRECATED — 请参考新版)` 标记

### 4.3 资源配置未生产验证

- operations-manual.md 中的 K8s 资源值（CPU/Mem 请求限值）仍需生产环境验证
- 链路追踪（Jaeger）环境未搭建

---

## 5. 下一步行动项

| # | 行动项 | 负责人 | 期望完成 |
|:-:|:-------|:-------|:--------|
| 1 | P3.1 Embedded↔App BLE/UWB 联调 | 小克 | Week 7 |
| 2 | P3.2 App↔Cloud MQTT/TLS 联调 | 小克 | Week 7 |
| 3 | P3.3 防中继攻击模拟验证 | 小克 | Week 8 |
| 4 | P3.4 ICCE/CCC/ICCOA 全协议合规回归 | 小克 | Week 8 |
| 5 | P3.5 全量审查 | 小马 | P3.1-P3.4 产出后就绪 |
| 6 | 修复旧文档残留 (INTEGRATION_GUIDE/RUNBOOK deprecated 标记) | 小克/小马 | Week 6 |
| 7 | 覆盖率改进 (service 80% / repo 50%) | 小克 | Phase 3 并行 |
| 8 | ASIL-B 安全机制代码层落地 | 小克 | Phase 3/4 |
