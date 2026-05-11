# WebSocket 协议一致性检查报告

**项目**: yuleDKCS Digital Key Connectivity Stack  
**审查日期**: 2026-05-09  
**版本**: 1.0.0  
**审查范围**: Backend / Frontend / Embedded 三端 WebSocket 协议实现

---

## 1. 执行摘要

| 检查项 | 状态 | 说明 |
|--------|------|------|
| 消息格式一致性 | 部分不一致 | Backend 与 Swagger 定义存在差异 |
| 消息类型一致性 | 不一致 | 三端消息类型定义不统一 |
| 版本号一致性 | 未定义 | WebSocket 协议版本未明确声明 |
| 连接方式一致性 | 部分一致 | URL 路径定义存在差异 |
| 心跳机制 | 一致 | ping/pong 机制一致 |

**总体评估**: 三端 WebSocket 协议实现存在多处不一致，需要统一规范。

---

## 2. 各端 WebSocket 实现概览

### 2.1 Backend (Go/Gin)

**文件位置**:
- `/home/admin/yuleDKCS/backend/internal/websocket/client.go`
- `/home/admin/yuleDKCS/backend/internal/websocket/hub.go`
- `/home/admin/yuleDKCS/backend/internal/handlers/ws_handler.go`

**消息结构**:
```go
type Message struct {
    Type      string      `json:"type"`
    Timestamp time.Time   `json:"timestamp"`
    Payload   interface{} `json:"payload"`
}
```

**连接端点**:
- `/ws` - 用户连接 (Query: `vehicle_id`)
- `/ws/vehicles/{id}` - 车辆连接
- `/ws/admin` - 管理员连接

**消息类型 (Inbound - 客户端到服务端)**:
| 类型 | 描述 | 处理函数 |
|------|------|----------|
| `vehicle_status` | 车辆状态更新 | handleVehicleStatus() |
| `command_result` | 命令执行结果 | handleCommandResult() |
| `ping` | 心跳请求 | 返回 pong |

**消息类型 (Outbound - 服务端到客户端)**:
| 类型 | 描述 |
|------|------|
| `command` | 发送命令到车辆 |
| `vehicle_status_update` | 车辆状态更新广播 |
| `command_result` | 命令执行结果 |
| `pong` | 心跳响应 |

**WebSocket 配置**:
```go
writeWait      = 10 * time.Second
pongWait       = 60 * time.Second
pingPeriod     = 54 * time.Second
maxMessageSize = 512 * 1024 // 512KB
```

---

### 2.2 Frontend (React/TypeScript)

**状态**: 未实现 WebSocket

**分析**: 检查 `/home/admin/yuleDKCS/frontend/` 目录下的所有源文件，未找到任何 WebSocket 相关实现。

**缺失内容**:
- WebSocket 客户端连接逻辑
- 消息收发处理
- 重连机制
- 心跳维护

---

### 2.3 Embedded (C/Mobile SDK)

**状态**: 无直接 WebSocket 实现

**通信路径**:
```
[Embedded SDK] <-- FFI/JNI --> [Mobile SDK] <-- HTTPS/WebSocket --> [Backend]
```

**文件位置**:
- Android: `/home/admin/yuleDKCS/mobile/android/src/main/java/com/yuledkcs/api/YuleDKCSApi.kt`
- iOS: `/home/admin/yuleDKCS/mobile/ios/Sources/YuleDKCS/YuleDKCS.swift`

**现状**: Mobile SDK 仅实现了 REST API 调用，未实现 WebSocket 通信。

---

## 3. 协议对比分析

### 3.1 消息格式对比

| 字段 | Backend 实现 | Swagger 定义 | Frontend | Embedded |
|------|-------------|--------------|----------|----------|
| `type` | string | 未定义结构 | N/A | N/A |
| `timestamp` | time.Time | 未定义结构 | N/A | N/A |
| `payload` | interface{} | 未定义结构 | N/A | N/A |

**问题**: Swagger 文档中未定义统一的 WebSocket 消息结构。

---

### 3.2 消息类型对比

#### Inbound (客户端 -> 服务端)

| 消息类型 | Backend | Swagger | 用途 |
|----------|---------|---------|------|
| `vehicle_status` | | | 车辆状态更新 |
| `command_result` | | | 命令执行结果 |
| `ping` | | (heartbeat) | 心跳检测 |
| `status` | | | (Swagger定义) |
| `location` | | | (Swagger定义) |
| `notification` | | | (Swagger定义) |

**不一致之处**:
1. Backend 使用 `ping`，Swagger 定义 `heartbeat`
2. Swagger 包含 `status/location/notification`，Backend 未实现

#### Outbound (服务端 -> 客户端)

| 消息类型 | Backend | Swagger | 用途 |
|----------|---------|---------|------|
| `command` | | | 发送控制命令 |
| `vehicle_status_update` | | | 车辆状态广播 |
| `command_result` | | | 命令结果 |
| `pong` | | (heartbeat) | 心跳响应 |
| `status` | | | (Swagger定义) |
| `notification` | | | (Swagger定义) |

**不一致之处**:
1. Backend 的 `vehicle_status_update` 与 Swagger 的 `status` 命名不一致
2. Swagger 包含 `notification`，Backend 未实现

---

### 3.3 连接端点对比

| 端点 | Backend | Swagger | 说明 |
|------|---------|---------|------|
| `/ws` | | | 用户连接 |
| `/ws?vehicle_id={id}` | | | (Backend实现) |
| `/ws/vehicles/{id}` | | | 车辆连接 |
| `/ws/vehicle/{id}` | | | (Swagger定义，路径不一致) |
| `/ws/admin` | | | 管理员连接 |

**不一致之处**:
1. Backend: `/ws/vehicles/{id}` vs Swagger: `/ws/vehicle/{id}` (单复数差异)

---

### 3.4 版本号

| 来源 | 版本声明 | 位置 |
|------|----------|------|
| Backend | 未声明 WebSocket 版本 | N/A |
| Swagger | API 1.0.0 | info.version |
| Frontend | N/A | N/A |
| Embedded | SDK 1.0.0 | YuleDKCS.swift |

**问题**: WebSocket 协议子协议版本未定义。

---

## 4. 详细不一致列表

### 4.1 严重不一致 (High Priority)

| # | 问题 | 影响 | 建议修复 |
|---|------|------|----------|
| 1 | Frontend 完全缺失 WebSocket 实现 | 无法接收实时推送 | 实现 WebSocket 客户端 |
| 2 | Mobile SDK 无 WebSocket 实现 | 车辆状态无法实时上报 | 添加 WebSocket 支持 |
| 3 | Backend 与 Swagger 消息类型命名不一致 | 文档与实现不同步 | 统一命名规范 |

### 4.2 中等不一致 (Medium Priority)

| # | 问题 | 影响 | 建议修复 |
|---|------|------|----------|
| 4 | WebSocket URL 路径不一致 | 集成困难 | 统一路径规范 |
| 5 | 缺少子协议版本号 | 协议演化困难 | 添加 `Sec-WebSocket-Protocol` |
| 6 | 消息结构未在 Swagger 定义 | 文档不完整 | 补充消息结构定义 |

### 4.3 轻微不一致 (Low Priority)

| # | 问题 | 影响 | 建议修复 |
|---|------|------|----------|
| 7 | 心跳消息命名差异 | 语义理解差异 | 统一使用 `ping/pong` |
| 8 | 部分消息类型 Swagger 有但 Backend 未实现 | 功能缺失 | 实现或移除 Swagger 定义 |

---

## 5. 推荐的统一协议规范

### 5.1 标准消息格式

```json
{
  "version": "1.0.0",
  "type": "message_type",
  "timestamp": "2026-05-09T20:52:00Z",
  "id": "uuid-string",
  "payload": { }
}
```

**字段说明**:
- `version`: WebSocket 协议版本号
- `type`: 消息类型 (小写，下划线分隔)
- `timestamp`: ISO 8601 格式时间戳
- `id`: 消息唯一标识 (UUID)
- `payload`: 消息载荷 (根据 type 变化)

---

### 5.2 标准消息类型

#### 心跳消息
```json
// 客户端 -> 服务端
{
  "version": "1.0.0",
  "type": "ping",
  "timestamp": "2026-05-09T20:52:00Z",
  "id": "ping-uuid",
  "payload": { }
}

// 服务端 -> 客户端
{
  "version": "1.0.0",
  "type": "pong",
  "timestamp": "2026-05-09T20:52:00Z",
  "id": "pong-uuid",
  "payload": { "ping_id": "ping-uuid" }
}
```

#### 车辆状态上报 (车辆 -> 服务端)
```json
{
  "version": "1.0.0",
  "type": "vehicle_status",
  "timestamp": "2026-05-09T20:52:00Z",
  "id": "uuid",
  "payload": {
    "vehicle_id": 123,
    "status": "online",
    "lock_state": "locked",
    "battery_level": 85,
    "location": {
      "latitude": 39.9042,
      "longitude": 116.4074
    }
  }
}
```

#### 车辆状态广播 (服务端 -> 客户端)
```json
{
  "version": "1.0.0",
  "type": "vehicle_status_update",
  "timestamp": "2026-05-09T20:52:00Z",
  "id": "uuid",
  "payload": {
    "vehicle_id": 123,
    "status": "online",
    "lock_state": "locked",
    "battery_level": 85
  }
}
```

#### 命令发送 (服务端 -> 车辆)
```json
{
  "version": "1.0.0",
  "type": "command",
  "timestamp": "2026-05-09T20:52:00Z",
  "id": "cmd-uuid",
  "payload": {
    "command_id": "cmd-uuid",
    "command": "unlock",
    "params": { }
  }
}
```

#### 命令结果 (车辆 -> 服务端 -> 客户端)
```json
{
  "version": "1.0.0",
  "type": "command_result",
  "timestamp": "2026-05-09T20:52:01Z",
  "id": "result-uuid",
  "payload": {
    "command_id": "cmd-uuid",
    "status": "success",
    "result": { }
  }
}
```

#### 系统通知 (服务端 -> 客户端)
```json
{
  "version": "1.0.0",
  "type": "notification",
  "timestamp": "2026-05-09T20:52:00Z",
  "id": "notif-uuid",
  "payload": {
    "level": "info",
    "title": "钥匙已分享",
    "message": "用户 XXX 接受了您的钥匙分享"
  }
}
```

---

### 5.3 标准连接端点

| 端点 | 认证方式 | 描述 |
|------|----------|------|
| `GET /ws/v1/user?vehicle_id={optional}` | JWT Bearer Token | 用户连接，订阅车辆更新 |
| `GET /ws/v1/vehicle/{id}` | Vehicle Token | 车辆连接，上报状态/接收命令 |
| `GET /ws/v1/admin` | Admin JWT Token | 管理员连接，系统监控 |

**子协议**: `yuledkcs.ws.v1`

---

### 5.4 连接参数

```go
// 写等待时间
writeWait = 10 * time.Second

// pong 等待时间 (最大间隔)
pongWait = 60 * time.Second

// ping 发送间隔
pingPeriod = 54 * time.Second

// 最大消息大小
maxMessageSize = 512 * 1024 // 512KB

// 发送缓冲区大小
sendBufferSize = 256

// 重连间隔 (客户端)
reconnectInterval = 3 * time.Second
maxReconnectInterval = 60 * time.Second
```

---

## 6. 实现建议

### 6.1 Frontend WebSocket 客户端

```typescript
// 建议实现结构
interface WebSocketClient {
  connect(url: string, token: string): void;
  disconnect(): void;
  send(type: string, payload: any): void;
  on(type: string, handler: (payload: any) => void): void;
  reconnect(): void;
}

// 消息类型常量
const WSMessageTypes = {
  PING: 'ping',
  PONG: 'pong',
  VEHICLE_STATUS: 'vehicle_status',
  VEHICLE_STATUS_UPDATE: 'vehicle_status_update',
  COMMAND: 'command',
  COMMAND_RESULT: 'command_result',
  NOTIFICATION: 'notification',
} as const;
```

### 6.2 Mobile SDK WebSocket 支持

```kotlin
// Android 建议接口
interface WebSocketService {
    fun connect(url: String, token: String): Flow<ConnectionState>
    fun subscribeVehicle(vehicleId: String): Flow<VehicleStatus>
    fun sendCommand(command: VehicleCommand): Flow<CommandResult>
    fun disconnect()
}
```

```swift
// iOS 建议接口
public protocol WebSocketService {
    func connect(url: String, token: String) -> AnyPublisher<ConnectionState, Error>
    func subscribeVehicle(vehicleId: String) -> AnyPublisher<VehicleStatus, Error>
    func sendCommand(_ command: VehicleCommand) -> AnyPublisher<CommandResult, Error>
    func disconnect()
}
```

---

## 7. 迁移计划

### Phase 1: 协议文档统一 (1-2 天)
- [ ] 更新 Swagger 文档，定义标准消息结构
- [ ] 更新 Backend 实现，遵循统一协议
- [ ] 添加 WebSocket 子协议版本声明

### Phase 2: Frontend 实现 (3-5 天)
- [ ] 实现 WebSocket 客户端类
- [ ] 集成 Redux/Store 状态管理
- [ ] 添加重连和心跳机制
- [ ] 车辆状态实时更新 UI

### Phase 3: Mobile SDK 实现 (5-7 天)
- [ ] Android WebSocket 服务实现
- [ ] iOS WebSocket 服务实现
- [ ] 与 Embedded SDK 集成测试

### Phase 4: 集成测试 (3-5 天)
- [ ] 三端端到端测试
- [ ] 性能测试 (并发连接)
- [ ] 稳定性测试 (长连接)

---

## 8. 结论

当前 yuleDKCS 项目的 WebSocket 协议实现存在明显的不一致性，主要表现为：

1. **Frontend 完全缺失** WebSocket 实现
2. **Mobile SDK 无 WebSocket** 支持
3. **Backend 与 Swagger** 消息类型命名不一致
4. **URL 路径** 定义存在差异

建议按照本报告第 5 节的推荐规范进行统一，并按照第 7 节的迁移计划逐步实施。

---

## 附录 A: 文件清单

| 文件路径 | 描述 |
|----------|------|
| `/home/admin/yuleDKCS/backend/internal/websocket/client.go` | Backend WebSocket 客户端实现 |
| `/home/admin/yuleDKCS/backend/internal/websocket/hub.go` | Backend WebSocket Hub |
| `/home/admin/yuleDKCS/backend/internal/handlers/ws_handler.go` | Backend WebSocket 处理器 |
| `/home/admin/yuleDKCS/docs/api/swagger.yaml` | API 文档 (含 WebSocket) |
| `/home/admin/yuleDKCS/backend/internal/router/router.go` | 路由定义 |
| `/home/admin/yuleDKCS/mobile/android/src/main/java/com/yuledkcs/api/YuleDKCSApi.kt` | Android API |
| `/home/admin/yuleDKCS/mobile/ios/Sources/YuleDKCS/YuleDKCS.swift` | iOS SDK |

---

*报告生成时间: 2026-05-09*
