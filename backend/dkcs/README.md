# DKCS (Digital Key Control Service)

Go层核心服务 - 负责密钥生命周期管理、车控指令下发、事件流处理

## 目录结构

```
cloud/dkcs/
├── cmd/dkcs/main.go              # 服务入口
├── internal/
│   ├── service/                  # 业务服务层
│   │   ├── key_service.go       # 密钥管理（创建/激活/吊销/分享）
│   │   ├── command_service.go   # 车控指令（解锁/闭锁/启动等）
│   │   └── event_service.go     # 事件流处理
│   ├── repository/              # 数据仓储层
│   │   ├── key_repo.go         # 密钥仓储
│   │   ├── vehicle_repo.go     # 车辆仓储
│   │   └── event_repo.go       # 事件仓储
│   ├── cache/                   # 缓存层
│   │   └── redis.go            # Redis操作封装
│   ├── mq/                      # 消息队列
│   │   └── kafka.go            # Kafka生产/消费
│   ├── middleware/             # 中间件
│   │   └── middleware.go       # 鉴权/限流/日志/恢复/超时
│   └── config/                  # 配置管理
│       └── config.go           # 环境变量加载
├── go.mod
└── README.md
```

## 功能模块

### 密钥管理 (KeyService)
- CreateKey: 创建新密钥
- ActivateKey: 激活密钥
- RevokeKey: 吊销密钥
- GetKey: 查询密钥
- ListKeys: 列出密钥
- ShareKey: 分享密钥

### 车控指令 (CommandService)
- Unlock: 解锁
- Lock: 上锁
- EngineStart: 启动引擎
- EngineStop: 停止引擎
- TrunkOpen: 打开后备箱
- Panic: 紧急报警
- FindVehicle: 寻车

### 事件流 (EventService)
- RecordEvent: 记录事件
- ListEvents: 查询事件列表
- StreamEvents: 实时事件流
- GetEventStats: 事件统计

## 运行

```bash
# 本地开发
go run cmd/dkcs/main.go

# 环境变量
export GRPC_PORT=50051
export DB_HOST=localhost
export DB_PORT=5432
export DB_USER=digitalkey
export DB_PASSWORD=your-password
export DB_NAME=digitalkey_db
export REDIS_ADDR=localhost:6379
export KAFKA_BROKERS=localhost:9092
export JWT_SECRET=your-secret-key

# Docker构建
docker build -t dkcs:latest .
docker run -p 50051:50051 dkcs:latest
```

## 协议规范

遵循 `protocol/unified/unified-protocol-spec.md`:
- BER-TLV编码
- 统一错误码 (DKxx, ACxx, SYxx)
- 统一日志格式
- 统一埋点接口

## 健康检查

gRPC健康检查服务:
```bash
grpcurl -plaintext localhost:50051 grpc.health.v1.Health/Check
```

## 监控指标

Prometheus指标端口: `:9090/metrics`
