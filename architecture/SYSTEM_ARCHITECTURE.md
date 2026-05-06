# 数字钥匙系统架构设计 (Go+Java混合架构)

**版本**: v2.0
**更新日期**: 2026-05-06
**状态**: 设计定稿

---

## 1. 架构概览

### 1.1 整体架构

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                              客户端层                                            │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                     │
│  │  微信小程序   │    │  Android App │    │   iOS App    │                     │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘                     │
│         │                    │                    │                             │
│         └────────────────────┼────────────────────┘                             │
│                              │                                                  │
└──────────────────────────────┼──────────────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────────────┐
│                           网关层 (Go)                                            │
│  ┌──────────────────────────────────────────────────────────────────────────┐   │
│  │                        API Gateway                                       │   │
│  │  • 统一入口/鉴权/限流                                                    │   │
│  │  • 协议路由 (HTTP/WebSocket/gRPC)                                        │   │
│  │  • 会话管理/设备绑定                                                     │   │
│  └──────────────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────┬──────────────────────────────────────────────────┘
                               │
         ┌─────────────────────┴─────────────────────┐
         │                                           │
┌────────▼────────┐                    ┌────────────▼─────────────┐
│   核心服务层    │◄──────────────────►│     适配服务层 (Java)     │
│      (Go)       │    gRPC双向流       │                           │
│                │                    │  ┌─────────────────────┐  │
│  • 车控指令    │                    │  │  Adapter Core        │  │
│  • 实时通信    │                    │  │  (抽象层/公共组件)   │  │
│  • 状态同步    │                    │  └─────────────────────┘  │
│  • 密钥分发    │                    │  ┌─────────────────────┐  │
│  • 事件推送    │                    │  │  CCC Adapter        │  │
│                │                    │  │  ICCOA Adapter      │  │
└────────┬───────┘                    │  │  ICCE Adapter       │  │
         │                            │  └─────────────────────┘  │
         │                            │  ┌─────────────────────┐  │
┌────────▼────────┐                   │  │  车企TSP Adapter    │  │
│   数据层        │                   │  │  • 比亚迪TSP        │  │
│  PostgreSQL     │                   │  │  • 蔚来TSP         │  │
│  Redis          │                   │  │  • 长安TSP         │  │
│  MongoDB        │                   │  └─────────────────────┘  │
└─────────────────┘                   └─────────────────────────────┘
         │
         │ 协议交互
┌────────▼────────┐
│   车端TCU        │
│  (嵌入式C)      │
│  • CCC协议栈    │
│  • ICCOA协议栈  │
│  • ICCE协议栈   │
└─────────────────┘
```

### 1.2 技术选型

| 层级 | 技术栈 | 说明 |
|------|--------|------|
| **客户端** | Flutter/原生 | 跨平台数字钥匙钱包 |
| **API网关** | Go + Gin | 高并发，轻量级 |
| **核心服务** | Go + gRPC | 实时车控，指令下发 |
| **适配服务** | Java + Spring Boot | 车企对接，协议转换 |
| **数据库** | PostgreSQL + Redis | 主数据+缓存 |
| **消息队列** | Kafka | 事件驱动架构 |
| **注册中心** | Consul/etcd | 服务发现 |

---

## 2. 组件设计

### 2.1 API Gateway (Go)

**职责**：
- 统一鉴权 (JWT/OAuth2)
- 请求路由 (HTTP → gRPC/WebSocket)
- 限流熔断 (令牌桶/滑动窗口)
- 协议转换 (HTTP ↔ gRPC)
- 会话管理

**接口**：
```protobuf
// HTTP/REST → 内部gRPC
service GatewayService {
    rpc AuthDevice(AuthRequest) returns (AuthResponse);
    rpc BindKey(BindRequest) returns (BindResponse);
    rpc SendCommand(CommandRequest) returns (stream CommandResponse);
    rpc GetKeyList(KeyListRequest) returns (KeyListResponse);
}
```

### 2.2 Core Service (Go)

**职责**：
- 实时车控指令下发
- 车辆状态同步
- 密钥生命周期管理
- 事件推送 (WebSocket/gRPC streaming)
- 与Adapter层的双向通信

**核心接口**：
```protobuf
service CoreService {
    // 密钥管理
    rpc CreateKey(CreateKeyRequest) returns (CreateKeyResponse);
    rpc ActivateKey(ActivateKeyRequest) returns (ActivateKeyResponse);
    rpc RevokeKey(RevokeKeyRequest) returns (RevokeKeyResponse);
    
    // 车控指令
    rpc SendVehicleCommand(VehicleCommand) returns (CommandResult);
    rpc StreamVehicleStatus(stream VehicleStatus) returns (stream ControlCommand);
    
    // 事件订阅
    rpc SubscribeEvents(EventSubscription) returns (stream DomainEvent);
}
```

### 2.3 Adapter Service (Java)

**职责**：
- 车企TSP对接 (HTTP/SOAP/WebSocket)
- 协议转换 (私有协议 ↔ 统一DKCS协议)
- 设备认证 (PKI/证书)
- 消息队列消费
- 适配器热插拔

**架构**：
```
┌─────────────────────────────────────────────────────────────┐
│                    Adapter Service                          │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                 Adapter Core                         │   │
│  │  • 抽象适配器基类                                     │   │
│  │  • 配置管理 (YAML)                                   │   │
│  │  • 健康检查/熔断                                     │   │
│  │  • 消息转换 (DTO ↔ Domain)                          │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │ CCC Adapter  │ │ICCOA Adapter │ │ ICCE Adapter │       │
│  │  (标准联盟)  │ │ (国内手机厂)  │ │ (车端边缘)   │       │
│  └──────────────┘ └──────────────┘ └──────────────┘       │
│                                                             │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       │
│  │ BYD Adapter  │ │ NIO Adapter  │ │ CH Adapter   │       │
│  │ (比亚迪)     │ │ (蔚来)       │ │ (长安)       │       │
│  └──────────────┘ └──────────────┘ └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

**抽象适配器接口**：
```java
public interface TspAdapter {
    // 初始化
    void initialize(AdapterConfig config);
    
    // 设备绑定
    BindingResult bindDevice(BindRequest request);
    
    // 发送车控指令
    CommandResult sendCommand(Command command);
    
    // 获取车辆状态
    VehicleStatus getVehicleStatus(String vehicleId);
    
    // 事件回调
    void onEvent(TspEvent event);
    
    // 健康检查
    HealthStatus healthCheck();
}
```

---

## 3. Go ↔ Java 接口设计

### 3.1 通信方式

| 场景 | 协议 | 说明 |
|------|------|------|
| 同步调用 | gRPC | 命令下发、查询 |
| 异步事件 | Kafka | 状态变更、事件推送 |
| 实时流 | gRPC Streaming | 车控状态双向同步 |

### 3.2 gRPC接口定义

```protobuf
// adapter.proto - Go与Java Adapter之间的接口
syntax = "proto3";

package dkcs.adapter;

option go_package = "github.com/digitalkey/cloud/adapter;adapter";
option java_package = "com.digitalkey.adapter.grpc";
option java_multiple_files = true;

service AdapterService {
    // 同步命令转发
    rpc SendCommand(AdapterCommand) returns (AdapterResponse);
    
    // 批量查询
    rpc BatchQuery(BatchQueryRequest) returns (BatchQueryResponse);
    
    // 状态订阅
    rpc SubscribeStatus(StatusSubscription) returns (stream StatusUpdate);
    
    // 健康检查
    rpc HealthCheck(HealthRequest) returns (HealthResponse);
}

// 适配器命令
message AdapterCommand {
    string request_id = 1;
    string adapter_type = 2;      // ccc, iccoa, byd, nio, etc.
    string action = 3;            // bind, unbind, command, query
    bytes payload = 4;            // TLV编码的业务数据
    map<string, string> metadata = 5;
}

// 适配器响应
message AdapterResponse {
    string request_id = 1;
    bool success = 2;
    int32 error_code = 3;
    string error_message = 4;
    bytes payload = 5;
}

// 状态订阅
message StatusSubscription {
    string vehicle_id = 1;
    repeated string status_types = 2;  // location, door, engine, etc.
}

// 状态更新
message StatusUpdate {
    string vehicle_id = 1;
    string status_type = 2;
    bytes status_data = 3;
    int64 timestamp = 4;
}

// 健康检查
message HealthRequest {}

message HealthResponse {
    bool healthy = 1;
    string version = 2;
    map<string, string> adapter_status = 3;
}
```

---

## 4. 数据模型

### 4.1 核心实体

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│     User        │     │   DigitalKey    │     │     Vehicle     │
├─────────────────┤     ├─────────────────┤     ├─────────────────┤
│ id (UUID)       │◄────│ user_id         │     │ id (VIN)        │
│ phone           │     │ vehicle_id ──────┼────►│ vendor_id       │
│ nickname        │     │ key_id (UUID)   │     │ model           │
│ status          │     │ key_type        │     │ year            │
│ created_at      │     │ status          │     │ tcu_id          │
└─────────────────┘     │ issuer_id       │     └─────────────────┘
                        │ valid_from      │
┌─────────────────┐     │ valid_until     │     ┌─────────────────┐
│   VehicleBind   │     │ max_uses        │     │   TspBinding    │
├─────────────────┤     │ protocol        │     ├─────────────────┤
│ id              │     │ share_code      │     │ id              │
│ user_id         │     │ created_at      │     │ vehicle_id      │
│ vehicle_id      │     └─────────────────┘     │ tsp_type        │
│ bind_time       │                             │ tsp_vehicle_id  │
│ unbind_time     │                             │ bind_time       │
│ status          │                             │ status          │
└─────────────────┘                             └─────────────────┘
```

### 4.2 数据库表设计

```sql
-- 用户表
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    phone VARCHAR(20) UNIQUE NOT NULL,
    nickname VARCHAR(100),
    avatar_url VARCHAR(500),
    status VARCHAR(20) DEFAULT 'active',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 数字钥匙表
CREATE TABLE digital_keys (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),
    vehicle_id VARCHAR(50) REFERENCES vehicles(id),
    key_type VARCHAR(20) NOT NULL,          -- primary, secondary, share, temp
    status VARCHAR(20) NOT NULL,            -- pending, active, suspended, revoked, expired
    issuer_id VARCHAR(50) NOT NULL,         -- 发行方ID
    protocol_version VARCHAR(20) NOT NULL,  -- CCC3.0, ICCOA2.0, etc.
    valid_from TIMESTAMP NOT NULL,
    valid_until TIMESTAMP NOT NULL,
    max_uses INTEGER,                      -- NULL表示无限制
    used_count INTEGER DEFAULT 0,
    share_code VARCHAR(20),
    metadata JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, vehicle_id, id)
);

-- 车辆表
CREATE TABLE vehicles (
    id VARCHAR(50) PRIMARY KEY,            -- VIN
    vendor_id VARCHAR(50) NOT NULL,
    model VARCHAR(100),
    year INTEGER,
    color VARCHAR(20),
    tcu_id VARCHAR(100),
    tcu_type VARCHAR(50),
    status VARCHAR(20) DEFAULT 'active',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 车辆绑定表
CREATE TABLE vehicle_binds (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),
    vehicle_id VARCHAR(50) REFERENCES vehicles(id),
    bind_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    unbind_time TIMESTAMP,
    status VARCHAR(20) DEFAULT 'bound',
    UNIQUE(user_id, vehicle_id)
);

-- TSP绑定表
CREATE TABLE tsp_bindings (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vehicle_id VARCHAR(50) REFERENCES vehicles(id),
    tsp_type VARCHAR(50) NOT NULL,        -- byd, nio, changan, etc.
    tsp_vehicle_id VARCHAR(100) NOT NULL, -- 车企系统的车辆ID
    tsp_user_id VARCHAR(100),             -- 车企系统的用户ID
    bind_token VARCHAR(500),
    bind_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expire_time TIMESTAMP,
    status VARCHAR(20) DEFAULT 'active',
    metadata JSONB
);

-- 密钥操作日志
CREATE TABLE key_operations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    key_id UUID REFERENCES digital_keys(id),
    operation VARCHAR(50) NOT NULL,        -- create, activate, use, revoke, share
    operator VARCHAR(50),                 -- user, system, tsp
    result VARCHAR(20) NOT NULL,           -- success, failed
    error_code VARCHAR(20),
    request_id UUID,
    metadata JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 车控指令日志
CREATE TABLE vehicle_commands (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    key_id UUID REFERENCES digital_keys(id),
    vehicle_id VARCHAR(50) REFERENCES vehicles(id),
    command_type VARCHAR(50) NOT NULL,     -- unlock, lock, start, etc.
    result VARCHAR(20) NOT NULL,
    response_time_ms INTEGER,
    tsp_response JSONB,
    request_id UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 索引
CREATE INDEX idx_keys_user ON digital_keys(user_id);
CREATE INDEX idx_keys_vehicle ON digital_keys(vehicle_id);
CREATE INDEX idx_keys_status ON digital_keys(status);
CREATE INDEX idx_operations_key ON key_operations(key_id);
CREATE INDEX idx_commands_vehicle ON vehicle_commands(vehicle_id);
CREATE INDEX idx_commands_time ON vehicle_commands(created_at);
```

---

## 5. 部署架构

### 5.1 Kubernetes部署

```
┌─────────────────────────────────────────────────────────────────┐
│                        Kubernetes Cluster                       │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   API Gateway (Go)                     │   │
│  │                   Deployment: 3 replicas               │   │
│  │                   HPA: CPU>70% → scale                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Core Service (Go)                    │   │
│  │                   Deployment: 3 replicas               │   │
│  │                   HPA: CPU>70% → scale                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                Adapter Service (Java)                  │   │
│  │                Deployment: 2 replicas per adapter       │   │
│  │                ConfigMap: adapter configurations       │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌────────┐ ┌────────┐ ┌────────┐                              │
│  │PostgreSQL│ │ Redis  │ │ Kafka  │                              │
│  │ (RDS)   │ │(Elasti │ │(MSK)   │                              │
│  └────────┘ └────────┘ └────────┘                              │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Docker Compose (开发环境)

```yaml
version: '3.8'

services:
  # API网关
  gateway:
    build: ./gateway
    ports:
      - "8080:8080"
    depends_on:
      - core
      - adapter-core
    environment:
      - CORE_SERVICE=gcore:9090
      - ADAPTER_SERVICE=adapter:9091

  # 核心服务
  core:
    build: ./core
    ports:
      - "9090:9090"
    depends_on:
      - postgres
      - redis
    environment:
      - DATABASE_URL=postgres://dk:password@postgres:5432/digitalkey
      - REDIS_URL=redis://redis:6379

  # Java适配器服务
  adapter:
    build: ./adapters
    ports:
      - "9091:9091"
    depends_on:
      - core
      - kafka
    environment:
      - KAFKA_BROKERS=kafka:9092
      - CORE_SERVICE=gcore:9090
      - SPRING_PROFILES_ACTIVE=docker

  postgres:
    image: postgres:15
    environment:
      POSTGRES_DB: digitalkey
      POSTGRES_USER: dk
      POSTGRES_PASSWORD: password
    volumes:
      - pgdata:/var/lib/postgresql/data

  redis:
    image: redis:7-alpine
    command: redis-server --appendonly yes

  kafka:
    image: confluentinc/cp-kafka:7.4.0
    depends_on:
      - zookeeper

  zookeeper:
    image: confluentinc/cp-zookeeper:7.4.0

volumes:
  pgdata:
```

---

## 6. 安全设计

### 6.1 认证体系

```
┌─────────────────────────────────────────────────────────────┐
│                    多因素认证                               │
│                                                             │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐                  │
│  │  证书   │ +  │  声纹   │ +  │  设备   │                  │
│  │ PKI    │    │  识别   │    │  绑定   │                  │
│  └────┬────┘    └────┬────┘    └────┬────┘                  │
│       │              │              │                       │
│       └──────────────┼──────────────┘                       │
│                     │                                      │
│              ┌──────▼──────┐                              │
│              │  认证中心    │                              │
│              │  (OAuth2)    │                              │
│              └──────┬──────┘                              │
│                     │                                      │
│              ┌──────▼──────┐                              │
│              │  JWT Token  │                              │
│              │  (15min)   │                              │
│              └─────────────┘                              │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 数据安全

| 数据类型 | 加密方式 | 密钥管理 |
|----------|----------|----------|
| 用户密码 | bcrypt | - |
| 传输数据 | TLS 1.3 | - |
| 数据库敏感字段 | AES-256-GCM | AWS KMS |
| 证书私钥 | RSA-2048 | HSM |
| 车控指令 | 端到端加密 | SE050 |

---

## 7. 监控运维

### 7.1 监控指标

```
┌─────────────────────────────────────────────────────────────┐
│                      监控大盘                                │
│                                                             │
│  服务健康                                                    │
│  ├─ Gateway可用率    ████████████████████ 99.9%            │
│  ├─ Core可用率       ████████████████████ 99.9%              │
│  └─ Adapter可用率    ████████████████████ 99.5%              │
│                                                             │
│  业务指标                                                    │
│  ├─ 日活用户         ████████████ 12,345                   │
│  ├─ 车控指令/日      ████████████████████ 98,234            │
│  └─ 平均响应时间     ████ 45ms                              │
│                                                             │
│  系统资源                                                    │
│  ├─ CPU使用率        ████████ 68%                          │
│  ├─ 内存使用率       ██████ 72%                            │
│  └─ 连接数          ████████████ 5,678                     │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 告警规则

| 指标 | 阈值 | 动作 |
|------|------|------|
| API错误率 | >1% | 短信告警 |
| P99延迟 | >500ms | 邮件告警 |
| 服务可用率 | <99.5% | 电话告警 |
| Kafka lag | >10000 | 钉钉通知 |
| 磁盘使用率 | >80% | 邮件通知 |

---

## 8. 实施计划

### 阶段一：架构落地 (第1-2周)

| 任务 | 负责人 | 工期 | 依赖 |
|------|--------|------|------|
| Go网关/核心服务完善 | pm_project | 3天 | - |
| Java Adapter框架搭建 | pm_project | 4天 | - |
| gRPC接口定义 | pm_project | 1天 | - |
| 数据库Schema更新 | pm_project | 1天 | - |
| Docker Compose开发环境 | pm_project | 1天 | - |

### 阶段二：适配器开发 (第3-4周)

| 任务 | 负责人 | 工期 | 依赖 |
|------|--------|------|------|
| CCC适配器 | pm_project | 3天 | Adapter框架 |
| ICCOA适配器 | pm_project | 3天 | Adapter框架 |
| ICCE适配器 | pm_project | 2天 | Adapter框架 |
| 第一个车企适配器Demo | pm_project | 5天 | 车企API文档 |

### 阶段三：集成测试 (第5-6周)

| 任务 | 负责人 | 工期 | 依赖 |
|------|--------|------|------|
| 接口联调 | pm_project | 2天 | 各适配器 |
| 性能测试 | pm_project | 2天 | 服务完成 |
| 安全测试 | pm_project | 2天 | 服务完成 |

---

## 9. 文档清单

| 文档 | 路径 | 状态 |
|------|------|------|
| 系统架构设计 | `architecture/SYSTEM_ARCHITECTURE.md` | ✅ |
| 接口协议规范 | `protocol/unified/unified-protocol-spec.md` | ✅ |
| 云端部署文档 | `cloud/docs/` | ⏳ 待补充 |
| Java Adapter开发指南 | `adapters/docs/` | ⏳ 待创建 |
| SDK集成文档 | `docs/sdk-integration.md` | ⏳ 待创建 |

---

**文档版本历史**:
- v1.0 (2026-05-05): 初始架构设计 (纯Go)
- v2.0 (2026-05-06): 升级为Go+Java混合架构
