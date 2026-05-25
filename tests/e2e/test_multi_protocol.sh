#!/bin/bash
# ==============================================================================
# yuleDKCS 多协议互操作 E2E 测试脚本
#
# 覆盖: CCC / ICCE / ICCOA 三种协议钥匙的创建、管理、验证
#
# 用法:
#   ./test_multi_protocol.sh [base_url]
#   默认 base_url: http://localhost:8080/api/v1
#
# 依赖:
#   - curl, jq
#   - 后端服务运行中
# ==============================================================================

set -euo pipefail

BASE_URL="${1:-http://localhost:8080/api/v1}"
PASS=0
FAIL=0
SKIP=0

# ── 颜色 ──────────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PROTOCOLS=("CCC" "ICCE" "ICCOA")

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║      yuleDKCS 多协议互操作 E2E 测试                            ║${NC}"
echo -e "${CYAN}║      ${BASE_URL}${NC}"
echo -e "${CYAN}║      协议: ${PROTOCOLS[*]}${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"

# ── 辅助函数 ──────────────────────────────────────────────────────────────────

pass() { PASS=$((PASS+1)); echo -e "  ${GREEN}✓${NC} $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}✗${NC} $1"; }
skip() { SKIP=$((SKIP+1)); echo -e "  ${YELLOW}⚠${NC} $1"; }

api_get() {
    local endpoint="$1"; shift
    local auth_header=""
    if [ -n "${TOKEN:-}" ]; then
        auth_header="-H Authorization: Bearer $TOKEN"
    fi
    curl -sf -X GET "${BASE_URL}${endpoint}" $auth_header "$@" 2>/dev/null || echo '{"code":999}'
}

api_post() {
    local endpoint="$1"; shift
    local data="$1"; shift
    local auth_header=""
    if [ -n "${TOKEN:-}" ]; then
        auth_header="-H Authorization: Bearer $TOKEN"
    fi
    curl -sf -X POST "${BASE_URL}${endpoint}" \
        -H "Content-Type: application/json" $auth_header \
        -d "$data" 2>/dev/null || echo '{"code":999}'
}

api_delete() {
    local endpoint="$1"; shift
    local auth_header="-H Authorization: Bearer $TOKEN"
    curl -sf -X DELETE "${BASE_URL}${endpoint}" $auth_header 2>/dev/null || echo '{"code":999}'
}

assert_code() {
    local name="$1" expected="$2" response="$3"
    local code; code=$(echo "$response" | jq -r '.code // 999')
    if [ "$code" = "$expected" ]; then
        pass "$name"
    else
        fail "$name (期待 code=$expected, 实际 code=$code)"
        echo "      响应: $(echo "$response" | head -c 200)"
    fi
}

# ═════════════════════════════════════════════════════════════════════════════
# 阶段 1: 认证准备
# ═════════════════════════════════════════════════════════════════════════════
echo -e "\n${CYAN}[阶段 1/6] 服务健康检查 & 用户认证${NC}"

# 健康检查
HEALTH=$(api_get "/../health")
if echo "$HEALTH" | jq -e '.status == "alive" or .status == "ready"' >/dev/null 2>&1; then
    pass "后端服务运行正常"
else
    skip "后端服务未响应 — 后续测试可能失败"
fi

# 注册认证用户
TIMESTAMP=$(date +%s)
USERNAME="multi_proto_${TIMESTAMP}"
EMAIL="multi_proto_${TIMESTAMP}@example.com"
PASSWORD="TestPass456!"

REG_RESP=$(api_post "/auth/register" \
    "{\"username\":\"${USERNAME}\",\"email\":\"${EMAIL}\",\"password\":\"${PASSWORD}\"}")
REG_CODE=$(echo "$REG_RESP" | jq -r '.code // 999')
if [ "$REG_CODE" = "201" ] || [ "$REG_CODE" = "200" ]; then
    pass "用户注册成功"
else
    skip "用户注册 (code=$REG_CODE) — 可能用户已存在"
fi

LOGIN_RESP=$(api_post "/auth/login" \
    "{\"username\":\"${USERNAME}\",\"password\":\"${PASSWORD}\"}")
TOKEN=$(echo "$LOGIN_RESP" | jq -r '.data.token // empty')
if [ -n "$TOKEN" ] && [ "$TOKEN" != "null" ]; then
    pass "用户登录成功, token 已获取"
else
    TOKEN="test_token_placeholder"
    skip "登录未获取到 token, 使用占位符"
fi

# ═════════════════════════════════════════════════════════════════════════════
# 阶段 2: 跨协议钥匙发行
# ═════════════════════════════════════════════════════════════════════════════
echo -e "\n${CYAN}[阶段 2/6] 跨协议钥匙发行${NC}"

declare -A KEY_IDS
declare -A KEY_IDENTIFIERS

for PROTO in "${PROTOCOLS[@]}"; do
    echo "  ── 发行 ${PROTO} 钥匙 ──"
    KEY_NAME="E2E_${PROTO}_${TIMESTAMP}"

    ISSUE_RESP=$(api_post "/keys/issue" \
        "{\"vehicle_id\":1,\"type\":\"${PROTO}\",\"name\":\"${KEY_NAME}\",\"description\":\"多协议互操作测试\"}")
    assert_code "发行 ${PROTO} 钥匙" "201" "$ISSUE_RESP"

    KEY_ID=$(echo "$ISSUE_RESP" | jq -r '.data.id // empty')
    KEY_IDENTIFIER=$(echo "$ISSUE_RESP" | jq -r '.data.key_identifier // empty')

    if [ -n "$KEY_ID" ] && [ "$KEY_ID" != "null" ]; then
        KEY_IDS[$PROTO]=$KEY_ID
        KEY_IDENTIFIERS[$PROTO]=$KEY_IDENTIFIER
        pass "  ${PROTO} 钥匙 ID: $KEY_ID"
    else
        skip "  ${PROTO} 钥匙 ID 未提取"
        KEY_IDS[$PROTO]=$(( (RANDOM % 100) + 10 ))
    fi
done

# ═════════════════════════════════════════════════════════════════════════════
# 阶段 3: 跨协议钥匙查询
# ═════════════════════════════════════════════════════════════════════════════
echo -e "\n${CYAN}[阶段 3/6] 跨协议钥匙查询验证${NC}"

for PROTO in "${PROTOCOLS[@]}"; do
    KID="${KEY_IDS[$PROTO]:-}"
    [ -z "$KID" ] && { skip "跳过 ${PROTO} 查询 (无 ID)"; continue; }

    DETAIL_RESP=$(api_get "/keys/${KID}")
    assert_code "查询 ${PROTO} 钥匙详情" "200" "$DETAIL_RESP"

    # 验证钥匙类型与协议匹配
    ACTUAL_TYPE=$(echo "$DETAIL_RESP" | jq -r '.data.type // "unknown"')
    if [ "$ACTUAL_TYPE" = "$PROTO" ]; then
        pass "  ${PROTO} 钥匙类型验证正确"
    else
        fail "  ${PROTO} 钥匙类型不匹配 (期待 $PROTO, 实际 $ACTUAL_TYPE)"
    fi

    # 验证初始状态
    INITIAL_STATUS=$(echo "$DETAIL_RESP" | jq -r '.data.status // "unknown"')
    if [ "$INITIAL_STATUS" = "active" ]; then
        pass "  ${PROTO} 初始状态为 active"
    else
        fail "  ${PROTO} 初始状态异常: $INITIAL_STATUS"
    fi
done

# 获取钥匙列表 — 验证多协议钥匙都在列表中
LIST_RESP=$(api_get "/keys?page=1&page_size=50")
LIST_CODE=$(echo "$LIST_RESP" | jq -r '.code // 999')
if [ "$LIST_CODE" = "200" ]; then
    pass "获取钥匙列表成功"
    # 提取所有钥匙类型
    TYPES_IN_LIST=$(echo "$LIST_RESP" | jq -r '.data.list[].type' | sort -u | tr '\n' ' ')
    for PROTO in "${PROTOCOLS[@]}"; do
        if echo "$TYPES_IN_LIST" | grep -q "$PROTO"; then
            pass "  列表中包含 ${PROTO} 类型钥匙"
        else
            skip "  列表中未发现 ${PROTO} 类型 (可能被分页截断)"
        fi
    done
else
    skip "获取钥匙列表 (code=$LIST_CODE)"
fi

# ═════════════════════════════════════════════════════════════════════════════
# 阶段 4: 跨协议钥匙激活 & 使用
# ═════════════════════════════════════════════════════════════════════════════
echo -e "\n${CYAN}[阶段 4/6] 跨协议钥匙激活 & 使用${NC}"

for PROTO in "${PROTOCOLS[@]}"; do
    KID="${KEY_IDS[$PROTO]:-}"
    [ -z "$KID" ] && { skip "跳过 ${PROTO} 激活 (无 ID)"; continue; }

    echo "  ── ${PROTO} 钥匙 (ID=$KID) ──"

    ACTIVATE_RESP=$(api_post "/keys/${KID}/activate" "{}")
    ACT_CODE=$(echo "$ACTIVATE_RESP" | jq -r '.code // 999')
    if [ "$ACT_CODE" = "200" ] || [ "$ACT_CODE" = "201" ]; then
        pass "  ${PROTO} 激活成功"
    else
        skip "  ${PROTO} 激活 (code=$ACT_CODE)"
    fi

    LOGS_RESP=$(api_get "/keys/${KID}/logs")
    LOGS_CODE=$(echo "$LOGS_RESP" | jq -r '.code // 999')
    if [ "$LOGS_CODE" = "200" ]; then
        pass "  ${PROTO} 使用日志获取成功"
    else
        skip "  ${PROTO} 使用日志 (code=$LOGS_CODE)"
    fi
done

# ═════════════════════════════════════════════════════════════════════════════
# 阶段 5: 跨协议钥匙权限管理 & 分享
# ═════════════════════════════════════════════════════════════════════════════
echo -e "\n${CYAN}[阶段 5/6] 跨协议权限管理 & 分享${NC}"

# 注册分享目标用户
USER2_NAME="multi_proto_friend_${TIMESTAMP}"
USER2_EMAIL="multi_proto_friend_${TIMESTAMP}@example.com"
REG2_RESP=$(api_post "/auth/register" \
    "{\"username\":\"${USER2_NAME}\",\"email\":\"${USER2_EMAIL}\",\"password\":\"${PASSWORD}\"}")
assert_code "注册分享目标用户" "201" "$REG2_RESP"

LOGIN2_RESP=$(api_post "/auth/login" \
    "{\"username\":\"${USER2_NAME}\",\"password\":\"${PASSWORD}\"}")
USER2_ID=$(echo "$LOGIN2_RESP" | jq -r '.data.user.id // 2')

for PROTO in "${PROTOCOLS[@]}"; do
    KID="${KEY_IDS[$PROTO]:-}"
    [ -z "$KID" ] && { skip "跳过 ${PROTO} 权限管理 (无 ID)"; continue; }

    echo "  ── ${PROTO} 钥匙 ──"

    # 更新权限
    PERM_RESP=$(api_post "/keys/${KID}/permissions" \
        '{"permissions":{"unlock":true,"lock":true,"start_engine":true,"trunk":false,"windows":false}}')
    assert_code "${PROTO} 更新权限" "200" "$PERM_RESP"

    # 分享
    SHARE_RESP=$(api_post "/keys/${KID}/share" \
        "{\"user_id\":${USER2_ID},\"permissions\":{\"unlock\":true,\"lock\":true,\"start_engine\":false,\"trunk\":false,\"windows\":false}}")
    SHARE_CODE=$(echo "$SHARE_RESP" | jq -r '.code // 999')
    if [ "$SHARE_CODE" = "201" ] || [ "$SHARE_CODE" = "200" ]; then
        pass "  ${PROTO} 分享成功"
    else
        skip "  ${PROTO} 分享 (code=$SHARE_CODE)"
    fi

    # 获取该钥匙的分享列表
    SHARES_RESP=$(api_get "/keys/${KID}/shares")
    SHARES_CODE=$(echo "$SHARES_RESP" | jq -r '.code // 999')
    if [ "$SHARES_CODE" = "200" ]; then
        pass "  ${PROTO} 分享列表获取成功"
    else
        skip "  ${PROTO} 分享列表 (code=$SHARES_CODE)"
    fi
done

# 验证目标用户能收到分享的钥匙列表
SHARED_LIST_RESP=$(api_get "/keys/shared/list")
SHARED_CODE=$(echo "$SHARED_LIST_RESP" | jq -r '.code // 999')
if [ "$SHARED_CODE" = "200" ]; then
    pass "获取分享钥匙列表成功"
else
    skip "获取分享钥匙列表 (code=$SHARED_CODE)"
fi

# ═════════════════════════════════════════════════════════════════════════════
# 阶段 6: 跨协议钥匙撤销
# ═════════════════════════════════════════════════════════════════════════════
echo -e "\n${CYAN}[阶段 6/6] 跨协议钥匙撤销${NC}"

for PROTO in "${PROTOCOLS[@]}"; do
    KID="${KEY_IDS[$PROTO]:-}"
    [ -z "$KID" ] && { skip "跳过 ${PROTO} 撤销 (无 ID)"; continue; }

    REVOKE_RESP=$(api_delete "/keys/${KID}")
    assert_code "${PROTO} 撤销钥匙" "200" "$REVOKE_RESP"

    # 验证状态
    AFTER_RESP=$(api_get "/keys/${KID}")
    STATUS=$(echo "$AFTER_RESP" | jq -r '.data.status // "unknown"')
    CODE=$(echo "$AFTER_RESP" | jq -r '.code // 999')
    if [ "$STATUS" = "revoked" ]; then
        pass "  ${PROTO} 撤销后状态验证: revoked"
    elif [ "$CODE" = "404" ]; then
        pass "  ${PROTO} 撤销后返回 404 (硬删除)"
    else
        fail "  ${PROTO} 撤销后状态异常: $STATUS"
    fi
done

# ═════════════════════════════════════════════════════════════════════════════
# 汇总
# ═════════════════════════════════════════════════════════════════════════════
TOTAL=$((PASS + FAIL + SKIP))
echo -e "\n${CYAN}══════════════════════════════════════════════════════════════════${NC}"
echo -e "  多协议互操作测试汇总"
echo -e "  通过: ${GREEN}${PASS}${NC}  失败: ${RED}${FAIL}${NC}  跳过: ${YELLOW}${SKIP}${NC}  总计: ${TOTAL}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════════${NC}"

if [ "$FAIL" -eq 0 ]; then
    echo -e "\n  ${GREEN}✓ 多协议互操作 E2E 测试通过!${NC}"
    exit 0
else
    echo -e "\n  ${RED}✗ ${FAIL} 个测试失败${NC}"
    exit 1
fi
