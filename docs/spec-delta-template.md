# Spec Delta Template

每次变更时必须填写此模板，与 PR 一并提交。

---

```markdown
# Spec Delta

**日期**: YYYY-MM-DD
**变更范围**: (简要说明影响的范围，如 "密钥分享流程" / "认证接口")
**影响模块**: (受影响的模块列表，如 dkcs/hub/adapters)
**变更类型**: feature | fix | refactor
**受影响需求**: REQ-xxx
**变更描述**:
(详细描述变更内容、动机和设计决策)

**测试验证**:
- [ ] 单元测试覆盖
- [ ] 集成测试覆盖
- [ ] 手动测试验证
- [ ] 回归测试通过
```

## 使用说明

| 字段 | 说明 |
|------|------|
| 日期 | 实际提交日期（ISO 格式） |
| 变更范围 | 一句话描述变更边界 |
| 影响模块 | 代码库中的子目录/组件名称 |
| 变更类型 | `feature` / `fix` / `refactor` 之一 |
| 受影响需求 | 需求追踪矩阵中的编号，参考 `docs/requirement-traceability-matrix.md` |
| 变更描述 | 动机 + 方案 + 关键决策，不少于 3 句 |
| 测试验证 | checklist 确保测试完备 |

## 放置位置

- 与 PR 描述一起提交
- 也可归档至 `docs/spec/deltas/` 目录，按 `YYYY-MM-DD-<brief>.md` 命名

## 检查清单

- [ ] 所有字段已填写
- [ ] 需求编号可在 `docs/requirement-traceability-matrix.md` 中找到
- [ ] 测试验证项至少勾选一项
- [ ] 影响模块与实际代码变更一致
