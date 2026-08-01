# yuleDKCS 部署文档（kustomize 官方路径）

> 本项目部署走 **kustomize**（本目录），为官方推荐路径。
> Helm 为历史保留备选，见 `backend/cloud/deploy/helm/README.md`。

## 目录结构

```
k8s/
├── kustomization.yaml          # 入口：namespace/labels/resources/images
├── namespace.yaml              # dkcs-prod
├── secrets.yaml                # 模板 Secret（勿提交真实凭据）
├── pdb.yaml / ingress.yaml
├── hub/                        # Hub 服务 (configmap/service/deployment/hpa)
├── dkcs/                       # DKCS 服务 (configmap/service/deployment/hpa)
├── postgres/
│   ├── statefulset.yaml        # PostgreSQL 16 单实例 StatefulSet + headless Service
│   └── exporter.yaml           # postgres-exporter Deployment + Service (:9187)
├── redis/statefulset.yaml      # Redis
├── kafka/kafka.yaml            # Kafka
├── emqx.yaml                   # EMQX
└── monitoring/
    ├── prometheus-config.yaml  # Prometheus 抓取配置 + 告警规则
    └── grafana-dashboard.json
```

## 部署

```bash
# 1. 准备 Secret（生产用外部 Secret 方案；此处为模板示例）
kubectl apply -k backend/cloud/deploy/k8s/ \
  --dry-run=client -o yaml | grep -A2 'postgres-password'   # 确认占位值需替换

# 2. 应用全部资源（含 postgres-exporter）
kubectl apply -k backend/cloud/deploy/k8s/

# 3. 确认 exporter 就绪
kubectl -n dkcs-prod get deploy,svc -l app=postgres-exporter
kubectl -n dkcs-prod rollout status deploy/postgres-exporter --timeout=120s
```

## postgres-exporter 说明

| 项 | 值 |
|:---|:---|
| 镜像 | `prometheuscommunity/postgres-exporter:v0.15.0` |
| Service | `postgres-exporter` : `9187/metrics`（namespace `dkcs-prod`） |
| 连接串 | `postgresql://yuledkcs:$(POSTGRES_PASSWORD)@postgres:5432/yuledkcs?sslmode=disable` |
| 密码来源 | Secret `dkcs-secrets` key `postgres-password`（与 StatefulSet 同一密钥，`$(...)` 由 K8s 容器内展开） |
| 抓取配置 | `monitoring/prometheus-config.yaml` job `postgres-exporter` → 静态 target `postgres-exporter:9187` |

- 独立 Deployment（非 sidecar）：与 postgres StatefulSet 解耦，exporter 重启不影响数据库。
- 不修改 `postgres/statefulset.yaml` 本身；exporter 仅以只读连接采集指标。

## 验证 scrape 目标

```bash
# 1. exporter 自检（进 Pod 或 port-forward）
kubectl -n dkcs-prod port-forward svc/postgres-exporter 9187:9187 &
curl -s localhost:9187/metrics | head -5
#   期望输出: # HELP pg_up ... / pg_up 1

# 2. Prometheus 侧确认 target 为 UP
#    Prometheus UI → Status → Targets → job=postgres-exporter 应为 UP (postgres-exporter:9187/metrics)

# 3. 查询指标
#    PromQL: up{job="postgres-exporter"} == 1
#    PromQL: pg_stat_database_numbackends / pg_up
```

若 target 显示 DOWN：检查 exporter Pod 日志（连接串/密码）、`dkcs-secrets` 的 `postgres-password` 是否与 StatefulSet 实际密码一致。
