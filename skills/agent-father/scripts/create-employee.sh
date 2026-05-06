#!/bin/bash
# 完整员工创建流程（包含上岗通知）
# 用法：./create-employee.sh <agent-name> <role> <phone> [description] [initial-user]

set -e

OPENCLAW_BASE="${OPENCLAW_BASE:-}"
if [ -z "$OPENCLAW_BASE" ] && command -v openclaw &> /dev/null; then
    OPENCLAW_BASE="$(openclaw config get baseDir 2>/dev/null || echo "")"
fi
if [ -z "$OPENCLAW_BASE" ]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    OPENCLAW_BASE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
fi
if [ -z "$OPENCLAW_BASE" ]; then
    OPENCLAW_BASE="$HOME/.openclaw"
fi
if [ ! -d "$OPENCLAW_BASE" ]; then
    echo "❌ 错误：OpenClaw 基础目录不存在：$OPENCLAW_BASE"
    exit 1
fi

BASE_AGENTS_DIR="$OPENCLAW_BASE/agents"
WORKSPACE_DIR="$OPENCLAW_BASE/workspace"
if [ ! -d "$WORKSPACE_DIR" ] && [ -d "$OPENCLAW_BASE/workspace-dev" ]; then
    WORKSPACE_DIR="$OPENCLAW_BASE/workspace-dev"
fi

AGENT_NAME="$1"
ROLE="$2"
PHONE="$3"
DESCRIPTION="${4:-$AGENT_NAME}"
INITIAL_USER="${5:-}"
INITIAL_USER="${FEISHU_INITIAL_USER:-$INITIAL_USER}"

if [ -z "$AGENT_NAME" ] || [ -z "$ROLE" ] || [ -z "$PHONE" ]; then
    echo "❌ 用法：$0 <agent-name> <role> <phone> [description] [initial-user]"
    exit 1
fi

AGENT_ID=$(echo "$ROLE" | tr '[:upper:]' '[:lower:]' | tr ' ' '-' | tr -cd 'a-z0-9-')

if [ -d "$BASE_AGENTS_DIR/$AGENT_ID" ]; then
    echo "❌ 错误：Agent ID '$AGENT_ID' 已存在！"
    exit 1
fi

WORKSPACE_NAME="workspace-${AGENT_ID}"

echo "🚀 开始创建员工：$AGENT_NAME ($ROLE)"
echo "   Agent ID: $AGENT_ID"
echo "========================================"
echo ""

# 步骤 1: 创建飞书群组
echo "📋 步骤 1: 创建飞书群组"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FEISHU_SCRIPT="$SCRIPT_DIR/create-feishu-chat.sh"

if [ -f "$FEISHU_SCRIPT" ]; then
    USERS_PARAM=""
    if [ -n "$INITIAL_USER" ]; then
        USERS_PARAM="--users \"$INITIAL_USER\""
    fi
    
    echo "   群组名称：$AGENT_NAME"
    CHAT_RESULT=$(bash "$FEISHU_SCRIPT" --name "$AGENT_NAME" --description "$DESCRIPTION" $USERS_PARAM)
    
    if echo "$CHAT_RESULT" | grep -q "✅"; then
        CHAT_ID=$(echo "$CHAT_RESULT" | grep "群 ID:" | awk '{print $3}')
        echo "   ✅ 飞书群组已创建：$CHAT_ID"
    else
        echo "   ⚠️ 群组创建失败"
        CHAT_ID="待创建"
    fi
else
    CHAT_ID="待创建"
fi
echo ""

# 步骤 2: 创建工作区
echo "📋 步骤 2: 创建独立工作区"
WORKSPACE_PATH="$OPENCLAW_BASE/$WORKSPACE_NAME"
mkdir -p "$WORKSPACE_PATH"
echo "   ✅ 工作区已创建：$WORKSPACE_PATH"
echo ""

# 步骤 3: 创建 Agent 配置
echo "📋 步骤 3: 创建 Agent 配置"
AGENT_BASE_DIR="$BASE_AGENTS_DIR/$AGENT_ID"
AGENT_DIR="$AGENT_BASE_DIR/agent"
mkdir -p "$AGENT_DIR"
SESSION_ID="session_${AGENT_ID}_$(date +%s)"

cat > "$AGENT_DIR/agent.json" << EOF
{
  "id": "$AGENT_ID",
  "name": "$AGENT_NAME",
  "workspace": "$WORKSPACE_PATH",
  "agentDir": "$AGENT_DIR",
  "identity": {
    "name": "$AGENT_NAME",
    "theme": "AI $AGENT_NAME",
    "emoji": "🤖",
    "avatar": "avatars/agent.png"
  },
  "model": {
    "primary": "bailian/qwen3.5-plus"
  },
  "contact": {
    "phone": "$PHONE",
    "email": "${AGENT_ID}@openclaw.local"
  },
  "chat": {
    "sessionId": "$SESSION_ID",
    "chatId": "$CHAT_ID",
    "channel": "feishu"
  },
  "status": "pending_onboarding",
  "createdAt": "$(date -Iseconds)",
  "updatedAt": "$(date -Iseconds)"
}
EOF
echo "   ✅ agent.json 已创建"
echo ""

# 步骤 4: 创建工作区文件
echo "📋 步骤 4: 创建工作区文件"
cat > "$WORKSPACE_PATH/IDENTITY.md" << EOF
# IDENTITY.md - ${AGENT_NAME}身份

- **Name:** ${AGENT_NAME}
- **Creature:** AI Agent
- **Vibe:** 专业、友好、高效
- **Emoji:** 🤖

---

## 职责
- 根据角色定义执行任务
- 与团队成员协作
- 持续学习和改进
EOF

cat > "$WORKSPACE_PATH/SOUL.md" << EOF
# SOUL.md - ${AGENT_NAME}的灵魂

## 核心职责
**${ROLE}** - 负责相关工作领域的任务执行

## 行动准则
1. 每日汇报工作进度
2. 遇到问题及时沟通
3. 持续优化工作方法
EOF

echo "   ✅ IDENTITY.md 已创建"
echo "   ✅ SOUL.md 已创建"
echo ""

# 步骤 5: 注册 Agent
echo "📋 步骤 5: 注册 Agent 到 openclaw"
if command -v openclaw &> /dev/null; then
    if openclaw agents add --agent-dir "$AGENT_DIR" --workspace "$WORKSPACE_PATH" --non-interactive "$AGENT_NAME" 2>&1 | grep -v "^\[plugins\]" | grep -v "^Config" | grep -v "^Updated" | grep -v "^Workspace" | grep -v "^Agent" | grep -v "^Sessions"; then
        echo "   ✅ Agent 已成功注册"
        
        if [ "$CHAT_ID" != "待创建" ]; then
            echo "   正在添加群组绑定..."
            openclaw agents bind "$AGENT_ID" --bind "feishu:$CHAT_ID" 2>&1 | grep -v "^\[plugins\]" | grep -v "^Config" | grep -v "^Updated" | grep -v "^Added" || true
            echo "   ✅ 群组绑定已添加"
        fi
    else
        echo "   ⚠️ 注册失败"
    fi
else
    echo "   ❌ openclaw 不可用"
fi
echo ""

# 步骤 6: 更新员工名单
echo "📋 步骤 6: 更新员工名单"
EMPLOYEE_JSON="$WORKSPACE_DIR/employees.json"
if [ ! -f "$EMPLOYEE_JSON" ]; then
    echo '{"employees": []}' > "$EMPLOYEE_JSON"
fi

if command -v node &> /dev/null; then
    node -e "
const fs = require('fs');
const data = JSON.parse(fs.readFileSync('$EMPLOYEE_JSON', 'utf-8'));
const exists = data.employees.find(e => e.id === '$ROLE');
if (!exists) {
    data.employees.push({
        id: '$ROLE', name: '$AGENT_NAME', phone: '$PHONE',
        chatId: '$CHAT_ID', sessionId: '$SESSION_ID',
        workspace: '$WORKSPACE_PATH', agentDir: '$AGENT_DIR',
        status: 'pending_onboarding', createdAt: new Date().toISOString()
    });
}
fs.writeFileSync('$EMPLOYEE_JSON', JSON.stringify(data, null, 2));
console.log('   ✅ 员工信息已添加');
" 2>/dev/null || echo "   ⚠️ 无法更新"
fi
echo ""

# 步骤 7: 发送上岗通知
echo "📋 步骤 7: 发送上岗通知"
if [ "$CHAT_ID" != "待创建" ] && command -v openclaw &> /dev/null; then
    echo "   正在发送通知到：$CHAT_ID"
    
    openclaw agent --to "$CHAT_ID" --message "🎉 欢迎 $AGENT_NAME 正式上岗！

📋 岗位职责：
- 根据角色定义执行任务
- 与团队成员协作
- 持续学习和改进

📝 工作制度：
- 每日汇报工作进度
- 遇到问题及时沟通
- 持续优化工作方法

📚 培训材料：
- IDENTITY.md - 身份职责
- SOUL.md - 工作原则

祝工作顺利！🚀" --deliver 2>&1 | grep -v "^\[plugins\]" | grep -v "^Config" | grep -v "^Updated" | grep -v "^Workspace" | grep -v "^Agent" | grep -v "^Sessions" || echo "   ⚠️ 发送失败"
    
    echo "   ✅ 上岗通知已发送"
else
    echo "   ⏳ 跳过通知发送"
fi
echo ""

# 完成总结
echo "========================================"
echo "✅ 员工创建流程完成！"
echo ""
echo "📊 信息汇总："
echo "   姓名：$AGENT_NAME"
echo "   工号：$ROLE"
echo "   群 ID: $CHAT_ID"
echo "   工作区：$WORKSPACE_PATH"
echo ""
echo "📁 文件位置："
echo "   - agent.json: $AGENT_DIR/agent.json"
echo "   - IDENTITY.md: $WORKSPACE_PATH/IDENTITY.md"
echo "   - SOUL.md: $WORKSPACE_PATH/SOUL.md"
echo ""
