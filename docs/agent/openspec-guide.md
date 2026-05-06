---
title: OpenSpec Usage Guide
owner: QClaw
last_reviewed: 2026-04-07
---

# OpenSpec Usage Guide

本指南整合 OpenSpec 规格驱动开发与 Harness Engineering 基础设施。

## 安装和初始化

```bash
# 安装 CLI
npm install -g @fission-ai/openspec@latest

# 在项目中初始化
cd /path/to/project
openspec init --tools claude

# 更新
openspec update
```

## 核心工作流

```
new → plan → apply → verify → archive
```

## 目录结构

```
workspace/
├── openspec/
│   ├── config.yaml          # 项目配置（默认 schema、上下文、规则）
│   ├── specs/               # 真值 — 当前系统行为
│   ├── changes/             # 活跃变更
│   │   └── <change-name>/
│   │       ├── .openspec.yaml
│   │       ├── proposal.md
│   │       ├── specs/       # Delta specs（变更内容）
│   │       ├── design.md
│   │       └── tasks.md
│   └── schemas/             # 自定义 schemas
```

## 整合配置

### config.yaml

```yaml
schema: spec-driven
context: |
  ## 技术栈
  - Runtime: Node.js, Python
  - Testing: Jest, Pytest
  - Framework: OpenClaw skills system

  ## 开发工作流
  - 使用 Superpowers 技能进行 brainstorm 和规划
  - 使用 Harness Engineering 模式组织文档
  - 所有变更必须遵循 OpenSpec 规格驱动流程

  ## 文档位置
  - 设计文档: docs/plans/
  - 架构文档: docs/agent/
  - 活跃变更: openspec/changes/

  ## 质量门禁
  - 所有代码变更必须有测试
  - 所有文档必须有 owner 和 last_reviewed
  - 提交前运行 agent_repo_check.py

rules:
  proposal:
    - 必须包含回滚计划
    - 必须关联 docs/plans/ 设计文档
  specs:
    - 使用 Given/When/Then 格式
    - 每个 spec 必须可测试
  tasks:
    - 每个任务 2-5 分钟可完成
    - 必须包含测试步骤
```

## Agent 使用流程

### 1. 检查项目状态

```bash
# 活跃变更
openspec list --json

# 当前规格
openspec list --specs --json

# 可用 schemas
openspec schemas --json
```

### 2. 创建变更

```bash
# 使用默认 schema
openspec new change <name>

# 指定 schema
openspec new change <name> --schema tdd-driven
```

### 3. 创建 Artifacts

**按顺序执行**:

```bash
# 获取下一个 artifact 的指令
openspec instructions --change <name> --json

# 检查进度
openspec status --change <name> --json
```

**Artifacts 顺序**:
1. `proposal.md` — 为什么和做什么
2. `specs/` — Given/When/Then 场景
3. `design.md` — 技术方案和决策
4. `tasks.md` — 实现清单

### 4. 实现

从 `tasks.md` 读取任务，逐个完成，标记 `[x]`。

### 5. 验证和归档

```bash
# 验证完整性
openspec validate --change <name> --json

# 归档（合并 delta specs 到主 specs）
openspec archive <name> --yes
```

## Spec 格式

使用 RFC 2119 关键词 (SHALL/MUST/SHOULD/MAY):

```markdown
### Requirement: User Authentication

The system SHALL issue a JWT token upon successful login.

#### Scenario: Valid credentials
- GIVEN a user with valid credentials
- WHEN the user submits login form
- THEN a JWT token is returned
- AND the token expires in 24 hours

#### Scenario: Invalid credentials
- GIVEN a user with invalid credentials
- WHEN the user submits login form
- THEN an error is returned
- AND no token is issued
```

## Delta Specs

变更不会重写规格，而是描述增量:

| 操作 | 说明 |
|------|------|
| `ADDED` | 新增的行为 |
| `MODIFIED` | 修改的行为 |
| `REMOVED` | 删除的行为 |

归档时自动合并到 `openspec/specs/`。

## 与 Superpowers 整合

### Brainstorm 阶段
- Superpowers 创建 `docs/plans/YYYY-MM-DD-<topic>-design.md`
- OpenSpec 的 `proposal.md` 引用该设计文档

### Tasks 阶段
- OpenSpec 的 `tasks.md` 成为 Subagent 执行的输入
- 每个 task 符合 2-5 分钟原则

### 验证阶段
- OpenSpec validate 检查规格完整性
- Superpowers 的 spec-reviewer 验证代码符合规格

## CLI 快速参考

| 命令 | 用途 |
|------|------|
| `openspec list [--specs] [--json]` | 列出变更或规格 |
| `openspec show <name> [--json]` | 显示详情 |
| `openspec status --change <name>` | Artifact 完成状态 |
| `openspec instructions --change <name>` | 获取创建指令 |
| `openspec validate <name>` | 验证变更 |
| `openspec archive <name>` | 归档变更 |
| `openspec schemas` | 列出可用 schemas |

**始终使用 `--json` 以便程序化处理**

## 自定义 Schema

### 创建 TDD-driven Schema

```bash
# 从内置 schema 创建
openspec schema fork spec-driven tdd-driven

# 或从零创建
openspec schema init my-workflow
```

Schema 文件位于 `openspec/schemas/<name>/schema.yaml`。
