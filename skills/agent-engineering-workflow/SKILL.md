---
name: agent-engineering-workflow
description: |
  🚀 AI 辅助工程最佳实践 — 整合 Harness Engineering + Superpowers + OpenSpec
  
  适用于任何需要规范化开发的软件项目。提供完整的基础设施、工作流和规格管理。
  
  触发场景：
  - "初始化工程环境"、"setup project for AI"
  - "开发新功能"、"build feature"
  - "修复 bug"、"debug issue"
  - "创建技能"、"new skill"
  
  自动整合三大技能形成完整的开发体系。
version: 1.0.0
author: QClaw
tags:
  - harness-engineering
  - superpowers
  - openspec
  - best-practices
  - workflow
---

# Agent Engineering Workflow

统一的 AI 辅助工程最佳实践，整合三大核心技能。

## 🎯 核心理念

**三大技能互补协作**：

| 技能 | 层面 | 职责 |
|------|------|------|
| **Harness Engineering** | 基础设施 | 让仓库对 AI 可读、可维护 |
| **Superpowers** | 工作流 | 规范开发流程，确保质量 |
| **OpenSpec** | 规格 | 结构化变更管理，可追溯 |

## 🚀 快速开始

### 初始化新项目

```
用户: "初始化这个项目的工程环境"

AI 执行:
1. 使用 Harness Engineering 创建基础设施
2. 创建 AGENTS.md 作为路由
3. 创建 docs/agent/ 文档结构
4. 创建 openspec/ 规格结构
5. 安装质量检查脚本
```

### 开发新功能

```
用户: "帮我开发用户认证功能"

AI 执行:
1. 启动 Superpowers Brainstorm 阶段
2. 创建设计文档到 docs/plans/
3. 创建 OpenSpec 变更
4. TDD 实现
5. 代码审查
6. 归档变更
```

### 修复 Bug

```
用户: "登录功能有 bug，帮我修复"

AI 执行:
1. 启动 Systematic Debugging
2. 根本原因分析
3. 创建 OpenSpec 变更记录修复
4. 验证修复
5. 更新文档
```

## 📁 标准目录结构

```
project/
├── AGENTS.md              # 总入口 (Harness)
├── docs/
│   ├── agent/             # 工程文档 (Harness)
│   │   ├── index.md       # 导航索引
│   │   ├── architecture.md
│   │   ├── specs.md
│   │   ├── quality.md
│   │   ├── reliability.md
│   │   ├── security.md
│   │   ├── superpowers-workflow.md
│   │   └── openspec-guide.md
│   └── plans/             # 设计文档 (Superpowers)
├── openspec/
│   ├── config.yaml        # OpenSpec 配置
│   ├── changes/           # 活跃变更
│   ├── specs/             # 系统规格
│   └── schemas/           # 自定义 schema
├── scripts/
│   ├── agent_repo_check.py
│   └── agent_gc_report.py
├── memory/
│   └── YYYY-MM-DD.md
├── MEMORY.md
├── USER.md
├── SOUL.md
└── TOOLS.md
```

## 🔄 完整工作流

```
┌─────────────────────────────────────────────────────────────┐
│                    Phase 1: Initialize                       │
│                    (Harness Engineering)                     │
├─────────────────────────────────────────────────────────────┤
│  python scripts/bootstrap_project.py --mode overlay          │
│  创建 AGENTS.md + docs/agent/ + openspec/                    │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Phase 2: Brainstorm                       │
│                    (Superpowers)                             │
├─────────────────────────────────────────────────────────────┤
│  探索 → 提问 → 方案 → 设计文档                               │
│  产出: docs/plans/YYYY-MM-DD-<topic>-design.md              │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Phase 3: Specify                          │
│                    (OpenSpec)                                │
├─────────────────────────────────────────────────────────────┤
│  openspec new change <name>                                  │
│  proposal → specs → design → tasks                           │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Phase 4: Implement                        │
│                    (Superpowers TDD)                         │
├─────────────────────────────────────────────────────────────┤
│  sessions_spawn (implementer)                                │
│  写测试 → 写代码 → 重构 → 提交                               │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Phase 5: Review                           │
│                    (Superpowers + Harness)                   │
├─────────────────────────────────────────────────────────────┤
│  spec-reviewer → quality-reviewer                            │
│  agent_repo_check.py                                         │
│  openspec validate                                           │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Phase 6: Archive                          │
│                    (OpenSpec)                                │
├─────────────────────────────────────────────────────────────┤
│  openspec archive <name>                                     │
│  合并 delta specs → 移动到归档                               │
└─────────────────────────────────────────────────────────────┘
```

## 📋 检查清单

### 开始开发前

- [ ] 读取 `docs/agent/index.md` 了解项目
- [ ] 读取 `docs/agent/architecture.md` 了解约束
- [ ] 检查 `openspec/specs/` 当前行为

### 创建变更时

- [ ] 使用 Superpowers Brainstorm
- [ ] 创建设计文档到 `docs/plans/`
- [ ] 创建 OpenSpec 变更
- [ ] 完成 proposal → specs → design → tasks

### 实现时

- [ ] 遵循 TDD（先写测试）
- [ ] 每个任务 2-5 分钟
- [ ] 频繁提交
- [ ] 保持 YAGNI 和 DRY

### 提交前

- [ ] 所有测试通过
- [ ] `agent_repo_check.py` 通过
- [ ] `openspec validate` 通过
- [ ] 代码审查完成

## 🛠️ 工具命令

### Harness Engineering

```bash
# 初始化仓库
python scripts/bootstrap_project.py --mode overlay

# 质量检查
python scripts/agent_repo_check.py

# 垃圾回收报告
python scripts/agent_gc_report.py
```

### Superpowers

```
# Brainstorm → Plan → Implement → Review → Finish
# 按工作流阶段执行，无需单独命令
```

### OpenSpec

```bash
# 创建变更
openspec new change <name>

# 检查状态
openspec status --change <name>

# 验证
openspec validate --change <name>

# 归档
openspec archive <name> --yes
```

## ⚠️ 重要规则

### HARD GATES (强制)

1. **禁止在用户批准设计前编写代码** (Superpowers)
2. **禁止在没有根本原因分析前修复 bug** (Superpowers)
3. **禁止在没有测试的情况下提交代码** (TDD)
4. **禁止将敏感数据写入日志或非 main session** (Security)

### 最佳实践

1. **渐进式披露**: 只加载当前任务需要的文档
2. **文档即代码**: 所有知识都记录在 docs/ 或 openspec/
3. **质量门禁**: 提交前运行所有检查
4. **持续改进**: 定期更新 MEMORY.md 和文档

## 📚 参考文档

- [docs/agent/index.md](../docs/agent/index.md) - 导航索引
- [docs/agent/superpowers-workflow.md](../docs/agent/superpowers-workflow.md) - 工作流详解
- [docs/agent/openspec-guide.md](../docs/agent/openspec-guide.md) - OpenSpec 使用
- [docs/agent/quality.md](../docs/agent/quality.md) - 质量标准

## 🔗 依赖技能

本技能整合以下技能的最佳实践：

- **agent-harness-engineering**: 仓库工程化基础设施
- **superpowers**: 开发工作流
- **openspec**: 规格驱动开发
- **agent-security-harness**: 安全测试（可选）

使用本技能时，会自动调用上述技能的相关功能。
