# CCC数字钥匙协议栈 - 模块设计文档

## 1. 模块概述

本文档详细描述CCC (Car Connectivity Consortium) Digital Key Release 3.0协议栈的车端各模块设计，包括模块架构、接口定义、数据结构和关键算法。协议栈基于NXP KW47A (BLE)、NCJ29D6 (UWB) 和 ST ST25R501 (NFC) 三模硬件平台，结合SE050安全芯片实现车端数字钥匙的完整功能。

| 项目 | 内容 |
|------|------|
| 协议标准 | CCC Digital Key Release 3.0 |
| 通信方式 | NFC (ISO 14443/NFC-F) + BLE 5.0 LE + UWB (IEEE 802.15.4z) |
| 硬件平台 | STM32L5 + KW47A + NCJ29D6 + ST25R501 + SE050 |
| 安全体系 | SCP03 + ECDSA P-256 + Attestation + TFM |

## 2. 协议栈层级架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                     CCC数字钥匙应用层                                  │
│    (钥匙管理 / 车辆控制 / 状态同步 / 被动解锁 / 远程控制)               │
├─────────────────────────────────────────────────────────────────────┤
│                     CCC协议管理层                                      │
│    (配钥/认证/密钥协商/会话管理/权限验证/安全通道)                       │
├──────────────────────┬──────────────────┬───────────────────────────┤
│   NFC通信模块        │  BLE通信模块      │  UWB通信模块              │
│   (ST25R501)         │  (KW47A)          │  (NCJ29D6)               │
├──────────────────────┴──────────────────┴───────────────────────────┤
│                       安全抽象层 (Security Abstraction Layer)          │
│          (SE050 SCP03 / ECDSA P-256 / AES-GCM / HMAC-SHA256)        │
├─────────────────────────────────────────────────────────────────────┤
│                        硬件抽象层 (HAL)                                │
│               (SPI / I2C / UART / GPIO / IRQ / DMA)                 │
└─────────────────────────────────────────────────────────────────────┘
```

## 3. 模块划分

```
┌────────────────────────────────────────────────────────────┐
│                    CCC数字钥匙协议栈                          │
├────────────────────────────────────────────────────────────┤
│  模块1: NFC通信模块 (nfc_comm)                              │
│  模块2: BLE通信模块 (ble_comm)                              │
│  模块3: UWB通信模块 (uwb_comm)                              │
│  模块4: 密钥管理模块 (key_management)                        │
│  模块5: 安全认证模块 (security_auth)                         │
│  模块6: 车辆集成模块 (vehicle_integration)                   │
│  模块7: 异常处理模块 (exception_handler)                     │
└────────────────────────────────────────────────────────────┘
```

---

## 3. 模块详细设计

### 3.1 NFC通信模块 (nfc_comm)

#### 3.1.1 模块职责
- 13.56MHz NFC场检测与唤醒
- ISO 14443-A / NFC-F (FeliCa) 协议通信
- NDEF消息解析与生成
- NFC触碰配对 (OOB数据交换)
- 钥匙数据近场传输通道

#### 3.1.2 状态机

```
                   ┌─────────────────────────────────────────────┐
                   │                 IDLE                        │
                   └──────────┬──────────────────────────────────┘
                              │ 场检测到(ST25R501 IRQ)
                              ▼
                   ┌─────────────────────────────────────────────┐
                   │            FIELD_DETECT                      │
                   └──────────┬──────────────────────────────────┘
                              │ ANTICOLLISION Start
                              ▼
                   ┌─────────────────────────────────────────────┐
                   │          ANTI_COLLISION                      │
                   └──────────┬──────────────────────────────────┘
                              │ 冲突解决完成
                              ▼
                   ┌─────────────────────────────────────────────┐
                   │              SELECT                          │
                   └──────────┬──────────────────────────────────┘
                              │ ISO 14443-4 Protocol Activation
                              ▼
                   ┌─────────────────────────────────────────────┐
                   │         DATA_EXCHANGE                        │
                   └──┬──────────┬──────────┬────────────────────┘
                      │          │          │
                      ▼          ▼          ▼
               ┌──────────┐ ┌──────────┐ ┌──────────┐
               │ OOB配对  │ │ 数据收发 │ │ 认证通道 │
               └──┬───────┘ └──┬───────┘ └────┬─────┘
                  │            │              │
                  └────────────┴──────────────┘
                           │ 通信完成 / 场丢失
                           ▼
                   ┌─────────────────────────────────────────────┐
                   │             COMPLETE                         │
                   └─────────────────────────────────────────────┘
```

#### 3.1.3 类图 / 模块结构

```
┌──────────────────────────────────────────────────────┐
│                  NfcManager                           │
├──────────────────────────────────────────────────────┤
│ - driver: St25r501Driver                              │
│ - rfal: RfalStack                                    │
│ - ndefParser: NdefParser                             │
│ - oobData: CccNfcOobData                             │
│ - state: NfcState                                    │
├──────────────────────────────────────────────────────┤
│ + init(): int32_t                                    │
│ + deinit(): int32_t                                  │
│ + startListen(): int32_t                             │
│ + stopListen(): int32_t                              │
│ + fieldDetect(): bool                                │
│ + send(data, len): int32_t                           │
│ + recv(buf, len): int32_t                            │
│ + oobExchange(oob): int32_t                          │
│ + registerCallback(cb): void                         │
└──────────────────────────────────────────────────────┘
```

#### 3.1.4 接口定义

```c
/**
 * @file nfc_comm.h
 * @brief NFC通信模块接口
 */

#ifndef NFC_COMM_H
#define NFC_COMM_H

#include <stdint.h>
#include <stdbool.h>

/* NFC错误码 */
typedef enum {
    NFC_SUCCESS = 0,
    NFC_ERR_NOT_INITIALIZED,
    NFC_ERR_FIELD_NOT_DETECTED,
    NFC_ERR_ANTICOLLISION_FAILED,
    NFC_ERR_SELECT_FAILED,
    NFC_ERR_COMMUNICATION_FAILED,
    NFC_ERR_NDEF_PARSE_FAILED,
    NFC_ERR_TIMEOUT,
    NFC_ERR_HARDWARE_FAULT,
    NFC_ERR_OOB_DATA_INVALID
} nfc_result_t;

/* NFC协议类型 */
typedef enum {
    NFC_PROTOCOL_UNKNOWN = 0,
    NFC_PROTOCOL_ISO14443_A,
    NFC_PROTOCOL_FELICA,
    NFC_PROTOCOL_ISO14443_B
} nfc_protocol_t;

/* NFC帧类型 */
typedef enum {
    NFC_FRAME_ATR_REQ = 0x00,
    NFC_FRAME_ATR_RES = 0x01,
    NFC_FRAME_OOB_DATA = 0x02,
    NFC_FRAME_KEY_DATA = 0x03,
    NFC_FRAME_AUTH_DATA = 0x04,
    NFC_FRAME_STATUS = 0x05
} nfc_frame_type_t;

/* CCC NDEF记录(URI) */
typedef struct {
    uint8_t  tnf;
    uint8_t  type_len;
    uint8_t  payload_len[4];
    uint8_t  type;
    uint8_t  uri_code;
    uint8_t  uri[128];
} ccc_ndef_record_t;

/* CCC OOB配对数据结构 */
typedef struct __attribute__((packed)) {
    uint16_t version;
    uint8_t  ble_mac[6];
    uint8_t  uwb_session_id[8];
    uint8_t  uwb_channel;
    uint8_t  uwb_preamble_code;
    uint8_t  capability[4];
    uint8_t  oob_data[32];
} ccc_nfc_oob_data_t;

/* 事件回调 */
typedef void (*nfc_event_callback_t)(nfc_frame_type_t type, uint8_t *data, uint16_t len);

/* 主要API接口 */

/**
 * @brief 初始化NFC模块(ST25R501)
 */
nfc_result_t nfc_init(void);

/**
 * @brief 反初始化NFC模块
 */
nfc_result_t nfc_deinit(void);

/**
 * @brief 启动NFC场监听
 */
nfc_result_t nfc_start_listen(void);

/**
 * @brief 停止NFC场监听
 */
nfc_result_t nfc_stop_listen(void);

/**
 * @brief 检测NFC场
 */
bool nfc_field_detect(void);

/**
 * @brief 发送NFC数据
 */
nfc_result_t nfc_send(const uint8_t *data, uint16_t len);

/**
 * @brief 接收NFC数据
 */
nfc_result_t nfc_recv(uint8_t *buf, uint16_t *len);

/**
 * @brief 执行OOB配对数据交换
 */
nfc_result_t nfc_oob_exchange(ccc_nfc_oob_data_t *oob);

/**
 * @brief 注册事件回调
 */
nfc_result_t nfc_register_callback(nfc_event_callback_t callback);

#endif /* NFC_COMM_H */
```

#### 3.1.5 数据结构

```c
/* ST25R501硬件接口定义 */
#define ST25R501_SPI_INSTANCE   SPI2
#define ST25R501_SPI_CLK        4000000
#define ST25R501_CS_PORT        GPIOB
#define ST25R501_CS_PIN         GPIO_PIN_12
#define ST25R501_IRQ_PORT       GPIOB
#define ST25R501_IRQ_PIN        GPIO_PIN_1
#define ST25R501_RST_PORT       GPIOB
#define ST25R501_RST_PIN        GPIO_PIN_0

/* NFC-F ATR (Attribution Request/Response) 帧 */
typedef struct __attribute__((packed)) {
    uint8_t  cmd;           /* 00: ATR_REQ, 01: ATR_RES */
    uint8_t  nfcid2[8];    /* NFC Identifier 2 */
    uint8_t  pad[8];        /* Padding */
    uint8_t  mrti_check;    /* Maximum Response Time Check */
    uint8_t  mrti_update;   /* Maximum Response Time Update */
    uint8_t  data[];        /* Optional system data */
} nfc_felica_atr_t;
```

---

### 3.2 BLE通信模块 (ble_comm)

#### 3.2.1 模块职责
- BLE广播 (Digital Key Service) 与扫描
- LE Secure Connections 加密连接管理
- CCC Digital Key GATT服务注册与管理
- 数据帧收发与重组
- NFC辅助OOB配对触发

#### 3.2.2 类图

```
┌──────────────────────────────────────────────────────┐
│                  BleManager                            │
├──────────────────────────────────────────────────────┤
│ - driver: Kw47aDriver                                 │
│ - gatt: CccDkGatt                                    │
│ - connections: BleConnection[MAX_CONN]                │
│ - stateMachine: BleStateMachine                       │
│ - oobData: BleOobData                                 │
├──────────────────────────────────────────────────────┤
│ + init(): int32_t                                    │
│ + startAdv(param): int32_t                           │
│ + stopAdv(): int32_t                                 │
│ + connect(param): int32_t                            │
│ + disconnect(handle): int32_t                        │
│ + send(handle, data, len): int32_t                   │
│ + oobPair(oob): int32_t                              │
│ + registerCallback(cb): void                         │
└──────────────────────────────────────────────────────┘
```

#### 3.2.3 接口定义

```c
/**
 * @file ble_comm.h
 * @brief BLE通信模块接口 (NXP KW47A)
 */

#ifndef BLE_COMM_H
#define BLE_COMM_H

#include <stdint.h>
#include <stdbool.h>

/* BLE错误码 */
typedef enum {
    BLE_SUCCESS = 0,
    BLE_ERR_NOT_INITIALIZED,
    BLE_ERR_ADV_FAILED,
    BLE_ERR_CONNECT_FAILED,
    BLE_ERR_DISCONNECTED,
    BLE_ERR_GATT_ERROR,
    BLE_ERR_DATA_TOO_LARGE,
    BLE_ERR_MTU_EXCEEDED,
    BLE_ERR_ENCRYPTION_FAILED,
    BLE_ERR_PAIRING_FAILED,
    BLE_ERR_TIMEOUT,
    BLE_ERR_HARDWARE_FAULT
} ble_result_t;

/* BLE连接状态 */
typedef enum {
    BLE_STATE_DISCONNECTED,
    BLE_STATE_SCANNING,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_ENCRYPTING,
    BLE_STATE_ENCRYPTED
} ble_state_t;

/* BLE连接参数 */
typedef struct {
    uint16_t conn_interval_min;
    uint16_t conn_interval_max;
    uint16_t slave_latency;
    uint16_t supervision_timeout;
} ble_conn_param_t;

/* BLE广播参数 */
typedef struct {
    uint8_t  adv_type;
    uint16_t interval_min;
    uint16_t interval_max;
    uint8_t  channel_map;
    uint8_t  filter_policy;
} ble_adv_param_t;

/* BLE连接信息 */
typedef struct {
    uint16_t    conn_handle;
    uint8_t     peer_addr[6];
    uint8_t     peer_addr_type;
    ble_state_t state;
    ble_conn_param_t params;
    uint16_t    mtu;
    uint32_t    last_activity;
} ble_connection_t;

/* CCC GATT服务句柄 */
typedef struct {
    uint16_t service_handle;
    struct {
        uint16_t pairing_handle;
        uint16_t key_data_handle;
        uint16_t auth_handle;
        uint16_t state_handle;
        uint16_t uwb_config_handle;
        uint16_t rssi_handle;
    } char_handles;
    struct {
        uint16_t pairing_ccc;
        uint16_t key_data_ccc;
        uint16_t auth_ccc;
        uint16_t state_ccc;
        uint16_t uwb_config_ccc;
    } ccc_handles;
} ccc_dk_gatt_t;

/* BLE数据帧头 */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;
    uint8_t  msg_id;
    uint16_t payload_len;
    uint8_t  reserved;
} ble_frame_header_t;

/* 消息类型 */
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

/* 事件回调 */
typedef void (*ble_event_callback_t)(uint16_t conn_handle, uint8_t event, void *data);

/* 主要API接口 */

/**
 * @brief 初始化BLE模块(KW47A)
 */
ble_result_t ble_init(void);

/**
 * @brief 启动BLE广播
 */
ble_result_t ble_start_adv(const ble_adv_param_t *param);

/**
 * @brief 停止BLE广播
 */
ble_result_t ble_stop_adv(void);

/**
 * @brief 建立BLE连接
 */
ble_result_t ble_connect(const ble_conn_param_t *param);

/**
 * @brief 断开BLE连接
 */
ble_result_t ble_disconnect(uint16_t conn_handle);

/**
 * @brief 发送BLE数据
 */
ble_result_t ble_send(uint16_t conn_handle, const uint8_t *data, uint16_t len);

/**
 * @brief NFC辅助OOB配对
 */
ble_result_t ble_oob_pair(const uint8_t *oob_data, uint16_t len);

/**
 * @brief 获取连接信息
 */
ble_result_t ble_get_connection_info(uint16_t conn_handle, ble_connection_t *info);

/**
 * @brief 注册事件回调
 */
ble_result_t ble_register_callback(ble_event_callback_t callback);

#endif /* BLE_COMM_H */
```

#### 3.2.4 GATT服务定义

```c
/* CCC Digital Key Service UUID */
#define CCC_DK_SERVICE_UUID       0xFFD1

/* Characteristic UUIDs */
#define CCC_DK_CHAR_PAIRING       0xFFD2  /* 配对控制 */
#define CCC_DK_CHAR_KEY_DATA      0xFFD3  /* 密钥数据 */
#define CCC_DK_CHAR_AUTH          0xFFD4  /* 认证数据 */
#define CCC_DK_CHAR_STATE         0xFFD5  /* 钥匙状态 */
#define CCC_DK_CHAR_UWB_CONFIG    0xFFD6  /* UWB配置 */
#define CCC_DK_CHAR_RSSI          0xFFD7  /* RSSI测距 */

/* CCC GATT特性安全属性 */
#define CCC_CHAR_PERM_READ        (1 << 0)
#define CCC_CHAR_PERM_WRITE       (1 << 1)
#define CCC_CHAR_PERM_NOTIFY      (1 << 2)
#define CCC_CHAR_PERM_AUTH_READ   (1 << 3)
#define CCC_CHAR_PERM_AUTH_WRITE  (1 << 4)
#define CCC_CHAR_PERM_ECC         (1 << 5)
```

---

### 3.3 UWB通信模块 (uwb_comm)

#### 3.3.1 模块职责
- IEEE 802.15.4z UWB物理层参数配置
- TWR (Two-Way Ranging) 双向测距
- STS (Scrambled Timestamp Sequence) 安全测距
- 多锚点定位与距离融合
- 距离区域判定与触发

#### 3.3.2 状态机

```
                    ┌──────────┐
                    │  IDLE    │
                    └────┬─────┘
                         │ init
                         ▼
                    ┌──────────┐
                    │   INIT   │
                    └────┬─────┘
                         │ create_session
                         ▼
                    ┌──────────┐
                    │ CONFIG   │
                    └────┬─────┘
                         │ start_ranging
                         ▼
              ┌──────────────────────┐
              │                      │
     ┌────────▼────────┐    ┌───────▼────────┐
     │ RANGING_ACTIVE  │    │ STS_SECURE_RNG │
     │ (非安全测距)     │    │ (安全加密测距)   │
     └────────┬────────┘    └───────┬────────┘
              │                     │
              ▼                     ▼
     ┌──────────────────────────────────────┐
     │         DISTANCE_CALCULATED           │
     │  T_round1 = T4 - T1                  │
     │  T_round2 = T3 - T2                  │
     │  T_prop = (T_round1 - T_round2) / 2  │
     │  Distance = T_prop × c               │
     └────────────────┬─────────────────────┘
                      │ stop_ranging / session_close
                      ▼
                    ┌──────────┐
                    │  IDLE    │
                    └──────────┘
```

#### 3.3.3 类图

```
┌──────────────────────────────────────────────────────┐
│                  UwbManager                           │
├──────────────────────────────────────────────────────┤
│ - driver: Ncj29d6Driver                              │
│ - sessions: UwbSession[MAX_SESSIONS]                 │
│ - distanceEngine: DistanceEngine                      │
│ - zoneManager: ZoneManager                            │
│ - stsEngine: StsCryptoEngine                          │
├──────────────────────────────────────────────────────┤
│ + init(): int32_t                                    │
│ + createSession(cfg): int32_t                        │
│ + startRanging(session_id): int32_t                  │
│ + stopRanging(session_id): int32_t                   │
│ + getDistance(session_id, dist): int32_t             │
│ + getZone(session_id): distance_zone_e               │
│ + destroySession(session_id): int32_t                │
│ + registerZoneCallback(cb): void                     │
└──────────────────────────────────────────────────────┘
```

#### 3.3.4 接口定义

```c
/**
 * @file uwb_comm.h
 * @brief UWB通信模块接口 (NXP NCJ29D6)
 */

#ifndef UWB_COMM_H
#define UWB_COMM_H

#include <stdint.h>
#include <stdbool.h>

/* UWB错误码 */
typedef enum {
    UWB_SUCCESS = 0,
    UWB_ERR_NOT_INITIALIZED,
    UWB_ERR_SESSION_EXISTS,
    UWB_ERR_SESSION_NOT_FOUND,
    UWB_ERR_RANGING_FAILED,
    UWB_ERR_STS_MISMATCH,
    UWB_ERR_CONFIG_INVALID,
    UWB_ERR_TIMEOUT,
    UWB_ERR_HARDWARE_FAULT
} uwb_result_t;

/* 距离区域 */
typedef enum {
    ZONE_LOCKED      = 0,   /* >10m */
    ZONE_APPROACH    = 1,   /* 5-10m */
    ZONE_UNLOCK      = 2,   /* 2-5m */
    ZONE_ENTRY       = 3,   /* 0-2m */
    ZONE_INSIDE      = 4    /* <0.5m */
} distance_zone_e;

/* 距离阈值 */
typedef struct {
    uint16_t approach_cm;
    uint16_t unlock_cm;
    uint16_t entry_cm;
    uint16_t inside_cm;
    uint16_t hysteresis_cm;
} distance_threshold_t;

/* UWB会话配置 */
typedef struct {
    uint8_t  session_id[8];
    uint8_t  sts_key[16];
    uint8_t  sts_iv[16];
    uint8_t  channel;
    uint8_t  preamble_code;
    uint8_t  prf_len;
    uint8_t  sfd_id;
    uint8_t  phr_rate;
    uint8_t  data_rate;
    uint8_t  tx_power;
    uint8_t  rframe_config;
    uint8_t  sts_config;
} uwb_session_config_t;

/* STS安全测距帧 */
typedef struct __attribute__((packed)) {
    uint8_t  phr;
    uint8_t  frame_ctrl[2];
    uint8_t  seq_num;
    uint8_t  pan_id[2];
    uint8_t  src_addr[8];
    uint8_t  dst_addr[8];
    uint8_t  sts_phy_hdr;
    uint8_t  sts_data[128];
    uint8_t  payload[];
    uint8_t  mic[4];
} uwb_sts_frame_t;

/* 测距结果 */
typedef struct {
    uint32_t session_id;
    uint16_t distance_cm;
    uint8_t  accuracy;
    int8_t   rssi;
    uint8_t  nlos;              /* 非视距标志 */
    uint32_t timestamp;
    uint8_t  confidence;
} uwb_ranging_result_t;

/* 区域变更回调 */
typedef void (*uwb_zone_callback_t)(distance_zone_e new_zone, const uwb_ranging_result_t *result);

/* 主要API接口 */

/**
 * @brief 初始化UWB模块(NCJ29D6)
 */
uwb_result_t uwb_init(void);

/**
 * @brief 创建UWB测距会话
 */
uwb_result_t uwb_create_session(const uwb_session_config_t *cfg);

/**
 * @brief 开始测距
 */
uwb_result_t uwb_start_ranging(uint32_t session_id);

/**
 * @brief 停止测距
 */
uwb_result_t uwb_stop_ranging(uint32_t session_id);

/**
 * @brief 获取当前距离
 */
uwb_result_t uwb_get_distance(uint32_t session_id, uint16_t *dist_cm);

/**
 * @brief 获取当前距离区域
 */
distance_zone_e uwb_get_zone(uint32_t session_id);

/**
 * @brief 销毁UWB会话
 */
uwb_result_t uwb_destroy_session(uint32_t session_id);

/**
 * @brief 注册区域变更回调
 */
uwb_result_t uwb_register_zone_callback(uwb_zone_callback_t callback);

#endif /* UWB_COMM_H */
```

---

### 3.4 密钥管理模块 (key_management)

#### 3.4.1 模块职责
- 数字钥匙的创建、删除、分享、挂起、恢复、撤销
- 钥匙数据模型维护与版本管理
- 钥匙生命周期管理 (Created → Active → Suspended/Expired/Revoked)
- 访问权限验证
- 证书链管理 (CA证书/设备证书/Attestation)

#### 3.4.2 钥匙生命周期

```
                    ┌─────────┐
                    │ CREATED │  ← 配钥/绑定
                    └────┬────┘
                         │ 激活
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

#### 3.4.3 接口定义

```c
/**
 * @file key_management.h
 * @brief CCC数字钥匙管理模块接口
 */

#ifndef KEY_MANAGEMENT_H
#define KEY_MANAGEMENT_H

#include <stdint.h>
#include <stdbool.h>

/* 密钥管理错误码 */
typedef enum {
    KEY_SUCCESS = 0,
    KEY_ERR_NOT_FOUND,
    KEY_ERR_EXISTS,
    KEY_ERR_INVALID_PARAM,
    KEY_ERR_STORAGE_FULL,
    KEY_ERR_EXPIRED,
    KEY_ERR_REVOKED,
    KEY_ERR_PERMISSION_DENIED,
    KEY_ERR_CERT_CHAIN_INVALID,
    KEY_ERR_ATTESTATION_FAILED,
    KEY_ERR_HARDWARE_FAULT
} key_result_t;

/* 密钥类型 */
typedef enum {
    KEY_TYPE_OWNER       = 0x01,
    KEY_TYPE_FRIEND      = 0x02,
    KEY_TYPE_SERVICE     = 0x03,
    KEY_TYPE_TEMPORARY   = 0x04
} key_type_e;

/* 密钥状态 */
typedef enum {
    KEY_STATE_INACTIVE    = 0x00,
    KEY_STATE_ACTIVE      = 0x01,
    KEY_STATE_SUSPENDED   = 0x02,
    KEY_STATE_EXPIRED     = 0x03,
    KEY_STATE_REVOKED     = 0x04
} key_state_e;

/* 访问权限位图 */
#define ACCESS_LOCK_UNLOCK    (1 << 0)
#define ACCESS_ENGINE_START   (1 << 1)
#define ACCESS_TRUNK         (1 << 2)
#define ACCESS_WINDOWS       (1 << 3)
#define ACCESS_SUNROOF       (1 << 4)
#define ACCESS_CLIMATE       (1 << 5)
#define ACCESS_SEAT          (1 << 6)
#define ACCESS_FUEL_DOOR     (1 << 7)

/* CCC数字钥匙数据模型 */
typedef struct __attribute__((packed)) {
    uint8_t  key_id[16];
    uint8_t  vehicle_id[16];
    uint8_t  owner_id[16];
    uint8_t  key_type;
    uint8_t  access_rights[4];
    uint8_t  restrictions[4];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t  state;
    uint8_t  version;
    uint8_t  ca_cert[256];
    uint8_t  device_cert[256];
    uint8_t  attestation[128];
    uint8_t  se_key_id;
    uint8_t  reserved[3];
} ccc_digital_key_t;

/* 主要API接口 */

/**
 * @brief 初始化密钥管理模块
 */
key_result_t key_init(void);

/**
 * @brief 创建数字钥匙
 */
key_result_t key_create(const ccc_digital_key_t *key);

/**
 * @brief 删除数字钥匙
 */
key_result_t key_delete(const uint8_t *key_id);

/**
 * @brief 获取钥匙信息
 */
key_result_t key_get(const uint8_t *key_id, ccc_digital_key_t *key);

/**
 * @brief 列出所有钥匙
 */
key_result_t key_list(ccc_digital_key_t *keys, uint8_t *count);

/**
 * @brief 分享钥匙
 */
key_result_t key_share(const uint8_t *key_id, key_type_e type, uint32_t duration);

/**
 * @brief 撤销钥匙
 */
key_result_t key_revoke(const uint8_t *key_id);

/**
 * @brief 挂起钥匙
 */
key_result_t key_suspend(const uint8_t *key_id);

/**
 * @brief 恢复钥匙
 */
key_result_t key_resume(const uint8_t *key_id);

/**
 * @brief 验证钥匙权限
 */
bool key_check_permission(const uint8_t *key_id, uint8_t access_bit);

/**
 * @brief 更新钥匙状态
 */
key_result_t key_update_state(const uint8_t *key_id, key_state_e new_state);

#endif /* KEY_MANAGEMENT_H */
```

---

### 3.5 安全认证模块 (security_auth)

#### 3.5.1 模块职责
- SCP03安全通道建立与管理 (与SE050通信)
- ECDSA P-256签名生成与验证
- AES-256-GCM加密/解密
- Attestation证明包生成与验证
- 证书链验证 (Root CA → Intermediate CA → Device Cert)
- 会话密钥协商

#### 3.5.2 类图

```
┌──────────────────────────────────────────────────────┐
│                 SecurityManager                        │
├──────────────────────────────────────────────────────┤
│ - se050: Se050Driver                                 │
│ - scp03: Scp03Channel                                 │
│ - keyStore: SecureKeyStore                             │
│ - cryptoEngine: CryptoEngine                           │
│ - sessionManager: SessionManager                       │
├──────────────────────────────────────────────────────┤
│ + scp03Open(ch): int32_t                             │
│ + scp03Close(ch): int32_t                            │
│ + encrypt(data, len, out): int32_t                   │
│ + decrypt(data, len, out): int32_t                   │
│ + sign(data, len, sig): int32_t                      │
│ + verify(data, len, sig): verify_result_e            │
│ + attestation(att): int32_t                          │
│ + verifyAttestation(att): verify_result_e            │
│ + storeKey(key_id, key): int32_t                     │
│ + loadKey(key_id, key): int32_t                      │
└──────────────────────────────────────────────────────┘
```

#### 3.5.3 接口定义

```c
/**
 * @file security_auth.h
 * @brief 安全认证模块接口 (SE050 + ECDSA + SCP03)
 */

#ifndef SECURITY_AUTH_H
#define SECURITY_AUTH_H

#include <stdint.h>
#include <stdbool.h>

/* 验签结果 */
typedef enum {
    VERIFY_OK                = 0x00,
    VERIFY_CERT_INVALID      = 0x01,
    VERIFY_SIGN_INVALID      = 0x02,
    VERIFY_KEY_EXPIRED       = 0x03,
    VERIFY_KEY_REVOKED       = 0x04,
    VERIFY_PERMISSION_DENIED = 0x05,
    VERIFY_FW_TAMPERED       = 0x06
} verify_result_e;

/* SCP03安全通道 */
typedef struct {
    uint8_t  enc_key[16];
    uint8_t  mac_key[16];
    uint8_t  dek_key[16];
    uint8_t  host_challenge[8];
    uint8_t  card_challenge[8];
    uint8_t  seq_counter[2];
    uint8_t  chain_mode;
} scp03_channel_t;

/* Attestation证明包 */
typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  nonce[16];
    uint8_t  device_id[16];
    uint8_t  key_id[16];
    uint8_t  key_type;
    uint8_t  access_rights[4];
    uint8_t  firmware_hash[32];
    uint8_t  security_state;
    uint8_t  attestation_cert[256];
    uint8_t  signature[64];
} ccc_attestation_t;

/* 会话信息 */
typedef struct {
    uint32_t session_id;
    uint16_t conn_handle;
    uint8_t  session_key[32];
    uint32_t creation_time;
    uint32_t expiry_time;
    bool     is_encrypted;
} session_info_t;

/* 主要API接口 */

/**
 * @brief 初始化安全模块
 */
int32_t security_init(void);

/**
 * @brief 建立SCP03安全通道
 */
int32_t security_scp03_open(scp03_channel_t *ch);

/**
 * @brief 关闭SCP03安全通道
 */
int32_t security_scp03_close(scp03_channel_t *ch);

/**
 * @brief AES-256-GCM加密
 */
int32_t security_encrypt(const uint8_t *data, uint32_t len, uint8_t *out);

/**
 * @brief AES-256-GCM解密
 */
int32_t security_decrypt(const uint8_t *data, uint32_t len, uint8_t *out);

/**
 * @brief ECDSA P-256签名
 */
int32_t security_sign(const uint8_t *data, uint32_t len, uint8_t *sig);

/**
 * @brief ECDSA P-256验签
 */
verify_result_e security_verify(const uint8_t *data, uint32_t len, const uint8_t *sig);

/**
 * @brief 生成Attestation证明包
 */
int32_t security_attestation(ccc_attestation_t *att);

/**
 * @brief 验证Attestation证明包
 */
verify_result_e security_verify_attestation(const ccc_attestation_t *att);

/**
 * @brief 存储密钥到SE050
 */
int32_t security_store_key(uint32_t key_id, const uint8_t *key_data, uint16_t len);

/**
 * @brief 从SE050加载密钥
 */
int32_t security_load_key(uint32_t key_id, uint8_t *key_data, uint16_t *len);

/**
 * @brief 派生会话密钥
 */
int32_t security_derive_session_key(const uint8_t *shared_secret, session_info_t *session);

#endif /* SECURITY_AUTH_H */
```

#### 3.5.4 认证流程

```
┌─────────┐                                    ┌─────────┐
│ 手机端  │                                    │  车端   │
└────┬────┘                                    └────┬────┘
     │                                              │
     │  1. 请求认证(打开BLE CCC Auth Char)          │
     │─────────────────────────────────────────────►│
     │                                              │
     │  2. 生成挑战(Nonce + Timestamp)               │
     │◄─────────────────────────────────────────────│
     │                                              │
     │  3. 手机端:                                   │
     │   - 生成Attestation证明包                     │
     │   - ECDSA签名挑战                             │
     │   - 发送签名结果+证书链                       │
     │─────────────────────────────────────────────►│
     │                                              │
     │                         4. 车端验证:          │
     │                          - 验证证书链         │
     │                          - 验证ECDSA签名      │
     │                          - 检查Nonce重放      │
     │                          - 检查时间戳有效期    │
     │                          - 检查钥匙权限       │
     │                          - 检查固件哈希       │
     │                                              │
     │  5. 认证结果                                  │
     │◄─────────────────────────────────────────────│
     │                                              │
     │  6. 会话密钥协商(ECDH P-256)                  │
     │◄────────────────────────────────────────────►│
     │                                              │
     │  7. 加密通信开始(AES-256-GCM)                 │
     │◄────────────────────────────────────────────►│
     │                                              │
```

---

### 3.6 车辆集成模块 (vehicle_integration)

#### 3.6.1 模块职责
- CAN总线通信 (车门/引擎/车窗等控制)
- 车辆状态监听与上报
- 命令执行与反馈
- 车辆状态同步 (通过BLE Notify)

#### 3.6.2 接口定义

```c
/**
 * @file vehicle_integration.h
 * @brief 车辆集成模块接口
 */

#ifndef VEHICLE_INTEGRATION_H
#define VEHICLE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

/* 车辆控制错误码 */
typedef enum {
    VEHICLE_SUCCESS = 0,
    VEHICLE_ERR_CAN_INIT_FAILED,
    VEHICLE_ERR_CAN_SEND_FAILED,
    VEHICLE_ERR_CAN_RECEIVE_FAILED,
    VEHICLE_ERR_INVALID_COMMAND,
    VEHICLE_ERR_EXECUTION_FAILED,
    VEHICLE_ERR_TIMEOUT,
    VEHICLE_ERR_VEHICLE_NOT_READY
} vehicle_result_t;

/* 车辆状态 */
typedef struct {
    uint8_t  door_status;
    uint8_t  lock_status;
    uint8_t  engine_status;
    uint8_t  gear_position;
    uint16_t battery_voltage;
    int16_t  temperature;
    uint32_t odometer;
    uint8_t  alarm_status;
    uint8_t  window_status[4];
    uint8_t  trunk_status;
    uint8_t  sunroof_status;
    uint8_t  climate_status;
    int8_t   battery_level;
} vehicle_state_t;

/* 车辆命令 */
typedef struct {
    uint8_t  command_type;
    uint8_t  target;
    uint8_t  parameters[8];
    uint32_t user_id;
    uint32_t timestamp;
    uint8_t  hmac[32];
} vehicle_command_t;

/* 命令执行结果 */
typedef struct {
    uint8_t  command_type;
    uint8_t  result;
    uint8_t  error_code;
    uint32_t execution_time_ms;
    uint8_t  response_data[16];
} command_result_t;

/* CAN消息 */
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint32_t timestamp;
} can_message_t;

/* 主要API接口 */

vehicle_result_t vehicle_init(void);
vehicle_result_t vehicle_execute_command(const vehicle_command_t *cmd, command_result_t *result);
vehicle_result_t vehicle_get_state(vehicle_state_t *state);
vehicle_result_t vehicle_register_state_callback(void (*callback)(const vehicle_state_t *state));
vehicle_result_t vehicle_send_can(const can_message_t *msg);
vehicle_result_t vehicle_receive_can(can_message_t *msg, uint32_t timeout_ms);
vehicle_result_t vehicle_start_monitoring(void);
vehicle_result_t vehicle_stop_monitoring(void);

#endif /* VEHICLE_INTEGRATION_H */
```

---

### 3.7 异常处理模块 (exception_handler)

#### 3.7.1 模块职责
- 异常检测与分级 (INFO/WARNING/ERROR/CRITICAL/FATAL)
- 异常日志记录与上报
- 故障恢复策略 (Retry/Restart/Fallback/SafeMode/Shutdown)
- 安全告警 (防篡改检测)

#### 3.7.2 接口定义

```c
/**
 * @file exception_handler.h
 * @brief 异常处理模块接口
 */

#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/* 异常级别 */
typedef enum {
    EXCEPTION_LEVEL_INFO = 0,
    EXCEPTION_LEVEL_WARNING,
    EXCEPTION_LEVEL_ERROR,
    EXCEPTION_LEVEL_CRITICAL,
    EXCEPTION_LEVEL_FATAL
} exception_level_t;

/* 异常类型 */
typedef enum {
    EXCEPTION_AUTH_FAILED        = 0x01,
    EXCEPTION_SIGNATURE_INVALID  = 0x02,
    EXCEPTION_NONCE_REUSE        = 0x03,
    EXCEPTION_KEY_EXPIRED        = 0x04,
    EXCEPTION_PERMISSION_DENIED  = 0x05,
    EXCEPTION_COMM_TIMEOUT       = 0x10,
    EXCEPTION_CONNECTION_LOST    = 0x11,
    EXCEPTION_HARDWARE_FAULT     = 0x12,
    EXCEPTION_STORAGE_ERROR      = 0x20,
    EXCEPTION_TAMPER_DETECTED    = 0xF0,
    EXCEPTION_SECURITY_BREACH    = 0xF1
} exception_type_t;

/* 恢复动作 */
typedef enum {
    RECOVERY_NONE = 0,
    RECOVERY_RETRY,
    RECOVERY_RESTART,
    RECOVERY_FALLBACK,
    RECOVERY_SAFE_MODE,
    RECOVERY_SHUTDOWN
} recovery_action_t;

/* 异常上下文 */
typedef struct {
    exception_type_t    type;
    exception_level_t   level;
    uint32_t            timestamp;
    uint16_t            conn_handle;
    uint32_t            user_id;
    uint8_t             context[64];
    uint8_t             stack_trace[128];
} exception_context_t;

/* 异常处理结果 */
typedef struct {
    uint32_t         exception_id;
    recovery_action_t action;
    uint8_t          action_params[16];
    uint32_t         next_retry_time;
    bool             notify_cloud;
} exception_result_t;

/* 主要API接口 */

int32_t exception_init(void);
int32_t exception_report(const exception_context_t *context, exception_result_t *result);
int32_t exception_handle(uint32_t exception_id);
int32_t exception_execute_recovery(recovery_action_t action, const uint8_t *params);
int32_t exception_register_callback(void (*callback)(const exception_context_t *context));
int32_t exception_get_health_status(uint8_t *status);

#endif /* EXCEPTION_HANDLER_H */
```

---

## 4. 状态机设计

### 4.1 系统主状态机

```
                    ┌──────────┐
                    │  SLEEP   │ ← 低功耗模式
                    └────┬─────┘
                         │ NFC场检测 / BLE广播触发
                         ▼
                    ┌──────────┐
              ┌────│  WAKEUP  │────┐
              │    └────┬─────┘    │
              │         │          │
         NFC触发       BLE连接    UWB测距
              │         │          │
              ▼         ▼          ▼
        ┌─────────┐ ┌─────────┐ ┌─────────┐
        │NFC_AUTH │ │BLE_AUTH │ │UWB_AUTH │
        └────┬────┘ └────┬────┘ └────┬────┘
             │           │           │
             └───────────┼───────────┘
                         │ 认证成功
                         ▼
                    ┌──────────┐
                    │  SESSION │ ← 会话已建立
                    ├──────────┤
                    │  COMMAND │ ← 执行车辆命令
                    ├──────────┤
                    │  MONITOR │ ← 车辆状态监听
                    └────┬─────┘
                         │ 超时/断开/休眠
                         ▼
                    ┌──────────┐
                    │  SLEEP   │
                    └──────────┘
```

### 4.2 BLE连接状态机

```
    DISCONNECTED ───► SCANNING ───► CONNECTING ───► CONNECTED
         ▲                                                │
         │                                                │
         │                ENCRYPTED ◄── ENCRYPTING ◄──────┘
         │                    │
         │                    ▼
         │           SERVICE_DISCOVERED
         │                    │
         └────────────────────┘
           断开/超时回到DISCONNECTED
```

### 4.3 UWB测距状态机

```
    IDLE ──► INIT ──► CONFIG ──► RANGING ──► STOP ──► IDLE
                               │     │
                               │     ▼
                               │  ZONE_CHANGED ──► TRIGGER_ACTION
                               │
                               └──► STS_RANGING ──► VERIFIED
```

---

## 5. 安全设计

### 5.1 安全架构

```
┌─────────────────────────────────────────────────────────────┐
│                      CCC安全体系架构                           │
├─────────────────────────────────────────────────────────────┤
│  1. SCP03安全通道 (SE050 ↔ Host MCU)                        │
│     - 双向认证 (Mutual Authentication)                       │
│     - 会话密钥派生 (ENC/MAC/DEK)                             │
│     - 加密通信 (AES-128-CBC)                                 │
│                                                              │
│  2. ECDSA P-256签名体系                                      │
│     - 车端签名: SE050内部签名                                │
│     - 手机端签名验证: 车端验签                               │
│     - 证书链: Root CA → OEM CA → Device Cert                │
│                                                              │
│  3. Attestation (密钥证明)                                    │
│     - 包含: Nonce + DeviceID + KeyID + 权限 + FW Hash       │
│     - 签名: SE050 ECDSA私钥                                  │
│     - 防篡改: FW Hash校验                                    │
│                                                              │
│  4. UWB安全测距 (STS)                                        │
│     - Scrambled Timestamp Sequence                           │
│     - 防中继攻击 (Relay Attack)                               │
│     - 加密测距帧 (STS Key派生)                                │
│                                                              │
│  5. 安全存储                                                 │
│     - 私钥存储在SE050内部(Tamper Resistant)                  │
│     - 钥匙数据AES加密存储                                    │
│     - 防物理攻击 (Active Shield)                              │
│                                                              │
│  6. 防重放攻击                                               │
│     - Nonce机制 (每次认证递增)                                │
│     - 时间戳校验 (5s窗口)                                    │
│     - Sequence Counter (SCP03)                               │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 SCP03初始化流程

```
1. 主机发送 INITIALIZE UPDATE 命令到SE050
2. SE050返回 卡片挑战(Card Challenge) + 序列计数器
3. 主机派生会话密钥 (ENC Key / MAC Key / DEK Key)
4. 主机发送 EXTERNAL AUTHENTICATE 命令 (带MAC)
5. SE050验证MAC，返回认证成功
6. 安全通道建立完成，后续通信加密
```

---

## 6. 关键流程

### 6.1 配钥流程 (Key Provisioning)

```
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│ 手机端  │     │  车端   │     │ SE050  │     │  云端   │
└────┬────┘     └────┬────┘     └────┬────┘     └────┬────┘
     │               │               │               │
     │  1. NFC触碰   │               │               │
     │◄─────────────►│               │               │
     │  OOB数据交换   │               │               │
     │               │               │               │
     │  2. BLE连接   │               │               │
     │◄─────────────►│               │               │
     │               │               │               │
     │  3. 配钥请求  │               │               │
     │──────────────►│               │               │
     │               │               │               │
     │  4. 挑战-响应  │               │               │
     │◄─────────────►│  5. SCP03     │               │
     │               │──────────────►│               │
     │               │  6. 生成密钥对│               │
     │               │◄──────────────│               │
     │               │               │               │
     │  7. 证书链    │  8. Attestation              │
     │◄──────────────│──────────────►│──────────────►│
     │               │               │               │
     │  9. 钥匙激活  │               │               │
     │──────────────►│──────────────►│               │
```

### 6.2 解锁流程 (Passive Entry)

```
┌─────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ 手机端  │    │ BLE模块  │    │ UWB模块  │    │ 认证模块 │    │ 车辆模块 │
└────┬────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘
     │              │               │               │               │
     │  1. 手机靠近 (5-10m)         │               │               │
     │              │               │               │               │
     │  2. BLE广播扫描              │               │               │
     │◄────────────►│               │               │               │
     │              │               │               │               │
     │  3. UWB测距会话              │               │               │
     │◄────────────►│◄─────────────►│               │               │
     │              │               │               │               │
     │  4. 距离2-5m(解锁区)         │               │               │
     │              │               │──────────────►│               │
     │              │               │               │               │
     │  5. 快速认证(BLE Auth Char)  │               │               │
     │◄────────────►│──────────────►│               │               │
     │              │               │  6. 验证权限  │               │
     │              │               │──────────────►│               │
     │              │               │               │               │
     │              │               │  7. 执行解锁  │               │
     │              │               │──────────────►│──────────────►│
     │              │               │               │               │
     │  8. 解锁结果                 │               │               │
     │◄────────────►│◄─────────────►│◄──────────────│◄──────────────│
```

### 6.3 启动流程 (Passive Start)

```
1. 用户进入车内 (Zone INSIDE < 0.5m)
2. UWB确认手机在车内 (车内定位)
3. 按启动按钮
4. 车端发起快速认证 (BLE或NFC)
5. 验证钥匙发动机启动权限
6. SE050签名验证
7. 通过CAN发送引擎启动指令
8. 引擎启动成功 → BLE Notify通知手机
```

---

## 7. 测试策略

### 7.1 单元测试

| 模块 | 测试项 | 覆盖率目标 |
|------|--------|-----------|
| NFC通信 | 场检测、NDEF解析、OOB交换、异常场处理 | ≥90% |
| BLE通信 | GATT服务、广播/扫描、连接管理、数据收发、断连重连 | ≥90% |
| UWB通信 | 会话创建/销毁、TWR测距精度、STS安全测距、区域判定 | ≥85% |
| 密钥管理 | CRUD操作、生命周期状态转换、权限验证、证书链校验 | ≥95% |
| 安全认证 | SCP03通道、ECDSA签名/验签、AES加解密、Attestation | ≥95% |

### 7.2 集成测试

| 测试场景 | 描述 |
|---------|------|
| NFC→BLE配对 | NFC触碰触发BLE连接建立 |
| BLE→UWB联动 | BLE连接后自动启动UWB测距会话 |
| 三模切换 | NFC/BLE/UWB三种通信方式无缝切换 |
| 多钥匙并发 | 多把钥匙同时连接管理 |
| 断连重连 | BLE断开后自动重连 |

### 7.3 安全测试

| 测试项 | 描述 |
|--------|------|
| 重放攻击 | 截获认证请求重放验证 |
| 中继攻击 | UWB STS防中继有效性验证 |
| 证书篡改 | 伪造证书链验签阻断 |
| 密钥提取 | SE050防物理攻击验证 |
| 固件篡改 | Attestation FW Hash校验 |

### 7.4 性能测试

| 测试项 | 目标值 |
|--------|--------|
| BLE连接建立时间 | ≤ 300ms |
| UWB首次测距时间 | ≤ 50ms |
| 认证响应时间 | ≤ 200ms |
| 从解锁区到解锁动作 | ≤ 500ms |
| 引擎启动响应时间 | ≤ 1000ms |
| NFC检测到数据交换 | ≤ 100ms |

---

## 8. 配置参数

| 参数 | 默认值 | 范围 | 描述 |
|------|--------|------|------|
| MAX_CONNECTIONS | 4 | 1-8 | 最大并发BLE连接数 |
| MAX_UWB_SESSIONS | 4 | 1-8 | 最大UWB会话数 |
| MAX_KEYS | 16 | 1-32 | 最大存储钥匙数 |
| AUTH_TIMEOUT_MS | 5000 | 1000-30000 | 认证超时时间 |
| UWB_RANGING_INTERVAL_MS | 100 | 50-1000 | UWB测距间隔 |
| ZONE_APPROACH_CM | 1000 | - | 接近距离阈值 |
| ZONE_UNLOCK_CM | 500 | - | 解锁距离阈值 |
| ZONE_ENTRY_CM | 200 | - | 进入距离阈值 |
| ZONE_INSIDE_CM | 50 | - | 车内距离阈值 |
| NONCE_WINDOW_MS | 5000 | - | Nonce有效时间窗口 |
| SESSION_TIMEOUT_S | 300 | - | 会话超时时间 |

## 9. 附录

### 9.1 错误码汇总表

| 模块 | 错误码范围 | 描述 |
|------|-----------|------|
| NFC | 0x00-0x0F | NFC通信错误 |
| BLE | 0x10-0x1F | BLE通信错误 |
| UWB | 0x20-0x2F | UWB测距错误 |
| Key Management | 0x30-0x3F | 密钥管理错误 |
| Security | 0x40-0x4F | 安全认证错误 |
| Vehicle | 0x50-0x5F | 车辆集成错误 |
| Exception | 0x60-0x6F | 异常处理错误 |
| System | 0xF0-0xFF | 系统级错误 |

### 9.2 硬件依赖

| 组件 | 接口 | 驱动 | 依赖版本 |
|------|------|------|---------|
| STM32L5 | - | STM32 HAL | 1.27+ |
| ST25R501 | SPI | RFAL | 2.4+ |
| KW47A | SPI+UART | KW47A SDK | 2.x |
| NCJ29D6 | SPI | NCJ29D6 SDK | 1.x |
| SE050 | I2C | Plug & Trust Middleware | 4.x |

---

**文档结束**
