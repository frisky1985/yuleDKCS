# yuleDKCS API 参考文档

**版本**: 1.0.0
**日期**: 2026-05-06

---

## 1. 概述

本文档定义了 yuleDKCS 系统的所有 API 接口，包括：
- REST API (手机 App ↔ HUB)
- gRPC API (HUB ↔ DKCS)
- MQTT API (DKCS ↔ TCU)

---

## 2. REST API (手机 App ↔ HUB)

### 2.1 基础信息

| 项目 | 值 |
|------|-----|
| Base URL | `https://api.yuledkcs.com/v1` |
| 认证方式 | Bearer Token (JWT) |
| 内容类型 | `application/json` |
| 字符编码 | UTF-8 |

### 2.2 通用响应格式

```json
{
  "code": 0,
  "message": "Success",
  "trace_id": "req-abc123def456",
  "timestamp": 1714963200,
  "data": { ... }
}
```

### 2.3 密钥管理 API

#### 2.3.1 创建密钥

**POST** `/keys`

**请求头**:
```
Authorization: Bearer <token>
Content-Type: application/json
```

**请求体**:
```json
{
  "vehicle_id": "VIN123456789",
  "key_type": "owner",
  "device_id": "device-uuid-xxx",
  "protocol": "ccc",
  "valid_from": "2026-05-06T00:00:00Z",
  "valid_until": "2027-05-06T00:00:00Z",
  "permissions": ["unlock", "lock", "engine_start", "trunk_open"]
}
```

**响应**:
```json
{
  "code": 0,
  "message": "Success",
  "trace_id": "req-xxx",
  "data": {
    "key_id": "key-uuid-xxx",
    "key_state": "pending",
    "created_at": "2026-05-06T10:00:00Z",
    "expires_at": "2027-05-06T00:00:00Z"
  }
}
```

#### 2.3.2 激活密钥

**POST** `/keys/{key_id}/activate`

**请求体**:
```json
{
  "activation_code": "ACT-CODE-XXX",
  "device_signature": "base64-encoded-signature"
}
```

**响应**:
```json
{
  "code": 0,
  "message": "Success",
  "data": {
    "key_id": "key-uuid-xxx",
    "key_state": "active",
    "activated_at": "2026-05-06T10:05:00Z"
  }
}
```

#### 2.3.3 获取密钥列表

**GET** `/keys`

**查询参数**:
| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| vehicle_id | string | 否 | 车辆ID过滤 |
| state | string | 否 | 密钥状态过滤 |
| page | int | 否 | 页码，默认1 |
| page_size | int | 否 | 每页数量，默认20 |

**响应**:
```json
{
  "code": 0,
  "data": {
    "keys": [
      {
        "key_id": "key-uuid-1",
        "vehicle_id": "VIN123",
        "key_type": "owner",
        "key_state": "active",
        "device_id": "device-1",
        "created_at": "2026-05-01T00:00:00Z",
        "expires_at": "2027-05-01T00:00:00Z"
      }
    ],
    "total": 5,
    "page": 1,
    "page_size": 20
  }
}
```

#### 2.3.4 吊销密钥

**POST** `/keys/{key_id}/revoke`

**请求体**:
```json
{
  "reason": "设备丢失",
  "revoke_all_related": false
}
```

**响应**:
```json
{
  "code": 0,
  "message": "Success",
  "data": {
    "key_id": "key-uuid-xxx",
    "key_state": "revoked",
    "revoked_at": "2026-05-06T11:00:00Z"
  }
}
```

#### 2.3.5 分享密钥

**POST** `/keys/{key_id}/share`

**请求体**:
```json
{
  "recipient_phone": "+8613800138000",
  "recipient_email": "user@example.com",
  "share_type": "temporary",
  "valid_from": "2026-05-07T00:00:00Z",
  "valid_until": "2026-05-10T00:00:00Z",
  "max_uses": 10,
  "permissions": ["unlock", "lock"]
}
```

**响应**:
```json
{
  "code": 0,
  "data": {
    "share_id": "share-uuid-xxx",
    "share_code": "SHARE-CODE-XXX",
    "share_url": "https://app.yuledkcs.com/accept?code=SHARE-CODE-XXX",
    "expires_at": "2026-05-06T12:00:00Z"
  }
}
```

### 2.4 车辆控制 API

#### 2.4.1 解锁车辆

**POST** `/vehicles/{vehicle_id}/unlock`

**请求体**:
```json
{
  "key_id": "key-uuid-xxx",
  "unlock_type": "all_doors",
  "location": {
    "latitude": 31.2304,
    "longitude": 121.4737
  }
}
```

**响应**:
```json
{
  "code": 0,
  "data": {
    "command_id": "cmd-uuid-xxx",
    "status": "pending",
    "estimated_time": 5
  }
}
```

#### 2.4.2 闭锁车辆

**POST** `/vehicles/{vehicle_id}/lock`

**请求体**:
```json
{
  "key_id": "key-uuid-xxx",
  "lock_type": "all_doors"
}
```

#### 2.4.3 启动引擎

**POST** `/vehicles/{vehicle_id}/engine/start`

**请求体**:
```json
{
  "key_id": "key-uuid-xxx",
  "duration_seconds": 300,
  "climate_control": {
    "enabled": true,
    "temperature": 22,
    "fan_speed": "auto"
  }
}
```

#### 2.4.4 停止引擎

**POST** `/vehicles/{vehicle_id}/engine/stop`

**请求体**:
```json
{
  "key_id": "key-uuid-xxx"
}
```

#### 2.4.5 打开后备箱

**POST** `/vehicles/{vehicle_id}/trunk/open`

**请求体**:
```json
{
  "key_id": "key-uuid-xxx"
}
```

#### 2.4.6 寻车

**POST** `/vehicles/{vehicle_id}/find`

**请求体**:
```json
{
  "key_id": "key-uuid-xxx"
}
```

**响应**:
```json
{
  "code": 0,
  "data": {
    "command_id": "cmd-uuid-xxx",
    "status": "pending",
    "vehicle_location": {
      "latitude": 31.2304,
      "longitude": 121.4737,
      "timestamp": "2026-05-06T10:00:00Z"
    }
  }
}
```

#### 2.4.7 查询指令状态

**GET** `/commands/{command_id}`

**响应**:
```json
{
  "code": 0,
  "data": {
    "command_id": "cmd-uuid-xxx",
    "command_type": "unlock",
    "status": "completed",
    "created_at": "2026-05-06T10:00:00Z",
    "completed_at": "2026-05-06T10:00:05Z",
    "result": {
      "success": true,
      "doors_unlocked": ["driver", "passenger", "rear_left", "rear_right"]
    }
  }
}
```

### 2.5 设备管理 API

#### 2.5.1 注册设备

**POST** `/devices`

**请求体**:
```json
{
  "device_type": "smartphone",
  "platform": "android",
  "platform_version": "14",
  "device_model": "Samsung Galaxy S24",
  "device_name": "我的手机",
  "push_token": "fcm-token-xxx",
  "capabilities": {
    "ble": true,
    "nfc": true,
    "uwb": true,
    "se": true
  }
}
```

**响应**:
```json
{
  "code": 0,
  "data": {
    "device_id": "device-uuid-xxx",
    "created_at": "2026-05-06T10:00:00Z"
  }
}
```

#### 2.5.2 获取设备列表

**GET** `/devices`

### 2.6 事件查询 API

#### 2.6.1 查询事件列表

**GET** `/events`

**查询参数**:
| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| vehicle_id | string | 否 | 车辆ID过滤 |
| event_type | string | 否 | 事件类型过滤 |
| start_time | string | 否 | 开始时间 |
| end_time | string | 否 | 结束时间 |
| page | int | 否 | 页码 |
| page_size | int | 否 | 每页数量 |

**响应**:
```json
{
  "code": 0,
  "data": {
    "events": [
      {
        "event_id": "evt-uuid-xxx",
        "event_type": "unlock",
        "vehicle_id": "VIN123",
        "key_id": "key-uuid-xxx",
        "device_id": "device-uuid-xxx",
        "timestamp": "2026-05-06T10:00:00Z",
        "location": {
          "latitude": 31.2304,
          "longitude": 121.4737
        },
        "result": "success"
      }
    ],
    "total": 100,
    "page": 1,
    "page_size": 20
  }
}
```

---

## 3. gRPC API (HUB ↔ DKCS)

### 3.1 服务定义

```protobuf
syntax = "proto3";

package dkcs;

option go_package = "github.com/digitalkey/proto/dkcs";

// 密钥管理服务
service KeyService {
  // 创建密钥
  rpc CreateKey(CreateKeyRequest) returns (CreateKeyResponse);
  // 激活密钥
  rpc ActivateKey(ActivateKeyRequest) returns (ActivateKeyResponse);
  // 吊销密钥
  rpc RevokeKey(RevokeKeyRequest) returns (RevokeKeyResponse);
  // 获取密钥
  rpc GetKey(GetKeyRequest) returns (GetKeyResponse);
  // 列出密钥
  rpc ListKeys(ListKeysRequest) returns (ListKeysResponse);
  // 分享密钥
  rpc ShareKey(ShareKeyRequest) returns (ShareKeyResponse);
}

// 车控指令服务
service CommandService {
  // 发送指令
  rpc SendCommand(SendCommandRequest) returns (SendCommandResponse);
  // 查询指令状态
  rpc GetCommandStatus(GetCommandStatusRequest) returns (GetCommandStatusResponse);
  // 流式指令（双向流）
  rpc StreamCommands(stream CommandStreamRequest) returns (stream CommandStreamResponse);
}

// 事件服务
service EventService {
  // 记录事件
  rpc RecordEvent(RecordEventRequest) returns (RecordEventResponse);
  // 查询事件
  rpc ListEvents(ListEventsRequest) returns (ListEventsResponse);
  // 事件流（服务端流）
  rpc StreamEvents(StreamEventsRequest) returns (stream Event);
}
```

### 3.2 消息定义

```protobuf
// 通用响应
message Response {
  uint32 code = 1;        // 错误码
  string message = 2;     // 错误信息
  string trace_id = 3;    // 追踪ID
  int64 timestamp = 4;    // 时间戳
}

// 密钥信息
message Key {
  string key_id = 1;
  string vehicle_id = 2;
  string device_id = 3;
  string key_type = 4;    // owner, friend, service
  string key_state = 5;   // pending, active, suspended, revoked, expired
  int64 created_at = 6;
  int64 activated_at = 7;
  int64 expires_at = 8;
  repeated string permissions = 9;
  string protocol = 10;   // ccc, iccoa, icce
}

// 创建密钥请求
message CreateKeyRequest {
  string vehicle_id = 1;
  string device_id = 2;
  string key_type = 3;
  string protocol = 4;
  int64 valid_from = 5;
  int64 valid_until = 6;
  repeated string permissions = 7;
  string trace_id = 8;
}

message CreateKeyResponse {
  Response response = 1;
  Key key = 2;
}

// 车控指令
message Command {
  string command_id = 1;
  string vehicle_id = 2;
  string key_id = 3;
  string command_type = 4;  // unlock, lock, engine_start, engine_stop, trunk_open, find, panic
  bytes payload = 5;        // BER-TLV 编码的指令参数
  int64 created_at = 6;
  int64 timeout_ms = 7;
}

// 发送指令请求
message SendCommandRequest {
  string vehicle_id = 1;
  string key_id = 2;
  string command_type = 3;
  bytes payload = 4;
  int64 timeout_ms = 5;
  string trace_id = 6;
}

message SendCommandResponse {
  Response response = 1;
  string command_id = 2;
  string status = 3;  // pending, sent, completed, failed, timeout
}

// 事件
message Event {
  string event_id = 1;
  string event_type = 2;
  string vehicle_id = 3;
  string key_id = 4;
  string device_id = 5;
  int64 timestamp = 6;
  bytes payload = 7;
  string result = 8;
}
```

---

## 4. MQTT API (DKCS ↔ TCU)

### 4.1 Topic 定义

| Topic | 方向 | 说明 |
|-------|------|------|
| `dkcs/tcu/{vehicle_id}/command` | DKCS → TCU | 指令下发 |
| `dkcs/tcu/{vehicle_id}/response` | TCU → DKCS | 指令响应 |
| `dkcs/tcu/{vehicle_id}/event` | TCU → DKCS | 事件上报 |
| `dkcs/tcu/{vehicle_id}/status` | TCU → DKCS | 状态上报 |

### 4.2 消息格式

所有消息采用 BER-TLV 编码，JSON 格式仅用于调试。

#### 4.2.1 指令消息

**Topic**: `dkcs/tcu/{vehicle_id}/command`

**JSON 示例** (调试用):
```json
{
  "msg_id": "msg-uuid-xxx",
  "msg_type": "command",
  "command_type": "unlock",
  "key_id": "key-uuid-xxx",
  "timestamp": 1714963200000,
  "payload": {
    "unlock_type": "all_doors",
    "auth_token": "base64-encoded-token"
  },
  "signature": "base64-encoded-signature"
}
```

**BER-TLV 结构**:
```
[0x80] Message ID (16 bytes)
[0x81] Message Type (1 byte: 0x01=command)
[0x82] Command Type (1 byte: 0x01=unlock, 0x02=lock, ...)
[0x83] Key ID (16 bytes)
[0x84] Timestamp (8 bytes)
[0x85] Payload (nested TLV)
  [0x90] Unlock Type (1 byte)
  [0x91] Auth Token (variable)
[0x86] Signature (64 bytes)
```

#### 4.2.2 响应消息

**Topic**: `dkcs/tcu/{vehicle_id}/response`

**JSON 示例**:
```json
{
  "msg_id": "msg-uuid-xxx",
  "request_id": "msg-uuid-yyy",
  "result": "success",
  "error_code": 0,
  "error_message": "",
  "timestamp": 1714963201000,
  "payload": {
    "doors_unlocked": ["driver", "passenger", "rear_left", "rear_right"]
  }
}
```

#### 4.2.3 事件消息

**Topic**: `dkcs/tcu/{vehicle_id}/event`

**JSON 示例**:
```json
{
  "msg_id": "msg-uuid-xxx",
  "event_type": "key_bound",
  "key_id": "key-uuid-xxx",
  "device_id": "device-uuid-xxx",
  "timestamp": 1714963200000,
  "payload": {
    "protocol": "ccc",
    "binding_method": "ble"
  }
}
```

### 4.3 QoS 策略

| 消息类型 | QoS | 理由 |
|----------|-----|------|
| 指令消息 | 1 | 至少一次送达 |
| 响应消息 | 1 | 至少一次送达 |
| 事件消息 | 1 | 至少一次送达 |
| 状态消息 | 0 | 最多一次，频繁上报 |

---

## 5. SDK API (手机端)

### 5.1 Android SDK

```kotlin
// 初始化
val client = DigitalKeyClient.Builder()
    .setApiKey("your-api-key")
    .setEnvironment(Environment.PRODUCTION)
    .build()

// 密钥管理
val keyManager = client.keyManager

// 创建密钥
val key = keyManager.createKey(
    vehicleId = "VIN123",
    keyType = KeyType.OWNER,
    permissions = listOf(Permission.UNLOCK, Permission.LOCK)
).await()

// 激活密钥
keyManager.activateKey(key.keyId, activationCode).await()

// 车辆控制
val vehicleManager = client.vehicleManager

// 解锁
val result = vehicleManager.unlock(
    vehicleId = "VIN123",
    keyId = key.keyId
).await()

// BLE 通信
val bleManager = client.bleManager
bleManager.connect(deviceAddress).await()
bleManager.sendCommand(command).await()
```

### 5.2 iOS SDK

```swift
// 初始化
let client = DigitalKeyClient(
    apiKey: "your-api-key",
    environment: .production
)

// 密钥管理
let keyManager = client.keyManager

// 创建密钥
let key = try await keyManager.createKey(
    vehicleId: "VIN123",
    keyType: .owner,
    permissions: [.unlock, .lock]
)

// 激活密钥
try await keyManager.activateKey(keyId: key.keyId, activationCode: code)

// 车辆控制
let vehicleManager = client.vehicleManager

// 解锁
let result = try await vehicleManager.unlock(
    vehicleId: "VIN123",
    keyId: key.keyId
)
```

---

## 6. 错误处理

### 6.1 错误码体系

详见: `backend/cloud/protocol/error-codes.md`

### 6.2 错误响应示例

```json
{
  "code": 777,
  "message": "Distance exceeded",
  "trace_id": "req-abc123",
  "timestamp": 1714963200,
  "data": {
    "max_distance_mm": 5000,
    "actual_distance_mm": 8500
  }
}
```

### 6.3 HTTP 状态码映射

| 错误码范围 | HTTP 状态码 |
|------------|-------------|
| 0x0000 | 200 |
| 0x0100-0x01FF | 400 |
| 0x0200-0x02FF | 401/403 |
| 0x0300-0x03FF | 404/409/403 |
| 0x0400-0x04FF | 404/503/504 |
| 0x0900-0x09FF | 500/503 |

---

## 7. 限流策略

### 7.1 限流规则

| API | 限流 | 窗口 |
|-----|------|------|
| 创建密钥 | 10 次 | 1 分钟 |
| 车控指令 | 60 次 | 1 分钟 |
| 查询接口 | 300 次 | 1 分钟 |

### 7.2 限流响应

```json
{
  "code": 263,
  "message": "Rate limit exceeded",
  "trace_id": "req-xxx",
  "data": {
    "retry_after": 30
  }
}
```

HTTP Header:
```
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1714963260
Retry-After: 30
```

---

## 8. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
