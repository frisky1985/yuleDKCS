# 数字钥匙系统架构 - 接口规范

## 接口定义文件

### 1. 公共类型定义

```c
#ifndef DK_TYPES_H
#define DK_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// 通信模式
typedef enum {
    COMM_MODE_NFC = 0,
    COMM_MODE_UWB,
    COMM_MODE_BLE,
    COMM_MODE_IDLE
} CommMode_e;

// 电源状态
typedef enum {
    POWER_STATE_ACTIVE = 0,
    POWER_STATE_IDLE,
    POWER_STATE_SLEEP,
    POWER_STATE_DEEP_SLEEP,
    POWER_STATE_OFF
} PowerState_e;

// 主状态机
typedef enum {
    STATE_INIT = 0,
    STATE_STANDBY,
    STATE_NFC_DETECT,
    STATE_NFC_READ,
    STATE_UWB_RANGING,
    STATE_BLE_CONNECTED,
    STATE_AUTH_PROCESS,
    STATE_UNLOCKED,
    STATE_LOCKED,
    STATE_ERROR
} MainState_e;

// 安全操作
typedef enum {
    SEC_OP_ENCRYPT = 0,
    SEC_OP_DECRYPT,
    SEC_OP_SIGN,
    SEC_OP_VERIFY
} SecOp_e;

// 通信消息
typedef struct {
    uint8_t  msg_id;
    uint8_t  data_len;
    uint8_t  data[256];
} CommMsg_t;

// 密钥结构
typedef struct {
    uint8_t  key_id[16];
    uint8_t  key_data[32];
} SecKey_t;

#endif // DK_TYPES_H
```

### 2. BLE接口 (hal_ble.h)

```c
#ifndef HAL_BLE_H
#define HAL_BLE_H

#include "dk_types.h"

// 广播参数
typedef struct {
    uint8_t  adv_interval;    // 广播间隔 (20ms - 10.24s)
    uint8_t  adv_type;       // 广播类型
    uint8_t  own_addr_type; // 本机地址类型
    uint8_t  peer_addr_type;// 对方地址类型
    uint8_t  peer_addr[6];  // 对方地址
    uint8_t  adv_channel_map;// 广播通道
} BLE_AdvParam_t;

// 连接参数
typedef struct {
    uint16_t conn_interval;   // 连接间隔 (7.5ms - 4s)
    uint16_t slave_latency;  // 从机延迟
    uint16_t supervision_timeout;// 监督超时
} BLE_ConnParam_t;

// BLE初始化
int HAL_BLE_Init(void);

// 广播控制
int HAL_BLE_StartAdv(BLE_AdvParam_t *param);
int HAL_BLE_StopAdv(void);

// 连接管理
int HAL_BLE_Connect(BLE_ConnParam_t *param);
int HAL_BLE_Disconnect(uint16_t conn_handle);

// 数据传输
int HAL_BLE_SendData(uint16_t conn_handle, uint8_t *data, uint32_t len);
int HAL_BLE_RecvData(uint16_t conn_handle, uint8_t *buffer, uint32_t len);

// 回调函数类型
typedef void (*BLE_Callback_t)(uint16_t conn_handle, uint8_t event);

#endif // HAL_BLE_H
```

### 3. UWB接口 (hal_uwb.h)

```c
#ifndef HAL_UWB_H
#define HAL_UWB_H

#include "dk_types.h"

// UWB角色
typedef enum {
    UWB_ROLE_INITIATOR = 0,
    UWB_ROLE_RESPONDER
} UWB_Role_e;

// 测距参数
typedef struct {
    uint8_t  anchor_addr[8];
    uint8_t  slot_duration; // 测距时隙时长
    uint8_t  num_slots;    // 测距槽数量
} UWB_RangingParam_t;

// 测距结果
typedef struct {
    uint8_t  anchor_addr[8];
    uint32_t distance_mm;  // 距离(毫米)
    uint16_t quality;     // 测距质量
    uint32_t timestamp;   // 时间戳
} UWB_RangingResult_t;

// UWB初始化
int HAL_UWB_Init(void);

// 测距控制
int HAL_UWB_SetRole(UWB_Role_e role);
int HAL_UWB_StartRanging(UWB_RangingParam_t *param);
int HAL_UWB_StopRanging(void);
int HAL_UWB_GetDistance(UWB_RangingResult_t *result);

// 回调函数类型
typedef void (*UWB_Callback_t)(UWB_RangingResult_t *result);

#endif // HAL_UWB_H
```

### 4. NFC接口 (hal_nfc.h)

```c
#ifndef HAL_NFC_H
#define HAL_NFC_H

#include "dk_types.h"

// NFC类型
typedef enum {
    NFC_TYPE_TAG = 0,
    NFC_TYPE_P2P,
    NFC_TYPE_RW
} NFC_Type_e;

// NFC标签数据
typedef struct {
    uint8_t  uid[8];         // 标签UID
    uint8_t  ndef[256];       // NDEF数据
    uint16_t ndef_len;       // NDEF数据长度
} NFC_TagData_t;

// NFC初始化
int HAL_NFC_Init(void);

// NFC操作
int HAL_NFC_StartListen(NFC_Type_e type);
int HAL_NFC_StopListen(void);
int HAL_NFC_ReadTag(NFC_TagData_t *tag_data);
int HAL_NFC_WriteTag(uint8_t *data, uint16_t len);
int HAL_NFC_FieldDetect(void);

// 回调函数类型
typedef void (*NFC_Callback_t)(NFC_TagData_t *tag_data);

#endif // HAL_NFC_H
```

### 5. 安全接口 (hal_sec.h)

```c
#ifndef HAL_SEC_H
#define HAL_SEC_H

#include "dk_types.h"

// 密钥类型
typedef enum {
    KEY_TYPE_ROOT = 0,
    KEY_TYPE_MASTER,
    KEY_TYPE_DEVICE,
    KEY_TYPE_SESSION
} KeyType_e;

// 密钥属性
typedef struct {
    KeyType_e type;
    uint8_t  key_id[16];
    uint32_t key_len;
    bool    exported;        // 是否可导出
} SecKeyAttr_t;

// 安全初始化
int HAL_SEC_Init(void);

// 密钥管理
int HAL_SEC_GenerateKey(KeyType_e type, SecKeyAttr_t *attr);
int HAL_SEC_ImportKey(SecKey_t *key, SecKeyAttr_t *attr);
int HAL_SEC_DeleteKey(KeyType_e type);

// 加密操作
int HAL_SEC_Encrypt(uint8_t * plaintext, uint32_t plain_len, 
                  uint8_t * ciphertext, uint32_t * cipher_len,
                  SecKey_t *key);
int HAL_SEC_Decrypt(uint8_t * ciphertext, uint32_t cipher_len,
                    uint8_t * plaintext, uint32_t * plain_len,
                    SecKey_t *key);
int HAL_SEC_Sign(uint8_t *data, uint32_t len, uint8_t *signature, SecKey_t *key);
int HAL_SEC_Verify(uint8_t *data, uint32_t len, uint8_t *signature, SecKey_t *key);

// 密钥派生
int HAL_SEC_DeriveKey(SecKey_t *input_key, uint8_t *info, uint32_t info_len,
                    SecKey_t *output_key);

// 安全启动
int HAL_SEC_SecureBoot(void);

#endif // HAL_SEC_H
```

### 6. 电源管理接口 (hal_pwr.h)

```c
#ifndef HAL_PWR_H
#define HAL_PWR_H

#include "dk_types.h"

// 唤醒源
typedef enum {
    WAKEUP_NFC = 0,
    WAKEUP_BLE,
    WAKEUP_UWB,
    WAKEUP_BUTTON,
    WAKEUP_TIMER,
    WAKEUP_ALL
} WakeupSrc_e;

// 电源配置
typedef struct {
    PowerState_e state;      // 目标状态
    uint32_t    wakeup_timeout; // 唤醒超时时间(ms)
    uint32_t    sleep_timeout;   // 休眠超时时间(ms)
} PowerConfig_t;

// 电源管理初始化
int HAL_PWR_Init(void);

// 电源控制
int HAL_PWR_SetState(PowerState_e state);
PowerState_e HAL_PWR_GetState(void);

// 唤醒管理
int HAL_PWR_EnableWakeup(WakeupSrc_e src);
int HAL_PWR_DisableWakeup(WakeupSrc_e src);
int HAL_PWR_RequestWakeup(uint32_t timeout_ms);

// 回调函数类型
typedef void (*Power_Callback_t)(PowerState_e old_state, PowerState_e new_state);

#endif // HAL_PWR_H
```

### 7. CAN接口 (hal_can.h)

```c
#ifndef HAL_CAN_H
#define HAL_CAN_H

#include "dk_types.h"

// CAN消息
typedef struct {
    uint32_t id;             // CAN ID
    uint8_t  dlc;           // 数据长度
    uint8_t  data[8];       // 数据
    uint32_t timestamp;     // 时间戳
} CAN_Msg_t;

// CAN初始化
int HAL_CAN_Init(uint32_t baudrate);

// CAN操作
int HAL_CAN_Send(CAN_Msg_t *msg);
int HAL_CAN_Recv(CAN_Msg_t *msg);

// 回调函数类型
typedef void (*CAN_Callback_t)(CAN_Msg_t *msg);

#endif // HAL_CAN_H
```

### 8. 通信管理器接口 (comm_mgr.h)

```c
#ifndef COMM_MGR_H
#define COMM_MGR_H

#include "dk_types.h"
#include "hal_ble.h"
#include "hal_uwb.h"
#include "hal_nfc.h"

// 通信管理器初始化
int COMM_MGR_Init(void);

// 通信模式控制
int COMM_MGR_SelectMode(CommMode_e mode);
CommMode_e COMM_MGR_GetCurrentMode(void);

// 消息发送/接收
int COMM_MGR_Send(CommMsg_t *msg);
int COMM_MGR_Recv(CommMsg_t *msg);

// 事件注册
typedef void (*COMM_EventCallback_t)(CommMsg_t *msg);
int COMM_MGR_RegisterCallback(COMM_EventCallback_t callback);

#endif // COMM_MGR_H
```

### 9. 安全管理器接口 (sec_mgr.h)

```c
#ifndef SEC_MGR_H
#define SEC_MGR_H

#include "dk_types.h"
#include "hal_sec.h"

// 鉴权请求
typedef struct {
    uint8_t  device_id[16];
    uint8_t  challenge[32];
    uint32_t timestamp;
} AuthRequest_t;

// 鉴权响应
typedef struct {
    uint8_t  response[64];
    uint8_t  certificate[256];
    uint32_t result;
} AuthResponse_t;

// 安全管理器初始化
int SEC_MGR_Init(void);

// 鉴权流程
int SEC_MGR_RequestAuth(AuthRequest_t *req, AuthResponse_t *resp);
int SEC_MGR_VerifyAuth(AuthRequest_t *req, AuthResponse_t *resp);

// 密钥同步
int SEC_MGR_SyncKeys(void);

#endif // SEC_MGR_H
```

### 10. 服务层接口 (service.h)

```c
#ifndef SERVICE_H
#define SERVICE_H

#include "dk_types.h"
#include "comm_mgr.h"
#include "sec_mgr.h"
#include "hal_pwr.h"

// 钥匙状态
typedef enum {
    KEY_STATE_INACTIVE = 0,
    KEY_STATE_ACTIVE,
    KEY_STATE_LOCKED,
    KEY_STATE_UNLOCKED
} KeyState_e;

// 车辆命令
typedef enum {
    CMD_UNLOCK = 0,
    CMD_LOCK,
    CMD_START,
    CMD_STOP,
    CMD_TRUNK_OPEN,
    CMD_WINDOW_CLOSE
} VehicleCmd_e;

// 服务层初始化
int SERVICE_Init(void);

// 钥匙服务
KeyState_e SERVICE_GetKeyState(void);
int SERVICE_SetKeyState(KeyState_e state);

// 车辆控制
int SERVICE_SendCommand(VehicleCmd_e cmd);

// 钥匙分享
int SERVICE_ShareKey(uint8_t *shared_key, uint32_t timeout);

#endif // SERVICE_H
```