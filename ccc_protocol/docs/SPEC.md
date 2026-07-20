# CCC数字钥匙协议栈技术规格书

## 文档信息

| 项目 | 内容 |
|------|------|
| 产品名称 | CCC Digital Key Protocol Stack |
| 规范版本 | CCC Digital Key Release 3.0 |
| 文档版本 | V1.0 |
| 日期 | 2026-05-06 |
| 状态 | 设计完成 |

---

## 1. 概述

### 1.1 规范范围

本协议栈基于CCC (Car Connectivity Consortium) Digital Key Release 3.0规范，实现车端数字钥匙的完整功能，包括：

- NFC近场通信（触碰解锁/启动）
- BLE蓝牙通信（数据传输/远程控制）
- UWB超宽带通信（精确测距/被动解锁）
- 数字密钥管理（创建/删除/分享/挂失）
- 安全通道（SCP03/Attestation/验签）

### 1.2 硬件平台

| 组件 | 型号 | 接口 | 角色 |
|------|------|------|------|
| BLE芯片 | NXP KW47A | SPI + UART | 蓝牙5.0 LE通信 |
| UWB芯片 | NXP NCJ29D6 | SPI | IEEE 802.15.4z UWB |
| NFC芯片 | ST ST25R501 | SPI + IRQ | ISO 14443/NFC-F |
| 安全芯片 | NXP SE050 | I2C | 密钥存储/加密 |
| 主MCU | STM32L5 | - | 系统主控 |

### 1.3 通信架构

```
手机端                          车端
┌──────────┐                ┌──────────────────────────────────────┐
│          │    NFC         │  ST25R501 ←SPI→ NFC Driver          │
│  CCC     │◄──────────────►│                                      │
│  Digital │    BLE         │  KW47A   ←SPI→ BLE Driver           │
│  Key     │◄──────────────►│                                      │
│  App     │    UWB         │  NCJ29D6 ←SPI→ UWB Driver           │
│          │◄──────────────►│                                      │
│          │                │  SE050   ←I2C→ Security Driver      │
└──────────┘                └──────────────────────────────────────┘
```

---

## 2. NFC通信层 (ST ST25R501)

### 2.1 功能需求

| 功能 | 描述 | 优先级 |
|------|------|--------|
| NFC场检测 | 13.56MHz场检测与唤醒 | P0 |
| ISO 14443-A | Type A卡通信 | P0 |
| NFC-F (Type F) | FeliCa协议通信（CCC要求） | P0 |
| NDEF解析 | NFC Data Exchange Format | P0 |
| 触碰配对 | NFC-F OOB配对数据交换 | P0 |
| 数据收发 | 密钥传输通道 | P1 |

### 2.2 ST25R501硬件接口

```c
// ST25R501 SPI接口定义
#define ST25R501_SPI_INSTANCE   SPI2
#define ST25R501_SPI_CLK        4000000     // 4MHz SPI时钟
#define ST25R501_CS_PORT        GPIOB
#define ST25R501_CS_PIN         GPIO_PIN_12
#define ST25R501_IRQ_PORT       GPIOB
#define ST25R501_IRQ_PIN        GPIO_PIN_1
#define ST25R501_RST_PORT       GPIOB
#define ST25R501_RST_PIN        GPIO_PIN_0
```

### 2.3 NFC状态机

```
IDLE → FIELD_DETECT → ANTI_COLLISION → SELECT → DATA_EXCHANGE → COMPLETE
  ↑                                                          │
  └──────────────────────────────────────────────────────────┘
```

### 2.4 NFC-F触碰配对流程

```
1. 手机靠近 → ST25R501检测到NFC场
2. 车端发送ATR_REQ (Attribution Request)
3. 手机响应ATR_RES (Attribution Response)
4. 交换OOB配对数据 (含BLE MAC地址/UWB参数)
5. 触发BLE连接建立
6. 完成配对
```

### 2.5 NDEF消息格式

```c
// CCC Digital Key NDEF Record
typedef struct {
    uint8_t  tnf;           // Type Name Format: NFC Forum Well-Known Type
    uint8_t  type_len;      // Type Length
    uint8_t  payload_len[4]; // Payload Length (4 bytes for long NDEF)
    uint8_t  type;          // "U" for URI
    uint8_t  uri_code;      // URI Identifier Code
    uint8_t  uri[];         // URI: ccc://dk/<vehicle_id>/<config_hash>
} ccc_ndef_record_t;

// CCC配对数据结构
typedef struct __attribute__((packed)) {
    uint16_t version;               // CCC DK版本
    uint8_t  ble_mac[6];           // BLE MAC地址
    uint8_t  uwb_session_id[8];    // UWB Session ID
    uint8_t  uwb_channel;          // UWB信道
    uint8_t  uwb_preamble_code;    // UWB前导码
    uint8_t  capability[4];        // 设备能力
    uint8_t  oob_data[32];         // OOB配对数据
} ccc_nfc_oob_data_t;
```

---

## 3. BLE蓝牙层 (NXP KW47A)

### 3.1 功能需求

| 功能 | 描述 | 优先级 |
|------|------|--------|
| BLE广播 | Digital Key Service广播 | P0 |
| BLE扫描 | 扫描手机端广播 | P0 |
| GATT Server | 数字钥匙GATT服务 | P0 |
| OOB配对 | NFC辅助OOB配对 | P0 |
| 加密连接 | LE Secure Connections | P0 |
| 数据传输 | 密钥/命令/状态数据 | P1 |

### 3.2 KW47A硬件接口

```c
// KW47A接口定义
#define KW47A_SPI_INSTANCE    SPI1
#define KW47A_SPI_CLK         8000000     // 8MHz SPI时钟
#define KW47A_CS_PORT         GPIOA
#define KW47A_CS_PIN          GPIO_PIN_4
#define KW47A_IRQ_PORT        GPIOA
#define KW47A_IRQ_PIN         GPIO_PIN_0
#define KW47A_RST_PORT        GPIOA
#define KW47A_RST_PIN         GPIO_PIN_1
#define KW47A_WAKE_PORT       GPIOA
#define KW47A_WAKE_PIN        GPIO_PIN_2

// KW47A UART (用于调试)
#define KW47A_UART_INSTANCE   USART1
#define KW47A_UART_BAUD       115200
```

### 3.3 GATT服务定义 (CCC Digital Key Service)

```c
// CCC Digital Key Service UUID (CCC规范定义)
#define CCC_DK_SERVICE_UUID       0xEFFF  // CCC DK Service

// Characteristic UUIDs
#define CCC_DK_CHAR_PAIRING       0xEFFE  // 配对控制
#define CCC_DK_CHAR_KEY_DATA      0xEFFD  // 密钥数据
#define CCC_DK_CHAR_AUTH          0xEFFC  // 认证数据
#define CCC_DK_CHAR_STATE         0xEFFB  // 钥匙状态
#define CCC_DK_CHAR_UWB_CONFIG    0xEFFA  // UWB配置
#define CCC_DK_CHAR_RSSI          0xEFF9  // RSSI测距

// GATT服务结构
typedef struct {
    // Service
    uint16_t service_handle;
    
    // Characteristics
    struct {
        uint16_t pairing_handle;
        uint16_t key_data_handle;
        uint16_t auth_handle;
        uint16_t state_handle;
        uint16_t uwb_config_handle;
        uint16_t rssi_handle;
    } char_handles;
    
    // CCC Descriptors
    struct {
        uint16_t pairing_ccc;
        uint16_t key_data_ccc;
        uint16_t auth_ccc;
        uint16_t state_ccc;
        uint16_t uwb_config_ccc;
    } ccc_handles;
} ccc_dk_gatt_t;
```

### 3.4 BLE配对流程

```
┌────────┐                          ┌────────┐
│  手机   │                          │  车端   │
└───┬────┘                          └───┬────┘
    │    BLE Scan Request              │
    │◄─────────────────────────────────┤
    │                                  │
    │    BLE Scan Response (含DK UUID) │
    ├─────────────────────────────────►│
    │                                  │
    │    BLE Connect Request           │
    │◄─────────────────────────────────┤
    │                                  │
    │    BLE Connect Response          │
    ├─────────────────────────────────►│
    │                                  │
    │    Service Discovery             │
    │◄─────────────────────────────────┤
    │                                  │
    │    OOB Pairing (NFC辅助)         │
    │◄────────────────────────────────►│
    │                                  │
    │    LE Secure Connections         │
    │◄────────────────────────────────►│
    │                                  │
    │    Encrypted Data Exchange       │
    │◄────────────────────────────────►│
```

### 3.5 BLE数据帧格式

```c
// BLE数据帧头
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // 消息类型
    uint8_t  msg_id;        // 消息ID (递增)
    uint16_t payload_len;   // 负载长度
    uint8_t  reserved;      // 预留
} ble_frame_header_t;

// 消息类型定义
typedef enum {
    BLE_MSG_PAIR_REQUEST     = 0x01,
    BLE_MSG_PAIR_RESPONSE    = 0x02,
    BLE_MSG_KEY_CREATE       = 0x10,
    BLE_MSG_KEY_DELETE       = 0x11,
    BLE_MSG_KEY_SHARE        = 0x12,
    BLE_MSG_AUTH_REQUEST     = 0x20,
    BLE_MSG_AUTH_RESPONSE    = 0x21,
    BLE_MSG_UWB_CONFIG       = 0x30,
    BLE_MSG_STATE_NOTIFY     = 0x40,
    BLE_MSG_ERROR            = 0xFF
} ble_msg_type_e;
```

---

## 4. UWB测距层 (NXP NCJ29D6)

### 4.1 功能需求

| 功能 | 描述 | 优先级 |
|------|------|--------|
| UWB初始化 | IEEE 802.15.4z参数配置 | P0 |
| TWR测距 | 双向测距 (Two-Way Ranging) | P0 |
| 安全测距 | 加密测距 (STS加密) | P0 |
| 多锚点定位 | 支持多锚点定位 | P1 |
| 距离触发 | 进入/离开距离触发 | P0 |

### 4.2 NCJ29D6硬件接口

```c
// NCJ29D6 SPI接口定义
#define NCJ29D6_SPI_INSTANCE   SPI3
#define NCJ29D6_SPI_CLK        20000000    // 20MHz SPI时钟
#define NCJ29D6_CS_PORT        GPIOC
#define NCJ29D6_CS_PIN         GPIO_PIN_13
#define NCJ29D6_IRQ_PORT       GPIOC
#define NCJ29D6_IRQ_PIN        GPIO_PIN_4
#define NCJ29D6_RST_PORT       GPIOC
#define NCJ29D6_RST_PIN        GPIO_PIN_5
#define NCJ29D6_WAKE_PORT      GPIOC
#define NCJ29D6_WAKE_PIN       GPIO_PIN_6

// UWB频段配置
#define UWB_CHANNEL_5          5           // Channel 5 (6.5 GHz)
#define UWB_CHANNEL_9          9           // Channel 9 (8.0 GHz)
#define UWB_PRF_LEN_128        128         // 前导码长度
#define UWB_PREAMBLE_CODE_9    9           // 前导码索引9 (Channel 5)
#define UWB_PREAMBLE_CODE_12   12          // 前导码索引12 (Channel 9)
```

### 4.3 TWR测距流程

```
┌────────┐                              ┌────────┐
│  车端   │                              │  手机   │
│Initiator│                              │Responder│
└───┬────┘                              └───┬────┘
    │                                       │
    │    Poll Message (Timestamp T1)        │
    ├──────────────────────────────────────►│
    │                                       │
    │    Response Message (Timestamp T2,T3) │
    │◄──────────────────────────────────────┤
    │                                       │
    │    Final Message (Timestamp T4)       │
    ├──────────────────────────────────────►│
    │                                       │
    │    距离计算:                           │
    │    T_round1 = T4 - T1                 │
    │    T_round2 = T3 - T2                 │
    │    T_prop = (T_round1 - T_round2) / 2 │
    │    Distance = T_prop × c (光速)       │
    │                                       │
```

### 4.4 安全测距 (STS - Scrambled Timestamp Sequence)

```c
// STS安全测距配置
typedef struct {
    uint8_t  session_id[8];       // UWB Session ID
    uint8_t  sts_key[16];         // STS加密密钥
    uint8_t  sts_iv[16];         // STS初始化向量
    uint8_t  channel;             // UWB信道
    uint8_t  preamble_code;       // 前导码索引
    uint8_t  prf_len;             // 前导码长度
    uint8_t  sfd_id;              // SFD标识
    uint8_t  phr_rate;            // PHR速率
    uint8_t  data_rate;           // 数据速率
    uint8_t  tx_power;            // 发射功率
    uint8_t  rframe_config;       // 帧配置 (SP0/SP1/SP3)
    uint8_t  sts_config;          // STS配置
} uwb_session_config_t;

// STS安全测距帧
typedef struct __attribute__((packed)) {
    // PHY Header
    uint8_t  phr;
    
    // MAC Header
    uint8_t  frame_ctrl[2];
    uint8_t  seq_num;
    uint8_t  pan_id[2];
    uint8_t  src_addr[8];
    uint8_t  dst_addr[8];
    
    // STS段
    uint8_t  sts_phy_hdr;
    uint8_t  sts_data[128];       // STS加扰数据
    
    // Payload (加密)
    uint8_t  payload[];           // 测距数据
    
    // MIC
    uint8_t  mic[4];              // Message Integrity Code
} uwb_sts_frame_t;
```

### 4.5 距离判断与区域划分

```c
// 距离区域定义
typedef enum {
    ZONE_LOCKED      = 0,   // 锁车区 (>10m)
    ZONE_APPROACH    = 1,   // 接近区 (5-10m)
    ZONE_UNLOCK      = 2,   // 解锁区 (2-5m)
    ZONE_ENTRY       = 3,   // 进入区 (0-2m)
    ZONE_INSIDE      = 4    // 车内区 (<0.5m)
} distance_zone_e;

// 距离阈值配置
typedef struct {
    uint16_t approach_cm;    // 接近距离 (cm), 默认 1000
    uint16_t unlock_cm;      // 解锁距离 (cm), 默认 500
    uint16_t entry_cm;       // 进入距离 (cm), 默认 200
    uint16_t inside_cm;      // 车内距离 (cm), 默认 50
    uint16_t hysteresis_cm;  // 迟滞距离 (cm), 默认 30
} distance_threshold_t;
```

---

## 5. 数字密钥管理系统

### 5.1 密钥数据结构

```c
// CCC Digital Key 数据模型
typedef struct __attribute__((packed)) {
    // 密钥标识
    uint8_t  key_id[16];            // 密钥唯一标识 (UUID)
    uint8_t  vehicle_id[16];        // 车辆标识
    uint8_t  owner_id[16];          // 钥匙持有者标识
    
    // 密钥类型与权限
    uint8_t  key_type;              // 密钥类型 (Owner/Friend/Service)
    uint8_t  access_rights[4];      // 访问权限位图
    uint8_t  restrictions[4];       // 限制条件
    
    // 密钥有效期
    uint32_t valid_from;            // 有效开始时间 (Unix timestamp)
    uint32_t valid_until;           // 有效结束时间
    
    // 密钥状态
    uint8_t  state;                 // 密钥状态
    uint8_t  version;               // 密钥版本
    
    // 证书链
    uint8_t  ca_cert[256];          // CA证书
    uint8_t  device_cert[256];      // 设备证书
    uint8_t  attestation[128];      // 证明数据
    
    // 密钥材料 (SE050内部存储, 此处仅存引用)
    uint8_t  se_key_id;             // SE050 Key Slot ID
    uint8_t  reserved[3];           // 预留对齐
} ccc_digital_key_t;

// 密钥类型定义
typedef enum {
    KEY_TYPE_OWNER       = 0x01,    // 车主钥匙 (全部权限)
    KEY_TYPE_FRIEND      = 0x02,    // 朋友钥匙 (受限权限)
    KEY_TYPE_SERVICE     = 0x03,    // 服务钥匙 (维修/代客泊车)
    KEY_TYPE_TEMPORARY   = 0x04     // 临时钥匙 (限时)
} key_type_e;

// 访问权限位图
#define ACCESS_LOCK_UNLOCK    (1 << 0)   // 锁车/解锁
#define ACCESS_ENGINE_START   (1 << 1)   // 发动机启动
#define ACCESS_TRUNK         (1 << 2)   // 后备箱
#define ACCESS_WINDOWS       (1 << 3)   // 车窗
#define ACCESS_SUNROOF       (1 << 4)   // 天窗
#define ACCESS_CLIMATE       (1 << 5)   // 空调
#define ACCESS_SEAT          (1 << 6)   // 座椅
#define ACCESS_FUEL_DOOR     (1 << 7)   // 油箱盖

// 密钥状态
typedef enum {
    KEY_STATE_INACTIVE    = 0x00,   // 未激活
    KEY_STATE_ACTIVE      = 0x01,   // 已激活
    KEY_STATE_SUSPENDED   = 0x02,   // 已挂起
    KEY_STATE_EXPIRED     = 0x03,   // 已过期
    KEY_STATE_REVOKED     = 0x04    // 已撤销
} key_state_e;
```

### 5.2 密钥生命周期

```
                    ┌─────────┐
                    │ CREATED │
                    └────┬────┘
                         │ 配对完成
                         ▼
                    ┌─────────┐
          ┌────────│ ACTIVE  │────────┐
          │        └────┬────┘        │
          │ 挂起         │ 过期        │ 撤销
          ▼              ▼             ▼
    ┌──────────┐   ┌──────────┐  ┌──────────┐
    │SUSPENDED │   │ EXPIRED  │  │ REVOKED  │
    └────┬─────┘   └──────────┘  └──────────┘
         │ 恢复
         ▼
    ┌─────────┐
    │ ACTIVE  │
    └─────────┘
```

### 5.3 密钥分享流程

```
1. 车主发起分享 → 生成Friend Key
2. Friend Key经BLE加密通道传输给被分享者手机
3. 被分享者手机验证证书链
4. 被分享者激活Friend Key
5. 车端同步Friend Key信息
6. 分享完成，被分享者可使用受限权限
```

---

## 6. 安全模块

### 6.1 SCP03安全通道

```c
// SCP03安全通道配置
typedef struct {
    uint8_t  enc_key[16];       // 加密密钥 (SE050内部)
    uint8_t  mac_key[16];       // MAC密钥 (SE050内部)
    uint8_t  dek_key[16];       // 数据加密密钥 (SE050内部)
    uint8_t  host_challenge[8]; // 主机挑战随机数
    uint8_t  card_challenge[8]; // 卡片挑战随机数
    uint8_t  seq_counter[2];    // 序列计数器
    uint8_t  chain_mode;        // 链模式
} scp03_channel_t;

// SCP03初始化流程
// 1. 主机发送INITIALIZE UPDATE命令
// 2. SE050返回卡片挑战+序列计数器
// 3. 主机派生会话密钥 (DK)
// 4. 主机发送EXTERNAL AUTHENTICATE命令
// 5. 安全通道建立完成
```

### 6.2 Attestation (证明)

```c
// Attestation证明包
typedef struct __attribute__((packed)) {
    uint8_t  version;               // 版本号
    uint8_t  nonce[16];             // 挑战随机数
    uint8_t  device_id[16];         // 设备标识
    uint8_t  key_id[16];            // 密钥标识
    uint8_t  key_type;              // 密钥类型
    uint8_t  access_rights[4];      // 访问权限
    uint8_t  firmware_hash[32];     // 固件哈希 (SHA-256)
    uint8_t  security_state;        // 安全状态
    uint8_t  attestation_cert[256]; // 证明证书
    uint8_t  signature[64];         // ECDSA签名
} ccc_attestation_t;
```

### 6.3 车端验签流程

```c
// 车端验签流程
// 1. 接收手机端证明包
// 2. 验证证书链 (Root CA → Intermediate CA → Device Cert)
// 3. 验证签名 (ECDSA P-256)
// 4. 检查密钥权限与有效期
// 5. 检查固件哈希 (防篡改)
// 6. 返回验签结果

typedef enum {
    VERIFY_OK              = 0x00,
    VERIFY_CERT_INVALID    = 0x01,
    VERIFY_SIGN_INVALID    = 0x02,
    VERIFY_KEY_EXPIRED     = 0x03,
    VERIFY_KEY_REVOKED     = 0x04,
    VERIFY_PERMISSION_DENIED = 0x05,
    VERIFY_FW_TAMPERED     = 0x06
} verify_result_e;
```

---

## 7. 模块接口总览

### 7.1 NFC模块接口

| 接口 | 函数签名 | 描述 |
|------|---------|------|
| 初始化 | `int32_t nfc_st25r501_init(void)` | 初始化ST25R501 |
| 反初始化 | `int32_t nfc_st25r501_deinit(void)` | 关闭ST25R501 |
| 场检测 | `bool nfc_field_detect(void)` | 检测NFC场 |
| 启动监听 | `int32_t nfc_start_listen(void)` | 启动NFC监听 |
| 停止监听 | `int32_t nfc_stop_listen(void)` | 停止NFC监听 |
| 发送数据 | `int32_t nfc_send(uint8_t *data, uint16_t len)` | 发送NFC数据 |
| 接收数据 | `int32_t nfc_recv(uint8_t *buf, uint16_t *len)` | 接收NFC数据 |
| OOB配对 | `int32_t nfc_oob_exchange(ccc_nfc_oob_data_t *oob)` | 交换OOB配对数据 |

### 7.2 BLE模块接口

| 接口 | 函数签名 | 描述 |
|------|---------|------|
| 初始化 | `int32_t ble_kw47a_init(void)` | 初始化KW47A |
| 启动广播 | `int32_t ble_start_adv(ble_adv_param_t *param)` | 启动BLE广播 |
| 停止广播 | `int32_t ble_stop_adv(void)` | 停止BLE广播 |
| 连接 | `int32_t ble_connect(ble_conn_param_t *param)` | 建立BLE连接 |
| 断开 | `int32_t ble_disconnect(uint16_t conn_handle)` | 断开BLE连接 |
| 发送数据 | `int32_t ble_send(uint16_t handle, uint8_t *data, uint16_t len)` | 发送BLE数据 |
| 接收回调 | `void ble_recv_cb(uint16_t handle, uint8_t *data, uint16_t len)` | BLE接收回调 |
| OOB配对 | `int32_t ble_oob_pair(ble_oob_data_t *oob)` | NFC辅助OOB配对 |

### 7.3 UWB模块接口

| 接口 | 函数签名 | 描述 |
|------|---------|------|
| 初始化 | `int32_t uwb_ncj29d6_init(void)` | 初始化NCJ29D6 |
| 创建会话 | `int32_t uwb_create_session(uwb_session_config_t *cfg)` | 创建UWB会话 |
| 开始测距 | `int32_t uwb_start_ranging(uint32_t session_id)` | 开始TWR测距 |
| 停止测距 | `int32_t uwb_stop_ranging(uint32_t session_id)` | 停止测距 |
| 获取距离 | `int32_t uwb_get_distance(uint32_t session_id, uint16_t *dist_cm)` | 获取距离 |
| 获取区域 | `distance_zone_e uwb_get_zone(uint32_t session_id)` | 获取距离区域 |
| 销毁会话 | `int32_t uwb_destroy_session(uint32_t session_id)` | 销毁UWB会话 |

### 7.4 密钥管理接口

| 接口 | 函数签名 | 描述 |
|------|---------|------|
| 创建密钥 | `int32_t key_create(ccc_digital_key_t *key)` | 创建数字钥匙 |
| 删除密钥 | `int32_t key_delete(uint8_t *key_id)` | 删除数字钥匙 |
| 获取密钥 | `int32_t key_get(uint8_t *key_id, ccc_digital_key_t *key)` | 获取密钥信息 |
| 列出密钥 | `int32_t key_list(ccc_digital_key_t *keys, uint8_t *count)` | 列出所有密钥 |
| 分享密钥 | `int32_t key_share(uint8_t *key_id, key_type_e type, uint32_t duration)` | 分享密钥 |
| 撤销密钥 | `int32_t key_revoke(uint8_t *key_id)` | 撤销密钥 |
| 挂起密钥 | `int32_t key_suspend(uint8_t *key_id)` | 挂起密钥 |
| 恢复密钥 | `int32_t key_resume(uint8_t *key_id)` | 恢复密钥 |

### 7.5 安全模块接口

| 接口 | 函数签名 | 描述 |
|------|---------|------|
| SCP03建立 | `int32_t sec_scp03_open(scp03_channel_t *ch)` | 建立SCP03通道 |
| SCP03关闭 | `int32_t sec_scp03_close(scp03_channel_t *ch)` | 关闭SCP03通道 |
| 加密 | `int32_t sec_encrypt(uint8_t *data, uint32_t len, uint8_t *out)` | AES-256-GCM加密 |
| 解密 | `int32_t sec_decrypt(uint8_t *data, uint32_t len, uint8_t *out)` | AES-256-GCM解密 |
| 签名 | `int32_t sec_sign(uint8_t *data, uint32_t len, uint8_t *sig)` | ECDSA签名 |
| 验签 | `verify_result_e sec_verify(uint8_t *data, uint32_t len, uint8_t *sig)` | ECDSA验签 |
| Attestation | `int32_t sec_attestation(ccc_attestation_t *att)` | 生成证明包 |
| 验证证明 | `verify_result_e sec_verify_attestation(ccc_attestation_t *att)` | 验证证明包 |

---

## 8. 编译配置

### 8.1 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(ccc_digital_key C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
add_compile_options(-Wall -Wextra -Werror -Os)

# 源文件
file(GLOB_RECURSE SOURCES "src/*.c")

# 头文件路径
include_directories(
    include
    src/core
    src/nfc
    src/ble
    src/uwb
    src/keymgmt
    src/security
)

# 目标
add_library(ccc_dk STATIC ${SOURCES})
```

### 8.2 依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| STM32 HAL | 1.27+ | MCU外设驱动 |
| RFAL (NFC) | 2.4+ | ST25R501 NFC协议栈 |
| KW47A SDK | 2.x | BLE协议栈 |
| NCJ29D6 SDK | 1.x | UWB驱动 |
| SE050 Plug & Trust | 4.x | 安全芯片中间件 |

---

## 9. 版本历史

| 版本 | 日期 | 修改内容 |
|------|------|----------|
| V1.0 | 2026-05-06 | 初版发布 |
