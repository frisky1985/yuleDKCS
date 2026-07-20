# AI 辅助工程最佳实践培训手册

> 整合 Harness Engineering + Superpowers + OpenSpec 的完整开发体系

**版本**: 1.0.0  
**日期**: 2026-04-07  
**作者**: QClaw

---

## 📋 目录

1. [概述](#概述)
2. [核心技能介绍](#核心技能介绍)
3. [安装与配置](#安装与配置)
4. [工程体系结构](#工程体系结构)
5. [完整工作流程](#完整工作流程)
6. [最佳实践指南](#最佳实践指南)
7. [实战案例](#实战案例)
8. [故障排查](#故障排查)
9. [参考资源](#参考资源)

---

## 概述

### 背景

随着 AI 辅助开发的普及，如何让 AI Agent 更高效、更可靠地参与软件开发成为关键问题。本培训手册介绍一套经过验证的最佳实践体系，整合三大核心技能：

- **Harness Engineering** - 让仓库对 AI 可读、可维护
- **Superpowers** - 规范开发流程，确保质量
- **OpenSpec** - 结构化变更管理，可追溯

### 核心理念

```
┌─────────────────────────────────────────────────────────────┐
│                    三层协作模型                               │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  基础设施层 (Harness) ─── 让仓库对 AI 可读                    │
│         ↓                                                    │
│  工作流层 (Superpowers) ─── 规范开发流程                      │
│         ↓                                                    │
│  规格层 (OpenSpec) ─── 结构化变更管理                        │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 适用场景

| 场景 | 使用方式 |
|------|---------|
| 新项目初始化 | 使用 Harness 创建工程基础设施 |
| 功能开发 | 遵循 Superpowers 工作流 |
| Bug 修复 | 系统化调试流程 |
| 团队协作 | OpenSpec 规格驱动 |
| 知识沉淀 | 渐进式文档披露 |

---

## 核心技能介绍

### 1. Harness Engineering

**定位**: 基础设施层

**核心思想**:
将 OpenAI 的 harness-engineering 文章转化为可复用的项目模式，让 AI Agent 更容易理解、修改和维护代码仓库。

**关键原则**:

| 原则 | 说明 |
|------|------|
| AGENTS.md 是路由器 | 保持简短，链接到详细文档 |
| 知识沉淀在 docs/ | 中长期知识存储在结构化文档中 |
| 渐进式披露 | 只加载当前任务需要的文档 |
| 机械检查 | 用脚本替代"记住" |

**产出物**:

```
project/
├── AGENTS.md              # 总入口（路由器）
├── docs/
│   └── agent/             # AI 可读文档
│       ├── index.md       # 导航索引
│       ├── architecture.md
│       ├── specs.md
│       ├── quality.md
│       └── ...
└── scripts/
    ├── agent_repo_check.py    # 质量检查
    └── agent_gc_report.py     # 垃圾回收
```

**何时使用**:
- 新项目初始化
- 现有项目工程化改造
- 文档体系重构

---

### 2. Superpowers

**定位**: 工作流层

**核心思想**:
强制性的软件开发工作流，确保每个步骤都有质量保证。

**五阶段流程**:

```
Phase 1: Brainstorming
    ↓ 探索、提问、设计方案
Phase 2: Writing Plans
    ↓ 详细任务拆分
Phase 3: Subagent-Driven Development
    ↓ TDD 实现
Phase 4: Code Review
    ↓ 规格 + 质量审查
Phase 5: Finish Branch
    → 测试、合并、清理
```

**HARD GATES** (强制门禁):

1. **禁止在用户批准设计前编写代码**
2. **禁止在没有根本原因分析前修复 bug**
3. **禁止在没有测试的情况下提交代码**

**TDD 原则**:
```python
# 正确流程
1. 写失败测试
2. 写最小实现使测试通过
3. 重构优化
4. 提交

# 错误流程
❌ 先写实现再补测试
```

---

### 3. OpenSpec

**定位**: 规格层

**核心思想**:
通过结构化的变更管理，让所有代码变更有迹可循、有据可查。

**变更流程**:

```
new → plan → apply → verify → archive
```

**Artifacts 序列**:

| Artifact | 内容 | 格式 |
|----------|------|------|
| `proposal.md` | 为什么做、做什么 | 背景、目标、范围 |
| `specs/` | 行为规格 | Given/When/Then |
| `design.md` | 技术方案 | 架构、接口、数据 |
| `tasks.md` | 实现清单 | 带复选框的任务列表 |

**Spec 格式示例**:

```markdown
### Requirement: User Authentication

The system SHALL issue a JWT token upon successful login.

#### Scenario: Valid credentials
- GIVEN a user with valid credentials
- WHEN the user submits login form
- THEN a JWT token is returned
- AND the token expires in 24 hours
```

**Delta Specs**:
- 变更不会重写规格，而是描述增量 (ADDED/MODIFIED/REMOVED)
- 归档时自动合并到主规格

---

## 安装与配置

### 前置要求

- Node.js v22+
- Python 3.10+
- Git
- OpenClaw/QClaw 环境

### 步骤 1: 安装核心技能

```bash
# 通过 SkillHub 安装
skillhub install openai-whisper
skillhub install github
skillhub install ontology
skillhub install gog
skillhub install tavily-search
skillhub install summarize
skillhub install agent-browser

# 通过 ClawHub 安装
clawhub install agent-harness-engineering
clawhub install agent-security-harness
clawhub install superpowers
clawhub install openspec
clawhub install using-harness
```

### 步骤 2: 安装 OpenSpec CLI

```bash
npm install -g @fission-ai/openspec@latest

# 在项目中初始化
cd /path/to/project
openspec init --tools claude
```

### 步骤 3: 创建工程结构

**方式 A: 使用 Harness 脚本**

```bash
python scripts/bootstrap_project.py --mode overlay
```

**方式 B: 手动创建**

创建以下目录结构:

```
project/
├── AGENTS.md
├── docs/
│   ├── agent/
│   │   ├── index.md
│   │   ├── architecture.md
│   │   ├── specs.md
│   │   ├── quality.md
│   │   ├── reliability.md
│   │   ├── security.md
│   │   ├── superpowers-workflow.md
│   │   └── openspec-guide.md
│   └── plans/
├── openspec/
│   ├── config.yaml
│   ├── changes/
│   ├── specs/
│   └── schemas/
├── scripts/
├── memory/
├── MEMORY.md
├── USER.md
└── SOUL.md
```

### 步骤 4: 配置 OpenSpec

创建 `openspec/config.yaml`:

```yaml
schema: spec-driven
context: |
  ## 技术栈
  - Runtime: Node.js, Python
  - Testing: Jest, Pytest
  
  ## 开发工作流
  - 使用 Superpowers 技能进行 brainstorm 和规划
  - 使用 Harness Engineering 模式组织文档
  - 所有变更必须遵循 OpenSpec 规格驱动流程
  
  ## 文档位置
  - 设计文档: docs/plans/
  - 架构文档: docs/agent/
  - 活跃变更: openspec/changes/

rules:
  proposal:
    - 必须包含背景和目标
    - 必须包含回滚计划
  specs:
    - 使用 Given/When/Then 格式
    - 每个 spec 必须可测试
  tasks:
    - 每个任务 2-5 分钟可完成
    - 必须包含测试步骤
```

---

## 工程体系结构

### 目录结构详解

```
project/
├── AGENTS.md              # 📍 总入口（路由器）
│                          # 保持简短，链接到详细文档
│
├── docs/
│   ├── agent/             # 🏗️ 工程文档（Harness 管理）
│   │   ├── index.md       # 导航索引 - 所有文档的入口
│   │   ├── architecture.md # 架构边界和约束
│   │   ├── specs.md       # 规格总览
│   │   ├── quality.md     # 质量标准和检查规则
│   │   ├── reliability.md # 可靠性要求
│   │   ├── security.md    # 安全边界
│   │   ├── superpowers-workflow.md # 开发工作流详解
│   │   └── openspec-guide.md       # OpenSpec 使用指南
│   │
│   └── plans/             # 📐 设计文档（Superpowers 管理）
│       └── YYYY-MM-DD-<topic>-design.md
│
├── openspec/              # 📋 规格管理（OpenSpec 管理）
│   ├── config.yaml        # 项目配置
│   ├── changes/           # 活跃变更
│   │   └── <change-name>/
│   │       ├── proposal.md
│   │       ├── specs/
│   │       ├── design.md
│   │       └── tasks.md
│   ├── specs/             # 系统规格（真值）
│   └── schemas/           # 自定义 schema
│
├── scripts/               # 🔧 工具脚本
│   ├── agent_repo_check.py    # 质量检查
│   └── agent_gc_report.py     # 垃圾回收报告
│
├── memory/                # 💭 日志记忆
│   └── YYYY-MM-DD.md
│
├── MEMORY.md              # 🧠 长期记忆（仅 main session）
├── USER.md                # 👤 用户信息
├── SOUL.md                # 🎭 Agent 人格
└── TOOLS.md               # 🛠️ 工具笔记
```

### 三层协作关系

```
┌─────────────────────────────────────────────────────────────┐
│                    AGENTS.md (路由器)                        │
│                    "读我导航到其他文档"                       │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                 docs/agent/ (工程知识)                       │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ architecture │  │   workflow   │  │   quality    │       │
│  │   架构边界    │  │   工作流程    │  │   质量标准    │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
│                                                              │
│  特点: 渐进式披露 - 只加载当前任务需要的文档                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│              openspec/changes/ (活跃变更)                    │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  proposal    │→ │    specs     │→ │    tasks     │       │
│  │  提案文档     │  │   行为规格    │  │   实现任务    │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
│                                                              │
│  特点: 结构化变更 - 所有改动有迹可循                          │
└─────────────────────────────────────────────────────────────┘
```

### 文档责任矩阵

| 文档 | 负责技能 | 更新时机 | 审阅周期 |
|------|---------|---------|---------|
| AGENTS.md | Harness | 新增文档链接 | 30 天 |
| docs/agent/*.md | Harness | 架构/流程变更 | 90 天 |
| docs/plans/*.md | Superpowers | 新功能设计 | 功能完成 |
| openspec/changes/ | OpenSpec | 变更开始 | 变更归档 |

---

## 完整工作流程

### Phase 1: 项目初始化

**触发**: 新项目或现有项目工程化改造

**执行步骤**:

```bash
# 1. 运行 Harness 脚本
python scripts/bootstrap_project.py --mode overlay

# 可选参数:
# --mode overlay  # 现有项目（默认）
# --mode full     # 全新项目
# --with-gc       # 启用垃圾回收
# --dry-run       # 预览变更
```

**产出**:

- ✅ AGENTS.md 作为路由器
- ✅ docs/agent/ 文档结构
- ✅ openspec/ 规格结构
- ✅ scripts/ 检查脚本

**验证**:

```bash
python scripts/agent_repo_check.py
```

---

### Phase 2: Brainstorming (功能设计)

**触发**: 用户说 "让我构建..."、"帮我规划..."、"我想添加 X..."

**执行流程**:

```
┌─────────────────────────────────────────────────────────────┐
│  Step 1: 探索项目上下文                                      │
├─────────────────────────────────────────────────────────────┤
│  - 读取 docs/agent/architecture.md                          │
│  - 读取 openspec/specs/ 了解当前行为                         │
│  - 检查最近 commits                                          │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 2: 提问澄清 (一次一个问题，多选题优先)                  │
├─────────────────────────────────────────────────────────────┤
│  例:                                                         │
│  "这个功能的主要用户是谁？"                                   │
│  A) 内部团队  B) 外部客户  C) 两者都有                       │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 3: 提出方案 (2-3 个)                                   │
├─────────────────────────────────────────────────────────────┤
│  方案 A: [描述] 优点: [...] 缺点: [...]                      │
│  方案 B: [描述] 优点: [...] 缺点: [...]                      │
│  推荐: [方案 X] 理由: [...]                                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 4: 呈现设计 (分段展示，每段等待确认)                    │
├─────────────────────────────────────────────────────────────┤
│  概述 → 用户流程 → 技术设计 → 边界情况                       │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 5: 写入设计文档                                        │
├─────────────────────────────────────────────────────────────┤
│  保存到: docs/plans/YYYY-MM-DD-<topic>-design.md            │
│  提交到 Git                                                  │
└─────────────────────────────────────────────────────────────┘
```

**HARD GATE**:
> 🚫 **禁止在用户批准设计前编写任何代码**

---

### Phase 3: OpenSpec 规格化

**触发**: 设计文档已获批准

**执行步骤**:

```bash
# 1. 创建变更
openspec new change user-authentication

# 2. 创建 proposal
openspec instructions proposal --change user-authentication --json
# 编写 proposal.md: 背景、目标、范围、回滚计划

# 3. 创建 specs
openspec instructions specs --change user-authentication --json
# 编写 specs/: Given/When/Then 场景

# 4. 创建 design
openspec instructions design --change user-authentication --json
# 编写 design.md: 技术方案、接口设计

# 5. 创建 tasks
openspec instructions tasks --change user-authentication --json
# 编写 tasks.md: 带复选框的任务列表

# 6. 检查进度
openspec status --change user-authentication --json
```

**tasks.md 示例**:

```markdown
# Implementation Tasks

## Backend
- [ ] Create User model with password hashing
- [ ] Implement JWT token generation
- [ ] Add login endpoint `/api/auth/login`
- [ ] Add logout endpoint `/api/auth/logout`

## Frontend
- [ ] Create login form component
- [ ] Implement token storage
- [ ] Add auth state management

## Testing
- [ ] Write unit tests for User model
- [ ] Write integration tests for auth endpoints
- [ ] Write E2E tests for login flow
```

---

### Phase 4: TDD 实现

**触发**: tasks.md 已创建

**执行循环** (每个 task):

```
┌─────────────────────────────────────────────────────────────┐
│  Step 1: 写失败测试                                          │
├─────────────────────────────────────────────────────────────┤
│  describe('User Authentication', () => {                    │
│    it('should return JWT token on valid login', () => {     │
│      // 还没有实现，测试会失败                                │
│    });                                                       │
│  });                                                         │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 2: 运行测试确认失败                                     │
├─────────────────────────────────────────────────────────────┤
│  npm test                                                    │
│  # 或 pytest                                                 │
│  # 确认测试失败                                               │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 3: 写最小实现使测试通过                                 │
├─────────────────────────────────────────────────────────────┤
│  // 只写足够让测试通过的代码                                  │
│  function login(credentials) {                               │
│    return generateToken(credentials);                        │
│  }                                                           │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 4: 重构优化                                            │
├─────────────────────────────────────────────────────────────┤
│  - 提取重复代码                                               │
│  - 改善命名                                                   │
│  - 添加错误处理                                               │
│  # 保持测试通过                                               │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 5: 提交                                                │
├─────────────────────────────────────────────────────────────┤
│  git add .                                                   │
│  git commit -m "feat: add user authentication"              │
└─────────────────────────────────────────────────────────────┘
```

**TDD 红绿重构循环**:

```
🔴 Red:   写失败测试
    ↓
🟢 Green: 写最小实现使测试通过
    ↓
🔵 Refactor: 重构优化，保持测试通过
    ↓
   提交
```

---

### Phase 5: 代码审查

**触发**: Task 实现完成

**审查流程**:

```
┌─────────────────────────────────────────────────────────────┐
│  Step 1: 规格审查 (Spec Reviewer)                            │
├─────────────────────────────────────────────────────────────┤
│  - 代码是否符合 OpenSpec specs?                              │
│  - 所有 Given/When/Then 场景是否覆盖?                        │
│  - 是否有遗漏的边界情况?                                      │
└─────────────────────────────────────────────────────────────┘
                              ↓
                        [通过?]
                       /        \
                     是          否
                      ↓           ↓
┌──────────────────────────┐  返回修复 → 重审
│  Step 2: 质量审查         │
│  (Quality Reviewer)       │
├──────────────────────────┤
│  - 代码可读性              │
│  - 无重复代码 (DRY)        │
│  - 无过度设计 (YAGNI)      │
│  - 错误处理完善            │
└──────────────────────────┘
                              ↓
                        [通过?]
                       /        \
                     是          否
                      ↓           ↓
┌──────────────────────────┐  返回修复 → 重审
│  Step 3: 自动检查         │
├──────────────────────────┤
│  python scripts/          │
│    agent_repo_check.py    │
│                           │
│  openspec validate        │
│    --change <name>        │
└──────────────────────────┘
                              ↓
                           [通过]
                              ↓
                       进入归档阶段
```

---

### Phase 6: 归档变更

**触发**: 所有 tasks 完成，所有审查通过

**执行步骤**:

```bash
# 1. 最终验证
openspec validate --change user-authentication --json

# 2. 归档（合并 delta specs 到主 specs）
openspec archive user-authentication --yes

# 3. 清理
git add openspec/
git commit -m "chore: archive user-authentication change"
```

**归档后的结构**:

```
openspec/
├── specs/                     # 真值（更新后）
│   ├── authentication.md      # 新增的规格
│   └── ...
└── archive/                   # 归档目录
    └── user-authentication/   # 已完成的变更
        ├── proposal.md
        ├── specs/
        ├── design.md
        └── tasks.md
```

---

## 最佳实践指南

### 渐进式披露原则

**错误做法**:
```
AI 启动时一次性加载所有文档 → token 浪费、上下文污染
```

**正确做法**:
```
AI 启动时只读 AGENTS.md 
    ↓
根据任务链接到特定文档
    ↓
只加载当前任务需要的文档
```

**示例**:

```markdown
<!-- AGENTS.md -->
# Quick Navigation

| 任务 | 文档 |
|------|------|
| 开发新功能 | [superpowers-workflow.md](docs/agent/superpowers-workflow.md) |
| 了解架构 | [architecture.md](docs/agent/architecture.md) |

<!-- AI 只读取相关文档，不加载所有 -->
```

---

### TDD 最佳实践

**常见错误**:

| 错误 | 后果 | 正确做法 |
|------|------|---------|
| 先写实现再补测试 | 测试设计被实现影响 | 先写测试，让实现适应测试 |
| 一次写太多测试 | 难以定位问题 | 一次一个测试场景 |
| 跳过重构 | 技术债务累积 | 每次提交前重构 |

---

### 文档维护

**Frontmatter 必填字段**:

```yaml
---
title: 文档标题
owner: 负责人
last_reviewed: 2026-04-07
---
```

**审阅周期**:

| 文档类型 | 审阅周期 | 触发条件 |
|---------|---------|---------|
| AGENTS.md | 30 天 | agent_repo_check.py 检查 |
| 架构文档 | 90 天 | agent_repo_check.py 检查 |
| 设计文档 | 功能完成时 | 变更归档 |
| 规格文档 | 每次变更 | openspec validate |

---

### 安全最佳实践

**信任边界**:

```
┌─────────────────────────────────────────┐
│          Trusted Zone                   │
│  - 本地文件系统                          │
│  - Bundled skills                       │
│  - 用户直接输入                          │
└─────────────────────────────────────────┘
                    │
                    │ 需要审批
                    ▼
┌─────────────────────────────────────────┐
│          External Zone                  │
│  - 网络请求                              │
│  - 外部 API                              │
│  - 未验证的 skill                        │
└─────────────────────────────────────────┘
```

**敏感数据处理**:

| 数据类型 | 处理方式 |
|---------|---------|
| API Keys | 环境变量，不入 Git |
| 用户数据 | 仅限 main session |
| MEMORY.md | 不暴露给 group chat |

---

## 实战案例

### 案例 1: 新项目初始化

**场景**: 创建一个新的微服务项目

**步骤**:

```bash
# 1. 初始化项目
mkdir my-service && cd my-service
git init

# 2. 运行 Harness 脚本
python /path/to/scripts/bootstrap_project.py --mode full --with-gc

# 3. 验证
python scripts/agent_repo_check.py

# 4. 初始化 OpenSpec
openspec init --tools claude

# 5. 提交
git add .
git commit -m "chore: initialize project with harness engineering"
```

**产出**:

```
my-service/
├── AGENTS.md
├── docs/
│   ├── agent/ (7 个文档)
│   └── plans/
├── openspec/
│   ├── config.yaml
│   ├── changes/
│   ├── specs/
│   └── schemas/
├── scripts/
│   ├── agent_repo_check.py
│   └── agent_gc_report.py
└── ...
```

---

### 案例 2: 开发用户认证功能

**场景**: 为现有项目添加用户认证

**完整流程**:

```
用户: "帮我开发用户认证功能"

AI Phase 1: Brainstorm
─────────────────────────────────────────────
AI: 让我了解一下项目...
    [读取 architecture.md, specs.md]
    
AI: 这个功能的主要用户是谁？
    A) 内部团队  B) 外部客户  C) 两者都有
用户: B

AI: 有两个方案：
    方案 A: JWT Token (推荐)
      优点: 无状态、易扩展
      缺点: Token 无法主动撤销
    方案 B: Session
      优点: 可控性强
      缺点: 需要存储
用户: 用方案 A

AI: [呈现设计] 概述 → 流程 → 技术细节
用户: 批准

AI: [写入] docs/plans/2026-04-07-auth-design.md
─────────────────────────────────────────────

AI Phase 2: OpenSpec
─────────────────────────────────────────────
$ openspec new change user-auth
$ openspec instructions proposal --change user-auth
[编写 proposal.md]
$ openspec instructions specs --change user-auth
[编写 specs/]
$ openspec instructions design --change user-auth
[编写 design.md]
$ openspec instructions tasks --change user-auth
[编写 tasks.md: 20 个任务]
─────────────────────────────────────────────

AI Phase 3: TDD 实现
─────────────────────────────────────────────
Task 1: [ ] Create User model
  🔴 写测试: User model should hash password
  🟢 实现: bcrypt.hash(password, salt)
  🔵 重构: 提取 hashPassword() 方法
  ✅ 提交

Task 2: [ ] Implement JWT generation
  🔴 写测试: should generate valid JWT
  🟢 实现: jwt.sign(payload, secret)
  🔵 重构: 提取 TokenService
  ✅ 提交

... (继续 20 个任务)
─────────────────────────────────────────────

AI Phase 4: 审查
─────────────────────────────────────────────
Spec Reviewer: ✅ 符合所有 Given/When/Then
Quality Reviewer: ✅ 代码质量良好
$ python scripts/agent_repo_check.py
✅ 通过
$ openspec validate --change user-auth
✅ 通过
─────────────────────────────────────────────

AI Phase 5: 归档
─────────────────────────────────────────────
$ openspec archive user-auth --yes
✅ 已归档
$ git commit -m "feat: add user authentication"
─────────────────────────────────────────────

完成！
```

---

### 案例 3: Bug 修复流程

**场景**: 登录功能出现间歇性失败

**系统化调试流程**:

```
用户: "登录功能有 bug，有时会失败"

AI: 启动 Systematic Debugging 流程
─────────────────────────────────────────────
Phase 1: Root Cause Investigation
─────────────────────────────────────────────
1. 阅读错误日志
   Error: Token generation timeout
   
2. 重现问题
   - 连续登录 10 次，第 7 次失败
   
3. 检查最近变更
   git log --since="1 week ago"
   发现: 上周修改了 JWT 库版本
   
4. 追踪数据流
   credentials → validation → token generation → timeout
   问题点: token generation 偶尔超时
─────────────────────────────────────────────

Phase 2: Pattern Analysis
─────────────────────────────────────────────
1. 找到工作示例
   本地开发环境正常
   
2. 对比差异
   本地: jwt.sign() 同步
   生产: jwt.sign() 异步，但用了同步调用
   
3. 识别模式
   库升级后 API 从同步变为异步
─────────────────────────────────────────────

Phase 3: Hypothesis + Testing
─────────────────────────────────────────────
假设: JWT 库升级后，sign() 方法变为异步，
      但代码仍以同步方式调用，导致有时返回 undefined

测试:
  const token = jwt.sign(payload, secret);
  console.log(token); // 有时是 undefined
  
验证: ✅ 假设成立
─────────────────────────────────────────────

Phase 4: Fix + Verification
─────────────────────────────────────────────
修复方案:
  // 旧代码
  const token = jwt.sign(payload, secret);
  
  // 新代码
  const token = await jwt.sign(payload, secret);

验证:
  1. 单元测试通过
  2. 集成测试通过
  3. 连续登录 100 次全部成功
─────────────────────────────────────────────

Phase 5: 记录
─────────────────────────────────────────────
1. 创建 OpenSpec 变更记录修复
2. 更新 docs/agent/reliability.md
3. 添加回归测试
4. 提交并归档
─────────────────────────────────────────────
```

---

## 故障排查

### 常见问题

#### Q1: agent_repo_check.py 报告文档过期

**症状**:
```
Warning: docs/agent/architecture.md last_reviewed > 90 days
```

**解决**:
```yaml
# 更新 frontmatter
---
last_reviewed: 2026-04-07
---
```

---

#### Q2: OpenSpec validate 失败

**症状**:
```
Error: Missing required artifact: specs/
```

**解决**:
```bash
# 检查缺失的 artifacts
openspec status --change <name> --json

# 创建缺失的 artifact
openspec instructions specs --change <name>
```

---

#### Q3: 测试覆盖不足

**症状**:
```
Error: New code has no tests
```

**解决**:
```bash
# 检查未覆盖的文件
npm test -- --coverage

# 为未覆盖的代码补充测试
# 遵循 TDD: 先写失败测试，再写实现
```

---

## 参考资源

### 官方文档

| 资源 | 链接 |
|------|------|
| OpenAI Harness Engineering | https://openai.com/index/harness-engineering/ |
| Superpowers (obra) | https://github.com/obra/superpowers |
| OpenSpec CLI | https://github.com/fission-ai/openspec |

### 本地文档

| 文档 | 路径 |
|------|------|
| 导航索引 | `docs/agent/index.md` |
| 架构文档 | `docs/agent/architecture.md` |
| 工作流详解 | `docs/agent/superpowers-workflow.md` |
| OpenSpec 指南 | `docs/agent/openspec-guide.md` |
| 质量标准 | `docs/agent/quality.md` |

### CLI 命令速查

```bash
# Harness
python scripts/bootstrap_project.py --mode overlay
python scripts/agent_repo_check.py
python scripts/agent_gc_report.py

# OpenSpec
openspec new change <name>
openspec status --change <name>
openspec validate --change <name>
openspec archive <name>

# ClawHub
clawhub search <query>
clawhub install <skill>
clawhub list

# SkillHub
skillhub install <skill>
skillhub list
```

---

## 附录

### 检查清单

#### 开始开发前

- [ ] 读取 `docs/agent/index.md` 了解项目
- [ ] 读取 `docs/agent/architecture.md` 了解约束
- [ ] 检查 `openspec/specs/` 当前行为

#### 创建变更时

- [ ] 使用 Superpowers Brainstorm
- [ ] 创建设计文档到 `docs/plans/`
- [ ] 创建 OpenSpec 变更
- [ ] 完成 proposal → specs → design → tasks

#### 实现时

- [ ] 遵循 TDD（先写测试）
- [ ] 每个任务 2-5 分钟
- [ ] 频繁提交
- [ ] 保持 YAGNI 和 DRY

#### 提交前

- [ ] 所有测试通过
- [ ] `agent_repo_check.py` 通过
- [ ] `openspec validate` 通过
- [ ] 代码审查完成

---

**培训结束**

如有疑问，请查阅本地文档或联系 QClaw 团队。
