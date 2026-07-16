# yuleDKCS Demo API 调用示例

> **版本**: v1.0 | **日期**: 2026-07-08
> **基础 URL**: `http://localhost:8080/api/v1`

---

## 快速入门

### 设置环境变量

```bash
# 基础 URL
export BASE="http://localhost:8080/api/v1"

# 登录获取 Token
TOKEN=$(curl -s -X POST "$BASE/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"admin123"}' | jq -r '.token')

# 导出 Token 供后续使用
export TOKEN="$TOKEN"
export AUTH="Authorization: Bearer $TOKEN"
```

---

## 1. 认证 API

### 1.1 发送验证码

```bash
curl -s -X POST "$BASE/auth/send-code" \
  -H "Content-Type: application/json" \
  -d '{
    "phone": "+8613912340001",
    "type": "LOGIN"
  }'
```

**成功响应:**
```json
{
  "code": 0,
  "message": "success",
  "data": {
    "codeId": "code_abc123",
    "expiresIn": 300
  }
}
```

### 1.2 手机号登录

```bash
curl -s -X POST "$BASE/auth/login" \
  -H "Content-Type: application/json" \
  -d '{
    "phone": "+8613912340001",
    "code": "666666",
    "deviceInfo": {
      "deviceId": "dev-iphone-01",
      "deviceType": "iOS",
      "osVersion": "18.0",
      "appVersion": "1.0.0",
      "deviceModel": "iPhone 15 Pro"
    }
  }'
```

**成功响应:**
```json
{
  "code": 0,
  "message": "success",
  "data": {
    "userId": "usr_owner_001",
    "accessToken": "eyJhbGciOiJSUzI1NiIs...",
    "refreshToken": "eyJhbGciOiJSUzI1NiIs...",
    "expiresIn": 3600,
    "refreshExpiresIn": 604800
  }
}
```

### 1.3 Token 刷新

```bash
curl -s -X POST "$BASE/auth/token/refresh" \
  -H "Content-Type: application/json" \
  -d '{
    "refreshToken": "eyJhbGciOiJSUzI1NiIs..."
  }'
```

---

## 2. 设备管理

### 2.1 注册设备

```bash
curl -s -X POST "$BASE/devices" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "platform": "ios",
    "model": "iPhone 15 Pro",
    "os_version": "18.0",
    "app_version": "1.2.3",
    "ble": true,
    "uwb": true,
    "nfc": true,
    "secure_element": true
  }'
```

**成功响应:**
```json
{
  "device_id": "dev-iphone-01",
  "platform": "ios",
  "model": "iPhone 15 Pro",
  "ble": true,
  "uwb": true,
  "nfc": true,
  "max_devices": 5
}
```

### 2.2 列出设备

```bash
curl -s "$BASE/devices" \
  -H "$AUTH" | jq '.devices[] | {device_id, model, platform}'
```

### 2.3 查看设备详情

```bash
curl -s "$BASE/devices/dev-iphone-01" \
  -H "$AUTH" | jq .
```

### 2.4 删除设备

```bash
curl -s -X DELETE "$BASE/devices/dev-iphone-01" \
  -H "$AUTH"
```

---

## 3. 车辆管理

### 3.1 绑定车辆

```bash
curl -s -X POST "$BASE/vehicles/bind" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "LSVXXXXXXXDEMO0001",
    "proofType": "QR_CODE",
    "proofData": "demo-pairing-code-001",
    "nickname": "我的汉 EV"
  }'
```

**成功响应:**
```json
{
  "code": 0,
  "message": "success",
  "data": {
    "vehicleId": "veh_demo_001",
    "bindId": "bind_xxx",
    "status": "PENDING",
    "message": "请在车辆中控屏确认配对"
  }
}
```

### 3.2 列出车辆

```bash
curl -s "$BASE/vehicles" \
  -H "$AUTH" | jq '.data.vehicles[] | {vehicleId, model, licensePlate, bindStatus}'
```

### 3.3 查看车辆详情

```bash
curl -s "$BASE/vehicles/VH-REMOTE-001" \
  -H "$AUTH" | jq .
```

### 3.4 获取车辆状态

```bash
curl -s "$BASE/vehicles/VH-REMOTE-001/status" \
  -H "$AUTH" | jq '.status'
```

**成功响应:**
```json
{
  "doors": {
    "driver": "LOCKED",
    "passenger": "LOCKED",
    "rearLeft": "LOCKED",
    "rearRight": "LOCKED",
    "trunk": "CLOSED"
  },
  "engine": "OFF",
  "battery": {
    "level": 85,
    "range": 520,
    "charging": false
  }
}
```

### 3.5 解绑车辆

```bash
curl -s -X DELETE "$BASE/vehicles/veh_demo_001/bind" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{"reason": "更换车辆"}'
```

---

## 4. 钥匙管理

### 4.1 创建钥匙（绑定密钥）

```bash
curl -s -X POST "$BASE/keys" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "vehicle_id": "VH-BIND-001",
    "device_id": "dev-iphone-01",
    "vendor": "APPLE",
    "protocol": "CCC_DK3",
    "key_type": "OWNER",
    "access_level": {
      "lock": true,
      "unlock": true,
      "engine": true,
      "trunk": true,
      "window": true,
      "climate": true,
      "find": true,
      "seat": true
    },
    "device_pubkey": "<base64-encoded-public-key-from-device-SE>",
    "valid_from": 1715000000,
    "valid_until": 1815000000,
    "trace_id": "create-key-demo-001"
  }'
```

**成功响应:**
```json
{
  "key": {
    "key_id": "key-ccc-apple-001",
    "vehicle_id": "VH-BIND-001",
    "device_id": "dev-iphone-01",
    "key_type": "OWNER",
    "protocol": "CCC_DK3",
    "status": "ACTIVE",
    "access_level": { "lock": true, "unlock": true, "engine": true, ... },
    "valid_from": 1715000000,
    "valid_until": 1815000000
  },
  "vehicle_pubkey": "<vehicle-public-key-base64>",
  "error_code": ""
}
```

### 4.2 配钥到设备

```bash
curl -s -X POST "$BASE/devices/dev-iphone-01/provision" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "vehicle_id": "VH-PROV-001"
  }'
```

### 4.3 列出钥匙

```bash
# 全部钥匙
curl -s "$BASE/keys" \
  -H "$AUTH" | jq '.keys[] | {key_id, key_type, status, protocol}'

# 按车辆过滤
curl -s "$BASE/keys?vehicle_id=VH-REMOTE-001" \
  -H "$AUTH" | jq .

# 按状态过滤
curl -s "$BASE/keys?status=ACTIVE" \
  -H "$AUTH" | jq '.keys[] | {key_id, status}'
```

### 4.4 查看钥匙详情

```bash
curl -s "$BASE/keys/key-ccc-apple-001" \
  -H "$AUTH" | jq '.key | {key_id, key_type, status, permissions, validity}'
```

### 4.5 暂停钥匙

```bash
curl -s -X PUT "$BASE/keys/key-ccc-apple-001/suspend" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{"reason": "手机丢失"}'
```

### 4.6 恢复钥匙

```bash
curl -s -X PUT "$BASE/keys/key-ccc-apple-001/resume" \
  -H "$AUTH"
```

### 4.7 吊销钥匙

```bash
curl -s -X DELETE "$BASE/keys/key-shared-001" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{"reason": "车辆已出售", "notifyUser": true}'
```

**成功响应:**
```json
{
  "code": 0,
  "message": "success",
  "data": {
    "keyId": "key-shared-001",
    "status": "REVOKED",
    "revokedAt": "2026-07-08T00:00:00Z"
  }
}
```

### 4.8 续期钥匙

```bash
curl -s -X PUT "$BASE/keys/key-ccc-apple-001/renew" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "valid_until": 1825000000
  }'
```

---

## 5. 钥匙分享

### 5.1 创建分享（指定用户）

```bash
curl -s -X POST "$BASE/shares" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "key_id": "key-ccc-apple-001",
    "to_user_id": "usr_family_001",
    "access_level": {
      "lock": true,
      "unlock": true,
      "engine": false,
      "trunk": true,
      "window": false,
      "climate": false,
      "find": false,
      "seat": false
    },
    "valid_from": 1715000000,
    "valid_until": 1715086400,
    "max_uses": 50
  }'
```

### 5.2 创建分享（生成分享码）

```bash
curl -s -X POST "$BASE/shares" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "key_id": "key-ccc-apple-001",
    "access_level": {
      "lock": true,
      "unlock": true,
      "engine": true,
      "trunk": false,
      "window": false,
      "climate": false,
      "find": true,
      "seat": false
    },
    "valid_from": 1715000000,
    "valid_until": 1715086400
  }'
```

**成功响应:**
```json
{
  "share_id": "share-abc-123",
  "share_link": "https://digitalkey.app/s/abc123xyz",
  "share_code": "382914",
  "expires_at": 1715086400
}
```

### 5.3 接受分享

```bash
curl -s -X POST "$BASE/shares/accept" \
  -H "Authorization: Bearer $FAMILY_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "share_code": "382914",
    "device_id": "dev-xiaomi-01",
    "vendor": "XIAOMI",
    "device_pubkey": "<base64-public-key>"
  }'
```

### 5.4 查询分享详情

```bash
curl -s "$BASE/shares/share-abc-123" \
  -H "$AUTH" | jq .
```

### 5.5 取消分享

```bash
curl -s -X DELETE "$BASE/shares/share-abc-123" \
  -H "$AUTH"
```

---

## 6. 车辆控制

### 6.1 解锁

```bash
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "unlock",
    "key_id": "key-ccc-apple-001",
    "params": {"method": "uwb_pke", "distance_mm": 800},
    "source": 3
  }'
```

**成功响应:**
```json
{
  "cmd_id": "ctrl_abc123",
  "result_code": 0,
  "error_msg": ""
}
```

### 6.2 上锁

```bash
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "lock",
    "key_id": "key-ccc-apple-001",
    "params": {},
    "source": 1
  }'
```

### 6.3 启动引擎

```bash
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "engine_on",
    "key_id": "key-ccc-apple-001",
    "params": {"method": "ble_inside"},
    "source": 2
  }'
```

### 6.4 停止引擎

```bash
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "engine_off",
    "key_id": "key-ccc-apple-001",
    "params": {},
    "source": 2
  }'
```

### 6.5 开启后备箱

```bash
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "trunk",
    "key_id": "key-ccc-apple-001",
    "params": {},
    "source": 1
  }'
```

### 6.6 开启空调

```bash
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "climate",
    "key_id": "key-ccc-apple-001",
    "params": {
      "temperature": 22,
      "fanSpeed": 2,
      "defrost": true
    },
    "source": 4
  }'
```

### 6.7 寻车

```bash
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "find",
    "key_id": "key-ccc-apple-001",
    "params": {"duration": 3},
    "source": 4
  }'
```

### 6.8 流式接收车辆状态 (SSE)

```bash
# 使用 curl 订阅 SSE 事件流
curl -s -N "$BASE/vehicles/VH-REMOTE-001/status" \
  -H "$AUTH"
# 每次状态变化推送一行:
# data: {"vehicle_id":"VH-REMOTE-001","lock_status":1,"engine_status":0,"battery_pct":85,...}
```

---

## 7. Token 管理

### 7.1 签发授权 Token（生态服务商场景）

```bash
curl -s -X POST "$BASE/tokens" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "subject_id": "user-valet-001",
    "vehicle_id": "VH-REMOTE-001",
    "permissions": ["lock", "engine", "trunk"],
    "duration": "2h",
    "max_uses": 10
  }'
```

**成功响应:**
```json
{
  "token_id": "tok-valet-001",
  "expires_at": 1715086400,
  "signature": "<signed-token>"
}
```

### 7.2 验证 Token

```bash
curl -s "$BASE/tokens/tok-valet-001" \
  -H "$AUTH" | jq .
```

### 7.3 Token 换发钥匙

```bash
curl -s -X POST "$BASE/tokens/tok-valet-001/exchange" \
  -H "Content-Type: application/json"
```

### 7.4 挂起 Token

```bash
curl -s -X PUT "$BASE/tokens/tok-valet-001/suspend" \
  -H "$AUTH"
```

### 7.5 恢复 Token

```bash
curl -s -X PUT "$BASE/tokens/tok-valet-001/resume" \
  -H "$AUTH"
```

### 7.6 吊销 Token

```bash
curl -s -X DELETE "$BASE/tokens/tok-valet-001" \
  -H "$AUTH"
```

---

## 8. HUB 管理与监控

### 8.1 健康检查（公开）

```bash
curl -s "$BASE/health" | jq .
# {"status":"ok"}
```

### 8.2 适配器状态

```bash
# 查看所有适配器
curl -s "$BASE/hub/adapters" \
  -H "$AUTH" | jq '.adapters[] | {vendor, protocol, healthy, last_check_ms}'
```

**成功响应:**
```json
{
  "healthy": true,
  "adapters": [
    {"vendor": "APPLE",    "protocol": "CCC_DK3",   "healthy": true, "last_check_ms": 1715000000000},
    {"vendor": "SAMSUNG",  "protocol": "CCC_DK3",   "healthy": true, "last_check_ms": 1715000000000},
    {"vendor": "XIAOMI",   "protocol": "ICCOA_DK40", "healthy": true, "last_check_ms": 1715000000000},
    {"vendor": "OPPO",     "protocol": "ICCOA_DK40", "healthy": true, "last_check_ms": 1715000000000},
    {"vendor": "VIVO",     "protocol": "ICCOA_DK40", "healthy": true, "last_check_ms": 1715000000000}
  ]
}
```

---

## 9. 完整 Demo 脚本

一个连续可执行的完整脚, 按步骤展示所有 DEMO 功能:

```bash
#!/bin/bash
set -e

echo "╔═══════════════════════════════════════════════════════╗"
echo "║  yuleDKCS End-to-End Demo Script                     ║"
echo "╚═══════════════════════════════════════════════════════╝"

BASE="http://localhost:8080/api/v1"

# ── Step 1: 健康检查 ──
echo ""
echo "=== Step 1/10: 健康检查 ==="
curl -s "$BASE/health" | jq .
sleep 1

# ── Step 2: 登录 ──
echo ""
echo "=== Step 2/10: 管理员登录 ==="
TOKEN=$(curl -s -X POST "$BASE/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"admin123"}' | jq -r '.token')
echo "Token: ${TOKEN:0:20}..."
AUTH="Authorization: Bearer $TOKEN"

# ── Step 3: 适配器状态 ──
echo ""
echo "=== Step 3/10: 适配器状态 ==="
curl -s "$BASE/hub/adapters" -H "$AUTH" | jq '.adapters[] | {vendor, protocol, healthy}'
sleep 1

# ── Step 4: 设备注册 ──
echo ""
echo "=== Step 4/10: 注册车主设备 ==="
DEVICE_ID=$(curl -s -X POST "$BASE/devices" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{"platform":"ios","model":"iPhone 15 Pro","os_version":"18.0","app_version":"1.2.3","ble":true,"uwb":true,"nfc":true,"secure_element":true}' | jq -r '.device_id')
echo "设备 ID: $DEVICE_ID"

# ── Step 5: 创建钥匙 ──
echo ""
echo "=== Step 5/10: 创建数字钥匙 ==="
KEY_RESP=$(curl -s -X POST "$BASE/keys" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "vehicle_id":"VH-BIND-001",
    "device_id":"'"$DEVICE_ID"'",
    "vendor":"APPLE",
    "protocol":"CCC_DK3",
    "key_type":"OWNER",
    "access_level":{"lock":true,"unlock":true,"engine":true,"trunk":true,"window":true,"climate":true,"find":true,"seat":true},
    "valid_from":1715000000,
    "valid_until":1815000000
  }')
KEY_ID=$(echo "$KEY_RESP" | jq -r '.key.key_id')
echo "钥匙 ID: $KEY_ID"
echo "钥匙状态: $(echo "$KEY_RESP" | jq -r '.key.status')"

# ── Step 6: 无感解锁 ──
echo ""
echo "=== Step 6/10: 无感解锁 (UWB PKE) ==="
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"'"$KEY_ID"'",
    "params":{"method":"uwb_pke","distance_mm":800},
    "source":3
  }' | jq '.result_code'
echo "✅ 解锁成功 (result_code=0)"

# ── Step 7: 启动引擎 ──
echo ""
echo "=== Step 7/10: 启动引擎 ==="
curl -s -X POST "$BASE/vehicles/VH-REMOTE-001/command" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"engine_on",
    "key_id":"'"$KEY_ID"'",
    "params":{"method":"ble_inside"},
    "source":2
  }' | jq '.result_code'
echo "✅ 引擎启动成功"

# ── Step 8: 钥匙分享 ──
echo ""
echo "=== Step 8/10: 钥匙分享 ==="
SHARE_RESP=$(curl -s -X POST "$BASE/shares" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{
    "key_id":"'"$KEY_ID"'",
    "to_user_id":"usr_family_001",
    "access_level":{"lock":true,"unlock":true,"engine":true,"trunk":false,"window":false,"climate":false,"find":false,"seat":false},
    "valid_from":1715000000,
    "valid_until":1715086400,
    "max_uses":50
  }')
SHARE_ID=$(echo "$SHARE_RESP" | jq -r '.share_id')
echo "分享 ID: $SHARE_ID"

# ── Step 9: 吊销钥匙 ──
echo ""
echo "=== Step 9/10: 吊销钥匙 ==="
curl -s -X DELETE "$BASE/keys/$KEY_ID" \
  -H "$AUTH" \
  -H "Content-Type: application/json" \
  -d '{"reason":"Demo 完成","notifyUser":false}' | jq '.data.status'
echo "✅ 钥匙已吊销"

# ── Step 10: 最终检查 ──
echo ""
echo "=== Step 10/10: 最终验证 ==="
curl -s "$BASE/keys" -H "$AUTH" | jq '.keys[] | {key_id, status}'

echo ""
echo "╔═══════════════════════════════════════════════════════╗"
echo "║  🎉 Demo 顺利完成!                                   ║"
echo "╚═══════════════════════════════════════════════════════╝"
```

---

## 错误码速查

| 错误码 | HTTP 状态 | 说明 |
|:-------|:----------|:-----|
| `BAD_REQUEST` | 400 | 请求参数错误 |
| `AUTH_INVALID_TOKEN` | 401 | Token 无效或已过期 |
| `FORBIDDEN` | 403 | 无操作权限 |
| `GRPC_NOT_FOUND` | 404 | 资源不存在 |
| `GRPC_UNAVAILABLE` | 503 | HUB 服务不可用 |
| `GRPC_DEADLINE_EXCEEDED` | 504 | 请求超时 |
| `ERR_RATE_LIMIT` | 429 | 频率限制 |

---

## 数据格式

### 统一请求头

```http
Authorization: Bearer <JWT_TOKEN>
Content-Type: application/json
```

### 统一成功响应

```json
{
  "code": 0,
  "message": "success",
  "data": { ... },
  "requestId": "req_abc123",
  "timestamp": 1715000000000
}
```

### 统一错误响应

```json
{
  "code": 10001,
  "error": "GRPC_NOT_FOUND",
  "message": "resource not found",
  "detail": "key key-nonexistent not found",
  "requestId": "req_abc123"
}
```
