# yuleDKCS 运维手册

> **版本**: 1.0.0 | **面向**: 运维人员
> **最后更新**: 2026-07-08

---

## 1. 服务拓扑

```
                              ┌──────────────────────────┐
                              │  Nginx (Ingress, TLS 1.3) │
                              └────────────┬─────────────┘
                                           │
                              ┌────────────▼─────────────┐
                              │       Hub (Go) x3         │
                              │  REST API 入口 :8080     │
                              └────────────┬─────────────┘
                                           │ gRPC (mTLS)
                    ┌──────────────────────┼──────────────────────┐
                    │                      │                      │
         ┌──────────▼──────────┐ ┌─────────▼──────────┐ ┌────────▼────────┐
         │   DKCS (Go) x3     │ │ Java Adapter CCC    │ │ Java Adapter    │
         │   :50051           │ │   + ICCE + ICCOA    │ │  ICCE           │
         └──────────┬──────────┘ └────────────────────┘ └─────────────────┘
                    │
    ┌───────────────┼───────────────────────┐
    │               │                       │
┌───▼────┐  ┌──────▼──────┐  ┌────────────▼─────────┐
│PostgreSQL│ │  Redis 7+   │  │  Kafka 3.6+ (3-broker)│
│ 15+     │  │ (Cluster)   │  │  (事件总线)           │
└─────────┘  └─────────────┘  └──────────────────────┘

监控: Prometheus + Grafana | 日志: 文件 + stdout (zap JSON)
```

**部署模式**:
| 模式 | 说明 | 适用场景 |
|:-----|:-----|:---------|
| `all-in-one` | Hub + DKCS + Adapters 单体运行 | 开发/测试 |
| `hub-only` | 仅 Hub 独立运行 | 生产（Hub 集群 + 外部 DKCS） |
| `server-only` | 仅 DKCS 独立运行 | 生产（DKCS 集群 + 外部 Hub） |
| `separated` | Hub + DKCS + Adapters 分别部署 | 生产推荐 |

---

## 2. Docker 环境

### 2.1 启动/停止

```bash
# 启动全部（生产 + 集成测试依赖）
cd ~/yuleDKCS && docker compose up -d

# 仅启动基础设施（不启动业务服务）
docker compose up -d postgres redis zookeeper kafka

# 查看状态
docker compose ps

# 查看日志
docker compose logs -f hub dkcs

# 停止并清理
docker compose down

# 完全清理（包括数据卷）
docker compose down -v
```

### 2.2 服务配置

| 服务 | 镜像 | 端口 | 数据持久化 | 备注 |
|:-----|:-----|:----|:-----------|:-----|
| PostgreSQL | postgres:16-alpine | 5432 | 卷 `pgdata:/var/lib/postgresql/data` | 用户/密码: user/pass |
| Redis | redis:7-alpine | 6379 | 卷 `redis-data:/data` | 无密码 |
| Zookeeper | confluentinc/cp-zookeeper:7.6.0 | 2181 | 卷 `zk-data:/var/lib/zookeeper/data` | Kafka 协调 |
| Kafka | confluentinc/cp-kafka:7.6.0 | 9092 | 卷 `kafka-data:/var/lib/kafka/data` | 3 个预置 topic |

### 2.3 环境变量

```bash
# Redis
export REDIS_ADDR=localhost:6379
export REDIS_PASSWORD=
export REDIS_DB=0

# Kafka
export KAFKA_BROKERS=localhost:9092
export KAFKA_TOPIC_KEY_EVENTS=dkcs.key.events
export KAFKA_TOPIC_COMMANDS=dkcs.commands
export KAFKA_TOPIC_EVENTS=dkcs.events
export KAFKA_TOPIC_DLQ=dkcs.dlq

# PostgreSQL
export DB_HOST=localhost
export DB_PORT=5432
export DB_USER=user
export DB_PASSWORD=pass
export DB_NAME=yuledkcs

# Go 服务
export DKCS_LOG_LEVEL=info         # debug/info/warn/error
export DKCS_LOG_FILE=              # 留空=stdout
export DKCS_MODE=all-in-one        # all-in-one/hub-only/server-only/separated
```

---

## 3. Kubernetes 部署

### 3.1 基础设施（生产建议配置）

```yaml
# PostgreSQL: 1 Primary + N Replica (Patroni 或 Cloud Native PG Operator)
# Redis: Cluster (3 nodes minimum)
# Kafka: Strimzi Operator → 3-broker cluster
```

### 3.2 服务部署文件 (Deployment 示例)

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: yuledkcs-hub
spec:
  replicas: 3
  selector:
    matchLabels:
      app: hub
  template:
    spec:
      containers:
      - name: hub
        image: yuledkcs/hub:1.0.0
        ports:
        - containerPort: 8080
        env:
        - name: DKCS_MODE
          value: "hub-only"
        resources:
          requests:
            cpu: "500m"
            memory: "512Mi"
          limits:
            cpu: "2"
            memory: "1Gi"
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 10
          periodSeconds: 15
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 10
---
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: yuledkcs-hub-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: yuledkcs-hub
  minReplicas: 3
  maxReplicas: 10
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
```

### 3.3 服务发现与入口

```yaml
apiVersion: v1
kind: Service
metadata:
  name: hub-service
spec:
  selector:
    app: hub
  ports:
  - port: 8080
    targetPort: 8080
---
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: yuledkcs-ingress
  annotations:
    nginx.ingress.kubernetes.io/ssl-redirect: "true"
spec:
  ingressClassName: nginx
  tls:
  - hosts:
    - api.yuledkcs.com
    secretName: yuledkcs-tls
  rules:
  - host: api.yuledkcs.com
    http:
      paths:
      - path: /v1
        pathType: Prefix
        backend:
          service:
            name: hub-service
            port:
              number: 8080
```

---

## 4. 监控与告警

### 4.1 Prometheus 指标

Hub/DKCS 暴露 Prometheus 指标端口（默认 `:9090/metrics`）:

```yaml
# Prometheus ServiceMonitor
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: yuledkcs-monitor
spec:
  selector:
    matchLabels:
      app: yuledkcs
  endpoints:
  - port: metrics
    path: /actuator/prometheus   # Java Adapters
    interval: 15s
  - port: metrics
    path: /metrics               # Go 服务
    interval: 15s
```

### 4.2 关键告警规则

| 告警名 | 条件 | 严重度 | 说明 |
|:-------|:-----|:------:|:-----|
| HubDown | `up{job="hub"} == 0` | P0 | Hub 实例宕机 |
| DKCSDown | `up{job="dkcs"} == 0` | P0 | DKCS 实例宕机 |
| HighLatency | `http_request_duration_seconds{p95} > 2` | P1 | API 响应超时 |
| KafkaLag | `kafka_consumer_lag > 1000` | P1 | 消息积压 |
| PGConnection | `pg_connections > 100` | P1 | 数据库连接池满 |
| CoverageDrop | 覆盖率环比下降 > 5% | P2 | 代码质量下降 |

### 4.3 Grafana 仪表板

推荐配置面板:
1. **服务状态总览**: Hub/DKCS/Adapter 实例健康状态
2. **API 请求量/延迟/P99 分布**: REST API 性能
3. **钥匙操作统计**: 绑定/分享/吊销/解锁操作计数
4. **Kafka 消费延迟**: 各 topic 的 consumer lag
5. **资源使用率**: CPU/Memory/网络 IO
6. **错误率追踪**: 5xx 响应 + 异常日志频率

---

## 5. 日志管理

### 5.1 日志格式 (Go — zap JSON)

```json
{"level":"info","ts":"2026-07-07T10:00:00.000+0800","caller":"hub/main.go:42",
 "msg":"Hub started","mode":"hub-only","port":8080}
```

### 5.2 日志级别

| 级别 | 生产 | 测试 | 建议 |
|:-----|:----:|:----:|:-----|
| debug | ❌ | ✅ | 开发环境启用 |
| info | ✅ | ✅ | 默认级别 |
| warn | ✅ | ✅ | 需关注的事件 |
| error | ✅ | ✅ | 需要人为干预 |

### 5.3 日志采集

```bash
# 开发环境 — 文件输出
./hub --log-file=/var/log/yuledkcs/hub.log --log-level=info

# 生产环境 (stdout + 日志采集 Agent)
# - stdout 日志由 Fluentd/Logstash 采集
# - 输出到 Elasticsearch / Loki
# - Grafana 或 Kibana 可视化

# 查看指定服务日志
docker compose logs -f --tail=100 hub
docker compose logs -f --tail=100 dkcs

# 搜索关键事件
docker compose logs hub 2>&1 | grep -E "error|warn|panic"
```

---

## 6. 健康检查

### 6.1 端到端健康检查

```bash
# Hub API 健康检查
curl -s http://localhost:8080/health | jq .
# 预期: {"status":"UP","components":{"db":"UP","redis":"UP","kafka":"DEGRADED"}}

# gRPC 健康检查 (DKCS)
grpcurl -plaintext localhost:50051 grpc.health.v1.Health/Check

# 数据库连接检查
docker compose exec postgres pg_isready -U user -d yuledkcs

# Redis 检查
docker compose exec redis redis-cli ping
# 预期: PONG

# Kafka 检查
docker compose exec kafka kafka-broker-api-versions \
  --bootstrap-server localhost:9092 | head -5

# 预置 Topic 确认
docker compose exec kafka kafka-topics \
  --bootstrap-server localhost:9092 --list
# 预期: dkcs.key.events, dkcs.commands, dkcs.events, dkcs.dlq
```

### 6.2 诊断脚本 (verify-hub.sh)

```bash
#!/bin/bash
# ~/yuleDKCS/scripts/verify-hub.sh
HUB_URL="${HUB_URL:-http://localhost:8080}"

echo "=== yuleDKCS Health Check ==="

# Hub API
echo -n "Hub API: "
curl -sf "$HUB_URL/health" > /dev/null && echo "✅" || echo "❌"

# gRPC (if grpcurl available)
if command -v grpcurl &>/dev/null; then
  echo -n "DKCS gRPC: "
  grpcurl -plaintext localhost:50051 grpc.health.v1.Health/Check > /dev/null 2>&1 \
    && echo "✅" || echo "❌"
fi

# Redis
echo -n "Redis: "
docker compose exec redis redis-cli ping 2>/dev/null | grep -q PONG \
  && echo "✅" || echo "❌"

# PostgreSQL
echo -n "PostgreSQL: "
docker compose exec postgres pg_isready -q -U user -d yuledkcs 2>/dev/null \
  && echo "✅" || echo "❌"

# Kafka
echo -n "Kafka: "
docker compose exec kafka kafka-topics --bootstrap-server localhost:9092 --list \
  > /dev/null 2>&1 && echo "✅" || echo "❌"

echo "=========================="
```

---

## 7. 备份与恢复

### 7.1 数据库备份

```bash
# 每日备份 (crontab)
0 2 * * * docker compose exec -T postgres pg_dump -U user yuledkcs \
  | gzip > /backups/yuledkcs-$(date +\%Y\%m\%d).sql.gz

# 保留最近 30 天
0 3 * * * find /backups/ -name "yuledkcs-*.sql.gz" -mtime +30 -delete
```

### 7.2 数据库恢复

```bash
gunzip < /backups/yuledkcs-20260707.sql.gz \
  | docker compose exec -T postgres psql -U user yuledkcs
```

### 7.3 Kafka 数据持久化

Kafka 消息已配置数据卷映射，broker 重启后消息不丢失。
> Topic 配置: `log.retention.hours=168` (7天), `log.segment.bytes=1GB`

---

## 8. 故障排查

### 8.1 服务无法启动

```bash
# 检查日志
docker compose logs hub | tail -50

# 检查端口冲突
lsof -i :8080 -i :50051

# 检查配置
docker compose config
```

### 8.2 数据库连接失败

```bash
# 检查数据库运行状态
docker compose ps postgres

# 测试连接
docker compose exec postgres psql -U user -d yuledkcs -c "SELECT 1;"

# 检查连接数
docker compose exec postgres psql -U user -d yuledkcs \
  -c "SELECT count(*) FROM pg_stat_activity;"
```

### 8.3 Kafka 消息不消费

```bash
# 检查 consumer group 状态
docker compose exec kafka kafka-consumer-groups \
  --bootstrap-server localhost:9092 \
  --group dkcs-group --describe

# 查看 topic 最新消息
docker compose exec kafka kafka-console-consumer \
  --bootstrap-server localhost:9092 \
  --topic dkcs.key.events --from-beginning --max-messages 5
```

### 8.4 证书问题

```bash
# 检查 TLS 证书有效期
echo | openssl s_client -connect api.yuledkcs.com:443 2>/dev/null \
  | openssl x509 -noout -dates

# gRPC mTLS 证书
openssl x509 -in /etc/yuledkcs/certs/client.crt -noout -text
```

---

## 9. 性能基准

| 指标 | 目标值 | 报警阈值 |
|:-----|:------:|:--------:|
| API P50 延迟 | < 200ms | > 500ms |
| API P99 延迟 | < 1s | > 2s |
| Hub 吞吐量 | > 1000 req/s | < 500 req/s |
| DKCS gRPC 延迟 | < 50ms | > 200ms |
| 数据库连接池 | < 50 | > 100 |
| Kafka 消费延迟 | < 100ms | > 5s |
| 解锁响应时间 | < 500ms | > 1s |

---

## 10. 相关文档

| 文档 | 路径 |
|:-----|:-----|
| 部署指南 | `docs/DEPLOYMENT_GUIDE.md` |
| API 参考 | `docs/API_REFERENCE.md` |
| 集成指南 | `docs/integration-guide.md` |
| 安全指南 | `docs/SECURITY_GUIDE.md` |
| Docker Compose | `docker-compose.yml` |
| 诊断脚本 | `scripts/verify-hub.sh` |
| 启动脚本 | `scripts/start-prod.sh` |
| 初始化脚本 | `scripts/init-db.sh` |
