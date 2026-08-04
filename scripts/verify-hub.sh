#!/bin/bash
# verify-hub.sh — yuleHUB 本地启动验证
#
# 验证 yuleHUB 是否能编译、启动、响应健康检查、处理请求。
# 用法:  bash verify-hub.sh
# 依赖:  go, curl
# 可选:  grpcurl (安装: brew install grpcurl)
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color
pass()  { echo -e "${GREEN}[PASS]${NC} $1"; }
fail()  { echo -e "${RED}[FAIL]${NC} $1"; }
info()  { echo -e "${YELLOW}[INFO]${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HUB_MODULE="$PROJECT_DIR/backend/hub"

GRPC_PORT="${GRPC_PORT:-9090}"
REST_PORT="${REST_PORT:-8080}"
JWT_SECRET="${JWT_SECRET:-verify-hub-test-secret}"
BUILD_OUT="/tmp/yulehub"
EXIT_CODE=0

cleanup() {
    if [ -n "${HUB_PID:-}" ]; then
        info "Stopping yuleHUB (PID=$HUB_PID)..."
        kill "$HUB_PID" 2>/dev/null || true
        wait "$HUB_PID" 2>/dev/null || true
        info "yuleHUB stopped"
    fi
}
trap cleanup EXIT

# ── Phase 1: Build ──
echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Phase 1: Build yuleHUB             ║"
echo "╚══════════════════════════════════════╝"

cd "$HUB_MODULE"

if go build -o "$BUILD_OUT" ./cmd/hub/...; then
    pass "yuleHUB compiled successfully"
else
    fail "yuleHUB build failed"
    exit 1
fi

# ── Phase 2: Start ──
echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Phase 2: Start yuleHUB             ║"
echo "╚══════════════════════════════════════╝"

# Ensure ports are free
for port in "$GRPC_PORT" "$REST_PORT"; do
    if lsof -ti :"$port" &>/dev/null; then
        fail "Port $port is already in use"
        exit 1
    fi
done

export JWT_SECRET
info "Starting yuleHUB (gRPC=:$GRPC_PORT, REST=:$REST_PORT)..."
"$BUILD_OUT" \
    --grpc-port="$GRPC_PORT" \
    --rest-port="$REST_PORT" \
    --log-level=warn &
HUB_PID=$!

# Wait for hub to start
for i in $(seq 1 10); do
    if curl -sf "http://localhost:$REST_PORT/healthz" >/dev/null 2>&1; then
        pass "yuleHUB health endpoint responding (attempt $i)"
        break
    fi
    if [ $i -eq 10 ]; then
        fail "yuleHUB failed to start within 10 seconds"
        exit 1
    fi
    sleep 1
done

info "yuleHUB running (PID=$HUB_PID)"

# ── Phase 3: Health Check ──
echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Phase 3: Health Check              ║"
echo "╚══════════════════════════════════════╝"

# 3a. /health (simple)
HEALTH_SIMPLE=$(curl -sf "http://localhost:$REST_PORT/health" 2>/dev/null || echo "")
if echo "$HEALTH_SIMPLE" | grep -q '"status":"ok"'; then
    pass "/health endpoint: $HEALTH_SIMPLE"
else
    fail "/health endpoint returned: ${HEALTH_SIMPLE:-<empty>}"
    EXIT_CODE=1
fi

# 3b. /healthz (detailed)
HEALTHZ=$(curl -sf "http://localhost:$REST_PORT/healthz" 2>/dev/null || echo "")
if echo "$HEALTHZ" | grep -q '"status":"healthy"'; then
    pass "/healthz endpoint healthy"
    info "  Details: $(echo "$HEALTHZ" | python3 -c 'import sys,json; d=json.load(sys.stdin); print(f"version={d[\"version\"]}, uptime={d[\"uptime\"]}, go={d[\"go_version\"]}")' 2>/dev/null || echo "$HEALTHZ")"
else
    fail "/healthz endpoint returned: ${HEALTHZ:-<empty>}"
    EXIT_CODE=1
fi

# ── Phase 4: Login & Auth Flow ──
echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Phase 4: Auth & API Flow           ║"
echo "╚══════════════════════════════════════╝"

# 4a. Login with default admin credentials
LOGIN_RESP=$(curl -sf -X POST "http://localhost:$REST_PORT/api/v1/auth/login" \
    -H 'Content-Type: application/json' \
    -d '{"user_id":"admin","password":"admin123"}' 2>/dev/null || echo "")
TOKEN=$(echo "$LOGIN_RESP" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("token",""))' 2>/dev/null || echo "")

if [ -n "$TOKEN" ]; then
    pass "Login successful, received JWT token"
else
    fail "Login failed: ${LOGIN_RESP:-<empty>}"
    EXIT_CODE=1
fi

# 4b. Test an authenticated endpoint (will fail with 503 since no gRPC conn, but that's expected flow)
if [ -n "$TOKEN" ]; then
    # /api/v1/keys requires gRPC conn which we don't have — expect 503, not 401/403
    KEYS_RESP=$(curl -s "http://localhost:$REST_PORT/api/v1/keys" \
        -H "Authorization: Bearer $TOKEN" 2>/dev/null || echo "")
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:$REST_PORT/api/v1/keys" \
        -H "Authorization: Bearer $TOKEN" 2>/dev/null || echo "")
    
    if [ "$HTTP_CODE" = "503" ]; then
        pass "Authenticated endpoint returns 503 (expected — no gRPC backend)"
    elif [ "$HTTP_CODE" = "401" ] || [ "$HTTP_CODE" = "403" ]; then
        fail "Authenticated endpoint returned $HTTP_CODE (expected 503, auth works)"
    else
        info "Authenticated endpoint returned HTTP $HTTP_CODE"
    fi
fi

# ── Phase 5: gRPC Check ──
echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Phase 5: gRPC Reflection           ║"
echo "╚══════════════════════════════════════╝"

if command -v grpcurl &>/dev/null; then
    GRPC_SERVICES=$(grpcurl -plaintext "localhost:$GRPC_PORT" list 2>/dev/null || echo "")
    if [ -n "$GRPC_SERVICES" ]; then
        pass "gRPC services available via reflection:"
        echo "$GRPC_SERVICES" | while IFS= read -r svc; do
            info "  - $svc"
        done
    else
        fail "gRPC reflection returned no services"
        EXIT_CODE=1
    fi
else
    info "grpcurl not installed — skipping gRPC verification (install: brew install grpcurl)"
fi

# ── Summary ──
echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Summary                            ║"
echo "╚══════════════════════════════════════╝"

if [ "$EXIT_CODE" -eq 0 ]; then
    pass "yuleHUB verification PASSED"
else
    fail "yuleHUB verification had FAILURES"
fi

exit "$EXIT_CODE"
