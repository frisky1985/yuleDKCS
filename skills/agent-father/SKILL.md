---
name: agent-father
description: 创建和管理 AI Agent/员工的全流程工具。支持一键创建 Agent、飞书群组、工作区配置、员工入职。自动从 openclaw.json 读取配置。使用场景：(1) 创建新 Agent/员工，(2) 批量入职，(3) 群组配置，(4) 岗前培训，(5) 员工管理。
metadata: { "openclaw": { "emoji": "👨‍🍼", "requires": { "bins": ["bash", "mkdir", "cat", "grep", "sed", "tr", "curl", "node"] } } }
---

# 👨‍🍼 Agent Father Skill

**创建新 Agent 和员工的完整解决方案**，自动从 openclaw.json 读取飞书配置，包含 JSON 配置、会话管理、群组配置和岗前培训。

## 🚀 快速开始

### 创建单个员工

```bash
# 基本用法
./scripts/create-employee.sh <姓名> <工号> <电话> [描述] [初始用户]

# 示例：创建客服工程师
./scripts/create-employee.sh "客服工程师" "CS-001" "13800138000" "客服团队"
```

### 批量创建员工

```bash
# 使用批量脚本
./scripts/batch-create.sh employees.csv
```

### 删除员工

```bash
./scripts/delete-agent.sh <agent-id>
```

---

## 📋 核心脚本

### 1. create-employee.sh - 创建员工

**用法：**
```bash
./scripts/create-employee.sh <agent-name> <role> <phone> [description] [initial-user]
```

**参数：**
| 参数 | 必填 | 说明 | 示例 |
|------|------|------|------|
| agent-name | ✅ | Agent 显示名称 | `客服工程师` |
| role | ✅ | 工号/角色代码 | `CS-001` |
| phone | ✅ | 联系电话 | `13800138000` |
| description | ❌ | 群组描述 | `客服团队` |
| initial-user | ❌ | 初始成员用户 ID | `ou_xxx` |

**执行流程：**
1. ✅ 创建飞书群组
2. ✅ 创建独立工作区
3. ✅ 生成 agent.json 配置
4. ✅ 创建 IDENTITY.md 和 SOUL.md
5. ✅ 注册到 openclaw
6. ✅ 更新员工名单

---

### 2. delete-agent.sh - 删除员工

**用法：**
```bash
./scripts/delete-agent.sh <agent-id>
```

**功能：**
- 删除 agent 目录
- 删除工作区
- 从 openclaw.json 移除配置
- 从员工名单移除
- 可选：删除飞书群组

---

### 3. create-feishu-chat.sh - 创建飞书群组

**用法：**
```bash
./scripts/create-feishu-chat.sh --name "群名称" --description "描述" --users "ou_xxx,ou_yyy"
```

**参数：**
- `--name`: 群名称（必填）
- `--description`: 群描述（可选）
- `--users`: 初始成员，逗号分隔（可选）
- `--owner`: 群主用户 ID（可选）
- `--type`: 群类型 public/private/group（默认：group）

**配置：**
自动从 `~/.openclaw/openclaw.json` 读取飞书 App ID 和 App Secret

---

### 4. onboarding.sh - 员工入职培训

**用法：**
```bash
./scripts/onboarding.sh <agent-id>
```

**功能：**
- 生成岗前培训材料
- 配置工作流程
- 设置工作规范

---

## 🔧 工具函数

### list-employees - 列出所有员工

```bash
# 查看员工名单
cat ~/.openclaw/workspace/employees.json | jq '.employees[] | {id, name, phone, chatId}'
```

### get-agent-info - 获取 Agent 信息

```bash
# 查看指定 Agent 配置
cat ~/.openclaw/agents/<agent-id>/agent/agent.json | jq .
```

### check-agent-status - 检查 Agent 状态

```bash
# 检查配置是否有效
openclaw agents list --bindings
```

---

## 📁 目录结构

```
agent-father/
├── SKILL.md                      # 技能文档
├── QUICKSTART.md                 # 快速开始指南
├── scripts/
│   ├── create-employee.sh        # 创建员工（主脚本）
│   ├── delete-agent.sh           # 删除员工
│   ├── create-feishu-chat.sh     # 创建飞书群组
│   ├── onboarding.sh             # 入职培训
│   └── batch-create.sh           # 批量创建
├── references/
│   ├── templates/                # 配置模板
│   └── examples/                 # 使用示例
└── 修复报告.md                    # 问题修复记录
```

---

## 🎯 使用场景

### 场景 1: 创建新岗位

```bash
# 创建产品经理
./scripts/create-employee.sh "产品经理" "PM-001" "13900139000" "产品团队"

# 创建测试工程师
./scripts/create-employee.sh "测试工程师" "QA-001" "13700137000" "测试团队"
```

### 场景 2: 员工离职

```bash
# 删除员工（保留数据）
./scripts/delete-agent.sh cs-001

# 删除员工（包含飞书群组）
./scripts/delete-agent.sh cs-001 --delete-chat
```

---

## ⚠️ 注意事项

### 1. 命名规范

- **Agent ID**: 小写字母 + 数字 + 连字符（如：`cs-001`）
- **工号格式**: 大写字母 + 连字符 + 数字（如：`CS-001`）
- **避免使用**: `main` 是保留名称

### 2. 飞书配置

确保 `~/.openclaw/openclaw.json` 包含飞书配置：

```json
{
  "channels": {
    "feishu": {
      "appId": "cli_xxx",
      "appSecret": "xxx"
    }
  }
}
```

---

## 🔍 故障排查

### 问题 1: "main" is reserved
**解决**: 使用其他名称，如 `cs-001`、`dev-001`

### 问题 2: JSON 配置无效
**解决**: 
```bash
openclaw status
openclaw doctor --fix
```

### 问题 3: 飞书群组创建失败
**解决**:
```bash
cat ~/.openclaw/openclaw.json | grep -A 2 feishu
openclaw channels login --channel feishu
```

---

_用 AI 赋能团队，让每个岗位都有专属智能助手。_
