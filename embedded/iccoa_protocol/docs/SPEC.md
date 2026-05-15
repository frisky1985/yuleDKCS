# ICCOA数字钥匙协议栈技术规格书

## 文档信息

| 项目 | 内容 |
|------|------|
| 产品名称 | ICCOA Digital Key Protocol Stack |
| 规范版本 | ICCOA DK 3.0 / DK 4.0 |
| 文档版本 | V1.0 |
| 日期 | 2026-05-06 |
| 状态 | 设计完成 |

---

## 1. 概述

ICCOA (智慧车联开放联盟) 由小米、OPPO、vivo三家手机厂商发起，联合国内主流车企制定数字钥匙规范。本协议栈实现车端ICCOA DK 3.0和DK 4.0协议。

### 1.1 ICCOA vs CCC对比

| 特性 | CCC | ICCOA |
|------|-----|-------|
| 发起方 | 国际车企+手机厂商 | 国内手机厂商+车企 |
| 通信方式 | NFC+BLE+UWB | BLE为主 |
| 密钥体系 | Applet+证书链 | 简化密钥体系 |
| 手机生态 | Apple/Google | 小米/OPPO/vivo |
| NFC依赖 | 必须 | 可选 |

### 1.2 硬件平台

| 组件 | 型号 | 角色 |
|------|------|------|
| BLE芯片 | NXP KW47A | 蓝牙5.0 LE通信 |
| 安全芯片 | NXP SE050 | 密钥存储/加密 |

---

## 2. 蓝牙通信层

### 2.1 ICCOA BLE广播格式

```c
// ICCOA BLE广播数据
typedef struct __attribute__((packed)) {
    uint8_t  flags_len;        // Flags长度
    uint8_t  flags_type;       // 0x01 (Flags)
    uint8_t  flags;            // 0x06 (LE General Discoverable + BR/EDR Not Supported)
    uint8_t  svc_len;          // Service UUID长度
    uint8_t  svc_type;         // 0x03 (Complete 16-bit Service UUID)
    uint8_t  svc_uuid[2];      // ICCOA Service UUID
    uint8_t  mfg_len;          // Manufacturer Data长度
    uint8_t  mfg_type;         // 0xFF (Manufacturer Specific)
    uint8_t  mfg_id[2];        // 厂商ID
    uint8_t  vehicle_id[6];    // 车辆标识
    uint8_t  protocol_ver;     // 协议版本
    uint8_t  capability;       // 设备能力
} iccoa_adv_data_t;
```

### 2.2 GATT服务定义

```c
// ICCOA Digital Key Service
#define ICCOA_DK_SERVICE_UUID        0xFEF5

// Characteristics
#define ICCOA_DK_CHAR_CTRL           0xFEF6  // 控制通道
#define ICCOA_DK_CHAR_DATA           0xFEF7  // 数据通道
#define ICCOA_DK_CHAR_NOTIFY         0xFEF8  // 通知通道
#define ICCOA_DK_CHAR_AUTH           0xFEF9  // 认证通道
```

---

## 3. ICCOA DK 3.0协议

### 3.1 协议帧格式

```c
typedef struct __attribute__((packed)) {
    uint8_t  sop;              // Start of Packet: 0xAA
    uint8_t  cmd_id;           // 命令ID
    uint16_t seq_num;          // 序列号
    uint16_t payload_len;      // 负载长度
    uint8_t  payload[];        // 负载数据
    uint8_t  checksum;         // 校验和 (XOR)
    uint8_t  eop;              // End of Packet: 0x55
} iccoa_dk30_frame_t;

// DK 3.0命令定义
typedef enum {
    ICCOA_CMD_BIND_REQUEST      = 0x01,
    ICCOA_CMD_BIND_RESPONSE     = 0x02,
    ICCOA_CMD_UNBIND_REQUEST    = 0x03,
    ICCOA_CMD_UNBIND_RESPONSE   = 0x04,
    ICCOA_CMD_AUTH_REQUEST      = 0x10,
    ICCOA_CMD_AUTH_RESPONSE     = 0x11,
    ICCOA_CMD_CTRL_REQUEST      = 0x20,
    ICCOA_CMD_CTRL_RESPONSE     = 0x21,
    ICCOA_CMD_STATUS_NOTIFY     = 0x30,
    ICCOA_CMD_KEY_SHARE         = 0x40,
    ICCOA_CMD_KEY_SHARE_ACK     = 0x41,
    ICCOA_CMD_ERROR             = 0xFF
} iccoa_dk30_cmd_e;
```

### 3.2 绑定流程

```
手机                              车端
  │   BLE Scan + Connect            │
  │◄───────────────────────────────►│
  │                                  │
  │   BIND_REQUEST (手机公钥+签名)  │
  ├─────────────────────────────────►│
  │                                  │
  │   BIND_RESPONSE (车端公钥+签名) │
  │◄─────────────────────────────────┤
  │                                  │
  │   密钥协商 (ECDH P-256)         │
  │◄────────────────────────────────►│
  │                                  │
  │   绑定完成                       │
```

---

## 4. ICCOA DK 4.0协议

### 4.1 DK 4.0增强特性

- **UWB支持**: 增加UWB测距能力
- **多设备**: 支持多手机同时连接
- **远程分享**: 基于云端的密钥分享
- **数字身份**: 强化身份认证

### 4.2 DK 4.0帧格式

```c
typedef struct __attribute__((packed)) {
    uint16_t magic;            // 0xICC0
    uint8_t  version;          // 协议版本: 4
    uint8_t  msg_type;         // 消息类型
    uint16_t msg_id;           // 消息ID
    uint16_t flags;            // 标志位
    uint16_t payload_len;      // 负载长度
    uint8_t  session_token[4]; // 会话令牌
    uint8_t  payload[];        // 负载数据
    uint8_t  hmac[16];         // HMAC-SHA256截断
} iccoa_dk40_frame_t;
```

### 4.3 DK 4.0命令

```c
typedef enum {
    ICCOA_V4_CMD_SESSION_OPEN   = 0x01,
    ICCOA_V4_CMD_SESSION_CLOSE  = 0x02,
    ICCOA_V4_CMD_BIND           = 0x10,
    ICCOA_V4_CMD_AUTH           = 0x20,
    ICCOA_V4_CMD_CTRL           = 0x30,
    ICCOA_V4_CMD_UWB_CONFIG     = 0x40,
    ICCOA_V4_CMD_SHARE          = 0x50,
    ICCOA_V4_CMD_NOTIFY         = 0x60,
    ICCOA_V4_CMD_ERROR          = 0xFF
} iccoa_dk40_cmd_e;
```

---

## 5. 授权认证模块

### 5.1 认证流程

```c
typedef struct __attribute__((packed)) {
    uint8_t  auth_type;          // 认证类型
    uint8_t  user_id[16];        // 用户标识
    uint8_t  challenge[16];      // 挑战随机数
    uint8_t  response[32];       // 响应 (签名)
    uint8_t  cert[256];          // 用户证书
    uint16_t cert_len;
} iccoa_auth_data_t;

// 认证类型
typedef enum {
    ICCOA_AUTH_BIND         = 0x01,  // 绑定认证
    ICCOA_AUTH_DAILY        = 0x02,  // 日常认证
    ICCOA_AUTH_REMOTE       = 0x03,  // 远程认证
    ICCOA_AUTH_SHARE        = 0x04   // 分享认证
} iccoa_auth_type_e;
```

### 5.2 权限管理

```c
// ICCOA权限定义
#define ICCOA_PERM_LOCK         (1 << 0)
#define ICCOA_PERM_UNLOCK       (1 << 1)
#define ICCOA_PERM_ENGINE       (1 << 2)
#define ICCOA_PERM_TRUNK        (1 << 3)
#define ICCOA_PERM_WINDOW       (1 << 4)
#define ICCOA_PERM_CLIMATE      (1 << 5)
#define ICCOA_PERM_FIND         (1 << 6)
#define ICCOA_PERM_SEAT         (1 << 7)

typedef struct {
    uint8_t  user_id[16];
    uint8_t  permissions[4];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t  max_uses;        // 最大使用次数 (0=无限)
    uint8_t  used_count;      // 已使用次数
} iccoa_permission_t;
```

---

## 6. 互联服务模块

### 6.1 车辆控制接口

```c
typedef enum {
    CTRL_LOCK       = 0x01,
    CTRL_UNLOCK     = 0x02,
    CTRL_ENGINE_ON  = 0x03,
    CTRL_ENGINE_OFF = 0x04,
    CTRL_TRUNK_OPEN = 0x05,
    CTRL_WINDOW_UP  = 0x06,
    CTRL_WINDOW_DOWN= 0x07,
    CTRL_CLIMATE_ON = 0x08,
    CTRL_CLIMATE_OFF= 0x09,
    CTRL_FIND       = 0x0A,
    CTRL_HORN       = 0x0B
} iccoa_ctrl_cmd_e;

typedef struct __attribute__((packed)) {
    uint8_t  ctrl_cmd;
    uint8_t  ctrl_param;
    uint8_t  result;
    uint8_t  reserved;
} iccoa_ctrl_frame_t;
```

### 6.2 状态同步

```c
typedef struct __attribute__((packed)) {
    uint8_t  door_status;      // 门状态 (4 bit per door)
    uint8_t  window_status;    // 窗户状态
    uint8_t  engine_status;    // 发动机状态
    uint8_t  lock_status;      // 锁状态
    int8_t   battery_level;    // 电池电量
    int16_t  interior_temp;    // 车内温度 (0.1°C)
    uint8_t  alarm_status;     // 报警状态
    uint8_t  reserved[3];
} iccoa_vehicle_status_t;
```
