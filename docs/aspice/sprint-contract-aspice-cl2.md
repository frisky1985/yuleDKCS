# Sprint Contract — yuleDKCS ASPICE CL2 证据补齐

> **Planner**: 小明 (Orchestrator) | **Generator**: 小克 👨‍💻 | **Evaluator**: 小马 🐴
> **日期**: 2026-08-06 | **基线**: master @ 2808c69（三层 CI 全绿，MISRA 0 违规）
> **依据**: `.osh/evidence/aspice-gap-report.md`（18 BP：1 就绪 / 5 部分 / 12 缺失）

---

## 🎯 Done 标准（验收定义）

重跑 `yuleosh ev` 后满足以下全部条件，本 Sprint 才算完成：

| # | 验收项 | 当前 | 目标 |
|:-:|:-------|:----:|:----:|
| D1 | SWE.1 需求分析 BP1/BP2/BP3 | 全缺失 | ≥2 转绿（BP1 SRS + BP2 结构化必过） |
| D2 | SWE.2 架构设计 BP1/BP2/BP3 | 全缺失 | ≥2 转绿（BP1 架构文档 + BP2 接口定义必过） |
| D3 | SWE.3 详细设计 BP1/BP2/BP3 | 全缺失 | ≥1 转绿（BP1 代码规范可基于已有证据） |
| D4 | SWE.4 单元验证 BP1/BP2/BP3 | 2部分+1缺失 | 3 项全部转绿或 ≥2 转绿 + 剩余部分 |
| D5 | SWE.5 集成 BP3 | 缺失 | 转绿（tests/integration/ 建立） |
| D6 | SWE.6 合格性 BP1/BP2/BP3 | 1部分+2缺失 | ≥2 转绿（策略 + SIL/HIL 结果归档） |
| D7 | 证据包 | 已生成 | `yuleosh evidence pack` + `yuleosh audit evidence` 重新生成且包含新证据 |
| D8 | 仓库状态 | — | 全部产物 commit + push origin/master，工作区干净 |

**总目标**: 缺失 BP 从 12 降到 ≤5，完全就绪 BP ≥6。

---

## 📋 工作分解（P0 优先，P1 尽力）

### P0-1: SRS 文档（SWE.1.BP1/BP2/BP3）
- 创建 `docs/software-requirements.md`：从 `specs/requirements-index.md`（已有 RS-xxx 体系）+ 8 个 spec 文件提炼
- 每条需求 **REQ-xxx 唯一标识 + SHALL 语句**，按功能域分组（DKCS Core / Hub / Protocol / Embedded / Frontend），带属性（优先级/状态）
- 创建 `docs/impact-analysis.md`：需求变更影响分析模板 + 首版记录（进度/资源/风险）

### P0-2: 架构文档 + 接口定义（SWE.2.BP1/BP2）
- 创建 `docs/architecture.md`：复用 `docs/design/DK-HUB-ARCHITECTURE.md`(303行) + `docs/design/HUB-DETAILED-DESIGN.md`(1012行) + `docs/aspice/SWE.4-software-arch.md` 提炼，覆盖组件边界/接口/数据流
- 接口头文件盘点：`firmware/include/`、`embedded/*/include/` 已有 9 处，整理成接口清单写入架构文档；缺的补 `include/` 汇总层

### P0-3: 集成测试（SWE.5.BP3）
- 创建 `tests/integration/`：从 `tests/e2e/`（已有 scenarios/client）提取组织，覆盖组件接口与数据流
- 接入 CMake/pytest，纳入 `yuleosh ci run 2`（integration-tests 目前 skip）

### P0-4: 合格性测试策略 + SIL/HIL 归档（SWE.6.BP1/BP2）
- 创建 `docs/qualification-strategy.md`：覆盖全部需求，逐条验收标准
- SIL/HIL 结果归档：`tests/hil/reports/` + `docs/hil/HIL-TEST-PLAN.md` 已有内容，整理为证据归档

### P0-5: 追溯矩阵（SWE.4.BP2 / SWE.6.BP3）
- 需求→单测→合格性测试双向追溯：跑 `yuleosh traceability matrix` / `yuleosh evidence pack` 重新生成
- 缺失的 Covers 标记补到测试文件

### P1: 质量项（尽力而为）
- 单测 100% 通过（当前 CI L1 go-test 已绿）、语句覆盖率 ≥80%、分支 ≥70%
- 架构/设计审查记录 `docs/architecture-review.md` / `docs/design-review.md`（复用 docs/design/CODE-REVIEW-V2.md）
- 失败测试分析 + 回归测试策略（ci-config.yaml）

---

## 🚫 约束

1. **只读分析 + 证据生成**：除上述文档/测试/追溯产物外，不改产品代码逻辑
2. 复用优先：现有 docs/design/、docs/aspice/、specs/ 内容提炼落位，不重复造轮子
3. 工作区：`/Users/stefan/.openclaw/workspace/yuleDKCS`（master 分支），唯一写者
4. 完成后自动 commit + push origin/master，工作区干净
5. 每个 P0 完成时跑一次 `yuleosh ev` 记录 gap 变化，最终报告附前后对比

## 📤 交付物

- 全部文档/测试/证据产物（已 push）
- 最终验收报告：`yuleosh ev` 前后 gap 对比表 + 各 BP 状态变化 + 遗留项清单
- 写 checkpoint 到小克 memory
