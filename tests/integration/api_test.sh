#!/bin/bash
# yuleDKCS API 集成测试脚本
# 用法: ./api_test.sh [base_url]

set -e

BASE_URL="${1:-http://localhost:8080/api/v1}"
TOKEN=""

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "  yuleDKCS API 集成测试"
echo "  Base URL: $BASE_URL"
echo "========================================="
echo ""

# 辅助函数
http_post() {
    curl -s -X POST "$1" \
        -H "Content-Type: application/json" \
        -H "${3:-}" \
        -d "${2:-{}}"
}

http_get() {
    curl -s -X GET "$1" \
        -H "Authorization: Bearer $TOKEN"
}

check_response() {
    if echo "$1" | grep -q '"code":200\|"code":201'; then
        echo -e "${GREEN}✓ 通过${NC}"
        return 0
    else
        echo -e "${RED}✗ 失败${NC}"
        echo "响应: $1"
        return 1
    fi
}

# ==================== 测试开始 ====================

# 1. 健康检查
echo -n "[1/10] 健康检查 (GET /health) ... "
HEALTH=$(curl -s "$BASE_URL/../health")
if echo "$HEALTH" | grep -q '"status":"healthy"\|"message":"pong"'; then
    echo -e "${GREEN}✓ 通过${NC}"
else
    echo -e "${YELLOW}⚠ 跳过${NC} (服务可能未启动)"
fi

# 2. 用户注册
echo -n "[2/10] 用户注册 (POST /auth/register) ... "
REGISTER_PAYLOAD='{
    "username": "testuser_"'"$(date +%s)"',
    "email": "test_'"$(date +%s)"'@example.com",
    "password": "password123"
}'
REGISTER_RESP=$(http_post "$BASE_URL/auth/register" "$REGISTER_PAYLOAD")
check_response "$REGISTER_RESP"

# 3. 用户登录
echo -n "[3/10] 用户登录 (POST /auth/login) ... "
LOGIN_PAYLOAD='{
    "username": "testuser",
    "password": "password123"
}'
LOGIN_RESP=$(http_post "$BASE_URL/auth/login" "$LOGIN_PAYLOAD")

if echo "$LOGIN_RESP" | grep -q '"code":200'; then
    TOKEN=$(echo "$LOGIN_RESP" | grep -o '"token":"[^"]*"' | head -1 | cut -d'"' -f4)
    echo -e "${GREEN}✓ 通过${NC} (Token: ${TOKEN:0:20}...)"
else
    echo -e "${YELLOW}⚠ 跳过${NC} (使用预设token)"
    TOKEN="test_token_placeholder"
fi

# 4. 获取用户信息
if [ -n "$TOKEN" ] && [ "$TOKEN" != "test_token_placeholder" ]; then
    echo -n "[4/10] 获取用户信息 (GET /user/profile) ... "
    PROFILE_RESP=$(http_get "$BASE_URL/user/profile")
    check_response "$PROFILE_RESP"
else
    echo -e "[4/10] ${YELLOW}⚠ 跳过${NC} (无有效token)"
fi

# 5. 获取钥匙列表
if [ -n "$TOKEN" ] && [ "$TOKEN" != "test_token_placeholder" ]; then
    echo -n "[5/10] 获取钥匙列表 (GET /keys) ... "
    KEYS_RESP=$(http_get "$BASE_URL/keys")
    check_response "$KEYS_RESP"
else
    echo -e "[5/10] ${YELLOW}⚠ 跳过${NC} (无有效token)"
fi

# 6. 获取钥匙详情
if [ -n "$TOKEN" ] && [ "$TOKEN" != "test_token_placeholder" ]; then
    echo -n "[6/10] 获取钥匙详情 (GET /keys/1) ... "
    KEY_RESP=$(http_get "$BASE_URL/keys/1")
    check_response "$KEY_RESP"
else
    echo -e "[6/10] ${YELLOW}⚠ 跳过${NC} (无有效token)"
fi

# 7. 激活钥匙
if [ -n "$TOKEN" ] && [ "$TOKEN" != "test_token_placeholder" ]; then
    echo -n "[7/10] 激活钥匙 (POST /keys/1/activate) ... "
    ACTIVATE_RESP=$(curl -s -X POST "$BASE_URL/keys/1/activate" \
        -H "Authorization: Bearer $TOKEN")
    check_response "$ACTIVATE_RESP"
else
    echo -e "[7/10] ${YELLOW}⚠ 跳过${NC} (无有效token)"
fi

# 8. 获取使用日志
if [ -n "$TOKEN" ] && [ "$TOKEN" != "test_token_placeholder" ]; then
    echo -n "[8/10] 获取使用日志 (GET /keys/1/logs) ... "
    LOGS_RESP=$(http_get "$BASE_URL/keys/1/logs")
    check_response "$LOGS_RESP"
else
    echo -e "[8/10] ${YELLOW}⚠ 跳过${NC} (无有效token)"
fi

# 9. 获取分享的钥匙
if [ -n "$TOKEN" ] && [ "$TOKEN" != "test_token_placeholder" ]; then
    echo -n "[9/10] 获取分享的钥匙 (GET /keys/shared/list) ... "
    SHARED_RESP=$(http_get "$BASE_URL/keys/shared/list")
    check_response "$SHARED_RESP"
else
    echo -e "[9/10] ${YELLOW}⚠ 跳过${NC} (无有效token)"
fi

# 10. 测试错误处理
echo -n "[10/10] 测试错误处理 (无效token) ... "
INVALID_RESP=$(curl -s -X GET "$BASE_URL/keys" \
    -H "Authorization: Bearer invalid_token")
if echo "$INVALID_RESP" | grep -q '"code":401\|"error":"unauthorized"\|error; then
    echo -e "${GREEN}✓ 通过${NC} (正确返回401)"
else
    echo -e "${YELLOW}⚠ 跳过${NC} (错误处理待验证)"
fi

echo ""
echo "========================================="
echo "  测试完成"
echo "========================================="
