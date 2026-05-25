# yuleDKCS 三端联调指南

## 概述

本文档指导如何在本地环境进行 yuleDKCS 三端（Backend + Frontend + Mobile）联调测试。

## 环境要求

- Go 1.20+
- Node.js 18+
- Docker & Docker Compose
- PostgreSQL 15（可选，可用 Docker）

---

## 快速启动

### 第一步：启动基础设施

```bash
cd /home/admin/yuleDKCS/deploy

# 启动 Postgres 和 Redis
docker-compose -f docker-compose.dev.yml up -d postgres redis
```

### 第二步：初始化数据库

```bash
cd /home/admin/yuleDKCS/backend

# 执行数据库迁移
make migrate-up

# 导入种子数据
make seed

# 或使用 psql 直接执行
# psql postgres://yuledkcs:yuledkcs_dev@localhost:5432/yuledkcs -f database/migrations/001_initial_schema.sql
# psql postgres://yuledkcs:yuledkcs_dev@localhost:5432/yuledkcs -f database/seeds/001_test_data.sql
```

### 第三步：启动后端服务

```bash
cd /home/admin/yuleDKCS/backend

# 开发模式（热重载）
make dev

# 或直接运行
go run main.go
```

后端将在 http://localhost:8080 启动

### 第四步：启动前端服务

```bash
cd /home/admin/yuleDKCS/frontend

# 安装依赖
npm install

# 启动开发服务
npm run dev
```

前端将在 http://localhost:5173 启动（默认）

---

## 测试步骤

### 1. 测试登录接口

```bash
# 测试登录
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "testuser",
    "password": "password123"
  }'

# 预期响应：200 OK + JWT Token
{
  "code": 200,
  "message": "登录成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
    "type": "Bearer",
    "user": {
      "id": 1,
      "username": "testuser",
      "email": "test@example.com"
    }
  }
}
```

### 2. 测试钥匙列表接口

```bash
# 使用登录返回的 token
export TOKEN="your_jwt_token_here"

curl -X GET http://localhost:8080/api/v1/keys \
  -H "Authorization: Bearer $TOKEN"
```

### 3. 测试前端登录

1. 打开 http://localhost:5173/auth/login
2. 输入测试账号：testuser / password123
3. 点击登录
4. 验证跳转到首页

### 4. 测试钥匙功能

- 查看钥匙列表
- 点击钥匙进入详情
- 测试激活/停用功能
- 查看使用日志

---

## API 端点一览

| 方法 | 端点 | 说明 | 认证 |
|------|------|------|------|
| POST | /api/v1/auth/login | 登录 | 否 |
| POST | /api/v1/auth/register | 注册 | 否 |
| POST | /api/v1/auth/refresh | 刷新Token | 是 |
| GET | /api/v1/keys | 钥匙列表 | 是 |
| GET | /api/v1/keys/:id | 钥匙详情 | 是 |
| POST | /api/v1/keys/:id/activate | 激活钥匙 | 是 |
| POST | /api/v1/keys/:id/deactivate | 停用钥匙 | 是 |
| GET | /api/v1/keys/:id/logs | 使用日志 | 是 |
| POST | /api/v1/keys/:id/share | 分享钥匙 | 是 |
| GET | /api/v1/user/profile | 用户信息 | 是 |

---

## 常见问题

### Q1: 数据库连接失败

**问题：**
```
dial tcp localhost:5432: connect: connection refused
```

**解决：**
```bash
# 确保 PostgreSQL 容器运行中
docker ps | grep postgres

# 如果未运行，重新启动
docker-compose -f docker-compose.dev.yml up -d postgres
```

### Q2: 跨域请求被拒绝

**问题：**
```
CORS policy: No 'Access-Control-Allow-Origin' header
```

**解决：**
- 确保后端启用了 CORS 中间件
- 前端 `client.ts` 配置了正确的 baseURL

### Q3: JWT Token 过期

**现象：** API 返回 401

**解决：**
- 前端会自动跳转到登录页
- 或调用 `/auth/refresh` 端点刷新 token

---

## 目录结构

```
yuleDKCS/
├── backend/               # Go 后端
│   ├── database/
│   │   ├── migrations/    # 数据库迁移
│   │   └── seeds/         # 种子数据
│   ├── internal/
│   │   ├── handlers/      # HTTP 处理器
│   │   ├── middleware/    # JWT/CORS 等
│   │   ├── services/      # 业务逻辑
│   │   ├── repository/    # 数据访问层
│   │   └── router/        # 路由配置
│   └── Makefile           # 开发命令
├── frontend/              # React 前端
│   ├── src/api/
│   │   ├── client.ts      # Axios 配置
│   │   ├── auth.ts        # 认证 API
│   │   └── keys.ts        # 钥匙 API
│   └── src/pages/auth/
│       └── LoginPage.tsx  # 登录页面
├── mobile/
│   ├── android/           # Android SDK
│   │   └── api/
│   │       └── YuleDKCSApi.kt
│   └── ios/               # iOS SDK
│       └── Services/
│           └── APIService.swift
├── deploy/
│   └── docker-compose.dev.yml
└── docs/
    └── protocol-specs/
        └── API_ENDPOINTS.md
```

---

## 开发命令速查

### 后端
```bash
make build          # 构建
make run            # 运行
make dev            # 热重载开发
make test           # 测试
make migrate-up     # 数据库迁移
make seed           # 种子数据
make docker-up      # 启动 Docker
```

### 前端
```bash
npm install         # 安装依赖
npm run dev         # 开发服务
npm run build       # 生产构建
npm run test        # 测试
```

---

## 测试账号

| 用户名 | 邮箱 | 密码 |
|--------|------|------|
| testuser | test@example.com | password123 |
| admin | admin@example.com | password123 |
| friend1 | friend1@example.com | password123 |

---

## 下一步

1. **验证后端 API** - 使用 curl 测试所有端点
2. **验证前端功能** - 完整走通登录到钥匙管理流程
3. **Mobile 对接** - Android/iOS SDK 调试
4. **编写联调测试用例** - 自动化测试

---

*Last Updated: 2024-05-12*
*Commit: 46e4ac6*
