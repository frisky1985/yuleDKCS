# 🛠 yuleDKCS 运维手册 (Operations Runbook)

> 版本: 1.0.0  
> 更新日期: 2026-07-27  
> 适用版本: v2.1.x
>
> **数据库选型**: 本手册已统一为 PostgreSQL 15+，详见 [DATABASE-DECISION.md](./DATABASE-DECISION.md)

---

## 目录

1. [系统架构概述](#1-系统架构概述)
2. [部署架构](#2-部署架构)
3. [环境要求](#3-环境要求)
4. [启动/停止/重启](#4-启动停止重启)
5. [日志查看](#5-日志查看)
6. [健康检查端点](#6-健康检查端点)
7. [常见故障排查](#7-常见故障排查)
8. [备份与恢复策略](#8-备份与恢复策略)
9. [监控指标建议](#9-监控指标建议)
10. [扩容策略](#10-扩容策略)

---

## 1. 系统架构概述

yuleDKCS 是一套完整的数字钥匙解决方案，采用三层云端架构：

```
┌─────────────────┐     ┌──────────────────┐     ┌──────────────────────┐
│   手机端 (App)   │◄───►│  云 端 服 务 平 台  │◄───►│    车端 TCU (C)      │
│  iOS / Android  │     │                  │     │  ICCE/CCC/ICCOA      │
│  BLE/UWB/NFC    │     │ Hub + DKCS + Adapters│  │  BLE/UWB/NFC/SE050   │
└─────────────────┘     └──────────────────┘     └──────────────────────┘
```

### 云端核心组件

| 组件 | 技术栈 | 角色 |
|------|--------|------|
| **Hub** | Go / gRPC | API 网关、协议转接、速率限制、JWT 鉴权 |
| **DKCS** | Go / PostgreSQL | 核心业务逻辑、密钥管理、车控指令、事件流处理 |
| **Adapters** | Java / Spring Boot | 协议适配（CCC/ICCOA/ICCE）、TSP 对接 |

### 基础设施中间件

| 组件 | 用途 | 部署方式 |
|------|------|----------|
| PostgreSQL 15+ | 持久化存储（密钥记录、设备信息、操作日志） | StatefulSet / Docker |
| Redis 7 | 缓存层（会话、令牌、速率限制计数器） | StatefulSet / Docker |
| Kafka 7.5 | 消息队列（密钥事件、车控指令、遥测数据） | StatefulSet / Docker |
| EMQX 5.5 | MQTT Broker（TCU 车端通信） | Deployment / Docker |
| Prometheus | 指标采集与告警 | Deployment / Docker |
| Grafana | 监控仪表盘 | Deployment / Docker |

> 详细系统架构请参阅 [SYSTEM_ARCHITECTURE.md](./SYSTEM_ARCHITECTURE.md)

---

## 2. 部署架构

### 2.1 Kubernetes 部署（生产推荐）

命名空间: `dkcs-prod`

```
kustomization.yaml
├── namespace.yaml              # dkcs-prod 命名空间
├── secrets.yaml                # 敏感凭证（mysql/redis/jwt）
├── pdb.yaml                    # Pod 分布预算
├── ingress.yaml                # TLS Ingress (api.digitalkey.example.com)
├── hub/
│   ├── configmap.yaml          # Hub 环境配置
│   ├── service.yaml            # ClusterIP: 8080(REST)/9090(gRPC)/9093(metrics)
│   ├── deployment.yaml         # 3 副本, 滚动更新
│   └── hpa.yaml                # HPA: 3-20 副本, CPU 70% / Mem 80%
├── dkcs/
│   ├── configmap.yaml          # DKCS 环境配置
│   ├── service.yaml            # ClusterIP: 9091(gRPC+metrics)
│   ├── deployment.yaml         # 3 副本, 滚动更新
│   └── hpa.yaml                # HPA: 3-20 副本, CPU 70% / Mem 80%
├── postgresql/statefulset.yaml # PostgreSQL 15+ 单节点
├── redis/statefulset.yaml      # Redis 7 单节点
├── kafka/kafka.yaml            # Kafka 7.5 + Zookeeper 单节点
├── emqx.yaml                   # EMQX 5.5 MQTT Broker
└── monitoring/
    └── prometheus-config.yaml  # Prometheus 配置
```

##### 副本数配置

| 组件 | 生产 (dkcs-prod) | 预发 (dkcs-staging) |
|------|------------------|---------------------|
| Hub | 3 副本 (HPA 3-20) | 1 副本 (HPA 1-3) |
| DKCS | 3 副本 (HPA 3-20) | 1 副本 (HPA 1-3) |
| PostgreSQL | 1 单节点 | 1 单节点 |
| Redis | 1 单节点 | 1 单节点 |
| Kafka | 1 单节点 | 1 单节点 |

##### 资源配额

| 组件 | Requests | Limits |
|------|----------|--------|
| Hub | 500m CPU / 512Mi Mem | 1000m CPU / 1Gi Mem |
| DKCS | 500m CPU / 512Mi Mem | 1000m CPU / 1Gi Mem |

预发环境资源减半：200m CPU / 256Mi Mem (requests), 500m CPU / 512Mi Mem (limits)。

### 2.2 Docker Compose 部署（开发/测试）

文件: `backend/cloud/deploy/docker-compose.yml`

启动所有服务（包括中间件）：
```bash
cd backend/cloud/deploy
cp .env.example .env   # 编辑密码等敏感信息
docker compose up -d
```

仅启动监控栈：
```bash
docker compose -f docker-compose.monitoring.yml up -d
```

### 2.3 Helm 部署

Helm Chart 位于 `backend/cloud/deploy/helm/dkcs/`，支持 values.yaml 自定义配置：
```bash
helm install yule-dkcs ./backend/cloud/deploy/helm/dkcs \
  --namespace dkcs-prod \
  --create-namespace \
  -f ./backend/cloud/deploy/helm/dkcs/values.yaml
```

### 2.4 部署模式

yuleDKCS 支持三种运行模式，通过 `--mode` 参数控制：

```bash
# 1. 一体化模式（默认）：Hub + DKCS 同进程
go run ./backend/cloud/hub/cmd/yuledkcs \
  --mode=all-in-one \
  --http-addr=:8080 \
  --grpc-addr=:9090 \
  --jwt-secret=<SECRET>

# 2. Hub 仅编排层：通过 gRPC 连车厂 DK Server
go run ./backend/cloud/hub/cmd/yuledkcs \
  --mode=hub-only \
  --http-addr=:8080 \
  --jwt-secret=<SECRET>

# 3. DK Server 仅密钥材料层：接受 Hub 的 gRPC 请求
go run ./backend/cloud/hub/cmd/yuledkcs \
  --mode=server-only \
  --grpc-addr=:9090
```

---

## 3. 环境要求

### 3.1 操作系统

- **Linux** (推荐 Ubuntu 22.04 LTS / CentOS 9)
- **macOS** (开发测试, macOS 14+)
- 内核版本 ≥ 5.10

### 3.2 运行时依赖

| 依赖 | 版本要求 | 用途 |
|------|----------|------|
| Go | ≥ 1.22 | Hub / DKCS 服务编译与运行 |
| Java | ≥ 17 | Adapters 协议适配层 |
| Maven | ≥ 3.9 | Java Adapters 构建 |
| Docker | ≥ 24.0 | 容器化部署 |
| Kubernetes | ≥ 1.28 | 生产集群 |
| Helm | ≥ 3.14 | Helm Chart 部署 |

### 3.3 中间件版本

| 中间件 | 版本 | 关键配置 |
|--------|------|----------|
| MySQL | 8.0 | utf8mb4, innodb_buffer_pool_size=1G, max_connections=500 |
| Redis | 7-alpine | maxmemory=512mb, maxmemory-policy=allkeys-lru |
| Kafka | 7.5.0 (Confluent) | log_retention_hours=168, auto_create_topics=false |
| EMQX | 5.5 | MQTT TCP 1883, WebSocket 8083, Dashboard 18083 |
| Prometheus | 最新 | scrape_interval=15s, evaluation_interval=15s |
| Grafana | 最新 | 预配 Prometheus 数据源 |

### 3.4 网络端口

| 端口 | 组件 | 协议 | 说明 |
|------|------|------|------|
| 8080 | Hub | HTTP/REST | 外部 API 入口 |
| 9090 | Hub | gRPC | Hub 内部 gRPC |
| 9091 | DKCS | gRPC | DKCS 核心业务 gRPC (也承载 metrics) |
| 9093 | Hub | HTTP | Prometheus metrics |
| 3306 | MySQL | TCP | 数据库 |
| 6379 | Redis | TCP | 缓存 |
| 9092 | Kafka | TCP | 消息队列 |
| 1883 | EMQX | MQTT | TCU 车端连接 |
| 18083 | EMQX | HTTP | MQTT Dashboard |

---

## 4. 启动/停止/重启

### 4.1 K8s 部署

```bash
# 部署全栈
kubectl apply -k backend/cloud/deploy/k8s/

# 仅部署服务层（假设中间件已部署）
kubectl apply -k backend/cloud/deploy/k8s/ --selector='app.kubernetes.io/component in (gateway,control-plane)'

# 滚动重启 Hub
kubectl rollout restart deployment/hub -n dkcs-prod

# 滚动重启 DKCS
kubectl rollout restart deployment/dkcs -n dkcs-prod

# 查看部署状态
kubectl rollout status deployment/hub -n dkcs-prod
kubectl rollout status deployment/dkcs -n dkcs-prod

# 停止服务
kubectl scale deployment/hub -n dkcs-prod --replicas=0
kubectl scale deployment/dkcs -n dkcs-prod --replicas=0

# 彻底删除
kubectl delete -k backend/cloud/deploy/k8s/
```

### 4.2 Docker Compose 部署

```bash
cd backend/cloud/deploy

# 启动全部服务
docker compose up -d

# 启动单个组件
docker compose up -d hub
docker compose up -d dkcs

# 重启单个服务
docker compose restart hub
docker compose restart dkcs

# 查看日志
docker compose logs -f hub
docker compose logs -f dkcs

# 停止全部
docker compose down

# 停止并清理数据卷（谨慎使用）
docker compose down -v
```

### 4.3 原生启动（开发调试）

```bash
# Hub（一体化模式）
cd backend/cloud/hub
go run ./cmd/yuledkcs --mode=all-in-one --http-addr=:8080 --grpc-addr=:9090 --jwt-secret=dev-secret

# Java Adapters
cd backend/adapters
mvn spring-boot:run
```

---

## 5. 日志查看

### 5.1 日志格式

所有 Go 服务采用 **JSON 日志格式**，默认输出到 `stdout`。

日志字段说明：
```json
{
  "level": "info",            // 日志级别: debug | info | warn | error | fatal
  "time": "2026-07-27T10:00:00Z",  // ISO 8601 时间戳
  "caller": "hub/server.go:42",   // 源文件及行号
  "message": "...",           // 日志消息
  "request_id": "xxx",        // 请求追踪 ID（可选）
  "duration_ms": 123,         // 请求耗时（可选）
  "error": "..."              // 错误信息（可选）
}
```

### 5.2 组件日志路径

| 组件 | K8s 查看方式 | Docker Compose 查看方式 |
|------|-------------|------------------------|
| Hub | `kubectl logs -n dkcs-prod -l app=hub -f` | `docker compose logs -f hub` |
| DKCS | `kubectl logs -n dkcs-prod -l app=dkcs -f` | `docker compose logs -f dkcs` |
| MySQL | `kubectl logs -n dkcs-prod statefulset/mysql` | `docker compose logs -f mysql` |
| Redis | `kubectl logs -n dkcs-prod statefulset/redis` | `docker compose logs -f redis` |
| Kafka | `kubectl logs -n dkcs-prod statefulset/kafka` | `docker compose logs -f kafka` |
| EMQX | `kubectl logs -n dkcs-prod deployment/emqx` | `docker compose logs -f emqx` |

### 5.3 日志收集建议

- **生产环境**: 使用 Fluentd/Logstash 采集 JSON 日志并写入 Elasticsearch
- **日志保留**: 建议保留 30 天，关键审计日志保留 180 天
- **日志级别**: 生产环境推荐 `info`；调试时临时改为 `debug`

```bash
# 临时调整为 debug 级别（零停机）
kubectl set env deployment/hub -n dkcs-prod LOG_LEVEL=debug
```

---

## 6. 健康检查端点

### 6.1 Hub 健康检查

| 路径 | 方法 | 说明 | 预期返回 |
|------|------|------|----------|
| `/healthz` | GET | 整体健康状态 | `200 OK` (Ingress 直接代理) |
| `/metrics` | GET | Prometheus 指标 (port 9093) | `200` |

### 6.2 Kubernetes 探针配置

**Hub (port 9090 gRPC)**:
- **Liveness Probe**: gRPC port 9090, initialDelaySeconds=15, periodSeconds=20
- **Readiness Probe**: gRPC port 9090, initialDelaySeconds=5, periodSeconds=10
- **Startup Probe**: TCP port 9090, initialDelaySeconds=3, periodSeconds=5, failureThreshold=30

**DKCS (port 9091 gRPC)**:
- **Liveness Probe**: gRPC port 9091, initialDelaySeconds=15, periodSeconds=20
- **Readiness Probe**: gRPC port 9091, initialDelaySeconds=5, periodSeconds=10
- **Startup Probe**: TCP port 9091, initialDelaySeconds=3, periodSeconds=5, failureThreshold=30

### 6.3 手动健康检查

```bash
# Hub REST
curl -s -o /dev/null -w "%{http_code}" https://api.digitalkey.example.com/healthz

# Hub gRPC (需 grpcurl)
grpcurl -plaintext localhost:9090 grpc.health.v1.Health/Check

# DKCS gRPC
grpcurl -plaintext localhost:9091 grpc.health.v1.Health/Check

# MySQL
mysqladmin ping -h mysql -u dk_service -p

# Redis
redis-cli -h redis -a <password> ping

# EMQX
curl -s http://localhost:18083/api/v5/status
```

---

## 7. 常见故障排查

### 7.1 服务无法启动

**现象**: Pod 反复 CrashLoopBackOff

**排查步骤**:
```bash
# 查看 Pod 状态
kubectl describe pod -n dkcs-prod -l app=hub

# 查看上次退出日志
kubectl logs -n dkcs-prod -l app=hub --previous

# 确认依赖中间件就绪
kubectl get pods -n dkcs-prod | grep -E 'mysql|redis|kafka'
```

**常见原因**:
1. MySQL/Redis 不可达 → 检查 ConfigMap 中的 DB_HOST/REDIS_ADDR
2. JWT Secret 未配置 → 检查 Secrets 中的 jwt-secret
3. 数据库 Schema 未初始化 → 执行 `mysql -u dk_service < backend/db/schema.sql`

### 7.2 gRPC 连接失败

**现象**: Hub 日志中出现 `connection refused` 或 `transport: authentication handshake failed`

**排查步骤**:
```bash
# 检查 gRPC 端口是否监听
kubectl exec -n dkcs-prod deployment/hub -- netstat -tlnp | grep 9090

# 检查服务 DNS 解析
kubectl exec -n dkcs-prod deployment/hub -- nslookup dkcs

# 检查 gRPC 服务注册
kubectl exec -n dkcs-prod deployment/hub -- grpcurl -plaintext localhost:9090 list
```

### 7.3 高错误率告警

**现象**: Prometheus 告警 `ServiceHighErrorRate` (5% 错误率)

**排查步骤**:
```bash
# 查看最近的错误日志
kubectl logs -n dkcs-prod -l app=hub --tail=100 | grep '"level":"error"'

# 检查速率限制是否触发
kubectl logs -n dkcs-prod -l app=hub --tail=100 | grep -i 'rate.limit\|429'

# 检查数据库连接池
kubectl exec -n dkcs-prod deployment/hub -- wget -qO- http://localhost:9093/metrics | grep -i db_conn
```

### 7.4 高延迟

**现象**: P95 延迟 > 2s

**排查步骤**:
1. 检查 CPU/Memory 是否达到 Limit
2. 检查数据库慢查询
3. 检查 Kafka 消费者 Lag
4. 检查网络延迟

```bash
# 查看 Pod 资源使用
kubectl top pod -n dkcs-prod -l app=hub

# 检查 MySQL 慢查询
kubectl exec -n dkcs-prod statefulset/mysql -- mysql -e "SHOW FULL PROCESSLIST;"

# 检查 Kafka 消费延迟（需 kafka 客户端）
kubectl exec -n dkcs-prod statefulset/kafka -- \
  kafka-consumer-groups --bootstrap-server localhost:9092 --group dkcs-consumer --describe
```

### 7.5 MQTT 通信异常

**现象**: 车端 TCU 无法连接到云端

**排查步骤**:
```bash
# 检查 EMQX 状态
curl -s http://emqx:18083/api/v5/status

# 查看 EMQX 连接数
curl -s http://emqx:18083/api/v5/connections | jq '.count'

# 检查 DKCS 中 MQTT 客户端日志
kubectl logs -n dkcs-prod -l app=dkcs --tail=50 | grep -i mqtt
```

### 7.6 部署命令速查

```bash
# 查看所有 Pod 状态
kubectl get pods -n dkcs-prod -o wide

# 查看所有 Service
kubectl get svc -n dkcs-prod

# 查看所有 ConfigMap
kubectl get configmap -n dkcs-prod

# 查看 HPA 状态
kubectl get hpa -n dkcs-prod

# 查看事件
kubectl get events -n dkcs-prod --sort-by='.lastTimestamp'

# 端口转发调试
kubectl port-forward -n dkcs-prod deployment/hub 8080:8080
```

---

## 8. 备份与恢复策略

### 8.1 数据库备份

```bash
# MySQL 全量备份（生产建议每小时）
kubectl exec -n dkcs-prod statefulset/mysql -- \
  mysqldump -u root -p$MYSQL_ROOT_PASSWORD --all-databases \
  --single-transaction --routines --triggers \
  | gzip > /backup/dkcs-mysql-$(date +%Y%m%d-%H%M%S).sql.gz

# 仅备份 digital_key 数据库
kubectl exec -n dkcs-prod statefulset/mysql -- \
  mysqldump -u dk_service -p$MYSQL_PASSWORD digital_key \
  --single-transaction \
  | gzip > /backup/dk-digital_key-$(date +%Y%m%d-%H%M%S).sql.gz
```

### 8.2 Redis 备份

```bash
# 触发 Redis 持久化
kubectl exec -n dkcs-prod statefulset/redis -- redis-cli -a $REDIS_PASSWORD BGSAVE

# 复制 AOF/RDB 文件
kubectl cp dkcs-prod/redis-0:/data/dump.rdb /backup/dk-redis-$(date +%Y%m%d).rdb
```

### 8.3 K8s 资源清单备份

```bash
# 备份所有 K8s 资源定义
kubectl get all,configmap,secret,ingress,hpa,pdb -n dkcs-prod -o yaml \
  > /backup/dk-k8s-manifest-$(date +%Y%m%d).yaml
```

### 8.4 备份策略

| 数据类型 | 备份频率 | 保留策略 | 存储位置 |
|----------|----------|----------|----------|
| MySQL 全量 | 每小时 | 7 天 | 本地 NFS + 远程对象存储 |
| MySQL 每日 | 每天 02:00 | 30 天 | 远程对象存储 |
| MySQL 每周 | 每周日 02:00 | 90 天 | 远程对象存储 |
| Redis RDB | 每小时 | 24 小时 | 本地 NFS |
| K8s 资源 | 每天 | 30 天 | Git 仓库 + 远程存储 |

### 8.5 恢复流程

```bash
# MySQL 恢复
kubectl exec -i -n dkcs-prod statefulset/mysql -- \
  mysql -u root -p$MYSQL_ROOT_PASSWORD < dkcs-mysql-backup.sql

# Redis 恢复
kubectl cp dk-redis-backup.rdb dkcs-prod/redis-0:/data/dump.rdb
kubectl exec -n dkcs-prod statefulset/redis -- redis-cli -a $REDIS_PASSWORD DEBUG LOAD /data/dump.rdb
```

---

## 9. 监控指标建议

### 9.1 Prometheus 指标

Hub 和 DKCS 均在 `/metrics` 端点暴露标准 Go 运行时指标和业务指标。

| 指标 | 类型 | 说明 | 所属 |
|------|------|------|------|
| `go_goroutines` | Gauge | 当前 goroutine 数量 | Hub / DKCS |
| `go_memstats_alloc_bytes` | Gauge | 堆内存使用量 | Hub / DKCS |
| `grpc_server_handling_seconds` | Histogram | gRPC 请求延迟分布 | Hub / DKCS |
| `grpc_server_handled_total` | Counter | gRPC 请求计数（按状态码） | Hub / DKCS |
| `http_request_duration_seconds` | Histogram | REST 请求延迟分布 | Hub |
| `http_requests_total` | Counter | REST 请求计数 | Hub |
| `db_connections_open` | Gauge | 数据库当前连接数 | Hub / DKCS |
| `db_query_duration_seconds` | Histogram | 数据库查询延迟 | Hub / DKCS |

### 9.2 预设告警规则

Prometheus 告警规则文件：`backend/cloud/deploy/monitoring/rules/service-alerts.yml`

| 告警名称 | 条件 | 严重程度 | 说明 |
|----------|------|----------|------|
| `ServiceHighErrorRate` | 5分钟错误率 > 5% | Critical | 服务错误率过高 |
| `ServiceHighLatency` | P95 延迟 > 2s | Warning | 请求延迟过高 |
| `ServiceDown` | `up == 0` 持续 1 分钟 | Critical | 服务实例宕机 |
| `HighGoroutineCount` | goroutine > 500 | Warning | 协程数异常增长 |

### 9.3 Grafana 仪表盘

Grafana 预配仪表盘位于 `backend/cloud/deploy/monitoring/grafana-provisioning/`：

- **数据源**: Prometheus（自动预配）
- **仪表盘**: `dkcs.yml` 自动加载 Grafana Dashboard JSON（文件: `grafana-dashboard.json`）

### 9.4 推荐的 Grafana Dashboard 面板

| 面板 | 指标 |
|------|------|
| 服务状态 | `up{job=~"hub\|dkcs"}` |
| 请求 QPS | `rate(grpc_server_handled_total[5m])` |
| 延迟 P95/P99 | `histogram_quantile(0.95, rate(grpc_server_handling_seconds_bucket[5m]))` |
| 错误率 | `rate(grpc_server_handled_total{grpc_code=~"Internal\|Unavailable"}[5m])` |
| 资源使用 | `process_cpu_seconds_total`, `go_memstats_alloc_bytes` |
| Go GC 暂停 | `go_gc_duration_seconds` |
| DB 连接池 | `db_connections_open` |
| 数据库延迟 | `histogram_quantile(0.95, rate(db_query_duration_seconds_bucket[5m]))` |

### 9.5 基础设施监控

| 目标 | Prometheus Job | Exporter |
|------|---------------|----------|
| MySQL | `mysql` | mysql-exporter:9104 |
| Redis | `redis` | redis-exporter:9121 |
| Kafka | `kafka` | kafka-exporter:9308 |

---

## 10. 扩容策略

### 10.1 水平自动扩缩容 (HPA)

Hub 和 DKCS 均已配置 HorizontalPodAutoscaler：

| 配置项 | Hub | DKCS |
|--------|-----|------|
| 最小副本 | 3 | 3 |
| 最大副本 | 20 | 20 |
| CPU 触发 | 70% | 70% |
| 内存触发 | 80% | 80% |
| 扩容稳定窗口 | 60s | 60s |
| 缩容稳定窗口 | 300s | 300s |

### 10.2 扩容策略建议

#### 垂直扩容 (Vertical Scaling)
- **CPU 瓶颈**: 将 CPU limit 从 1000m 提升至 2000m
- **内存瓶颈**: 将 Memory limit 从 1Gi 提升至 2Gi
- **DB 连接池**: 增加 `DB_MAX_OPEN_CONNS`（当前 100）

#### 水平扩容 (Horizontal Scaling)
- **QPS 增长**: HPA 自动触发，无需人工干预
- **峰值预期**: 建议在大型活动前将 minReplicas 提前设为 5-10
- **预热**: 使用 HPA 的 `scaleUp.policies` 支持快速扩容（100%/15s）

### 10.3 数据层扩容

| 组件 | 当前 | 扩容方案 |
|------|------|----------|
| MySQL | 单节点 | 升级为 RDS/Aurora 主从或集群 |
| Redis | 单节点 | 升级为 Redis Sentinel 或 Redis Cluster |
| Kafka | 单节点 | 增加 Broker 节点，增加分区数 |
| EMQX | 单节点 | 升级为 EMQX 集群 |

### 10.4 扩容前检查清单

- [ ] 确认 Prometheus 指标采集正常
- [ ] 确认 HPA 工作正常: `kubectl get hpa -n dkcs-prod`
- [ ] 确认数据库连接池有足够余量
- [ ] 确认 Kafka 分区足够（建议 3-6 分区/主题）
- [ ] 确认下游 TSP 系统能承受对应流量
- [ ] 通知相关团队扩容计划

---

## 附录

### A. 配置文件参考

| 文件 | 说明 |
|------|------|
| `backend/cloud/deploy/.env.example` | Docker Compose 环境变量模板 |
| `backend/cloud/deploy/k8s/hub/configmap.yaml` | Hub 服务配置 |
| `backend/cloud/deploy/k8s/dkcs/configmap.yaml` | DKCS 服务配置 |
| `backend/cloud/deploy/k8s/secrets.yaml` | 敏感凭证配置 |
| `backend/cloud/deploy/k8s/ingress.yaml` | 入口流量配置 |
| `backend/cloud/deploy/monitoring/prometheus.yml` | Prometheus 采集配置 |
| `backend/cloud/deploy/monitoring/rules/service-alerts.yml` | 告警规则 |
| `backend/cloud/deploy/helm/dkcs/values.yaml` | Helm Chart 配置 |

### B. 快速诊断命令

```bash
# 一键诊断脚本
echo "=== Pod 状态 ==="
kubectl get pods -n dkcs-prod -o wide
echo "=== 服务状态 ==="
kubectl get svc -n dkcs-prod
echo "=== HPA 状态 ==="
kubectl get hpa -n dkcs-prod
echo "=== 最近事件 ==="
kubectl get events -n dkcs-prod --sort-by='.lastTimestamp' | tail -20
echo "=== 资源使用 ==="
kubectl top pod -n dkcs-prod
```
