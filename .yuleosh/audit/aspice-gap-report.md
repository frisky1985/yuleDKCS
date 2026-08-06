# 🔍 yuleOSH ASPICE Compliance Gap Check

> **项目**: `/Users/stefan/.openclaw/workspace/yuleDKCS`
> **标准**: ASPICE v3.1
> **生成时间**: 2026-08-06T18:32:28.710973

---

## 📊 概要

| 指标 | 数量 |
|:-----|-----:|
| 总 BP 数 | 18 |
| ✅ 完全就绪 | 13 |
| ⚠️  部分就绪 | 3 |
| ❌ 缺失/未开始 | 2 |

🚩 **5 个 Base Practice 尚待补齐** — 详见下方逐项检查

---

## SWE.1: Software Requirements Analysis
> Transform system requirements into a structured set of software requirements.

**状态**: 2/3 BP 尚待补齐

### SWE.1.BP1: Specify software requirements

**状态**: ⚠️  部分就绪 (1/3 项未通过)

**❌ 缺失项**:

- Missing evidence: Alternative requirements document
- Check: Each requirement has a unique identifier (REQ-xxx) (unknown check type — not recognized)

**✅ 修复步骤**:

- 📄 创建 `docs/software-requirements.md` 或 `docs/requirements.md`
- 🔖 每条需求分配唯一标识符（REQ-xxx），包含 SHALL 语句
- 🔗 将每条需求追溯至系统需求

**💡 相关 CLI 命令**:

- `yuleosh spec validate docs/requirements.md` — 验证需求文件
- 查阅 `yuleosh --help` 获取全部命令

### SWE.1.BP2: Structure software requirements

**状态**: ❌ 缺失 (2/2 项未通过)

**❌ 缺失项**:

- Check: Requirements are organized by functional area
- Check: Requirements have defined attributes (priority, status) (unknown check type — not recognized)

**✅ 修复步骤**:

- 📂 在 `specs/` 目录下按功能区域组织需求文件
- 🏷️  为每条需求定义属性（优先级、状态）

**💡 相关 CLI 命令**:

- `yuleosh spec diff old.md new.md` — 对比需求变更
- 查阅 `yuleosh --help` 获取全部命令

### SWE.1.BP3: Evaluate impact of requirements

**状态**: ✅ 已就绪 (2/2)

---

## SWE.2: Software Architectural Design
> Establish a software architectural design that identifies components,
their interfaces, and data flow.

**状态**: 0/3 BP 尚待补齐

### SWE.2.BP1: Develop software architecture

**状态**: ✅ 已就绪 (2/2)

### SWE.2.BP2: Define interfaces

**状态**: ✅ 已就绪 (2/2)

### SWE.2.BP3: Verify architecture

**状态**: ✅ 已就绪 (2/2)

---

## SWE.3: Software Detailed Design and Unit Construction
> Develop a detailed design for each software component and construct units.

**状态**: 1/3 BP 尚待补齐

### SWE.3.BP1: Develop detailed design

**状态**: ❌ 缺失 (3/3 项未通过)

**❌ 缺失项**:

- Missing evidence: Source code directory
- Check: Source code follows defined coding standards
- Check: Each function has a clear, single responsibility
- Check: Code complexity is managed (functions < 50 lines)

**✅ 修复步骤**:

- 💻 确保 `src/` 目录包含全部源代码
- 📏 遵循已定义的编码规范（.clang-format / pyproject.toml）
- ✂️  每个函数保持单一职责，鼓励函数 < 50 行

**💡 相关 CLI 命令**:

- `yuleosh review auto` — 自动审查代码
- 查阅 `yuleosh --help` 获取全部命令

### SWE.3.BP2: Define unit test cases

**状态**: ✅ 已就绪 (3/3)

### SWE.3.BP3: Verify detailed design

**状态**: ✅ 已就绪 (2/2)

---

## SWE.4: Software Unit Verification
> Verify software units against the detailed design and requirements.

**状态**: 0/3 BP 尚待补齐

### SWE.4.BP1: Perform unit verification

**状态**: ✅ 已就绪 (3/3)

### SWE.4.BP2: Establish bidirectional traceability

**状态**: ✅ 已就绪 (2/2)

### SWE.4.BP3: Evaluate unit verification results

**状态**: ✅ 已就绪 (2/2)

---

## SWE.5: Software Integration and Integration Test
> Integrate software units and verify the integrated software against
the architecture and requirements.

**状态**: 1/3 BP 尚待补齐

### SWE.5.BP1: Develop integration strategy

**状态**: ⚠️  部分就绪 (1/2 项未通过)

**❌ 缺失项**:

- Check: Stubs/drivers are identified (unknown check type — not recognized)

**✅ 修复步骤**:

- 📄 创建 `docs/integration-strategy.md`，定义集成序列
- 🧩 识别所需的桩/驱动（stubs/drivers）

**💡 相关 CLI 命令**:

- `yuleosh coverage c` — 查看集成覆盖率
- 查阅 `yuleosh --help` 获取全部命令

### SWE.5.BP2: Integrate software units

**状态**: ✅ 已就绪 (2/2)

### SWE.5.BP3: Perform integration tests

**状态**: ✅ 已就绪 (3/3)

---

## SWE.6: Software Qualification Test
> Test the complete software against software requirements in the target
environment or a simulated environment.

**状态**: 1/3 BP 尚待补齐

### SWE.6.BP1: Develop qualification test strategy

**状态**: ✅ 已就绪 (2/2)

### SWE.6.BP2: Perform qualification tests

**状态**: ⚠️  部分就绪 (1/3 项未通过)

**❌ 缺失项**:

- Check: Resulting evidence is archived (unknown check type — not recognized)

**✅ 修复步骤**:

- 🧪 运行全部合格性测试并确认通过
- 🎯 在目标环境或等效环境中执行测试
- 📦 归档测试证据（运行 `yuleosh audit evidence`）

**💡 相关 CLI 命令**:

- `yuleosh audit evidence` — 归档测试证据
- 查阅 `yuleosh --help` 获取全部命令

### SWE.6.BP3: Establish traceability

**状态**: ✅ 已就绪 (2/2)

---

## 🚀 快速启动

```bash
# 生成证据包（补齐多数 BP 证据）
yuleosh evidence pack

# 生成 CL2 审计证据包
yuleosh audit evidence

# 查看覆盖趋势
yuleosh coverage trend

# 查看 MISRA 违规趋势
yuleosh misra trend

# 查看 KPI 仪表盘
yuleosh kpi status
```

---
*报告由 yuleOSH ASPICE Gap Check 生成*