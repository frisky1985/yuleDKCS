# yuleDKCS API 接口参考

## 基础信息

- **基础URL**: `https://api.yuledkcs.com/api/v1`
- **开发环境**: `http://localhost:8080/api/v1`
- **认证方式**: `Authorization: Bearer ***`
- **内容类型**: `application/json`
- **版本**: 1.0.0

## API 端点一览

| 类别 | 方法 | 端点 | 认证 | 说明 |
|------|------|------|------|------|
| 健康检查 | GET | `/health` | 否 | 健康检查 |
| 健康检查 | GET | `/health/live` | 否 | 存活检查 |
| 健康检查 | GET | `/health/ready` | 否 | 就绪检查 |
| 指标 | GET | `/metrics` | 否 | Prometheus 指标 |
| 系统 | GET | `/api/v1/ping` | 否 | Ping/连通性测试 |
| 认证 | POST | `/api/v1/auth/login` | 否 | 用户登录 |
| 认证 | POST | `/api/v1/auth/register` | 否 | 用户注册 |
| 认证 | POST | `/api/v1/auth/refresh` | 否 | 刷新 Token |
| 用户 | GET | `/api/v1/user/profile` | 是 | 获取用户信息 |
| 钥匙 | POST | `/api/v1/keys/issue` | 是 | 发行钥匙 |
| 钥匙 | GET | `/api/v1/keys` | 是 | 获取钥匙列表 |
| 钥匙 | GET | `/api/v1/keys/:id` | 是 | 获取钥匙详情 |
| 钥匙 | POST | `/api/v1/keys/:id/activate` | 是 | 激活钥匙 |
| 钥匙 | POST | `/api/v1/keys/:id/deactivate` | 是 | 停用钥匙 |
| 钥匙 | POST | `/api/v1/keys/:id/share` | 是 | 分享钥匙 |
| 钥匙 | DELETE | `/api/v1/keys/:id` | 是 | 撤销钥匙 |
| 钥匙 | PUT | `/api/v1/keys/:id/permissions` | 是 | 更新钥匙权限 |
| 钥匙 | GET | `/api/v1/keys/:id/logs` | 是 | 获取钥匙使用日志 |
| 钥匙 | GET | `/api/v1/keys/shared/list` | 是 | 获取收到的分享钥匙 |
| 钥匙 | GET | `/api/v1/keys/:id/shares` | 是 | 获取钥匙的分享列表 |
| 钥匙 | DELETE | `/api/v1/keys/shares/:share_id` | 是 | 撤销分享 |
| 车辆 | POST | `/api/v1/vehicles` | 是 | 注册车辆 |
| 车辆 | GET | `/api/v1/vehicles` | 是 | 获取用户车辆列表 |
| 车辆 | GET | `/api/v1/vehicles/:id` | 是 | 获取车辆详情 |
| 车辆 | GET | `/api/v1/vehicles/:id/status` | 是 | 获取车辆状态 |
| 车辆 | POST | `/api/v1/vehicles/:id/commands` | 是 | 发送车辆命令 |
| 车辆 | GET | `/api/v1/vehicles/:id/commands/:command_id` | 是 | 查询命令状态 |
| 车辆 | PUT | `/api/v1/vehicles/:id/location` | 是 | 更新车辆位置 |
| 车辆 | POST | `/api/v1/vehicles/:id/heartbeat` | 是 | 车辆心跳 |
| OTA | POST | `/api/v1/ota/firmwares` | 是 | 创建固件 |
| OTA | GET | `/api/v1/ota/firmwares` | 是 | 获取固件列表 |
| OTA | GET | `/api/v1/ota/firmwares/:id` | 是 | 获取固件详情 |
| OTA | PUT | `/api/v1/ota/firmwares/:id` | 是 | 更新固件 |
| OTA | DELETE | `/api/v1/ota/firmwares/:id` | 是 | 删除固件 |
| OTA | PUT | `/api/v1/ota/firmwares/:id/deactivate` | 是 | 停用固件 |
| OTA | POST | `/api/v1/ota/check` | 是 | 检查更新 (POST) |
| OTA | GET | `/api/v1/ota/updates` | 是 | 检查更新 (GET) |
| OTA | POST | `/api/v1/ota/firmwares/:id/download` | 是 | 下载固件 |
| OTA | GET | `/api/v1/vehicles/:id/ota/status` | 是 | 获取车辆 OTA 状态 |
| OTA | PUT | `/api/v1/vehicles/:id/ota/status` | 是 | 更新车辆 OTA 状态 |
| OTA | POST | `/api/v1/vehicles/:id/ota/start` | 是 | 启动 OTA 更新 |
| OTA | GET | `/api/v1/vehicles/:id/ota/history` | 是 | 获取 OTA 历史 |
| WebSocket | GET | `/ws` | 是 | WebSocket 实时通信 |
| WebSocket | GET | `/ws/user` | 是 | 用户 WebSocket |
| WebSocket | GET | `/ws/vehicle/:id` | 是 | 车辆 WebSocket |
| WebSocket | GET | `/ws/admin` | 是 | 管理 WebSocket |

---

## 1. 健康检查

### 1.1 存活检查
```http
GET /health/live
```

**响应:**
```json
{
  "status": "alive",
  "timestamp": "2026-05-26T10:00:00Z"
}
```

### 1.2 就绪检查
```http
GET /health/ready
```

**响应:**
```json
{
  "status": "ready",
  "checks": {
    "database": "ok",
    "redis": "ok",
    "config": "ok"
  },
  "timestamp": "2026-05-26T10:00:00Z"
}
```

### 1.3 Ping
```http
GET /api/v1/ping
```

**响应:**
```json
{
  "message": "pong",
  "service": "yuleDKCS",
  "version": "1.0.0"
}
```

---

## 2. 用户认证 API

### 2.1 用户注册

```http
POST /api/v1/auth/register
Content-Type: application/json

{
  "username": "zhangsan",
  "email": "zhangsan@example.com",
  "password": "SecurePass123",
  "phone": "+8613800138000"
}
```

**响应 (201):**
```json
{
  "code": 201,
  "message": "注册成功",
  "data": {
    "id": 1,
    "username": "zhangsan",
    "email": "zhangsan@example.com"
  }
}
```

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名 (3-50字符) |
| email | string | 是 | 邮箱 |
| password | string | 是 | 密码 (最少6位) |
| phone | string | 否 | 手机号 |

### 2.2 用户登录

```http
POST /api/v1/auth/login
Content-Type: application/json

{
  "username": "zhangsan",
  "password": "SecurePass123"
}
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "登录成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
    "type": "Bearer",
    "user": {
      "id": 1,
      "username": "zhangsan",
      "email": "zhangsan@example.com",
      "role": "user"
    }
  }
}
```

### 2.3 刷新 Token

```http
POST /api/v1/auth/refresh
Authorization: Bearer <expired_or_valid_token>
```

**响应 (200):**
```json
{
  "token": "eyJhbGciOiJIUzI1NiIs...",
  "type": "Bearer"
}
```

支持在 Token 过期后 60 天内刷新。

### 2.4 获取用户信息

```http
GET /api/v1/user/profile
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "data": {
    "id": 1,
    "username": "zhangsan",
    "email": "zhangsan@example.com",
    "phone": "+8613800138000",
    "role": "user",
    "created_at": "2026-01-15T08:30:00Z"
  }
}
```

---

## 3. 钥匙注册/查询/更新/删除 API

### 3.1 发行钥匙

```http
POST /api/v1/keys/issue
Authorization: Bearer <token>
Content-Type: application/json

{
  "vehicle_id": 1,
  "type": "CCC",
  "permissions": {
    "unlock": true,
    "lock": true,
    "start_engine": true,
    "trunk": true,
    "windows": false
  },
  "name": "我的主钥匙",
  "description": "日常使用",
  "expires_at": "2027-05-26T10:00:00Z"
}
```

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| vehicle_id | uint | 是 | 车辆 ID |
| type | string | 是 | 协议类型: `CCC`, `ICCE`, `ICCOA` |
| permissions | object | 否 | 权限（默认: unlock/lock/start_engine=true） |
| name | string | 否 | 钥匙名称 |
| description | string | 否 | 钥匙描述 |
| expires_at | datetime | 否 | 过期时间 |

**响应 (201):**
```json
{
  "code": 201,
  "message": "钥匙发行成功",
  "data": {
    "id": 1,
    "user_id": 1,
    "vehicle_id": 1,
    "vehicle_name": "我的奔驰",
    "type": "CCC",
    "status": "active",
    "permissions": {
      "unlock": true,
      "lock": true,
      "start_engine": true,
      "trunk": true,
      "windows": false
    },
    "key_identifier": "abc123...",
    "name": "我的主钥匙",
    "description": "日常使用",
    "expires_at": "2027-05-26T10:00:00Z",
    "usage_count": 0,
    "is_shared": false,
    "created_at": "2026-05-26T10:00:00Z",
    "updated_at": "2026-05-26T10:00:00Z"
  }
}
```

### 3.2 获取钥匙列表

```http
GET /api/v1/keys?page=1&page_size=20
Authorization: Bearer <token>
```

| 参数 | 类型 | 默认 | 说明 |
|------|------|------|------|
| page | int | 1 | 页码 |
| page_size | int | 10 | 每页数量 |

**响应 (200):**
```json
{
  "code": 200,
  "message": "获取成功",
  "data": {
    "list": [
      {
        "id": 1,
        "user_id": 1,
        "vehicle_id": 1,
        "vehicle_name": "我的奔驰",
        "type": "CCC",
        "status": "active",
        "permissions": {
          "unlock": true,
          "lock": true,
          "start_engine": true,
          "trunk": true,
          "windows": false
        },
        "key_identifier": "abc123...",
        "name": "我的主钥匙",
        "description": "日常使用",
        "expires_at": "2027-05-26T10:00:00Z",
        "last_used_at": "2026-05-25T15:30:00Z",
        "usage_count": 42,
        "is_shared": false,
        "created_at": "2026-05-26T10:00:00Z",
        "updated_at": "2026-05-26T10:00:00Z"
      }
    ],
    "total": 1,
    "page": 1,
    "page_size": 10
  }
}
```

### 3.3 获取钥匙详情

```http
GET /api/v1/keys/:id
Authorization: Bearer <token>
```

**响应 (200):** 同 3.2 中的单个钥匙 `data` 对象。

### 3.4 激活钥匙

```http
POST /api/v1/keys/:id/activate
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "钥匙已激活",
  "data": { "...钥匙对象..." }
}
```

### 3.5 停用钥匙

```http
POST /api/v1/keys/:id/deactivate
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "钥匙已停用",
  "data": { "...钥匙对象..." }
}
```

### 3.6 撤销钥匙

```http
DELETE /api/v1/keys/:id
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "钥匙已撤销"
}
```

撤销钥匙会同时撤销该钥匙的所有分享记录。

### 3.7 更新钥匙权限

```http
PUT /api/v1/keys/:id/permissions
Authorization: Bearer <token>
Content-Type: application/json

{
  "permissions": {
    "unlock": true,
    "lock": true,
    "start_engine": false,
    "trunk": false,
    "windows": false
  }
}
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "权限更新成功"
}
```

### 3.8 获取钥匙使用日志

```http
GET /api/v1/keys/:id/logs?page=1&page_size=20
Authorization: Bearer <token>
```

| 参数 | 类型 | 默认 | 说明 |
|------|------|------|------|
| page | int | 1 | 页码 |
| page_size | int | 20 | 每页数量 |

**响应 (200):**
```json
{
  "code": 200,
  "message": "获取成功",
  "data": {
    "list": [],
    "total": 0,
    "page": 1,
    "page_size": 20
  }
}
```

---

## 4. 钥匙分享 API

### 4.1 分享钥匙

```http
POST /api/v1/keys/:id/share
Authorization: Bearer <token>
Content-Type: application/json

{
  "user_id": 2,
  "permissions": {
    "unlock": true,
    "lock": true,
    "start_engine": false,
    "trunk": true,
    "windows": false
  },
  "expires_at": "2026-06-26T10:00:00Z"
}
```

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| user_id | uint | 是 | 被分享用户 ID |
| permissions | object | 否 | 分享权限（不能超过原钥匙权限） |
| expires_at | datetime | 否 | 分享过期时间 |

**响应 (201):**
```json
{
  "code": 201,
  "message": "钥匙分享成功",
  "data": {
    "id": 1,
    "key_id": 1,
    "shared_to": 2,
    "shared_by": 1,
    "permissions": {
      "unlock": true,
      "lock": true,
      "start_engine": false,
      "trunk": true,
      "windows": false
    },
    "status": "active",
    "shared_at": "2026-05-26T10:00:00Z",
    "expires_at": "2026-06-26T10:00:00Z"
  }
}
```

### 4.2 获取收到的分享钥匙

```http
GET /api/v1/keys/shared/list
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "获取成功",
  "data": [
    {
      "id": 1,
      "key_id": 1,
      "shared_to": 2,
      "shared_by": 1,
      "status": "active",
      "shared_at": "2026-05-26T10:00:00Z",
      "expires_at": "2026-06-26T10:00:00Z",
      "key": { "...钥匙对象..." },
      "shared_by_user": { "...用户对象..." }
    }
  ]
}
```

### 4.3 获取钥匙的分享列表

```http
GET /api/v1/keys/:id/shares
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "获取成功",
  "data": [
    {
      "id": 1,
      "key_id": 1,
      "shared_to": 2,
      "shared_by": 1,
      "status": "active",
      "shared_at": "2026-05-26T10:00:00Z",
      "expires_at": "2026-06-26T10:00:00Z"
    }
  ]
}
```

### 4.4 撤销分享

```http
DELETE /api/v1/keys/shares/:share_id
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "分享已撤销"
}
```

---

## 5. 车辆管理 API

### 5.1 注册车辆

```http
POST /api/v1/vehicles
Authorization: Bearer <token>
Content-Type: application/json

{
  "vin": "LVSHCADMXXX123456",
  "brand": "奔驰",
  "model": "E300L",
  "year": 2024,
  "color": "黑色",
  "plate": "京A12345",
  "name": "我的奔驰",
  "ble_mac": "AA:BB:CC:DD:EE:FF",
  "uwb_capable": true
}
```

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| vin | string | 是 | VIN码 (17位) |
| brand | string | 是 | 品牌 |
| model | string | 是 | 车型 |
| year | int | 是 | 年份 (1900-2100) |
| color | string | 否 | 颜色 |
| plate | string | 否 | 车牌号 |
| name | string | 是 | 车辆名称 |
| ble_mac | string | 否 | BLE MAC地址 |
| uwb_capable | bool | 否 | 是否支持UWB |

**响应 (201):**
```json
{
  "id": 1,
  "owner_id": 1,
  "name": "我的奔驰",
  "vin": "LVSHCADMXXX123456",
  "plate": "京A12345",
  "brand": "奔驰",
  "model": "E300L",
  "year": 2024,
  "color": "黑色",
  "status": "active",
  "lock_status": "locked",
  "engine_status": "off",
  "online_status": "offline",
  "ble_mac": "AA:BB:CC:DD:EE:FF",
  "uwb_capable": true,
  "uwb_enabled": false,
  "last_seen_at": null,
  "software_version": "",
  "ota_available": false,
  "created_at": "2026-05-26T10:00:00Z",
  "updated_at": "2026-05-26T10:00:00Z"
}
```

### 5.2 获取用户车辆列表

```http
GET /api/v1/vehicles?page=1&page_size=10
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "vehicles": [{ "...车辆对象..." }],
  "total": 1,
  "page": 1,
  "pageSize": 10
}
```

### 5.3 获取车辆详情

```http
GET /api/v1/vehicles/:id
Authorization: Bearer <token>
```

**响应 (200):** 单个车辆对象（同 5.1 响应结构）

### 5.4 获取车辆状态

```http
GET /api/v1/vehicles/:id/status
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "id": 1,
  "status": "online",
  "lock_status": "locked",
  "engine_status": "off",
  "online_status": "online",
  "location": {
    "latitude": 39.9042,
    "longitude": 116.4074,
    "altitude": 50.0,
    "accuracy": 10.0,
    "timestamp": 1719300000
  },
  "last_seen_at": "2026-05-26T10:00:00Z",
  "software_version": "2.1.0"
}
```

### 5.5 发送车辆命令

```http
POST /api/v1/vehicles/:id/commands
Authorization: Bearer <token>
Content-Type: application/json

{
  "command": "unlock",
  "params": {}
}
```

支持的命令类型: `unlock`, `lock`, `engine_start`, `engine_stop`, `trunk`, `windows_up`, `windows_down`, `find_my_car`.

**响应 (202):**
```json
{
  "command_id": "cmd_abc123",
  "status": "pending"
}
```

### 5.6 查询命令状态

```http
GET /api/v1/vehicles/:id/commands/:command_id
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "command_id": "cmd_abc123",
  "command": "unlock",
  "status": "completed",
  "message": "车辆已解锁",
  "created_at": "2026-05-26T10:00:00Z",
  "executed_at": "2026-05-26T10:00:05Z"
}
```


### 5.7 更新车辆位置

```http
PUT /api/v1/vehicles/:id/location
Authorization: Bearer ***
Content-Type: application/json

{
  "latitude": 39.9042,
  "longitude": 116.4074,
  "altitude": 50.0,
  "accuracy": 10.0,
  "speed": 0.0,
  "heading": 0.0,
  "timestamp": 1719300000
}
```

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| latitude | float | 是 | 纬度 (-90 ~ 90) |
| longitude | float | 是 | 经度 (-180 ~ 180) |
| altitude | float | 否 | 海拔 (米) |
| accuracy | float | 否 | 定位精度 (米) |
| speed | float | 否 | 速度 (km/h) |
| heading | float | 否 | 朝向 (度, 0=北) |
| timestamp | int64 | 是 | Unix 时间戳 (秒) |

**响应 (200):**
```json
{
  "code": 200,
  "message": "位置更新成功"
}
```

### 5.8 车辆心跳

```http
POST /api/v1/vehicles/:id/heartbeat
Authorization: Bearer ***
Content-Type: application/json

{
  "lock_status": "locked",
  "engine_status": "off",
  "battery_level": 85,
  "location": {
    "latitude": 39.9042,
    "longitude": 116.4074
  },
  "timestamp": 1719300000
}
```

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| lock_status | string | 否 | 门锁状态: `locked`, `unlocked` |
| engine_status | string | 否 | 引擎状态: `on`, `off` |
| battery_level | int | 否 | 电量百分比 (0-100) |
| location | object | 否 | 位置信息 (latitude/longitude) |
| timestamp | int64 | 是 | Unix 时间戳 (秒) |

**响应 (200):**
```json
{
  "code": 200,
  "message": "心跳接收成功",
  "data": {
    "next_heartbeat_interval": 60
  }
}
```

---

## 6. OTA API

### 6.1 创建固件

```http
POST /api/v1/ota/firmwares
Authorization: Bearer <token>
Content-Type: application/json

{
  "version": "2.2.0",
  "type": "ecu",
  "url": "https://storage.yuledkcs.com/firmwares/ecu_2.2.0.bin",
  "size": 52428800,
  "checksum": "sha256:abc123...",
  "release_notes": "修复了蓝牙连接稳定性问题",
  "min_hardware_version": "1.0.0",
  "max_hardware_version": "3.0.0",
  "release_date": "2026-05-26T00:00:00Z"
}
```

固件类型: `ecu`, `ivi`, `tbox`, `adas`, `gateway`, `bms`

**响应 (201):**
```json
{
  "code": 201,
  "message": "固件创建成功",
  "data": {
    "id": 1,
    "version": "2.2.0",
    "type": "ecu",
    "size": 52428800,
    "checksum": "sha256:abc123...",
    "release_notes": "修复了蓝牙连接稳定性问题",
    "min_hardware_version": "1.0.0",
    "max_hardware_version": "3.0.0",
    "release_date": "2026-05-26T00:00:00Z",
    "created_at": "2026-05-26T10:00:00Z"
  }
}
```

### 6.2 获取固件列表

```http
GET /api/v1/ota/firmwares?type=ecu&page=1&page_size=10
Authorization: Bearer <token>
```

### 6.3 获取固件详情

```http
GET /api/v1/ota/firmwares/:id
Authorization: Bearer <token>
```

### 6.4 更新固件

```http
PUT /api/v1/ota/firmwares/:id
Authorization: Bearer <token>
```

### 6.5 删除固件

```http
DELETE /api/v1/ota/firmwares/:id
Authorization: Bearer <token>
```

### 6.6 停用固件

```http
PUT /api/v1/ota/firmwares/:id/deactivate
Authorization: Bearer <token>
```

### 6.7 检查更新 (POST)

```http
POST /api/v1/ota/check
Content-Type: application/json

{
  "current_version": "2.0.0",
  "hardware_version": "1.0.0",
  "firmware_type": "ecu",
  "vehicle_id": 1
}
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "检查完成",
  "data": {
    "has_update": true,
    "firmware": { "...固件对象..." },
    "update_required": false,
    "message": "有新版本可用: 2.2.0"
  }
}
```

### 6.8 检查更新 (GET)

```http
GET /api/v1/ota/updates?current_version=2.0.0&hardware_version=1.0.0&firmware_type=ecu&vehicle_id=1
```

### 6.9 获取车辆 OTA 状态

```http
GET /api/v1/vehicles/:id/ota/status
Authorization: Bearer <token>
```

**响应 (200):**
```json
{
  "code": 200,
  "message": "获取成功",
  "data": {
    "id": "1",
    "vehicle_id": 1,
    "current_version": "2.0.0",
    "target_version": "2.2.0",
    "status": "downloading",
    "progress": 45,
    "error_message": null,
    "started_at": "2026-05-26T10:00:00Z",
    "completed_at": null,
    "updated_at": "2026-05-26T10:05:00Z"
  }
}
```

OTA 状态: `pending`, `downloading`, `installing`, `completed`, `failed`, `rolled_back`

### 6.10 更新车辆 OTA 状态

```http
PUT /api/v1/vehicles/:id/ota/status
Authorization: Bearer <token>
```

### 6.11 启动 OTA 更新

```http
POST /api/v1/vehicles/:id/ota/start
Authorization: Bearer <token>
Content-Type: application/json

{
  "target_version": "2.2.0"
}
```

### 6.12 获取 OTA 历史

```http
GET /api/v1/vehicles/:id/ota/history?page=1&page_size=10
Authorization: Bearer <token>
```

---

## 7. WebSocket 事件 API

### 7.1 WebSocket 连接

**用户连接:**
```
GET /ws?vehicle_id=1
Upgrade: websocket
Authorization: Bearer <token>
```

**车辆连接:**
```
GET /ws/vehicle/:id
Upgrade: websocket
```

**管理员连接:**
```
GET /ws/admin
Upgrade: websocket
Authorization: Bearer <token>
```

### 7.2 消息格式

所有 WebSocket 消息遵循统一的 JSON 格式：

```json
{
  "type": "message_type",
  "timestamp": "2026-05-26T10:00:00Z",
  "payload": { ... }
}
```

### 7.3 客户端到服务器消息

| 消息类型 | 方向 | 说明 |
|----------|------|------|
| `vehicle_status` | Client → Server | 车辆上报状态更新 |
| `command_result` | Client → Server | 命令执行结果上报 |
| `ping` | Client → Server | 心跳检测 |

**ping 示例:**
```json
{
  "type": "ping",
  "timestamp": "2026-05-26T10:00:00Z",
  "payload": {}
}
```

**响应:** `{"type": "pong", ...}`

**车辆状态上报示例:**
```json
{
  "type": "vehicle_status",
  "timestamp": "2026-05-26T10:00:00Z",
  "payload": {
    "lock_status": "locked",
    "engine_status": "off",
    "battery_level": 85,
    "location": {
      "lat": 39.9042,
      "lng": 116.4074
    }
  }
}
```

### 7.4 服务器到客户端消息

| 消息类型 | 方向 | 说明 |
|----------|------|------|
| `vehicle_status_update` | Server → Client | 车辆状态变更通知 |
| `command` | Server → Client | 服务器下发命令到车辆 |
| `command_result` | Server → Client | 命令执行结果通知 |
| `key_status_changed` | Server → Client | 钥匙状态变更通知 |
| `key_shared` | Server → Client | 收到钥匙分享通知 |
| `ota_progress` | Server → Client | OTA 更新进度通知 |
| `notification` | Server → Client | 通用通知 |

**命令下发示例 (服务端→车辆):**
```json
{
  "type": "command",
  "timestamp": "2026-05-26T10:00:00Z",
  "payload": {
    "command": "lock",
    "params": {},
    "command_id": "cmd_abc123"
  }
}
```

**钥匙状态变更通知示例:**
```json
{
  "type": "key_status_changed",
  "timestamp": "2026-05-26T10:00:00Z",
  "payload": {
    "key_id": 1,
    "status": "revoked",
    "reason": "owner_cancelled"
  }
}
```

---

## 8. MQTT 主题规范

MQTT 桥接服务运行于独立端口（默认 8085），提供 MQTT 与 WebSocket 之间的消息转发。

### 8.1 MQTT 订阅主题

桥接服务订阅以下主题：

| 主题 | QoS | 说明 |
|------|-----|------|
| `vehicle/+/status` | 1 | 车辆状态更新 |
| `vehicle/+/telemetry` | 1 | 车辆遥测数据 |
| `vehicle/+/response` | 1 | 车辆命令响应 |
| `user/+/notification` | 1 | 用户通知 |
| `ota/+/progress` | 1 | OTA 更新进度 |
| `key/+/status` | 1 | 钥匙状态变更 |

### 8.2 MQTT 发布主题

| 主题 | 说明 |
|------|------|
| `vehicle/{id}/command` | 下发命令到指定车辆 |
| `vehicle/{id}/command/{cmd_id}/result` | 命令执行结果 |
| `user/{id}/notification` | 推送给指定用户的通知 |
| `ota/{id}/progress` | OTA 进度更新 |

### 8.3 ACL 规则

| 用户名 | 允许的主题 | 说明 |
|--------|-----------|------|
| `admin`, `bridge` | 所有主题 | 系统管理员 |
| 普通用户 | `vehicle/`, `user/`, `ota/`, `key/` 前缀 | 按权限过滤 |

### 8.4 MQTT 认证

桥接服务提供 HTTP 认证回调端点，供 EMQ X 等 MQTT Broker 调用：

| 端点 | 说明 |
|------|------|
| `POST /api/v1/mqtt/auth` | MQTT 连接认证 |
| `POST /api/v1/mqtt/acl` | MQTT 发布/订阅 ACL 检查 |

---

## 9. 数据模型

### 9.1 DigitalKey (钥匙)

| 字段 | 类型 | 说明 |
|------|------|------|
| id | uint | 唯一标识 |
| user_id | uint | 所有者 ID |
| vehicle_id | uint | 车辆 ID |
| vehicle_name | string | 车辆名称（可选） |
| type | string | 协议类型: `CCC`, `ICCE`, `ICCOA` |
| status | string | 状态: `active`, `inactive`, `revoked`, `expired`, `pending` |
| permissions | KeyPermissions | 权限对象 |
| key_identifier | string | 钥匙唯一标识符 |
| name | string | 名称 |
| description | string | 描述 |
| expires_at | datetime | 过期时间 |
| last_used_at | datetime | 最后使用时间 |
| usage_count | int | 使用次数 |
| is_shared | bool | 是否是分享的钥匙 |
| shared_by | uint | 分享者 ID（被分享钥匙） |
| created_at | datetime | 创建时间 |
| updated_at | datetime | 更新时间 |

### 9.2 KeyPermissions (钥匙权限)

| 字段 | 类型 | 说明 |
|------|------|------|
| unlock | bool | 解锁 |
| lock | bool | 上锁 |
| start_engine | bool | 启动引擎 |
| trunk | bool | 后备箱 |
| windows | bool | 车窗 |

### 9.3 KeyShare (钥匙分享)

| 字段 | 类型 | 说明 |
|------|------|------|
| id | uint | 唯一标识 |
| key_id | uint | 钥匙 ID |
| shared_to | uint | 被分享用户 ID |
| shared_by | uint | 分享者用户 ID |
| permissions | KeyPermissions | 分享的权限 |
| status | string | 状态: `active`, `revoked`, `expired` |
| shared_at | datetime | 分享时间 |
| expires_at | datetime | 过期时间 |
| revoked_at | datetime | 撤销时间 |

### 9.4 Vehicle (车辆)

| 字段 | 类型 | 说明 |
|------|------|------|
| id | uint | 唯一标识 |
| owner_id | uint | 车主 ID |
| name | string | 车辆名称 |
| vin | string | VIN码 |
| plate | string | 车牌号 |
| brand | string | 品牌 |
| model | string | 车型 |
| year | int | 年份 |
| color | string | 颜色 |
| status | string | 状态: `active`, `inactive`, `maintenance` |
| lock_status | string | 门锁状态: `locked`, `unlocked` |
| engine_status | string | 引擎状态: `on`, `off` |
| online_status | string | 在线状态: `online`, `offline` |
| ble_mac | string | BLE MAC地址 |
| uwb_capable | bool | 是否支持UWB |
| uwb_enabled | bool | UWB是否启用 |
| location | string | 位置信息(JSON) |
| last_seen_at | datetime | 最后在线时间 |
| software_version | string | 软件版本 |
| hardware_version | string | 硬件版本 |
| ota_available | bool | OTA可用 |

### 9.5 Firmware (固件)

| 字段 | 类型 | 说明 |
|------|------|------|
| id | uint | 唯一标识 |
| version | string | 版本号 |
| type | string | 类型: `ecu`, `ivi`, `tbox`, `adas`, `gateway`, `bms` |
| url | string | 下载地址 |
| size | int64 | 文件大小 |
| checksum | string | 校验和 |
| signature | string | 签名 |
| release_notes | string | 发布说明 |
| min_hardware_version | string | 最低硬件版本 |
| max_hardware_version | string | 最高硬件版本 |
| is_active | bool | 是否启用 |
| release_date | datetime | 发布日期 |

### 9.6 VehicleOTAStatus (车辆OTA状态)

| 字段 | 类型 | 说明 |
|------|------|------|
| id | uint | 唯一标识 |
| vehicle_id | uint | 车辆 ID |
| current_version | string | 当前版本 |
| target_version | string | 目标版本 |
| status | string | 状态: `pending`, `downloading`, `installing`, `completed`, `failed`, `rolled_back` |
| progress | int | 进度 0-100 |
| error_message | string | 错误信息 |

### 9.7 VehicleCommandResponse (命令响应)

| 字段 | 类型 | 说明 |
|------|------|------|
| command_id | string | 命令 ID |
| command | string | 命令类型 |
| status | string | 执行状态: `pending`, `processing`, `completed`, `failed` |
| message | string | 消息 |
| created_at | datetime | 创建时间 |
| executed_at | datetime | 执行时间 |

### 9.8 User (用户)

| 字段 | 类型 | 说明 |
|------|------|------|
| id | uint | 唯一标识 |
| username | string | 用户名 |
| email | string | 邮箱 |
| phone | string | 手机号 |
| role | string | 角色: `user`, `admin` |
| created_at | datetime | 创建时间 |

---

## 10. 通用响应格式

### 10.1 成功响应 (统一格式)

钥匙和 OTA 相关接口使用统一响应格式:

```json
{
  "code": 200,
  "message": "操作成功描述",
  "data": { ... }
}
```

车辆接口使用简化格式:

```json
{
  "...直接返回对象或数组..."
}
```

### 10.2 列表响应 (分页)

```json
{
  "code": 200,
  "message": "获取成功",
  "data": {
    "list": [ ... ],
    "total": 100,
    "page": 1,
    "page_size": 20
  }
}
```

### 10.3 错误响应

```json
{
  "error": "错误描述",
  "code": "ERROR_CODE"
}
```

### 10.4 认证错误

```json
{
  "error": "缺少 Authorization 头",
  "code": "AUTH_MISSING"
}
```

```json
{
  "error": "Token 已过期",
  "code": "AUTH_TOKEN_EXPIRED"
}
```

```json
{
  "error": "Token 无效",
  "code": "AUTH_INVALID_TOKEN"
}
```

---

## 11. 错误码

| HTTP 状态码 | 错误码 | 说明 |
|------------|--------|------|
| 400 | BAD_REQUEST | 请求参数错误 |
| 401 | AUTH_MISSING | 缺少认证头 |
| 401 | AUTH_TOKEN_EXPIRED | Token 已过期 |
| 401 | AUTH_INVALID_TOKEN | Token 无效 |
| 401 | AUTH_INVALID_FORMAT | 认证格式错误 |
| 403 | FORBIDDEN | 权限不足 |
| 403 | INSUFFICIENT_PERMISSIONS | 权限不够 |
| 404 | NOT_FOUND | 资源不存在 |
| 409 | CONFLICT | 状态冲突（如OTA正在进行中） |
| 429 | RATE_LIMITED | 请求过于频繁 |
| 500 | INTERNAL_ERROR | 服务器内部错误 |

---

## 12. 认证方式

所有需要认证的 API 端点使用 **JWT Bearer Token**:

```http
Authorization: Bearer eyJhbGciOiJIUzI1NiIs...
```

Token 默认有效期为 **24 小时**, 过期后 60 天内可通过 `/auth/refresh` 刷新。

### 12.1 JWT Claims 结构

```json
{
  "user_id": "1",
  "username": "zhangsan",
  "role": "user",
  "exp": 1719300000,
  "iat": 1719213600,
  "iss": "yuleDKCS"
}
```

---

## 13. Pagination 分页规范

接口统一使用以下分页参数：

| 参数 | 类型 | 默认 | 说明 |
|------|------|------|------|
| page | int | 1 | 页码 |
| page_size | int | 10-20 | 每页数量 |

**注意**: 不同接口默认 page_size 不同:
- 钥匙列表: 默认 10
- 钥匙日志: 默认 20
- 固件列表: 默认 10
- OTA 历史: 默认 10
