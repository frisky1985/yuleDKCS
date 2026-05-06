#!/bin/bash
# 创建新 Agent 脚本（可移植版本）
# 用法：./create-agent.sh <agent-name> <role> <phone> [group-id]

set -e

# ========== 配置检测 ==========

# 1. 尝试从环境变量读取基础路径
OPENCLAW_BASE="${OPENCLAW_BASE:-}"

# 2. 尝试从 openclaw 命令获取配置
if [ -z "$OPENCLAW_BASE" ] && command -v openclaw &> /dev/null; then
    # 尝试获取 openclaw 配置
    OPENCLAW_BASE="$(openclaw config get baseDir 2>/dev/null || echo "")"
fi

# 3. 尝试从常见位置推断
if [ -z "$OPENCLAW_BASE" ]; then
    # 从脚本位置推断（skills 在 openclaw 目录下）
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    # skills/agent-father/scripts -> 向上三级到 openclaw 根目录
    OPENCLAW_BASE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
fi

# 4. 最后使用默认值（相对路径，支持用户自定义）
if [ -z "$OPENCLAW_BASE" ]; then
    OPENCLAW_BASE="$HOME/.openclaw"
fi

# 验证路径是否存在
if [ ! -d "$OPENCLAW_BASE" ]; then
    echo "❌ 错误：OpenClaw 基础目录不存在：$OPENCLAW_BASE"
    echo ""
    echo "请设置环境变量 OPENCLAW_BASE 指向正确的路径："
    echo "  export OPENCLAW_BASE=/path/to/your/.openclaw"
    echo ""
    echo "或者确保 openclaw 命令可用且已正确配置"
    exit 1
fi

# 构建路径
BASE_AGENTS_DIR="$OPENCLAW_BASE/agents"
WORKSPACE_DIR="$OPENCLAW_BASE/workspace"

# 如果 workspace 不存在，使用 workspace-dev 作为备选
if [ ! -d "$WORKSPACE_DIR" ] && [ -d "$OPENCLAW_BASE/workspace-dev" ]; then
    WORKSPACE_DIR="$OPENCLAW_BASE/workspace-dev"
fi

# ========== 参数解析 ==========

AGENT_NAME="$1"
ROLE="$2"
PHONE="$3"
GROUP_ID="$4"

if [ -z "$AGENT_NAME" ] || [ -z "$ROLE" ]; then
    echo "❌ 用法：$0 <agent-name> <role> <phone> [group-id]"
    echo "示例：$0 '客服工程师' 'CS-001' '13800138000' 'oc_xxx'"
    echo ""
    echo "环境变量："
    echo "  OPENCLAW_BASE - OpenClaw 基础目录（默认：\$HOME/.openclaw）"
    echo ""
    echo "当前配置："
    echo "  OPENCLAW_BASE: $OPENCLAW_BASE"
    echo "  BASE_AGENTS_DIR: $BASE_AGENTS_DIR"
    echo "  WORKSPACE_DIR: $WORKSPACE_DIR"
    exit 1
fi

# ========== Agent ID 生成（兼容性好） ==========

# 将 ROLE 转换为小写并替换空格为连字符
# 使用 tr 而不是 Bash 4.0+ 的 ${var,,} 语法，提高兼容性
AGENT_ID=$(echo "$ROLE" | tr '[:upper:]' '[:lower:]' | tr ' ' '-' | tr -cd 'a-z0-9-')

# 验证 AGENT_ID
if [ -z "$AGENT_ID" ]; then
    echo "❌ 错误：无法从 ROLE='$ROLE' 生成有效的 Agent ID"
    echo "ROLE 应包含字母或数字"
    exit 1
fi

# ========== 目录结构 ==========

AGENT_BASE_DIR="$BASE_AGENTS_DIR/$AGENT_ID"
AGENT_DIR="$AGENT_BASE_DIR/agent"

# 工作区 Agent 目录（使用中文名称，需要清理特殊字符）
WORKSPACE_AGENT_NAME=$(echo "$AGENT_NAME" | tr -d '/*?"<>|:')
WORKSPACE_AGENT_DIR="$WORKSPACE_DIR/agents/$WORKSPACE_AGENT_NAME"

# ========== 主流程 ==========

# 创建目录结构
mkdir -p "$AGENT_DIR"
mkdir -p "$WORKSPACE_AGENT_DIR"

echo "🏗️ 创建 Agent: $AGENT_NAME ($ROLE)"
echo "   OpenClaw 基础目录：$OPENCLAW_BASE"
echo "   Agent 配置目录：$AGENT_DIR"
echo "   工作区目录：$WORKSPACE_AGENT_DIR"
echo ""

# 1. 创建 agent.json（包含会话 ID、群 ID、联系方式）
SESSION_ID="session_${AGENT_ID}_$(date +%s)"
CHAT_ID="${GROUP_ID:-待创建}"

cat > "$AGENT_DIR/agent.json" << EOF
{
  "id": "$AGENT_ID",
  "name": "$AGENT_NAME",
  "workspace": "$WORKSPACE_DIR",
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

echo "✅ agent.json 已创建"
echo "   会话 ID: $SESSION_ID"
echo "   群 ID: $CHAT_ID"
echo "   联系方式：$PHONE"

# 2. 创建 IDENTITY.md
cat > "$WORKSPACE_AGENT_DIR/IDENTITY.md" << EOF
# IDENTITY.md - ${AGENT_NAME}身份

- **Name:** ${AGENT_NAME}
- **Creature:** AI Agent
- **Vibe:** 专业、友好、高效
- **Emoji:** 🤖
- **Avatar:** avatars/agent.png

---

## 职责

- 根据角色定义执行任务
- 与团队成员协作
- 持续学习和改进

---

_用 AI 创造价值，用服务赢得信任。_
EOF

echo "✅ IDENTITY.md 已创建"

# 3. 创建 SOUL.md
cat > "$WORKSPACE_AGENT_DIR/SOUL.md" << EOF
# SOUL.md - ${AGENT_NAME}的灵魂

## 核心职责

**${ROLE}** - 负责相关工作领域的任务执行

## 工作方向

### 1. 核心任务
- 根据需求完成工作任务
- 保证工作质量和效率

### 2. 协作沟通
- 与团队成员保持良好沟通
- 及时同步工作进度

### 3. 持续学习
- 学习新技能和知识
- 不断提升工作能力

## 行动准则

1. 每日汇报工作进度
2. 遇到问题及时沟通
3. 持续优化工作方法

---

_用专业创造价值，用态度赢得信任。_
EOF

echo "✅ SOUL.md 已创建"

# 4. 更新员工名单
EMPLOYEE_LIST="$WORKSPACE_DIR/员工名单.md"
if [ ! -f "$EMPLOYEE_LIST" ]; then
    cat > "$EMPLOYEE_LIST" << EOF
# 员工名单

| 工号 | 姓名 | 角色 | 联系方式 | 群 ID | 会话 ID | 状态 | 创建时间 |
|------|------|------|----------|-------|---------|------|----------|
EOF
    echo "📝 创建员工名单：$EMPLOYEE_LIST"
fi

# 添加员工记录（如果不存在）
if ! grep -q "$ROLE" "$EMPLOYEE_LIST" 2>/dev/null; then
    echo "| $ROLE | $AGENT_NAME | $AGENT_NAME | $PHONE | $CHAT_ID | $SESSION_ID | 待入职 | $(date +%Y-%m-%d) |" >> "$EMPLOYEE_LIST"
    echo "✅ 员工名单已更新"
else
    echo "⚠️ 员工已存在，跳过名单更新"
fi

echo ""
echo "🎉 Agent '$AGENT_NAME' 创建完成！"
echo ""
echo "📋 下一步："
echo "   1. 创建飞书群组（如需要）"
echo "   2. 更新 openclaw.json 配置"
echo "   3. 部署通知系统"
echo "   4. 发送岗前培训材料"
echo ""
echo "📁 文件位置："
echo "   - Agent 配置：$AGENT_DIR/agent.json"
echo "   - 工作区文件：$WORKSPACE_AGENT_DIR/"
echo "   - 员工名单：$EMPLOYEE_LIST"
