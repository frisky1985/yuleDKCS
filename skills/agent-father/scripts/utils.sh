#!/bin/bash
# utils.sh - Agent Father 工具函数库

OPENCLAW_BASE="${OPENCLAW_BASE:-$HOME/.openclaw}"

# 列出所有员工
list_employees() {
    local employees_file="$OPENCLAW_BASE/workspace/employees.json"
    
    if [ ! -f "$employees_file" ]; then
        echo "❌ 员工名单不存在：$employees_file"
        return 1
    fi
    
    echo "📋 员工名单："
    echo ""
    if command -v jq &> /dev/null; then
        jq -r '.employees[] | "  \(.id) - \(.name) (\(.phone))"' "$employees_file"
    else
        grep -o '"name": "[^"]*"' "$employees_file" | sed 's/"name": "/  - /g'
    fi
}

# 获取 Agent 信息
get_agent_info() {
    local agent_id="$1"
    
    if [ -z "$agent_id" ]; then
        echo "❌ 用法：get_agent_info <agent-id>"
        return 1
    fi
    
    local agent_file="$OPENCLAW_BASE/agents/$agent_id/agent/agent.json"
    
    if [ ! -f "$agent_file" ]; then
        echo "❌ Agent 不存在：$agent_id"
        return 1
    fi
    
    echo "📊 Agent 信息：$agent_id"
    echo ""
    if command -v jq &> /dev/null; then
        jq '.' "$agent_file"
    else
        cat "$agent_file"
    fi
}

# 检查 Agent 状态
check_agent_status() {
    echo "🔍 检查 Agent 状态..."
    echo ""
    
    if command -v openclaw &> /dev/null; then
        openclaw agents list --bindings 2>&1 | head -30
    else
        echo "❌ openclaw 命令不可用"
        return 1
    fi
}

# 验证配置
validate_config() {
    echo "🔍 验证 openclaw.json 配置..."
    
    if command -v python3 &> /dev/null; then
        python3 -c "import json; json.load(open('$OPENCLAW_BASE/openclaw.json'))" 2>&1
        if [ $? -eq 0 ]; then
            echo "✅ 配置有效"
        else
            echo "❌ 配置无效"
            return 1
        fi
    else
        echo "⚠️ 无法验证（需要 python3）"
    fi
}

# 显示帮助
show_help() {
    cat << HELP
Agent Father 工具函数库

用法：
  source scripts/utils.sh
  list_employees              # 列出所有员工
  get_agent_info <agent-id>   # 获取 Agent 信息
  check_agent_status          # 检查 Agent 状态
  validate_config             # 验证配置

示例：
  source scripts/utils.sh
  list_employees
  get_agent_info cs-001
  check_agent_status
HELP
}

# 如果直接执行，显示帮助
if [ "${BASH_SOURCE[0]}" == "${0}" ]; then
    show_help
fi
