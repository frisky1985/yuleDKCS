#!/bin/bash
#
# 飞书群组创建脚本
# 使用飞书 API 创建群聊
# 自动从 openclaw.json 读取配置
#

# ========== 配置检测 ==========

# 1. 优先使用环境变量 OPENCLAW_BASE
OPENCLAW_BASE="${OPENCLAW_BASE:-}"

# 2. 尝试从 openclaw 命令获取
if [ -z "$OPENCLAW_BASE" ] && command -v openclaw &> /dev/null; then
    OPENCLAW_BASE="$(openclaw config get baseDir 2>/dev/null || echo "")"
fi

# 3. 尝试从用户主目录查找
if [ -z "$OPENCLAW_BASE" ] && [ -d "$HOME/.openclaw" ]; then
    OPENCLAW_BASE="$HOME/.openclaw"
fi

# 4. 最后使用默认值
if [ -z "$OPENCLAW_BASE" ]; then
    OPENCLAW_BASE="$HOME/.openclaw"
fi

# 从 openclaw.json 读取飞书配置
OPENCLAW_CONFIG="$OPENCLAW_BASE/openclaw.json"
if [ -f "$OPENCLAW_CONFIG" ]; then
    # 使用 grep 和 sed 提取配置（避免依赖 jq）
    APP_ID=$(grep -o '"appId"[[:space:]]*:[[:space:]]*"[^"]*"' "$OPENCLAW_CONFIG" | head -1 | sed 's/.*"\([^"]*\)"$/\1/')
    APP_SECRET=$(grep -o '"appSecret"[[:space:]]*:[[:space:]]*"[^"]*"' "$OPENCLAW_CONFIG" | head -1 | sed 's/.*"\([^"]*\)"$/\1/')
    
    if [ -n "$APP_ID" ] && [ -n "$APP_SECRET" ]; then
        echo "✅ 已从 openclaw.json 读取飞书配置"
        echo "   APP_ID: $APP_ID"
        echo "   APP_SECRET: [已隐藏]"
    else
        echo "⚠️ 未在 openclaw.json 中找到飞书配置"
        echo "   请手动配置 APP_ID 和 APP_SECRET"
    fi
else
    echo "⚠️ openclaw.json 不存在：$OPENCLAW_CONFIG"
    echo "   请手动配置 APP_ID 和 APP_SECRET"
fi

# 5. 允许环境变量覆盖
APP_ID="${FEISHU_APP_ID:-$APP_ID}"
APP_SECRET="${FEISHU_APP_SECRET:-$APP_SECRET}"

# API 端点
API_BASE="https://open.feishu.cn/open-apis"

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 显示帮助
show_help() {
  cat << EOF
飞书群组创建工具

用法:
  $0 --name "群名称" [--description "描述"] [--users "user1,user2"]

参数:
  --name          群名称（必填）
  --description   群描述（可选）
  --users         初始成员用户 ID，逗号分隔（可选）
  --owner         群主用户 ID（可选）
  --type          群类型：public, private, group（默认：group）
  --help          显示帮助信息

配置:
  自动从 openclaw.json 读取飞书配置
  或使用环境变量：
    FEISHU_APP_ID
    FEISHU_APP_SECRET

示例:
  $0 --name "项目讨论组" --description "项目相关讨论"
  $0 --name "测试群" --users "ou_xxx,ou_yyy"
EOF
}

# 获取访问令牌
get_access_token() {
  local response
  response=$(curl -s -X POST "${API_BASE}/auth/v3/app_access_token/internal" \
    -H "Content-Type: application/json" \
    -d "{
      \"app_id\": \"${APP_ID}\",
      \"app_secret\": \"${APP_SECRET}\"
    }")
  
  local code
  code=$(echo "$response" | grep -o '"code":[0-9]*' | cut -d':' -f2)
  
  if [ "$code" != "0" ]; then
    printf "${RED}获取访问令牌失败${NC}\n"
    echo "$response"
    return 1
  fi
  
  echo "$response" | grep -o '"app_access_token":"[^"]*"' | cut -d'"' -f4
}

# 创建群聊
create_chat() {
  local name="$1"
  local description="$2"
  local users="$3"
  local owner="$4"
  local chat_type="$5"
  
  local token
  token=$(get_access_token)
  
  if [ -z "$token" ]; then
    printf "${RED}获取访问令牌失败${NC}\n"
    return 1
  fi
  
  # 构建请求数据
  local data="{\"name\": \"${name}\", \"chat_type\": \"${chat_type}\""
  
  if [ -n "$description" ]; then
    data="${data}, \"description\": \"${description}\""
  fi
  
  if [ -n "$owner" ]; then
    data="${data}, \"owner_id\": \"${owner}\""
  fi
  
  if [ -n "$users" ]; then
    # 将逗号分隔的用户 ID 转换为数组
    local user_array
    user_array=$(echo "$users" | sed 's/,/","/g')
    data="${data}, \"user_id_list\": [\"${user_array}\"]"
  fi
  
  data="${data}}"
  
  # 调用创建群聊 API
  local response
  response=$(curl -s -X POST "${API_BASE}/im/v1/chats" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer ${token}" \
    -d "$data")
  
  echo "$response"
}

# 解析参数
NAME=""
DESCRIPTION=""
USERS=""
OWNER=""
CHAT_TYPE="group"

while [[ $# -gt 0 ]]; do
  case $1 in
    --name)
      NAME="$2"
      shift 2
      ;;
    --description)
      DESCRIPTION="$2"
      shift 2
      ;;
    --users)
      USERS="$2"
      shift 2
      ;;
    --owner)
      OWNER="$2"
      shift 2
      ;;
    --type)
      CHAT_TYPE="$2"
      shift 2
      ;;
    --help)
      show_help
      exit 0
      ;;
    *)
      echo "未知参数：$1"
      show_help
      exit 1
      ;;
  esac
done

# 检查必填参数
if [ -z "$NAME" ]; then
  printf "${RED}错误：群名称是必填参数${NC}\n"
  show_help
  exit 1
fi

# 创建群聊
echo "正在创建群聊：${NAME}..."
RESULT=$(create_chat "$NAME" "$DESCRIPTION" "$USERS" "$OWNER" "$CHAT_TYPE")

# 解析结果
CODE=$(echo "$RESULT" | grep -o '"code":[0-9]*' | head -1 | cut -d':' -f2)

if [ "$CODE" = "0" ]; then
  CHAT_ID=$(echo "$RESULT" | grep -o '"chat_id":"[^"]*"' | cut -d'"' -f4)
  printf "${GREEN}✅ 群聊创建成功!${NC}\n"
  echo "群 ID: ${CHAT_ID}"
  echo "群名称：${NAME}"
else
  MSG=$(echo "$RESULT" | grep -o '"msg":"[^"]*"' | cut -d'"' -f4)
  printf "${RED}❌ 群聊创建失败${NC}\n"
  echo "错误信息：${MSG}"
  echo "原始响应：${RESULT}"
  exit 1
fi
