# MISRA C:2023 合规计划

> 版本: v1.0 | 日期: 2026-07-29

## 目标

实现 yuleDKCS 嵌入式代码对 MISRA C:2023 的 **Required** 规则 100% 合规，Advisory 规则 ≥ 90% 合规。

## 合规策略

| 层次 | 方法 | 工具 |
|------|------|------|
| 静态分析 | clang-tidy + cppcheck | `.clang-tidy` 配置已部署 |
| 代码审查 | Deviation 审批流程 | 偏差记录在 `deviations/` |
| 持续集成 | PR 门禁 | GitHub Actions 集成 |
| 审计 | 每 Sprint 合规检查 | 合规报告 |

## 规则覆盖矩阵

| 规则类别 | 总数 | 已覆盖 | 待覆盖 |
|---------|------|--------|--------|
| Required (R) | 143 | 82 | 61 |
| Advisory (A) | 107 | 48 | 59 |

## Deviation 流程

1. 开发者提交 Deviation 申请
2. 架构师 + 安全工程师评审
3. 记录到 `.yuleosh/ci-config.yaml` 的 deviations 段
4. Deviation 有效期 ≤ 6 个月

## 当前状态

- ✅ `.clang-tidy` 配置已创建
- ⏳ 首次全量扫描待执行
- ⏳ Deviation 清单待编制

## 里程碑

| 日期 | 里程碑 |
|------|--------|
| 2026-08 | 首次全量扫描 + 偏差清单 |
| 2026-09 | Required 规则 80% 合规 |
| 2026-10 | Required 规则 100% 合规 |
| 2026-11 | Advisory 规则 90% 合规 |
