# 统一错误码规范 v2.0

**版本**: 2.0.0
**日期**: 2026-05-06
**状态**: 正式版 (三端统一)

---

## 概述

本规范定义了数字钥匙系统中三端（Cloud/SDK/Vehicle）统一的错误码体系。

---

## 1. 错误码结构

```
┌────────────────────────────────────────────────────────┐
│                    Error Code (4字节/16位)               │
├─────────────────────┬──────────────────────────────────┤
│   Category (高8位)  │          Code (低8位)             │
│   错误类别          │          具体错误码               │
└─────────────────────┴──────────────────────────────────┘
```

**Category计算**: `Category = ErrorCode >> 8`
**具体Code**: `Code = ErrorCode & 0xFF`

---

## 2. 错误类别定义

| Category | 值 | 名称 | 说明 |
|----------|-----|------|------|
| SUCCESS | 0x00 | 成功 | 成功类 |
| REQUEST | 0x01 | 请求 | 请求参数类错误 |
| AUTH | 0x02 | 认证 | 认证鉴权类错误 |
| KEY | 0x03 | 密钥 | 密钥管理类错误 |
| VEHICLE | 0x04 | 车辆 | 车辆控制类错误 |
| SHARE | 0x05 | 分享 | 分享业务类错误 |
| DEVICE | 0x06 | 设备 | 设备硬件类错误 |
| VENDOR | 0x07 | 厂商 | 厂商适配类错误 |
| TRANSPORT | 0x08 | 通信 | 通信链路类错误 |
| SYSTEM | 0x09 | 系统 | 系统内部类错误 |
| TCU | 0xAC | TCU | 车端专用错误 |

---

## 3. 成功类 (0x0000-0x00FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 0 | 0x0000 | SUCCESS | 200 | 成功 |
| 1 | 0x0001 | SUCCESS_ASYNC | 202 | 异步成功，已接收 |
| 2 | 0x0002 | SUCCESS_PARTIAL | 206 | 部分成功 |

---

## 4. 请求错误类 (0x0100-0x01FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 257 | 0x0101 | ERR_INVALID_REQUEST | 400 | 无效请求 |
| 258 | 0x0102 | ERR_INVALID_PARAMETER | 400 | 参数错误 |
| 259 | 0x0103 | ERR_MISSING_PARAMETER | 400 | 缺少参数 |
| 260 | 0x0104 | ERR_INVALID_FORMAT | 400 | 格式错误 |
| 261 | 0x0105 | ERR_INVALID_LENGTH | 400 | 长度错误 |
| 262 | 0x0106 | ERR_INVALID_SIGNATURE | 400 | 签名无效 |
| 263 | 0x0107 | ERR_RATE_LIMIT | 429 | 请求过于频繁 |
| 264 | 0x0108 | ERR_QUOTA_EXCEEDED | 429 | 配额超限 |
| 265 | 0x0109 | ERR_VERSION_MISMATCH | 400 | 版本不匹配 |

---

## 5. 认证鉴权类 (0x0200-0x02FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 513 | 0x0201 | ERR_UNAUTHORIZED | 401 | 未授权 |
| 514 | 0x0202 | ERR_INVALID_TOKEN | 401 | Token无效 |
| 515 | 0x0203 | ERR_TOKEN_EXPIRED | 401 | Token过期 |
| 516 | 0x0204 | ERR_FORBIDDEN | 403 | 禁止访问 |
| 517 | 0x0205 | ERR_ACCESS_DENIED | 403 | 权限不足 |
| 518 | 0x0206 | ERR_USER_DISABLED | 403 | 用户已禁用 |
| 519 | 0x0207 | ERR_ACCOUNT_LOCKED | 403 | 账户已锁定 |
| 520 | 0x0208 | ERR_SESSION_EXPIRED | 401 | 会话已过期 |

---

## 6. 密钥管理类 (0x0300-0x03FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 769 | 0x0301 | ERR_KEY_NOT_FOUND | 404 | 密钥不存在 |
| 770 | 0x0302 | ERR_KEY_EXISTS | 409 | 密钥已存在 |
| 771 | 0x0303 | ERR_KEY_EXPIRED | 403 | 密钥已过期 |
| 772 | 0x0304 | ERR_KEY_REVOKED | 403 | 密钥已撤销 |
| 773 | 0x0305 | ERR_KEY_SUSPENDED | 403 | 密钥已挂起 |
| 774 | 0x0306 | ERR_KEY_PENDING | 409 | 密钥待激活 |
| 775 | 0x0307 | ERR_KEY_MAX_REACHED | 409 | 密钥数量达上限 |
| 776 | 0x0308 | ERR_KEY_MAX_USES | 403 | 使用次数已用完 |
| 777 | 0x0309 | ERR_DISTANCE_EXCEEDED | 403 | 距离超出限制 |
| 778 | 0x030A | ERR_KEY_TYPE_NOT_ALLOWED | 403 | 密钥类型不允许 |
| 779 | 0x030B | ERR_KEY_NOT_ACTIVE | 403 | 密钥未激活 |
| 780 | 0x030C | ERR_KEY_VALIDITY_NOT_STARTED | 403 | 密钥未到生效时间 |
| 781 | 0x030D | ERR_KEY_BIND_FAILED | 500 | 密钥绑定失败 |
| 782 | 0x030E | ERR_KEY_UNBIND_FAILED | 500 | 密钥解绑失败 |

---

## 7. 车辆控制类 (0x0400-0x04FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 1025 | 0x0401 | ERR_VEHICLE_NOT_FOUND | 404 | 车辆不存在 |
| 1026 | 0x0402 | ERR_VEHICLE_OFFLINE | 503 | 车辆离线 |
| 1027 | 0x0403 | ERR_VEHICLE_NOT_BOUND | 404 | 车辆未绑定 |
| 1028 | 0x0404 | ERR_TCU_OFFLINE | 503 | TCU离线 |
| 1029 | 0x0405 | ERR_TCU_TIMEOUT | 504 | TCU超时 |
| 1030 | 0x0406 | ERR_TCU_ERROR | 500 | TCU内部错误 |
| 1031 | 0x0407 | ERR_COMMAND_FAILED | 500 | 指令执行失败 |
| 1032 | 0x0408 | ERR_COMMAND_TIMEOUT | 504 | 指令执行超时 |
| 1033 | 0x0409 | ERR_COMMAND_REJECTED | 403 | 指令被拒绝 |
| 1034 | 0x040A | ERR_COMMAND_INVALID | 400 | 指令无效 |
| 1035 | 0x040B | ERR_COMMAND_IN_PROGRESS | 409 | 指令执行中 |
| 1036 | 0x040C | ERR_VEHICLE_NOT_SUPPORTED | 400 | 车辆不支持该操作 |
| 1037 | 0x040D | ERR_BATTERY_LOW | 403 | 电瓶电量低 |
| 1038 | 0x040E | ERR_ENGINE_RUNNING | 403 | 引擎运行中 |
| 1039 | 0x040F | ERR_VEHICLE_MOVING | 403 | 车辆正在移动 |
| 1040 | 0x0410 | ERR_DOOR_OPEN | 403 | 车门未关闭 |

---

## 8. 分享业务类 (0x0500-0x05FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 1281 | 0x0501 | ERR_SHARE_NOT_FOUND | 404 | 分享不存在 |
| 1282 | 0x0502 | ERR_SHARE_EXPIRED | 403 | 分享已过期 |
| 1283 | 0x0503 | ERR_SHARE_ACCEPTED | 409 | 分享已被接受 |
| 1284 | 0x0504 | ERR_SHARE_CANCELLED | 409 | 分享已取消 |
| 1285 | 0x0505 | ERR_SHARE_MAX_USES | 403 | 分享次数已用完 |
| 1286 | 0x0506 | ERR_SHARE_NOT_ALLOWED | 403 | 不允许分享 |
| 1287 | 0x0507 | ERR_CODE_INVALID | 400 | 分享码无效 |
| 1288 | 0x0508 | ERR_CODE_EXPIRED | 403 | 分享码过期 |
| 1289 | 0x0509 | ERR_VENDOR_NOT_MATCH | 403 | 厂商不匹配 |
| 1290 | 0x050A | ERR_SHARE_TO_SELF | 400 | 不能分享给自己 |

---

## 9. 设备硬件类 (0x0600-0x06FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 1537 | 0x0601 | ERR_DEVICE_NOT_FOUND | 404 | 设备不存在 |
| 1538 | 0x0602 | ERR_DEVICE_NOT_BOUND | 404 | 设备未绑定 |
| 1539 | 0x0603 | ERR_DEVICE_DISABLED | 403 | 设备已禁用 |
| 1540 | 0x0604 | ERR_DEVICE_NOT_SUPPORTED | 400 | 设备不支持 |
| 1541 | 0x0605 | ERR_BLE_DISABLED | 400 | 蓝牙未开启 |
| 1542 | 0x0606 | ERR_NFC_DISABLED | 400 | NFC未开启 |
| 1543 | 0x0607 | ERR_UWB_DISABLED | 400 | UWB未开启 |
| 1544 | 0x0608 | ERR_SE_NOT_AVAILABLE | 400 | 安全元件不可用 |
| 1545 | 0x0609 | ERR_SE_ERROR | 500 | SE通信错误 |
| 1546 | 0x060A | ERR_STORAGE_FULL | 400 | 存储满 |
| 1547 | 0x060B | ERR_BLE_SCAN_FAILED | 500 | BLE扫描失败 |
| 1548 | 0x060C | ERR_BLE_CONNECT_FAILED | 500 | BLE连接失败 |
| 1549 | 0x060D | ERR_NFC_READ_FAILED | 500 | NFC读取失败 |
| 1550 | 0x060E | ERR_NFC_WRITE_FAILED | 500 | NFC写入失败 |
| 1551 | 0x060F | ERR_UWB_RANGING_FAILED | 500 | UWB测距失败 |

---

## 10. 厂商适配类 (0x0700-0x07FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 1793 | 0x0701 | ERR_VENDOR_NOT_SUPPORTED | 400 | 厂商不支持 |
| 1794 | 0x0702 | ERR_ADAPTER_NOT_FOUND | 500 | 适配器未找到 |
| 1795 | 0x0703 | ERR_VENDOR_API_ERROR | 502 | 厂商API错误 |
| 1796 | 0x0704 | ERR_VENDOR_API_TIMEOUT | 504 | 厂商API超时 |
| 1797 | 0x0705 | ERR_VENDOR_AUTH_FAILED | 401 | 厂商认证失败 |
| 1798 | 0x0706 | ERR_VENDOR_BIND_FAILED | 500 | 厂商绑定失败 |
| 1799 | 0x0707 | ERR_PROTOCOL_ERROR | 400 | 协议错误 |

---

## 11. 通信链路类 (0x0800-0x08FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 2049 | 0x0801 | ERR_NETWORK_ERROR | 0 | 网络错误 |
| 2050 | 0x0802 | ERR_NETWORK_TIMEOUT | 0 | 网络超时 |
| 2051 | 0x0803 | ERR_SERVER_UNREACHABLE | 0 | 服务器不可达 |
| 2052 | 0x0804 | ERR_CONNECTION_REFUSED | 0 | 连接被拒绝 |
| 2053 | 0x0805 | ERR_MQTT_DISCONNECTED | 0 | MQTT断开 |
| 2054 | 0x0806 | ERR_MQTT_PUBLISH_FAILED | 0 | MQTT发布失败 |
| 2055 | 0x0807 | ERR_GRPC_UNAVAILABLE | 0 | gRPC不可用 |
| 2056 | 0x0808 | ERR_GRPC_DEADLINE | 0 | gRPC超时 |

---

## 12. 系统内部类 (0x0900-0x09FF)

| 错误码 | 十六进制 | 枚举名 | HTTP | 说明 |
|--------|----------|--------|------|------|
| 2305 | 0x0901 | ERR_INTERNAL_ERROR | 500 | 内部错误 |
| 2306 | 0x0902 | ERR_SERVICE_UNAVAILABLE | 503 | 服务不可用 |
| 2307 | 0x0903 | ERR_DATABASE_ERROR | 500 | 数据库错误 |
| 2308 | 0x0904 | ERR_CACHE_ERROR | 500 | 缓存错误 |
| 2309 | 0x0905 | ERR_QUEUE_ERROR | 500 | 队列错误 |
| 2310 | 0x0906 | ERR_CONFIG_ERROR | 500 | 配置错误 |
| 2311 | 0x0907 | ERR_CRYPTO_ERROR | 500 | 加密错误 |
| 2312 | 0x0908 | ERR_SIGN_ERROR | 500 | 签名错误 |
| 2313 | 0x0909 | ERR_VERIFY_ERROR | 500 | 验签错误 |
| 2314 | 0x090A | ERR_FEATURE_NOT_SUPPORTED | 501 | 功能不支持 |
| 2315 | 0x090B | ERR_MAINTENANCE | 503 | 系统维护 |
| 2316 | 0x090C | ERR_CAPACITY_EXCEEDED | 503 | 容量超限 |

---

## 13. TCU车端专用类 (0xAC00-0xACFF)

| 错误码 | 十六进制 | 枚举名 | 说明 |
|--------|----------|--------|------|
| 44033 | 0xAC01 | ERR_TCU_BLE_ERROR | BLE通信错误 |
| 44034 | 0xAC02 | ERR_TCU_NFC_ERROR | NFC通信错误 |
| 44035 | 0xAC03 | ERR_TCU_UWB_ERROR | UWB通信错误 |
| 44036 | 0xAC04 | ERR_TCU_SE_ERROR | SE通信错误 |
| 44037 | 0xAC05 | ERR_TCU_PAIRING_FAILED | 配对失败 |
| 44038 | 0xAC06 | ERR_TCU_AUTH_FAILED | 认证失败 |
| 44039 | 0xAC07 | ERR_TCU_CRYPTO_FAILED | 加密失败 |
| 44040 | 0xAC08 | ERR_TCU_STORAGE_ERROR | 存储错误 |
| 44041 | 0xAC09 | ERR_TCU_POWER_LOW | 电量低 |
| 44042 | 0xAC0A | ERR_TCU_WATCHDOG_RESET | 看门狗复位 |
| 44043 | 0xAC0B | ERR_TCU_ANTENNA_FAULT | 天线故障 |
| 44044 | 0xAC0C | ERR_TCU_MSG_QUEUE_FULL | 消息队列满 |

---

## 14. 错误响应格式

### 14.1 Cloud端REST API响应

```json
{
  "code": 307,
  "message": "Distance exceeded",
  "trace_id": "sdk-abc123def456",
  "timestamp": 1714963200,
  "data": null
}
```

### 14.2 Cloud端gRPC响应

```protobuf
message Response {
    uint32 code = 1;      // 错误码
    string message = 2;   // 错误信息
    string trace_id = 3;  // 追踪ID
    bytes data = 4;       // 响应数据
}
```

### 14.3 SDK端响应

```json
{
  "code": 307,
  "message": "距离超出限制",
  "details": {
    "max_distance_mm": 5000,
    "actual_distance_mm": 8500
  },
  "trace_id": "sdk-abc123"
}
```

### 14.4 Vehicle端日志输出

```
[2026-05-06 09:53:00.123][ERR][KEYMGR][T:abc123] Distance exceeded: max=5000mm, actual=8500mm (0x0309)
```

---

## 15. 错误码常量定义文件

### Go (Cloud)
见: `cloud/hub/internal/error/error.go`

### Kotlin (Android)
见: `phone/android/src/main/kotlin/com/digitalkey/sdk/error/DkError.kt`

### Swift (iOS)
见: `phone/ios/Sources/DigitalKeySDK/Error/DkError.swift`

### C (Vehicle)
见: `ccc_protocol/src/error/dk_error.h`

---

## 16. 版本演进

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
| 2.0.0 | 2026-05-06 | 三端统一版本 |
