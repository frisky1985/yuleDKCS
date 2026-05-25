# API 接口文档

> 前端与后端的所有 RESTful API 接口文档。基础 URL: `/api/v1`

---

## 目录

- [通用说明](#通用说明)
- [认证 API](#认证-api)
- [仪表盘 API](#仪表盘-api)
- [车辆 API](#车辆-api)
- [数字钥匙 API](#数字钥匙-api)
- [用户 API](#用户-api)

---

## 通用说明

### 请求头

```http
Content-Type: application/json
Authorization: Bearer <jwt_token>   # 需要认证的接口
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
    "token": "eyJhbGciOiJIUzI1NiIs...",
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

### POST /auth/logout

用户登出（可选，用于后端记录会话失效）。

**请求头:** 需要 `Authorization`

**响应 (200):**

```json
{
  "code": 200,
  "message": "登出成功",
  "data": null
}
```

### POST /auth/refresh

刷新 Token。

**请求头:** 需要 `Authorization`

**响应 (200):**

```json
{
  "code": 200,
  "message": "Token 刷新成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
    "type": "Bearer"
  }
}
```

---

## 仪表盘 API

### GET /dashboard/stats

获取仪表盘统计数据。

**请求头:** 需要 `Authorization`

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "total_vehicles": 3,
    "active_keys": 5,
    "today_operations": 12,
    "alerts": 1
  }
}
```

| 字段               | 类型     | 说明               |
| ------------------ | -------- | ------------------ |
| `total_vehicles`   | `number` | 车辆总数           |
| `active_keys`      | `number` | 有效钥匙数         |
| `today_operations` | `number` | 今日操作次数       |
| `alerts`           | `number` | 未处理告警数       |

### GET /activities/recent

获取最近活动记录。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数    | 类型     | 默认值 | 说明     |
| ------- | -------- | ------ | -------- |
| `limit` | `number` | `20`   | 返回条数 |

**响应 (200):**

```json
{
  "code": 200,
  "message": "success",
  "data": [
    {
      "id": 1,
      "type": "unlock",
      "message": "已解锁车门",
      "timestamp": "2025-05-25T10:30:00Z",
      "vehicle": "Tesla Model 3"
    }
  ]
}
```

---

## 车辆 API

### GET /vehicles

获取当前用户的所有车辆列表。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数   | 类型     | 默认值 | 说明             |
| ------ | -------- | ------ | ---------------- |
| `page` | `number` | `1`    | 页码             |
| `limit`| `number` | `20`   | 每页条数         |

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

### PUT /vehicles/:id

更新车辆信息。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**请求体:**

```json
{
  "color": "烈焰红",
  "plate_number": "京C13579"
}
```

**响应 (200):**

```json
{
  "code": 200,
  "message": "车辆信息更新成功",
  "data": {
    "id": 1,
    "vin": "LVGBE42KXNG123456",
    "brand": "Tesla",
    "model": "Model 3",
    "year": 2024,
    "color": "烈焰红",
    "plate_number": "京C13579"
  }
}
```

### DELETE /vehicles/:id

删除车辆。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数 | 类型     | 说明   |
| ---- | -------- | ------ |
| `id` | `number` | 车辆ID |

**响应 (200):**

```json
{
  "code": 200,
  "message": "车辆删除成功",
  "data": null
}
```

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

### POST /vehicles/:id/control

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

### GET /vehicles/:id/keys

获取车辆关联的数字钥匙列表。

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
  "data": [
    {
      "id": "1",
      "type": "CCC",
      "key_type": "owner",
      "status": "active",
      "permissions": { "unlock": true, "lock": true, "start_engine": true },
      "created_at": "2025-01-15T08:00:00Z"
    }
  ]
}
```

---

## 数字钥匙 API

### GET /keys

获取我的数字钥匙列表。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数       | 类型     | 默认值 | 说明                                |
| ---------- | -------- | ------ | ----------------------------------- |
| `status`   | `string` | -      | 过滤: `active`, `inactive`, `expired`, `revoked` |
| `protocol` | `string` | -      | 过滤: `CCC`, `ICCOA`, `ICCE`        |
| `page`     | `number` | `1`    | 页码                                |
| `limit`    | `number` | `20`   | 每页条数                            |

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

### GET /keys/shared

获取分享给我的钥匙列表。

**请求头:** 需要 `Authorization`

**查询参数:** 同 `GET /keys`

**响应:** 同 `GET /keys`

### GET /keys/:keyId

获取单个钥匙详情。

**请求头:** 需要 `Authorization`

**路径参数:**

| 参数    | 类型     | 说明   |
| ------- | -------- | ------ |
| `keyId` | `string` | 钥匙ID |

**响应:** 同钥匙对象

### POST /keys/:keyId/activate

激活钥匙。

**请求头:** 需要 `Authorization`

**响应 (200):**

```json
{
  "code": 200,
  "message": "钥匙已激活",
  "data": { ... }
}
```

### POST /keys/:keyId/deactivate

停用钥匙。

**请求头:** 需要 `Authorization`

**响应:** 同激活

### DELETE /keys/:keyId

撤销钥匙。

**请求头:** 需要 `Authorization`

**响应 (200):**

```json
{
  "code": 200,
  "message": "钥匙已撤销",
  "data": null
}
```

### POST /keys/batch/revoke

批量撤销钥匙。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "key_ids": ["1", "2", "3"]
}
```

### POST /keys/share

分享钥匙。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "key_id": "1",
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

**响应 (200):**

```json
{
  "code": 200,
  "message": "分享成功",
  "data": {
    "share_id": "share_abc123",
    "qr_code_url": "https://api.yuledkcs.com/qr/share_abc123",
    "share_link": "https://yuledkcs.com/accept-share?token=xxx",
    "expires_at": "2025-06-01T12:00:00Z"
  }
}
```

### GET /keys/:keyId/shares

获取钥匙的分享记录。

**请求头:** 需要 `Authorization`

### DELETE /keys/shares/:shareId

撤销分享。

**请求头:** 需要 `Authorization`

### PUT /keys/:keyId/permissions

更新钥匙权限。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "permissions": [
    { "type": "unlock", "enabled": true },
    { "type": "lock", "enabled": true }
  ]
}
```

### GET /keys/:keyId/logs

获取钥匙使用记录。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数         | 类型     | 默认值 | 说明         |
| ------------ | -------- | ------ | ------------ |
| `start_date` | `string` | -      | 开始日期     |
| `end_date`   | `string` | -      | 结束日期     |
| `page`       | `number` | `1`    | 页码         |
| `limit`      | `number` | `20`   | 每页条数     |

### GET /keys/logs/all

获取所有钥匙的使用记录。

**请求头:** 需要 `Authorization`

**查询参数:**

| 参数         | 类型     | 默认值 | 说明                                |
| ------------ | -------- | ------ | ----------------------------------- |
| `start_date` | `string` | -      | 开始日期                            |
| `end_date`   | `string` | -      | 结束日期                            |
| `operation`  | `string` | -      | 操作类型: `unlock`, `lock` 等       |
| `page`       | `number` | `1`    | 页码                                |
| `limit`      | `number` | `20`   | 每页条数                            |

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

### POST /keys/:keyId/qrcode

生成钥匙二维码。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "type": "share"
}
```

| 字段   | 类型     | 必填 | 说明                              |
| ------ | -------- | ---- | --------------------------------- |
| `type` | `string` | 是   | `share` / `activate` / `temp`     |

### POST /keys/scan

扫码激活钥匙。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "qr_data": "yuledkcs://key/activate?token=xxx"
}
```

### POST /keys/:keyId/extend

延长钥匙有效期。

**请求头:** 需要 `Authorization`

**请求体:**

```json
{
  "days": 30
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

---

## 附录: 版本历史

| 日期         | 版本  | 说明                          |
| ------------ | ----- | ----------------------------- |
| 2025-05-25   | v1.0  | 初始版本，覆盖认证/钥匙/车辆  |
