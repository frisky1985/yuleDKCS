# 📌 Agent Father 快速参考

## 常用命令

### 创建员工
```bash
./scripts/create-employee.sh "姓名" "工号" "电话" "描述"
```

### 批量创建
```bash
./scripts/batch-create.sh employees.csv
```

### 删除员工
```bash
./scripts/delete-agent.sh <agent-id>
```

### 查看员工
```bash
source scripts/utils.sh
list_employees
```

### 查看 Agent 信息
```bash
get_agent_info <agent-id>
```

### 检查状态
```bash
check_agent_status
```

## CSV 格式
```csv
姓名，工号，电话，描述
客服工程师，CS-001,13800138000，客服团队
```

## 文件位置
- **Agent 配置**: `~/.openclaw/agents/<id>/agent/agent.json`
- **工作区**: `~/.openclaw/workspace-<id>/`
- **员工名单**: `~/.openclaw/workspace/employees.json`

## 故障排查
```bash
# 验证配置
openclaw status

# 修复配置
openclaw doctor --fix

# 查看日志
openclaw logs
```
