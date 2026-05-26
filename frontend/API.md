# API 接口文档

> 前端与后端的所有 RESTful API 接口文档。基础 URL: `/api/v1`

---

## 目录

- [通用说明](#通用说明)
- [认证 API](#认证-api)
- [车辆 API](#车辆-api)
- [数字钥匙 API](#数字钥匙-api)
- [OTA / 固件管理 API](#ota--固件管理-api)
- [用户 API](#用户-api)
- [系统 API](#系统-api)
- [附录: 状态码速查](#附录-状态码速查)
- [附录: 版本历史](#附录-版本历史)

---

## 通用说明

### 请求头

```http
Content-Type: application/json
Authorization: Bearer ***   # 需要认证的接口
```

### 统一响应格式

所有 API 响应遵循以下格式:

```json
{
  "code": 200,
  "message": "success",
  "data": { ... }
}
```

| 字段      | 类型             | 说明                         |
| --------- | ---------------- | ---------------------------- |
| `code`    | `number`         | 状态码 (200=成功, 400=参数错误, 401=未认证, 403=无权限, 404=未找到, 500=服务器错误) |
| `message` | `string`         | 提示信息                     |
| `data`    | `T \| null`      | 响应数据                     |

### 错误响应

```json
{
  "code": 400,
  "message": "参数错误",
  "data": null,
  "details": {
    "email": ["邮箱格式不正确"],
    "password": ["密码长度至少6位"]
  }
}
```

### 分页参数

| 参数    | 类型     | 默认值 | 说明       |
| ------- | -------- | ------ | ---------- |
| `page`  | `number` | `1`    | 页码       |
| `limit` | `number` | `20`   | 每页条数   |

---

## 认证 API

### POST /auth/login

用户登录。

**请求体:**

```json
{
  "username": "admin",
  "password": "password123"
}
```

| 字段       | 类型     | 必填 | 说明     |
| ---------- | -------- | ---- | -------- |
| `username` | `string` | 是   | 用户名   |
| `password` | `string` | 是   | 密码     |

**响应 (200):**

```json
{
  "code": 200,
  "message": "登录成功",
  "data": {
    "token": "eyJhbG...NiIs...",
    "type": "Bearer",
    "user": {
      "id": "1",
      "username": "admin",
      "email": "admin@example.com",
      "role": "admin"
    }
  }
}
```

| 字段        | 类型     | 说明                           |
| ----------- | -------- | ------------------------------ |
| `token`     | `string` | JWT access token               |
| `type`      | `string` | Token 类型，固定 `Bearer`      |
| `user`      | `object` | 用户基本信息                   |

---

### POST /auth/register

用户注册。

**请求体:**

```json
{
  "username": "newuser",
  "email": "user@example.com",
  "password": "password123",
  "phone": "13800138000"
}
```

| 字段       | 类型     | 必填 | 说明           |
| ---------- | -------- | ---- | -------------- |
| `username` | `string` | 是   | 用户名         |
| `email`    | `string` | 是   | 邮箱           |
| `password` | `string` | 是   | 密码 (≥6位)    |
| `phone`    | `string` | 否   | 手机号         |

**响应 (201):**

```json
{
  "code": 201,
  "message": "注册成功",
  "data": {
    "id": "2",
    "username": "newuser",
    "email": "user@example.com"
  }
}
```

---

### POST /auth/refresh

刷新 Token。

**请求头:** 需要 `Authorization: Bearer <旧 token>`

**响应 (200):**

```json
{
  "code": 200,
  "message": "Token 刷新成功",
  "data": {
    "token": "eyJhbG...NiIs...",
    "type": "Bearer"
  }
}
```

| 字段    | 类型     | 说明                      |
| ------- | -------- | ------------------------- |
| `token` | `string` | 新 JWT access token       |
| `type`  | `string` | Token 类型，固定 `Bearer` |

---

## 车辆 API

### POST /vehicles

注册/添加新车辆。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "vin": "LVGBE42KXNG789012",
  "brand": "BYD",
  "model": "汉 EV",
  "year": 2024,
  "color": "极光蓝",
  "plate_number": "京B67890"
}
```

| 字段           | 类型     | 必填 | 说明       |
| -------------- | -------- | ---- | ---------- |
| `vin`          | `string` | 是   | 车架号     |
| `brand`        | `string` | 是   | 品牌       |
| `model`        | `string` | 是   | 型号       |
| `year`         | `number` | 否   | 年份       |
| `color`        | `string` | 否   | 颜色       |
| `plate_number` | `string` | 否   | 车牌号     |

**响应 (201):**

```json
{
  "code": 201,
  "message": "车辆添加成功",
  "data": {
    "id": 4,
    "vin": "LVGBE42KXNG789012",
    "brand": "BYD",
    "model": "汉 EV",
    "year": 2024,
    "color": "极光蓝",
    "plate_number": "京B67890",
    "created_at": "2025-05-25T12:00:00Z"
  }
}
```

---

### GET /vehicles

获取当前用户的所有车辆列表。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数    | 类型     | 默认值 | 说明             |
| ------- | -------- | ------ | ---------------- |
| `page`  | `number` | `1`    | 页码             |
| `limit` | `number` | `20`   | 每页条数         |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "vehicles": [
      {
        "id": 1,
        "vin": "LVGBE42KXNG123456",
        "brand": "Tesla",
        "model": "Model 3",
        "year": 2024,
        "color": "珍珠白",
        "plate_number": "京A12345",
        "status": "online",
        "battery_level": 85,
        "location": { "lat": 39.9042, "lng": 116.4074 },
        "created_at": "2025-01-15T08:00:00Z",
        "updated_at": "2025-05-25T12:00:00Z"
      }
    ],
    "total": 3,
    "page": 1,
    "limit": 20
  }
}
```

---

### GET /vehicles/:id

获取单个车辆详情。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "vin": "LVGBE42KXNG123456",
    "brand": "Tesla",
    "model": "Model 3",
    "year": 2024,
    "color": "珍珠白",
    "plate_number": "京A12345",
    "status": "online",
    "battery_level": 85,
    "location": { "lat": 39.9042, "lng": 116.4074 },
    "created_at": "2025-01-15T08:00:00Z",
    "updated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

### GET /vehicles/:id/status

获取车辆实时状态。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "vehicle_id": 1,
    "status": "online",
    "lock_state": "locked",
    "engine_state": "stopped",
    "battery_level": 85,
    "fuel_level": 60,
    "location": {
      "latitude": 39.9042,
      "longitude": 116.4074,
      "altitude": 50,
      "accuracy": 10,
      "timestamp": "2025-05-25T12:00:00Z"
    },
    "last_updated": "2025-05-25T12:00:00Z"
  }
}
```

| 字段           | 类型      | 说明                                       |
| -------------- | --------- | ------------------------------------------ |
| `status`       | `string`  | `online` / `offline` / `sleeping` / `unknown` |
| `lock_state`   | `string`  | `locked` / `unlocked` / `unknown`          |
| `engine_state` | `string`  | `running` / `stopped` / `unknown`          |
| `battery_level`| `number`  | 电量百分比 0-100                           |
| `fuel_level`   | `number`  | 油量百分比 0-100 (燃油车)                  |

---

### POST /vehicles/:id/commands

发送车辆控制命令。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**请求体:**

```json
{
  "command": "unlock",
  "params": {}
}
```

| 字段      | 类型     | 必填 | 说明                                                                 |
| --------- | -------- | ---- | -------------------------------------------------------------------- |
| `command` | `string` | 是   | 命令类型: `unlock`, `lock`, `engine_start`, `engine_stop`, `trunk_open`, `trunk_close`, `climate_on`, `climate_off`, `horn`, `lights`, `locate` |
| `params`  | `object` | 否   | 命令参数 (可选)                                                      |

**响应 (200):**

```json
{
  "code": 200,
  "message": "命令已发送",
  "data": {
    "command_id": "cmd_abc123",
    "status": "pending",
    "executed_at": "2025-05-25T12:00:05Z"
  }
}
```

---

### GET /vehicles/:id/commands/:command_id

获取车辆命令执行结果。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数         | 类型     | 说明         |
| ------------ | -------- | ------------ |
| `id`         | `number` | 车辆ID       |
| `command_id` | `string` | 命令ID       |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "command_id": "cmd_abc123",
    "vehicle_id": 1,
    "command": "unlock",
    "status": "completed",
    "result": {
      "success": true,
      "message": "车门已解锁"
    },
    "created_at": "2025-05-25T12:00:00Z",
    "completed_at": "2025-05-25T12:00:03Z"
  }
}
```

| 字段      | 类型     | 说明                                            |
| --------- | -------- | ----------------------------------------------- |
| `status`  | `string` | `pending` / `processing` / `completed` / `failed` |
| `result`  | `object` | 命令执行结果详情                                 |

---

### PUT /vehicles/:id/location

更新车辆位置信息。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**请求体:**

```json
{
  "latitude": 39.9042,
  "longitude": 116.4074,
  "altitude": 50,
  "accuracy": 10,
  "speed": 0,
  "heading": 180,
  "timestamp": "2025-05-25T12:00:00Z"
}
```

| 字段        | 类型     | 必填 | 说明                 |
| ----------- | -------- | ---- | -------------------- |
| `latitude`  | `number` | 是   | 纬度                 |
| `longitude` | `number` | 是   | 经度                 |
| `altitude`  | `number` | 否   | 海拔 (米)            |
| `accuracy`  | `number` | 否   | 定位精度 (米)        |
| `speed`     | `number` | 否   | 速度 (km/h)          |
| `heading`   | `number` | 否   | 朝向角度 0-360       |
| `timestamp` | `string` | 否   | 定位时间戳 (ISO 8601) |

**响应 (200):**

```json
{
  "code": 200,
  "message": "位置更新成功",
  "data": null
}
```

---

### POST /vehicles/:id/heartbeat

车辆心跳上报。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**请求体:**

```json
{
  "status": "online",
  "battery_level": 85,
  "lock_state": "locked",
  "engine_state": "stopped",
  "timestamp": "2025-05-25T12:00:00Z"
}
```

| 字段           | 类型     | 必填 | 说明                                              |
| -------------- | -------- | ---- | ------------------------------------------------- |
| `status`       | `string` | 是   | `online` / `offline` / `sleeping`                 |
| `battery_level`| `number` | 否   | 电量百分比 0-100                                  |
| `lock_state`   | `string` | 否   | `locked` / `unlocked`                             |
| `engine_state` | `string` | 否   | `running` / `stopped`                             |
| `timestamp`    | `string` | 否   | 心跳时间戳 (ISO 8601)                             |

**响应 (200):**

```json
{
  "code": 200,
  "message": "心跳接收成功",
  "data": null
}
```

---

## 数字钥匙 API

### POST /keys/issue

为用户签发新的数字钥匙。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "vehicle_id": 1,
  "key_type": "owner",
  "protocol": "CCC",
  "permissions": {
    "unlock": true,
    "lock": true,
    "start_engine": true,
    "trunk": true,
    "windows": false
  },
  "expires_in_days": 365
}
```

| 字段               | 类型      | 必填 | 说明                                       |
| ------------------ | --------- | ---- | ------------------------------------------ |
| `vehicle_id`       | `number`  | 是   | 车辆ID                                     |
| `key_type`         | `string`  | 是   | 钥匙类型: `owner` / `guest` / `temp`       |
| `protocol`         | `string`  | 是   | 协议类型: `CCC` / `ICCOA` / `ICCE`         |
| `permissions`      | `object`  | 否   | 权限配置                                   |
| `expires_in_days`  | `number`  | 否   | 有效期天数 (默认: 永久)                    |

**响应 (201):**

```json
{
  "code": 201,
  "message": "钥匙签发成功",
  "data": {
    "id": "key_abc123",
    "vehicle_id": 1,
    "key_type": "owner",
    "protocol": "CCC",
    "status": "active",
    "permissions": {
      "unlock": true,
      "lock": true,
      "start_engine": true,
      "trunk": true,
      "windows": false
    },
    "key_identifier": "key_abc123",
    "expires_at": "2026-05-25T12:00:00Z",
    "created_at": "2025-05-25T12:00:00Z"
  }
}
```

---

### GET /keys

获取我的数字钥匙列表。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数       | 类型     | 默认值 | 说明                                                |
| ---------- | -------- | ------ | --------------------------------------------------- |
| `status`   | `string` | -      | 过滤: `active`, `inactive`, `expired`, `revoked`     |
| `protocol` | `string` | -      | 过滤: `CCC`, `ICCOA`, `ICCE`                        |
| `page`     | `number` | `1`    | 页码                                                |
| `limit`    | `number` | `20`   | 每页条数                                            |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "list": [
      {
        "id": "1",
        "user_id": 1,
        "vehicle_id": 1,
        "type": "CCC",
        "status": "active",
        "permissions": {
          "unlock": true,
          "lock": true,
          "start_engine": true,
          "trunk": true,
          "windows": false
        },
        "key_identifier": "key_abc123",
        "expires_at": "2026-01-15T08:00:00Z",
        "usage_count": 42,
        "last_used_at": "2025-05-24T15:30:00Z",
        "created_at": "2025-01-15T08:00:00Z",
        "vehicle": {
          "id": 1,
          "vin": "LVGBE42KXNG123456",
          "brand": "Tesla",
          "model": "Model 3",
          "year": 2024,
          "color": "珍珠白",
          "plate_number": "京A12345"
        },
        "is_shared": false
      }
    ],
    "total": 5,
    "page": 1,
    "limit": 20
  }
}
```

---

### GET /keys/:id

获取单个钥匙详情。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": "1",
    "user_id": 1,
    "vehicle_id": 1,
    "type": "CCC",
    "status": "active",
    "permissions": {
      "unlock": true,
      "lock": true,
      "start_engine": true
    },
    "key_identifier": "key_abc123",
    "expires_at": "2026-01-15T08:00:00Z",
    "usage_count": 42,
    "last_used_at": "2025-05-24T15:30:00Z",
    "created_at": "2025-01-15T08:00:00Z",
    "vehicle": {
      "id": 1,
      "vin": "LVGBE42KXNG123456",
      "brand": "Tesla",
      "model": "Model 3",
      "year": 2024,
      "color": "珍珠白",
      "plate_number": "京A12345"
    },
    "is_shared": false
  }
}
```

---

### POST /keys/:id/activate

激活钥匙。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "钥匙已激活",
  "data": {
    "id": "1",
    "status": "active",
    "activated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

### POST /keys/:id/deactivate

停用钥匙。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "钥匙已停用",
  "data": {
    "id": "1",
    "status": "inactive",
    "deactivated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

### POST /keys/:id/share

分享钥匙给其他用户。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**请求体:**

```json
{
  "recipient_email": "friend@example.com",
  "recipient_phone": "13800138000",
  "expires_in_days": 7,
  "permissions": [
    { "type": "unlock", "enabled": true },
    { "type": "lock", "enabled": true },
    { "type": "start_engine", "enabled": false }
  ],
  "message": "临时停车用钥匙"
}
```

| 字段               | 类型       | 必填 | 说明                     |
| ------------------ | ---------- | ---- | ------------------------ |
| `recipient_email`  | `string`   | 否   | 接收者邮箱               |
| `recipient_phone`  | `string`   | 否   | 接收者手机号             |
| `expires_in_days`  | `number`   | 否   | 有效期天数               |
| `permissions`      | `array`    | 否   | 权限列表                 |
| `message`          | `string`   | 否   | 分享留言                 |

**响应 (200):**

```json
{
  "code": 200,
  "message": "分享成功",
  "data": {
    "share_id": "share_abc123",
    "qr_code_url": "https://api.yuledkcs.com/qr/share_abc123",
    "share_link": "https://yuledkcs.com/accept-share?token=***",
    "expires_at": "2025-06-01T12:00:00Z"
  }
}
```

---

### DELETE /keys/:id

删除/撤销钥匙。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "钥匙已撤销",
  "data": null
}
```

---

### PUT /keys/:id/permissions

更新钥匙权限。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**请求体:**

```json
{
  "permissions": [
    { "type": "unlock", "enabled": true },
    { "type": "lock", "enabled": true },
    { "type": "start_engine", "enabled": false },
    { "type": "trunk", "enabled": true }
  ]
}
```

| 字段          | 类型    | 必填 | 说明                   |
| ------------- | ------- | ---- | ---------------------- |
| `permissions` | `array` | 是   | 权限配置列表           |

**响应 (200):**

```json
{
  "code": 200,
  "message": "权限更新成功",
  "data": {
    "id": "1",
    "permissions": {
      "unlock": true,
      "lock": true,
      "start_engine": false,
      "trunk": true
    },
    "updated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

### GET /keys/:id/logs

获取钥匙使用记录。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**查询参数:**

| 参数         | 类型     | 默认值 | 说明         |
| ------------ | -------- | ------ | ------------ |
| `start_date` | `string` | -      | 开始日期     |
| `end_date`   | `string` | -      | 结束日期     |
| `page`       | `number` | `1`    | 页码         |
| `limit`      | `number` | `20`   | 每页条数     |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "list": [
      {
        "id": "1",
        "key_id": "1",
        "operation": "unlock",
        "status": "success",
        "timestamp": "2025-05-25T10:30:00Z",
        "location": { "lat": 39.9042, "lng": 116.4074 },
        "device_info": "{\"model\":\"iPhone 15\",\"os\":\"iOS 18\"}",
        "failure_reason": null
      }
    ],
    "total": 120,
    "page": 1,
    "limit": 20
  }
}
```

---

### GET /keys/shared/list

获取分享给我的钥匙列表。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数    | 类型     | 默认值 | 说明             |
| ------- | -------- | ------ | ---------------- |
| `page`  | `number` | `1`    | 页码             |
| `limit` | `number` | `20`   | 每页条数         |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "list": [
      {
        "id": "shared_key_1",
        "original_key_id": "1",
        "sharer": {
          "id": 1,
          "username": "admin"
        },
        "vehicle": {
          "id": 1,
          "brand": "Tesla",
          "model": "Model 3",
          "plate_number": "京A12345"
        },
        "permissions": {
          "unlock": true,
          "lock": true,
          "start_engine": false
        },
        "status": "active",
        "expires_at": "2025-06-01T12:00:00Z",
        "shared_at": "2025-05-25T12:00:00Z"
      }
    ],
    "total": 3,
    "page": 1,
    "limit": 20
  }
}
```

---

### GET /keys/:id/shares

获取钥匙的分享记录。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `string` | 钥匙ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "list": [
      {
        "share_id": "share_abc123",
        "recipient": {
          "id": 3,
          "username": "friend1",
          "email": "friend@example.com"
        },
        "status": "active",
        "permissions": {
          "unlock": true,
          "lock": true,
          "start_engine": false
        },
        "expires_at": "2025-06-01T12:00:00Z",
        "shared_at": "2025-05-25T12:00:00Z"
      }
    ],
    "total": 2
  }
}
```

---

### DELETE /keys/shares/:share_id

撤销钥匙分享。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数       | 类型     | 说明     |
| ---------- | -------- | -------- |
| `share_id` | `string` | 分享记录ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "分享已撤销",
  "data": null
}
```

---

## OTA / 固件管理 API

### 固件管理

#### POST /firmware

上传新固件。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "name": "ECU v2.1.0",
  "version": "2.1.0",
  "file_url": "https://storage.example.com/firmware/ecu-v2.1.0.bin",
  "md5": "a1b2c3d4e5f6...",
  "description": "性能优化和安全修复",
  "target_module": "ECU"
}
```

| 字段            | 类型     | 必填 | 说明                           |
| --------------- | -------- | ---- | ------------------------------ |
| `name`          | `string` | 是   | 固件名称                       |
| `version`       | `string` | 是   | 版本号                         |
| `file_url`      | `string` | 是   | 固件文件下载地址               |
| `md5`           | `string` | 是   | 文件 MD5 校验值                |
| `description`   | `string` | 否   | 描述信息                       |
| `target_module` | `string` | 否   | 目标模块 (ECU / BCM / TBOX 等) |

**响应 (201):**

```json
{
  "code": 201,
  "message": "固件上传成功",
  "data": {
    "id": 1,
    "name": "ECU v2.1.0",
    "version": "2.1.0",
    "size": 16777216,
    "md5": "a1b2c3d4e5f6...",
    "description": "性能优化和安全修复",
    "target_module": "ECU",
    "status": "active",
    "created_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### GET /firmware

获取固件列表。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数            | 类型     | 默认值 | 说明                           |
| --------------- | -------- | ------ | ------------------------------ |
| `target_module` | `string` | -      | 过滤: `ECU` / `BCM` / `TBOX`  |
| `status`        | `string` | -      | 过滤: `active` / `inactive`    |
| `page`          | `number` | `1`    | 页码                           |
| `limit`         | `number` | `20`   | 每页条数                       |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "list": [
      {
        "id": 1,
        "name": "ECU v2.1.0",
        "version": "2.1.0",
        "size": 16777216,
        "md5": "a1b2c3d4e5f6...",
        "description": "性能优化和安全修复",
        "target_module": "ECU",
        "status": "active",
        "created_at": "2025-01-15T08:00:00Z"
      }
    ],
    "total": 5,
    "page": 1,
    "limit": 20
  }
}
```

---

#### GET /firmware/:id

获取固件详情。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 固件ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "name": "ECU v2.1.0",
    "version": "2.1.0",
    "size": 16777216,
    "md5": "a1b2c3d4e5f6...",
    "description": "性能优化和安全修复",
    "target_module": "ECU",
    "status": "active",
    "file_url": "https://storage.example.com/firmware/ecu-v2.1.0.bin",
    "download_count": 128,
    "compatible_models": ["Model 3", "Model Y"],
    "created_at": "2025-01-15T08:00:00Z",
    "updated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### PUT /firmware/:id

更新固件信息。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 固件ID |

**请求体:**

```json
{
  "name": "ECU v2.1.1",
  "description": "修复蓝牙连接稳定性问题"
}
```

| 字段          | 类型     | 必填 | 说明         |
| ------------- | -------- | ---- | ------------ |
| `name`        | `string` | 否   | 固件名称     |
| `description` | `string` | 否   | 描述信息     |

**响应 (200):**

```json
{
  "code": 200,
  "message": "固件信息更新成功",
  "data": {
    "id": 1,
    "name": "ECU v2.1.1",
    "description": "修复蓝牙连接稳定性问题",
    "updated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### DELETE /firmware/:id

删除固件。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 固件ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "固件已删除",
  "data": null
}
```

---

#### PUT /firmware/:id/deactivate

停用固件（标记为不可用，不再推送）。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 固件ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "固件已停用",
  "data": {
    "id": 1,
    "status": "inactive",
    "deactivated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

### OTA 更新

#### POST /ota/check

全局检查是否有新固件更新（返回所有可用固件列表）。

**请求头:** 需要 `Authorization`

**响应 (200):**

```json
{
  "code": 200,
  "message": "检查完成",
  "data": {
    "available_firmwares": [
      {
        "id": 1,
        "name": "ECU v2.1.0",
        "version": "2.1.0",
        "target_module": "ECU",
        "size": 16777216,
        "description": "性能优化和安全修复",
        "release_notes": "修复了已知问题，提升了系统稳定性",
        "created_at": "2025-05-25T12:00:00Z"
      }
    ],
    "total": 1
  }
}
```

---

#### GET /ota/updates

获取 OTA 更新记录列表。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数         | 类型     | 默认值 | 说明                                   |
| ------------ | -------- | ------ | -------------------------------------- |
| `status`     | `string` | -      | 过滤: `pending` / `downloading` / `installing` / `completed` / `failed` |
| `vehicle_id` | `number` | -      | 过滤: 车辆ID                           |
| `page`       | `number` | `1`    | 页码                                   |
| `limit`      | `number` | `20`   | 每页条数                               |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "list": [
      {
        "id": 1,
        "vehicle_id": 1,
        "firmware_id": 1,
        "firmware_name": "ECU v2.1.0",
        "status": "completed",
        "progress": 100,
        "started_at": "2025-05-20T10:00:00Z",
        "completed_at": "2025-05-20T10:15:30Z"
      }
    ],
    "total": 10,
    "page": 1,
    "limit": 20
  }
}
```

---

#### POST /ota/firmwares/:id/download

触发固件下载任务。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 固件ID |

**请求体:**

```json
{
  "vehicle_id": 1
}
```

| 字段         | 类型     | 必填 | 说明   |
| ------------ | -------- | ---- | ------ |
| `vehicle_id` | `number` | 是   | 车辆ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "下载任务已创建",
  "data": {
    "task_id": "ota_task_1",
    "firmware_id": 1,
    "vehicle_id": 1,
    "status": "downloading",
    "progress": 0,
    "created_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### GET /vehicles/:id/ota/status

获取车辆 OTA 状态。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "vehicle_id": 1,
    "current_firmware_version": "2.0.0",
    "latest_firmware_version": "2.1.0",
    "has_pending_update": true,
    "update_status": "pending_confirm",
    "progress": 0,
    "pending_firmware": {
      "id": 1,
      "name": "ECU v2.1.0",
      "version": "2.1.0",
      "size": 16777216,
      "description": "性能优化和安全修复"
    },
    "last_check_at": "2025-05-25T12:00:00Z",
    "last_update_at": "2025-01-15T08:00:00Z"
  }
}
```

| 字段                       | 类型      | 说明                                                    |
| -------------------------- | --------- | ------------------------------------------------------- |
| `update_status`            | `string`  | `idle` / `pending_confirm` / `downloading` / `installing` / `completed` / `failed` |
| `progress`                 | `number`  | 下载/安装进度 0-100                                     |
| `has_pending_update`       | `boolean` | 是否有待处理的更新                                      |

---

#### PUT /vehicles/:id/ota/status

手动更新车辆 OTA 状态。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**请求体:**

```json
{
  "update_status": "downloading",
  "progress": 45
}
```

| 字段            | 类型     | 必填 | 说明                                                    |
| --------------- | -------- | ---- | ------------------------------------------------------- |
| `update_status` | `string` | 是   | `idle` / `downloading` / `installing` / `completed` / `failed` |
| `progress`      | `number` | 否   | 进度百分比 0-100                                        |

**响应 (200):**

```json
{
  "code": 200,
  "message": "OTA 状态已更新",
  "data": {
    "vehicle_id": 1,
    "update_status": "downloading",
    "progress": 45,
    "updated_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### POST /vehicles/:id/ota/start

启动 OTA 更新。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**请求体:**

```json
{
  "firmware_id": 1
}
```

| 字段          | 类型     | 必填 | 说明   |
| ------------- | -------- | ---- | ------ |
| `firmware_id` | `number` | 是   | 固件ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "OTA 更新已启动",
  "data": {
    "vehicle_id": 1,
    "firmware_id": 1,
    "status": "downloading",
    "started_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### POST /vehicles/:id/ota/check

检查车辆是否有新固件更新。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "检查完成",
  "data": {
    "vehicle_id": 1,
    "has_update": true,
    "current_version": "2.0.0",
    "latest_version": "2.1.0",
    "firmware": {
      "id": 1,
      "name": "ECU v2.1.0",
      "version": "2.1.0",
      "size": 16777216,
      "description": "性能优化和安全修复",
      "release_notes": "修复了已知问题，提升了系统稳定性"
    },
    "checked_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### POST /vehicles/:id/ota/confirm

确认 OTA 更新（将待确认状态转为下载中）。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**请求体:**

```json
{
  "firmware_id": 1
}
```

| 字段          | 类型     | 必填 | 说明   |
| ------------- | -------- | ---- | ------ |
| `firmware_id` | `number` | 是   | 固件ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "OTA 更新已确认",
  "data": {
    "vehicle_id": 1,
    "firmware_id": 1,
    "status": "downloading",
    "confirmed_at": "2025-05-25T12:00:00Z"
  }
}
```

---

#### GET /vehicles/:id/ota/history

获取车辆的 OTA 更新历史记录。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**查询参数:**

| 参数    | 类型     | 默认值 | 说明             |
| ------- | -------- | ------ | ---------------- |
| `page`  | `number` | `1`    | 页码             |
| `limit` | `number` | `20`   | 每页条数         |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "list": [
      {
        "id": 1,
        "firmware_name": "ECU v2.1.0",
        "firmware_version": "2.1.0",
        "previous_version": "2.0.0",
        "status": "completed",
        "progress": 100,
        "started_at": "2025-05-20T10:00:00Z",
        "completed_at": "2025-05-20T10:15:30Z",
        "duration_seconds": 930
      },
      {
        "id": 2,
        "firmware_name": "BCM v1.3.0",
        "firmware_version": "1.3.0",
        "previous_version": "1.2.0",
        "status": "completed",
        "progress": 100,
        "started_at": "2025-04-10T08:00:00Z",
        "completed_at": "2025-04-10T08:12:00Z",
        "duration_seconds": 720
      }
    ],
    "total": 5,
    "page": 1,
    "limit": 20
  }
}
```

---

## 用户 API

### GET /user/profile

获取当前用户信息。

**请求头:** 需要 `Authorization`

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": "1",
    "username": "admin",
    "email": "admin@example.com",
    "phone": "13800138000",
    "role": "admin",
    "created_at": "2025-01-01T00:00:00Z"
  }
}
```

---

## 系统 API

> 系统级 API 通常**不需要**认证，用于健康检查和运维监控。

### GET /ping

心跳探测。

**请求头:** 不需认证

**响应 (200):**

```json
{
  "code": 200,
  "message": "pong",
  "data": null
}
```

---

### GET /health

综合健康检查。

**请求头:** 不需认证

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "status": "healthy",
    "version": "1.0.0",
    "uptime_seconds": 86400,
    "timestamp": "2025-05-25T12:00:00Z",
    "components": {
      "database": { "status": "healthy", "latency_ms": 5 },
      "redis": { "status": "healthy", "latency_ms": 2 },
      "mqtt": { "status": "healthy", "connected_brokers": 3 }
    }
  }
}
```

| 字段        | 类型      | 说明                                    |
| ----------- | --------- | --------------------------------------- |
| `status`    | `string`  | `healthy` / `degraded` / `unhealthy`    |
| `version`   | `string`  | 服务版本号                              |
| `components`| `object`  | 各组件健康状态                          |

---

### GET /health/live

存活探针 (Liveness Probe)，用于 Kubernetes 等容器编排平台。

**请求头:** 不需认证

**响应 (200):**

```json
{
  "code": 200,
  "message": "alive",
  "data": null
}
```

---

### GET /health/ready

就绪探针 (Readiness Probe)，检查服务是否可接收流量。

**请求头:** 不需认证

**响应 (200):**

```json
{
  "code": 200,
  "message": "ready",
  "data": {
    "database": "connected",
    "redis": "connected",
    "mqtt": "connected"
  }
}
```

**响应 (503) — 服务未就绪:**

```json
{
  "code": 503,
  "message": "service not ready",
  "data": {
    "database": "disconnected",
    "redis": "connected",
    "mqtt": "connected"
  }
}
```

---

### GET /metrics

Prometheus 指标暴露端点。

**请求头:** 不需认证

**响应 (200):**

```
# HELP http_requests_total Total number of HTTP requests
# TYPE http_requests_total counter
http_requests_total{method="GET",path="/api/v1/vehicles",status="200"} 1024
# HELP http_request_duration_seconds HTTP request duration in seconds
# TYPE http_request_duration_seconds histogram
http_request_duration_seconds_bucket{le="0.1"} 500
http_request_duration_seconds_bucket{le="0.5"} 800
http_request_duration_seconds_bucket{le="+Inf"} 1024
...
```

> 返回 Prometheus 文本格式的指标数据。格式: `Content-Type: text/plain; version=0.0.4`

---

### WebSocket: GET /ws

WebSocket 连接端点，用于实时推送车辆状态、钥匙事件、OTA 进度等。

**请求头:** 需要 `Authorization` (通过 URL 查询参数传递 Token)

```
Authorization: Bearer ***
Upgrade: websocket
Connection: Upgrade
```

**URL 参数:**

| 参数     | 类型     | 必填 | 说明                     |
| -------- | -------- | ---- | ------------------------ |
| `token`  | `string` | 是   | JWT Token (备用通过URL)  |

**支持的事件类型:**

| 事件类型             | 说明                       |
| -------------------- | -------------------------- |
| `vehicle.status`     | 车辆状态变更               |
| `vehicle.location`   | 车辆位置更新               |
| `key.operation`      | 钥匙操作通知               |
| `key.share`          | 钥匙分享通知               |
| `ota.progress`       | OTA 更新进度推送           |
| `ota.status`         | OTA 状态变更               |
| `alert`              | 系统告警通知               |

**示例消息格式 (服务端推送):**

```json
{
  "type": "vehicle.status",
  "data": {
    "vehicle_id": 1,
    "status": "online",
    "lock_state": "unlocked",
    "timestamp": "2025-05-25T12:00:00Z"
  }
}
```

---

## 附录: 状态码速查

| 状态码 | 含义           | 说明                         |
| ------ | -------------- | ---------------------------- |
| 200    | OK             | 请求成功                     |
| 201    | Created        | 创建成功                     |
| 400    | Bad Request    | 请求参数错误                 |
| 401    | Unauthorized   | 未认证 / Token 过期          |
| 403    | Forbidden      | 无权限                       |
| 404    | Not Found      | 资源不存在                   |
| 409    | Conflict       | 资源冲突 (如重复注册)        |
| 422    | Unprocessable  | 请求语义错误                 |
| 429    | Too Many Req   | 请求频率过高                 |
| 500    | Server Error   | 服务器内部错误               |
| 503    | Service Unavailable | 服务暂不可用             |

---

## 附录: 版本历史

| 日期         | 版本  | 说明                          |
| ------------ | ----- | ----------------------------- |
| 2025-05-26   | v2.0  | 完整覆盖所有后端 API 路由文档 |
| 2025-05-25   | v1.0  | 初始版本，覆盖认证/钥匙/车辆  |
