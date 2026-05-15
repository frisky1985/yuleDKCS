# ICCE数字钥匙协议栈 - 模块设计文档

## 1. 模块概述

本文档详细描述ICCE数字钥匙协议栈的各个模块设计，包括模块架构、接口定义、数据结构和关键算法。

## 2. 模块划分

```
┌────────────────────────────────────────────────────────────┐
│                    ICCE数字钥匙协议栈                        │
├────────────────────────────────────────────────────────────┤
│  模块1: BLE通信模块 (ble_comm)                              │
│  模块2: 安全认证模块 (security_auth)                        │
│  模块3: 缓存管理模块 (cache_manager)                        │
│  模块4: 离线决策模块 (offline_decision)                     │
│  模块5: 车辆集成模块 (vehicle_integration)                  │
│  模块6: 异常处理模块 (exception_handler)                    │
└────────────────────────────────────────────────────────────┘
```

---

## 3. 模块详细设计

### 3.1 BLE通信模块 (ble_comm)

#### 3.1.1 模块职责
- BLE设备广播与扫描
- 连接建立与管理
- GATT服务与特征管理
- 数据包收发与重组

#### 3.1.2 类图

```
┌─────────────────────────────────────────────────┐
│              BleManager                          │
├─────────────────────────────────────────────────┤
│ - adapter: BleAdapter                            │
│ - connections: Map<DeviceId, Connection>         │
│ - gattServer: GattServer                         │
│ - stateMachine: ConnectionStateMachine           │
├─────────────────────────────────────────────────┤
│ + initialize(): Result                           │
│ + startAdvertising(config: AdvConfig): void      │
│ + stopAdvertising(): void                        │
│ + connect(device: Device): Result                │
│ + disconnect(device: Device): void               │
│ + sendData(device: Device, data: bytes): Result  │
│ + registerCallback(cb: BleCallback): void        │
└─────────────────────────────────────────────────┘
           │
           ├───┬────────────────┬──────────────┐
           │   │                │              │
           ▼   ▼                ▼              ▼
    ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
    │BleAdapter│  │Connection│  │GattServer│  │AdvManager│
    └──────────┘  └──────────┘  └──────────┘  └──────────┘
```

#### 3.1.3 接口定义

```c
/**
 * @file ble_manager.h
 * @brief BLE通信管理模块接口
 */

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* 错误码定义 */
typedef enum {
    BLE_SUCCESS = 0,
    BLE_ERR_NOT_INITIALIZED,
    BLE_ERR_ALREADY_INITIALIZED,
    BLE_ERR_ADAPTER_NOT_FOUND,
    BLE_ERR_CONNECTION_FAILED,
    BLE_ERR_TIMEOUT,
    BLE_ERR_INVALID_PARAM,
    BLE_ERR_BUFFER_OVERFLOW,
    BLE_ERR_HARDWARE_FAULT
} ble_result_t;

/* 设备地址类型 */
typedef enum {
    BLE_ADDR_PUBLIC,
    BLE_ADDR_RANDOM,
    BLE_ADDR_RPA_PUBLIC,
    BLE_ADDR_RPA_RANDOM
} ble_addr_type_t;

/* 设备地址结构 */
typedef struct {
    uint8_t addr[6];
    ble_addr_type_t type;
} ble_device_addr_t;

/* 设备标识 */
typedef struct {
    ble_device_addr_t address;
    uint8_t name[32];
    int8_t rssi;
} ble_device_t;

/* 连接参数 */
typedef struct {
    uint16_t conn_interval_min;    // 最小连接间隔 (1.25ms单位)
    uint16_t conn_interval_max;    // 最大连接间隔
    uint16_t slave_latency;         // 从机延迟
    uint16_t supervision_timeout;   // 超时时间 (10ms单位)
} ble_conn_params_t;

/* 广播配置 */
typedef struct {
    uint8_t flags;
    uint16_t appearance;
    uint8_t local_name[32];
    uint16_t service_uuids[8];
    uint8_t service_uuid_count;
    uint32_t min_interval_ms;
    uint32_t max_interval_ms;
} ble_adv_config_t;

/* 连接状态 */
typedef enum {
    BLE_CONN_STATE_DISCONNECTED,
    BLE_CONN_STATE_CONNECTING,
    BLE_CONN_STATE_CONNECTED,
    BLE_CONN_STATE_DISCONNECTING,
    BLE_CONN_STATE_ENCRYPTING,
    BLE_CONN_STATE_ENCRYPTED
} ble_conn_state_t;

/* 连接信息 */
typedef struct {
    uint16_t conn_handle;
    ble_device_t device;
    ble_conn_state_t state;
    ble_conn_params_t params;
    uint32_t last_activity_time;
} ble_connection_t;

/* 回调函数类型 */
typedef void (*ble_event_callback_t)(uint16_t conn_handle, uint8_t event, void *data);

/* 主要API接口 */

/**
 * @brief 初始化BLE模块
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_manager_init(void);

/**
 * @brief 启动广播
 * @param config 广播配置
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_start_advertising(const ble_adv_config_t *config);

/**
 * @brief 停止广播
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_stop_advertising(void);

/**
 * @brief 开始扫描
 * @param duration_ms 扫描持续时间
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_start_scan(uint32_t duration_ms);

/**
 * @brief 停止扫描
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_stop_scan(void);

/**
 * @brief 连接设备
 * @param device 目标设备
 * @param params 连接参数
 * @param conn_handle 输出连接句柄
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_connect(const ble_device_t *device, 
                         const ble_conn_params_t *params,
                         uint16_t *conn_handle);

/**
 * @brief 断开连接
 * @param conn_handle 连接句柄
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_disconnect(uint16_t conn_handle);

/**
 * @brief 发送数据
 * @param conn_handle 连接句柄
 * @param data 数据指针
 * @param length 数据长度
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_send_data(uint16_t conn_handle, 
                           const uint8_t *data, 
                           uint16_t length);

/**
 * @brief 注册事件回调
 * @param callback 回调函数
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_register_callback(ble_event_callback_t callback);

/**
 * @brief 获取连接信息
 * @param conn_handle 连接句柄
 * @param conn_info 输出连接信息
 * @return BLE_SUCCESS 成功
 */
ble_result_t ble_get_connection_info(uint16_t conn_handle, 
                                     ble_connection_t *conn_info);

#endif /* BLE_MANAGER_H */
```

#### 3.1.4 数据结构

```c
/* GATT服务定义 */
#define GATT_UUID_DIGITAL_KEY_SERVICE    0x18F0
#define GATT_UUID_KEY_STATUS            0x2AF1
#define GATT_UUID_RANGING_DATA          0x2AF2
#define GATT_UUID_AUTH_CHALLENGE        0x2AF3
#define GATT_UUID_CONTROL_COMMAND       0x2AF4
#define GATT_UUID_SESSION_KEY           0x2AF5

/* 钥匙状态特征 */
typedef struct __attribute__((packed)) {
    uint8_t key_validity;      // 0:无效, 1:有效
    uint8_t permission_level;   // 0:访客, 1:普通, 2:管理员, 3:车主
    uint16_t key_id;           // 钥匙ID
    uint32_t expiry_time;      // 过期时间戳
    uint8_t reserved[8];
} key_status_t;

/* 测距数据特征 */
typedef struct __attribute__((packed)) {
    int8_t rssi;              // 信号强度 dBm
    uint16_t tof_distance;    // 飞行时间测距 cm
    uint8_t accuracy;         // 精度等级 0-3
    uint8_t reserved[4];
} ranging_data_t;

/* 认证挑战特征 */
typedef struct __attribute__((packed)) {
    uint8_t challenge[32];    // 挑战值
    uint8_t nonce[16];        // 随机数
    uint32_t timestamp;       // 时间戳
} auth_challenge_t;

/* 认证响应 */
typedef struct __attribute__((packed)) {
    uint8_t signature[64];    // ECDSA签名
    uint8_t public_key[64];   // 公钥 (可选)
    uint32_t timestamp;       // 时间戳
} auth_response_t;

/* 控制命令 */
typedef struct __attribute__((packed)) {
    uint8_t command_type;     // 命令类型
    uint8_t target;           // 目标设备
    uint32_t user_id;         // 用户ID
    uint8_t hmac[32];         // 命令HMAC
} control_command_t;

/* 命令类型定义 */
typedef enum {
    CMD_UNLOCK_DOOR = 0x01,
    CMD_LOCK_DOOR = 0x02,
    CMD_START_ENGINE = 0x03,
    CMD_STOP_ENGINE = 0x04,
    CMD_OPEN_TRUNK = 0x05,
    CMD_QUERY_STATUS = 0x06
} command_type_t;
```

---

### 3.2 安全认证模块 (security_auth)

#### 3.2.1 模块职责
- 密钥管理与存储
- 挑战-响应认证
- 会话密钥协商
- 签名验证与生成

#### 3.2.2 类图

```
┌─────────────────────────────────────────────────┐
│              SecurityManager                     │
├─────────────────────────────────────────────────┤
│ - keyStore: KeyStore                             │
│ - cryptoEngine: CryptoEngine                     │
│ - sessionManager: SessionManager                 │
│ - nonceCache: NonceCache                         │
├─────────────────────────────────────────────────┤
│ + generateChallenge(): Challenge                 │
│ + verifyResponse(challenge, response): Result    │
│ + establishSession(connHandle): SessionKey       │
│ + encryptData(key, data): CipherData             │
│ + decryptData(key, cipherData): Data             │
│ + signData(privateKey, data): Signature          │
│ + verifySignature(publicKey, data, sig): bool    │
│ + storeKey(keyId, key): Result                   │
│ + loadKey(keyId): Key                            │
└─────────────────────────────────────────────────┘
```

#### 3.2.3 接口定义

```c
/**
 * @file security_auth.h
 * @brief 安全认证模块接口
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
    SEC_ERR_HARDWARE_FAULT,
    SEC_ERR_TAMPER_DETECTED
} security_result_t;

/* 密钥类型 */
typedef enum {
    KEY_TYPE_AES_128,
    KEY_TYPE_AES_256,
    KEY_TYPE_ECC_P256_PRIVATE,
    KEY_TYPE_ECC_P256_PUBLIC,
    KEY_TYPE_HMAC_SHA256
} key_type_t;

/* 密钥结构 */
typedef struct {
    uint32_t key_id;
    key_type_t type;
    uint16_t length;
    uint8_t data[64];
    uint32_t creation_time;
    uint32_t expiry_time;
    uint8_t flags;          // 权限标志
} crypto_key_t;

/* 挑战结构 */
typedef struct {
    uint8_t nonce[16];
    uint8_t server_random[32];
    uint32_t timestamp;
    uint32_t expiry;
} auth_challenge_t;

/* 认证响应 */
typedef struct {
    uint8_t client_random[32];
    uint8_t signature[64];
    uint8_t public_key[64];
    uint32_t timestamp;
} auth_response_t;

/* 会话信息 */
typedef struct {
    uint32_t session_id;
    uint16_t conn_handle;
    uint8_t session_key[32];
    uint8_t session_id_key[16];
    uint32_t creation_time;
    uint32_t expiry_time;
    uint8_t is_encrypted;
} session_info_t;

/* 主要API接口 */

/**
 * @brief 初始化安全模块
 */
security_result_t security_init(void);

/**
 * @brief 生成认证挑战
 * @param challenge 输出挑战
 * @return SEC_SUCCESS 成功
 */
security_result_t security_generate_challenge(auth_challenge_t *challenge);

/**
 * @brief 验证认证响应
 * @param challenge 原始挑战
 * @param response 认证响应
 * @param session 输出会话信息
 * @return SEC_SUCCESS 成功
 */
security_result_t security_verify_response(
    const auth_challenge_t *challenge,
    const auth_response_t *response,
    session_info_t *session);

/**
 * @brief 建立会话密钥
 * @param conn_handle 连接句柄
 * @param public_key 对端公钥
 * @param session 输出会话
 * @return SEC_SUCCESS 成功
 */
security_result_t security_establish_session(
    uint16_t conn_handle,
    const uint8_t *public_key,
    session_info_t *session);

/**
 * @brief 加密数据
 * @param session 会话信息
 * @param plaintext 明文
 * @param plaintext_len 明文长度
 * @param ciphertext 输出密文
 * @param ciphertext_len 输出长度
 * @return SEC_SUCCESS 成功
 */
security_result_t security_encrypt(
    const session_info_t *session,
    const uint8_t *plaintext,
    uint16_t plaintext_len,
    uint8_t *ciphertext,
    uint16_t *ciphertext_len);

/**
 * @brief 解密数据
 */
security_result_t security_decrypt(
    const session_info_t *session,
    const uint8_t *ciphertext,
    uint16_t ciphertext_len,
    uint8_t *plaintext,
    uint16_t *plaintext_len);

/**
 * @brief 签名数据
 */
security_result_t security_sign(
    const crypto_key_t *private_key,
    const uint8_t *data,
    uint16_t data_len,
    uint8_t *signature,
    uint16_t *sig_len);

/**
 * @brief 验证签名
 */
security_result_t security_verify_signature(
    const crypto_key_t *public_key,
    const uint8_t *data,
    uint16_t data_len,
    const uint8_t *signature,
    uint16_t sig_len);

/**
 * @brief 存储密钥到安全存储
 */
security_result_t security_store_key(uint32_t key_id, const crypto_key_t *key);

/**
 * @brief 从安全存储加载密钥
 */
security_result_t security_load_key(uint32_t key_id, crypto_key_t *key);

/**
 * @brief 销毁会话
 */
security_result_t security_destroy_session(uint32_t session_id);

#endif /* SECURITY_AUTH_H */
```

#### 3.2.4 认证流程

```
┌─────────┐                                    ┌─────────┐
│ 手机端  │                                    │  车端   │
└────┬────┘                                    └────┬────┘
     │                                              │
     │  1. 请求认证                                 │
     │─────────────────────────────────────────────►│
     │                                              │
     │  2. 生成挑战(Nonce + ServerRandom + TS)      │
     │◄─────────────────────────────────────────────│
     │                                              │
     │  3. 签名响应(ClientRandom + Sig + PK)        │
     │─────────────────────────────────────────────►│
     │                                              │
     │                         4. 验证签名          │
     │                         5. 检查Nonce         │
     │                         6. 检查时间戳        │
     │                         7. 生成SessionKey    │
     │                                              │
     │  8. 会话密钥确认                             │
     │◄─────────────────────────────────────────────│
     │                                              │
     │  9. 加密通信开始                             │
     │◄────────────────────────────────────────────►│
     │                                              │
```

---

### 3.3 缓存管理模块 (cache_manager)

#### 3.3.1 模块职责
- 钥匙数据缓存
- 权限配置缓存
- 缓存生命周期管理
- 缓存同步策略

#### 3.3.2 类图

```
┌─────────────────────────────────────────────────┐
│              CacheManager                        │
├─────────────────────────────────────────────────┤
│ - memoryCache: LRUCache                          │
│ - persistentCache: Storage                       │
│ - syncQueue: Queue<SyncTask>                     │
│ - policy: CachePolicy                            │
├─────────────────────────────────────────────────┤
│ + get(key): Value                                │
│ + set(key, value, ttl): void                     │
│ + delete(key): void                              │
│ + clear(): void                                  │
│ + sync(): Result                                 │
│ + setPolicy(policy): void                        │
│ + getStats(): CacheStats                         │
└─────────────────────────────────────────────────┘
```

#### 3.3.3 接口定义

```c
/**
 * @file cache_manager.h
 * @brief 缓存管理模块接口
 */

#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* 缓存错误码 */
typedef enum {
    CACHE_SUCCESS = 0,
    CACHE_ERR_NOT_FOUND,
    CACHE_ERR_EXPIRED,
    CACHE_ERR_STORAGE_FULL,
    CACHE_ERR_INVALID_KEY,
    CACHE_ERR_INVALID_VALUE,
    CACHE_ERR_SYNC_FAILED,
    CACHE_ERR_CORRUPTED
} cache_result_t;

/* 缓存类型 */
typedef enum {
    CACHE_TYPE_MEMORY,      // 内存缓存
    CACHE_TYPE_PERSISTENT,  // 持久化缓存
    CACHE_TYPE_SECURE       // 安全缓存(TEE)
} cache_type_t;

/* 缓存策略 */
typedef enum {
    CACHE_POLICY_LRU,       // 最近最少使用
    CACHE_POLICY_LFU,       // 最不经常使用
    CACHE_POLICY_FIFO,      // 先进先出
    CACHE_POLICY_TTL        // 按过期时间
} cache_policy_t;

/* 缓存项 */
typedef struct {
    uint8_t key[32];
    uint16_t key_len;
    uint8_t *value;
    uint32_t value_len;
    uint32_t creation_time;
    uint32_t expiry_time;
    uint32_t access_count;
    uint32_t last_access_time;
    uint8_t flags;
} cache_item_t;

/* 缓存统计 */
typedef struct {
    uint32_t total_items;
    uint32_t memory_usage;
    uint32_t hit_count;
    uint32_t miss_count;
    uint32_t eviction_count;
    uint32_t sync_count;
    float hit_rate;
} cache_stats_t;

/* 缓存配置 */
typedef struct {
    cache_type_t type;
    cache_policy_t policy;
    uint32_t max_size;
    uint32_t default_ttl;
    uint8_t enable_sync;
    uint32_t sync_interval;
} cache_config_t;

/* 主要API接口 */

/**
 * @brief 初始化缓存管理器
 */
cache_result_t cache_init(const cache_config_t *config);

/**
 * @brief 获取缓存值
 */
cache_result_t cache_get(const uint8_t *key, uint16_t key_len,
                         uint8_t *value, uint32_t *value_len);

/**
 * @brief 设置缓存值
 */
cache_result_t cache_set(const uint8_t *key, uint16_t key_len,
                         const uint8_t *value, uint32_t value_len,
                         uint32_t ttl);

/**
 * @brief 删除缓存项
 */
cache_result_t cache_delete(const uint8_t *key, uint16_t key_len);

/**
 * @brief 清空缓存
 */
cache_result_t cache_clear(void);

/**
 * @brief 检查缓存是否存在
 */
cache_result_t cache_exists(const uint8_t *key, uint16_t key_len, bool *exists);

/**
 * @brief 获取缓存统计
 */
cache_result_t cache_get_stats(cache_stats_t *stats);

/**
 * @brief 同步缓存到持久化存储
 */
cache_result_t cache_sync(void);

/**
 * @brief 设置缓存策略
 */
cache_result_t cache_set_policy(cache_policy_t policy);

/**
 * @brief 预加载缓存
 */
cache_result_t cache_preload(const cache_item_t *items, uint32_t count);

#endif /* CACHE_MANAGER_H */
```

#### 3.3.4 钥匙缓存数据结构

```c
/* 钥匙缓存项 */
typedef struct {
    uint32_t key_id;
    uint8_t public_key[64];
    uint8_t permission_level;
    uint32_t owner_user_id;
    uint32_t creation_time;
    uint32_t expiry_time;
    uint8_t status;         // 0:无效, 1:有效, 2:暂停
    uint8_t key_fingerprint[32];
    uint32_t last_sync_time;
    uint8_t reserved[16];
} key_cache_item_t;

/* 权限缓存项 */
typedef struct {
    uint32_t user_id;
    uint8_t permissions[16];    // 位图表示各项权限
    uint32_t valid_from;
    uint32_t valid_until;
    uint32_t last_update;
    uint8_t signature[64];      // 云端签名
} permission_cache_item_t;

/* 令牌缓存项 */
typedef struct {
    uint8_t token_id[16];
    uint8_t token[32];
    uint32_t user_id;
    uint32_t creation_time;
    uint32_t expiry_time;
    uint8_t scope[8];
} token_cache_item_t;
```

---

### 3.4 离线决策模块 (offline_decision)

#### 3.4.1 模块职责
- 本地认证决策
- 风险评估
- 离线权限验证
- 决策日志记录

#### 3.4.2 类图

```
┌─────────────────────────────────────────────────┐
│              DecisionEngine                      │
├─────────────────────────────────────────────────┤
│ - ruleEngine: RuleEngine                         │
│ - riskAnalyzer: RiskAnalyzer                     │
│ - auditLogger: AuditLogger                       │
│ - cacheManager: CacheManager                     │
├─────────────────────────────────────────────────┤
│ + evaluate(request): Decision                    │
│ + addRule(rule): void                            │
│ + removeRule(ruleId): void                       │
│ + getRiskScore(request): RiskScore               │
│ + logDecision(decision): void                    │
└─────────────────────────────────────────────────┘
```

#### 3.4.3 接口定义

```c
/**
 * @file offline_decision.h
 * @brief 离线决策模块接口
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
    REASON_OFFLINE_EXPIRED,
    REASON_DEVICE_MISMATCH,
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
    uint32_t user_id;
    uint32_t key_id;
    uint8_t command_type;
    uint8_t device_fingerprint[32];
    int8_t rssi;
    uint32_t timestamp;
    uint8_t signature[64];
    uint8_t nonce[16];
} decision_request_t;

/* 风险评分 */
typedef struct {
    risk_level_t level;
    uint32_t score;             // 0-100
    uint8_t factors[8];         // 风险因素位图
    float confidence;           // 置信度
} risk_score_t;

/* 决策结果 */
typedef struct {
    decision_result_t result;
    decision_reason_t reason;
    risk_score_t risk_score;
    uint32_t user_id;
    uint32_t key_id;
    uint8_t command_type;
    uint32_t valid_duration;    // 有效时长(秒)
    uint8_t challenge[32];      // 如果需要额外挑战
    uint32_t decision_id;       // 决策ID(用于审计)
} decision_output_t;

/* 决策规则 */
typedef struct {
    uint32_t rule_id;
    uint8_t rule_type;
    uint8_t condition[64];
    uint8_t action;
    int32_t priority;
    uint8_t enabled;
} decision_rule_t;

/* 主要API接口 */

/**
 * @brief 初始化决策引擎
 */
int32_t decision_init(void);

/**
 * @brief 评估决策请求
 */
int32_t decision_evaluate(const decision_request_t *request,
                          decision_output_t *output);

/**
 * @brief 添加决策规则
 */
int32_t decision_add_rule(const decision_rule_t *rule);

/**
 * @brief 移除决策规则
 */
int32_t decision_remove_rule(uint32_t rule_id);

/**
 * @brief 计算风险评分
 */
int32_t decision_calculate_risk(const decision_request_t *request,
                                risk_score_t *score);

/**
 * @brief 记录决策日志
 */
int32_t decision_log(const decision_output_t *decision);

/**
 * @brief 获取决策历史
 */
int32_t decision_get_history(uint32_t user_id,
                             decision_output_t *history,
                             uint32_t *count);

/**
 * @brief 更新风险评估模型
 */
int32_t decision_update_model(const uint8_t *model_data, uint32_t len);

#endif /* OFFLINE_DECISION_H */
```

#### 3.4.4 决策流程图

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
           │ 检查请求格式    │──N──► 拒绝(INVALID_PARAM)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 查找钥匙缓存    │──N──► 拒绝(KEY_NOT_FOUND)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 检查钥匙有效性  │──N──► 拒绝(KEY_EXPIRED)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 验证签名        │──N──► 拒绝(SIGNATURE_INVALID)
           └────────┬────────┘
                    │Y
                    ▼
           ┌─────────────────┐
           │ 检查Nonce       │──N──► 拒绝(NONCE_REUSE)
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

### 3.5 车辆集成模块 (vehicle_integration)

#### 3.5.1 模块职责
- CAN总线通信
- 车辆状态监听
- 命令执行与反馈
- 状态同步

#### 3.5.2 接口定义

```c
/**
 * @file vehicle_integration.h
 * @brief 车辆集成模块接口
 */

#ifndef VEHICLE_INTEGRATION_H
#define VEHICLE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

/* CAN错误码 */
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
    uint8_t door_status;        // 位图: bit0-3 四门, bit4-5 前后盖
    uint8_t lock_status;        // 锁止状态
    uint8_t engine_status;      // 0:熄火, 1:运行
    uint8_t gear_position;      // 档位
    uint16_t battery_voltage;   // 电池电压 mV
    int16_t temperature;        // 车内温度 0.1°C
    uint32_t odometer;          // 里程 km
    uint8_t alarm_status;       // 报警状态
    uint8_t window_status[4];   // 四窗状态
} vehicle_state_t;

/* 车辆命令 */
typedef struct {
    uint8_t command_type;
    uint8_t target;
    uint8_t parameters[8];
    uint32_t user_id;
    uint32_t timestamp;
    uint8_t hmac[32];
} vehicle_command_t;

/* 命令执行结果 */
typedef struct {
    uint8_t command_type;
    uint8_t result;             // 0:成功, 其他:失败
    uint8_t error_code;
    uint32_t execution_time;
    uint8_t response_data[16];
} command_result_t;

/* CAN消息 */
typedef struct {
    uint32_t id;                // CAN ID
    uint8_t dlc;                // 数据长度
    uint8_t data[8];            // 数据
    uint32_t timestamp;
} can_message_t;

/* 主要API接口 */

/**
 * @brief 初始化车辆集成模块
 */
vehicle_result_t vehicle_init(void);

/**
 * @brief 执行车辆命令
 */
vehicle_result_t vehicle_execute_command(const vehicle_command_t *cmd,
                                         command_result_t *result);

/**
 * @brief 获取车辆状态
 */
vehicle_result_t vehicle_get_state(vehicle_state_t *state);

/**
 * @brief 监听车辆状态变化
 */
vehicle_result_t vehicle_register_state_callback(
    void (*callback)(const vehicle_state_t *state));

/**
 * @brief 发送CAN消息
 */
vehicle_result_t vehicle_send_can(const can_message_t *msg);

/**
 * @brief 接收CAN消息
 */
vehicle_result_t vehicle_receive_can(can_message_t *msg, uint32_t timeout_ms);

/**
 * @brief 启动车辆状态监听
 */
vehicle_result_t vehicle_start_monitoring(void);

/**
 * @brief 停止车辆状态监听
 */
vehicle_result_t vehicle_stop_monitoring(void);

#endif /* VEHICLE_INTEGRATION_H */
```

---

### 3.6 异常处理模块 (exception_handler)

#### 3.6.1 模块职责
- 异常检测与分类
- 异常日志记录
- 故障恢复
- 安全告警

#### 3.6.2 接口定义

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
    EXCEPTION_AUTH_FAILED = 0x01,
    EXCEPTION_SIGNATURE_INVALID = 0x02,
    EXCEPTION_NONCE_REUSE = 0x03,
    EXCEPTION_KEY_EXPIRED = 0x04,
    EXCEPTION_PERMISSION_DENIED = 0x05,
    EXCEPTION_COMM_TIMEOUT = 0x10,
    EXCEPTION_CONNECTION_LOST = 0x11,
    EXCEPTION_CAN_ERROR = 0x12,
    EXCEPTION_STORAGE_ERROR = 0x20,
    EXCEPTION_CACHE_CORRUPTED = 0x21,
    EXCEPTION_SYSTEM_FAULT = 0x30,
    EXCEPTION_TAMPER_DETECTED = 0xF0,
    EXCEPTION_SECURITY_BREACH = 0xF1
} exception_type_t;

/* 异常上下文 */
typedef struct {
    exception_type_t type;
    exception_level_t level;
    uint32_t timestamp;
    uint16_t conn_handle;
    uint32_t user_id;
    uint8_t context[64];
    uint8_t stack_trace[128];
} exception_context_t;

/* 恢复动作 */
typedef enum {
    RECOVERY_NONE = 0,
    RECOVERY_RETRY,
    RECOVERY_RESTART,
    RECOVERY_FALLBACK,
    RECOVERY_SAFE_MODE,
    RECOVERY_SHUTDOWN
} recovery_action_t;

/* 异常处理结果 */
typedef struct {
    uint32_t exception_id;
    recovery_action_t action;
    uint8_t action_params[16];
    uint32_t next_retry_time;
    uint8_t notify_cloud;
} exception_result_t;

/* 主要API接口 */

/**
 * @brief 初始化异常处理模块
 */
int32_t exception_init(void);

/**
 * @brief 报告异常
 */
int32_t exception_report(const exception_context_t *context,
                         exception_result_t *result);

/**
 * @brief 处理异常
 */
int32_t exception_handle(uint32_t exception_id);

/**
 * @brief 获取异常历史
 */
int32_t exception_get_history(exception_context_t *exceptions,
                              uint32_t *count);

/**
 * @brief 清除异常记录
 */
int32_t exception_clear(uint32_t exception_id);

/**
 * @brief 注册异常回调
 */
int32_t exception_register_callback(
    void (*callback)(const exception_context_t *context));

/**
 * @brief 执行恢复动作
 */
int32_t exception_execute_recovery(recovery_action_t action,
                                   const uint8_t *params);

/**
 * @brief 获取系统健康状态
 */
int32_t exception_get_health_status(uint8_t *status);

#endif /* EXCEPTION_HANDLER_H */
```

---

## 4. 模块交互序列图

### 4.1 完整解锁流程

```
手机端          BLE模块        认证模块        决策模块        车辆模块        缓存
  │               │              │              │              │              │
  │──扫描广播──►  │              │              │              │              │
  │               │◄──广播响应── │              │              │              │
  │──连接请求──►  │              │              │              │              │
  │               │──建立连接──► │              │              │              │
  │               │              │              │              │              │
  │──认证请求──►  │              │              │              │              │
  │               │──生成挑战──► │              │              │              │
  │◄──挑战值──── │◄─挑战值────  │              │              │              │
  │               │              │              │              │              │
  │──签名响应──►  │              │              │              │              │
  │               │──验证响应──► │              │              │              │
  │               │              │──查询钥匙──► │              │              │
  │               │              │              │◄──钥匙数据── │              │
  │               │              │              │              │◄──缓存查询── │
  │               │              │◄─验证结果──  │              │              │
  │               │              │              │              │              │
  │               │              │──评估决策──► │              │              │
  │               │              │              │──风险计算──► │              │
  │               │              │              │◄─决策结果──  │              │
  │               │              │◄─允许/拒绝─ │              │              │
  │               │              │              │              │              │
  │               │              │──执行命令──────────────────►│              │
  │               │              │              │              │──CAN发送──►  │
  │               │              │              │              │◄─执行结果──  │
  │◄──解锁结果── │◄──────────────┴──────────────┴──────────────│              │
  │               │              │              │              │              │
```

---

## 5. 配置参数

### 5.1 系统配置表

| 参数 | 默认值 | 范围 | 描述 |
|------|--------|------|------|
| MAX_CONNECTIONS | 4 | 1-8 | 最大并发连接数 |
| AUTH_TIMEOUT_MS | 5000 | 1000-30000 | 认证超时 |
| CACHE_MAX_SIZE | 1048576 | 65536-4194304 | 缓存最大字节数 |
| CACHE_DEFAULT_TTL | 86400 | 3600-604800 | 默认缓存TTL(秒) |
| DECISION_TIMEOUT_MS | 500 | 100-5000 | 决策超时 |
| MAX_RETRY_COUNT | 3 | 1-10 | 最大重试次数 |
| OFFLINE_VALID_HOURS | 24 | 1-168 | 离线有效期(小时) |

---

## 6. 附录

### 6.1 错误码汇总表

| 模块 | 错误码范围 | 描述 |
|------|------------|------|
| BLE | 0x00-0x0F | BLE通信错误 |
| Security | 0x10-0x1F | 安全认证错误 |
| Cache | 0x20-0x2F | 缓存管理错误 |
| Decision | 0x30-0x3F | 决策引擎错误 |
| Vehicle | 0x40-0x4F | 车辆集成错误 |
| Exception | 0x50-0x5F | 异常处理错误 |
| System | 0xF0-0xFF | 系统级错误 |

---
**文档结束**
