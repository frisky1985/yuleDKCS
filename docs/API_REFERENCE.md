# yuleDKCS API 接口参考

## 基础信息

- 基础URL: `https://api.yuledkcs.com/v1`
- 认证方式: Header `X-API-Key`
- 内容类型: `application/json`

## 端点列表

### 钥匙管理

#### 获取钥匙列表
```http
GET /keys
Authorization: Bearer {token}
```

**响应:**
```json
{
  "keys": [
    {
      "id": "key_123",
      "vehicle": {
        "id": "veh_456",
        "vin": "LVSHCADMXXX",
        "brand": "奔驰",
        "model": "E300L",
        "year": 2024,
        "color": "黑色"
      },
      "protocol": "CCC",
      "type": "owner",
      "status": "active",
      "permissions": [
        {"type": "unlock", "enabled": true},
        {"type": "lock", "enabled": true},
        {"type": "start_engine", "enabled": true}
      ],
      "issued_at": "2026-05-01T10:00:00Z",
      "expires_at": "2027-05-01T10:00:00Z",
      "use_count": 42,
      "last_used_at": "2026-05-10T15:30:00Z"
    }
  ],
  "total": 1,
  "page": 1,
  "limit": 20
}
```

#### 获取钥匙详情
```http
GET /keys/{keyId}
Authorization: Bearer {token}
```

#### 激活钥匙
```http
POST /keys/{keyId}/activate
Authorization: Bearer {token}
```

#### 停用钥匙
```http
POST /keys/{keyId}/deactivate
Authorization: Bearer {token}
```

#### 撤销钥匙
```http
DELETE /keys/{keyId}
Authorization: Bearer {token}
```

### 钥匙分享

#### 分享钥匙
```http
POST /keys/share
Authorization: Bearer {token}
Content-Type: application/json

{
  "key_id": "key_123",
  "recipient_email": "friend@example.com",
  "recipient_phone": "+86138xxxx",
  "expires_in_days": 7,
  "permissions": [
    {"type": "unlock", "enabled": true},
    {"type": "lock", "enabled": true}
  ],
  "message": "这是我的车钥匙，请查收"
}
```

**响应:**
```json
{
  "share_id": "share_789",
  "qr_code_url": "https://api.yuledkcs.com/qrcode/share_789.png",
  "share_link": "https://yuledkcs.com/receive?token=abc123",
  "expires_at": "2026-05-18T10:00:00Z"
}
```

#### 接收分享的钥匙
```http
POST /keys/receive
Authorization: Bearer {token}
Content-Type: application/json

{
  "token": "abc123"
}
```

#### 获取分享记录
```http
GET /keys/{keyId}/shares
Authorization: Bearer {token}
```

#### 撤销分享
```http
DELETE /keys/shares/{shareId}
Authorization: Bearer {token}
```

### 权限管理

#### 更新权限
```http
PUT /keys/{keyId}/permissions
Authorization: Bearer {token}
Content-Type: application/json

{
  "permissions": [
    {
      "type": "unlock",
      "enabled": true,
      "constraints": {
        "time_range": {"start": "06:00", "end": "22:00"},
        "days_of_week": [1, 2, 3, 4, 5, 6, 7],
        "max_uses": 10
      }
    }
  ]
}
```

### 使用记录

#### 获取钥匙使用记录
```http
GET /keys/{keyId}/logs?start_date=2026-05-01&end_date=2026-05-31&page=1&limit=50
Authorization: Bearer {token}
```

**响应:**
```json
{
  "logs": [
    {
      "id": "log_001",
      "operation": "unlock",
      "status": "success",
      "timestamp": "2026-05-10T15:30:00Z",
      "location": {
        "lat": 39.9042,
        "lng": 116.4074
      },
      "device_info": "iPhone 15 Pro",
      "failure_reason": null
    }
  ],
  "total": 150,
  "page": 1,
  "limit": 50
}
```

#### 获取所有使用记录
```http
GET /keys/logs/all?start_date=2026-05-01&operation=unlock
Authorization: Bearer {token}
```

#### 记录使用
```http
POST /keys/{keyId}/logs
Authorization: Bearer {token}
Content-Type: application/json

{
  "operation": "unlock",
  "status": "success",
  "timestamp": "2026-05-10T15:30:00Z",
  "location": {
    "lat": 39.9042,
    "lng": 116.4074
  }
}
```

### 二维码

#### 生成二维码
```http
POST /keys/{keyId}/qrcode
Authorization: Bearer {token}
Content-Type: application/json

{
  "type": "share"
}
```

#### 扫码激活
```http
POST /keys/scan
Authorization: Bearer {token}
Content-Type: application/json

{
  "qr_data": "encrypted_qr_data_here"
}
```

### 批量操作

#### 批量撤销钥匙
```http
POST /keys/batch/revoke
Authorization: Bearer {token}
Content-Type: application/json

{
  "key_ids": ["key_1", "key_2", "key_3"]
}
```

## 数据模型

### DigitalKey
| 字段 | 类型 | 说明 |
|-----|------|------|
| id | string | 钥匙唯一标识 |
| vehicle | Vehicle | 车辆信息 |
| protocol | string | 协议类型 (CCC/ICCOA/ICCE) |
| type | string | 钥匙类型 (owner/shared) |
| status | string | 状态 (active/inactive/expired/revoked) |
| permissions | KeyPermission[] | 权限列表 |
| issued_at | datetime | 发行时间 |
| expires_at | datetime | 过期时间 |
| use_count | integer | 使用次数 |
| last_used_at | datetime | 最后使用时间 |

### KeyPermission
| 字段 | 类型 | 说明 |
|-----|------|------|
| type | string | 权限类型 |
| enabled | boolean | 是否启用 |
| constraints | PermissionConstraints | 限制条件 |

### PermissionConstraints
| 字段 | 类型 | 说明 |
|-----|------|------|
| time_range | object | 时间段限制 {start, end} |
| days_of_week | integer[] | 允许使用的星期 [1-7] |
| max_uses | integer | 最大使用次数 |
| geofence | GeoFence | 地理围栏 |

### KeyUsageLog
| 字段 | 类型 | 说明 |
|-----|------|------|
| id | string | 记录ID |
| operation | string | 操作类型 |
| status | string | 状态 (success/failure/timeout) |
| timestamp | datetime | 操作时间 |
| location | Location | 位置信息 |
| device_info | string | 设备信息 |
| failure_reason | string | 失败原因 |

## 错误码

| HTTP状态码 | 错误码 | 说明 |
|----------|--------|------|
| 400 | BAD_REQUEST | 请求参数错误 |
| 401 | UNAUTHORIZED | API Key 无效 |
| 403 | FORBIDDEN | 权限不足 |
| 404 | NOT_FOUND | 资源不存在 |
| 409 | CONFLICT | 状态冲突 |
| 410 | GONE | 钥匙已过期/被撤销 |
| 429 | RATE_LIMITED | 请求过于频繁 |
| 500 | INTERNAL_ERROR | 服务器内部错误 |

## 错误响应格式

```json
{
  "error": {
    "code": "PERMISSION_DENIED",
    "message": "钥匙没有此操作的权限",
    "details": {
      "key_id": "key_123",
      "required_permission": "start_engine",
      "current_permissions": ["unlock", "lock"]
    }
  }
}
```
