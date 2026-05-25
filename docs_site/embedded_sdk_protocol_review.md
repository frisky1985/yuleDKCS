# Embedded SDK 通信协议审查报告

## 概述
**项目名称**: yuleDKCS Digital Key Connectivity Stack  
**审查范围**: embedded/sdk/ 目录下的通信接口  
**审查日期**: 2026-05-09  
**版本**: 1.0.0

---

## 1. 整体架构

### 1.1 通信协议栈层次
```
┌─────────────────────────────────────────────────────────────────────┐
│                        应用层 (Application Layer)                      │
│         CCC │ ICCE │ ICCOA │ 车辆控制 │ 钥匙管理 │ OTA              │
├─────────────────────────────────────────────────────────────────────┤
│                        协议层 (Protocol Layer)                         │
│         ccc_core.c │ icce_core.c │ iccoa_core.c │ dkcs.c            │
├─────────────────────────────────────────────────────────────────────┤
│                        安全层 (Security Layer)                         │
│         ECC HAL │ SE050 │ SCP03 │ SE 接口抽象                         │
├─────────────────────────────────────────────────────────────────────┤
│                        传输层 (Transport Layer)                        │
│         BLE │ NFC │ UWB │ HAL 抽象                                  │
├─────────────────────────────────────────────────────────────────────┤
│                        存储层 (Storage Layer)                          │
│         NVM Key Storage │ Flash │ EEPROM                            │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 支持的协议
| 协议 | 版本 | 连接方式 | 地区支持 |
|------|------|----------|----------|
| CCC Digital Key | R3 2.0.0r2 | BLE/UWB/NFC | 全球 |
| ICCE | 2.0 | BLE/NFC | 中国 |
| ICCOA | 1.2 | BLE/NFC | 中国 |

---

## 2. CCC Digital Key R3 协议实现

### 2.1 文件位置
- `/home/admin/yuleDKCS/include/ccc.h` - CCC 协议头文件
- `/home/admin/yuleDKCS/include/ccc_uwb.h` - UWB 测距扩展
- `/home/admin/yuleDKCS/src/ccc/protocol/ccc_core.c` - 核心协议实现
- `/home/admin/yuleDKCS/src/ccc/protocol/ccc_uwb.c` - UWB 测距实现

### 2.2 核心功能

#### 2.2.1 配对流程 (Pairing Flow)
```c
// 配对会话状态机
CCC_PAIRING_STATE_IDLE              // 空闲
CCC_PAIRING_STATE_WAITING_DEVICE    // 等待设备
CCC_PAIRING_STATE_DEVICE_VERIFIED   // 设备已验证
CCC_PAIRING_STATE_WAITING_VEHICLE   // 等待车辆
CCC_PAIRING_STATE_COMPLETE          // 完成
CCC_PAIRING_STATE_FAILED            // 失败
```

**配对消息格式**:
```
配对请求: [Version(1)][EphemeralPubKey(65)][Challenge(32)][DeviceCertLen(2)][DeviceCert(n)]
配对响应: [Version(1)][VehiclePubKey(65)][VehicleChallenge(32)][Signature(64)]
```

#### 2.2.2 会话建立 (Session Establishment)
- 基于 ECDH 密钥交换 (P-256)
- HKDF 密钥派生
- AES-128-GCM 加密
- 会话密钥派生:
  - Master Secret (48 bytes)
  - Session Encryption Key (16 bytes)
  - Session MAC Key (16 bytes)

#### 2.2.3 车辆控制命令
```c
CCC_CMD_VEHICLE_UNLOCK      = 0x11  // 解锁
CCC_CMD_VEHICLE_LOCK        = 0x12  // 上锁
CCC_CMD_ENGINE_START        = 0x13  // 启动
CCC_CMD_ENGINE_STOP         = 0x14  // 熄火
CCC_CMD_TRUNK_UNLOCK        = 0x15  // 后备箱解锁
CCC_CMD_CLIMATE_ON          = 0x17  // 空调开启
CCC_CMD_CHARGE_PORT_OPEN    = 0x19  // 充电口开启
```

### 2.3 UWB 测距协议 (CCC R3 UWB)

#### 2.3.1 配置支持
| 配置ID | 描述 |
|--------|------|
| 0x01 | 基础测距 |
| 0x02 | 高速测距 |
| 0x03 | 高精度测距 |
| 0x04 | 长距离测距 |
| 0x05 | 安全测距 (默认) |
| 0x06 | 安全 + AoA |

#### 2.3.2 STS (Scrambled Timestamp Sequence) 安全测距
```c
typedef struct {
    uint8_t sts_key[16];           // 128-bit STS 密钥
    uint8_t sts_iv[8];             // 64-bit STS IV
    uint8_t sts_index;             // STS 索引
    bool dynamic_sts_enabled;      // 动态 STS
    uint32_t dynamic_sts_interval_ms; // 更新间隔
} ccc_uwb_sts_config_t;
```

#### 2.3.3 UWB 芯片支持
- Qorvo DW3000/DW3720
- NXP NCJ29D5/SR040

---

## 3. ICCE 协议实现

### 3.1 文件位置
- `/home/admin/yuleDKCS/include/icce.h`
- `/home/admin/yuleDKCS/src/icce/protocol/icce_core.c`

### 3.2 核心特性
- **国密算法支持**: SM2/SM4
- **APDU 命令格式**: 标准智能卡 APDU
- **安全元件**: 硬件 SE/SIM/eSE/TEE

### 3.3 APDU 命令结构
```c
typedef struct {
    uint8_t cla;                    // 命令类别 (0x00)
    uint8_t ins;                    // 指令码
    uint8_t p1;                     // 参数1
    uint8_t p2;                     // 参数2
    uint8_t lc;                     // 数据长度
    uint8_t data[255];              // 数据字段
    uint8_t le;                     // 期望响应长度
} icce_apdu_t;
```

### 3.4 配对模式
```c
ICCE_PAIRING_MODE_STANDARD      // 标准配对
ICCE_PAIRING_MODE_FAST          // 快速配对
ICCE_PAIRING_MODE_OFFLINE       // 离线配对
```

---

## 4. ICCOA 协议实现

### 4.1 文件位置
- `/home/admin/yuleDKCS/include/iccoa.h`
- `/home/admin/yuleDKCS/src/iccoa/protocol/iccoa_core.c`

### 4.2 核心特性
- **TLV 消息格式**: 类型-长度-值结构
- **手机厂商兼容**: 小米/OPPO/vivo/一加
- **连接方式**: BLE/NFC 双模

### 4.3 消息格式
```c
typedef struct __attribute__((packed)) {
    uint8_t magic;                  // 魔数 0xA5
    uint8_t version;                // 协议版本
    uint16_t length;                // 整个消息长度
    uint8_t cmd;                    // 命令码
    uint8_t flags;                  // 标志位
    uint16_t sequence;              // 序列号
} iccoa_message_header_t;
```

### 4.4 标志位定义
```c
#define ICCOA_FLAG_ENCRYPTED        (1 << 0)  // 加密
#define ICCOA_FLAG_FRAGMENTED       (1 << 1)  // 分片
#define ICCOA_FLAG_LAST_FRAGMENT    (1 << 2)  // 最后分片
#define ICCOA_FLAG_RESPONSE         (1 << 3)  // 响应消息
#define ICCOA_FLAG_ERROR            (1 << 4)  // 错误消息
```

---

## 5. BLE/NFC/UWB 传输层接口

### 5.1 BLE 接口 (ccc.h)
```c
#ifdef CCC_ENABLE_BLE
    error_t ccc_ble_init(void);
    error_t ccc_ble_start_advertising(const uint8_t *vin);
    error_t ccc_ble_connect(const uint8_t *vehicle_mac);
    error_t ccc_ble_send_data(const uint8_t *data, size_t len);
    error_t ccc_ble_receive_data(uint8_t *data, size_t *len, uint32_t timeout_ms);
#endif
```

### 5.2 NFC 接口 (ccc.h)
```c
#ifdef CCC_ENABLE_NFC
    typedef enum {
        CCC_NFC_TYPE_POLLER = 0,    // 轮询模式
        CCC_NFC_TYPE_LISTENER = 1,  // 监听模式
    } ccc_nfc_mode_t;
    
    error_t ccc_nfc_init(ccc_nfc_mode_t mode);
    error_t ccc_nfc_start_emulation(const uint8_t *aid, size_t aid_len);
    error_t ccc_nfc_transceive(const uint8_t *apdu, size_t apdu_len,
                                   uint8_t *response, size_t *response_len);
#endif
```

### 5.3 UWB HAL 接口 (uwb_hal.h)
```c
// UWB 芯片驱动 VTable
typedef struct {
    int (*init)(struct uwb_chip_driver *driver, void *spi_handle, void *gpio_handle);
    int (*configure)(struct uwb_chip_driver *driver, const uwb_hal_config_t *config);
    int (*start_ranging)(struct uwb_chip_driver *driver);
    int (*stop_ranging)(struct uwb_chip_driver *driver);
    int (*get_measurement)(struct uwb_chip_driver *driver, uwb_hal_measurement_t *result);
    int (*enable_irq)(struct uwb_chip_driver *driver, bool enable);
    int (*sleep)(struct uwb_chip_driver *driver);
    int (*wakeup)(struct uwb_chip_driver *driver);
} uwb_driver_ops_t;
```

---

## 6. Backend 通信协议

### 6.1 通信方式
Embedded SDK 通过 **Mobile SDK** 与 Backend 通信:
```
[Embedded SDK] <-- FFI/JNI --> [Mobile SDK (Android/iOS)] <-- HTTPS --> [Backend API]
```

### 6.2 Backend API 接口 (Android)
位置: `/home/admin/yuleDKCS/mobile/android/src/main/java/com/yuledkcs/api/YuleDKCSApi.kt`

```kotlin
interface YuleDKCSApi {
    @POST("keys/issue")
    suspend fun issueKey(@Header("Authorization") apiKey: String, 
                         @Body request: IssueKeyRequest): Response<DigitalKey>
    
    @GET("keys")
    suspend fun listKeys(@Header("Authorization") apiKey: String): Response<List<DigitalKey>>
    
    @POST("keys/{keyId}/share")
    suspend fun shareKey(@Header("Authorization") apiKey: String,
                         @Path("keyId") keyId: String,
                         @Body request: ShareKeyRequest): Response<Unit>
    
    @DELETE("keys/{keyId}")
    suspend fun revokeKey(@Header("Authorization") apiKey: String,
                          @Path("keyId") keyId: String): Response<Unit>
    
    @GET("vehicles/{vehicleId}/status")
    suspend fun getVehicleStatus(@Header("Authorization") apiKey: String,
                                 @Path("vehicleId") vehicleId: String): Response<VehicleStatusResponse>
}
```

### 6.3 与 Backend 的交互场景
1. **钥匙签发**: 通过 Backend API 获取钥匙数据
2. **钥匙分享**: 通过 Backend 分享钥匙给其他用户
3. **钥匙吊销**: 通过 Backend 撤销钥匙权限
4. **车辆状态**: 通过 Backend 查询车辆在线状态

---

## 7. 安全层接口

### 7.1 CCC SE 接口
```c
typedef struct {
    error_t (*init)(void);
    error_t (*generate_key_pair)(uint8_t *pub_key, uint8_t *priv_key);
    error_t (*sign)(const uint8_t *data, size_t len, const uint8_t *priv_key, uint8_t *signature);
    error_t (*verify)(const uint8_t *data, size_t len, const uint8_t *pub_key, const uint8_t *signature);
    error_t (*derive_shared_secret)(const uint8_t *priv_key, const uint8_t *pub_key, uint8_t *shared_secret);
    error_t (*secure_store)(uint32_t key_id, const uint8_t *data, size_t len);
    error_t (*secure_read)(uint32_t key_id, uint8_t *data, size_t *len);
} ccc_se_interface_t;
```

### 7.2 ICCE SE 接口
```c
typedef struct {
    error_t (*open_channel)(uint8_t *atr, size_t *atr_len);
    error_t (*transmit)(const uint8_t *apdu, size_t apdu_len, uint8_t *response, size_t *response_len);
    error_t (*generate_sm2_key)(uint8_t *pub_key, uint8_t *priv_key);
    error_t (*sm2_sign)(const uint8_t *data, size_t len, uint8_t *signature);
    error_t (*sm2_verify)(const uint8_t *data, size_t len, const uint8_t *pub_key, const uint8_t *signature);
    error_t (*sm4_encrypt)(const uint8_t *key, const uint8_t *iv, const uint8_t *plaintext, size_t len, uint8_t *ciphertext);
    error_t (*sm4_decrypt)(const uint8_t *key, const uint8_t *iv, const uint8_t *ciphertext, size_t len, uint8_t *plaintext);
} icce_se_interface_t;
```

### 7.3 支持的 SE 类型
| SE 类型 | 描述 | 适用协议 |
|---------|------|----------|
| SE_ESE | Embedded SE | CCC/ICCE/ICCOA |
| SE_SIM | SIM-based SE | CCC/ICCE |
| SE_TEE | Trusted Execution Environment | 所有 |
| SE_SOFTWARE | 软实现 (仅测试) | 所有 |

---

## 8. 存储层接口

### 8.1 NVM Key Storage
位置: `/home/admin/yuleDKCS/include/nvm_key_storage.h`

#### 8.1.1 安全特性
- AES-256-GCM 加密
- HKDF 密钥派生
- CRC32 + HMAC-SHA256 完整性校验
- 双份存储防回滚
- 序列号防重放

#### 8.1.2 钥匙记录结构
```c
typedef struct {
    uint32_t magic;                 // NVM_MAGIC_NUMBER
    uint8_t key_id[16];             // 钥匙唯一ID
    protocol_type_t protocol;       // 协议类型
    uint32_t created_time;          // 创建时间戳
    uint32_t expire_time;           // 过期时间戳
    uint16_t permissions;           // 权限位图
    uint8_t salt[32];               // HKDF Salt
    uint8_t iv[12];                 // AES-GCM IV
    uint8_t tag[16];                // AES-GCM Tag
    uint16_t encrypted_data_len;    // 加密数据长度
    uint8_t encrypted_data[2048];   // 加密钥匙材料
    uint32_t crc32;                 // CRC32 校验
    uint8_t signature[32];          // HMAC-SHA256
} nvm_key_record_t;
```

#### 8.1.3 HAL 接口
```c
typedef struct {
    int32_t (*read)(uint32_t addr, uint8_t *buffer, uint32_t len);
    int32_t (*write)(uint32_t addr, const uint8_t *buffer, uint32_t len);
    int32_t (*erase)(uint32_t addr, uint32_t len);
} nvm_hal_interface_t;
```

---

## 9. 统一接口层 (DKCS)

### 9.1 主头文件
位置: `/home/admin/yuleDKCS/include/dkcs.h`

### 9.2 统一 API
```c
// 初始化和配置
error_t dkcs_init(const session_config_t *config);
error_t dkcs_deinit(void);
const char* dkcs_get_version(void);

// 配对和认证
error_t dkcs_pairing_start(protocol_type_t protocol, const uint8_t *vin,
                           dkcs_auth_complete_callback_t callback, void *user_ctx);

// 车辆控制
error_t dkcs_vehicle_unlock(const uint8_t *key_id, const uint8_t *vin);
error_t dkcs_vehicle_lock(const uint8_t *key_id, const uint8_t *vin);
error_t dkcs_engine_start(const uint8_t *key_id, const uint8_t *vin);

// 钥匙管理
error_t dkcs_key_share(const uint8_t *key_id, key_role_t target_role,
                       uint16_t permissions, uint32_t valid_duration_sec);
error_t dkcs_key_revoke(const uint8_t *key_id);
error_t dkcs_key_list(key_info_t *keys, size_t *key_count);
error_t dkcs_key_import(const uint8_t *key_data, size_t key_len, key_info_t *key_info);
error_t dkcs_key_export(const uint8_t *key_id, uint8_t *key_data, size_t *key_len);
```

---

## 10. 发现的问题

### 10.1 关键问题

#### 问题1: CCC 配对请求消息格式不完整
**位置**: `/home/admin/yuleDKCS/src/ccc/protocol/ccc_core.c:172-174`
```c
/* 证书 (M1: 简化版本) */
request_data[offset++] = 0x00;
request_data[offset++] = 0x00;
```
**描述**: 证书序列化未完整实现，只是占位。
**建议**: 实现完整的 X.509 证书编码。

#### 问题2: AES-GCM 加密未实现
**位置**: `/home/admin/yuleDKCS/src/ccc/protocol/ccc_core.c:398-403`
```c
/* TODO: AES-128-GCM 加密载荷 */
memcpy(&encrypted_data[offset], payload, payload_len);
offset += payload_len;

/* MAC/Tag (占位) */
memset(&encrypted_data[offset], 0, 16);
```
**描述**: 消息加密仅使用 memcpy，未实现 AES-GCM。
**建议**: 集成 mbedtls 或其他加密库实现 AES-GCM。

#### 问题3: UWB 测距结果为模拟值
**位置**: `/home/admin/yuleDKCS/src/ccc/protocol/ccc_uwb.c:42-44`
```c
/* 模拟测距结果 */
*distance_m = 2.5f;
*accuracy_m = 0.1f;
```
**描述**: UWB 测距结果固定为模拟值。
**建议**: 接入真实 UWB 芯片驱动。

#### 问题4: SE 接口未实现
**位置**: `/home/admin/yuleDKCS/src/common/dkcs.c:408-414`
```c
/* 创建简化的 SE 接口 */
ccc_se_interface_t ccc_se = {0};
/* 设置基本回调函数 */
ret = ccc_init(&ccc_se);
```
**描述**: SE 接口结构体未填充实际函数指针。
**建议**: 实现具体的 SE 适配器 (SE050/SCP03)。

### 10.2 设计建议

1. **错误处理**: 增加更详细的错误码和日志
2. **状态机保护**: 加强会话状态机的并发保护
3. **超时处理**: 完善网络/通信超时机制
4. **内存管理**: 统一内存分配/释放接口
5. **文档**: 补充更多 API 使用示例

---

## 11. 总结

### 11.1 协议实现完整性

| 协议/模块 | 完整性 | 状态 |
|-----------|--------|------|
| CCC R3 核心协议 | 70% | 框架完成，部分功能待实现 |
| CCC UWB 测距 | 40% | 头文件完整，实现为模拟 |
| ICCE 协议 | 60% | APDU 框架完成 |
| ICCOA 协议 | 60% | TLV 框架完成 |
| BLE 传输 | 50% | 接口定义完成 |
| NFC 传输 | 50% | 接口定义完成 |
| UWB HAL | 70% | 驱动抽象层完成 |
| 安全 SE 接口 | 50% | 接口定义完成 |
| NVM 存储 | 70% | 结构定义完整，实现简化 |

### 11.2 优点
1. **协议支持全面**: 同时支持 CCC/ICCE/ICCOA 三大标准
2. **架构清晰**: 分层设计，HAL 抽象良好
3. **安全设计**: 完整的密钥派生和加密体系
4. **扩展性强**: 统一的协议接口便于添加新协议

### 11.3 待完成工作
1. 实现完整的加密/解密功能 (AES-GCM, SM4)
2. 接入真实的 UWB 芯片驱动
3. 实现 SE 适配器 (SE050/SCP03/TEE)
4. 完善 BLE/NFC 传输层实现
5. 完整的错误处理和边界条件检查
6. 单元测试和集成测试

---

## 附录

### A. 文件清单

| 文件路径 | 描述 |
|----------|------|
| `/home/admin/yuleDKCS/include/dkcs.h` | 统一接口头文件 |
| `/home/admin/yuleDKCS/include/ccc.h` | CCC 协议头文件 |
| `/home/admin/yuleDKCS/include/ccc_uwb.h` | CCC UWB 扩展头 |
| `/home/admin/yuleDKCS/include/icce.h` | ICCE 协议头文件 |
| `/home/admin/yuleDKCS/include/iccoa.h` | ICCOA 协议头文件 |
| `/home/admin/yuleDKCS/include/uwb_hal.h` | UWB HAL 头文件 |
| `/home/admin/yuleDKCS/include/nvm_key_storage.h` | NVM 存储头文件 |
| `/home/admin/yuleDKCS/src/ccc/protocol/ccc_core.c` | CCC 核心实现 |
| `/home/admin/yuleDKCS/src/ccc/protocol/ccc_uwb.c` | CCC UWB 实现 |
| `/home/admin/yuleDKCS/src/common/dkcs.c` | DKCS 统一实现 |
| `/home/admin/yuleDKCS/src/hal/uwb_driver.c` | UWB 驱动实现 |
| `/home/admin/yuleDKCS/src/storage/nvm_key_storage.c` | NVM 存储实现 |

### B. 参考规范
- CCC Digital Key Release 3 Specification 2.0.0r2
- IEEE 802.15.4z HRP UWB
- ICCE-DK-001 数字钥匙技术规范
- ICCOA-DK-001 数字钥匙技术规范
