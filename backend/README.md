# yuleDKCS Backend

yuleDKCS 后端 API 服务 - Go + Gin 框架实现

## 技术栈

- **Web 框架**: Gin
- **ORM**: GORM + PostgreSQL
- **缓存**: Redis
- **认证**: JWT
- **实时通信**: WebSocket
- **配置管理**: Viper

## 项目结构

```
backend/
├── cmd/api/          # 应用启动入口
├── internal/         # 内部代码
│   ├── config/       # 配置管理
│   ├── router/       # 路由初始化
│   ├── middleware/   # 中间件
│   ├── handlers/     # HTTP 处理器
│   ├── models/       # 数据模型
│   ├── services/     # 业务逻辑
│   └── repository/   # 数据访问层
├── pkg/dkcs/         # yuleDKCS C++ 绑定
├── migrations/       # 数据库迁移
├── main.go           # 主入口
└── docker-compose.yml
```

## 快速开始

### 1. 配置环境

复制配置文件示例：

```bash
cp config.example.yaml config.yaml
```

编辑 config.yaml 设置您的配置。

### 2. 启动依赖服务

```bash
docker-compose up -d postgres redis
```

### 3. 运行应用

```bash
# 下载依赖
go mod download

# 运行
go run main.go
```

### 4. 验证

```bash
curl http://localhost:8080/health
```

## 配置方式

配置优先级（从高到低）：

1. 环境变量（使用 `YULEDKCS_` 前缀）
2. 配置文件 (config.yaml)
3. 默认值

示例环境变量：

```bash
export YULEDKCS_SERVER_PORT=8080
export YULEDKCS_DATABASE_HOST=localhost
export YULEDKCS_JWT_SECRET=your-secret
```

## Docker 部署

```bash
# 构建并启动所有服务
docker-compose up -d --build
```

## API 文档

启动后访问：http://localhost:8080/api/v1/ping

## 开发

```bash
# 格式化代码
go fmt ./...

# 运行测试
go test ./...

# 构建
go build -o main .
```

## License

MIT