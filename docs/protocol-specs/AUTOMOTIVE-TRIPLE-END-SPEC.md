# 车云一体化三端通信协议规范

> **版本**: v1.0.0  
> **适用场景**: 车载嵌入式(车端) ↔ 云端(Backend) ↔ 手机 SDK(Frontend)  
> **制定日期**: 2026-05-10

---

## 目录

1. [架构总览](#1-架构总览)
2. [Backend-云端协议](#2-backend-云端协议)
3. [Frontend-手机SDK协议](#3-frontend-手机sdk协议)
4. [Embedded-车端协议](#4-embedded-车端协议)
5. [三端数据一致性](#5-三端数据一致性)
6. [安全与认证](#6-安全与认证)
7. [版本控制](#7-版本控制)

---

## 1. 架构总览

### 1.1 三端定义

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              车云一体化架构                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌──────────────┐         MQTT/HTTPS          ┌───────────────────────┐   │
│   │   Embedded   │  ◄──────────────────────►   │       Backend         │   │
│   │    车端      │    (低功耗/弱网优化)         │       云端            │   │
│   │              │                             │  ┌─────────────────┐  │   │
│   │ • T-Box      │                             │  │   API Gateway   │  │   │
│   │ • IVI中控    │                             │  │   (Kong/AWS)    │  │   │
│   │ • 域控制器    │                             │  └────────┬────────┘  │   │
│   └──────────────┘                             │           │           │   │
│          ▲                                     │  ┌────────▼────────┐  │   │
│          │ CAN/Ethernet                        │  │  Microservices  │  │   │
│          ▼                                     │  │  • User Svc     │  │   │
│   ┌──────────────┐                             │  │  • Device Svc   │  │   │
│   │   Vehicle    │                             │  │  • Data Svc     │  │   │
│   │    车辆      │                             │  │  • Sync Svc     │  │   │
│   │              │                             │  └────────┬────────┘  │   │
│   │ • ECU集群    │                             │           │           │   │
│   │ • 传感器     │                             │  ┌────────▼────────┐  │   │
│   │ • 执行器     │                             │  │   PostgreSQL    │  │   │
│   └──────────────┘                             │  │   + Redis       │  │   │
│                                                │  └─────────────────┘  │   │
│                                                └───────────────────────┘   │
│                                                           ▲                │
│                                                           │ HTTP/2 + gRPC  │
│                                                           ▼                │
│                                                ┌───────────────────────┐   │
│   ┌──────────────┐         HTTP/2              │      Frontend         │   │
│   │   Frontend   │  ◄──────────────────────►   │     手机SDK           │   │
│   │   手机SDK    │    (实时推送/高带宽)         │                       │   │
│   │              │                             │  • Android SDK        │   │
│   │ • Android    │                             │  • iOS SDK            │   │
│   │ • iOS        │                             │  • Flutter Plugin     │   │
│   │ • Flutter    │                             │  • React Native       │   │
│   └──────────────┘                             └───────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 三端职责划分

| 端 | 主要职责 | 网络特点 | 协议选择 |
|----|---------|----------|----------|
| **Backend 云端** | 业务逻辑、数据持久化、消息路由、OTA管理 | 高可用、高并发 | REST/gRPC + MQTT Broker |
| **Frontend 手机SDK** | 用户交互、远程控制、数据展示、消息推送 | 高带宽、有/WiFi | HTTP/2 + WebSocket + FCM/APNs |
| **Embedded 车端** | 数据采集、执行控制、状态上报、离线缓存 | 弱网/间歇连接、低功耗 | MQTT + HTTPS(OTA) |

---

## 2. Backend-云端协议

### 2.1 服务端架构

```yaml
# 微服务划分
services:
  api-gateway:
    - Kong/AWS API Gateway
    - 限流、鉴权、路由
    
  user-service:
    - 用户管理、OAuth2
    - JWT签发与验证
    
  device-service:
    - 设备注册、心跳管理
    - T-Box/IVI生命周期
    
  vehicle-service:
    - 车辆数据、CAN信号
    - 诊断数据、故障码
    
  sync-service:
    - 三端数据同步
    - 冲突解决、版本控制
    
  notification-service:
    - 推送服务集成
    - FCM/APNs/MQTT
    
  ota-service:
    - 固件管理、差分升级
    - 升级策略、回滚机制
```

### 2.2 数据库 Schema (车云场景)

```sql
-- 用户表 (扩展)
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    phone VARCHAR(20) UNIQUE,           -- 手机号登录
    email VARCHAR(254),
    password_hash VARCHAR(255),
    name VARCHAR(50),
    avatar_url VARCHAR(500),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 车辆表
CREATE TABLE vehicles (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vin VARCHAR(17) UNIQUE NOT NULL,    -- 车架号
    user_id UUID REFERENCES users(id),
    model VARCHAR(50),                  -- 车型
    brand VARCHAR(30),                  -- 品牌
    license_plate VARCHAR(20),          -- 车牌号
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 设备表 (T-Box/IVI)
CREATE TABLE devices (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vehicle_id UUID REFERENCES vehicles(id),
    device_type VARCHAR(20) NOT NULL,   -- 'tbox', 'ivi', 'adas'
    device_sn VARCHAR(50) UNIQUE,       -- 设备序列号
    firmware_version VARCHAR(30),
    hardware_version VARCHAR(30),
    status VARCHAR(20) DEFAULT 'offline', -- online/offline/sleep
    last_heartbeat_at TIMESTAMPTZ,
    ip_address INET,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 遥测数据表 (时序数据，建议用TimescaleDB)
CREATE TABLE telemetry (
    time TIMESTAMPTZ NOT NULL,
    device_id UUID NOT NULL,
    vehicle_id UUID NOT NULL,
    
    -- 位置
    latitude DOUBLE PRECISION,
    longitude DOUBLE PRECISION,
    altitude DOUBLE PRECISION,
    speed DOUBLE PRECISION,             -- km/h
    heading INTEGER,                    -- 方向角
    
    -- 车辆状态
    ignition BOOLEAN,                   -- 点火状态
    mileage BIGINT,                     -- 总里程
    fuel_level INTEGER,                 -- 油量百分比
    battery_voltage DECIMAL(4,2),       -- 电瓶电压
    
    -- 告警
    alarm_codes INTEGER[],              -- 告警码列表
    
    PRIMARY KEY (time, device_id)
);

-- 选择 TimescaleDB  hypertable
SELECT create_hypertable('telemetry', 'time', chunk_time_interval => INTERVAL '1 day');

-- CAN信号原始数据表
CREATE TABLE can_signals (
    time TIMESTAMPTZ NOT NULL,
    device_id UUID NOT NULL,
    signal_id VARCHAR(50) NOT NULL,     -- 信号ID
    signal_name VARCHAR(100),           -- 信号名称
    value DOUBLE PRECISION,             -- 信号值
    unit VARCHAR(20),                   -- 单位
    raw_hex VARCHAR(20),                -- 原始HEX
    
    PRIMARY KEY (time, device_id, signal_id)
);

-- OTA任务表
CREATE TABLE ota_tasks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    device_id UUID REFERENCES devices(id),
    firmware_version VARCHAR(30) NOT NULL,
    package_url VARCHAR(500) NOT NULL,
    package_size BIGINT,
    package_hash VARCHAR(64),           -- SHA256
    status VARCHAR(20) DEFAULT 'pending', -- pending/downloading/installing/completed/failed
    progress INTEGER DEFAULT 0,         -- 0-100
    created_at TIMESTAMPTZ DEFAULT NOW(),
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ,
    error_code VARCHAR(50),
    error_message TEXT
);

-- 同步版本表 (多端同步用)
CREATE TABLE sync_versions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    entity_type VARCHAR(30) NOT NULL,   -- 'vehicle', 'device', 'config'
    entity_id UUID NOT NULL,
    client_type VARCHAR(20) NOT NULL,   -- 'embedded', 'mobile', 'web'
    version INTEGER NOT NULL,
    data JSONB,
    synced_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(entity_type, entity_id, client_type)
);
```

### 2.3 REST API 设计 (Backend ↔ Frontend)

#### 车辆管理
```yaml
GET    /api/v1/vehicles              # 获取用户车辆列表
GET    /api/v1/vehicles/:id          # 获取车辆详情
GET    /api/v1/vehicles/:id/status   # 获取实时状态
GET    /api/v1/vehicles/:id/location # 获取最新位置
GET    /api/v1/vehicles/:id/history  # 获取历史轨迹
POST   /api/v1/vehicles/:id/bind     # 绑定车辆
POST   /api/v1/vehicles/:id/unbind   # 解绑车辆
```

#### 设备管理
```yaml
GET    /api/v1/devices               # 获取设备列表
GET    /api/v1/devices/:id           # 获取设备详情
POST   /api/v1/devices/:id/command   # 下发指令
GET    /api/v1/devices/:id/telemetry # 获取遥测数据
GET    /api/v1/devices/:id/diagnostics # 获取诊断信息
```

#### 远程控制 (手机→云端→车端)
```yaml
POST   /api/v1/vehicles/:id/commands/unlock     # 车门解锁
POST   /api/v1/vehicles/:id/commands/lock       # 车门上锁
POST   /api/v1/vehicles/:id/commands/start      # 远程启动
POST   /api/v1/vehicles/:id/commands/stop       # 远程熄火
POST   /api/v1/vehicles/:id/commands/climate    # 空调控制
POST   /api/v1/vehicles/:id/commands/flash      # 寻车闪灯
POST   /api/v1/vehicles/:id/commands/honk       # 寻车鸣笛

# 指令响应
Response: {
  "data": {
    "commandId": "cmd-uuid",
    "status": "pending",      # pending/delivered/executed/failed
    "timeout": 30000,         # 超时时间(ms)
    "estimatedTime": 5000     # 预计执行时间(ms)
  }
}
```

### 2.4 gRPC 服务定义 (Backend 内部/高吞吐场景)

```protobuf
// vehicle.proto
syntax = "proto3";
package automotive;

service VehicleService {
  // 流式遥测数据上传 (车端→云端)
  rpc StreamTelemetry(stream TelemetryPacket) returns (Ack);
  
  // 双向指令通道 (云端↔车端)
  rpc CommandChannel(stream CommandRequest) returns (stream CommandResponse);
  
  // 批量历史数据查询
  rpc GetTelemetryHistory(HistoryRequest) returns (stream TelemetryPacket);
}

message TelemetryPacket {
  string device_id = 1;
  int64 timestamp = 2;
  Position position = 3;
  VehicleStatus status = 4;
  repeated CANSignal can_signals = 5;
}

message Position {
  double latitude = 1;
  double longitude = 2;
  double altitude = 3;
  double speed = 4;
  int32 heading = 5;
  int32 accuracy = 6;  // GPS精度
}

message VehicleStatus {
  bool ignition = 1;
  int64 mileage = 2;
  int32 fuel_level = 3;
  float battery_voltage = 4;
  int32 engine_temp = 5;
}

message CANSignal {
  string signal_id = 1;
  string name = 2;
  double value = 3;
  string unit = 4;
}

message CommandRequest {
  string command_id = 1;
  string device_id = 2;
  string action = 3;       # unlock/lock/start/stop/climate
  bytes payload = 4;       # 指令参数
  int32 timeout_ms = 5;
}

message CommandResponse {
  string command_id = 1;
  string status = 2;       # received/executed/failed/timeout
  string error_code = 3;
  bytes result = 4;
  int64 timestamp = 5;
}
```

---

## 3. Frontend-手机SDK协议

### 3.1 SDK 架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Frontend SDK (Android/iOS)                │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐  │
│  │                  Application Layer                    │  │
│  │  • VehicleManager    • RemoteControl    • DataSync    │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                  Service Layer                        │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │  │
│  │  │  HTTP/2     │  │  WebSocket  │  │   Push       │ │  │
│  │  │  Client     │  │  Client     │  │   Receiver   │ │  │
│  │  └─────────────┘  └─────────────┘  └──────────────┘ │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                  Data Layer                           │  │
│  │  • Local Cache (Room/CoreData)  • Sync Queue          │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 手机 SDK 数据结构

```typescript
// Android (Kotlin) / iOS (Swift) 对应定义

interface Vehicle {
  id: string;
  vin: string;
  brand: string;
  model: string;
  licensePlate?: string;
  devices: Device[];
  status: VehicleStatus;
  location?: Location;
  lastUpdateTime: string;
}

interface Device {
  id: string;
  type: 'tbox' | 'ivi' | 'adas';
  serialNumber: string;
  firmwareVersion: string;
  status: 'online' | 'offline' | 'sleep';
  lastHeartbeatAt?: string;
}

interface VehicleStatus {
  ignition: boolean;
  locked: boolean;
  mileage: number;
  fuelLevel: number;
  batteryVoltage: number;
  engineTemperature?: number;
  tirePressure?: TirePressure[];
  doorStatus?: DoorStatus;
  windowStatus?: WindowStatus;
}

interface Location {
  latitude: number;
  longitude: number;
  altitude?: number;
  speed?: number;
  heading?: number;
  accuracy?: number;
  timestamp: string;
}

interface RemoteCommand {
  id: string;
  vehicleId: string;
  action: 'unlock' | 'lock' | 'start' | 'stop' | 'climate' | 'flash' | 'honk';
  params?: Record<string, any>;
  status: 'pending' | 'delivered' | 'executed' | 'failed' | 'timeout';
  createdAt: string;
  completedAt?: string;
  errorCode?: string;
  errorMessage?: string;
}

// 推送消息
interface PushNotification {
  type: 'command_result' | 'alert' | 'ota' | 'vehicle_status';
  vehicleId: string;
  payload: any;
  timestamp: string;
}
```

### 3.3 长连接协议 (WebSocket)

```yaml
# 连接建立
Client ──► GET /ws/v1/vehicles/{vehicleId}/stream
         Headers:
           Authorization: Bearer {jwt}
           X-Client-Version: 2.1.0

# 上行 (手机→云端)
{
  "type": "subscribe",
  "topics": ["status", "location", "alerts"]
}

# 下行 (云端→手机)
{
  "type": "vehicle_status",
  "timestamp": "2026-05-10T10:30:00.000Z",
  "data": {
    "vehicleId": "vh-xxx",
    "status": {
      "ignition": true,
      "locked": false,
      "fuelLevel": 65
    }
  }
}

{
  "type": "command_result",
  "commandId": "cmd-xxx",
  "status": "executed",
  "timestamp": "2026-05-10T10:30:05.000Z"
}
```

### 3.4 推送通道 (FCM/APNs)

```yaml
# 离线推送
Title: 车辆告警
Body: 您的车辆(vin: xxx)检测到异常震动
Data:
  type: alert
  vehicleId: vh-xxx
  alertCode: 1001
  alertLevel: warning

# OTA推送
Title: 系统更新
Body: 车辆有新的固件版本可用
Data:
  type: ota
  vehicleId: vh-xxx
  version: 2.5.0
  size: 256MB
```

---

## 4. Embedded-车端协议

### 4.1 车端架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        车辆电子电气架构                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐ │
│   │    IVI      │    │   T-Box     │    │    Domain Ctrl      │ │
│   │   (中控)    │◄──►│  (通信盒)   │◄──►│   (域控制器)         │ │
│   │             │    │             │    │                     │ │
│   │ • Android   │    │ • 4G/5G     │    │ • ADAS             │ │
│   │ • Linux     │    │ • GPS       │    │ • Chassis          │ │
│   │ • Qt HMI    │    │ • WiFi/BT   │    │ • Powertrain       │ │
│   └─────────────┘    │ • MCU       │    └─────────────────────┘ │
│          ▲           └──────┬──────┘              ▲              │
│          │                  │                     │              │
│          └──────────────────┴─────────────────────┘              │
│                      CAN/Ethernet                                │
│                          ▼                                       │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                    ECU 网络                              │   │
│   │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ │   │
│   │  │ Engine │ │  ABS   │ │  HVAC  │ │  BCM   │ │  TPMS  │ │   │
│   │  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 车端软件架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Embedded System (Linux/QNX)               │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐  │
│  │                 Application Layer                     │  │
│  │  • HMI Display    • Data Logger    • OTA Agent       │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                 Middleware Layer                      │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐   │  │
│  │  │  MQTT    │  │  SOME/IP│  │   Data Sync      │   │  │
│  │  │  Client  │  │  Service│  │   Engine         │   │  │
│  │  └──────────┘  └──────────┘  └──────────────────┘   │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                  HAL Layer                            │  │
│  │  • CAN Driver    • GPS Driver    • 4G Modem Driver   │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                  OS Layer                             │  │
│  │  • Linux/QNX     • Device Tree   • BSP               │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 4.3 MQTT 协议 (车端↔云端)

#### Topic 设计

```yaml
# 车端→云端 (上报)
vehicle/{vin}/telemetry/location       # GPS位置 (1-10s间隔)
vehicle/{vin}/telemetry/status         # 车辆状态 (变化上报)
vehicle/{vin}/telemetry/can            # CAN信号 (实时/批量)
vehicle/{vin}/system/heartbeat         # 心跳 (30s间隔)
vehicle/{vin}/system/alerts            # 告警事件
vehicle/{vin}/ota/status               # OTA状态上报
vehicle/{vin}/diagnostics/faults       # 故障码
vehicle/{vin}/diagnostics/log          # 诊断日志

# 云端→车端 (指令)
cloud/{vin}/commands/remote            # 远程控制指令
cloud/{vin}/commands/config            # 配置更新
cloud/{vin}/ota/request                # OTA触发
cloud/{vin}/sync/request               # 数据同步请求

# 车端↔云端 (响应)
vehicle/{vin}/commands/response        # 指令响应
```

#### MQTT QoS 策略

| Topic | QoS | Retain | 说明 |
|-------|-----|--------|------|
| telemetry/location | 0 | No | 高频数据，允许丢失 |
| telemetry/status | 1 | Yes | 状态需可靠传输，保留最新值 |
| system/alerts | 2 | No | 告警必须送达 |
| commands/remote | 1 | No | 指令需确认 |

#### MQTT Payload 格式

```json
// 位置上报 (telemetry/location)
{
  "ts": 1715410200000,
  "lat": 31.230416,
  "lng": 121.473701,
  "alt": 12.5,
  "spd": 45.2,
  "hdg": 90,
  "acc": 5,
  "src": "gps"
}

// 车辆状态 (telemetry/status)
{
  "ts": 1715410200000,
  "ign": true,
  "lock": true,
  "mileage": 12345678,
  "fuel": 65,
  "bat": 12.4,
  "engTemp": 85
}

// 心跳 (system/heartbeat)
{
  "ts": 1715410200000,
  "fwVer": "2.5.1",
  "hwVer": "1.0",
  "uptime": 86400,
  "rssi": -85,
  "netType": "4G"
}

// 告警 (system/alerts)
{
  "ts": 1715410200000,
  "code": 1001,
  "level": "warning",
  "name": "碰撞检测",
  "desc": "检测到车辆异常震动",
  "data": {
    "accX": 2.5,
    "accY": -1.2,
    "accZ": 0.8
  }
}

// 远程指令 (云端下发)
{
  "cmdId": "cmd-uuid-xxx",
  "action": "unlock",
  "ts": 1715410200000,
  "timeout": 30000,
  "params": {}
}

// 指令响应 (车端回复)
{
  "cmdId": "cmd-uuid-xxx",
  "status": "executed",
  "ts": 1715410203000,
  "result": {
    "doorStatus": "unlocked"
  }
}
```

### 4.4 车端离线缓存机制

```c
// 车端数据队列结构 (C语言伪代码)
typedef struct {
    uint32_t timestamp;
    uint8_t data_type;      // LOCATION, STATUS, CAN, ALERT
    uint16_t payload_len;
    uint8_t payload[2048];
    uint8_t retry_count;
} DataQueueItem;

// 队列管理
typedef struct {
    DataQueueItem *buffer;
    uint16_t head;
    uint16_t tail;
    uint16_t capacity;
    uint16_t count;
    pthread_mutex_t lock;
} DataQueue;

// 网络状态检测
NetworkState check_network(void) {
    if (modem.rssi < -100) return NET_WEAK;
    if (!mqtt.connected) return NET_DISCONNECTED;
    return NET_OK;
}

// 数据上报策略
void enqueue_telemetry(DataType type, void *data, uint16_t len) {
    NetworkState net = check_network();
    
    switch (net) {
        case NET_OK:
            // 网络正常：实时发送
            if (!mqtt_publish(type, data, len)) {
                // 发送失败，入队
                queue_push(&offline_queue, type, data, len);
            }
            break;
            
        case NET_WEAK:
            // 弱网：优先级数据实时发送，其他批量缓存
            if (type == ALERT || type == CMD_RESPONSE) {
                mqtt_publish_qos1(type, data, len); // 高QoS
            } else {
                queue_push(&offline_queue, type, data, len);
            }
            break;
            
        case NET_DISCONNECTED:
            // 离线：全部入队，压缩存储
            if (type == LOCATION) {
                // 位置数据降采样
                if (should_downsample()) return;
            }
            queue_push(&offline_queue, type, data, len);
            break;
    }
}

// 网络恢复后批量上传
void flush_offline_queue(void) {
    while (!queue_empty(&offline_queue)) {
        DataQueueItem *item = queue_pop(&offline_queue);
        
        // 批量打包上传
        if (batch_buffer_size + item->payload_len > MAX_BATCH_SIZE) {
            mqtt_publish_batch(batch_buffer, batch_buffer_size);
            batch_buffer_size = 0;
        }
        
        memcpy(batch_buffer + batch_buffer_size, item, item->payload_len);
        batch_buffer_size += item->payload_len;
        
        // 删除已确认数据
        queue_commit_pop(&offline_queue);
    }
}
```

### 4.5 OTA 升级协议

```yaml
# 1. 版本检测
车端 ──► PUBLISH vehicle/{vin}/ota/check
         { "currentVersion": "2.5.1", "hardwareVersion": "1.0" }

云端 ──► PUBLISH cloud/{vin}/ota/available
         { 
           "available": true,
           "version": "2.5.2",
           "size": 134217728,
           "hash": "sha256:xxx",
           "url": "https://ota.example.com/firmware/2.5.2.bin",
           "changelog": "修复了...",
           "force": false
         }

# 2. 下载阶段
车端 ──► HTTPS GET https://ota.example.com/firmware/2.5.2.bin
         (断点续传，Range header)

车端 ──► PUBLISH vehicle/{vin}/ota/progress
         { "stage": "downloading", "progress": 45 }

# 3. 安装阶段
车端 ──► PUBLISH vehicle/{vin}/ota/progress
         { "stage": "installing", "progress": 0 }

# 4. 完成/回滚
车端 ──► PUBLISH vehicle/{vin}/ota/result
         { "status": "success", "newVersion": "2.5.2" }
         
# 失败时
车端 ──► PUBLISH vehicle/{vin}/ota/result
         { "status": "failed", "errorCode": "E001", "rolledBack": true }
```

---

## 5. 三端数据一致性

### 5.1 数据同步矩阵

| 数据类型 | 车端→云端 | 云端→车端 | 手机→云端 | 云端→手机 | 冲突策略 |
|---------|-----------|-----------|-----------|-----------|----------|
| 车辆状态 | ✅实时 | ❌ | ❌ | ✅推送 | 车端优先 |
| 位置轨迹 | ✅实时 | ❌ | ❌ | ✅查询 | 车端优先 |
| 远程指令 | ❌ | ✅指令 | ✅发起 | ✅结果 | 时间戳 |
| 告警记录 | ✅实时 | ❌ | ❌ | ✅推送 | 车端优先 |
| 配置参数 | ❌ | ✅下发 | ❌ | ❌ | 云端优先 |
| OTA状态 | ✅上报 | ✅触发 | ❌ | ✅通知 | 云端优先 |
| 用户设置 | ❌ | ✅同步 | ✅修改 | ✅同步 | 时间戳 |

### 5.2 冲突解决机制

```typescript
// 乐观锁 + 向量时钟
interface SyncMetadata {
  version: number;           // 单调递增版本
  vectorClock: {
    embedded: number;        // 车端时钟
    backend: number;         // 云端时钟
    frontend: number;        // 手机时钟
  };
  timestamp: string;         // ISO 8601
  clientType: 'embedded' | 'backend' | 'frontend';
}

// 冲突检测
function detectConflict(
  local: SyncMetadata,
  remote: SyncMetadata
): ConflictType {
  // 1. 版本号相同：无冲突
  if (local.version === remote.version) {
    return ConflictType.NONE;
  }
  
  // 2. 向量时钟比较
  const cmp = compareVectorClocks(local.vectorClock, remote.vectorClock);
  
  if (cmp === 0) {
    // 并发修改，需要冲突解决
    return ConflictType.CONCURRENT;
  } else if (cmp < 0) {
    // 本地落后
    return ConflictType.LOCAL_BEHIND;
  } else {
    // 远程落后
    return ConflictType.REMOTE_BEHIND;
  }
}

// 冲突解决策略
function resolveConflict(
  local: Data,
  remote: Data,
  type: ConflictType
): ResolvedData {
  switch (type) {
    case ConflictType.CONCURRENT:
      // 根据数据类型选择策略
      if (local.dataType === 'vehicle_status') {
        // 车端状态以车端为准
        return { data: local, winner: 'embedded' };
      } else if (local.dataType === 'user_settings') {
        // 用户设置以时间戳最新为准
        return local.timestamp > remote.timestamp 
          ? { data: local, winner: 'local' }
          : { data: remote, winner: 'remote' };
      }
      break;
      
    case ConflictType.LOCAL_BEHIND:
      return { data: remote, winner: 'remote' };
      
    case ConflictType.REMOTE_BEHIND:
      return { data: local, winner: 'local' };
  }
}
```

### 5.3 同步时序图

```
车端(Embedded)          云端(Backend)          手机(Frontend)
    │                        │                        │
    │ 1. 状态变更              │                        │
    │ ─────────────────────► │                        │
    │  MQTT: status update   │                        │
    │                        │ 2. 存储并触发推送         │
    │                        │ ─────────────────────► │
    │                        │   FCM/APNs Push        │
    │                        │                        │
    │◄───────────────────────│ 3. 手机查询状态          │
    │                        │◄───────────────────────│
    │                        │   HTTP GET /status     │
    │                        │                        │
    │◄───────────────────────│ 4. 返回最新状态          │
    │                        │───────────────────────►│
    │                        │                        │
    │◄───────────────────────│ 5. 下发远程指令          │
    │                        │◄───────────────────────│
    │   MQTT: command        │   POST /commands/lock  │
    │                        │                        │
    │ 6. 执行指令              │                        │
    │ (CAN bus操作)           │                        │
    │                        │                        │
    │ 7. 指令结果              │                        │
    │ ─────────────────────► │                        │
    │   MQTT: response       │                        │
    │                        │ 8. 推送给手机            │
    │                        │ ─────────────────────► │
    │                        │   WebSocket/FCM        │
    │                        │                        │
```

---

## 6. 安全与认证

### 6.1 车端设备认证

```yaml
# PKI 证书链
Root CA (OEM)
  └── Intermediate CA
      └── Device Certificate (T-Box/IVI)
          • 设备序列号
          • VIN绑定
          • 有效期: 10年

# MQTT TLS 连接
车端 ──► CONNECT broker.ota.example.com:8883
         TLS: 双向认证
         Client Cert: device.crt
         Client Key: device.key
         CA Cert: root-ca.crt
         
# 首次激活流程
1. 车端生成密钥对
2. 产线注入证书 (HSM安全芯片)
3. 云端验证证书链 + VIN绑定
4. 颁发JWT访问令牌 (有效期24h)
5. MQTT连接使用证书 + JWT
```

### 6.2 手机SDK认证

```yaml
# OAuth2 + PKCE 流程
1. 手机App打开授权页
2. 用户登录 -> 获取授权码
3. 交换Access Token + Refresh Token
4. API请求: Authorization: Bearer {access_token}
5. Token过期 -> 使用Refresh Token续期

# 远程指令额外验证
POST /api/v1/vehicles/:vin/commands/unlock
Headers:
  Authorization: Bearer {jwt}
  X-Request-Signature: HMAC-SHA256(时间戳+指令+密钥)
  X-Timestamp: 1715410200000

# 防重放攻击
云端验证:
  - 时间戳在 ±5分钟窗口内
  - Signature未使用过
  - HMAC签名正确
```

### 6.3 数据传输加密

| 通道 | 协议 | 加密方式 | 证书管理 |
|------|------|----------|----------|
| 车端↔云端 | MQTT | TLS 1.3 + mTLS | X.509设备证书 |
| 车端↔云端 | HTTPS(OTA) | TLS 1.3 + 证书固定 | 内置根证书 |
| 手机↔云端 | HTTP/2 | TLS 1.3 | 标准CA证书 |
| 手机↔云端 | WebSocket | WSS (TLS) | 标准CA证书 |

### 6.4 指令安全级别

| 指令 | 安全级别 | 验证方式 |
|------|---------|----------|
| 查询状态 | Level 1 | JWT验证 |
| 远程解锁 | Level 2 | JWT + 短信验证码 |
| 远程启动 | Level 3 | JWT + 短信 + 生物识别 |
| OTA升级 | Level 4 | JWT + 证书签名 + 回滚策略 |

---

## 7. 版本控制

### 7.1 协议版本矩阵

| 端 | 当前版本 | 支持版本 | 废弃版本 |
|----|---------|---------|---------|
| Backend API | v2.1.0 | v2.x, v1.x | v0.x |
| Frontend SDK | v2.1.0 | v2.x | v1.x |
| Embedded OTA | v2.5.2 | v2.5.x, v2.4.x | v2.3.x |
| MQTT Protocol | v1.2.0 | v1.x | - |

### 7.2 版本协商机制

#### 7.2.1 协商时序图

```
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│                              版本协商完整流程                                              │
├───────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                              │
│   ┌───────────────┐    Step 1     ┌─────────────────────┐    Step 2     ┌───────────────┐    │
│   │    Client      │  ────────►  │      Gateway       │  ────────►  │   Backend    │    │
│   │  (车端/SDK)   │   版本检测    │    (云端网关)    │   版本匹配    │   (服务)    │    │
│   │                 │              │                   │              │              │    │
│   └─────┬──────────┘              └─────┬────────────────┘              └─────┬────────────┘    │
│         │                                │                                  │             │
│         │  1.1 携带版本信息请求              │                                  │             │
│         │  ───────────────────────────►  │                                  │             │
│         │  Accept-Version: v2                │                                  │             │
│         │  X-Client-Type: embedded          │                                  │             │
│         │  X-Client-Version: 2.5.2           │                                  │             │
│         │  X-Protocol-Version: 1.2.0         │                                  │             │
│         │                                │                                  │             │
│         │                                │  1.2 解析并转发版本信息              │             │
│         │                                │  ───────────────────────────►  │             │
│         │                                │                                  │             │
│         │                                │                                  │  1.3 版本检查 │
│         │                                │                                  │             │
│         │                                │         ┌────────────────┐        │             │
│         │                                │    ◄────┤  版本匹配库   │────────────┤        │             │
│         │                                │         └────────────────┘        │             │
│         │                                │                                  │             │
│   Step 4 │                                │           Step 3 (版本兼容)             │             │
│         │                                │  ───────────────────────────────────────▴        │             │
│         │  ┌────────────────┐                   │                                  │             │
│         │◄─┤  正常响应   │─────────────────────────────────────┤                                  │             │
│         │  │  200 OK         │                   │                                  │             │
│         │  │  API-Version     │                   │                                  │             │
│         │  │  Sunset          │                   │                                  │             │
│         │  └────────────────┘                   │                                  │             │
│         │                                │                                  │             │
│         │  ┌────────────────┐                   │                                  │             │
│         │◄─┤  不兼容响应   │───────────────────────────────────┤                                  │             │
│         │  │  406/410         │                   │                                  │             │
│         │  │  升级指引        │                   │                                  │             │
│         │  └────────────────┘                   │                                  │             │
│         │                                │                                  │             │
│         │  ┌────────────────┐                   │                                  │             │
│         │◄─┤  协商降级    │───────────────────────────────────┤                                  │             │
│         │  │  300 Multiple   │                   │                                  │             │
│         │  │  Choices        │                   │                                  │             │
│         │  └────────────────┘                   │                                  │             │
│         │                                │                                  │             │
└───────────────────────────────────────────────────────────────────────────────────────────┘
```

#### 7.2.2 HTTP API 版本协商详解

```yaml
# 【正常请求 - 版本匹配】
Request:
  GET /api/v2/vehicles/{vin}/status
  Headers:
    Accept: application/json
    Accept-Version: v2           # 主版本号 (Major)
    X-API-Version: 2.1.0         # 客户端支持的精确版本
    X-SDK-Version: 2.1.0         # SDK版本
    X-Client-Type: mobile        # 客户端类型: mobile/embedded/web
    X-OS-Version: Android-14     # 操作系统版本
    X-Device-Model: SM-G998U     # 设备型号
    Accept-Language: zh-CN       # 语言偏好

Response:
  HTTP/1.1 200 OK
  Headers:
    Content-Type: application/json
    API-Version: 2.1.3           # 服务端实际版本 (SemVer)
    Supported-Versions: v1, v2   # 支持的主版本列表
    Minimum-Version: 2.0.0       # 最低要求版本
    Deprecation: false           # 是否已废弃
    Sunset:                      # 废弃预告 (RFC 8594)
    Links: </api/v3/vehicles/{vin}/status>; rel="successor-version"  # 后续版本
  
  Body:
    {
      "data": { ... },
      "meta": {
        "version": "2.1.3",
        "requestId": "req-uuid",
        "timestamp": "2026-05-10T10:00:00.000Z"
      }
    }

# 【版本过低 - 需要升级】
Response:
  HTTP/1.1 406 Not Acceptable
  Headers:
    API-Version: 2.1.3
    Minimum-Version: 2.0.0
    Sunset: Sun, 01 Nov 2026 00:00:00 GMT
  
  Body:
    {
      "error": {
        "code": "VERSION_NOT_SUPPORTED",
        "message": "Client version 1.5.0 is no longer supported",
        "details": {
          "currentVersion": "1.5.0",
          "minimumVersion": "2.0.0",
          "latestVersion": "2.1.3",
          "supportedVersions": ["v2"],
          "upgradeUrl": "https://app.example.com/upgrade",
          "forceUpgrade": true,
          "gracePeriodDays": 0
        }
      }
    }

# 【版本已废弃 - 服务终止】
Response:
  HTTP/1.1 410 Gone
  Headers:
    API-Version: 2.1.3
    Deprecation: true
    Deprecation-Date: Sun, 01 Jun 2026 00:00:00 GMT
  
  Body:
    {
      "error": {
        "code": "VERSION_DEPRECATED",
        "message": "API version v1 has been retired",
        "details": {
          "retiredVersion": "v1",
          "retirementDate": "2026-05-01T00:00:00Z",
          "migrationGuide": "https://docs.example.com/migration/v1-to-v2",
          "supportContact": "support@example.com"
        }
      }
    }

# 【协商降级 - 服务可用但功能受限】
Response:
  HTTP/1.1 300 Multiple Choices
  Headers:
    API-Version: 2.1.3
  
  Body:
    {
      "alternatives": [
        {
          "version": "v2",
          "url": "/api/v2/vehicles/{vin}/status",
          "features": ["full"],
          "stable": true
        },
        {
          "version": "v1",
          "url": "/api/v1/vehicles/{vin}/status",
          "features": ["read-only"],
          "sunset": "2026-11-01T00:00:00Z",
          "warning": "Write operations will be rejected"
        }
      ]
    }
```

#### 7.2.3 MQTT 版本协商 (车端↔云端)

```yaml
# 连接阶段版本协商
CONNECT:
  clientId: "vin-LSVAG2180E2100001"
  protocolName: "MQTT"
  protocolVersion: 5              # MQTT v5 支持用户属性
  cleanSession: false
  keepAlive: 30
  
  # MQTT v5 用户属性用于版本协商
  userProperties:
    clientType: "embedded"        # 客户端类型
    deviceType: "tbox"            # 设备类型
    firmwareVersion: "2.5.2"      # 固件版本 (SemVer)
    hardwareVersion: "1.0"        # 硬件版本
    protocolVersion: "1.2.0"      # 业务协议版本
    supportedProtocolVersions: "1.1.0,1.2.0,1.3.0-beta"
    vehicleModel: "Model-X-Pro"
    manufactureDate: "2026-01"
    
  willTopic: "vehicle/LSVAG2180E2100001/system/offline"
  willMessage: '{"ts":0,"reason":"unexpected"}'
  willQos: 1
  willRetain: false

# 连接响应
CONNACK:
  sessionPresent: false
  reasonCode: 0x00                # Success
  
  userProperties:
    serverProtocolVersion: "1.2.0"     # 服务端协议版本
    minimumProtocolVersion: "1.1.0"    # 最低支持版本
    serverTime: "2026-05-10T10:00:00Z"
    sessionExpiry: 3600                # 会话过期秒数
    
  # 版本不兼容时
  reasonCode: 0x84                # Unsupported Protocol Version
  reasonString: "Firmware 2.3.x is deprecated, please upgrade to 2.5.x"
  userProperties:
    requiredVersion: "2.5.0"
    otaUrl: "https://ota.example.com/firmware/2.5.2.bin"
    supportUrl: "https://support.example.com/ota"
```

#### 7.2.4 版本冲突解决策略

```typescript
// 版本匹配算法
interface VersionCheckResult {
  compatible: boolean;           // 是否兼容
  action: 'allow' | 'deny' | 'negotiate' | 'warn';
  serverVersion: string;         // 服务端版本
  effectiveVersion: string;      // 实际使用版本
  warnings?: string[];           // 警告信息
  sunsetInfo?: {
    date: string;
    daysRemaining: number;
  };
}

class VersionNegotiator {
  private serverVersion: string = "2.1.3";
  private minimumVersion: string = "2.0.0";
  private deprecatedVersions: string[] = ["1.x"];
  private supportedMajors: number[] = [1, 2];
  
  negotiate(
    clientVersion: string,
    acceptVersion: string,
    clientType: 'mobile' | 'embedded' | 'web'
  ): VersionCheckResult {
    const client = this.parseVersion(clientVersion);
    const requested = this.parseVersion(acceptVersion);
    const server = this.parseVersion(this.serverVersion);
    
    // 1. 检查版本是否已废弃
    if (this.isDeprecated(clientVersion)) {
      return {
        compatible: false,
        action: 'deny',
        serverVersion: this.serverVersion,
        effectiveVersion: null,
        warnings: [`Version ${clientVersion} has been deprecated`]
      };
    }
    
    // 2. 检查版本是否过低
    if (this.compareVersions(clientVersion, this.minimumVersion) < 0) {
      return {
        compatible: false,
        action: 'deny',
        serverVersion: this.serverVersion,
        effectiveVersion: null,
        warnings: [
          `Version ${clientVersion} is below minimum supported ${this.minimumVersion}`,
          `Force upgrade required`
        ]
      };
    }
    
    // 3. 检查请求的主版本是否支持
    if (!this.supportedMajors.includes(requested.major)) {
      // 尝试协商到支持的版本
      const negotiatedMajor = this.findNearestMajor(requested.major);
      return {
        compatible: true,
        action: 'negotiate',
        serverVersion: this.serverVersion,
        effectiveVersion: `${negotiatedMajor}.0.0`,
        warnings: [`Requested version v${requested.major} not supported, using v${negotiatedMajor}`]
      };
    }
    
    // 4. 检查是否即将废弃
    const sunsetInfo = this.getSunsetInfo(clientVersion);
    if (sunsetInfo && sunsetInfo.daysRemaining < 90) {
      return {
        compatible: true,
        action: 'warn',
        serverVersion: this.serverVersion,
        effectiveVersion: clientVersion,
        warnings: [`Version ${clientVersion} will be deprecated in ${sunsetInfo.daysRemaining} days`],
        sunsetInfo
      };
    }
    
    // 5. 完全兼容
    return {
      compatible: true,
      action: 'allow',
      serverVersion: this.serverVersion,
      effectiveVersion: clientVersion
    };
  }
  
  // SemVer 解析
  private parseVersion(version: string): { major: number; minor: number; patch: number } {
    const [major, minor, patch] = version.replace('v', '').split('.').map(Number);
    return { major, minor, patch: patch || 0 };
  }
  
  // 版本比较 (-1: v1<v2, 0: v1=v2, 1: v1>v2)
  private compareVersions(v1: string, v2: string): number {
    const a = this.parseVersion(v1);
    const b = this.parseVersion(v2);
    
    if (a.major !== b.major) return a.major > b.major ? 1 : -1;
    if (a.minor !== b.minor) return a.minor > b.minor ? 1 : -1;
    if (a.patch !== b.patch) return a.patch > b.patch ? 1 : -1;
    return 0;
  }
}
```

### 7.3 版本生命周期 (Version Lifecycle)

#### 7.3.1 T-90/60/30/0 退役时间表

```
┌───────────────────────────────────────────────────────────────────────────────────────┐
│                        API版本生命周期 (v1 → v2 过渡)                             │
├───────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                              │
│   v1.0 发布                    v2.0 发布                    v1 废弃                    │
│      │                            │                            │                           │
│      ▼                            ▼                            ▼                           │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐   │
│  │                                T-90    T-60    T-30    T-0                            │   │
│  │ T-180  ───────────────────────────────────────────────────────────────┘   │
│  │   │                            │       │       │       │                               │
│  │   ▼                            ▼       ▼       ▼       ▼                               │
│  │  ┌───────────┐              ┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐                              │
│  │  │ v1 生命期   │              │ 发布通告  │    │ 添加警告  │    │ 更新文档  │    │ v1 关闭  │                              │
│  │  │           │              │         │    │ 头中止新  │    │ 移除支持  │    │ 强制升级  │                              │
│  │  │           │              │         │    │ 用户注册  │    │         │    │         │                              │
│  │  └────┬────┘              └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘                              │
│  │     │                       │            │            │            │                              │
│  │     └───────────────────────────┼───────────┼───────────┼───────────┘                              │
│  │                          │            │            │            │                              │
│  │                          │            │            │            │                              │
│  │  ┌───────────────────────────────────────────────────────────────────────────────────────┘                              │
│  │  v1: 全功能支持  ────────────────────────────────────────────────────────────────────────────────────────┘                              │
│                                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐                                  │
│  │ v2: 开发完成 ────────────────────────────────────────────────────────────────────────────────────────┘                                  │
│                                                                                              │
└─────────────────────────────────────────────────────────────────────────────────────┘

时间节点说明:
• T-180: v1 稳定运行期，v2 开发中
• T-90:  发布 v2，宣布 v1 退役时间表
• T-60:  v1 响应头添加 Sunset 警告
• T-30:  文档更新，停止新用户使用 v1
• T-0:   v1 API 关闭，返回 410 Gone
• T+30:  v1 SDK 停止支持
• T+90:  强制所有剩余用户升级
```

#### 7.3.2 版本退役梯度策略

| 阶段 | 时间 | 云端行为 | 客户端体验 |
|-----|------|----------|-----------|
| **T-90** | 发布日 | • 发布退役通告邮件<br>• 更新API文档<br>• 日志标记即将退役版本 | 引导用户迁移到新版本 |
| **T-60** | 退役前60天 | • 响应头添加 `Sunset`<br>• 返回警告信息<br>• 推送升级通知 | 每次请求都看到即将废弃提示 |
| **T-30** | 退役前30天 | • 终止新用户注册<br>• 更新版本选择器<br>• 发布迁移指南 | 新下载用户自动获取新版本 |
| **T-0** | 退役日 | • v1 API返回410<br>• 重定向到升级页面<br>• 清理历史数据 | 显示"版本已停用"强制升级 |
| **T+30** | 退役后30天 | • 停止v1 SDK支持<br>• 关闭兼容性测试 | 应用市场下架旧版本 |
| **T+90** | 退役后90天 | • 强制退出所有v1会话<br>• 完全移除v1代码 | 剩余用户被强制升级 |

### 7.4 多版本并行策略

#### 7.4.1 蓝绿部署 (Blue/Green Deployment)

```yaml
# 云端部署架构
Load Balancer (Kong/AWS ALB)
    │
    ├──────────────────────────────┐
    ▼                                            ▼
Blue Environment (v2.1.3)                  Green Environment (v2.2.0-beta)
- API Pods: v2.1.3                         - API Pods: v2.2.0
- Traffic: 100%                            - Traffic: 0%
- Status: Production                       - Status: Staging

# 版本路由规则
路由逻辑:
  - Header X-API-Version = "2.2.0-beta"  →  Green
  - Header X-API-Version = "2.1.x"       →  Blue
  - Default                               →  Blue (fallback)

# 渐进切流
Phase 1: 5%  流量到 Green (Canary)
Phase 2: 25% 流量到 Green (内部测试)
Phase 3: 50% 流量到 Green (灰度发布)
Phase 4: 100%流量到 Green (全量)
Rollback: < 30秒切回 Blue
```

#### 7.4.2 版本适配层 (Version Adapter)

```typescript
// 版本适配器模式 - 支持多版本同时运行
class VersionAdapter {
  private adapters: Map<string, VersionHandler> = new Map();
  
  constructor() {
    // 注册各版本处理器
    this.adapters.set('v1', new V1Handler());
    this.adapters.set('v2', new V2Handler());
    this.adapters.set('v3', new V3Handler());
  }
  
  async handle(request: Request, version: string): Promise<Response> {
    const handler = this.adapters.get(version);
    
    if (!handler) {
      // 尝试降级或升级
      const fallbackVersion = this.findCompatibleVersion(version);
      if (fallbackVersion) {
        return this.handle(request, fallbackVersion);
      }
      throw new VersionNotSupportedError(version);
    }
    
    // 数据格式转换
    const normalizedRequest = this.normalizeRequest(request, version);
    
    // 调用处理器
    const result = await handler.process(normalizedRequest);
    
    // 格式转换回客户端版本
    return this.formatResponse(result, version);
  }
  
  // 请求格式转换
  private normalizeRequest(request: Request, fromVersion: string): NormalizedRequest {
    switch(fromVersion) {
      case 'v1':
        return {
          ...request,
          // v1 用下划线命名
          vehicleId: request.vehicle_id,
          timestamp: new Date(request.timestamp).toISOString()
        };
      case 'v2':
        return request; // v2 是标准格式
      default:
        return request;
    }
  }
  
  // 响应格式转换
  private formatResponse(result: any, toVersion: string): Response {
    switch(toVersion) {
      case 'v1':
        return {
          ...result,
          vehicle_id: result.vehicleId,  // 转回下划线
          created_at: result.createdAt,
          is_deleted: result.isDeleted
        };
      case 'v2':
        return result;
    }
  }
}
```

### 7.5 升级/降级策略

#### 7.5.1 升级路径校验

```yaml
# 车端固件升级路径图
Version Graph:
  2.3.0 ──────────────────────────────────────────────────┐
    │                                              │
    │ upgrade                                      │ EOL
    ▼                                              ▼
  2.4.0 ───────────────────────────────────────────────────┘
    │
    │ upgrade (mandatory)
    ▼
  2.4.5 ───────────────────────────────────────────────────┐
    │                                              │
    │ upgrade                                      │ EOL
    ▼                                              ▼
  2.5.0 ───────────────────────────────────────────────────┘
    │
    │ upgrade
    ▼
  2.5.1 ───────────────────────────────────────────────────┐
    │                                              │
    │ upgrade                                      │ Current
    ▼                                              ▼
  2.5.2 ───────────────────────────────────────────────────┘
    │
    │ upgrade (blocked - requires 2.5.5 first)
    ▼
  2.6.0 (Target - blocked)

升级规则:
  1. 允许: 相同版本 (patch) 或相邻 minor 版本
  2. 禁止: 跳级 minor 版本 (如 2.4.x → 2.6.0)
  3. 必须: 先升级到当前 minor 的最新 patch 再往下一级
  4. 防护: 降级必须通过签名验证，防止回滚攻击
```

#### 7.5.2 降级安全机制

```yaml
# 强制降级场景 (紧急故障恢复)
Authorization: Emergency-Downgrade-Token
  - 由安全团队签发
  - 有效期: 24小时
  - 使用次数: 1次
  - 目标版本: 指定
  - VIN绑定: 特定车辆

# 降级流程
1. 运营人员提交降级申请 (包含故障描述)
2. 安全审批 + 生成临时Token
3. 云端签发差分回退包 (Rollback Package)
4. 车端验证Token + 下载回退包
5. 执行回退 + 验证完整性
6. 确认恢复后锁定Token

# 技术限制
- 最低可降级版本: 硬件安全验证要求的最低版本
- 回退窗口: 升级后72小时内可回退
- 数据兼容: 高版本数据格式可能不兼容低版本
```

### 7.6 OTA版本兼容性

```yaml
# 升级路径检查
当前: 2.5.1
目标: 2.5.2

云端检查:
  - 2.5.1 → 2.5.2: ✅ 允许
  - 2.5.1 → 2.6.0: ❌ 需先升级到 2.5.x 最新版
  - 2.4.x → 2.5.2: ❌ 不支持跳级升级

# 降级保护
车端固件:
  - 禁止降级到旧版本 (防回滚攻击)
  - 仅允许升级到相同或更高版本
  - 紧急情况下需现场工程师解锁
```

---

## 附录

### A. 错误码表

| 错误码 | 含义 | HTTP/MQTT | 处理建议 |
|--------|------|-----------|----------|
| 0 | 成功 | 200/PUBACK | - |
| 1001 | 认证失败 | 401/CONNACK | 重新登录/检查证书 |
| 1002 | 权限不足 | 403 | 检查用户权限 |
| 1003 | 车辆离线 | 503 | 等待车辆上线 |
| 1004 | 指令超时 | 504 | 检查车端网络 |
| 1005 | 执行失败 | 500 | 查看车端日志 |
| 2001 | 版本不支持 | 406 | 升级SDK/固件 |
| 2002 | 参数错误 | 400 | 检查请求参数 |
| 2003 | 频率限制 | 429 | 降低请求频率 |

### B. 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 车端→云端延迟 | < 500ms | 4G网络下 |
| 远程指令响应 | < 3s | 车端收到到执行完成 |
| 位置数据上报 | 1-10s间隔 | 行驶中高频，静止低频 |
| 心跳间隔 | 30s | 保活检测 |
| OTA下载速度 | > 500KB/s | 差分升级包 |
| 离线缓存 | 7天 | 本地存储上限 |

---

**文档维护**: 车云架构组  
**最后更新**: 2026-05-10  
**下期评审**: 2026-06-10
