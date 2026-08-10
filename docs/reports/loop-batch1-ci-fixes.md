# Batch 1: CI 与分支 — 修复报告

> **日期**: 2026-07-16  
> **作者**: 子代理 (subagent)

---

## 修复 1: 覆盖率硬性门槛 (cover-check.yml)

**文件**: `.github/workflows/cover-check.yml`

**改动**:
1. 保留原有的 Hub 模块级别门禁（codec 50%, 其余 60%）
2. 新增 **Overall Coverage Gate (≥60%)** 步骤（`id: overall_cov`）：
   - 从 DKCS 和 Hub 的 `coverage.txt` 提取 total 行
   - 计算两者简单平均值作为整体覆盖率
   - 若整体 < 60% 则 `exit 1`（非零退出码，使 job 失败）
   - 输出 `overall`、`dkcs`、`hub` 三个变量供后续步骤使用
3. 新增 **Post Coverage Report as PR Comment** 步骤：
   - 仅在 `pull_request` 事件触发
   - 使用 `actions/github-script@v7` 将所有包/函数覆盖率解析为 Markdown 表格
   - 每行带 🟢/🟡/🔴 状态（≥50% 绿, ≥30% 黄, <30% 红）
   - 底部汇总 Hub/DKCS/Overall 百分比 + 门禁通过/失败标识
4. 保留原有的 `Upload Coverage Artifacts` 步骤，并补充了 dkcs 的 coverage.txt

---

## 修复 2: 嵌入式 C 测试 CI 集成 (ci.yml)

**文件**: `.github/workflows/ci.yml`

**改动**:
1. 新增 `c-test` job（运行在 `ubuntu-latest`）：
   - `needs: [test]` — 在 Go 测试之后执行
   - 安装依赖: `gcc make libc6-dev`（`ubuntu-latest` 自带 gcc 但显式安装以确保版本）
   - `make clean && make test_all` — 编译并运行 Unity 测试框架
   - 输出日志上传为 artifact `c-test-logs`
2. 将 `coverage-gate` 的 `needs` 改为 `[test, c-test]`，确保 C 测试也通过后才触发覆盖率门禁

**可选用法**: 如需单独 run 某个子套件，Makefile 支持 `test_iccoa`、`test_ccc`、`test_unified` 等 target。

---

## 修复 3: .gitignore 清理

**文件**: `.gitignore`

**新增条目**（在 `*.log` 之后、`Secrets` 之前）：
```
.coverage
.benchmarks/
.pytest_cache/
__pycache__/
```

这些目录/文件是 Go 测试、Python 测试、pytest/benchmark 等工具的运行时产物，不应被 commit。

---

## 验证清单

| # | 检查项 | 状态 |
|---|--------|------|
| 1a | cover-check.yml: 整体 ≥60% 门槛存在 | ✅ |
| 1b | cover-check.yml: 非零退出码（`exit 1`）| ✅ |
| 1c | cover-check.yml: PR comment 步骤 | ✅ |
| 2a | ci.yml: c-test job 存在 | ✅ |
| 2b | ci.yml: c-test needs test (Go 之后) | ✅ |
| 2c | ci.yml: coverage-gate needs c-test | ✅ |
| 3a | .gitignore: .coverage 已添加 | ✅ |
| 3b | .gitignore: .benchmarks/ 已添加 | ✅ |
| 3c | .gitignore: .pytest_cache/ 已添加 | ✅ |
| 3d | .gitignore: __pycache__/ 已添加 | ✅ |
| — | 未修改业务代码 (src/backend/, embedded/, frontend/) | ✅ |

---

## 后续建议

1. **本地验证**: 在 CI 实际运行前，可以先在本地跑 `make -C embedded/tests test_all` 确认 Unity 测试可编译通过
2. **docker 化 C 测试环境**: 如果 embedded 依赖特定交叉编译工具链（如 `arm-none-eabi`），可考虑使用 Docker 镜像
3. **覆盖率合并**: 当前整体覆盖率使用简单平均（Hub + DKCS）/2，若需要加权可后续调整
