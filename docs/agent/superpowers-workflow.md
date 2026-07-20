---
title: Superpowers Workflow Guide
owner: QClaw
last_reviewed: 2026-04-07
---

# Superpowers Workflow Guide

本文档整合 Superpowers 工作流与 Harness Engineering 基础设施和 OpenSpec 规格管理。

## 完整开发流程

```
┌────────────────────────────────────────────────────────────────┐
│                    Superpowers Pipeline                         │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Phase 1: Brainstorm                                            │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 探索上下文 → 提问澄清 → 提出方案 → 设计文档             │   │
│  │ 产出: docs/plans/YYYY-MM-DD-<topic>-design.md           │   │
│  └─────────────────────────────────────────────────────────┘   │
│                            ↓                                    │
│  Phase 2: OpenSpec 规格化                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ proposal → specs → design → tasks                       │   │
│  │ 产出: openspec/changes/<feature>/                        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                            ↓                                    │
│  Phase 3: Subagent-Driven Development                          │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ TDD 循环: 测试 → 实现 → 重构 → 提交                     │   │
│  │ 每个 task 用 sessions_spawn 分配给 subagent              │   │
│  └─────────────────────────────────────────────────────────┘   │
│                            ↓                                    │
│  Phase 4: Code Review                                          │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ spec-reviewer → quality-reviewer → fix → re-review      │   │
│  └─────────────────────────────────────────────────────────┘   │
│                            ↓                                    │
│  Phase 5: Finish Branch                                        │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 验证测试 → 合并/PR → 清理                               │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

## Phase 1: Brainstorming

### 触发条件
- 用户说 "让我构建..."
- 用户说 "帮我规划..."
- 用户说 "我想添加 X..."
- 任何需要设计的新功能

### 执行步骤
1. **探索项目上下文**
   - 读取 `docs/agent/architecture.md`
   - 读取 `openspec/specs/` 了解当前行为
   - 检查最近 commits

2. **提问澄清** (一次一个问题，多选题优先)
   - 这个功能的主要用户是谁？
   - 成功的标准是什么？
   - 有哪些约束？

3. **提出方案** (2-3 个)
   - 每个方案的优缺点
   - 推荐方案及理由

4. **呈现设计** (分段展示)
   - 概述 → 用户流程 → 技术设计 → 边界情况
   - 每段等待用户确认

5. **写入设计文档**
   ```
   docs/plans/YYYY-MM-DD-<topic>-design.md
   ```
   提交到 Git

### HARD GATE
**禁止在用户批准设计前编写任何代码**

## Phase 2: OpenSpec 规格化

### 触发条件
设计文档已获批准

### 执行步骤
```bash
# 创建变更
openspec new change <feature-name>

# 按顺序创建 artifacts
openspec instructions proposal --change <name>
openspec instructions specs --change <name>
openspec instructions design --change <name>
openspec instructions tasks --change <name>
```

### Artifacts 说明
| Artifact | 内容 | 格式 |
|----------|------|------|
| `proposal.md` | 为什么做、做什么 | 背景、目标、范围 |
| `specs/` | 行为规格 | Given/When/Then |
| `design.md` | 技术方案 | 架构、接口、数据 |
| `tasks.md` | 实现清单 | 带复选框的任务列表 |

## Phase 3: Subagent-Driven Development

### 触发条件
OpenSpec 规格已创建，用户选择 subagent 执行

### 每个 Task 的执行循环

```
┌─────────────────────────────────────┐
│  1. sessions_spawn (implementer)    │
│     - 任务描述 + 上下文             │
│     - TDD 要求                      │
└─────────────────────────────────────┘
                ↓
┌─────────────────────────────────────┐
│  2. sessions_spawn (spec-reviewer)  │
│     - 验证代码符合规格              │
└─────────────────────────────────────┘
                ↓
┌─────────────────────────────────────┐
│  3. sessions_spawn (quality-review) │
│     - 代码质量、最佳实践            │
└─────────────────────────────────────┘
                ↓
         [通过?]
        /       \
      是         否
       ↓          ↓
   下一个task   修复问题 → 重审
```

### TDD 强制要求
1. 先写失败测试
2. 实现最小代码使测试通过
3. 重构优化
4. 提交

**禁止**: 在测试前编写实现代码

## Phase 4: Systematic Debugging

### 触发条件
- Bug 报告
- 测试失败
- 意外行为

### 四阶段调试
1. **Root Cause Investigation**
   - 阅读错误信息
   - 重现问题
   - 检查最近变更
   - 追踪数据流

2. **Pattern Analysis**
   - 找到工作示例
   - 对比差异
   - 识别模式

3. **Hypothesis + Testing**
   - 一次一个假设
   - 测试验证/反驳

4. **Fix + Verification**
   - 修复根本原因（非症状）
   - 验证修复不破坏其他功能

### HARD GATE
**禁止在没有根本原因分析前进行修复**

## Phase 5: Finish Branch

### 执行步骤
1. 运行所有测试
2. 确定基础分支
3. 提供 4 个选项:
   - 本地合并
   - 推送 + 创建 PR
   - 保留分支
   - 丢弃分支
4. 执行用户选择
5. 清理

## Subagent Dispatch 模板

```
Goal: [一句话目标]
Context: [为什么重要，关联的 plan 文件]
Files: [精确路径]
Constraints: [禁止做什么 - 无范围蔓延，仅 TDD]
Verify: [如何确认成功 - 测试通过，具体命令]
Task text: [粘贴 plan 中的完整任务]
```

## 关键原则

| 原则 | 说明 |
|------|------|
| **一次一个问题** | Brainstorm 阶段逐个提问 |
| **TDD 必须** | 先写失败测试 |
| **YAGNI** | 删除不必要的功能 |
| **DRY** | 无重复代码 |
| **系统化优于临时** | 尤其在压力下遵循流程 |
| **证据优于声明** | 验证后再宣称成功 |
| **频繁提交** | 每个绿测试后提交 |
