# yuleDKCS API 端点规范

## 基础信息

| 项目 | 值 |
|------|-----|
| Base URL | `http://localhost:8080/api/v1` |
| 认证方式 | Bearer Token (JWT) |
| Content-Type | `application/json` |

## 认证相关

### POST /auth/login
用户登录

**请求体:**
```json
{
  "username": "testuser",
  "password": "password123"
}
```

**响应:**
```json
{
  "code": 200,
  "message": "登录成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
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

### POST /auth/register
用户注册

**请求体:**
```json
{
  "username": "newuser",
  "email": "new@example.com",
  "password": "password123"
}
```

### POST /auth/refresh
刷新 Token

**请求头:** `Authorization: Bearer {token}`

## 钥匙管理

### GET /keys
获取用户钥匙列表

**请求头:** `Authorization: Bearer {token}`

**查询参数:**
- `page`: 页码 (默认: 1)
- `page_size`: 每页数量 (默认: 10)

**响应:**
```json
{
  "code": 200,
  "message": "获取成功",
  "data": {
    "list": [...],
    "total": 10,
    "page": 1,
    "page_size": 10
  }
}
```

### GET /keys/:id
获取钥匙详情

### POST /keys/issue
发行新钥匙

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

### POST /keys/:id/activate
激活钥匙

### POST /keys/:id/deactivate
停用钥匙

### DELETE /keys/:id
撤销钥匙

### PUT /keys/:id/permissions
更新钥匙权限

**请求体:**
```json
{
  "unlock": true,
  "lock": true,
  "start_engine": false
}
```

### GET /keys/:id/logs
获取钥匙使用日志

**查询参数:**
- `page`: 页码 (默认: 1)
- `page_size`: 每页数量 (默认: 20)

## 钥匙分享

### GET /keys/shared/list
获取分享的钥匙列表

### POST /keys/:id/share
分享钥匙

**请求体:**
```json
{
  "shared_to_username": "friend1",
  "expires_at": "2024-12-31T23:59:59Z",
  "permissions": {
    "unlock": true,
    "lock": true
  }
}
```

### GET /keys/:id/shares
获取钥匙的分享记录

### DELETE /keys/shares/:share_id
撤销分享

## 用户信息

### GET /user/profile
获取当前用户信息

**请求头:** `Authorization: Bearer {token}`

**响应:**
```json
{
  "code": 200,
  "data": {
    "id": 1,
    "username": "testuser",
    "email": "test@example.com",
    "phone": "13800138000",
    "role": "user",
    "created_at": "2024-01-01T00:00:00Z"
  }
}
```

## 错误码说明

| 错误码 | HTTP状态 | 说明 |
|--------|---------|------|
| 400 | Bad Request | 请求参数错误 |
| 401 | Unauthorized | 未认证或Token无效 |
| 403 | Forbidden | 权限不足 |
| 404 | Not Found | 资源不存在 |
| 500 | Internal Server Error | 服务器内部错误 |

## 三端联调测试步骤

### 1. 启动后端服务
```bash
cd /home/admin/yuleDKCS/deploy
docker-compose -f docker-compose.dev.yml up -d

# 或本地运行
cd /home/admin/yuleDKCS/backend
make migrate-up
make seed
go run main.go
```

### 2. 测试登录
```bash
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"password123"}'
```

### 3. 测试钥匙列表
```bash
curl -X GET http://localhost:8080/api/v1/keys \
  -H "Authorization: Bearer {token}"
```

### 测试账号
- 用户名: `testuser`
- 密码: `password123`
