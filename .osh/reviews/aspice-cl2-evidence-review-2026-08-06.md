# Review Record — ASPICE CL2 证据补齐 Sprint 独立评审（Evaluator 复验）

- **Review type**: evidence / gap 数字复验（独立 Evaluator，非作者自评）
- **Target**: yuleDKCS master @ 086ced0（ASPICE CL2 证据补齐 Sprint，P0 声称全完成）
- **Reviewer**: 小马 (Evaluator) | **日期**: 2026-08-06
- **基线**: `docs/aspice/sprint-contract-aspice-cl2.md`（D1-D8 Done 标准）
- **Decision**: ⚠️ 有条件通过（P1×3 修复后可转通过）
- **Score**: 80/100

---

## 0. 评审方法

只读评审。独立复跑：`yuleosh ev check`、`yuleosh ci run 1/2/3`、`pytest tests/integration`、`pytest tests/hil --collect-only`、Go 集成套件 `go test -tags=integration`；手工核验 40 条 REQ 的 SHALL/属性/追溯标记；比对 HIL 原始 JSON 与归档汇总；抽查文档数字与真实文件盘点的一致性。评审期间工具产生的 `.yuleosh/reports/*` 变更已恢复，工作区恢复干净。

## 1. 验收矩阵（逐项）

| # | 评审项 | 结论 | 证据链 |
|:-:|:-------|:----:|:-------|
| 1 | Gap 数字 13/3/2 可复现 | ✅ PASS | `yuleosh ev check` 独立复跑 exit 0：完全就绪 13 / 部分 3 / 缺失 2；与 `.osh/evidence/aspice-gap-report.md` (18:33) 一致 |
| 2 | SRS 真实性（40 REQ + SHALL + 属性） | ✅ PASS | `docs/software-requirements.md` 436 行：40 个唯一 REQ 头，逐条含 SHALL（193 处）与 域/优先级/状态/ASIL/来源 属性；脚本校验 0 条缺 SHALL、0 条缺属性 |
| 3 | 机器可读表可解析 | ✅ PASS | `specs/requirements-shall-table.md` 40 行，`\| ID \| SHALL 语句 \| ASIL \| 范围 \|` 标准 markdown 表 |
| 4 | SRS ↔ 上游体系一致 | ✅ PASS | `specs/requirements-index.md`：9 RS + 31 SWR = 40；SRS §4 映射表 40 条全对上（RS-001~009、SWR-DKC/HUB/PRO/EMB/FE）；REQ-001 SHALL 与 RS-001 逐条一致 |
| 5 | 架构文档覆盖组件边界/接口/数据流 | ✅ PASS | `docs/architecture.md` §3 组件边界 / §4 接口 / §5 数据流（绑定/控车/分享/签名）/ §6 需求映射 / §10 诚实披露缺口 |
| 6 | include/ 契约头真实 | ✅ PASS | `include/dk_hal.h`、`dk_interfaces.h`、`dk_protocol.h` 已跟踪，内容为真实契约（消息信封/类型码/枚举/范围），非空壳 |
| 7 | 头文件盘点 41 个 | ⚠️ PASS（带误差） | 实际 41 = embedded 38 + firmware 1 + freestanding 2；但 §4.3 表格 freertos 行写 "8 个" 实为 9 个，"9 处 include 目录" 含非 include 目录的 freestanding_includes |
| 8 | 集成测试真实可跑（skip→pass） | ✅ PASS | `pytest tests/integration` 独立复跑 **53 passed**；归档佐证：layer2-2107f7f integration-tests=skipped → layer2-2c1f7c4=passed |
| 9 | HIL 37/37 文件支撑 | ✅ PASS | `tests/hil/reports/hil-report-20260716-123152.json` summary total=37 passed=37 100%；4 次运行原始报告均已跟踪（tests/hil/reports/ + docs/hil/evidence/）；`pytest tests/hil --collect-only` 收集 37 用例 |
| 10 | HIL 失败分析闭环 | ✅ PASS | `docs/hil/HIL-TEST-RESULTS.md` §3：run#3 的 HIL-UL-01（BLE 唤醒延迟）、HIL-NFC-04（场强校准）→ 根因 → 修复 → run#4 37/37 回归通过；原始 JSON failures 字段 ID 与文档一致 |
| 11 | 追溯 40/40 Covers | ✅ PASS | tests 中 `Covers: REQ-xxx` 覆盖 40 个唯一 REQ；`traceability-matrix.json` 40 条 REQ 全部 matched_tests 非空 |
| 12 | CI 三层全绿 | ✅ PASS | `yuleosh ci run 1/2/3` 独立复跑均 exit 0；L1 go-build/vet/test + MISRA 0 blocking；L2 integration-tests passed；L3 evidence pack 生成 |
| 13 | 无编造/占位符 | ✅ PASS | 新文档无 TODO/TBD/lorem；仅 impact-analysis.md 的 IA-XXX 属模板；reviews 有实质内容（AR-01~04、DR-01~02） |
| 14 | 契约 Done 标准 D1-D8 | ❌ FAIL（D1） | D2/D3/D4/D5/D6/D7/D8 达成；**D1 未达成**：要求 SWE.1 ≥2 转绿且 "BP1 SRS + BP2 结构化必过"，实际 BP1 ⚠️部分 / BP2 ❌缺失，仅 BP3 绿 |

## 2. 问题清单

### P0 — 无

### P1

- **P1-1 | D1 未达成 + checker 识别缺口**：`yuleosh ev` 对 SWE.1.BP1（REQ 唯一标识）、BP2（功能域组织/属性）、SWE.3.BP1（单一职责/<50 行）、SWE.5.BP1（stubs/drivers）、SWE.6.BP2（证据归档）全部报 `unknown check type — not recognized` 或朴素路径检查（`src/` 空目录）。**证据实际存在**（手工核验 SRS 40 REQ 全含 SHALL+属性、`docs/integration-strategy.md`、`docs/aspice/SWE.5-software-detailed-design.md` 均在），但工具不识别 → gap 报告长期显示 "2 缺失"。commit 声称 "P0 全完成" 与合同 D1 不符。
  - 修复建议：a) 扩展 yuleosh check 类型，支持解析 `docs/software-requirements.md`（REQ 头 + 属性表）与 `specs/requirements-shall-table.md`；b) 若工具不在本 sprint 范围，则在 gap report 与验收报告中显式记录工具限制，并把 D1 验收口径改为"证据存在性"（需小明裁决）。
- **P1-2 | 聚合覆盖率报告 FAIL**：`.osh/evidence/requirement-coverage.md`（L3 复跑可重现）显示 `Requirement Coverage: 44/184 (24%)`、Threshold 100%、**Pass: ❌**。根因：工具索引了遗留 SHALL ID（KL-/PE-/NF-/RC-/ES-/KS-/KR-/RA-/KSS-/CM-/OT-/UA-/AL-/DP-/OM-SHALL-xx 共 140 条）且无测试映射；traceability-matrix.md 对遗留 ID 同时出现 "✅ Covered" 与 "Test files: 0 ❌" 矛盾行。REQ-001~040 本身 40/40 ✅。
  - 修复建议：将遗留 SHALL ID 与 REQ-xxx 建立映射表（或标记 deprecated 排除出索引），重跑 traceability/evidence pack 至 Pass: ✅；消除矩阵中的矛盾状态行。
- **P1-3 | QTS E3 "14 场景 100%" 不可复现/无归档支撑**：`docs/qualification-strategy.md` §3 声称 E3 14/14=100%，但：归档 `integration-report.html` 仅 10 个场景且全部 **0µs**（旧 commit 产物，疑似 mock 直出）；实际套件 16 个测试函数（编号 01~14 且有 e2e_06 重复）；顶层 hub 包（gRPC/login）需运行中服务，当前环境 **FAIL**（dial localhost:9090 超时），`run_integration.sh` 自己明示 Go E2E "best-effort，失败不阻塞"。
  - 修复建议：统一 E3 数字（16 函数/15 编号并修正 06 重复）；QTS 注明 E3 运行前提（需 hub 实例）与 best-effort 属性；归档一次真实绿跑的 go-integration.log。

### P2

- P2-1 | SRS §5 属性汇总与逐条不符：声称 P0 25/P1 11/P2 4，实际逐条 P0 25/**P1 14**/**P2 1**（Approved 22/Implemented 18 正确）。
- P2-2 | QTS §3 E2 集成契约层写 "26 用例"，实际 `pytest tests/integration` 收集 **53**。
- P2-3 | architecture.md §4.3：freertos 行 "8 个" 实为 9；"9 处 include 目录" 表述含 freestanding_includes（非 include/ 目录），建议改为 "8 处 include/ + freestanding 头"。
- P2-4 | acceptance-matrix.md 40/40 状态全 ✅，而 QTS 中 REQ-004/005/031/032/035/036/037 为 🟡/⬜ — "状态"列语义（映射存在 vs 已验证）未定义，易被误读为全量验证通过；建议加图例或对齐状态。
- P2-5 | `docs/impact-analysis.md` 引用 `docs/impact-analysis-log.md` 不存在（IA-001 记录内联在本文档 §3）— 建文件或改引用。
- P2-6 | SRS 头部 "上游 RS-xxx 体系 40 条" 表述不精确（实为 9 RS + 31 SWR）。
- P2-7 | sprint contract 头部 "MISRA 0 违规" 基线 vs 当前 misra-trend 为 1 条 unknown（nofile:0，cppcheck 信息级，0 required/advisory blocking，自 2107f7f 起存在）— 非回归但表述过时。
- P2-8 | 集成套件 e2e_06 编号重复（ccc_remote_control / ota_update 两个文件同名 06）。

## 3. 评分（/100）

| 维度 | 权重 | 得分 | 依据 |
|:-----|:----:|:----:|:-----|
| 证据真实性（抽查无编造） | 25% | 9.0 | 40 REQ/HIL 37/HIL 失败分析/集成 53 全真实可复现 |
| Gap 数字可复现 | 15% | 10 | 13/3/2 独立复跑一致 |
| CI 全绿 | 10% | 10 | L1/L2/L3 独立复跑 exit 0 |
| 追溯完整性 | 15% | 7.5 | REQ 40/40 ✅，但聚合报告 24% FAIL |
| 文档一致性 | 15% | 6.0 | 多处数字不一致（优先级/E2/E3/头文件/06 重复） |
| 契约验收达成 | 20% | 6.5 | D2-D8 达成，D1 未达成（工具识别缺口） |
| **合计** | | **80** | |

## 4. 结论

**⚠️ 有条件通过。** 核心声称（13/3/2、HIL 37/37、integration skip→pass、40/40 REQ 追溯、CI 三层全绿）全部独立复现属实，无编造证据，证据链可追溯。但 **P1-1（D1 未达成 + checker 不识别）** 与 **P1-2（证据包内覆盖率报告自报 FAIL）** 不修复，CL2 证据包不能对外声称"就绪"；P1-3 的 E3 数字需对账。P1 修复后转"通过"。

- **保留项（TRACKING）**：AR-01 BSW 集成、AR-02 嵌入式错误码、AR-04 VFB 未定义、REQ-004/005/031/032/035/036/037 验收 🟡/⬜ — 均为文档已诚实披露的后续 Sprint 项，不构成本 sprint 阻塞。
- 评审期间工作区已恢复干净（仅 gitignore 的生成物被刷新，无跟踪文件变更）。
