# ICCOA数字钥匙协议栈 - 模块设计文档

## 1. 模块概述

本文档详细描述ICCOA (智慧车联开放联盟) Digital Key 3.0 和 DK 4.0 协议栈的车端各模块设计，包括模块架构、接口定义、数据结构和关键算法。协议栈基于NXP KW47A (BLE) 通信硬件平台，支持UWB扩展（DK 4.0），结合SE050安全芯片实现数字钥匙的完整功能。

| 项目 | 内容 |
|------|------|
| 协议标准 | ICCOA DK 3.0 / DK 4.0 |
| 通信方式 | BLE 5.0 LE (DK 3.0) / BLE + UWB (DK 4.0) |
| 硬件平台 | NXP KW47A + NCJ29D6 (DK 4.0) + SE050 |
| 安全体系 | ECDH P-256 + ECDSA + AES-GCM + HMAC-SHA256 |
| 发起方 | 小米、OPPO、vivo联合国内车企 |

### 1.1 ICCOA vs CCC 对比

| 特性 | ICCA | CCC |
|------|------|-----|
| 发起方 | 国内手机厂商+车企 | 国际车企+手机厂商 |
| 通信方式 | BLE为主 (UWB可选) | NFC+BLE+UWB三模 |
| 密钥体系 | 简化密钥体系 | Applet+证书链 |
| 手机生态 | 小米/OPPO/vivo | Apple/Google |
| NFC依赖 | 可选 | 必须 |
| 远程分享 | 云端推送 | 端到端分享 |

---

## 2. 协议栈层级架构

```
┌───────────────────────────────────────────────────────────────┐
│                     ICCOA数字钥匙应用层                          │
│  (钥匙管理 / 车辆控制 / 状态同步 / 远程分享 / 数字身份认证)      │
├───────────────────────────────────────────────────────────────┤
│                   ICCOA协议管理层                                │
│   DK 3.0: 绑定/认证/控制/分享          DK 4.0: 会话管理/UWB    │
│   帧封装/序列号管理/校验和             HMAC验证/令牌管理        │
├───────────────────────┬───────────────────────────────────────┤
│  BLE通信模块 (KW47A)  │  UWB通信模块 (NCJ29D6) — DK 4.0 only  │
│  广播/扫描/GATT服务   │  测距/STS安全/区域判定                  │
├───────────────────────┴───────────────────────────────────────┤
│                     安全抽象层                                  │
│     (SE050 ECDH P-256 / ECDSA / AES-GCM / HMAC-SHA256)       │
├───────────────────────────────────────────────────────────────┤
│                      硬件抽象层 (HAL)                           │
│                (SPI / I2C / UART / GPIO / IRQ)                │
└───────────────────────────────────────────────────────────────┘
```

## 3. 模块划分

```
┌──────────────────────────────────────────────────────────────┐
│                   ICCOA数字钥匙协议栈                           │
├──────────────────────────────────────────────────────────────┤
│  模块1: BLE通信模块 (ble_comm)                                │
│  模块2: UWB通信模块 (uwb_comm) — DK 4.0                      │
│  模块3: 安全认证模块 (security_auth)                           │
│  模块4: 密钥管理模块 (key_management)                          │
│  模块5: 离线决策模块 (offline_decision)                        │
│  模块6: 车辆集成模块 (vehicle_integration)                     │
│  模块7: 异常处理模块 (exception_handler)                       │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. 模块详细设计

### 3.1 BLE通信模块 (ble_comm)

#### 3.1.1 模块职责
- BLE广播 (ICCOA Digital Key Service UUID) 与扫描
- 连接建立与管理 (支持多连接, DK 4.0)
- ICCOA GATT服务注册 (控制/数据/通知/认证通道)
- ICCOA协议帧封装/解析 (DK 3.0 / DK 4.0双协议)
- LE Secure Connections 加密连接

#### 3.1.2 类图

```
┌──────────────────────────────────────────────────────┐
│                  BleManager                            │
├──────────────────────────────────────────────────────┤
│ - driver: Kw47aDriver                                 │
│ - gatt: IcocaDkGatt                                  │
│ - connections: BleConnection[MAX_CONNECTIONS]         │
│ - protocolVersion: uint8_t (3 or 4)                   │
│ - frameEncoder: IcocaFrameEncoder                    │
├──────────────────────────────────────────────────────┤
│ + init(version): int32_t                             │
│ + startAdv(param): int32_t                           │
│ + stopAdv(): int32_t                                 │
│ + connect(param): int32_t                            │
│ + disconnect(handle): int32_t                        │
│ + send(handle, data, len): int32_t                   │
│ + recvCallback(handle, data, len): void              │
│ + registerCallback(cb): void                         │
└──────────────────────────────────────────────────────┘
```

#### 3.1.3 接口定义

```c
/**
 * @file ble_comm.h
 * @brief ICCOA BLE通信模块接口 (NXP KW47A)
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
    BLE_ERR_PROTOCOL_UNSUPPORTED,
    BLE_ERR_TIMEOUT,
    BLE_ERR_HARDWARE_FAULT
} ble_result_t;

/* BLE状态 */
typedef enum {
    BLE_STATE_DISCONNECTED,
    BLE_STATE_ADVERTISING,
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_ENCRYPTED
} ble_state_t;

/* BLE连接信息 */
typedef struct {
    uint16_t    conn_handle;
    uint8_t     peer_addr[6];
    uint8_t     peer_addr_type;
    ble_state_t state;
    uint16_t    mtu;
    uint8_t     protocol_version;   /* 3: DK 3.0, 4: DK 4.0 */
    uint32_t    last_activity;
} ble_connection_t;

/* BLE广播参数 */
typedef struct {
    uint8_t  adv_type;
    uint16_t interval_min;
    uint16_t interval_max;
    uint8_t  channel_map;
    uint8_t  filter_policy;
    uint8_t  vehicle_id[6];
    uint8_t  protocol_ver;
    uint8_t  capability;
} ble_adv_param_t;

/* ICCOA GATT服务句柄 */
typedef struct {
    uint16_t service_handle;
    struct {
        uint16_t ctrl_handle;       /* 控制通道 (0xFEF6) */
        uint16_t data_handle;       /* 数据通道 (0xFEF7) */
        uint16_t notify_handle;     /* 通知通道 (0xFEF8) */
        uint16_t auth_handle;       /* 认证通道 (0xFEF9) */
    } char_handles;
    struct {
        uint16_t ctrl_ccc;
        uint16_t data_ccc;
        uint16_t notify_ccc;
        uint16_t auth_ccc;
    } ccc_handles;
} iccoa_dk_gatt_t;

/* 事件回调 */
typedef void (*ble_event_callback_t)(uint16_t conn_handle, uint8_t event, void *data);

/* 主要API接口 */

ble_result_t ble_init(uint8_t protocol_version);
ble_result_t ble_start_adv(const ble_adv_param_t *param);
ble_result_t ble_stop_adv(void);
ble_result_t ble_connect(uint16_t conn_interval);
ble_result_t ble_disconnect(uint16_t conn_handle);
ble_result_t ble_send(uint16_t conn_handle, const uint8_t *data, uint16_t len);
ble_result_t ble_get_connection_info(uint16_t conn_handle, ble_connection_t *info);
ble_result_t ble_register_callback(ble_event_callback_t callback);

#endif /* BLE_COMM_H */
```

#### 3.1.4 ICCOA协议帧格式

**DK 3.0 帧格式:**

```c
#define ICCOA_DK_SERVICE_UUID      0xFEF5

#define ICCOA_DK_CHAR_CTRL         0xFEF6   /* 控制通道 */
#define ICCOA_DK_CHAR_DATA         0xFEF7   /* 数据通道 */
#define ICCOA_DK_CHAR_NOTIFY       0xFEF8   /* 通知通道 */
#define ICCOA_DK_CHAR_AUTH         0xFEF9   /* 认证通道 */

/* DK 3.0 协议帧 */
typedef struct __attribute__((packed)) {
    uint8_t  sop;               /* Start of Packet: 0xAA */
    uint8_t  cmd_id;            /* 命令ID */
    uint16_t seq_num;           /* 序列号 */
    uint16_t payload_len;       /* 负载长度 */
    uint8_t  payload[];         /* 负载数据 */
    uint8_t  checksum;          /* XOR校验和 */
    uint8_t  eop;               /* End of Packet: 0x55 */
} iccoa_dk30_frame_t;

/* DK 3.0 命令 */
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

/* DK 4.0 协议帧 */
typedef struct __attribute__((packed)) {
    uint16_t magic;             /* 0xICC0 */
    uint8_t  version;           /* 协议版本: 4 */
    uint8_t  msg_type;          /* 消息类型 */
    uint16_t msg_id;            /* 消息ID */
    uint16_t flags;             /* 标志位 */
    uint16_t payload_len;       /* 负载长度 */
    uint8_t  session_token[4];  /* 会话令牌 */
    uint8_t  payload[];         /* 负载数据 */
    uint8_t  hmac[16];          /* HMAC-SHA256截断 */
} iccoa_dk40_frame_t;

/* DK 4.0 命令 */
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

#### 3.1.5 ICCOA广播数据

```c
/* ICCOA BLE广播数据格式 */
typedef struct __attribute__((packed)) {
    uint8_t  flags_len;         /* Flags长度 */
    uint8_t  flags_type;        /* 0x01 (Flags) */
    uint8_t  flags;             /* 0x06 */
    uint8_t  svc_len;           /* Service UUID长度 */
    uint8_t  svc_type;          /* 0x03 (Complete 16-bit Service UUID) */
    uint8_t  svc_uuid[2];       /* ICCOA Service UUID (0xFEF5) */
    uint8_t  mfg_len;           /* Manufacturer Data长度 */
    uint8_t  mfg_type;          /* 0xFF (Manufacturer Specific) */
    uint8_t  mfg_id[2];         /* 厂商ID */
    uint8_t  vehicle_id[6];     /* 车辆标识 */
    uint8_t  protocol_ver;      /* 协议版本 */
    uint8_t  capability;        /* 设备能力 */
} iccoa_adv_data_t;
```

---

### 3.2 UWB通信模块 (uwb_comm) — DK 4.0

#### 3.2.1 模块职责
- UWB测距会话管理 (DK 4.0新增)
- IEEE 802.15.4z TWR双向测距
- STS安全测距 (防中继攻击)
- 距离区域判定

#### 3.2.2 接口定义

```c
/**
 * @file uwb_comm.h
 * @brief ICCOA UWB通信模块接口 (DK 4.0选项)
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

/* 距离区域 (ICCOA DK 4.0) */
typedef enum {
    UWB_ZONE_FAR      = 0,   /* >10m */
    UWB_ZONE_APPROACH = 1,   /* 5-10m */
    UWB_ZONE_NEAR     = 2,   /* 2-5m */
    UWB_ZONE_CLOSE    = 3,   /* 0-2m */
    UWB_ZONE_INSIDE   = 4    /* <0.5m */
} uwb_zone_e;

/* UWB会话配置 */
typedef struct {
    uint8_t  session_id[8];
    uint8_t  sts_key[16];
    uint8_t  channel;
    uint8_t  preamble_code;
    uint8_t  prf_len;
} uwb_session_config_t;

/* 测距结果 */
typedef struct {
    uint32_t session_id;
    uint16_t distance_cm;
    int8_t   rssi;
    uint8_t  nlos;
    uint32_t timestamp;
} uwb_ranging_result_t;

/* 主要API接口 */

uwb_result_t uwb_init(void);
uwb_result_t uwb_create_session(const uwb_session_config_t *cfg);
uwb_result_t uwb_start_ranging(uint32_t session_id);
uwb_result_t uwb_stop_ranging(uint32_t session_id);
uwb_result_t uwb_get_distance(uint32_t session_id, uint16_t *dist_cm);
uwb_zone_e   uwb_get_zone(uint32_t session_id);
uwb_result_t uwb_destroy_session(uint32_t session_id);

#endif /* UWB_COMM_H */
```

---

### 3.3 安全认证模块 (security_auth)

#### 3.3.1 模块职责
- 绑定认证 (BIND: 公钥交换+ECDH密钥协商)
- 日常认证 (DAILY: 挑战-响应签名验证)
- 远程认证 (REMOTE: 云端协助认证)
- 分享认证 (SHARE: 钥匙分享认证)
- ECDH P-256密钥协商
- ECDSA签名生成与验证
- AES-GCM数据加密/解密
- HMAC-SHA256消息完整性保护

#### 3.3.2 类图

```
┌──────────────────────────────────────────────────────┐
│                 SecurityManager                        │
├──────────────────────────────────────────────────────┤
│ - se050: Se050Driver                                  │
│ - keyStore: SecureKeyStore                            │
│ - cryptoEngine: CryptoEngine                           │
│ - sessionManager: SessionManager                       │
│ - nonceCache: NonceCache                               │
├──────────────────────────────────────────────────────┤
│ + init(): int32_t                                    │
│ + bindAuth(request, response): int32_t               │
│ + dailyAuth(challenge, response): int32_t            │
│ + remoteAuth(challenge, response): int32_t           │
│ + shareAuth(challenge, response): int32_t            │
│ + ecdhKeyExchange(pubKey, sharedSecret): int32_t    │
│ + sign(data, len, sig): int32_t                      │
│ + verify(data, len, sig): int32_t                    │
│ + encrypt(key, plain, cipher): int32_t               │
│ + decrypt(key, cipher, plain): int32_t               │
│ + hmac(key, data, len, mac): int32_t                 │
└──────────────────────────────────────────────────────┘
```

#### 3.3.3 接口定义

```c
/**
 * @file security_auth.h
 * @brief ICCOA安全认证模块接口
 */

#ifndef SECURITY_AUTH_H
#define SECURITY_AUTH_H

#include <stdint.h>
#include <stdbool.h>

/* 安全错误码 */
typedef enum {
    SEC_SUCCESS = 0,
    SEC_ERR_KEY_NOT_FOUND,
    SEC_ERR_SIGNATURE_INVALID,
    SEC_ERR_CHALLENGE_EXPIRED,
    SEC_ERR_NONCE_REUSE,
    SEC_ERR_ENCRYPTION_FAILED,
    SEC_ERR_DECRYPTION_FAILED,
    SEC_ERR_KEY_GENERATION_FAILED,
    SEC_ERR_ECDH_FAILED,
    SEC_ERR_CERT_INVALID,
    SEC_ERR_HARDWARE_FAULT,
    SEC_ERR_TAMPER_DETECTED
} security_result_t;

/* 认证类型 */
typedef enum {
    ICCOA_AUTH_BIND   = 0x01,  /* 绑定认证 */
    ICCOA_AUTH_DAILY  = 0x02,  /* 日常认证 */
    ICCOA_AUTH_REMOTE = 0x03,  /* 远程认证 */
    ICCOA_AUTH_SHARE  = 0x04   /* 分享认证 */
} iccoa_auth_type_e;

/* ICCOA认证数据结构 */
typedef struct __attribute__((packed)) {
    uint8_t  auth_type;
    uint8_t  user_id[16];
    uint8_t  challenge[16];
    uint8_t  response[32];
    uint8_t  cert[256];
    uint16_t cert_len;
} iccoa_auth_data_t;

/* 会话信息 */
typedef struct {
    uint32_t session_id;
    uint16_t conn_handle;
    uint8_t  session_key[32];
    uint8_t  session_token[4];      /* DK 4.0 会话令牌 */
    uint32_t creation_time;
    uint32_t expiry_time;
    uint8_t  auth_type;
    bool     is_encrypted;
} session_info_t;

/* 主要API接口 */

security_result_t security_init(void);

/**
 * @brief 绑定认证 — 首次配钥时交换公钥
 */
security_result_t security_bind_auth(const uint8_t *peer_pub_key,
                                     uint8_t *own_pub_key,
                                     uint8_t *shared_secret);

/**
 * @brief 日常认证 — 挑战-响应认证
 */
security_result_t security_daily_auth(const iccoa_auth_data_t *request,
                                      iccoa_auth_data_t *response,
                                      session_info_t *session);

/**
 * @brief 远程认证 — 云端协助认证
 */
security_result_t security_remote_auth(const iccoa_auth_data_t *request,
                                       session_info_t *session);

/**
 * @brief 分享认证 — 钥匙分享认证
 */
security_result_t security_share_auth(const iccoa_auth_data_t *request,
                                      session_info_t *session);

/**
 * @brief ECDSA签名
 */
security_result_t security_sign(const uint8_t *data, uint16_t data_len,
                                uint8_t *signature, uint16_t *sig_len);

/**
 * @brief ECDSA验签
 */
security_result_t security_verify(const uint8_t *data, uint16_t data_len,
                                  const uint8_t *signature, uint16_t sig_len,
                                  const uint8_t *public_key);

/**
 * @brief AES-GCM加密
 */
security_result_t security_encrypt(const uint8_t *key, const uint8_t *plaintext,
                                   uint16_t plaintext_len, uint8_t *ciphertext,
                                   uint16_t *ciphertext_len);

/**
 * @brief AES-GCM解密
 */
security_result_t security_decrypt(const uint8_t *key, const uint8_t *ciphertext,
                                   uint16_t ciphertext_len, uint8_t *plaintext,
                                   uint16_t *plaintext_len);

/**
 * @brief HMAC-SHA256计算
 */
security_result_t security_hmac(const uint8_t *key, uint16_t key_len,
                                const uint8_t *data, uint16_t data_len,
                                uint8_t *mac, uint16_t *mac_len);

#endif /* SECURITY_AUTH_H */
```

#### 3.3.4 认证流程

```
┌─────────┐                              ┌─────────┐
│ 手机端  │                              │  车端   │
└────┬────┘                              └────┬────┘
     │                                        │
     │  ==== 绑定认证 (BIND) ====             │
     │                                        │
     │  1. BIND_REQUEST (手机公钥+签名)       │
     ├──────────────────────────────────────►│
     │                         2. 验证签名    │
     │                         3. ECDH密钥协商 │
     │  4. BIND_RESPONSE (车端公钥+签名)      │
     │◄──────────────────────────────────────┤
     │                         5. 存储绑定信息 │
     │                                        │
     │  ==== 日常认证 (DAILY) ====            │
     │                                        │
     │  6. AUTH_REQUEST (挑战值+时间戳)        │
     │├───────────────────────────────────────│  (车端生成挑战)
     │                         7. 验证签名    │
     │                         8. 检查Nonce   │
     │                         9. 生成Session │
     │ 10. AUTH_RESPONSE (会话Token)          │
     │◄──────────────────────────────────────┤
     │                                        │
     │  ==== 加密通信 ====                    │
     │  (AES-GCM + HMAC-SHA256)              │
     │◄──────────────────────────────────────►│
```

---

### 3.4 密钥管理模块 (key_management)

#### 3.4.1 模块职责
- 数字钥匙的绑定/解绑/分享管理
- 权限配置维护 (ICCOA权限位图)
- 钥匙有效期管理
- 使用次数管理 (限次钥匙)
- 钥匙状态同步

#### 3.4.2 接口定义

```c
/**
 * @file key_management.h
 * @brief ICCOA密钥管理模块接口
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
    KEY_ERR_PERMISSION_DENIED,
    KEY_ERR_USAGE_EXCEEDED,
    KEY_ERR_SIGNATURE_INVALID,
    KEY_ERR_HARDWARE_FAULT
} key_result_t;

/* ICCOA权限位图 */
#define ICCOA_PERM_LOCK         (1 << 0)
#define ICCOA_PERM_UNLOCK       (1 << 1)
#define ICCOA_PERM_ENGINE       (1 << 2)
#define ICCOA_PERM_TRUNK        (1 << 3)
#define ICCOA_PERM_WINDOW       (1 << 4)
#define ICCOA_PERM_CLIMATE      (1 << 5)
#define ICCOA_PERM_FIND         (1 << 6)
#define ICCOA_PERM_SEAT         (1 << 7)

/* 钥匙状态 */
typedef enum {
    KEY_STATE_INACTIVE = 0,
    KEY_STATE_ACTIVE,
    KEY_STATE_SUSPENDED,
    KEY_STATE_EXPIRED,
    KEY_STATE_REVOKED
} key_state_e;

/* ICCOA权限结构 */
typedef struct {
    uint8_t  user_id[16];
    uint8_t  permissions[4];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t  max_uses;       /* 最大使用次数 (0=无限) */
    uint8_t  used_count;     /* 已使用次数 */
} iccoa_permission_t;

/* ICCOA钥匙条目 */
typedef struct __attribute__((packed)) {
    uint8_t  key_id[16];
    uint8_t  user_id[16];
    uint8_t  vehicle_id[6];
    uint8_t  permissions[4];
    uint8_t  state;
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t  max_uses;
    uint8_t  used_count;
    uint8_t  pub_key[64];        /* 对端公钥 */
    uint8_t  shared_secret[32];  /* ECDH共享密钥 (加密存储) */
    uint8_t  se_key_slot;        /* SE050密钥槽 */
} iccoa_key_entry_t;

/* 主要API接口 */

key_result_t key_init(void);
key_result_t key_bind(const iccoa_key_entry_t *entry);
key_result_t key_unbind(const uint8_t *user_id);
key_result_t key_get(const uint8_t *user_id, iccoa_key_entry_t *entry);
key_result_t key_list(iccoa_key_entry_t *entries, uint8_t *count);
key_result_t key_share(const uint8_t *user_id, const iccoa_permission_t *perm);
key_result_t key_revoke(const uint8_t *user_id);
key_result_t key_update_permissions(const uint8_t *user_id, const iccoa_permission_t *perm);
bool key_check_permission(const uint8_t *user_id, uint8_t perm_bit);
uint8_t key_get_used_count(const uint8_t *user_id);
key_result_t key_increment_used_count(const uint8_t *user_id);

#endif /* KEY_MANAGEMENT_H */
```

---

### 3.5 离线决策模块 (offline_decision)

#### 3.5.1 模块职责
- 离线状态下本地认证决策
- 权限验证 (有效期/使用次数/权限位)
- 风险评估与安全策略
- 决策日志记录 (用于审计)

#### 3.5.2 类图

```
┌──────────────────────────────────────────────────────┐
│                 DecisionEngine                         │
├──────────────────────────────────────────────────────┤
│ - ruleEngine: RuleEngine                              │
│ - riskAnalyzer: RiskAnalyzer                          │
│ - auditLogger: AuditLogger                            │
│ - keyManager: KeyManager                              │
├──────────────────────────────────────────────────────┤
│ + evaluate(request): Decision                         │
│ + addRule(rule): void                                 │
│ + removeRule(ruleId): void                            │
│ + getRiskScore(request): RiskScore                    │
│ + logDecision(decision): void                         │
└──────────────────────────────────────────────────────┘
```

#### 3.5.3 接口定义

```c
/**
 * @file offline_decision.h
 * @brief ICCOA离线决策模块接口
 */

#ifndef OFFLINE_DECISION_H
#define OFFLINE_DECISION_H

#include <stdint.h>
#include <stdbool.h>

/* 决策结果 */
typedef enum {
    DECISION_ALLOW = 0,
    DECISION_DENY,
    DECISION_REQUIRE_ONLINE,
    DECISION_CHALLENGE_REQUIRED,
    DECISION_COOLDOWN
} decision_result_t;

/* 决策原因 */
typedef enum {
    REASON_SUCCESS = 0,
    REASON_KEY_NOT_FOUND,
    REASON_KEY_EXPIRED,
    REASON_PERMISSION_DENIED,
    REASON_SIGNATURE_INVALID,
    REASON_NONCE_REUSE,
    REASON_RATE_LIMITED,
    REASON_RISK_TOO_HIGH,
    REASON_USAGE_EXCEEDED,
    REASON_OFFLINE_EXPIRED,
    REASON_TIME_INVALID
} decision_reason_t;

/* 风险等级 */
typedef enum {
    RISK_LOW = 0,
    RISK_MEDIUM,
    RISK_HIGH,
    RISK_CRITICAL
} risk_level_t;

/* 决策请求 */
typedef struct {
    uint8_t  user_id[16];
    uint8_t  command_type;
    int8_t   rssi;
    uint32_t timestamp;
    uint8_t  signature[64];
    uint8_t  nonce[16];
} decision_request_t;

/* 决策结果 */
typedef struct {
    decision_result_t  result;
    decision_reason_t  reason;
    risk_level_t       risk_level;
    uint32_t           valid_duration;
    uint8_t            challenge[32];
    uint32_t           decision_id;
} decision_output_t;

/* 主要API接口 */

int32_t decision_init(void);
int32_t decision_evaluate(const decision_request_t *request, decision_output_t *output);
int32_t decision_add_rule(const uint8_t *rule_data, uint16_t len);
int32_t decision_calculate_risk(const decision_request_t *request, risk_level_t *level);
int32_t decision_log(const decision_output_t *decision);
int32_t decision_get_history(uint8_t *history, uint32_t *count);

#endif /* OFFLINE_DECISION_H */
```

#### 3.5.4 决策流程图

```
                    开始
                     │
                     ▼
           ┌─────────────────┐
           │ 接收决策请求    │
           └────────┬────────┘
                    │
                    ▼
           ┌─────────────────┐
           │ 检查请求格式    │──N──► 拒绝
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 查找钥匙信息    │──N──► 拒绝(KEY_NOT_FOUND)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 检查钥匙有效期  │──N──► 拒绝(KEY_EXPIRED)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 验证签名        │──N──► 拒绝(SIGNATURE_INVALID)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 检查使用次数    │──N──► 拒绝(USAGE_EXCEEDED)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 检查权限        │──N──► 拒绝(PERMISSION_DENIED)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 风险评估        │
           └────────┬────────┘
                    │
                    ▼
           ┌─────────────────┐
           │ 风险等级 > 高?  │──Y──► 拒绝(RISK_TOO_HIGH)
           └────────┬────────┘
                    │N
                    ▼
           ┌─────────────────┐
           │ 允许操作        │
           └────────┬────────┘
                    │
                    ▼
                    结束
```

---

### 3.6 车辆集成模块 (vehicle_integration)

#### 3.6.1 模块职责
- CAN总线通信与车辆控制接口
- 车辆状态监听与上报 (通过BLE Notify)
- ICCOA控制命令执行 (锁/解锁/引擎/车窗/空调等)
- 车辆状态数据结构维护

#### 3.6.2 接口定义

```c
/**
 * @file vehicle_integration.h
 * @brief ICCOA车辆集成模块接口
 */

#ifndef VEHICLE_INTEGRATION_H
#define VEHICLE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

/* 车辆控制命令 */
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

/* 控制帧 */
typedef struct __attribute__((packed)) {
    uint8_t  ctrl_cmd;
    uint8_t  ctrl_param;
    uint8_t  result;
    uint8_t  reserved;
} iccoa_ctrl_frame_t;

/* 车辆状态 */
typedef struct __attribute__((packed)) {
    uint8_t  door_status;
    uint8_t  window_status;
    uint8_t  engine_status;
    uint8_t  lock_status;
    int8_t   battery_level;
    int16_t  interior_temp;
    uint8_t  alarm_status;
    uint8_t  reserved[3];
} iccoa_vehicle_status_t;

/* 主要API接口 */

int32_t vehicle_init(void);
int32_t vehicle_execute_command(uint8_t cmd, uint8_t param, uint8_t *result);
int32_t vehicle_get_status(iccoa_vehicle_status_t *status);
int32_t vehicle_register_status_callback(void (*cb)(const iccoa_vehicle_status_t *status));
int32_t vehicle_start_monitoring(void);
int32_t vehicle_stop_monitoring(void);

#endif /* VEHICLE_INTEGRATION_H */
```

---

### 3.7 异常处理模块 (exception_handler)

#### 3.7.1 模块职责
- 异常检测与分级
- 异常日志记录 (本地存储 + 云端上报)
- 故障恢复策略
- 安全告警

#### 3.7.2 接口定义

```c
/**
 * @file exception_handler.h
 * @brief ICCOA异常处理模块接口
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

/* 异常类型 (ICCOA) */
typedef enum {
    EXCEP_AUTH_FAILED        = 0x01,
    EXCEP_SIGNATURE_INVALID  = 0x02,
    EXCEP_NONCE_REUSE        = 0x03,
    EXCEP_KEY_EXPIRED        = 0x04,
    EXCEP_PERMISSION_DENIED  = 0x05,
    EXCEP_USAGE_EXCEEDED     = 0x06,
    EXCEP_COMM_TIMEOUT       = 0x10,
    EXCEP_CONNECTION_LOST    = 0x11,
    EXCEP_BLE_ERROR          = 0x12,
    EXCEP_STORAGE_ERROR      = 0x20,
    EXCEP_TAMPER_DETECTED    = 0xF0
} exception_type_t;

/* 恢复动作 */
typedef enum {
    RECOVERY_NONE = 0,
    RECOVERY_RETRY,
    RECOVERY_RESTART_BLE,
    RECOVERY_FALLBACK,
    RECOVERY_SAFE_MODE,
    RECOVERY_SHUTDOWN
} recovery_action_t;

/* 主要API接口 */

int32_t exception_init(void);
int32_t exception_report(exception_type_t type, exception_level_t level,
                         const uint8_t *context, uint16_t ctx_len);
int32_t exception_handle(uint32_t exception_id);
int32_t exception_register_callback(void (*cb)(exception_type_t type, const uint8_t *ctx));
int32_t exception_get_health_status(uint8_t *status);

#endif /* EXCEPTION_HANDLER_H */
```

---

## 4. 状态机设计

### 4.1 系统主状态机

```
                    ┌──────────┐
                    │  SLEEP   │ ← 低功耗待机
                    └────┬─────┘
                         │ BLE广播唤醒
                         ▼
                    ┌──────────┐
                    │ ADVERTISE│ ← 发送ICCOA广播
                    └────┬─────┘
                         │ 手机扫描连接
                         ▼
                    ┌──────────┐
                    │ CONNECTED│ ← BLE连接建立
                    └────┬─────┘
                         │ 协议版本协商
                         ▼
              ┌──────────────────────┐
              │                      │
         DK 3.0                 DK 4.0
              │                      │
              ▼                      ▼
     ┌─────────────────┐  ┌──────────────────┐
     │ DK30_AUTH       │  │ SESSION_OPEN     │
     │ 绑定/日常认证    │  │ 会话令牌交换      │
     └────────┬────────┘  └────────┬─────────┘
              │                    │
              ▼                    ▼
     ┌─────────────────┐  ┌──────────────────┐
     │ COMMAND_EXEC    │  │ UWB_CONFIG       │
     │ 车辆控制/状态    │  │ UWB测距配置       │
     └────────┬────────┘  └────────┬─────────┘
              │                    │
              ▼                    ▼
     ┌─────────────────┐  ┌──────────────────┐
     │     IDLE        │  │ UWB_RANGING      │
     │  等待下一命令    │  │ 测距+区域判定     │
     └────────┬────────┘  └────────┬─────────┘
              │                    │
              └────────┬───────────┘
                       │ 断开/超时
                       ▼
                  ┌──────────┐
                  │  SLEEP   │
                  └──────────┘
```

### 4.2 连接状态机

```
   DISCONNECTED ───► SCANNING ───► CONNECTING ───► CONNECTED
        ▲                                                │
        │                                         PROTOCOL_NEG
        │                                                │
        │                     READY ◄── AUTH_OK ◄── AUTH
        │                        │
        └────────────────────────┘
           断开/超时回到DISCONNECTED
```

---

## 5. 安全设计

### 5.1 安全架构

```
┌─────────────────────────────────────────────────────────────┐
│                    ICCOA安全体系架构                          │
├─────────────────────────────────────────────────────────────┤
│  1. ECDH P-256密钥协商                                       │
│     - 绑定阶段交换公钥                                       │
│     - 生成共享密钥 (Shared Secret)                           │
│     - 派生会话密钥 (KDF)                                     │
│                                                              │
│  2. ECDSA签名体系                                            │
│     - 绑定请求/响应签名                                       │
│     - 日常认证签名                                           │
│     - 控制命令签名                                            │
│                                                              │
│  3. AES-256-GCM加密                                          │
│     - 密钥数据传输加密                                        │
│     - 会话层通信加密                                          │
│                                                              │
│  4. HMAC-SHA256消息完整性 (DK 4.0)                           │
│     - 协议帧HMAC字段                                          │
│     - 防篡改保护                                              │
│                                                              │
│  5. 防重放攻击                                               │
│     - Nonce机制 (每次递增)                                    │
│     - 时间戳校验                                              │
│     - 序列号跟踪                                              │
│                                                              │
│  6. 安全存储 (SE050)                                         │
│     - 私钥存储在SE050                                        │
│     - 共享密钥加密存储                                        │
│     - 防物理攻击                                              │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 密钥协商流程 (ECDH)

```
1. 手机端生成 ECDH P-256 密钥对 (sk_m, pk_m)
2. 车端生成 ECDH P-256 密钥对 (sk_c, pk_c)
3. 手机 → 车端: BIND_REQUEST(pk_m + signature)
4. 车端验证签名, 计算 shared_secret = ECDH(sk_c, pk_m)
5. 车端 → 手机: BIND_RESPONSE(pk_c + signature)
6. 手机验证签名, 计算 shared_secret = ECDH(sk_m, pk_c)
7. 双方派生: session_key = KDF(shared_secret, nonce, "ICCOA-DK")
8. 后续通信使用 session_key 加密/HMAC
```

---

## 6. 关键流程

### 6.1 绑定流程 (Key Binding)

```
┌─────────┐                              ┌─────────┐     ┌─────────┐
│ 手机端  │                              │  车端   │     │ SE050  │
└────┬────┘                              └────┬────┘     └────┬────┘
     │                                        │               │
     │  1. BLE Scan + Connect                 │               │
     │◄──────────────────────────────────────►│               │
     │                                        │               │
     │  2. 协议版本协商 (DK 3.0/4.0)          │               │
     │◄──────────────────────────────────────►│               │
     │                                        │               │
     │  3. BIND_REQUEST                       │               │
     │  (手机公钥pk_m + 签名 + 用户ID)        │               │
     ├──────────────────────────────────────►│               │
     │                                        │               │
     │                         4. 验签         │               │
     │                         5. ECDH密钥协商  │──────────────►│
     │                         6. 生成共享密钥  │◄──────────────│
     │                                        │               │
     │  7. BIND_RESPONSE                      │               │
     │  (车端公钥pk_c + 签名 + 结果)          │               │
     │◄──────────────────────────────────────┤               │
     │                                        │               │
     │  8. 绑定完成, 存储绑定信息              │               │
     │                                        │               │
```

### 6.2 车辆控制流程 (Vehicle Control)

```
┌─────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ 手机端  │    │ BLE模块  │    │ 决策模块 │    │ 车辆模块 │
└────┬────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘
     │              │               │               │
     │  1. CTRL_REQUEST              │               │
     │  (命令+签名+Nonce)            │               │
     ├──────────────►               │               │
     │              │               │               │
     │              │  2. 转发请求  │               │
     │              │──────────────►│               │
     │              │               │               │
     │              │               │  3. 验证签名  │
     │              │               │  4. 检查Nonce │
     │              │               │  5. 检查权限  │
     │              │               │  6. 检查次数  │
     │              │               │               │
     │              │               │  7. 执行命令  │
     │              │               │──────────────►│
     │              │               │               │──CAN──►
     │              │               │               │◄─结果─│
     │              │               │               │
     │              │               │  8. 更新使用计数             │
     │              │               │               │
     │  9. CTRL_RESPONSE            │               │
     │◄──────────────│◄──────────────│◄──────────────│
     │              │               │               │
```

### 6.3 状态同步流程 (Status Notification)

```
1. 手机订阅 BLE Notify (ICCOA_DK_CHAR_NOTIFY)
2. 车端定期 (或事件触发) 采集车辆状态
3. 车端封装 iccoa_vehicle_status_t
4. 通过 BLE Notification 发送
5. 手机端解析并展示

状态更新触发条件:
  - 定时更新: 每 5 秒 (连接态)
  - 事件触发: 门锁变化/引擎状态变化/报警触发
  - 请求响应: 手机主动查询
```

---

## 7. 测试策略

### 7.1 单元测试

| 模块 | 测试项 | 覆盖率目标 |
|------|--------|-----------|
| BLE通信 | 广播/扫描、GATT服务、连接管理、帧封装解析、DK3.0/DK4.0协议 | ≥90% |
| UWB通信 | 会话管理、TWR测距、STS安全测距、区域判定 (DK 4.0) | ≥85% |
| 安全认证 | ECDH密钥协商、ECDSA签名/验签、AES-GCM加解密、HMAC | ≥95% |
| 密钥管理 | 绑定/解绑、权限验证、使用次数、状态转换 | ≥95% |
| 离线决策 | 决策评估、风险计算、规则引擎、日志管理 | ≥90% |
| 车辆集成 | 命令执行、状态采集、CAN通信 | ≥85% |

### 7.2 集成测试

| 测试场景 | 描述 |
|---------|------|
| 绑定流程 | 完整绑定流程 (扫描→连接→公钥交换→ECDH→存储) |
| 解锁控制 | 手机→车端解锁命令的完整链路 |
| 多连接 | DK 4.0多手机同时连接管理 |
| 协议切换 | DK 3.0 ↔ DK 4.0协议版本兼容性 |
| 断连重连 | BLE断开后自动重连与会话恢复 |

### 7.3 安全测试

| 测试项 | 描述 |
|--------|------|
| 重放攻击 | 截获帧重放验证 (Nonce/序列号检查) |
| 签名伪造 | 伪造签名绑定的阻断验证 |
| 越权操作 | 低权限钥匙执行高权限操作的拒绝验证 |
| 超量使用 | 限次钥匙超过使用次数后的拒绝验证 |
| 中间人攻击 | ECDH密钥协商过程防MITM验证 (签名绑定) |

### 7.4 性能测试

| 测试项 | 目标值 |
|--------|--------|
| BLE连接建立时间 | ≤ 300ms |
| 绑定流程完成时间 | ≤ 2000ms |
| 日常认证时间 | ≤ 200ms |
| 车辆控制命令响应 | ≤ 500ms |
| 状态通知延迟 | ≤ 1000ms |
| 离线决策时间 | ≤ 50ms |

---

## 8. 配置参数

| 参数 | 默认值 | 范围 | 描述 |
|------|--------|------|------|
| MAX_CONNECTIONS | 4 | 1-8 | 最大并发BLE连接数 |
| MAX_KEYS | 16 | 1-32 | 最大存储钥匙数 |
| AUTH_TIMEOUT_MS | 5000 | 1000-30000 | 认证超时 |
| CACHE_MAX_SIZE | 524288 | 65536-2097152 | 缓存最大字节数 |
| CACHE_DEFAULT_TTL | 86400 | 3600-604800 | 缓存TTL(秒) |
| DECISION_TIMEOUT_MS | 500 | 100-5000 | 决策超时 |
| MAX_RETRY_COUNT | 3 | 1-10 | 最大重试次数 |
| OFFLINE_VALID_HOURS | 24 | 1-168 | 离线有效期(小时) |
| STATUS_NOTIFY_INTERVAL | 5000 | 1000-30000 | 状态通知间隔(ms) |
| SESSION_TIMEOUT_S | 300 | 60-3600 | 会话超时(秒) |

## 9. 附录

### 9.1 错误码汇总表

| 模块 | 错误码范围 | 描述 |
|------|-----------|------|
| BLE | 0x00-0x0F | BLE通信错误 |
| UWB | 0x10-0x1F | UWB测距错误 (DK 4.0) |
| Security | 0x20-0x2F | 安全认证错误 |
| Key Management | 0x30-0x3F | 密钥管理错误 |
| Decision | 0x40-0x4F | 决策引擎错误 |
| Vehicle | 0x50-0x5F | 车辆集成错误 |
| Exception | 0x60-0x6F | 异常处理错误 |
| System | 0xF0-0xFF | 系统级错误 |

### 9.2 硬件依赖

| 组件 | 接口 | 驱动 | 依赖版本 |
|------|------|------|---------|
| STM32L5 | - | STM32 HAL | 1.27+ |
| KW47A | SPI+UART | KW47A SDK | 2.x |
| NCJ29D6 (DK 4.0) | SPI | NCJ29D6 SDK | 1.x |
| SE050 | I2C | Plug & Trust Middleware | 4.x |

### 9.3 ICCOA DK 3.0 vs DK 4.0 差异矩阵

| 特性 | DK 3.0 | DK 4.0 |
|------|--------|--------|
| BLE通信 | ✓ 必须 | ✓ 必须 |
| UWB测距 | ✗ 不支持 | ✓ 可选支持 |
| NFC支持 | ✗ 可选 | ✗ 可选 |
| 帧格式 | 0xAA/0x55定界 + XOR校验 | 0xICC0 Magic + HMAC |
| 多连接 | 1对1 | 多对多 |
| 远程分享 | 仅本地 | 云端推送 |
| 会话管理 | 无 | 会话令牌 + 生命周期 |
| 加密 | AES-128 | AES-256-GCM |
| 证书 | 简化 | 数字身份 |

---

**文档结束**
