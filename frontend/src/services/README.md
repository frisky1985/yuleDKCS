# WebSocket Service

Frontend WebSocket 客户端实现，支持实时通信、车辆状态更新、命令发送等功能。

## 特性

- 支持 JWT 认证
- 自动重连（指数退避）
- 心跳机制（ping/pong）
- 消息订阅系统
- 支持 vehicle_status、command_result 等消息类型
- React Hook 支持

## 快速开始

### 1. 使用 React Hook

```tsx
import { useWebSocket } from '@/services/websocket'
import type { VehicleStatus, CommandResult } from '@/types'

function VehicleControl({ vehicleId }: { vehicleId: number }) {
  const { isConnected, isConnecting, error, sendCommand } = useWebSocket({
    vehicleId,
    autoConnect: true,
    onVehicleStatus: (status: VehicleStatus) => {
      console.log('车辆状态更新:', status)
    },
    onCommandResult: (result: CommandResult) => {
      console.log('命令结果:', result)
    },
    onConnect: () => {
      console.log('连接成功')
    },
    onDisconnect: () => {
      console.log('连接断开')
    },
  })

  const handleUnlock = () => {
    sendCommand('unlock')
  }

  const handleLock = () => {
    sendCommand('lock')
  }

  return (
    <div>
      <div>状态: {isConnected ? '已连接' : isConnecting ? '连接中...' : '已断开'}</div>
      {error && <div className="error">错误: {error.message}</div>}
      <button onClick={handleUnlock} disabled={!isConnected}>解锁</button>
      <button onClick={handleLock} disabled={!isConnected}>锁车</button>
    </div>
  )
}
```

### 2. 使用 Service 实例

```tsx
import { wsService, WS_MESSAGE_TYPES } from '@/services/websocket'
import type { VehicleStatus, CommandResult } from '@/types'

// 连接
await wsService.connect(vehicleId.toString())

// 订阅车辆状态
const unsubscribe = wsService.onVehicleStatus((status: VehicleStatus) => {
  console.log('车辆状态:', status)
})

// 订阅命令结果
wsService.onCommandResult((result: CommandResult) => {
  console.log('命令结果:', result)
})

// 发送命令
wsService.sendCommand(vehicleId, 'unlock')
wsService.sendCommand(vehicleId, 'engine_start', { duration: 600 })

// 取消订阅
unsubscribe()

// 断开连接
wsService.disconnect()
```

### 3. 自定义配置

```tsx
import { WebSocketService } from '@/services/websocket'

const wsService = new WebSocketService({
  baseURL: 'wss://api.yuledkcs.com/ws',
  reconnectInterval: 5000,        // 5秒后重连
  maxReconnectInterval: 300000,   // 最大重连间隔 5分钟
  maxReconnectAttempts: 20,       // 最多重试20次
  heartbeatInterval: 30000,       // 30秒心跳
  heartbeatTimeout: 10000,        // 10秒心跳超时
  connectionTimeout: 15000,       // 15秒连接超时
})
```

## 配置环境变量

在 `.env` 文件中配置 WebSocket URL：

```bash
# 开发环境
VITE_WS_URL=ws://localhost:8080

# 生产环境
VITE_WS_URL=wss://api.yuledkcs.com
```

## 消息类型

### 支持的消息类型

| 类型 | 说明 |
|------|------|
| `ping` | 心跳请求 |
| `pong` | 心跳响应 |
| `auth` | 认证消息 |
| `vehicle_status` | 车辆状态 |
| `vehicle_status_update` | 车辆状态更新 |
| `command` | 发送命令 |
| `command_result` | 命令执行结果 |
| `notification` | 系统通知 |
| `error` | 错误消息 |

### 车辆状态格式

```typescript
interface VehicleStatus {
  vehicle_id: number
  status: 'online' | 'offline' | 'sleeping' | 'unknown'
  lock_state: 'locked' | 'unlocked' | 'unknown'
  engine_state: 'running' | 'stopped' | 'unknown'
  battery_level: number
  fuel_level?: number
  location?: {
    latitude: number
    longitude: number
    altitude?: number
    accuracy?: number
    timestamp?: string
  }
  last_updated: string
}
```

### 命令结果格式

```typescript
interface CommandResult {
  command_id: string
  status: 'success' | 'failed' | 'pending' | 'timeout'
  result?: Record<string, unknown>
  error?: string
  executed_at?: string
}
```

## 支持的车辆命令

| 命令 | 说明 |
|------|------|
| `unlock` | 解锁车辆 |
| `lock` | 锁车 |
| `engine_start` | 启动发动机 |
| `engine_stop` | 关闭发动机 |
| `trunk_open` | 打开后备箱 |
| `trunk_close` | 关闭后备箱 |
| `climate_on` | 开启空调 |
| `climate_off` | 关闭空调 |
| `horn` | 鸣叫 |
| `lights` | 闪灯 |
| `locate` | 定位 |

## 高级用法

### 自定义消息处理

```tsx
import { wsService, WS_MESSAGE_TYPES } from '@/services/websocket'

// 订阅所有消息
wsService.on('*', (payload, type) => {
  console.log(`收到 ${type} 消息:`, payload)
})

// 订阅特定类型消息
const unsubscribe = wsService.on(WS_MESSAGE_TYPES.NOTIFICATION, (payload) => {
  console.log('收到通知:', payload)
})

// 取消订阅
unsubscribe()
```

### 监听连接状态

```tsx
wsService.on('connect', () => {
  console.log('已连接')
})

wsService.on('disconnect', ({ code, reason }) => {
  console.log('连接断开:', code, reason)
})

wsService.on('reconnecting', ({ attempt, delay }) => {
  console.log(`第 ${attempt} 次重连，${delay}ms 后尝试`)
})

wsService.on('max_reconnect_exceeded', ({ attempts }) => {
  console.log(`达到最大重连次数: ${attempts}`)
})
```

### 监听认证状态

```tsx
wsService.on('auth_success', (payload) => {
  console.log('认证成功:', payload.user_id)
})

wsService.on('auth_error', (payload) => {
  console.log('认证失败:', payload.message)
})
```

## 错误处理

```tsx
const { error, isConnected } = useWebSocket({
  onError: (err) => {
    console.error('WebSocket 错误:', err)
  },
})

// 或者使用 service
wsService.on('error', (error) => {
  console.error('WebSocket 错误:', error)
})
```

## API 参考

### WebSocketService 方法

| 方法 | 说明 | 返回值 |
|------|------|--------|
| `connect(vehicleId?)` | 连接到服务器 | `Promise<void>` |
| `disconnect()` | 断开连接 | `void` |
| `isConnected()` | 检查是否连接 | `boolean` |
| `getState()` | 获取当前状态 | `WebSocketState` |
| `send(type, payload)` | 发送消息 | `boolean` |
| `sendCommand(vehicleId, command, params?)` | 发送车辆命令 | `boolean` |
| `subscribeVehicle(vehicleId)` | 订阅车辆状态 | `boolean` |
| `unsubscribeVehicle(vehicleId)` | 取消订阅 | `boolean` |
| `on(type, handler)` | 订阅消息 | `() => void` |
| `off(type, handler)` | 取消订阅 | `void` |
| `onVehicleStatus(handler)` | 订阅车辆状态 | `() => void` |
| `onCommandResult(handler)` | 订阅命令结果 | `() => void` |
| `onNotification(handler)` | 订阅通知 | `() => void` |
| `onError(handler)` | 订阅错误 | `() => void` |

### useWebSocket Hook 返回值

| 属性/方法 | 类型 | 说明 |
|----------|------|------|
| `isConnected` | `boolean` | 是否已连接 |
| `isConnecting` | `boolean` | 是否正在连接 |
| `error` | `Error \| null` | 最后一个错误 |
| `connect()` | `() => Promise<void>` | 手动连接 |
| `disconnect()` | `() => void` | 手动断开 |
| `sendCommand()` | `(command, params?) => boolean` | 发送命令 |
| `service` | `WebSocketService` | Service 实例 |

## 协议兼容性

本 WebSocket 客户端完全兼容 Backend WebSocket 协议：

- 消息格式: `{ version, type, timestamp, id, payload }`
- 认证方式: JWT Token (查询参数或 auth 消息)
- 心跳机制: ping/pong 消息
- 重连策略: 指数退避 + 随机抖动

## 相关文件

- `websocket.ts` - 主要实现
- `websocket.types.ts` - 类型定义
- `index.ts` - 导出
