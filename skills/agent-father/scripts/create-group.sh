#!/bin/bash
# 创建新群组脚本（可移植版本）
# 用法：./create-group.sh <group-name> <members...>

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

WORKSPACE_DIR="$OPENCLAW_BASE/workspace"
if [ ! -d "$WORKSPACE_DIR" ] && [ -d "$OPENCLAW_BASE/workspace-dev" ]; then
    WORKSPACE_DIR="$OPENCLAW_BASE/workspace-dev"
fi

# ========== 参数解析 ==========

GROUP_NAME="$1"
shift
MEMBERS="$*"

if [ -z "$GROUP_NAME" ]; then
    echo "❌ 用法：$0 <group-name> [member1] [member2] ..."
    echo "示例：$0 '项目开发组' '张三' '李四' '王五'"
    echo ""
    echo "当前配置："
    echo "  OPENCLAW_BASE: $OPENCLAW_BASE"
    echo "  WORKSPACE_DIR: $WORKSPACE_DIR"
    exit 1
fi

# ========== 创建群组配置 ==========

GROUP_ID=$(echo "$GROUP_NAME" | tr '[:upper:]' '[:lower:]' | tr ' ' '-' | tr -cd 'a-z0-9-')
GROUP_DIR="$WORKSPACE_DIR/groups/$GROUP_NAME"

mkdir -p "$GROUP_DIR"

echo "📢 创建群组：$GROUP_NAME"
echo "   工作区：$WORKSPACE_DIR"
echo "   群组目录：$GROUP_DIR"
echo "   成员：${MEMBERS:-待添加}"
echo ""

cat > "$GROUP_DIR/GROUP.md" << EOF
# GROUP.md - ${GROUP_NAME}

- **Created:** $(date -Iseconds)
- **Creator:** Agent Father
- **Members:** ${MEMBERS:-待添加}
- **Group ID:** $GROUP_ID

---

## 群组目标

${GROUP_NAME} 致力于：
- 完成相关工作任务
- 促进团队协作沟通
- 提升工作效率

---

## 群组规则

1. 保持专业友好的沟通氛围
2. 及时响应工作相关消息
3. 定期同步工作进度
4. 遇到问题及时沟通

---

_团队协作，共创价值！_
EOF

echo "✅ 群组配置已创建：$GROUP_DIR/GROUP.md"
echo ""
echo "🎉 群组 '$GROUP_NAME' 创建完成！"
echo ""
echo "📋 下一步："
echo "   1. 在飞书中创建实际群组"
echo "   2. 更新成员列表"
echo "   3. 将群 ID 记录到 agent.json"
echo ""
