#!/usr/bin/env bash
# =============================================================================
# sdk-hub-contract-e2e.sh — SDK × Hub REST 契约固化测试 (Phase 4.2 / W3)
# -----------------------------------------------------------------------------
# 固化 SDK (iOS/Android) 与 Hub REST Gateway 之间的请求形状契约:
#
#   [1] POST /api/v1/auth/login     {"user_id","password"}                  → {token,...}
#   [2] POST /api/v1/keys           {"vehicleId","deviceId","devicePubkey",
#                                    "vendor":"APPLE","protocol":"CCC_DK3",
#                                    "keyType":"OWNER","traceId"}           → {key:{keyId},...}
#   [3] GET  /api/v1/keys/:keyId                                            → {key:{keyId},...}
#   [4] GET  /api/v1/keys                                                    → {keys:[...],...}
#   [5] POST /api/v1/mailbox        {"payload","displayInfo","senderVendor",
#                                    "senderDeviceId","expirationSeconds",
#                                    "maxUpdates","traceId"}                → {mailboxId,...}
#   [6] POST /api/v1/vehicles/:vehicleId/command  {"action":"lock","key_id",  → {cmdId,resultCode}
#                                    "params","source":4,"trace_id"}         (RemoteLock)
#   [7] 同上 action="unlock"                                                  (RemoteUnlock)
#   [8] POST /api/v1/shares        {"keyId","toVendor":"APPLE",             → {shareId,shareCode,
#                                    "accessLevel":{"lock":true,"unlock":true},  sharingUrl}
#                                    "traceId"}
#   [9] POST /api/v1/shares/accept {"shareCode","deviceId","vendor":"APPLE", → {key:{keyId,keyType,
#                                    "devicePubkey"(base64),"traceId"}           status:FRIEND/ACTIVE}}
#   [10] DELETE /api/v1/shares/:shareId                                      → {} (200)
#   [11] DELETE /api/v1/keys/:keyId                                          → {} (200)
#
# 契约要点（SDK 必须遵守，Hub 端由 protojson 强制执行）:
#   - 枚举字段必须传 枚举名字符串 (vendor="APPLE" 而非 "1"; protocol="CCC_DK3" 而非 "3")
#   - 字段名 camelCase (vehicleId / deviceId / devicePubkey / keyType / traceId)
#   - devicePubkey 为 base64 编码的设备公钥
#   - 例外: [6]/[7] sendCommand 是唯一非 protojson 的 REST 端点 (Go struct + snake_case:
#     key_id / trace_id / source=4 数字), 发 camelCase 会丢失字段
#
# 用法:
#   scripts/sdk-hub-contract-e2e.sh          # 完整运行 (默认端口 8080/9090)
#   HUB_PORT=18080 GRPC_PORT=19090 scripts/sdk-hub-contract-e2e.sh
#
# 退出码: 0 = 全部通过; 非 0 = 有失败
# =============================================================================

set -euo pipefail

HUB_PORT="${HUB_PORT:-8080}"
GRPC_PORT="${GRPC_PORT:-9090}"
BASE="http://localhost:${HUB_PORT}/api/v1"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HUB_BIN="${HUB_BIN:-/tmp/yuledkcs-hub}"
HUB_LOG="$(mktemp /tmp/hub-contract.XXXXXX.log)"
PID=""
PASS=0
FAIL=0

# 契约固定请求体 (与 SDK bindKey 逐字段一致)
VEHICLE_ID="LSV-CONTRACT-$(date +%s)"
DEVICE_ID="dev-contract-iphone"
PUBKEY_B64="QmxhaUJsYWlCbGFpQmxhaUJsYWlCbGFpQmxhaUJsYWk="  # 48B base64 (SE P-256 公钥占位)

cleanup() {
  if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
  fi
  rm -f "$HUB_LOG"
}
trap cleanup EXIT

check() {
  local name="$1" cond="$2"
  if [ "$cond" = "true" ]; then
    echo "  ✓ PASS: $name"
    PASS=$((PASS+1))
  else
    echo "  ✗ FAIL: $name"
    FAIL=$((FAIL+1))
  fi
}

# ── 0. 前置: postgres 就绪 ──────────────────────────────────────────────
echo "[0/11] 检查 Postgres..."
if docker exec yuledkcs-postgres pg_isready -U yuledkcs >/dev/null 2>&1; then
  check "Postgres 就绪 (yuledkcs-postgres)" "true"
else
  check "Postgres 就绪" "false"
  echo "  → 请先启动: cd $REPO_ROOT && docker compose up -d postgres"
  exit 1
fi

# ── 0b. 端口空闲 ─────────────────────────────────────────────────────────
if lsof -i :"$HUB_PORT" -i :"$GRPC_PORT" >/dev/null 2>&1; then
  echo "✗ 端口 ${HUB_PORT}/${GRPC_PORT} 被占用 — 请先释放或设置 HUB_PORT/GRPC_PORT"
  exit 1
fi
check "端口 ${HUB_PORT}/${GRPC_PORT} 空闲" "true"

# ── 1. 构建并启动 Hub ────────────────────────────────────────────────────
echo "[1/11] 构建并启动 Hub..."
if [ ! -x "$HUB_BIN" ]; then
  (cd "$REPO_ROOT/backend/cloud/hub" && go build -o "$HUB_BIN" ./cmd/hub)
fi
JWT_SECRET="contract-$(openssl rand -hex 16)"
ADMIN_PASS="contract-admin-$(openssl rand -hex 12)"
DATABASE_URL="postgres://yuledkcs:yuledkcs@localhost:5432/yuledkcs?sslmode=disable" \
JWT_SECRET="$JWT_SECRET" \
ADMIN_USERNAME="contract-admin" \
ADMIN_PASSWORD="$ADMIN_PASS" \
"$HUB_BIN" >"$HUB_LOG" 2>&1 &
PID=$!

# 等待健康检查 (最多 15s)
for i in $(seq 1 30); do
  if curl -sf "http://localhost:${HUB_PORT}/health" >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done
check "Hub 健康检查 (${HUB_PORT})" "$(curl -sf "http://localhost:${HUB_PORT}/health" >/dev/null 2>&1 && echo true || echo false)"

# ── 2. login ──────────────────────────────────────────────────────────────
echo "[2/11] login..."
LOGIN_HTTP=$(curl -s -o /tmp/contract-login.json -w '%{http_code}' -X POST "$BASE/auth/login" \
  -H 'Content-Type: application/json' \
  -d "{\"user_id\":\"contract-admin\",\"password\":\"$ADMIN_PASS\"}")
TOKEN=""
if [ "$LOGIN_HTTP" = "200" ]; then
  TOKEN=$(python3 -c "import sys,json;print(json.load(open('/tmp/contract-login.json'))['token'])" 2>/dev/null || true)
fi
check "login 200 + token 非空" "$([ "$LOGIN_HTTP" = "200" ] && [ -n "$TOKEN" ] && echo true || echo false)"
rm -f /tmp/contract-login.json

# ── 3. bindKey (SDK 请求形状) ────────────────────────────────────────────
echo "[3/11] bindKey (枚举名契约)..."
BIND_HTTP=$(curl -s -o /tmp/contract-bind.json -w '%{http_code}' -X POST "$BASE/keys" \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"vehicleId\":\"$VEHICLE_ID\",\"deviceId\":\"$DEVICE_ID\",\"devicePubkey\":\"$PUBKEY_B64\",\"vendor\":\"APPLE\",\"protocol\":\"CCC_DK3\",\"keyType\":\"OWNER\",\"traceId\":\"contract-e2e-1\"}")
KEY_ID=""
if [ "$BIND_HTTP" = "200" ]; then
  KEY_ID=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-bind.json'));print(d.get('key',{}).get('keyId',''))" 2>/dev/null || true)
fi
check "bindKey 200 + keyId 非空" "$([ "$BIND_HTTP" = "200" ] && [ -n "$KEY_ID" ] && echo true || echo false)"
rm -f /tmp/contract-bind.json

# ── 4. getKey ─────────────────────────────────────────────────────────────
echo "[4/11] getKey..."
GET_HTTP=$(curl -s -o /tmp/contract-get.json -w '%{http_code}' "$BASE/keys/$KEY_ID" \
  -H "Authorization: Bearer $TOKEN")
GOT_KEY_ID=""
if [ "$GET_HTTP" = "200" ]; then
  GOT_KEY_ID=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-get.json'));print(d.get('key',{}).get('keyId',''))" 2>/dev/null || true)
fi
check "getKey 200 + keyId 匹配" "$([ "$GET_HTTP" = "200" ] && [ "$GOT_KEY_ID" = "$KEY_ID" ] && echo true || echo false)"
rm -f /tmp/contract-get.json

# ── 5. listKeys ───────────────────────────────────────────────────────────
echo "[5/11] listKeys..."
LIST_HTTP=$(curl -s -o /tmp/contract-list.json -w '%{http_code}' "$BASE/keys" \
  -H "Authorization: Bearer $TOKEN")
LIST_HIT="false"
if [ "$LIST_HTTP" = "200" ]; then
  LIST_HIT=$(python3 -c "
import sys,json
d=json.load(open('/tmp/contract-list.json'))
print(str(any(k.get('keyId')=='$KEY_ID' for k in d.get('keys',[]))).lower())" 2>/dev/null || echo false)
fi
check "listKeys 200 + 包含新 keyId" "$([ "$LIST_HTTP" = "200" ] && [ "$LIST_HIT" = "true" ] && echo true || echo false)"
rm -f /tmp/contract-list.json

# ── 6. mailbox create (SDK 请求形状) ────────────────────────────────────
echo "[6/11] mailbox create..."
MB_HTTP=$(curl -s -o /tmp/contract-mb.json -w '%{http_code}' -X POST "$BASE/mailbox" \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"payload\":\"cGF5bG9hZC1kYXRh\",\"displayInfo\":\"\",\"senderVendor\":\"APPLE\",\"senderDeviceId\":\"$DEVICE_ID\",\"expirationSeconds\":86400,\"maxUpdates\":10,\"notificationToken\":\"\",\"deviceAttestation\":\"\",\"traceId\":\"contract-e2e-2\"}")
MB_ID=""
if [ "$MB_HTTP" = "200" ]; then
  MB_ID=$(python3 -c "import sys,json;print(json.load(open('/tmp/contract-mb.json')).get('mailboxId',''))" 2>/dev/null || true)
fi
check "mailbox create 200 + mailboxId 非空" "$([ "$MB_HTTP" = "200" ] && [ -n "$MB_ID" ] && echo true || echo false)"
rm -f /tmp/contract-mb.json

# ── 7. remoteLock (SendCommand → 远程锁车) ──────────────────────────────
# 注意: sendCommand 是唯一非 protojson 的 REST 端点 (Go struct + snake_case)
echo "[7/11] remoteLock (SendCommand action=lock, source=4/Remote)..."
LOCK_HTTP=$(curl -s -o /tmp/contract-lock.json -w '%{http_code}' -X POST "$BASE/vehicles/$VEHICLE_ID/command" \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"action\":\"lock\",\"key_id\":\"$KEY_ID\",\"params\":{},\"source\":4,\"trace_id\":\"contract-e2e-3\"}")
LOCK_CMD_ID=""
LOCK_RESULT=""
if [ "$LOCK_HTTP" = "200" ]; then
  LOCK_CMD_ID=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-lock.json'));print(d.get('cmdId',''))" 2>/dev/null || true)
  LOCK_RESULT=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-lock.json'));print(d.get('resultCode',''))" 2>/dev/null || true)
fi
echo "  ↳ 请求: POST $BASE/vehicles/$VEHICLE_ID/command {\"action\":\"lock\",\"source\":4}"
echo "  ↳ 响应: HTTP $LOCK_HTTP  $(cat /tmp/contract-lock.json 2>/dev/null)"
check "remoteLock 200 + cmdId 非空" "$([ "$LOCK_HTTP" = "200" ] && [ -n "$LOCK_CMD_ID" ] && echo true || echo false)"
# protojson 省略零值字段: resultCode 缺省(空)即 0
check "remoteLock resultCode=0" "$([ -z "$LOCK_RESULT" ] || [ "$LOCK_RESULT" = "0" ] && echo true || echo false)"
rm -f /tmp/contract-lock.json

# ── 8. remoteUnlock (SendCommand → 远程解锁) ────────────────────────────
echo "[8/11] remoteUnlock (SendCommand action=unlock, source=4/Remote)..."
UNLOCK_HTTP=$(curl -s -o /tmp/contract-unlock.json -w '%{http_code}' -X POST "$BASE/vehicles/$VEHICLE_ID/command" \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"action\":\"unlock\",\"key_id\":\"$KEY_ID\",\"params\":{},\"source\":4,\"trace_id\":\"contract-e2e-4\"}")
UNLOCK_CMD_ID=""
UNLOCK_RESULT=""
if [ "$UNLOCK_HTTP" = "200" ]; then
  UNLOCK_CMD_ID=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-unlock.json'));print(d.get('cmdId',''))" 2>/dev/null || true)
  UNLOCK_RESULT=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-unlock.json'));print(d.get('resultCode',''))" 2>/dev/null || true)
fi
echo "  ↳ 请求: POST $BASE/vehicles/$VEHICLE_ID/command {\"action\":\"unlock\",\"source\":4}"
echo "  ↳ 响应: HTTP $UNLOCK_HTTP  $(cat /tmp/contract-unlock.json 2>/dev/null)"
check "remoteUnlock 200 + cmdId 非空" "$([ "$UNLOCK_HTTP" = "200" ] && [ -n "$UNLOCK_CMD_ID" ] && echo true || echo false)"
# protojson 省略零值字段: resultCode 缺省(空)即 0
check "remoteUnlock resultCode=0" "$([ -z "$UNLOCK_RESULT" ] || [ "$UNLOCK_RESULT" = "0" ] && echo true || echo false)"
rm -f /tmp/contract-unlock.json

# ── 9. createShare (protojson: 枚举名 + camelCase + base64) ─────────────
echo "[9/11] createShare (枚举名契约)..."
CS_HTTP=$(curl -s -o /tmp/contract-cs.json -w '%{http_code}' -X POST "$BASE/shares" \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"keyId\":\"$KEY_ID\",\"toVendor\":\"APPLE\",\"accessLevel\":{\"lock\":true,\"unlock\":true},\"traceId\":\"contract-e2e-5\"}")
SHARE_ID=""
SHARE_CODE=""
SHARE_URL=""
if [ "$CS_HTTP" = "200" ]; then
  SHARE_ID=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-cs.json'));print(d.get('shareId',''))" 2>/dev/null || true)
  SHARE_CODE=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-cs.json'));print(d.get('shareCode',''))" 2>/dev/null || true)
  SHARE_URL=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-cs.json'));print(d.get('sharingUrl',''))" 2>/dev/null || true)
fi
echo "  ↳ 请求: POST $BASE/shares {\"keyId\":\"$KEY_ID\",\"toVendor\":\"APPLE\",\"accessLevel\":{\"lock\":true,\"unlock\":true}}"
echo "  ↳ 响应: HTTP $CS_HTTP  $(cat /tmp/contract-cs.json 2>/dev/null)"
check "createShare 200 + shareId 非空" "$([ "$CS_HTTP" = "200" ] && [ -n "$SHARE_ID" ] && echo true || echo false)"
check "createShare shareCode 非空 (6位分享码)" "$([ "$CS_HTTP" = "200" ] && [ -n "$SHARE_CODE" ] && echo true || echo false)"
check "createShare 附带 sharingUrl (CCC mailbox)" "$([ "$CS_HTTP" = "200" ] && [ -n "$SHARE_URL" ] && echo true || echo false)"
rm -f /tmp/contract-cs.json

# ── 10. acceptShare (凭 shareCode 领取 → FRIEND 密钥) ───────────────────
echo "[10/11] acceptShare (shareCode → FRIEND key)..."
AS_HTTP=$(curl -s -o /tmp/contract-as.json -w '%{http_code}' -X POST "$BASE/shares/accept" \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"shareCode\":\"$SHARE_CODE\",\"deviceId\":\"dev-contract-android\",\"vendor\":\"APPLE\",\"devicePubkey\":\"$PUBKEY_B64\",\"traceId\":\"contract-e2e-6\"}")
ACCEPTED_KEY_ID=""
ACCEPTED_KEY_TYPE=""
ACCEPTED_STATUS=""
if [ "$AS_HTTP" = "200" ]; then
  ACCEPTED_KEY_ID=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-as.json'));print(d.get('key',{}).get('keyId',''))" 2>/dev/null || true)
  ACCEPTED_KEY_TYPE=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-as.json'));print(d.get('key',{}).get('keyType',''))" 2>/dev/null || true)
  ACCEPTED_STATUS=$(python3 -c "import sys,json;d=json.load(open('/tmp/contract-as.json'));print(d.get('key',{}).get('status',''))" 2>/dev/null || true)
fi
echo "  ↳ 请求: POST $BASE/shares/accept {\"shareCode\":\"$SHARE_CODE\",\"vendor\":\"APPLE\",\"devicePubkey\":base64}"
echo "  ↳ 响应: HTTP $AS_HTTP  $(cat /tmp/contract-as.json 2>/dev/null)"
check "acceptShare 200 + keyId 非空" "$([ "$AS_HTTP" = "200" ] && [ -n "$ACCEPTED_KEY_ID" ] && echo true || echo false)"
check "acceptShare keyType=FRIEND" "$([ "$ACCEPTED_KEY_TYPE" = "FRIEND" ] && echo true || echo false)"
check "acceptShare status=ACTIVE" "$([ "$ACCEPTED_STATUS" = "ACTIVE" ] && echo true || echo false)"
rm -f /tmp/contract-as.json

# ── 11. cancelShare + unbindKey (DELETE 类) ─────────────────────────────
echo "[11/11] cancelShare + unbindKey..."
CANCEL_HTTP=$(curl -s -o /tmp/contract-cancel.json -w '%{http_code}' -X DELETE "$BASE/shares/$SHARE_ID" \
  -H "Authorization: Bearer $TOKEN")
echo "  ↳ 请求: DELETE $BASE/shares/$SHARE_ID"
echo "  ↳ 响应: HTTP $CANCEL_HTTP  $(cat /tmp/contract-cancel.json 2>/dev/null)"
check "cancelShare 200" "$([ "$CANCEL_HTTP" = "200" ] && echo true || echo false)"
rm -f /tmp/contract-cancel.json

UNBIND_HTTP=$(curl -s -o /tmp/contract-unbind.json -w '%{http_code}' -X DELETE "$BASE/keys/$KEY_ID" \
  -H "Authorization: Bearer $TOKEN")
UNBIND_ERR=""
if [ "$UNBIND_HTTP" = "200" ]; then
  UNBIND_ERR=$(python3 -c "import sys,json;print(json.load(open('/tmp/contract-unbind.json')).get('errorCode',''))" 2>/dev/null || true)
fi
echo "  ↳ 请求: DELETE $BASE/keys/$KEY_ID"
echo "  ↳ 响应: HTTP $UNBIND_HTTP  $(cat /tmp/contract-unbind.json 2>/dev/null)"
check "unbindKey 200 + errorCode 为空" "$([ "$UNBIND_HTTP" = "200" ] && [ -z "$UNBIND_ERR" ] && echo true || echo false)"

# 状态确认: hub 当前语义 = 适配器侧解绑, store 记录保留 → getKey 仍 200
AFTER_HTTP=$(curl -s -o /tmp/contract-after.json -w '%{http_code}' "$BASE/keys/$KEY_ID" \
  -H "Authorization: Bearer $TOKEN")
echo "  ↳ 状态确认: GET $BASE/keys/$KEY_ID → HTTP $AFTER_HTTP"
check "unbind 后 getKey 仍可查询 (记录保留语义)" "$([ "$AFTER_HTTP" = "200" ] && echo true || echo false)"
rm -f /tmp/contract-unbind.json /tmp/contract-after.json

# ── 汇总 ─────────────────────────────────────────────────────────────────
echo "=================================================="
echo "   PASS: $PASS   FAIL: $FAIL"
echo "=================================================="
[ "$FAIL" -eq 0 ]
