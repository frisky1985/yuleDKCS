---
title: Quality Gates
owner: QClaw
last_reviewed: 2026-04-07
---

# Quality Gates

本文档定义代码和文档的质量标准及检查规则。

## 质量门禁清单

### 代码质量

| 检查项 | 要求 | 验证方式 |
|--------|------|---------|
| 测试覆盖 | 所有新代码必须有测试 | `npm test` / `pytest` |
| TDD 流程 | 先写失败测试再实现 | 代码审查 |
| Lint | 无 lint 错误 | `eslint` / `pylint` |
| 类型检查 | TypeScript 严格模式 | `tsc --noEmit` |
| 文档注释 | 公共 API 有注释 | 代码审查 |

### 文档质量

| 检查项 | 要求 | 验证方式 |
|--------|------|---------|
| Frontmatter | 必须有 owner, last_reviewed | `agent_repo_check.py` |
| 链接完整 | 所有链接有效 | `agent_repo_check.py` |
| 索引更新 | 新文档在 index.md 中列出 | `agent_repo_check.py` |
| 审阅日期 | last_reviewed < 90 天 | `agent_repo_check.py` |

### OpenSpec 质量

| 检查项 | 要求 | 验证方式 |
|--------|------|---------|
| 完整性 | proposal → specs → design → tasks | `openspec validate` |
| 可测试性 | 每个 spec 有 Given/When/Then | `openspec validate` |
| 任务粒度 | 每个任务 2-5 分钟 | 代码审查 |

## 自动化检查脚本

### agent_repo_check.py

```bash
python scripts/agent_repo_check.py
```

检查内容:
- [ ] `AGENTS.md` 存在且包含导航链接
- [ ] `docs/agent/index.md` 指向所有活跃文档
- [ ] 所有文档有正确的 frontmatter
- [ ] last_reviewed 日期在 90 天内
- [ ] 无孤立文档（未被索引链接）
- [ ] OpenSpec 变更完整

### 集成到 CI

```yaml
# .github/workflows/quality.yml
name: Quality Gates
on: [push, pull_request]

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
      - run: pip install pyyaml
      - run: python scripts/agent_repo_check.py
      - run: openspec validate --all --json
```

## 代码审查清单

### 必须通过

- [ ] 所有测试通过
- [ ] 无 lint 错误
- [ ] 代码符合规格 (OpenSpec)
- [ ] 新代码有测试
- [ ] 无安全漏洞

### 应该通过

- [ ] 代码可读性好
- [ ] 无重复代码 (DRY)
- [ ] 无过度设计 (YAGNI)
- [ ] 错误处理完善

## 文档审查清单

- [ ] 文档有明确的 owner
- [ ] last_reviewed 日期正确
- [ ] 内容与代码一致
- [ ] 已添加到 index.md
- [ ] 链接有效

## 质量度量

### 代码质量指标

| 指标 | 目标 | 测量方式 |
|------|------|---------|
| 测试覆盖率 | > 80% | `jest --coverage` |
| 圈复杂度 | < 10 | `eslint complexity` |
| 重复代码 | < 5% | `jscpd` |

### 文档质量指标

| 指标 | 目标 | 测量方式 |
|------|------|---------|
| 文档覆盖率 | 100% API | 手动检查 |
| 审阅及时性 | < 90 天 | `agent_repo_check.py` |
| 链接有效性 | 100% | 自动检查 |

## 垃圾回收

### 触发条件
- 仓库变化频繁
- 多 agent 协作
- 文档/代码膨胀

### GC 报告检查项

| 检查项 | 处理方式 |
|--------|---------|
| 过期文档 | 更新或删除 |
| 过大文件 | 拆分 |
| 可疑文件名 (final-final, v2) | 重命名 |
| TODO/FIXME 集群 | 创建 issue |
| 孤立文档 | 添加到索引或删除 |

### 运行 GC

```bash
python scripts/agent_gc_report.py
```

**注意**: GC 只报告候选，不自动删除。
