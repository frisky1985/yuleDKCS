# BER-TLV 协议部署指南

> **版本**: v1.0.0  
> **协议版本**: v1.2.0  
> **最后更新**: 2026-05-10

---

## 1. 部署架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   TLV Protocol Deployment Architecture                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐        │
│  │                     Backend (云端)                               │        │
│  ├───────────────────────────────────────────────────────────────────────┤        │
│  │  ┌─────────────────────────────────────────────────────────────────┐          │        │
│  │  │  MQTT Broker Cluster (EMQX/VerneMQ)                            │          │        │
│  │  │  • TLS 1.3 + mTLS                                              │          │        │
│  │  │  • 连接数: 100万+                                             │          │        │
│  │  │  • Topic过滤 + ACL                                           │          │        │
│  │  └─────────────────────────────────────────────────────────────────┘          │        │
│  │  ┌─────────────────────────────────────────────────────────────────┐          │        │
│  │  │  Protocol Gateway (Kubernetes)                                 │          │        │
│  │  │  • TLV解码/验证/转换                                         │          │        │
│  │  │  • Auto-scaling (HPA)                                        │          │        │
│  │  │  • 多活动部署 (Region A/B)                                      │          │        │
│  │  └─────────────────────────────────────────────────────────────────┘          │        │
│  └───────────────────────────────────────────────────────────────────────┘        │
│                                    │                                     │
│                                    ▼ MQTT/HTTP2                        │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐        │
│  │                     Embedded (车端)                              │        │
│  ├───────────────────────────────────────────────────────────────────────┤        │
│  │  T-Box/IVI → MQTT Client → TLS加密 → 离线缓存 → 云端             │        │
│  │  • 固件OTA升级                                                  │        │
│  │  • 低功耗模式 (Sleep/Wake)                                       │        │
│  │  • Watchdog + 安全引导                                          │        │
│  └───────────────────────────────────────────────────────────────────────┘        │
│                                    │                                     │
│                                    ▼ HTTP/2 + FCM/APNs                │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐        │
│  │                      Frontend (手机SDK)                           │        │
│  ├───────────────────────────────────────────────────────────────────────┤        │
│  │  Android/iOS SDK → HTTP/2 → 推送服务                                │        │
│  │  • 本地缓存 + 增量同步                                          │        │
│  │  • 前后台保活 + 重连机制                                          │        │
│  │  • 推送Token管理                                                │        │
│  └───────────────────────────────────────────────────────────────────────┘        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 环境准备

### 2.1 云端部署

```yaml
# docker-compose.yml - 本地开发环境
version: '3.8'

services:
  # MQTT Broker
  emqx:
    image: emqx/emqx:5.3.0
    ports:
      - "1883:1883"   # MQTT
      - "8883:8883"   # MQTT over TLS
      - "8083:8083"   # WebSocket
      - "18083:18083" # Dashboard
    environment:
      - EMQX_AUTHENTICATION__1__MECHANISM=password_based
      - EMQX_AUTHENTICATION__1__BACKEND=builtin_database
      - EMQX_AUTHORIZATION__SOURCES__1__TYPE=file
    volumes:
      - ./emqx.conf:/opt/emqx/etc/emqx.conf
      - ./certs:/opt/emqx/etc/certs

  # Protocol Gateway
  gateway:
    build: ./gateway
    ports:
      - "8080:8080"
      - "8443:8443"
    environment:
      - MQTT_BROKER=emqx:1883
      - DB_HOST=postgres
      - REDIS_HOST=redis
    depends_on:
      - emqx
      - postgres
      - redis
    deploy:
      replicas: 2
      resources:
        limits:
          cpus: '2.0'
          memory: 2G

  # TimescaleDB
  postgres:
    image: timescale/timescaledb:latest-pg15
    environment:
      - POSTGRES_USER=tlv_user
      - POSTGRES_PASSWORD=secure_password
      - POSTGRES_DB=automotive_db
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./init.sql:/docker-entrypoint-initdb.d/init.sql
    ports:
      - "5432:5432"

  # Redis (缓存/会话)
  redis:
    image: redis:7-alpine
    command: redis-server --appendonly yes --maxmemory 2gb
    volumes:
      - redis_data:/data

  # 监控
  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
    ports:
      - "9090:9090"

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin

volumes:
  postgres_data:
  redis_data:
```

### 2.2 Kubernetes 部署

```yaml
# k8s/namespace.yaml
apiVersion: v1
kind: Namespace
metadata:
  name: automotive-tlv
  labels:
    app: tlv-protocol
    version: v1.2.0

---
# k8s/gateway-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: tlv-gateway
  namespace: automotive-tlv
spec:
  replicas: 3
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  selector:
    matchLabels:
      app: tlv-gateway
  template:
    metadata:
      labels:
        app: tlv-gateway
        version: v1.2.0
    spec:
      affinity:
        podAntiAffinity:
          preferredDuringSchedulingIgnoredDuringExecution:
            - weight: 100
              podAffinityTerm:
                labelSelector:
                  matchExpressions:
                    - key: app
                      operator: In
                      values:
                        - tlv-gateway
                topologyKey: kubernetes.io/hostname
      containers:
        - name: gateway
          image: automotive/tlv-gateway:v1.2.0
          ports:
            - containerPort: 8080
              name: http
            - containerPort: 8443
              name: https
            - containerPort: 8883
              name: mqtt-tls
          resources:
            requests:
              memory: "512Mi"
              cpu: "500m"
            limits:
              memory: "2Gi"
              cpu: "2000m"
          env:
            - name: PROTOCOL_VERSION
              value: "1.2.0"
            - name: MQTT_BROKER
              valueFrom:
                configMapKeyRef:
                  name: tlv-config
                  key: mqtt.broker
          livenessProbe:
            httpGet:
              path: /health
              port: 8080
            initialDelaySeconds: 30
            periodSeconds: 10
          readinessProbe:
            httpGet:
              path: /ready
              port: 8080
            initialDelaySeconds: 5
            periodSeconds: 5

---
# k8s/hpa.yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: tlv-gateway-hpa
  namespace: automotive-tlv
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: tlv-gateway
  minReplicas: 3
  maxReplicas: 50
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70
    - type: Resource
      resource:
        name: memory
        target:
          type: Utilization
          averageUtilization: 80
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 60
      policies:
        - type: Percent
          value: 100
          periodSeconds: 15
    scaleDown:
      stabilizationWindowSeconds: 300
      policies:
        - type: Percent
          value: 10
          periodSeconds: 60
```

---

## 3. 车端部署

### 3.1 固件集成

```c
// 固件main.c示例
#include "tlv_codec.h"
#include "mqtt_client.h"
#include "tls_wrapper.h"

#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 2
#define FIRMWARE_VERSION_PATCH 0

// 协议配置
static const tlv_protocol_config_t protocol_config = {
    .version = {
        .major = FIRMWARE_VERSION_MAJOR,
        .minor = FIRMWARE_VERSION_MINOR,
        .patch = FIRMWARE_VERSION_PATCH
    },
    .max_message_size = 4096,
    .heartbeat_interval_ms = 30000,
    .reconnect_backoff_ms = 5000,
    .enable_compression = true,
    .compression_threshold = 256
};

void app_main(void) {
    // 初始化协议库
    tlv_init(&protocol_config);
    
    // 初始化MQTT客户端
    mqtt_client_config_t mqtt_config = {
        .broker_url = "mqtts://cloud.automotive.com:8883",
        .client_id = get_device_id(),
        .tls_ca_cert = ca_cert_pem,
        .tls_client_cert = client_cert_pem,
        .tls_client_key = client_key_pem
    };
    
    mqtt_client_handle_t client = mqtt_client_init(&mqtt_config);
    
    // 注册消息处理回调
    mqtt_client_register_callback(client, MQTT_EVENT_DATA, on_mqtt_message);
    
    // 启动连接
    mqtt_client_connect(client);
    
    // 主循环
    while (1) {
        // 发送心跳
        send_heartbeat(client);
        
        // 检查OTA
        check_ota_update(client);
        
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

static void on_mqtt_message(void *handler_args, esp_event_base_t base, 
                            int32_t event_id, void *event_data) {
    mqtt_event_handle_t event = event_data;
    
    // 验证TLV消息
    if (tlv_validate_message((uint8_t*)event->data, event->data_len) != TLV_OK) {
        ESP_LOGE(TAG, "Invalid TLV message received");
        return;
    }
    
    // 解析消息
    TlvParser parser;
    tlv_parser_init(&parser, (uint8_t*)event->data, event->data_len);
    
    // 处理命令
    process_command(&parser);
}
```

### 3.2 OTA升级流程

```c
// ota_handler.c
#include "esp_ota_ops.h"

void handle_ota_update(mqtt_client_handle_t client, const tlv_element_t* package_info) {
    // 验证OTA包签名
    if (!verify_ota_signature(package_info)) {
        send_ota_response(client, OTA_ERROR_SIGNATURE_INVALID);
        return;
    }
    
    // 下载差分包
    esp_err_t err = esp_https_ota(&ota_config);
    if (err == ESP_OK) {
        // 验证新固件
        if (verify_firmware()) {
            send_ota_response(client, OTA_SUCCESS);
            esp_restart();
        } else {
            send_ota_response(client, OTA_ERROR_VERIFICATION_FAILED);
            esp_ota_abort();
        }
    }
}
```

---

## 4. 手机SDK部署

### 4.1 Android SDK

```kotlin
// TlvProtocolClient.kt
class TlvProtocolClient(
    private val context: Context,
    private val config: ClientConfig
) {
    private val httpClient = OkHttpClient.Builder()
        .protocols(listOf(Protocol.HTTP_2, Protocol.HTTP_1_1))
        .connectionSpecs(listOf(ConnectionSpec.MODERN_TLS))
        .addInterceptor(TlvEncodingInterceptor())
        .build()
    
    private val tlvCodec = TlvCodec()
    private val messageQueue = LinkedBlockingQueue<TlvMessage>()
    
    suspend fun connect() {
        // 版本协商
        val negotiatedVersion = performVersionNegotiation()
        
        // 认证
        authenticate()
        
        // 启动定期同步
        startPeriodicSync()
        
        // 注册推送处理
        FirebaseMessaging.getInstance().subscribeToTopic("vehicle_${config.vin}")
    }
    
    suspend fun sendCommand(command: VehicleCommand): CommandResult {
        val message = tlvCodec.encodeCommand(command)
        
        return withContext(Dispatchers.IO) {
            val request = Request.Builder()
                .url("${config.baseUrl}/v2/command")
                .post(message.toRequestBody("application/x-tlv".toMediaType()))
                .header("X-Protocol-Version", "1.2.0")
                .build()
            
            httpClient.newCall(request).execute().use { response ->
                if (response.isSuccessful) {
                    tlvCodec.decodeCommandResult(response.body!!.bytes())
                } else {
                    throw CommandException(response.code)
                }
            }
        }
    }
    
    private fun startPeriodicSync() {
        WorkManager.getInstance(context).enqueueUniquePeriodicWork(
            "vehicle_sync",
            ExistingPeriodicWorkPolicy.KEEP,
            PeriodicWorkRequestBuilder<VehicleSyncWorker>(15, TimeUnit.MINUTES)
                .setConstraints(
                    Constraints.Builder()
                        .setRequiredNetworkType(NetworkType.CONNECTED)
                        .build()
                )
                .build()
        )
    }
}
```

### 4.2 iOS SDK

```swift
// TlvProtocolClient.swift
import Foundation
import Network

public class TlvProtocolClient {
    private let config: ClientConfig
    private var connection: NWConnection?
    private let tlvCodec = TlvCodec()
    
    public init(config: ClientConfig) {
        self.config = config
    }
    
    public func connect() async throws {
        let tlsOptions = NWProtocolTLS.Options()
        sec_protocol_options_set_verify_block(
            tlsOptions.securityProtocolOptions,
            { _, _, completion in completion(true) },
            DispatchQueue.global()
        )
        
        let parameters = NWParameters(tls: tlsOptions)
        parameters.defaultProtocolStack.applicationProtocols.insert(
            NWProtocolWebSocket.Options(),
            at: 0
        )
        
        connection = NWConnection(
            to: NWEndpoint.host(config.host, port: config.port),
            using: parameters
        )
        
        try await withCheckedThrowingContinuation { continuation in
            connection?.stateUpdateHandler = { state in
                switch state {
                case .ready:
                    continuation.resume()
                case .failed(let error):
                    continuation.resume(throwing: error)
                default:
                    break
                }
            }
            connection?.start(queue: .global())
        }
    }
    
    public func sendCommand(_ command: VehicleCommand) async throws -> CommandResult {
        let message = tlvCodec.encode(command)
        
        return try await withCheckedThrowingContinuation { continuation in
            connection?.send(content: message, completion: .contentProcessed { error in
                if let error = error {
                    continuation.resume(throwing: error)
                    return
                }
                
                // 等待响应
                self.connection?.receiveMessage { data, context, isComplete, error in
                    if let data = data {
                        let result = self.tlvCodec.decodeCommandResult(data)
                        continuation.resume(returning: result)
                    }
                }
            })
        }
    }
}
```

---

## 5. 监控与警报

### 5.1 Prometheus指标

```yaml
# prometheus.yml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'tlv-gateway'
    static_configs:
      - targets: ['gateway:8080']
    metrics_path: /metrics

  - job_name: 'mqtt-broker'
    static_configs:
      - targets: ['emqx:18083']

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rule_files:
  - 'tlv_alerts.yml'
```

```yaml
# tlv_alerts.yml
groups:
  - name: tlv_protocol
    rules:
      - alert: HighDecodeErrorRate
        expr: rate(tlv_decode_errors_total[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "TLV解码错误率过高"
          description: "{{ $value }} 错误/秒, 超过阈值"

      - alert: ProtocolVersionMismatch
        expr: increase(tlv_version_mismatch_total[1h]) > 100
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "TLV版本不匹配"
          description: "检洗可能需要协议升级"

      - alert: HighMessageLatency
        expr: histogram_quantile(0.99, rate(tlv_message_duration_seconds_bucket[5m])) > 0.5
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "TLV消息延迟过高"

      - alert: VehicleOffline
        expr: tlv_vehicle_online == 0
        for: 10m
        labels:
          severity: info
        annotations:
          summary: "车辆离线: {{ $labels.vin }}"
```

### 5.2 Grafana仪表盘

```json
{
  "dashboard": {
    "title": "TLV Protocol Monitoring",
    "panels": [
      {
        "title": "Message Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(tlv_messages_total[5m])",
            "legendFormat": "{{ message_type }}"
          }
        ]
      },
      {
        "title": "Decode Latency",
        "type": "heatmap",
        "targets": [
          {
            "expr": "rate(tlv_decode_duration_seconds_bucket[5m])"
          }
        ]
      },
      {
        "title": "Active Connections",
        "type": "stat",
        "targets": [
          {
            "expr": "tlv_active_connections",
            "instant": true
          }
        ]
      },
      {
        "title": "Protocol Version Distribution",
        "type": "piechart",
        "targets": [
          {
            "expr": "tlv_protocol_version_total",
            "legendFormat": "{{ version }}"
          }
        ]
      }
    ]
  }
}
```

---

## 6. 安全配置

### 6.1 TLS配置

```nginx
# nginx.conf - MQTT over WebSocket代理
stream {
    upstream mqtt_backend {
        server emqx-1:1883;
        server emqx-2:1883;
        server emqx-3:1883;
    }
    
    server {
        listen 8883 ssl;
        proxy_pass mqtt_backend;
        proxy_buffer_size 4k;
        
        ssl_certificate /etc/nginx/certs/server.crt;
        ssl_certificate_key /etc/nginx/certs/server.key;
        ssl_client_certificate /etc/nginx/certs/ca.crt;
        ssl_verify_client optional;
        ssl_verify_depth 2;
        
        # 强制TLS 1.3
        ssl_protocols TLSv1.3;
        ssl_ciphers TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256;
        ssl_prefer_server_ciphers off;
    }
}
```

### 6.2 密钥管理

```yaml
# HashiCorp Vault配置
path "secret/data/automotive/tlv" {
  capabilities = ["read"]
}

# 从Vault获取证书
vault kv get -format=json secret/automotive/tls > /tmp/certs.json
```

---

## 7. 灾难恢复

### 7.1 故障转移流程

```bash
#!/bin/bash
# failover.sh

# 检测主数据库
disaster_recovery() {
    echo "Starting disaster recovery procedure..."
    
    # 1. 切换DNS到备用区域
    aws route53 change-resource-record-sets \
        --hosted-zone-id $ZONE_ID \
        --change-batch file://failover.json
    
    # 2. 激活备用MQTT集群
    kubectl scale deployment emqx-backup --replicas=3 -n automotive-tlv
    
    # 3. 通知运营团队
    curl -X POST $SLACK_WEBHOOK \
        -H 'Content-Type: application/json' \
        -d '{"text": "⚠️ 灾难恢复已启动 - TLV服务切换到备用区域"}'
}

# 健康检查
if ! pg_isready -h postgres-primary; then
    disaster_recovery
fi
```

### 7.2 数据备份

```yaml
# velero备份策略
apiVersion: velero.io/v1
kind: Schedule
metadata:
  name: tlv-daily-backup
  namespace: velero
spec:
  schedule: "0 2 * * *"
  template:
    includedNamespaces:
      - automotive-tlv
    includedResources:
      - deployments
      - configmaps
      - secrets
      - persistentvolumeclaims
    labelSelector:
      matchLabels:
        app: tlv-protocol
    storageLocation: default
    ttl: 720h0m0s
```

---

## 8. 验证清单

### 8.1 部署前检查

| 检查项 | 命令/方法 | 通过标准 |
|--------|----------|---------|
| 证书有效性 | `openssl x509 -in cert.pem -text -noout` | 未过期 |
| 端口通讯 | `nc -zv host 8883` | 连通 |
| DNS解析 | `dig +short mqtt.automotive.com` | 正确IP |
| 资源限制 | `kubectl describe limitrange` | 设置合理 |
| 配置一致性 | `diff config.yaml configmap.yaml` | 匹配 |

### 8.2 部署后验证

```bash
# 1. 检查Pod状态
kubectl get pods -n automotive-tlv -o wide

# 2. 验证服务端点
curl -s http://gateway:8080/health | jq .

# 3. 测试MQTT连接
mosquitto_sub -h mqtt.automotive.com -p 8883 -t test \
  --cafile ca.crt --cert client.crt --key client.key

# 4. 发送测试消息
python3 -c "
import sys
sys.path.insert(0, 'generated/python')
from tlv_protocol import TlvCodec
# ... 发送测试消息
"

# 5. 检查日志
kubectl logs -n automotive-tlv -l app=tlv-gateway --tail=100

# 6. 验证指标
curl -s http://gateway:8080/metrics | grep tlv_
```

---

*部署指南版本: v1.0.0*  
*支持版本: v1.2.0*