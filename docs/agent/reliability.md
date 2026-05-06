---
title: Reliability Requirements
owner: QClaw
last_reviewed: 2026-04-07
---

# Reliability Requirements

本文档定义系统的可靠性要求和错误处理标准。

## 可靠性目标

| 指标 | 目标 | 测量方式 |
|------|------|---------|
| 正常运行时间 | > 99.9% | 监控系统 |
| 错误恢复时间 | < 5 分钟 | 手动测量 |
| 数据丢失 | 0 | 检查点验证 |

## 错误处理原则

### 1. 明确的错误分类

| 类别 | 示例 | 处理方式 |
|------|------|---------|
| **用户错误** | 无效输入 | 友好提示，引导修正 |
| **系统错误** | 服务不可用 | 重试 + 降级 |
| **致命错误** | 数据损坏 | 日志 + 停止 + 通知 |

### 2. 防御性编程

```python
# 好的做法
def process_file(path: str) -> Result[Data, Error]:
    if not os.path.exists(path):
        return Error(f"File not found: {path}")
    
    try:
        with open(path, 'r') as f:
            data = parse(f.read())
    except ParseError as e:
        return Error(f"Parse failed: {e}")
    
    return Ok(data)

# 坏的做法
def process_file(path: str) -> Data:
    with open(path, 'r') as f:  # 可能抛出异常
        return parse(f.read())   # 可能抛出异常
```

### 3. 重试策略

| 场景 | 策略 |
|------|------|
| 网络请求 | 指数退避，最多 3 次 |
| 文件操作 | 立即重试 1 次 |
| 数据库操作 | 线性退避，最多 5 次 |

## 降级策略

### Agent 系统降级

```
正常模式
    ↓ (技能服务不可用)
有限模式 (仅核心技能)
    ↓ (核心技能不可用)
安全模式 (仅文件操作)
```

### 降级行为

| 模式 | 可用功能 | 不可用功能 |
|------|---------|-----------|
| 正常 | 所有技能 | - |
| 有限 | 文件、Git、执行 | 网络搜索、API 集成 |
| 安全 | 读写文件 | 所有外部调用 |

## 恢复机制

### 1. 检查点

- 每个 subagent 任务完成后保存状态
- 内存文件每次更新后提交
- 设计文档每个阶段后保存

### 2. 回滚

```bash
# Git 回滚
git revert <commit>

# OpenSpec 回滚
openspec rollback <change-name>

# 文档回滚
git checkout HEAD~1 -- docs/
```

### 3. 灾难恢复

| 场景 | 恢复步骤 |
|------|---------|
| 代码丢失 | `git reflog` → `git reset` |
| 文档损坏 | 从 Git 历史恢复 |
| 配置错误 | 使用备份配置 |

## 监控和告警

### 关键指标

- 任务成功率
- 错误率 (按类别)
- 恢复时间

### 告警条件

| 条件 | 级别 | 行动 |
|------|------|------|
| 错误率 > 5% | 警告 | 检查日志 |
| 错误率 > 20% | 严重 | 暂停自动化 |
| 恢复时间 > 10min | 严重 | 人工介入 |

## 可靠性检查清单

### 实现新功能时

- [ ] 定义错误处理策略
- [ ] 添加重试逻辑（如适用）
- [ ] 定义降级行为
- [ ] 添加恢复测试

### 修复 Bug 时

- [ ] 根本原因分析
- [ ] 添加防御性检查
- [ ] 添加回归测试
- [ ] 更新文档

## 参考文档

- [quality.md](quality.md) - 质量标准
- [security.md](security.md) - 安全要求
- [../plans/](../plans/) - 设计文档
