# ICCOA Digital Key Protocol Stack

## 概述

ICCOA (智慧车联开放联盟) 数字钥匙协议栈实现，支持 DK 3.0 和 DK 4.0 两个版本。

## 特性

### DK 3.0
- BLE 数据通道
- SOP/EOP 帧格式
- XOR 校验
- 配对/认证/控制命令

### DK 4.0 (推荐)
- UWB 安全测距 (FiRa 兼容)
- BLE 数据通道
- Session Token 认证
- HMAC-SHA256 消息完整性
- 多设备并发 (最多 4 个)

## 目录结构

```
iccoa_protocol/
├── include/
│   └── iccoa_digital_key.h    # 主头文件
├── src/
│   ├── auth/
│   │   └── iccoa_auth.c       # 认证模块
│   ├── ble/
│   │   └── iccoa_ble.c        # BLE 服务
│   ├── iccoa/
│   │   ├── iccoa_dk_core.c    # 核心入口
│   │   ├── dk30/
│   │   │   └── iccoa_dk30.c   # DK 3.0 实现
│   │   └── dk40/
│   │       └── iccoa_dk40.c   # DK 4.0 实现
│   └── service/
│       └── iccoa_service.c    # 车辆控制服务
├── docs/
│   └── SPEC.md
├── tests/
├── CMakeLists.txt
└── README.md
```

## API

### 初始化

```c
#include "iccoa_digital_key.h"

int32_t iccoa_dk_init(void);    // 初始化协议栈
int32_t iccoa_dk_run(void);     // 运行主循环
int32_t iccoa_dk_deinit(void);  // 反初始化
```

### BLE 控制

```c
int32_t iccoa_ble_start_adv(void);
int32_t iccoa_ble_stop_adv(void);
int32_t iccoa_ble_send(const uint8_t *data, uint16_t len);
```

### 认证

```c
int32_t iccoa_auth_request(iccoa_auth_type_e type, const uint8_t *challenge, uint16_t len);
int32_t iccoa_auth_verify(const uint8_t *response, uint16_t len);
int32_t iccoa_auth_check_permission(const uint8_t *user_id, uint8_t perm);
```

### 车辆控制

```c
int32_t iccoa_ctrl_execute(iccoa_ctrl_cmd_e cmd, uint8_t param);
int32_t iccoa_service_get_status(iccoa_vehicle_status_t *status);
```

## 协议帧格式

### DK 3.0

```
| SOP(1) | CMD(1) | SEQ(2) | LEN(2) | PAYLOAD(n) | CS(1) | EOP(1) |
```

- SOP: 0xAA
- EOP: 0x55
- CS: XOR 校验

### DK 4.0

```
| MAGIC(2) | VER(1) | TYPE(1) | MSG_ID(2) | FLAGS(2) | LEN(2) |
| TOKEN(4) | PAYLOAD(n) | HMAC(16) |
```

- MAGIC: 0xICC0
- HMAC: SHA-256 截断 16 字节

## 权限定义

```c
#define ICCOA_PERM_LOCK     (1 << 0)  // 上锁
#define ICCOA_PERM_UNLOCK   (1 << 1)  // 解锁
#define ICCOA_PERM_ENGINE   (1 << 2)  // 引擎
#define ICCOA_PERM_TRUNK    (1 << 3)  // 后备箱
#define ICCOA_PERM_WINDOW   (1 << 4)  // 车窗
#define ICCOA_PERM_CLIMATE  (1 << 5)  // 空调
#define ICCOA_PERM_FIND     (1 << 6)  // 寻车
#define ICCOA_PERM_SEAT     (1 << 7)  // 座椅
```

## 控制命令

```c
CTRL_LOCK        = 0x01  // 上锁
CTRL_UNLOCK      = 0x02  // 解锁
CTRL_ENGINE_ON   = 0x03  // 启动引擎
CTRL_ENGINE_OFF  = 0x04  // 停止引擎
CTRL_TRUNK_OPEN  = 0x05  // 打开后备箱
CTRL_WINDOW_UP   = 0x06  // 升窗
CTRL_WINDOW_DOWN = 0x07  // 降窗
CTRL_CLIMATE_ON  = 0x08  // 开空调
CTRL_CLIMATE_OFF = 0x09  // 关空调
CTRL_FIND        = 0x0A  // 寻车
CTRL_HORN        = 0x0B  // 鸣笛
```

## 依赖

- SE050 安全芯片 (密钥管理、签名验证)
- NXP KW47A BLE 芯片
- NXP NCJ29D6 UWB 芯片 (DK 4.0)

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 版本历史

- v1.0 (2026-05-08): 初始实现，支持 DK 3.0/4.0
