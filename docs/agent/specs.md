---
title: Specs Overview
owner: QClaw
last_reviewed: 2026-04-07
---

# Specs Overview

本文档提供系统规格总览。详细规格位于 `openspec/specs/`。

## 核心行为规格

### 1. Agent 技能系统

**位置**: `openspec/specs/skills-system.md`

| 规格项 | 描述 |
|--------|------|
| 技能发现 | Agent 必须能通过 AGENTS.md 发现所需技能 |
| 技能加载 | 按需加载，渐进式披露 |
| 技能协作 | 技能间通过文件系统通信 |

### 2. 开发工作流

**位置**: `openspec/specs/development-workflow.md`

| 规格项 | 描述 |
|--------|------|
| 设计先行 | 所有代码变更必须有设计文档 |
| TDD 必须 | 先写测试再实现 |
| 规格驱动 | 所有变更必须有 OpenSpec 规格 |

### 3. 文档系统

**位置**: `openspec/specs/documentation-system.md`

| 规格项 | 描述 |
|--------|------|
| AGENTS.md 路由 | 保持简短，链接到详细文档 |
| 文档分层 | agent/ → plans/ → openspec/ |
| 质量检查 | 所有文档必须有 owner 和 last_reviewed |

## 关键约束

### SHALL (必须)

- 系统必须在代码变更前创建规格
- 所有测试必须通过才能合并
- AGENTS.md 必须链接到所有活跃文档

### MUST (强制)

- 每个文档必须有 owner
- last_reviewed 必须 < 90 天
- 所有公共 API 必须有文档

### SHOULD (应该)

- 每个功能应该有 Given/When/Then 场景
- 任务粒度应该 2-5 分钟可完成
- 复杂逻辑应该有注释

### MAY (可以)

- 可以创建自定义 OpenSpec schema
- 可以启用垃圾回收自动报告
- 可以使用 subagent 并行执行

## 行为场景

### 场景：开发新功能

```gherkin
GIVEN 用户请求添加新功能
WHEN agent 启动 Superpowers 工作流
THEN agent 创建设计文档到 docs/plans/
AND agent 创建 OpenSpec 变更
AND agent 使用 TDD 实现
AND 所有测试通过后合并
```

### 场景：修复 Bug

```gherkin
GIVEN 用户报告 Bug
WHEN agent 启动调试流程
THEN agent 进行根本原因分析
AND agent 创建 OpenSpec 变更记录修复
AND agent 验证修复不破坏其他功能
```

### 场景：文档维护

```gherkin
GIVEN 文档 last_reviewed 超过 90 天
WHEN agent_repo_check.py 运行
THEN 报告过期文档
AND 通知 owner 更新
```

## 规格变更流程

```
发现需求 → 创建 OpenSpec 变更 → 定义 delta specs → 
实现 → 验证 → 归档 (合并到主 specs)
```

## 详细规格文件

| 文件 | 内容 |
|------|------|
| `openspec/specs/skills-system.md` | 技能系统行为 |
| `openspec/specs/development-workflow.md` | 开发流程行为 |
| `openspec/specs/documentation-system.md` | 文档系统行为 |

**注意**: 这些是真值 (source of truth)。任何变更应通过 OpenSpec 流程。
