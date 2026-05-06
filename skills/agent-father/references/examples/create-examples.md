# 📝 创建 Agent 示例

## 示例 1: 创建客服工程师

```bash
./scripts/create-employee.sh "客服工程师" "CS-001" "13800138000" "客服团队"
```

## 示例 2: 创建带初始用户的群组

```bash
./scripts/create-employee.sh "产品经理" "PM-001" "13900139000" "产品团队" "ou_xxx"
```

## 示例 3: 批量创建

```bash
./scripts/batch-create.sh references/examples/employees-sample.csv
```

## 示例 4: 创建后验证

```bash
# 查看员工名单
cat ~/.openclaw/workspace/employees.json | jq '.employees[] | {id, name, chatId}'

# 查看 Agent 配置
cat ~/.openclaw/agents/cs-001/agent/agent.json | jq .

# 检查 Agent 状态
openclaw agents list --bindings
```
