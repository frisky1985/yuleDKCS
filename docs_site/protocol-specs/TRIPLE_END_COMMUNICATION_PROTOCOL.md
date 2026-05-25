# yuleDKCS 三端通信协议规范

> 版本: v1.0
> 更新日期: 2026-05-15
> 适用于: Backend + Frontend + Mobile 三端联调

---

## 1. 通信架构概览

```
┌────────────────────────────────────────────────────────────────────────┐
│                         yuleDKCS 系统架构                          │
└────────────────────────────────────────────────────────────────────────┘
                                   |
           ┌──────────────┬──────────────┬────────────────┐
           │             │             │                    │
           ▼             ▼             ▼                    ▼
    ┌───────────┐  ┌───────────┐  ┌───────────┐       ┌───────────┐
    │  Frontend   │  │   Mobile    │  │  Embedded   │       │   Vehicle   │
    │  (React)    │  │(Android/iOS)│  │   (MCU)      │       │   (ECU)     │
    └───────────┘  └───────────┘  └───────────┘       └───────────┘
           │             │             │                    ▲
           │             │             └────── BLE/UWB ──────┘
           │             └────── REST/WS ────────┘
           └─────── REST/WS ──────────────────────────────┘
                              |
                              ▼
                    ┌────────────────┐
                    │   Backend (Go)    │
                    │  - REST API       │
                    │  - WebSocket      │
                    │  - MQTT           │
                    └────────────────┘
```

---

## 2. 通信协议栈

### 2.1 协议层次

| 层次 | Frontend | Mobile | Embedded | Vehicle |
|------|----------|--------|----------|---------|
| 应用层 | REST API + WS | REST API + WS | CCC/ICCOA/ICCE | 自定义协议 |
| 安全层 | JWT + HTTPS | JWT + HTTPS | SCP03/DTLS | 内部安全通道 |
| 传输层 | HTTP/1.1 + WebSocket | HTTP/2 + WebSocket | BLE 5.0/UWB | CAN/Ethernet |
| 物理层 | WiFi/4G/5G | WiFi/4G/5G/BLE/UWB | BLE/UWB | 车辆总线 |

### 2.2 数字钥匙协议栈

```
┌────────────────────────────────────────────────────────────────────────┐
│                    数字钥匙通信协议栈                     │
└────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────┐
│  CCC 2.0 / ICCOA / ICCE (Application Layer)                      │
├────────────────────────────────────────────────────────────────────────┘
│  Secure Channel Protocol 03 (SCP03)                              │
├────────────────────────────────────────────────────────────────────────┘
│  BLE GATT / UWB (Data Link & Physical)                          │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. API 协议规范

### 3.1 基础信息

| 项目 | 值 |
|------|-----|
| Base URL | `http://localhost:8080/api/v1` |
| 认证方式 | Bearer Token (JWT) |
| Content-Type | `application/json` |
| 请求格式 | snake_case |
| 响应格式 | snake_case |

### 3.2 响应格式统一

```json
{
  "code": 200,
  "message": "success",
  "data": { }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int | 业务状态码 |
| message | string | 提示信息 |
| data | object/array | 数据载荷 |

### 3.3 错误码定义

| 错误码 | HTTP状态 | 说明 |
|--------|---------|------|
| 200 | OK | 成功 |
| 400 | Bad Request | 请求参数错误 |
| 401 | Unauthorized | 未认证或Token无效 |
| 403 | Forbidden | 权限不足 |
| 404 | Not Found | 资源不存在 |
| 429 | Too Many Requests | 请求过于频繁 |
| 500 | Internal Server Error | 服务器内部错误 |

---

## 4. 认证接口

### 4.1 登录

```http
POST /auth/login
```

**请求体:**
```json
{
  "username": "testuser",
  "password": "***"
}
```

**响应:**
```json
{
  "code": 200,
  "message": "登录成功",
  "data": {
    "token": "eyJhbG...",
    "type": "Bearer",
    "user": {
      "id": 1,
      "username": "testuser",
      "email": "test@example.com",
      "role": "user"
    }
  }
}
```

### 4.2 注册

```http
POST /auth/register
```

**请求体:**
```json
{
  "username": "newuser",
  "email": "new@example.com",
  "password": "***"
}
```

### 4.3 刷新Token

```http
POST /auth/refresh
Authorization: Bearer {token}
```

---

## 5. 钥匙管理接口

### 5.1 获取钥匙列表

```http
GET /keys?page=1&page_size=10
Authorization: Bearer {token}
```

**响应:**
```json
{
  "code": 200,
  "message": "获取成功",
  "data": {
    "list": [
      {
        "id": 1,
        "vehicle_id": 1,
        "name": "我的宝马",
        "type": "owner",
        "protocol": "CCC",
        "status": "active",
        "permissions": {
          "unlock": true,
          "lock": true,
          "start_engine": true,
          "open_trunk": true
        },
        "created_at": "2024-01-01T00:00:00Z",
        "expires_at": "2025-01-01T00:00:00Z"
      }
    ],
    "total": 10,
    "page": 1,
    "page_size": 10
  }
}
```

### 5.2 发行新钥匙

```http
POST /keys/issue
Authorization: Bearer {token}
```

**请求体:**
```json
{
  "vehicle_id": 1,
  "type": "owner",
  "protocol": "CCC",
  "name": "我的宝马",
  "permissions": {
    "unlock": true,
    "lock": true,
    "start_engine": true
  }
}
```

### 5.3 激活/停用钥匙

```http
POST /keys/{id}/activate
POST /keys/{id}/deactivate
Authorization: Bearer {token}
```

### 5.4 更新钥匙权限

```http
PUT /keys/{id}/permissions
Authorization: Bearer {token}
```

**请求体:**
```json
{
  "unlock": true,
  "lock": true,
  "start_engine": false
}
```

---

## 6. 车辆控制接口

### 6.1 注册车辆

```http
POST /vehicles
Authorization: Bearer {token}
```

**请求体:**
```json
{
  "vin": "WBA12345678901234",
  "brand": "BMW",
  "model": "X5",
  "year": 2024,
  "ble_mac": "AA:BB:CC:DD:EE:FF",
  "uwb_capable": true
}
```

### 6.2 获取车辆状态

```http
GET /vehicles/{id}/status
Authorization: Bearer {token}
```

**响应:**
```json
{
  "code": 200,
  "data": {
    "id": 1,
    "status": "active",
    "lock_status": "locked",
    "engine_status": "off",
    "online_status": "online",
    "last_seen_at": "2024-01-01T12:00:00Z",
    "software_version": "1.2.3",
    "location": {
      "latitude": 39.9042,
      "longitude": 116.4074,
      "altitude": 50.0,
      "accuracy": 10.0
    }
  }
}
```

### 6.3 发送控制命令

```http
POST /vehicles/{id}/commands
Authorization: Bearer {token}
```

**请求体:**
```json
{
  "command": "unlock",
  "params": {}
}
```

**命令列表:**
| 命令 | 说明 |
|------|------|
| unlock | 解锁车辆 |
| lock | 上锁车辆 |
| engine_start | 启动发动机 |
| engine_stop | 关闭发动机 |
| trunk | 开启后备箱 |
| windows_up | 关闭车窗 |
| windows_down | 打开车窗 |
| find_my_car | 寻车响应 |

---

## 7. WebSocket 协议

### 7.1 连接地址

```
ws://localhost:8080/ws?token={jwt_token}&vehicle_id={vehicle_id}
```

### 7.2 消息格式

```json
{
  "type": "event|command|status",
  "timestamp": "2024-01-01T12:00:00Z",
  "payload": {}
}
```

### 7.3 消息类型

| 类型 | 方向 | 说明 |
|------|------|------|
| vehicle_status | Server → Client | 车辆状态变更 |
| command_result | Server → Client | 命令执行结果 |
| key_update | Server → Client | 钥匙状态更新 |
| heartbeat | Client → Server | 心跳保活 |
| command | Client → Server | 发送控制命令 |

---

## 8. 数据模型对照

### 8.1 三端字段映射

| 后端字段 | 前端字段 | Mobile字段 | 说明 |
|----------|----------|-----------|------|
| id | id | keyId | 主键ID |
| created_at | createdAt | createdAt | 创建时间 |
| updated_at | updatedAt | updatedAt | 更新时间 |
| vehicle_id | vehicleId | vehicleId | 车辆ID |
| user_id | userId | userId | 用户ID |
| key_type | type | keyType | 钥匙类型 |
| key_status | status | keyStatus | 钥匙状态 |
| permissions | permissions | permissions | 权限对象 |

### 8.2 权限字段

```typescript
// 统一权限结构
interface KeyPermissions {
  unlock: boolean;        // 解锁
  lock: boolean;          // 上锁
  start_engine: boolean;  // 启动发动机
  stop_engine: boolean;   // 关闭发动机
  open_trunk: boolean;    // 开后备箱
  open_windows: boolean;  // 开车窗
  close_windows: boolean; // 关车窗
  remote_start: boolean;  // 远程启动
}
```

---

## 9. 安全规范

### 9.1 JWT Token

| 项目 | 规范 |
|------|------|
| 签名算法 | HS256 |
| 有效期 | 24小时 |
| 刷新窗口 | 7天 |
| Payload字段 | user_id, username, role, exp, iat |

### 9.2 HTTPS

- 生产环境必须使用HTTPS
- TLS 1.2+
- 禁用不安全的加密套件

### 9.3 限流策略

| 接口 | 限制 |
|------|------|
| /auth/login | 5次/分钟 |
| /auth/register | 3次/分钟 |
| API通用 | 100次/分钟/IP |

---

## 10. 本地联调测试

### 10.1 启动环境

```bash
# 1. 启动基础设施
cd /home/admin/yuleDKCS/deploy
docker-compose -f docker-compose.dev.yml up -d

# 2. 数据库迁移
cd /home/admin/yuleDKCS/backend
make migrate-up
make seed

# 3. 启动后端
SKIP_DB=true go run main.go
```

### 10.2 测试账号

| 账号 | 用户名 | 密码 |
|------|--------|------|
| 测试用户 | testuser | password123 |
| 管理员 | admin | admin123 |

### 10.3 快速测试

```bash
# 登录
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password": "***"}'

# 获取钥匙列表
curl -X GET http://localhost:8080/api/v1/keys \
  -H "Authorization: Bearer ***
```

---

## 11. 附录

### 11.1 相关文档

- [API_ENDPOINTS.md](./API_ENDPOINTS.md) - API端点详细说明
- [TRIPLE_INTEGRATION_GUIDE.md](../guides/TRIPLE_INTEGRATION_GUIDE.md) - 三端联调指南
- [API_REFERENCE.md](../API_REFERENCE.md) - API参考文档

### 11.2 协议版本

| 协议 | 版本 | 支持状态 |
|------|------|----------|
| CCC | 2.0 | 完全支持 |
| ICCOA | 1.0 | 完全支持 |
| ICCE | 1.0 | 完全支持 |

---

*文档维护: yuleDKCS 团队*
*最后更新: 2026-05-15*
