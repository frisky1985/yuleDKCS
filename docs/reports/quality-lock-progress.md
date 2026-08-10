# 质量锁死 — 防退化机制

## 变更清单

### 1. Coverage Gate 升级 60% → 70%

**文件:** `.github/workflows/ci.yml`

| 位置 | 旧值 | 新值 |
|------|------|------|
| L63 Step name | `Coverage Gate (≥60%)` | `Coverage Gate (≥70%)` |
| L71 dkcs check | `< 60` | `< 70` |
| L72 dkcs message | `below 60% threshold` | `below 70% threshold` |
| L83 hub check | `< 60` | `< 70` |
| L84 hub message | `below 60% threshold` | `below 70% threshold` |
| L89 success msg | `both modules ≥60%` | `both modules ≥70%` |

**验证通过:**
- dkcs: **88.7%** ≥ 70% ✅
- hub (CI gates, 排除 api/v1 + cmd/): **83.4%** ≥ 70% ✅

### 2. 退化自动告警

**新建文件:** `.github/workflows/coverage-monitor.yml`

- 每周一 08:00 UTC 自动运行
- 支持 `workflow_dispatch` 手动触发
- 输出 Markdown 趋势报告至 `reports/coverage-trend.md`
- 包含 dkcs + hub 两个模块的逐包覆盖率和门禁状态表

### 3. Lint 门禁从 warn 改成 block

**文件:** `.github/workflows/ci.yml`

| 位置 | 旧值 | 新值 |
|------|------|------|
| L181 golangci-lint step | `continue-on-error: true` | `continue-on-error: false` |

效果: golangci-lint 安全检查失败时不再跳过，直接阻断 CI pipeline。

### 4. 当前通过状态

| 模块 | 覆盖率 | 阈值 | 状态 |
|------|--------|------|------|
| dkcs | 88.7% | 70% | ✅ PASS |
| hub | 83.4% | 70% | ✅ PASS |

---

## 退化防护机制

```
┌─────────────────────────────────────┐
│          每个 PR / push              │
│         ┌───────────────┐           │
│         │ Coverage Gate │◀── 70%    │
│         │    (block)    │           │
│         └───────┬───────┘           │
│                 ▼ fail              │
│             ❌ PR blocked           │
│                 │                   │
│         ┌───────┴───────┐           │
│         │ golangci-lint │◀── block  │
│         │   (block)     │           │
│         └───────┬───────┘           │
│                 ▼ fail              │
│             ❌ PR blocked           │
└─────────────────────────────────────┘
              每周
         ┌───────────────┐
         │ Coverage      │
         │ Trend Monitor │
         └───────┬───────┘
                 ▼
         reports/coverage-trend.md
         (人工审查)
```
