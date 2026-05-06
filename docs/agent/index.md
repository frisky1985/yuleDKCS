---
title: Agent Engineering Index
owner: QClaw
last_reviewed: 2026-04-07
---

# Agent Engineering Index

本索引导航至所有 agent 可读的工程文档。**渐进式披露原则**：只读取当前任务所需的文档。

## 🏗️ 工程基础设施 (Harness Engineering)

| 文档 | 用途 | 何时阅读 |
|------|------|---------|
| [architecture.md](architecture.md) | 系统架构边界和约束 | 修改核心模块时 |
| [specs.md](specs.md) | 规格总览和行为预期 | 添加/修改功能时 |
| [quality.md](quality.md) | 质量标准和检查规则 | 提交代码前 |
| [reliability.md](reliability.md) | 可靠性要求 | 处理错误/异常时 |
| [security.md](security.md) | 安全边界和信任模型 | 涉及权限/数据时 |

## 🔄 开发工作流 (Superpowers)

| 文档 | 用途 | 何时阅读 |
|------|------|---------|
| [superpowers-workflow.md](superpowers-workflow.md) | 完整开发流程 | 开始新功能开发前 |
| [tdd-guide.md](tdd-guide.md) | TDD 实践指南 | 编写测试时 |

## 📋 规格驱动开发 (OpenSpec)

| 文档 | 用途 | 何时阅读 |
|------|------|---------|
| [openspec-guide.md](openspec-guide.md) | OpenSpec 使用指南 | 创建变更规格时 |
| [../plans/](../plans/) | 设计文档目录 | 规划阶段 |

## 🚀 快速开始

1. **新功能开发** → 阅读 `superpowers-workflow.md` → 使用 OpenSpec 创建变更
2. **Bug 修复** → 阅读 `quality.md` 和 `reliability.md` → 系统化调试
3. **架构决策** → 阅读 `architecture.md` → 更新相关文档

## 📁 目录结构

```
workspace/
├── AGENTS.md              # 总入口（本文件的路由源）
├── docs/
│   ├── agent/             # Agent 可读文档
│   └── plans/             # 设计文档
├── openspec/
│   ├── config.yaml        # OpenSpec 配置
│   ├── changes/           # 活跃变更
│   ├── specs/             # 系统规格（真值）
│   └── schemas/           # 自定义 schema
└── scripts/
    └── agent_repo_check.py # 质量检查脚本
```

## ⚠️ 重要规则

1. **AGENTS.md 是路由器** — 不要在此堆砌内容，链接到具体文档
2. **渐进式披露** — 只加载任务相关的文档
3. **质量门禁** — 提交前运行 `scripts/agent_repo_check.py`
4. **文档即代码** — 所有知识都应在 `docs/agent/` 或 `openspec/` 中有记录
