# yuleDKCS Docker 集成测试环境配置

## 服务概览

| 服务        | 镜像                           | 端口   | 用途                 |
|-------------|--------------------------------|--------|----------------------|
| PostgreSQL  | postgres:16-alpine             | 5432   | 持久化数据库          |
| Redis       | redis:7-alpine                 | 6379   | 缓存 / 分布式锁       |
| Zookeeper   | confluentinc/cp-zookeeper:7.6.0| 2181   | Kafka 协调服务        |
| Kafka       | confluentinc/cp-kafka:7.6.0    | 9092   | 消息队列 / 事件总线    |

## 快速启动

```bash
# 启动全部（生产 + 集成测试依赖）
cd ~/yuleDKCS && docker compose up -d

# 仅启动集成测试依赖（不启动 hub）
cd ~/yuleDKCS && docker compose up -d redis zookeeper kafka

# 查看服务状态
docker compose ps

# 查看日志
docker compose logs -f redis kafka

# 停止并清理
docker compose down
```

## 环境变量（Go 集成测试用）

```bash
# Redis
export REDIS_ADDR=localhost:6379
export REDIS_PASSWORD=
export REDIS_DB=0

# Kafka
export KAFKA_BROKERS=localhost:9092

# PostgreSQL (复用 docker-compose.yml 中的配置)
export DB_HOST=localhost
export DB_PORT=5432
export DB_USER=user
export DB_PASSWORD=pass
export DB_NAME=yuledkcs
```

## Go 代码中的默认值

参考 `backend/dkcs/internal/config/config.go`:

| 环境变量          | 默认值           | 说明              |
|-------------------|------------------|-------------------|
| `REDIS_ADDR`       | localhost:6379   | Redis 地址        |
| `REDIS_PASSWORD`   | (空)             | Redis 密码        |
| `REDIS_DB`         | 0                | Redis 数据库编号  |
| `KAFKA_BROKERS`    | localhost:9092   | Kafka broker 列表 |
| `KAFKA_TOPIC_KEY_EVENTS` | dkcs.key.events | 密钥事件 topic    |
| `KAFKA_TOPIC_COMMANDS`   | dkcs.commands    | 命令 topic        |
| `KAFKA_TOPIC_EVENTS`     | dkcs.events      | 通用事件 topic    |
| `KAFKA_TOPIC_DLQ`        | dkcs.dlq         | 死信队列 topic    |

由于 Docker 端口映射 `container:host` 一致（6379→6379, 9092→9092），Go 代码的默认配置可以直接连接，无需额外环境变量。

## 运行集成测试

```bash
# 运行所有测试（含集成测试标签）
cd ~/yuleDKCS && make test-integration

# 手动运行标记 integration 的测试
cd ~/yuleDKCS/backend/dkcs && go test -tags=integration -v -count=1 ./...

# 连接到 Kafka 手动验证
docker compose exec kafka kafka-topics --bootstrap-server localhost:9092 --list

# 连接到 Redis 手动验证
docker compose exec redis redis-cli ping
```

## 文件结构

```
yuleDKCS/
├── docker-compose.yml               ← 包含所有服务（生产 + 集成测试）
└── .yuleosh/
    └── docker-integration-config.md  ← 本文件
```

## 故障排查

- **端口冲突**: 如果本地 6379/9092 已被占用，在 `.env` 中覆盖端口映射，同时设置对应的 `REDIS_ADDR`/`KAFKA_BROKERS` 环境变量
- **Kafka 启动慢**: Kafka 依赖 Zookeeper，首次启动需要 20-30 秒。healthcheck 配置了 30s 的 `start_period`
- **镜像拉取**: 首次启动会自动 pull 镜像，依赖网络状况。可提前 `docker compose pull redis kafka`
