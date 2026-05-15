# Digital Key Unified Protocol Interface

统一接口层，整合 CCC / ICCOA / ICCE 三套数字钥匙协议。

## 设计目标

1. **设备类型驱动协议选择** - 通过 `dk_device_type_t` 指定协议和能力
2. **统一API** - 一套接口，内部路由到具体协议实现
3. **能力配置动态决定功能集** - 通过 `capabilities` 位掩码控制
4. **状态机统一管理** - 连接、认证、定位状态统一维护

## 架构

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│                  (dk_unified.h API)                     │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                  Unified Protocol Layer                  │
│  - 设备类型解析                                          │
│  - 能力检查                                              │
│  - 状态转换                                              │
│  - 协议路由                                              │
└────────┬───────────┬───────────┬───────────────────────┘
         │           │           │
    ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
    │   CCC   │ │  ICCOA  │ │  ICCE   │
    │Protocol │ │Protocol │ │Protocol │
    └─────────┘ └─────────┘ └─────────┘
```

## 核心数据结构

### 设备类型 (`dk_device_type_t`)

```c
typedef struct {
    /* 基本信息 */
    uint8_t           device_id[16];        /* 设备唯一标识 */
    dk_device_type_e  device_type;          /* 设备类型 */
    dk_protocol_e     protocol;             /* 协议类型 */
    uint16_t          protocol_version;     /* 协议版本 */
    
    /* 能力配置 */
    dk_capabilities_t capabilities;
    
    /* 硬件信息 */
    uint8_t           ble_mac[6];
    uint8_t           uwb_id[8];
    uint8_t           nfc_uid[10];
    uint8_t           se_id[16];
    
    /* 厂商扩展 */
    uint8_t           vendor_id[4];
    uint8_t           product_id[4];
    uint8_t           firmware_ver[4];
} dk_device_type_t;
```

### 能力配置 (`dk_capabilities_t`)

```c
#define DK_CAP_NFC             (1 << 0)   /* 支持 NFC */
#define DK_CAP_BLE             (1 << 1)   /* 支持 BLE */
#define DK_CAP_UWB             (1 << 2)   /* 支持 UWB */
#define DK_CAP_UWB_ANGULAR     (1 << 3)   /* UWB 到达角 */
#define DK_CAP_SE              (1 << 4)   /* 安全元件 */
#define DK_CAP_LPCD            (1 << 5)   /* NFC 低功耗检测 */
#define DK_CAP_EDGE_COMPUTE    (1 << 8)   /* 边缘计算 */
#define DK_CAP_KEY_SHARE       (1 << 10)  /* 钥匙分享 */
```

### 综合状态 (`dk_device_status_t`)

```c
typedef struct {
    dk_device_type_t   device;        /* 设备类型 */
    dk_conn_status_t   conn;          /* 连接状态 */
    dk_auth_status_t   auth;          /* 认证状态 */
    dk_location_t      location;      /* 定位数据 */
    uint32_t           timestamp;     /* 状态时间戳 */
    uint32_t           flags;         /* 状态标志位 */
} dk_device_status_t;
```

## 使用示例

### 初始化

```c
#include "dk_unified.h"

// 配置设备类型
dk_device_type_t device = {
    .device_type = DK_DEVICE_VEHICLE_TCU,
    .protocol = DK_PROTOCOL_CCC,
    .protocol_version = DK_VERSION_CCC_30,
    .capabilities = {
        .capabilities = DK_CAP_NFC | DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE | DK_CAP_LPCD,
        .max_keys = 8,
        .max_sessions = 4,
        .ble_mtu = 244,
        .uwb_max_range_cm = 1000
    }
};

// 初始化统一协议层
dk_status_t ret = dk_init(&device);
if (ret != DK_OK) {
    // 处理错误
}
```

### 连接与认证

```c
// 开始 NFC 监听
dk_nfc_start_listen();

// 开始 BLE 广播
dk_ble_start_adv(DK_PROTOCOL_CCC);

// 配对绑定
dk_auth_bind(DK_PROTOCOL_CCC);

// 认证验证
dk_auth_verify();

// 检查权限
if (dk_auth_check_permission(DK_ACCESS_UNLOCK)) {
    // 允许解锁
}
```

### 定位与区域

```c
// 开始 UWB 测距
uint32_t session_id = 0;
dk_uwb_start_ranging(&session_id);

// 获取位置
dk_location_t loc;
dk_location_get(&loc);

printf("Distance: %d mm, Zone: %d\n", loc.distance_mm, loc.zone);

// 设置区域阈值
dk_zone_set_threshold(1000, 500, 200, 50);  // approach, unlock, entry, inside
```

### 车辆控制

```c
// 解锁
dk_vehicle_ctrl(DK_CTRL_UNLOCK, 0);

// 启动引擎
dk_vehicle_ctrl(DK_CTRL_ENGINE_START, 0);

// 开启空调
dk_vehicle_ctrl(DK_CTRL_CLIMATE_ON, 22);  // 22度

// 获取车辆状态
dk_vehicle_status_t status;
dk_vehicle_get_status(&status);
```

### 主循环

```c
// 在主循环中调用
while (running) {
    dk_run();  // 非阻塞 tick
    usleep(10000);  // 10ms
}
```

## API 分类

| 分类 | 功能 | API |
|------|------|-----|
| **初始化** | 初始化/反初始化/状态/运行 | `dk_init`, `dk_deinit`, `dk_get_status`, `dk_run` |
| **连接** | NFC/BLE/UWB 连接管理 | `dk_nfc_*`, `dk_ble_*`, `dk_uwb_*` |
| **认证** | 绑定/解绑/验证/权限检查 | `dk_auth_*` |
| **钥匙** | 创建/删除/查询/分享/撤销 | `dk_key_*` |
| **定位** | 位置获取/区域设置 | `dk_location_*`, `dk_zone_*` |
| **车控** | 车辆控制/状态查询 | `dk_vehicle_*` |
| **扩展** | 原始数据/协议信息 | `dk_protocol_*` |

## 协议映射

| 统一 API | CCC 3.0 | ICCOA DK4.0 | ICCE 1.0 |
|----------|---------|-------------|----------|
| `dk_nfc_*` | ✅ | ❌ | ❌ |
| `dk_ble_*` | ✅ | ✅ | ✅ |
| `dk_uwb_*` | ✅ | ⚠️ | ✅ |
| `dk_auth_*` | SE050 | BLE | BLE |
| `dk_key_*` | 本地 | 云端 | 云端 |
| `dk_location_*` | UWB | ❌ | UWB+AoA |
| `dk_vehicle_*` | BLE | BLE | 边缘计算 |

## 编译

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## 目录结构

```
unified_protocol/
├── include/
│   └── dk_unified.h       # 统一接口定义
├── src/
│   └── dk_unified.c       # 协议路由实现
├── examples/
│   └── example.c          # 使用示例
├── CMakeLists.txt
└── README.md
```

## 扩展指南

### 添加新协议

1. 在 `dk_protocol_e` 中添加协议类型
2. 在 `dk_unified.c` 的 switch 语句中添加路由逻辑
3. 实现协议适配函数

### 添加新能力

1. 在 `dk_capabilities_t` 中定义能力位
2. 在 `protocol_supports_capability()` 中添加检查逻辑
3. 在对应 API 中添加能力检查

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-05-08 | 初始版本，支持 CCC/ICCOA/ICCE |
