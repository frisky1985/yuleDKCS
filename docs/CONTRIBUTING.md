# Git Flow 分支策略 & PR 流程

> **概要**: yuleDKCS 采用基于 Git Flow 的双分支策略，所有协作围绕 `develop` 和 `main` 两条主线展开。

---

## 分支拓扑

```
        main (only squash-merges from develop)
       ╱
feat/* ── develop ── release/* ── main
fix/*  ╱
       main (production-ready, protected)
```

| 分支 | 用途 | 权限 |
|------|------|------|
| `main` | 生产就绪代码，仅通过 squash-merge 从 `develop` 合并 | 🔒 受保护，需 PR + review |
| `develop` | 日常开发集成线，所有功能分支的合并目标 | PR 权限 |
| `release/*` | 发布候选分支，从 `develop` 切出 | 维护者 |
| `feat/*` | 功能开发 | 贡献者 |
| `fix/*` | 缺陷修复 | 贡献者 |

---

## 分支命名

| 模式 | 示例 | 用途 |
|------|------|------|
| `feat/<module>-<description>` | `feat/icce-uwb-ranging` | 新功能 |
| `fix/<module>-<description>` | `fix/hub-connection-timeout` | 缺陷修复 |
| `docs/<description>` | `docs/api-cleanup` | 文档改进 |
| `refactor/<module>-<description>` | `refactor/bertlv-decoder` | 代码重构 |
| `test/<module>-<description>` | `test/iccoa-dk4` | 测试补充 |
| `chore/<description>` | `chore/ci-setup` | 维护/工具链 |
| `release/<version>` | `release/v2.1.0` | 发布候选 |

---

## 开发流程

### 1. 从 `develop` 切出功能分支

```bash
git checkout develop
git pull origin develop
git checkout -b feat/my-module-description
```

### 2. 提交代码

遵循 [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(scope): concise description

[optional body explaining why]
```

示例：
```
feat(icce): add UWB secure ranging for ICCE protocol
fix(hub): fix WebSocket reconnection backoff
docs(api): update REST endpoint documentation
```

### 3. 发起 PR 到 `develop`

创建 Pull Request，目标分支为 `develop`。

**PR Checklist：**
- [ ] 代码遵循项目中对应语言的编码规范（参见 [根目录 CONTRIBUTING.md](../CONTRIBUTING.md)）
- [ ] 所有测试通过
- [ ] 新代码覆盖率 ≥ 80%
- [ ] 提交信息遵循 Conventional Commits
- [ ] 分支已 rebase 到最新的 `develop`
- [ ] 无敏感信息泄露

### 4. 合并到 `develop`

- 至少需要 1 名维护者审查
- 重大变更需要 2 名批准
- 通过 **Squash Merge** 合并

### 5. 从 `develop` 合并到 `main`

当 `develop` 积累足够功能并经过充分测试后：

```bash
git checkout main
git pull origin main
git merge --squash develop
git commit -m "release: vX.Y.Z"
```

或者通过 GitHub PR 从 `develop` → `main`，使用 squash-merge。

---

## CI/CD 触发策略

| 事件 | 触发的工作流 | 说明 |
|------|-------------|------|
| push/PR 到任意分支 | `ci.yml` | 单元测试 + 集成测试 |
| push/PR 到 `develop` | `android-ci.yml`, `ios-ci.yml`, `ci-java.yml` | 平台相关测试 (路径过滤) |
| push/PR 到 `main`, `develop`, `release/*` | `yuleosh-ci.yml` | 证据链合规检查 |
| push/PR 到 `main`, `develop`, `feat/**` | `misra-ci.yml` | MISRA C 静态分析 (路径过滤) |
| push/PR 到 `main`, `develop` | `fault-inject-ci.yml` | 故障注入测试 (路径过滤) |
| PR 到任意分支 | `cover-check.yml` | 覆盖率门禁 (Go 后端) |
| merge 到 `main` | 所有上述工作流 | 全面验证 + yuleOSH 证据包 |

---

## 与现有贡献指南的关系

本文件专注于 **分支策略与 PR 流程**。更全面的编码规范、测试要求、社区行为准则参见根目录：

- [`CONTRIBUTING.md`](../CONTRIBUTING.md) — 完整贡献指南
- [`CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) — 行为准则
- [`COMMUNITY.md`](../COMMUNITY.md) — 社区沟通渠道

---

## 快速参考

```bash
# 开始新功能
git checkout develop && git pull
git checkout -b feat/my-feature
# ... 开发、提交 ...
git push -u origin feat/my-feature
# → 在 GitHub 上创建 PR → develop

# 同步 develop 到本地
git checkout develop && git pull

# 发布
git checkout main && git pull
git merge --squash develop
git commit -m "release: v$(date +%Y.%m.%d)"
git push origin main
```

---

*如有疑问，请在 GitHub Discussions 或 Discord 中提问。*
