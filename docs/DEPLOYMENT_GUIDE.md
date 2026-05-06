# yuleDKCS 部署指南

**版本**: 1.0.0
**日期**: 2026-05-06

---

## 1. 环境要求

### 1.1 基础设施

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| Kubernetes | 1.28+ | 容器编排平台 |
| Docker | 24.0+ | 容器运行时 |
| PostgreSQL | 15.0+ | 主数据库 |
| Redis | 7.0+ | 缓存 + 分布式锁 |
| Kafka | 3.6+ | 消息队列 |
| Nginx | 1.24+ | Ingress 控制器 |

### 1.2 开发环境

| 组件 | 版本要求 |
|------|----------|
| Go | 1.22+ |
| Java | 17+ |
| Node.js | 20+ (可选，用于前端构建) |
| Docker Compose | 2.20+ |

---

## 2. 本地开发部署

### 2.1 使用 Docker Compose

创建 `docker-compose.yml`:

```yaml
version: '3.8'

services:
  postgres:
    image: postgres:15-alpine
    environment:
      POSTGRES_USER: digitalkey
      POSTGRES_PASSWORD: digitalkey123
      POSTGRES_DB: digitalkey_db
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./backend/db/schema.sql:/docker-entrypoint-initdb.d/schema.sql

  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"
    volumes:
      - redis_data:/data

  zookeeper:
    image: confluentinc/cp-zookeeper:7.5.0
    environment:
      ZOOKEEPER_CLIENT_PORT: 2181
      ZOOKEEPER_TICK_TIME: 2000

  kafka:
    image: confluentinc/cp-kafka:7.5.0
    depends_on:
      - zookeeper
    ports:
      - "9092:9092"
    environment:
      KAFKA_BROKER_ID: 1
      KAFKA_ZOOKEEPER_CONNECT: zookeeper:2181
      KAFKA_ADVERTISED_LISTENERS: PLAINTEXT://localhost:9092
      KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR: 1

  dkcs:
    build:
      context: ./backend/dkcs
      dockerfile: Dockerfile
    ports:
      - "50051:50051"
      - "9090:9090"
    environment:
      GRPC_PORT: 50051
      DB_HOST: postgres
      DB_PORT: 5432
      DB_USER: digitalkey
      DB_PASSWORD: digitalkey123
      DB_NAME: digitalkey_db
      REDIS_ADDR: redis:6379
      KAFKA_BROKERS: kafka:9092
      JWT_SECRET: dev-secret-change-in-prod
    depends_on:
      - postgres
      - redis
      - kafka

  hub:
    build:
      context: ./backend/cloud/hub
      dockerfile: Dockerfile
    ports:
      - "8080:8080"
    environment:
      GRPC_DKCS_ADDR: dkcs:50051
    depends_on:
      - dkcs

volumes:
  postgres_data:
  redis_data:
```

启动服务:

```bash
docker-compose up -d
```

### 2.2 本地直接运行

**启动依赖服务**:

```bash
# PostgreSQL (使用 Docker)
docker run -d --name postgres \
  -e POSTGRES_USER=digitalkey \
  -e POSTGRES_PASSWORD=digitalkey123 \
  -e POSTGRES_DB=digitalkey_db \
  -p 5432:5432 \
  postgres:15-alpine

# Redis
docker run -d --name redis -p 6379:6379 redis:7-alpine

# Kafka (使用 KRaft 模式)
docker run -d --name kafka \
  -e KAFKA_PROCESS_ROLES=broker,controller \
  -e KAFKA_NODE_ID=1 \
  -e KAFKA_CONTROLLER_QUORUM_VOTERS=1@localhost:9093 \
  -e KAFKA_LISTENERS=PLAINTEXT://0.0.0.0:9092,CONTROLLER://0.0.0.0:9093 \
  -e KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://localhost:9092 \
  -p 9092:9092 \
  confluentinc/cp-kafka:7.5.0
```

**初始化数据库**:

```bash
psql -h localhost -U digitalkey -d digitalkey_db -f backend/db/schema.sql
```

**运行 DKCS 服务**:

```bash
cd backend/dkcs

# 设置环境变量
export GRPC_PORT=50051
export DB_HOST=localhost
export DB_PORT=5432
export DB_USER=digitalkey
export DB_PASSWORD=digitalkey123
export DB_NAME=digitalkey_db
export REDIS_ADDR=localhost:6379
export KAFKA_BROKERS=localhost:9092
export JWT_SECRET=dev-secret-change-in-prod

# 运行
go run cmd/dkcs/main.go
```

**运行 HUB 服务**:

```bash
cd backend/cloud/hub
go run cmd/hub/main.go
```

**运行 Java Adapters**:

```bash
cd backend/adapters
mvn clean package -DskipTests
java -jar adapter-grpc-server/target/adapter-grpc-server.jar
```

---

## 3. Kubernetes 生产部署

### 3.1 命名空间

```yaml
# namespace.yaml
apiVersion: v1
kind: Namespace
metadata:
  name: dkcs
```

### 3.2 配置映射

```yaml
# configmap.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: dkcs-config
  namespace: dkcs
data:
  GRPC_PORT: "50051"
  DB_HOST: "postgres.dkcs.svc.cluster.local"
  DB_PORT: "5432"
  DB_USER: "digitalkey"
  DB_NAME: "digitalkey_db"
  DB_SSL_MODE: "require"
  REDIS_ADDR: "redis.dkcs.svc.cluster.local:6379"
  KAFKA_BROKERS: "kafka-0.kafka.dkcs.svc.cluster.local:9092,kafka-1.kafka.dkcs.svc.cluster.local:9092,kafka-2.kafka.dkcs.svc.cluster.local:9092"
  JWT_ISSUER: "dkcs"
  LOG_LEVEL: "info"
  LOG_FORMAT: "json"
  METRICS_ENABLED: "true"
  METRICS_PORT: "9090"
  METRICS_PATH: "/metrics"
```

### 3.3 密钥

```yaml
# secret.yaml
apiVersion: v1
kind: Secret
metadata:
  name: dkcs-secret
  namespace: dkcs
type: Opaque
stringData:
  DB_PASSWORD: "your-db-password"
  REDIS_PASSWORD: "your-redis-password"
  JWT_SECRET: "your-jwt-secret"
```

### 3.4 DKCS 部署

```yaml
# dkcs-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: dkcs
  namespace: dkcs
spec:
  replicas: 3
  selector:
    matchLabels:
      app: dkcs
  template:
    metadata:
      labels:
        app: dkcs
    spec:
      containers:
      - name: dkcs
        image: yuledkcs/dkcs:1.0.0
        ports:
        - containerPort: 50051
          name: grpc
        - containerPort: 9090
          name: metrics
        envFrom:
        - configMapRef:
            name: dkcs-config
        - secretRef:
            name: dkcs-secret
        resources:
          requests:
            cpu: 500m
            memory: 512Mi
          limits:
            cpu: 2000m
            memory: 2Gi
        livenessProbe:
          exec:
            command:
            - grpc_health_probe
            - -addr=:50051
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          exec:
            command:
            - grpc_health_probe
            - -addr=:50051
          initialDelaySeconds: 5
          periodSeconds: 5
---
apiVersion: v1
kind: Service
metadata:
  name: dkcs
  namespace: dkcs
spec:
  selector:
    app: dkcs
  ports:
  - port: 50051
    targetPort: 50051
    name: grpc
  - port: 9090
    targetPort: 9090
    name: metrics
```

### 3.5 HUB 部署

```yaml
# hub-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: hub
  namespace: dkcs
spec:
  replicas: 3
  selector:
    matchLabels:
      app: hub
  template:
    metadata:
      labels:
        app: hub
    spec:
      containers:
      - name: hub
        image: yuledkcs/hub:1.0.0
        ports:
        - containerPort: 8080
          name: http
        env:
        - name: GRPC_DKCS_ADDR
          value: "dkcs.dkcs.svc.cluster.local:50051"
        resources:
          requests:
            cpu: 500m
            memory: 512Mi
          limits:
            cpu: 2000m
            memory: 2Gi
---
apiVersion: v1
kind: Service
metadata:
  name: hub
  namespace: dkcs
spec:
  selector:
    app: hub
  ports:
  - port: 8080
    targetPort: 8080
```

### 3.6 Java Adapters 部署

```yaml
# adapters-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: adapter-ccc
  namespace: dkcs
spec:
  replicas: 2
  selector:
    matchLabels:
      app: adapter-ccc
  template:
    metadata:
      labels:
        app: adapter-ccc
    spec:
      containers:
      - name: adapter-ccc
        image: yuledkcs/adapter-ccc:1.0.0
        ports:
        - containerPort: 9090
        env:
        - name: GRPC_SERVER_PORT
          value: "9090"
        - name: DKCS_ADDR
          value: "dkcs.dkcs.svc.cluster.local:50051"
---
# 类似部署 adapter-iccoa 和 adapter-icce
```

### 3.7 Ingress

```yaml
# ingress.yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: dkcs-ingress
  namespace: dkcs
  annotations:
    nginx.ingress.kubernetes.io/ssl-redirect: "true"
    nginx.ingress.kubernetes.io/proxy-body-size: "10m"
    cert-manager.io/cluster-issuer: "letsencrypt-prod"
spec:
  ingressClassName: nginx
  tls:
  - hosts:
    - api.yuledkcs.com
    secretName: dkcs-tls
  rules:
  - host: api.yuledkcs.com
    http:
      paths:
      - path: /v1
        pathType: Prefix
        backend:
          service:
            name: hub
            port:
              number: 8080
```

---

## 4. 数据库部署

### 4.1 PostgreSQL

```yaml
# postgres.yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: postgres
  namespace: dkcs
spec:
  serviceName: postgres
  replicas: 1
  selector:
    matchLabels:
      app: postgres
  template:
    metadata:
      labels:
        app: postgres
    spec:
      containers:
      - name: postgres
        image: postgres:15-alpine
        ports:
        - containerPort: 5432
        env:
        - name: POSTGRES_USER
          value: "digitalkey"
        - name: POSTGRES_PASSWORD
          valueFrom:
            secretKeyRef:
              name: dkcs-secret
              key: DB_PASSWORD
        - name: POSTGRES_DB
          value: "digitalkey_db"
        volumeMounts:
        - name: postgres-data
          mountPath: /var/lib/postgresql/data
  volumeClaimTemplates:
  - metadata:
      name: postgres-data
    spec:
      accessModes: ["ReadWriteOnce"]
      resources:
        requests:
          storage: 100Gi
```

### 4.2 Redis

```yaml
# redis.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: redis
  namespace: dkcs
spec:
  replicas: 1
  selector:
    matchLabels:
      app: redis
  template:
    metadata:
      labels:
        app: redis
    spec:
      containers:
      - name: redis
        image: redis:7-alpine
        ports:
        - containerPort: 6379
        command:
        - redis-server
        - --appendonly yes
```

### 4.3 Kafka

使用 Strimzi Operator 部署 Kafka 集群:

```yaml
# kafka.yaml
apiVersion: kafka.strimzi.io/v1beta2
kind: Kafka
metadata:
  name: kafka
  namespace: dkcs
spec:
  kafka:
    version: 3.6.0
    replicas: 3
    listeners:
      - name: plain
        port: 9092
        type: internal
        tls: false
    config:
      offsets.topic.replication.factor: 3
      transaction.state.log.replication.factor: 3
      transaction.state.log.min.isr: 2
    storage:
      type: jbod
      volumes:
      - id: 0
        type: persistent-claim
        size: 100Gi
        class: fast-ssd
  zookeeper:
    replicas: 3
    storage:
      type: persistent-claim
      size: 10Gi
```

---

## 5. 监控部署

### 5.1 Prometheus

```yaml
# prometheus.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: prometheus
  namespace: monitoring
spec:
  replicas: 1
  selector:
    matchLabels:
      app: prometheus
  template:
    metadata:
      labels:
        app: prometheus
    spec:
      containers:
      - name: prometheus
        image: prom/prometheus:v2.48.0
        ports:
        - containerPort: 9090
        volumeMounts:
        - name: prometheus-config
          mountPath: /etc/prometheus
      volumes:
      - name: prometheus-config
        configMap:
          name: prometheus-config
```

### 5.2 Grafana

```yaml
# grafana.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: grafana
  namespace: monitoring
spec:
  replicas: 1
  selector:
    matchLabels:
      app: grafana
  template:
    metadata:
      labels:
        app: grafana
    spec:
      containers:
      - name: grafana
        image: grafana/grafana:10.2.0
        ports:
        - containerPort: 3000
        env:
        - name: GF_SECURITY_ADMIN_PASSWORD
          value: "admin"
```

---

## 6. 部署命令

### 6.1 部署顺序

```bash
# 1. 创建命名空间
kubectl apply -f namespace.yaml

# 2. 创建配置和密钥
kubectl apply -f configmap.yaml
kubectl apply -f secret.yaml

# 3. 部署数据库
kubectl apply -f postgres.yaml
kubectl apply -f redis.yaml
kubectl apply -f kafka.yaml

# 4. 等待数据库就绪
kubectl wait --for=condition=ready pod -l app=postgres -n dkcs --timeout=300s
kubectl wait --for=condition=ready pod -l app=redis -n dkcs --timeout=300s

# 5. 部署应用
kubectl apply -f dkcs-deployment.yaml
kubectl apply -f hub-deployment.yaml
kubectl apply -f adapters-deployment.yaml

# 6. 部署 Ingress
kubectl apply -f ingress.yaml

# 7. 部署监控
kubectl apply -f prometheus.yaml
kubectl apply -f grafana.yaml
```

### 6.2 验证部署

```bash
# 检查 Pod 状态
kubectl get pods -n dkcs

# 检查服务状态
kubectl get svc -n dkcs

# 检查日志
kubectl logs -f deployment/dkcs -n dkcs

# 端口转发测试
kubectl port-forward svc/hub 8080:8080 -n dkcs

# 健康检查
grpcurl -plaintext localhost:50051 grpc.health.v1.Health/Check
```

---

## 7. 滚动更新

```bash
# 更新镜像
kubectl set image deployment/dkcs dkcs=yuledkcs/dkcs:1.0.1 -n dkcs

# 查看滚动状态
kubectl rollout status deployment/dkcs -n dkcs

# 回滚
kubectl rollout undo deployment/dkcs -n dkcs
```

---

## 8. 扩缩容

```bash
# 手动扩容
kubectl scale deployment dkcs --replicas=5 -n dkcs

# HPA 自动扩缩容
kubectl autoscale deployment dkcs --min=3 --max=10 --cpu-percent=70 -n dkcs
```

---

## 9. 备份与恢复

### 9.1 数据库备份

```bash
# 备份
kubectl exec -it postgres-0 -n dkcs -- \
  pg_dump -U digitalkey digitalkey_db > backup_$(date +%Y%m%d).sql

# 恢复
kubectl exec -it postgres-0 -n dkcs -- \
  psql -U digitalkey digitalkey_db < backup_20260506.sql
```

---

## 10. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
