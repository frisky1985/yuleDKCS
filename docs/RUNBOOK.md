# yuleDKCS 运维手册

> ⚠️ **DEPRECATED** — 本文件已废弃，请参阅新版文档。
> **版本**: 1.0.0-rc.1 (DEPRECATED)
> **面向**: 运维人员
> **最后更新**: 2026-07-07 (标记为 DEPRECATED: 2026-07-08)
> **替代**: `docs/operations-manual.md` (新版运维手册)

---

> **注意**: 以下内容来自旧版运维手册，仅供历史参考。所有运维操作请参考
> `docs/operations-manual.md` 和 `docs/DEPLOYMENT_GUIDE.md` 中的最新信息。

---

## 1. 服务拓扑

```
Nginx (Ingress, TLS)
   └── Hub (Go) x3 (:8080) — REST API 入口
         └── gRPC
         ├── DKCS (Go) x3 (:50051)
         ├── Java Adapter CCC/ICCOA
         └── Java Adapter ICCE
DKCS ──┬── PostgreSQL (:5432)
       ├── Redis (:6379)
       ├── Kafka (:9092)
       └── Prometheus + Grafana
```

**部署模式**:
- `all-in-one`: 开发/测试
- `hub-only`: 生产（Hub 独立）
- `server-only`: 生产（DKCS 独立）

## 2. 启动 / 停止 / 重启

### Docker Compose（开发环境）
```bash
docker-compose up -d                          # 启动全部
docker-compose logs -f                        # 查看日志
docker-compose down                           # 停止全部
docker-compose restart dkcs                   # 重启单个
```

### Kubernetes（生产环境）
```bash
kubectl apply -f namespace.yaml configmap.yaml secret.yaml
kubectl apply -f postgres.yaml redis.yaml kafka.yaml
kubectl apply -f dkcs-deployment.yaml hub-deployment.yaml adapters-deployment.yaml ingress.yaml
kubectl rollout restart deployment/dkcs -n dkcs   # 重启
kubectl scale deployment dkcs --replicas=5 -n dkcs # 扩容
kubectl set image deployment/dkcs dkcs=yuledkcs/dkcs:1.0.1 -n dkcs  # 滚动更新
kubectl rollout undo deployment/dkcs -n dkcs                          # 回滚
```

### 二进制运行
```bash
# Hub
go run ./backend/cloud/hub/cmd/yuledkcs --mode=hub-only --http-addr=:8080 --jwt-secret=xxx
# DKCS
go run ./backend/cloud/hub/cmd/yuledkcs --mode=server-only --grpc-addr=:9090
# Java Adapters
cd backend/adapters && mvn spring-boot:run
```

## 3. 健康检查

| 检查项 | 方法 | 路径 | 预期返回 |
|:------|:----|:-----|:---------|
| Hub 存活 | GET | `/v1/health` | `{"status":"ok"}` |
| Hub 就绪 | GET | `/v1/ready` | `{"status":"ready"}` |
| DKCS gRPC | gRPC | `grpc.health.v1.Health/Check` | `SERVING` |

```bash
kubectl get pods -n dkcs
kubectl describe pod <pod-name> -n dkcs
kubectl port-forward svc/hub 8080:8080 -n dkcs && curl http://localhost:8080/v1/health
```

## 4. 日志查看

| 服务 | 日志方式 | 默认路径 |
|:----|:--------|:---------|
| Hub/DKCS | `go.uber.org/zap` | stdout (JSON) |
| Java Adapters | Logback | stdout + `logs/` |

```bash
# 运行时级别调整
go run ./backend/cloud/hub/cmd/yuledkcs --log-level=debug
# 可选: debug, info, warn, error, fatal

# Kubernetes 日志
kubectl logs -f deployment/dkcs -n dkcs
kubectl logs -f pod/dkcs-xxxxx -n dkcs --tail=100
kubectl logs -f deployment/hub -c hub -n dkcs
```

## 5. 常见故障排除

### Hub 端口冲突
```
Error: listen tcp :8080: bind: address already in use
→ lsof -i :8080 && kill -9 <PID>
```
默认端口: Hub HTTP=8080, DKCS gRPC=50051, Metrics=9090

### DKCS 数据库连接失败
```
ERROR: dial tcp: lookup postgres on ...: no such host
→ kubectl exec -it dkcs-xxxxx -- nslookup postgres.dkcs.svc.cluster.local
→ kubectl rollout restart deployment/dkcs -n dkcs  # 更新 Secret 后需重启
```

### gRPC 连接失败
```
rpc error: code = Unavailable desc = connection closed
→ kubectl get pods -n dkcs -l app=dkcs
→ grpcurl -plaintext dkcs:50051 grpc.health.v1.Health/Check
→ 检查 HUB 的 GRPC_DKCS_ADDR 环境变量
```

### 车端连接超时
排查顺序: 检查 TCU 网络连通 → MQTT Broker 负载 → 车端协议栈日志 → MQTT Topic ACL

## 6. 备份与恢复

### PostgreSQL
```bash
kubectl exec -it postgres-0 -n dkcs -- pg_dump -U digitalkey digitalkey_db > backup_$(date +%Y%m%d).sql
# 恢复
kubectl exec -it postgres-0 -n dkcs -- psql -U digitalkey digitalkey_db < backup_20260506.sql
# 定时备份 (crontab)
0 2 * * * kubectl exec -it postgres-0 -n dkcs -- pg_dump -U digitalkey digitalkey_db | gzip > /backup/db_$(date +\%Y\%m\%d).sql.gz
```

### Redis
```bash
redis-cli SAVE   # 手动触发 RDB 快照
cp /data/appendonly.aof /backup/   # 备份 AOF
```

## 7. 监控

| 工具 | 用途 | 端口 |
|:----|:-----|:----|
| Prometheus | 指标采集 | 9090 |
| Grafana | 可视化看板 | 3000 |
| Jaeger / OpenTelemetry | 链路追踪 | 建议使用 OpenTelemetry Collector + Jaeger UI |

> 参考 [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md) 第 5 章获取部署细节
