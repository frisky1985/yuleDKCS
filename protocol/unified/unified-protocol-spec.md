# 数字钥匙统一协议规范
## 三端一致性协议：Cloud / Vehicle / SDK

**版本**: 1.0.0
**日期**: 2026-05-06
**状态**: 正式版

---

## 一、协议架构总览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          三端协议一致性                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────┐      BERTLV      ┌─────────────┐                   │
│   │   SDK端      │ ◄─────────────► │   Cloud端    │                   │
│   │ Android/iOS  │     REST/gRPC    │  Go Service │                   │
│   └─────────────┘                  └──────┬──────┘                   │
│                                             │                           │
│                                    ┌────────▼────────┐                 │
│                                    │   DKCS服务      │                 │
│                                    │   (密钥管理)    │                 │
│                                    └────────┬────────┘                 │
│                                             │                           │
│                                             │ MQTT                      │
│                                    ┌────────▼────────┐                 │
│                                    │   TCU端         │                 │
│   ┌─────────────┐                  │   (车端C代码)   │                  │
│   │  Vehicle端   │ ◄───────────────── │  BLE/UWB/NFC │                   │
│   │  ECU/BCM/PEPS│    私有协议        └────────────────┘                  │
│   └─────────────┘                                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 二、统一错误码 (Error Codes)

### 2.1 错误码结构

```
┌────────────────────────────────────────────────────────┐
│                    Error Code (4字节)                   │
├─────────────────────┬──────────────────────────────────┤
│   Category (2位)    │          Code (2位)              │
│   错误类别          │          错误码                  │
└─────────────────────┴──────────────────────────────────┘
```

### 2.2 错误类别 (Category)

| 类别 | 范围 | 说明 |
|------|------|------|
| 00 | 0000-0099 | 成功 |
| 01 | 0100-0199 | 请求/参数 |
| 02 | 0200-0299 | 认证/鉴权 |
| 03 | 0300-0399 | 密钥管理 |
| 04 | 0400-0499 | 车辆控制 |
| 05 | 0500-0599 | 分享业务 |
| 06 | 0600-0699 | 设备/硬件 |
| 07 | 0700-0799 | 厂商适配 |
| 08 | 0800-0899 | 通信链路 |
| 09 | 0900-0999 | 系统内部 |

### 2.3 完整错误码表

#### 成功 (0000-0099)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0000 | SUCCESS | 200 | 成功 |
| 0x0001 | SUCCESS_ASYNC | 202 | 异步成功，已接收 |
| 0x0002 | SUCCESS_PARTIAL | 206 | 部分成功 |

#### 请求/参数错误 (0100-0199)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0101 | ERR_INVALID_REQUEST | 400 | 无效请求 |
| 0x0102 | ERR_INVALID_PARAMETER | 400 | 参数错误 |
| 0x0103 | ERR_MISSING_PARAMETER | 400 | 缺少参数 |
| 0x0104 | ERR_INVALID_FORMAT | 400 | 格式错误 |
| 0x0105 | ERR_INVALID_LENGTH | 400 | 长度错误 |
| 0x0106 | ERR_INVALID_SIGNATURE | 400 | 签名无效 |
| 0x0107 | ERR_RATE_LIMIT | 429 | 请求过于频繁 |
| 0x0108 | ERR_QUOTA_EXCEEDED | 429 | 配额超限 |
| 0x0109 | ERR_VERSION_MISMATCH | 400 | 版本不匹配 |

#### 认证/鉴权错误 (0200-0299)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0201 | ERR_UNAUTHORIZED | 401 | 未授权 |
| 0x0202 | ERR_INVALID_TOKEN | 401 | Token无效 |
| 0x0203 | ERR_TOKEN_EXPIRED | 401 | Token过期 |
| 0x0204 | ERR_FORBIDDEN | 403 | 禁止访问 |
| 0x0205 | ERR_ACCESS_DENIED | 403 | 权限不足 |
| 0x0206 | ERR_USER_DISABLED | 403 | 用户已禁用 |
| 0x0207 | ERR_ACCOUNT_LOCKED | 403 | 账户已锁定 |
| 0x0208 | ERR_SESSION_EXPIRED | 401 | 会话已过期 |

#### 密钥管理错误 (0300-0399)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0301 | ERR_KEY_NOT_FOUND | 404 | 密钥不存在 |
| 0x0302 | ERR_KEY_EXISTS | 409 | 密钥已存在 |
| 0x0303 | ERR_KEY_EXPIRED | 403 | 密钥已过期 |
| 0x0304 | ERR_KEY_REVOKED | 403 | 密钥已撤销 |
| 0x0305 | ERR_KEY_SUSPENDED | 403 | 密钥已挂起 |
| 0x0306 | ERR_KEY_PENDING | 409 | 密钥待激活 |
| 0x0307 | ERR_KEY_MAX_REACHED | 409 | 密钥数量达上限 |
| 0x0308 | ERR_KEY_MAX_USES | 403 | 使用次数已用完 |
| 0x0309 | ERR_DISTANCE_EXCEEDED | 403 | 距离超出限制 |
| 0x030A | ERR_KEY_TYPE_NOT_ALLOWED | 403 | 密钥类型不允许 |
| 0x030B | ERR_KEY_NOT_ACTIVE | 403 | 密钥未激活 |
| 0x030C | ERR_KEY_VALIDITY_NOT_STARTED | 403 | 密钥未到生效时间 |
| 0x030D | ERR_KEY_BIND_FAILED | 500 | 密钥绑定失败 |
| 0x030E | ERR_KEY_UNBIND_FAILED | 500 | 密钥解绑失败 |

#### 车辆控制错误 (0400-0499)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0401 | ERR_VEHICLE_NOT_FOUND | 404 | 车辆不存在 |
| 0x0402 | ERR_VEHICLE_OFFLINE | 503 | 车辆离线 |
| 0x0403 | ERR_VEHICLE_NOT_BOUND | 404 | 车辆未绑定 |
| 0x0404 | ERR_TCU_OFFLINE | 503 | TCU离线 |
| 0x0405 | ERR_TCU_TIMEOUT | 504 | TCU超时 |
| 0x0406 | ERR_TCU_ERROR | 500 | TCU内部错误 |
| 0x0407 | ERR_COMMAND_FAILED | 500 | 指令执行失败 |
| 0x0408 | ERR_COMMAND_TIMEOUT | 504 | 指令执行超时 |
| 0x0409 | ERR_COMMAND_REJECTED | 403 | 指令被拒绝 |
| 0x040A | ERR_COMMAND_INVALID | 400 | 指令无效 |
| 0x040B | ERR_COMMAND_IN_PROGRESS | 409 | 指令执行中 |
| 0x040C | ERR_VEHICLE_NOT_SUPPORTED | 400 | 车辆不支持该操作 |
| 0x040D | ERR_BATTERY_LOW | 403 | 电瓶电量低 |
| 0x040E | ERR_ENGINE_RUNNING | 403 | 引擎运行中 |
| 0x040F | ERR_VEHICLE_MOVING | 403 | 车辆正在移动 |
| 0x0410 | ERR_DOOR_OPEN | 403 | 车门未关闭 |

#### 分享业务错误 (0500-0599)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0501 | ERR_SHARE_NOT_FOUND | 404 | 分享不存在 |
| 0x0502 | ERR_SHARE_EXPIRED | 403 | 分享已过期 |
| 0x0503 | ERR_SHARE_ACCEPTED | 409 | 分享已被接受 |
| 0x0504 | ERR_SHARE_CANCELLED | 409 | 分享已取消 |
| 0x0505 | ERR_SHARE_MAX_USES | 403 | 分享次数已用完 |
| 0x0506 | ERR_SHARE_NOT_ALLOWED | 403 | 不允许分享 |
| 0x0507 | ERR_CODE_INVALID | 400 | 分享码无效 |
| 0x0508 | ERR_CODE_EXPIRED | 403 | 分享码过期 |
| 0x0509 | ERR_VENDOR_NOT_MATCH | 403 | 厂商不匹配 |
| 0x050A | ERR_SHARE_TO_SELF | 400 | 不能分享给自己 |

#### 设备/硬件错误 (0600-0699)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0601 | ERR_DEVICE_NOT_FOUND | 404 | 设备不存在 |
| 0x0602 | ERR_DEVICE_NOT_BOUND | 404 | 设备未绑定 |
| 0x0603 | ERR_DEVICE_DISABLED | 403 | 设备已禁用 |
| 0x0604 | ERR_DEVICE_NOT_SUPPORTED | 400 | 设备不支持 |
| 0x0605 | ERR_BLE_DISABLED | 400 | 蓝牙未开启 |
| 0x0606 | ERR_NFC_DISABLED | 400 | NFC未开启 |
| 0x0607 | ERR_UWB_DISABLED | 400 | UWB未开启 |
| 0x0608 | ERR_SE_NOT_AVAILABLE | 400 | 安全元件不可用 |
| 0x0609 | ERR_SE_ERROR | 500 | SE通信错误 |
| 0x060A | ERR_STORAGE_FULL | 400 | 存储满 |
| 0x060B | ERR_BLE_SCAN_FAILED | 500 | BLE扫描失败 |
| 0x060C | ERR_BLE_CONNECT_FAILED | 500 | BLE连接失败 |
| 0x060D | ERR_NFC_READ_FAILED | 500 | NFC读取失败 |
| 0x060E | ERR_NFC_WRITE_FAILED | 500 | NFC写入失败 |
| 0x060F | ERR_UWB_RANGING_FAILED | 500 | UWB测距失败 |

#### 厂商适配错误 (0700-0799)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0701 | ERR_VENDOR_NOT_SUPPORTED | 400 | 厂商不支持 |
| 0x0702 | ERR_ADAPTER_NOT_FOUND | 500 | 适配器未找到 |
| 0x0703 | ERR_VENDOR_API_ERROR | 502 | 厂商API错误 |
| 0x0704 | ERR_VENDOR_API_TIMEOUT | 504 | 厂商API超时 |
| 0x0705 | ERR_VENDOR_AUTH_FAILED | 401 | 厂商认证失败 |
| 0x0706 | ERR_VENDOR_BIND_FAILED | 500 | 厂商绑定失败 |
| 0x0707 | ERR_PROTOCOL_ERROR | 400 | 协议错误 |

#### 通信链路错误 (0800-0899)

| 错误码 | 枚举名 | HTTP | 说明 |
|--------|--------|------|------|
| 0x0801 | ERR_NETWORK_ERROR | 0 | 网络错误 |
| 0x0802 | ERR_NETWORK_TIMEOUT | 0 | 网络超时 |
| 0x0803 | ERR_SERVER_UNREACHABLE | 0 | 服务器不可达 |
| 0x0804 | ERR_CONNECTION_REFUSED | 0 | 连接被拒绝 |
| 0x0805 | ERR_MQTT_DISCONNECTED | 0 | MQTT断开 |
| 0x0806 | ERR_MQTT_PUBLISH_FAILED | 0 | MQTT发布失败 |
| 0x0807 | ERR_GRPC_UNAVAILABLE | 0 | gRPC不可用 |
| 0x0808 | ERR_GRPC_DEADLINE | 0 | gRPC超时 |

#### 系统内部错误 (0900-0999)

| 0x0901 | ERR_INTERNAL_ERROR | 500 | 内部错误 |
| 0x0902 | ERR_SERVICE_UNAVAILABLE | 503 | 服务不可用 |
| 0x0903 | ERR_DATABASE_ERROR | 500 | 数据库错误 |
| 0x0904 | ERR_CACHE_ERROR | 500 | 缓存错误 |
| 0x0905 | ERR_QUEUE_ERROR | 500 | 队列错误 |
| 0x0906 | ERR_CONFIG_ERROR | 500 | 配置错误 |
| 0x0907 | ERR_CRYPTO_ERROR | 500 | 加密错误 |
| 0x0908 | ERR_SIGN_ERROR | 500 | 签名错误 |
| 0x0909 | ERR_VERIFY_ERROR | 500 | 验签错误 |
| 0x090A | ERR_FEATURE_NOT_SUPPORTED | 501 | 功能不支持 |
| 0x090B | ERR_MAINTENANCE | 503 | 系统维护 |
| 0x090C | ERR_CAPACITY_EXCEEDED | 503 | 容量超限 |

### 2.4 TCU车端专用错误 (ACxx)

| 错误码 | 枚举名 | 说明 |
|--------|--------|------|
| 0xAC01 | ERR_TCU_BLE_ERROR | BLE通信错误 |
| 0xAC02 | ERR_TCU_NFC_ERROR | NFC通信错误 |
| 0xAC03 | ERR_TCU_UWB_ERROR | UWB通信错误 |
| 0xAC04 | ERR_TCU_SE_ERROR | SE通信错误 |
| 0xAC05 | ERR_TCU_PAIRING_FAILED | 配对失败 |
| 0xAC06 | ERR_TCU_AUTH_FAILED | 认证失败 |
| 0xAC07 | ERR_TCU_CRYPTO_FAILED | 加密失败 |
| 0xAC08 | ERR_TCU_STORAGE_ERROR | 存储错误 |
| 0xAC09 | ERR_TCU_POWER_LOW | 电量低 |
| 0xAC0A | ERR_TCU_WATCHDOG_RESET | 看门狗复位 |
| 0xAC0B | ERR_TCU_ANTENNA_FAULT | 天线故障 |
| 0xAC0C | ERR_TCU_MSG_QUEUE_FULL | 消息队列满 |

### 2.5 错误响应格式

**Cloud端响应**:
```json
{
  "code": 307,
  "msg": "Distance exceeded",
  "trace_id": "abc123def456",
  "timestamp": 1714963200,
  "data": null
}
```

**SDK端响应**:
```json
{
  "code": 307,
  "message": "距离超出限制",
  "details": {
    "max_distance_mm": 5000,
    "actual_distance_mm": 8500
  }
}
```

**Vehicle端日志**:
```
[2026-05-06 09:53:00.123][ERR][KEYMGR][0x0309] Distance exceeded: max=5000mm, actual=8500mm
```

---

## 三、统一埋点规范 (Telemetry)

### 3.1 埋点数据结构

所有三端使用统一的埋点数据格式：

```
┌─────────────────────────────────────────────────────────────┐
│                    Telemetry Event                         │
├─────────────────────────────────────────────────────────────┤
│  event_id:     UUID - 事件唯一标识                          │
│  event_type:   String - 事件类型                            │
│  timestamp:    Uint64 - Unix时间戳(毫秒)                    │
│  source:       Enum - 事件来源(SDK/TCU/CLOUD)               │
│  session_id:   String - 会话ID                             │
│  user_id:      String - 用户ID(可选)                       │
│  device_id:    String - 设备ID                              │
│  vehicle_id:   String - 车辆ID(可选)                        │
│  key_id:       String - 密钥ID(可选)                        │
│  properties:   Map - 事件属性                               │
│  context:      Map - 上下文信息                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 事件类型枚举

| 事件类型 | 说明 | SDK | TCU | Cloud |
|----------|------|-----|-----|-------|
| app_launch | App启动 | ✓ | - | - |
| app_background | App进入后台 | ✓ | - | - |
| sdk_init | SDK初始化 | ✓ | - | - |
| sdk_error | SDK错误 | ✓ | - | ✓ |
| user_login | 用户登录 | ✓ | - | ✓ |
| user_logout | 用户登出 | ✓ | - | ✓ |
| device_bind_start | 设备绑定开始 | ✓ | ✓ | ✓ |
| device_bind_success | 设备绑定成功 | ✓ | ✓ | ✓ |
| device_bind_failed | 设备绑定失败 | ✓ | ✓ | ✓ |
| device_unbind | 设备解绑 | ✓ | ✓ | ✓ |
| key_create | 创建密钥 | ✓ | ✓ | ✓ |
| key_delete | 删除密钥 | ✓ | ✓ | ✓ |
| key_refresh | 刷新密钥 | ✓ | ✓ | ✓ |
| key_use | 使用密钥 | ✓ | ✓ | ✓ |
| key_expired | 密钥过期 | ✓ | ✓ | ✓ |
| vehicle_unlock | 车辆解锁 | ✓ | ✓ | ✓ |
| vehicle_lock | 车辆闭锁 | ✓ | ✓ | ✓ |
| vehicle_start | 启动引擎 | ✓ | ✓ | ✓ |
| vehicle_stop | 熄火 | ✓ | ✓ | ✓ |
| share_create | 创建分享 | ✓ | - | ✓ |
| share_accept | 接受分享 | ✓ | - | ✓ |
| share_revoke | 撤销分享 | ✓ | - | ✓ |
| ble_scan | BLE扫描 | ✓ | ✓ | - |
| ble_connect | BLE连接 | ✓ | ✓ | - |
| ble_disconnect | BLE断开 | ✓ | ✓ | - |
| nfc_tap | NFC触碰 | ✓ | ✓ | - |
| uwb_ranging | UWB测距 | ✓ | ✓ | - |
| channel_switch | 通道切换 | ✓ | ✓ | - |
| protocol_msg | 协议消息 | ✓ | ✓ | ✓ |
| security_event | 安全事件 | ✓ | ✓ | ✓ |

### 3.3 埋点属性 (Properties)

**通用属性**:
| 属性名 | 类型 | 说明 |
|--------|------|------|
| event_name | string | 事件名称 |
| event_type | string | 事件类型 |
| duration_ms | uint32 | 耗时(毫秒) |
| result | string | 结果(success/failed/timeout) |
| error_code | uint16 | 错误码 |
| error_msg | string | 错误信息 |

**SDK特有属性**:
| 属性名 | 类型 | 说明 |
|--------|------|------|
| os_version | string | OS版本 |
| app_version | string | App版本 |
| sdk_version | string | SDK版本 |
| device_model | string | 设备型号 |
| channel_type | string | 通道类型(nfc/ble/uwb) |
| signal_strength | int8 | 信号强度 |
| battery_level | uint8 | 电量百分比 |
| network_type | string | 网络类型(wifi/4g/5g) |

**TCU特有属性**:
| 属性名 | 类型 | 说明 |
|--------|------|------|
| tcu_firmware_version | string | TCU固件版本 |
| ble_rssi | int8 | BLE信号强度 |
| uwb_distance_mm | uint16 | UWB距离(毫米) |
| uwb_accuracy_mm | uint16 | UWB精度(毫米) |
| nfc_tag_id | string | NFC标签ID |
| door_status | uint8 | 车门状态位掩码 |
| lock_status | string | 锁状态 |
| engine_status | string | 引擎状态 |

**Cloud特有属性**:
| 属性名 | 类型 | 说明 |
|--------|------|------|
| service_name | string | 服务名称 |
| endpoint | string | API端点 |
| request_id | string | 请求ID |
| response_time_ms | uint32 | 响应时间 |
| region | string | 区域 |
| instance_id | string | 实例ID |

### 3.4 安全事件属性

| 属性名 | 类型 | 说明 |
|--------|------|------|
| security_event_type | string | 安全事件类型 |
| threat_level | uint8 | 威胁等级(1-5) |
| ip_address | string | IP地址 |
| location | string | 地理位置 |
| attempt_count | uint8 | 尝试次数 |
| key_id | string | 涉及的密钥ID |
| action_taken | string | 采取的行动 |

### 3.5 埋点采样规则

| 事件类型 | 采样率 | 说明 |
|----------|--------|------|
| 常规事件 | 100% | 全量采集 |
| 性能事件 | 100% | 全量采集 |
| 安全事件 | 100% | 全量采集 |
| 调试事件 | 10% | 按配置采样 |
| 错误事件 | 100% | 全量采集 |

---

## 四、统一日志规范 (Logging)

### 4.1 日志格式

**Cloud端 (JSON格式)**:
```json
{
  "timestamp": "2026-05-06T09:53:00.123+08:00",
  "level": "INFO",
  "service": "hub-service",
  "instance": "hub-001",
  "trace_id": "abc123def456",
  "span_id": "span789",
  "logger": "key_management",
  "message": "Key created successfully",
  "fields": {
    "key_id": "KEY123456",
    "user_id": "USER001",
    "vehicle_id": "VH789",
    "duration_ms": 150
  }
}
```

**SDK端 (Android - JSON格式)**:
```json
{
  "timestamp": 1714963980123,
  "level": "I",
  "tag": "DigitalKeySDK",
  "trace_id": "sdk-abc123",
  "message": "BLE connection established",
  "fields": {
    "device_id": "ANDROID-xxx",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "mtu": 512,
    "duration_ms": 234
  }
}
```

**SDK端 (iOS - JSON格式)**:
```json
{
  "timestamp": 1714963980123,
  "level": "info",
  "subsystem": "com.digitalkey.sdk",
  "category": "BLEManager",
  "trace_id": "sdk-abc123",
  "message": "BLE connection established",
  "fields": {
    "device_id": "iOS-xxx",
    "peripheral_uuid": "xxx-xxx",
    "mtu": 512
  }
}
```

**Vehicle端 (C - 结构化文本)**:
```
[2026-05-06 09:53:00.123][I][KEYMGR][T:abc123] Key created: id=KEY123456, dur=150ms
[2026-05-06 09:53:00.456][W][BLE] Connection timeout: mac=AA:BB:CC:DD:EE:FF, dur=5000ms
[2026-05-06 09:53:01.789][E][SEC] Auth failed: err=0xAC06, key_id=KEY123456
```

### 4.2 日志级别

| 级别 | 值 | Cloud | SDK | Vehicle | 说明 |
|------|-----|-------|-----|---------|------|
| TRACE | 0 | ✓ | ✓ | ✓ | 跟踪详情 |
| DEBUG | 1 | ✓ | ✓ | ✓ | 调试信息 |
| INFO | 2 | ✓ | ✓ | ✓ | 一般信息 |
| WARN | 3 | ✓ | ✓ | ✓ | 警告信息 |
| ERROR | 4 | ✓ | ✓ | ✓ | 错误信息 |
| FATAL | 5 | ✓ | ✓ | ✓ | 致命错误 |

### 4.3 日志标签 (模块)

| 标签 | 说明 | SDK | TCU | Cloud |
|------|------|-----|-----|-------|
| INIT | 初始化 | ✓ | ✓ | ✓ |
| KEYMGR | 密钥管理 | ✓ | ✓ | ✓ |
| AUTH | 认证模块 | ✓ | ✓ | ✓ |
| BLE | BLE通信 | ✓ | ✓ | - |
| NFC | NFC通信 | ✓ | ✓ | - |
| UWB | UWB通信 | ✓ | ✓ | - |
| SEC | 安全模块 | ✓ | ✓ | ✓ |
| TRANSPORT | 传输层 | ✓ | ✓ | ✓ |
| PROTOCOL | 协议解析 | ✓ | ✓ | ✓ |
| SERVICE | 业务服务 | - | ✓ | ✓ |
| ADAPTER | 厂商适配 | - | - | ✓ |
| DB | 数据库 | - | - | ✓ |
| CACHE | 缓存 | - | - | ✓ |
| MQTT | MQTT通信 | - | ✓ | ✓ |
| GRPC | gRPC通信 | - | - | ✓ |

### 4.4 日志脱敏规则

| 敏感字段 | 脱敏规则 | 示例 |
|----------|----------|------|
| password | 完全隐藏 | `***` |
| token | 只显示前4后4 | `eyJ...xxxx` |
| phone | 只显示后4 | `****5678` |
| email | 只显示前后2 | `ji****om` |
| mac_address | 只显示后4 | `xx:xx:xx:DD:EE:FF` |
| vin | 只显示后4 | `****5678` |
| credit_card | 完全隐藏 | `****` |
| signature | 只显示长度 | `sig[32B]` |

### 4.5 日志输出目标

**Cloud端**:
- Console (stdout)
- File (/var/log/digitalkey/)
- Fluentd/Filebeat
- Syslog

**SDK端**:
- Console (可选)
- File (应用沙盒内)
- Crashlytics (错误日志)

**Vehicle端**:
- UART/Serial
- CAN Bus (诊断)
- Internal Flash (循环buffer)

---

## 五、Trace ID链路追踪

### 5.1 Trace ID格式

```
格式: {source}-{timestamp}-{random}
示例: SDK-20260506095300-a1b2c3d4
      TCU-20260506095301-e5f6g7h8
      CLOUD-20260506095302-i9j0k1l2
```

### 5.2 Trace ID传递

```
SDK ──[trace_id]──► Cloud ──[trace_id]──► DKCS ──[trace_id]──► TCU
        │                                       │
        └─────────────MQTT topic────────────────┘
```

### 5.3 Trace Span结构

| 字段 | 说明 |
|------|------|
| trace_id | 全链路ID |
| span_id | 当前Span ID |
| parent_span_id | 父Span ID |
| operation_name | 操作名称 |
| start_time | 开始时间 |
| duration_ms | 持续时间 |
| status_code | 状态码 |
| tags | 标签 |
| logs | 日志事件 |

---

## 六、BERTLV统一编解码

### 6.1 统一Tag定义

| Tag | 名称 | 类型 | 说明 |
|-----|------|------|------|
| 0xE001 | DeviceID | UTF8String | 设备ID |
| 0xE002 | VehicleID | UTF8String | 车辆ID |
| 0xE003 | KeyID | UTF8String | 密钥ID |
| 0xE004 | UserID | UTF8String | 用户ID |
| 0xE005 | CommandID | UINT8 | 命令ID |
| 0xE006 | StatusCode | UINT16 | 状态码 |
| 0xE007 | Timestamp | UINT64 | 时间戳 |
| 0xE008 | SequenceNo | UINT32 | 序列号 |
| 0xE009 | ErrorCode | UINT16 | 错误码 |
| 0xE00A | ErrorMsg | UTF8String | 错误信息 |
| 0xE00B | TraceID | UTF8String | 追踪ID |
| 0xE00C | Signature | OCTET_STRING | 签名 |
| 0xE00D | PublicKey | OCTET_STRING | 公钥 |
| 0xE00E | Certificate | OCTET_STRING | 证书 |
| 0xE00F | Nonce | OCTET_STRING | 随机数 |
| 0xE010 | SessionKey | OCTET_STRING | 会话密钥 |
| 0xE011 | EncryptedData | OCTET_STRING | 加密数据 |
| 0xE012 | ProtocolVersion | UINT8 | 协议版本 |
| 0xE013 | ChannelType | UINT8 | 通道类型 |
| 0xE014 | SignalStrength | INT8 | 信号强度 |
| 0xE015 | Distance | UINT16 | 距离(毫米) |
| 0xE016 | AccessLevel | UINT16 | 访问权限 |
| 0xE017 | KeyType | UINT8 | 密钥类型 |
| 0xE018 | KeyStatus | UINT8 | 密钥状态 |
| 0xE019 | ValidFrom | UINT64 | 生效时间 |
| 0xE01A | ValidUntil | UINT64 | 失效时间 |
| 0xE01B | MaxUses | UINT16 | 最大使用次数 |
| 0xE01C | UsedCount | UINT16 | 已使用次数 |
| 0xE01D | ShareCode | NUMERIC_STRING | 分享码 |
| 0xE01E | BleMac | OCTET_STRING | BLE MAC地址 |
| 0xE01F | UwbAddress | OCTET_STRING | UWB地址 |
| 0xE020 | NfcTagID | OCTET_STRING | NFC标签ID |
| 0xE0F0 | VendorData | OCTET_STRING | 厂商私有数据 |
| 0xE0FF | Extension | Constructed | 扩展数据 |

### 6.2 消息结构

**请求消息**:
```
Message ::= SEQUENCE {
    header      Header,
    body        Body,
    signature   Signature (optional)
}

Header ::= SEQUENCE {
    version     UINT8,
    timestamp   UINT64,
    msg_type    UINT16,
    sequence    UINT32,
    trace_id    UTF8String,
    device_id   UTF8String (optional),
    user_id     UTF8String (optional)
}

Body ::= ANY (根据msg_type确定)
```

**响应消息**:
```
Response ::= SEQUENCE {
    status      UINT16,      -- 错误码
    message     UTF8String,  -- 错误信息
    trace_id    UTF8String,
    data        ANY (optional)
}
```

---

## 七、三端SDK接口对照

### 7.1 错误处理统一接口

**Go (Cloud)**:
```go
type Error struct {
    Code    uint16
    Message string
    Details map[string]interface{}
    TraceID string
}

func (e *Error) Error() string {
    return fmt.Sprintf("[0x%04X] %s", e.Code, e.Message)
}

func NewError(code uint16, message string) *Error {
    return &Error{Code: code, Message: message}
}
```

**Kotlin (Android)**:
```kotlin
data class DigitalKeyError(
    val code: Int,
    val message: String,
    val details: Map<String, Any>? = null,
    val traceId: String? = null
) : Exception(message) {
    
    companion object {
        val SUCCESS = DigitalKeyError(0, "Success")
        
        fun fromCode(code: Int): DigitalKeyError {
            return DigitalKeyError(code, getErrorMessage(code))
        }
    }
}
```

**Swift (iOS)**:
```swift
struct DigitalKeyError: Error {
    let code: UInt16
    let message: String
    let details: [String: Any]?
    let traceId: String?
    
    static let success = DigitalKeyError(code: 0, message: "Success", details: nil, traceId: nil)
    
    static func from(code: UInt16) -> DigitalKeyError {
        DigitalKeyError(code: code, message: getErrorMessage(code), details: nil, traceId: nil)
    }
}
```

**C (Vehicle)**:
```c
typedef struct {
    uint16_t code;
    char message[128];
    char trace_id[32];
    void *details;
} dk_error_t;

static inline const char* dk_error_name(uint16_t code) {
    switch(code) {
        case 0x0000: return "SUCCESS";
        case 0x0301: return "ERR_KEY_NOT_FOUND";
        case 0x0402: return "ERR_VEHICLE_OFFLINE";
        // ...
        default: return "ERR_UNKNOWN";
    }
}
```

### 7.2 日志统一接口

**Go (Cloud)**:
```go
type Logger interface {
    Trace(msg string, fields ...Field)
    Debug(msg string, fields ...Field)
    Info(msg string, fields ...Field)
    Warn(msg string, fields ...Field)
    Error(msg string, fields ...Field)
    Fatal(msg string, fields ...Field)
}

type Field struct {
    Key   string
    Value interface{}
}

func WithTraceID(traceID string) Field { return Field{Key: "trace_id", Value: traceID} }
func WithError(err error) Field { return Field{Key: "error", Value: err.Error()} }
func WithDuration(ms uint64) Field { return Field{Key: "duration_ms", Value: ms} }
func WithKeyID(id string) Field { return Field{Key: "key_id", Value: id} }
func WithVehicleID(id string) Field { return Field{Key: "vehicle_id", Value: id} }
func WithUserID(id string) Field { return Field{Key: "user_id", Value: id} }
func WithDeviceID(id string) Field { return Field{Key: "device_id", Value: id} }
func WithErrorCode(code uint16) Field { return Field{Key: "error_code", Value: fmt.Sprintf("0x%04X", code)} }
```

**Kotlin (Android)**:
```kotlin
object DkLogger {
    enum class Level { VERBOSE, DEBUG, INFO, WARN, ERROR }
    
    fun v(tag: String, msg: String, vararg fields: Pair<String, Any?>)
    fun d(tag: String, msg: String, vararg fields: Pair<String, Any?>)
    fun i(tag: String, msg: String, vararg fields: Pair<String, Any?>)
    fun w(tag: String, msg: String, vararg fields: Pair<String, Any?>)
    fun e(tag: String, msg: String, throwable: Throwable? = null, vararg fields: Pair<String, Any?>)
    
    // 常用标签扩展
    fun keyMgr(msg: String, vararg fields: Pair<String, Any?>) = i("KEYMGR", msg, *fields)
    fun auth(msg: String, vararg fields: Pair<String, Any?>) = i("AUTH", msg, *fields)
    fun ble(msg: String, vararg fields: Pair<String, Any?>) = i("BLE", msg, *fields)
    fun nfc(msg: String, vararg fields: Pair<String, Any?>) = i("NFC", msg, *fields)
    fun sec(msg: String, vararg fields: Pair<String, Any?>) = i("SEC", msg, *fields)
}
```

**Swift (iOS)**:
```swift
enum DkLogLevel: String {
    case trace = "DEBUG"
    case debug = "DEBUG"
    case info = "INFO"
    case warning = "WARNING"
    case error = "ERROR"
}

struct DkLogger {
    static let subsystem = "com.digitalkey.sdk"
    
    static func trace(_ message: String, category: String, fields: [String: Any] = [:])
    static func debug(_ message: String, category: String, fields: [String: Any] = [:])
    static func info(_ message: String, category: String, fields: [String: Any] = [:])
    static func warning(_ message: String, category: String, fields: [String: Any] = [:])
    static func error(_ message: String, category: String, error: Error? = nil, fields: [String: Any] = [:])
    
    // 便捷方法
    static func keyManager(_ message: String, fields: [String: Any] = [:]) {
        info(message, category: "KEYMGR", fields: fields)
    }
    
    static func auth(_ message: String, fields: [String: Any] = [:]) {
        info(message, category: "AUTH", fields: fields)
    }
}
```

**C (Vehicle)**:
```c
// 日志级别
#define DK_LOG_LEVEL_TRACE  0
#define DK_LOG_LEVEL_DEBUG   1
#define DK_LOG_LEVEL_INFO    2
#define DK_LOG_LEVEL_WARN    3
#define DK_LOG_LEVEL_ERROR   4
#define DK_LOG_LEVEL_FATAL   5

// 日志宏
#define DK_LOG_TRACE(tag, fmt, ...)  dk_log(DK_LOG_LEVEL_TRACE, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define DK_LOG_DEBUG(tag, fmt, ...)  dk_log(DK_LOG_LEVEL_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define DK_LOG_INFO(tag, fmt, ...)   dk_log(DK_LOG_LEVEL_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define DK_LOG_WARN(tag, fmt, ...)  dk_log(DK_LOG_LEVEL_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define DK_LOG_ERROR(tag, fmt, ...)  dk_log(DK_LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define DK_LOG_FATAL(tag, fmt, ...) dk_log(DK_LOG_LEVEL_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 模块标签
#define LOG_TAG_INIT     "INIT"
#define LOG_TAG_KEYMGR   "KEYMGR"
#define LOG_TAG_AUTH     "AUTH"
#define LOG_TAG_BLE      "BLE"
#define LOG_TAG_NFC      "NFC"
#define LOG_TAG_UWB      "UWB"
#define LOG_TAG_SEC      "SEC"
#define LOG_TAG_PROTO    "PROTO"
#define LOG_TAG_MQTT     "MQTT"

// 示例
DK_LOG_INFO(LOG_TAG_KEYMGR, "Key created: id=%s, dur=%ums", key_id, duration_ms);
DK_LOG_ERROR(LOG_TAG_SEC, "Auth failed: err=0x%04X, key_id=%s", err_code, key_id);
```

### 7.3 埋点统一接口

**Go (Cloud)**:
```go
type Telemetry interface {
    Track(event string, properties map[string]interface{})
    TrackError(code uint16, err error, context map[string]interface{})
    SetUser(userID string)
    SetDevice(deviceID string)
}

// 使用示例
telemetry.Track("key_use", map[string]interface{}{
    "key_id": keyID,
    "vehicle_id": vehicleID,
    "channel_type": "BLE",
    "duration_ms": duration,
})
```

**Kotlin (Android)**:
```kotlin
object DkTelemetry {
    fun track(event: String, properties: Map<String, Any?> = emptyMap())
    fun trackError(code: Int, error: Throwable? = null, context: Map<String, Any?> = emptyMap())
    fun setUser(userId: String)
    fun setDevice(deviceId: String)
    
    // 便捷方法
    fun trackKeyUse(keyId: String, vehicleId: String, channel: String, durationMs: Long)
    fun trackVehicleCommand(action: String, vehicleId: String, result: String, errorCode: Int?)
    fun trackSecurityEvent(eventType: String, threatLevel: Int, details: Map<String, Any?>)
}

// 使用示例
DkTelemetry.trackKeyUse(keyId, vehicleId, "BLE", 150)
```

**Swift (iOS)**:
```swift
struct DkTelemetry {
    static func track(_ event: String, properties: [String: Any] = [:])
    static func trackError(code: Int, error: Error?, context: [String: Any] = [:])
    static func setUser(_ userId: String)
    static func setDevice(_ deviceId: String)
    
    // 便捷方法
    static func trackKeyUse(keyId: String, vehicleId: String, channel: String, durationMs: Int)
    static func trackVehicleCommand(action: String, vehicleId: String, result: String, errorCode: Int?)
}
```

**C (Vehicle)**:
```c
// 埋点事件类型
typedef enum {
    DK_EVENT_KEY_USE,
    DK_EVENT_VEHICLE_UNLOCK,
    DK_EVENT_VEHICLE_LOCK,
    DK_EVENT_VEHICLE_START,
    DK_EVENT_BLE_CONNECT,
    DK_EVENT_BLE_DISCONNECT,
    DK_EVENT_NFC_TAP,
    DK_EVENT_UWB_RANGING,
    DK_EVENT_SECURITY_ALERT,
    DK_EVENT_ERROR,
} dk_event_type_t;

// 埋点接口
void dk_telemetry_track(dk_event_type_t event, const char *trace_id, uint32_t props_count, ...);
void dk_telemetry_set_device_id(const char *device_id);
void dk_telemetry_set_vehicle_id(const char *vehicle_id);

// 便捷宏
#define TELEMETRY_KEY_USE(key_id, vehicle_id, channel, duration_ms) \
    dk_telemetry_track(DK_EVENT_KEY_USE, get_trace_id(), 4, \
        "key_id", key_id, \
        "vehicle_id", vehicle_id, \
        "channel_type", channel, \
        "duration_ms", duration_ms)

#define TELEMETRY_ERROR(code, msg) \
    dk_telemetry_track(DK_EVENT_ERROR, get_trace_id(), 3, \
        "error_code", code, \
        "error_msg", msg, \
        "file", __FILE__)

// 使用示例
TELEMETRY_KEY_USE("KEY123", "VH789", "BLE", 150);
TELEMETRY_ERROR(0xAC06, "Auth failed");
```

---

## 八、版本演进

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
