#!/bin/bash
# 删除 Agent 脚本
# 用法：./delete-agent.sh <agent-id> [--force]

set -e

# ========== 配置检测 ==========

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

BASE_AGENTS_DIR="$OPENCLAW_BASE/agents"
WORKSPACE_DIR="$OPENCLAW_BASE/workspace"

# ========== 参数解析 ==========

AGENT_ID="$1"
FORCE="${2:-}"

if [ -z "$AGENT_ID" ]; then
    echo "❌ 用法：$0 <agent-id> [--force]"
    echo ""
    echo "参数:"
    echo "  agent-id   - Agent ID（如：cs-001, test-002）"
    echo "  --force    - 强制删除，不询问确认"
    echo ""
    echo "示例:"
    echo "  $0 cs-001"
    echo "  $0 test-002 --force"
    exit 1
fi

# ========== 检查 Agent 是否存在 ==========

AGENT_DIR="$BASE_AGENTS_DIR/$AGENT_ID"
if [ ! -d "$AGENT_DIR" ]; then
    echo "❌ 错误：Agent '$AGENT_ID' 不存在"
    echo "   目录：$AGENT_DIR"
    exit 1
fi

# ========== 显示 Agent 信息 ==========

echo "🔍 准备删除 Agent: $AGENT_ID"
echo "========================================"
echo ""

# 读取 agent.json 信息
AGENT_JSON="$AGENT_DIR/agent.json"
if [ -f "$AGENT_JSON" ]; then
    echo "📊 Agent 信息："
    if command -v node &> /dev/null; then
        node -e "
const fs = require('fs');
const data = JSON.parse(fs.readFileSync('$AGENT_JSON', 'utf-8'));
console.log('   姓名：' + (data.name || 'N/A'));
console.log('   工作区：' + (data.workspace || 'N/A'));
console.log('   创建时间：' + (data.createdAt || 'N/A'));
" 2>/dev/null || echo "   无法读取 agent.json"
    fi
    echo ""
fi

# 查找相关工作区
WORKSPACE_PATH=""
if [ -f "$AGENT_JSON" ] && command -v node &> /dev/null; then
    WORKSPACE_PATH=$(node -e "const fs=require('fs');const d=JSON.parse(fs.readFileSync('$AGENT_JSON','utf-8'));console.log(d.workspace||'');" 2>/dev/null || echo "")
fi

if [ -n "$WORKSPACE_PATH" ] && [ -d "$WORKSPACE_PATH" ]; then
    echo "📁 将删除的目录："
    echo "   1. Agent 目录：$AGENT_DIR"
    echo "   2. 工作区目录：$WORKSPACE_PATH"
    echo ""
else
    echo "📁 将删除的目录："
    echo "   1. Agent 目录：$AGENT_DIR"
    echo ""
fi

# ========== 确认删除 ==========

if [ "$FORCE" != "--force" ] && [ "$FORCE" != "-f" ]; then
    echo "⚠️  警告：此操作不可逆！"
    echo ""
    read -p "确认删除？(y/N): " confirm
    if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
        echo "已取消删除"
        exit 0
    fi
fi

echo ""

# ========== 执行删除 ==========

echo "🗑️  开始删除..."
echo ""

# 1. 删除 Agent 目录
if [ -d "$AGENT_DIR" ]; then
    echo "   删除 Agent 目录：$AGENT_DIR"
    rm -rf "$AGENT_DIR"
    echo "   ✅ 已删除"
fi

# 2. 删除工作区目录
if [ -n "$WORKSPACE_PATH" ] && [ -d "$WORKSPACE_PATH" ]; then
    echo "   删除工作区目录：$WORKSPACE_PATH"
    rm -rf "$WORKSPACE_PATH"
    echo "   ✅ 已删除"
fi

# 3. 从 employees.json 中移除
EMPLOYEE_JSON="$WORKSPACE_DIR/employees.json"
if [ -f "$EMPLOYEE_JSON" ] && command -v node &> /dev/null; then
    echo "   更新员工名单：$EMPLOYEE_JSON"
    node -e "
const fs = require('fs');
const path = '$EMPLOYEE_JSON';
const data = JSON.parse(fs.readFileSync(path, 'utf-8'));
const originalLength = data.employees.length;
data.employees = data.employees.filter(e => e.id !== '$AGENT_ID');
if (data.employees.length < originalLength) {
    fs.writeFileSync(path, JSON.stringify(data, null, 2));
    console.log('   ✅ 已从员工名单中移除');
} else {
    console.log('   ⚠️  员工名单中未找到该 Agent');
}
" 2>/dev/null || echo "   ⚠️  无法更新员工名单"
fi

# 4. 提示更新 openclaw.json
OPENCLAW_CONFIG="$OPENCLAW_BASE/openclaw.json"
echo ""
echo "⚠️  待完成事项："
if [ -f "$OPENCLAW_CONFIG" ]; then
    echo "   请手动更新 openclaw.json，移除以下内容："
    echo ""
    echo "   1. agents.list 中删除 id 为 \"$AGENT_ID\" 的配置"
    echo "   2. bindings 中删除 agentId 为 \"$AGENT_ID\" 的绑定"
    echo "   3. channels.feishu.groups 中删除相关群组配置"
    echo ""
    echo "   或者执行以下命令查看需要删除的内容："
    echo "   grep -n \"$AGENT_ID\" $OPENCLAW_CONFIG"
else
    echo "   openclaw.json 不存在，无需更新"
fi

echo ""
echo "========================================"
echo "✅ Agent '$AGENT_ID' 已成功删除！"
echo ""
