# 嵌入式端开发指南（车端）

> **版本**：v1.0  
> **日期**：2026-04-26  
> **作者**：code_designer  
> **适用范围**：嵌入式团队  
> **协议标准**：ICCE (T/CA 110-2020) / CCC Digital Key 3.0

---

## 目录

1. [技术栈总览](#1-技术栈总览)
2. [开发环境搭建](#2-开发环境搭建)
3. [硬件平台架构](#3-硬件平台架构)
4. [代码架构设计](#4-代码架构设计)
5. [核心模块开发指南](#5-核心模块开发指南)
6. [关键算法实现](#6-关键算法实现)
7. [接口调用示例](#7-接口调用示例)
8. [测试要点](#8-测试要点)
9. [参考规范](#9-参考规范)

---

## 1. 技术栈总览

### 1.1 硬件选型

| 组件 | 型号 | 说明 |
|------|------|------|
| **主控 MCU** | NXP S32G2/G3 | 车规级，内置 HSE 安全引擎 |
| **UWB 芯片** | NXP SR250 或 Qorvo DW3300 | FiRa MAC 支持 |
| **BLE 芯片** | NXP KW38 | BLE 5.0+，车规级 |
| **NFC 芯片** | NXP PN5180 | ISO 14443 A/B |
| **安全芯片** | NXP SE050 或 Infineon SLB9670 | EAL6+，支持 SM2/SM3/SM4 |
| **通信接口** | SPI / I2C / UART / CAN-FD | - |

### 1.2 软件技术栈

| 层级 | 技术选型 | 说明 |
|------|---------|------|
| **RTOS** | FreeRTOS + 安全补丁 | 实时任务调度 |
| **内核** | ARM Cortex-M7 (×3) + Cortex-A53 (×4) | M7: 实时通信；A53: Linux |
| **开发语言** | C / C++ | 主要语言 |
| **编译工具链** | ARM GCC 10.x / IAR EWARM | 交叉编译 |
| **调试器** | Lauterbach TRACE32 / J-Link | 硬件调试 |
| **协议栈** | ICCE T/CA 110-2020 / CCC DK 3.0 | 双协议栈 |
| **安全库** | mbedTLS（国密扩展）/ OpenSSL | TLS/加解密 |
| **构建系统** | CMake + Ninja | 高效增量构建 |
| **测试框架** | Unity / Ceedling | 单元测试 |
| **固件格式** | UUE / SREC / 二进制 | 带签名 |

### 1.3 目录结构

```
digital-key-vehicle/
├── docs/                       # 设计文档
├── firmware/
│   ├── s32g/                   # S32G 主控固件
│   │   ├── bootloader/         # 安全启动引导程序
│   │   ├── kernel/             # FreeRTOS 内核配置
│   │   ├── apps/               # 应用程序
│   │   └── projects/           # S32 Design Studio 项目
│   ├── hal/                    # 硬件抽象层
│   │   ├── uwb/                # UWB 驱动
│   │   ├── ble/                # BLE 驱动
│   │   ├── nfc/                # NFC 驱动
│   │   └── se/                 # 安全芯片驱动
│   ├── protocol/               # 协议栈
│   │   ├── icce/               # ICCE 协议栈
│   │   └── ccc/                # CCC 协议栈
│   ├── security/               # 安全模块
│   │   ├── anti_relay/         # 防中继攻击
│   │   └── secure_boot/        # 安全启动
│   ├── vehicle/                # 车辆控制
│   │   ├── can/                # CAN 总线驱动
│   │   └── control/            # 车辆控制逻辑
│   └── tests/                  # 测试代码
├── tools/                      # 工具脚本
│   ├── build/                  # 构建脚本
│   ├── signing/                # 固件签名工具
│   └── flashing/               # 烧录工具
└── Makefile                    # 顶层 Makefile
```

---

## 2. 开发环境搭建

### 2.1 环境要求

| 项目 | 版本/要求 |
|------|----------|
| 操作系统 | Ubuntu 22.04 LTS 或 Windows 10/11 + WSL2 |
| 内存 | ≥ 16GB |
| 磁盘 | ≥ 100GB SSD |
| GCC ARM Toolchain | 10.3.1 |
| CMake | ≥ 3.20 |
| Python | 3.9+ |

### 2.2 工具链安装（Ubuntu）

```bash
# 1. 安装编译工具链
wget https://developer.arm.com/-/media/Files/downloads/gnu/10.3.rel1/binrel/gcc-arm-10.3.2021.10-x86_64-arm-none-eabi.tar.xz
tar -xf gcc-arm-10.3.2021.10-x86_64-arm-none-eabi.tar.xz -C /opt/
export PATH=$PATH:/opt/gcc-arm-10.3.2021.10-x86_64-arm-none-eabi/bin

# 2. 安装 CMake 和 Ninja
sudo apt update
sudo apt install -y cmake ninja-build python3-pip

# 3. 安装固件签名工具（国密 + ECDSA）
pip3 install cryptography pyasn1  # Python 国密支持

# 4. 克隆代码仓库
git clone git@github.com:digitalkey/firmware.git
cd firmware

# 5. 初始化子模块
git submodule update --init --recursive

# 6. 安装 S32 Design Studio (Linux)
# 下载地址: https://www.nxp.com/design/software/development-tools/s32-design-studio-ide
# 需要 NXP 账号授权
```

### 2.3 构建固件

```bash
# 配置构建（Debug 模式）
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DTOOLCHAIN_PREFIX=/opt/gcc-arm-10.3.2021.10-x86_64-arm-none-eabi

# 编译固件
cmake --build . -j$(nproc)

# 编译输出
# build/firmware/s32g_firmware.bin      # 主固件
# build/firmware/bootloader.bin         # 引导程序
# build/firmware/manifest.bin           # 签名清单
```

### 2.4 烧录与调试

```bash
# 烧录固件（通过 JTAG/SWD）
# 使用 Lauterbach TRACE32 或 J-Link

# 示例：J-Link 烧录
JLinkExe -device S32G2A -if SWD -speed 4000
loadbin build/firmware/bootloader.bin 0x00000000
loadbin build/firmware/s32g_firmware.bin 0x00080000
reset
go

# 或通过 OTA 升级烧录（生产环境）
# POST /vehicles/{vehicleId}/ota/apply
```

### 2.5 IDE 配置（S32 Design Studio）

1. **导入项目**：`File → Import → General → Existing Projects into Workspace`
2. **配置编译器**：`Project → Properties → C/C++ Build → Settings → Tool Settings`
3. **配置调试器**：`Run → Debug Configurations → GDB Hardware Debugging`
4. **添加符号文件**：连接 S32G HSE 固件调试符号

---

## 3. 硬件平台架构

### 3.1 系统架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         S32G2/G3 主控                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │ Cortex-A53  │  │ Cortex-M7×3 │  │        HSE              │ │
│  │  (×4 Core)  │  │  (实时核)   │  │  (Hardware Security)   │ │
│  │             │  │             │  │                         │ │
│  │ Linux OS    │  │ FreeRTOS    │  │ ·加密运算(SM2/AES)      │ │
│  │ ·协议栈     │  │ ·通信管理   │  │ ·安全启动               │ │
│  │ ·OTA        │  │ ·BLE/UWB    │  │ ·密钥存储               │ │
│  │ ·IVI 对接   │  │ ·CAN 控制   │  │ ·防篡改检测             │ │
│  └──────┬──────┘  └──────┬──────┘  └───────────┬─────────────┘ │
│         │                │                      │                │
│         │     IPC (RPMSG / shared memory)      │                │
│         └────────────────┴──────────────────────┘                │
│                              │                                    │
│  ┌───────────────────────────┴─────────────────────────────────┐ │
│  │                    外设接口                                   │ │
│  │  SPI │ I2C │ UART │ CAN-FD │ Ethernet │ GPIO │ DMA          │ │
│  └──────┬──────┬───────┬────────┬──────────┬───────────────────┘ │
└─────────┼──────┼───────┼────────┼──────────┼──────────────────────┘
          │      │       │        │          │
    ┌─────▼─┐ ┌──▼───┐ ┌──▼───┐ ┌─▼────┐  ┌─▼─────┐
    │ UWB   │ │ BLE  │ │ NFC  │ │  SE  │  │ CAN   │
    │ SR250 │ │ KW38 │ │PN5180│ │ SE050│  │ 总线  │
    │       │ │      │ │      │ │      │  │       │
    │SPI x1 │ │UART  │ │I2C x1│ │SPI x1│  │CAN-FD │
    └───────┘ └──────┘ └──────┘ └──────┘  └───────┘
```

### 3.2 内存布局

| 地址范围 | 大小 | 用途 |
|---------|------|------|
| 0x0000_0000 - 0x0007_FFFF | 512 KB | Boot ROM |
| 0x0008_0000 - 0x000F_FFFF | 512 KB | Bootloader (镜像 A) |
| 0x0010_0000 - 0x007F_FFFF | 8 MB | 主固件区 (镜像 A) |
| 0x0080_0000 - 0x00FF_FFFF | 8 MB | 主固件区 (镜像 B) |
| 0x2000_0000 - 0x2003_FFFF | 256 KB | HSE 固件 |
| 0x4000_0000 - 0x5FFF_FFFF | 512 MB | DDR (Linux) |
| 0x6000_0000 - 0x9FFF_FFFF | 1 GB | DDR (RTOS + 应用) |

---

## 4. 代码架构设计

### 4.1 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                    应用层 (Application)                      │
│   VehicleApp │ KeyMgmtApp │ OTAManager │ DiagnosticApp     │
├─────────────────────────────────────────────────────────────┤
│                    协议层 (Protocol)                         │
│   ICCE Protocol │ CCC Protocol │ ProtocolRouter           │
├─────────────────────────────────────────────────────────────┤
│                    安全层 (Security)                        │
│   AntiRelay │ SecureBoot │ CryptoService │ KeyVault        │
├─────────────────────────────────────────────────────────────┤
│                    通信层 (Communication)                   │
│   CommManager │ UWBDriver │ BLEDriver │ NFCDriver │ SE Driver│
├─────────────────────────────────────────────────────────────┤
│                 硬件抽象层 (HAL)                            │
│   GPIO │ SPI │ I2C │ UART │ CAN │ DMA │ Timer │ Interrupt  │
├─────────────────────────────────────────────────────────────┤
│                  板级支持包 (BSP)                          │
│   S32G2 BSP │ Clock Config │ Pin Mux │ Memory Config       │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 核心模块类图（伪代码）

```cpp
// ============== CommManager（通信管理器）====================
class CommManager {
public:
    // 启动所有通信模块
    void startAll();
    // 停止所有通信模块
    void stopAll();
    // 切换通信通道（BLE ↔ UWB ↔ NFC）
    CommChannel switchChannel(CommChannel target);
    // 获取当前活跃通道
    CommChannel getActiveChannel();
    // 注册协议回调
    void registerProtocolCallback(IProtocolHandler* handler);

private:
    UWBDriver* m_uwbDriver;
    BLEDriver* m_bleDriver;
    NFCDriver* m_nfcDriver;
    CommChannel m_activeChannel;
};

// ============== ProtocolRouter（协议路由器）=================
class ProtocolRouter {
public:
    // 协议协商
    NegotiateResult negotiate(DeviceCapabilities* caps);
    // 分发消息到对应协议栈
    void dispatch(Message* msg);
    // 获取当前活跃协议
    Protocol getActiveProtocol();
    // 协议降级
    void fallbackTo(Protocol protocol);

private:
    ICCEProtocol* m_icce;
    CCCProtocol* m_ccc;
    Protocol m_activeProtocol;
};

// ============== AntiRelay（防中继引擎）=======================
class AntiRelayEngine {
public:
    // 验证测距结果
    RelayCheckResult verifyRanging(const UWBRangingResult& result);
    // 验证时间戳
    bool verifyTimestamp(uint64_t challengeTs, uint64_t responseTs);
    // 获取信任等级
    TrustLevel getTrustLevel();
    // 记录攻击尝试
    void logAttackAttempt(const AttackEvent& event);

private:
    // 距离阈值（米）
    static constexpr float DISTANCE_THRESHOLD = 2.0f;
    // 时间窗口阈值（微秒）
    static constexpr uint64_t TIME_WINDOW_US = 3;
    // SE 签名验证器
    SEService* m_seService;
    // 防重放计数器
    uint32_t m_counter;
};

// ============== VehicleController（车辆控制）=================
class VehicleController {
public:
    // 解锁车门
    ControlResult unlock(DoorMask doors);
    // 上锁
    ControlResult lock(DoorMask doors);
    // 启动发动机
    ControlResult startEngine(EngineMode mode);
    // 执行寻车
    ControlResult findVehicle(uint32_t hornDuration, uint32_t lightDuration);
    // 获取车辆状态
    VehicleStatus getStatus();

private:
    // 发送 CAN 报文
    void sendCANMessage(uint32_t canId, const uint8_t* data, size_t len);
    // 解析 CAN 状态反馈
    VehicleStatus parseCANStatus(const CANFrame& frame);
    // 权限检查
    bool checkPermission(uint32_t keyId, Permission perm);
    // CAN 驱动
    CANDriver* m_canDriver;
    // 权限表
    PermissionTable* m_permissionTable;
};

// ============== KeyManager（钥匙管理器）=====================
class KeyManager {
public:
    // 激活钥匙
    KeyError activateKey(const KeyInfo& keyInfo);
    // 暂停钥匙
    KeyError suspendKey(const string& keyId);
    // 吊销钥匙
    KeyError revokeKey(const string& keyId);
    // 验证钥匙权限
    bool verifyPermission(const string& keyId, Permission perm);
    // 同步钥匙列表到本地
    KeyError syncKeys(const vector<KeyInfo>& keys);

private:
    // 钥匙存储（SE 芯片）
    SEConnection* m_seConnection;
    // 钥匙列表
    map<string, KeyInfo> m_keys;
    // 云端同步
    CloudSync* m_cloudSync;
};
```

---

## 5. 核心模块开发指南

### 5.1 BLE GATT 服务配置

**GATT 服务注册代码：**

```c
// gatt_service.h
#include "ble_gatt.h"

// CCC Digital Key Service UUID
#define BLE_SVC_DIGITAL_KEY_UUID       0xF5A3

// Characteristic UUIDs
#define BLE_CHAR_KEY_PAIRING_UUID       0xF5A4
#define BLE_CHAR_AUTH_CHALLENGE_UUID    0xF5A5
#define BLE_CHAR_AUTH_RESPONSE_UUID     0xF5A6
#define BLE_CHAR_VEHICLE_CONTROL_UUID   0xF5A7
#define BLE_CHAR_UWB_RANGING_UUID       0xF5A8
#define BLE_CHAR_KEY_STATUS_UUID        0xF5A9
#define BLE_CHAR_VEHICLE_STATUS_UUID    0xF5AA
#define BLE_CHAR_OTA_CONTROL_UUID      0xF5AB

// GATT 服务表
static const ble_gatt_service_t digital_key_service = {
    .uuid = { BLE_SVC_DIGITAL_KEY_UUID },
    .characteristics = (ble_gatt_char_t[]){
        {
            .uuid = { BLE_CHAR_KEY_PAIRING_UUID },
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .permissions = BLE_GATT_PERM_WRITE | BLE_GATT_PERM_READ,
            .value_len = 256,
            .on_write = on_pairing_write,
            .on_read = on_pairing_read,
        },
        {
            .uuid = { BLE_CHAR_AUTH_CHALLENGE_UUID },
            .properties = BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE,
            .permissions = BLE_GATT_PERM_READ,
            .value_len = 64,
            .on_write = NULL,
            .on_read = on_challenge_read,
        },
        {
            .uuid = { BLE_CHAR_AUTH_RESPONSE_UUID },
            .properties = BLE_GATT_PROP_WRITE,
            .permissions = BLE_GATT_PERM_WRITE,
            .value_len = 128,
            .on_write = on_auth_response_write,
            .on_read = NULL,
        },
        {
            .uuid = { BLE_CHAR_VEHICLE_CONTROL_UUID },
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .permissions = BLE_GATT_PERM_WRITE,
            .value_len = 128,
            .on_write = on_control_write,
            .on_read = NULL,
        },
        {
            .uuid = { BLE_CHAR_UWB_RANGING_UUID },
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_NOTIFY,
            .permissions = BLE_GATT_PERM_WRITE | BLE_GATT_PERM_READ,
            .value_len = 64,
            .on_write = on_uwb_write,
            .on_read = NULL,
        },
    },
    .num_characteristics = 5,
};

// 初始化 GATT 服务
int ble_gatt_init(void) {
    int ret = ble_gatt_add_service(&digital_key_service);
    if (ret != 0) {
        LOG_ERROR("Failed to add GATT service: %d\n", ret);
        return ret;
    }
    LOG_INFO("BLE GATT Digital Key service registered\n");
    return 0;
}
```

### 5.2 UWB 测距驱动

```c
// uwb_driver.h
#include <stdint.h>

#define UWB_CHANNEL_CH5    0x01  // 6.5 GHz
#define UWB_CHANNEL_CH9    0x02  // 8.0 GHz

typedef enum {
    UWB_ROLE_INITIATOR,
    UWB_ROLE_RESPONDER
} UWB_Role;

typedef struct {
    uint32_t session_id;
    UWB_Role role;
    uint8_t channel;
    float distance_m;        // 测距结果（米）
    float angle_azimuth;     // 方位角（度）
    float confidence;        // 置信度 0-1
    uint8_t is_secure;        // 是否为安全测距
    uint8_t signature[64];   // 测距结果签名
    uint64_t timestamp_us;   // 测距时间戳
} UWBRangingResult;

typedef void (*UWB_Callback)(const UWBRangingResult* result);

typedef struct {
    int (*init)(void);
    int (*deinit)(void);
    int (*start_ranging)(uint32_t session_id, uint8_t channel, UWB_Role role);
    int (*stop_ranging)(uint32_t session_id);
    int (*configure)(const UWBConfig* config);
    int (*send_data)(const uint8_t* data, size_t len);
    int (*register_callback)(UWB_Callback callback);
} UWBDriver;

extern UWBDriver g_uwb_driver;

// ============= uwb_driver.c =============
#include "uwb_driver.h"
#include "uwb_spi.h"
#include "fira_mac.h"

static UWB_Callback g_ranging_callback = NULL;
static bool g_ranging_active = false;

int uwb_init(void) {
    // 1. SPI 接口初始化
    int ret = spi_init(UWB_SPI_PORT, UWB_SPI_BAUDRATE);
    if (ret != 0) return ret;
    
    // 2. 复位 UWB 芯片
    uwb_reset();
    
    // 3. 加载 FiRa MAC 固件
    ret = fira_load_firmware();
    if (ret != 0) {
        LOG_ERROR("FiRa firmware load failed\n");
        return ret;
    }
    
    // 4. 配置 FiRa MAC
    fira_mac_config_t mac_config = {
        .device_role = FIRA_ROLE_RESPONDER,
        .slot_duration_ms = 2,
        .max_slot_num = 16,
        .tx_power_dbm = 0,
    };
    ret = fira_mac_configure(&mac_config);
    
    return ret;
}

int uwb_start_ranging(uint32_t session_id, uint8_t channel, UWB_Role role) {
    fira_session_params_t params = {
        .session_id = session_id,
        .device_role = (role == UWB_ROLE_INITIATOR) ? 
                       FIRA_ROLE_INITIATOR : FIRA_ROLE_RESPONDER,
        .channel = channel,
        .session_type = FIRA_SESSION_TYPE_RANGING,
        .slots_per_rrt = 6,
    };
    
    int ret = fira_session_init(&params);
    if (ret != 0) return ret;
    
    ret = fira_session_start(session_id);
    if (ret == 0) {
        g_ranging_active = true;
        LOG_INFO("UWB ranging started, session_id=%u\n", session_id);
    }
    return ret;
}

int uwb_stop_ranging(uint32_t session_id) {
    int ret = fira_session_stop(session_id);
    if (ret == 0) {
        g_ranging_active = false;
        LOG_INFO("UWB ranging stopped\n");
    }
    return ret;
}

int uwb_register_callback(UWB_Callback callback) {
    g_ranging_callback = callback;
    return 0;
}

// FiRa MAC 事件处理（在中断上下文中调用）
void fira_on_ranging_result(fira_frame_t* frame) {
    if (!g_ranging_active || g_ranging_callback == NULL) return;
    
    UWBRangingResult result = {0};
    result.session_id = frame->session_id;
    result.distance_m = frame->distance / 100.0f;  // 转换为米
    result.angle_azimuth = frame->angle_azimuth;
    result.confidence = frame->quality;
    result.is_secure = frame->sts_indicator > 0;
    result.timestamp_us = get_timestamp_us();
    
    // 从 SE 获取测距签名
    se_sign_ranging_result(&result, frame->raw_data, frame->raw_len);
    
    // 调用回调
    g_ranging_callback(&result);
}
```

### 5.3 ICCE 协议栈状态机

```cpp
// icce_state_machine.h

// ICCE 配对状态机
enum class ICCEPairingState {
    IDLE,
    WAIT_APP_PAIRING_REQUEST,
    WAIT_PUBLIC_KEY_EXCHANGE,
    WAIT_KEY_AGREEMENT,
    WAIT_ACTIVATION_CONFIRM,
    PAIRING_COMPLETE,
    ERROR
};

// ICCE 状态机实现
class ICCEStateMachine {
public:
    ICCEStateMachine() {
        m_currentState = ICCEPairingState::IDLE;
        m_timeout = pdMS_TO_TICKS(30000);  // 30s 超时
    }
    
    // 处理收到的 ICCE 消息
    ICCEError onMessage(const ICCEMessage& msg) {
        ICCEError err = ICCEError::OK;
        
        switch (m_currentState) {
            case ICCEPairingState::IDLE:
                if (msg.type == ICCE_MSG_PAIRING_REQUEST) {
                    err = handlePairingRequest(msg);
                    if (err == ICCEError::OK) {
                        m_currentState = ICCEPairingState::WAIT_PUBLIC_KEY_EXCHANGE;
                        startTimeoutTimer(m_timeout);
                    }
                }
                break;
                
            case ICCEPairingState::WAIT_PUBLIC_KEY_EXCHANGE:
                if (msg.type == ICCE_MSG_PUBLIC_KEY) {
                    err = handlePublicKeyExchange(msg);
                    if (err == ICCEError::OK) {
                        m_currentState = ICCEPairingState::WAIT_KEY_AGREEMENT;
                        restartTimeoutTimer(m_timeout);
                    }
                }
                break;
                
            case ICCEPairingState::WAIT_KEY_AGREEMENT:
                if (msg.type == ICCE_MSG_KEY_AGREEMENT) {
                    err = handleKeyAgreement(msg);
                    if (err == ICCEError::OK) {
                        m_currentState = ICCEPairingState::WAIT_ACTIVATION_CONFIRM;
                        restartTimeoutTimer(m_timeout);
                    }
                }
                break;
                
            case ICCEPairingState::WAIT_ACTIVATION_CONFIRM:
                if (msg.type == ICCE_MSG_ACTIVATION_CONFIRM) {
                    err = handleActivationConfirm(msg);
                    if (err == ICCEError::OK) {
                        m_currentState = ICCEPairingState::PAIRING_COMPLETE;
                        stopTimeoutTimer();
                        notifyPairingComplete();
                    }
                }
                break;
        }
        
        if (err != ICCEError::OK) {
            m_currentState = ICCEPairingState::ERROR;
            notifyError(err);
        }
        
        return err;
    }
    
    // 触发超时
    void onTimeout() {
        LOG_WARN("ICCE pairing timeout, state=%d\n", m_currentState);
        m_currentState = ICCEPairingState::ERROR;
        notifyError(ICCEError::TIMEOUT);
    }
    
private:
    ICCEPairingState m_currentState;
    TickType_t m_timeout;
    TimerHandle_t m_timeoutTimer;
    
    ICCEError handlePairingRequest(const ICCEMessage& msg) {
        // 解析配对请求
        ICCEPairingRequest req;
        ICCE_DecodePairingRequest(msg.data, &req);
        
        // 验证协议版本
        if (req.protocolVersion != ICCE_PROTOCOL_VERSION) {
            return ICCEError::UNSUPPORTED_VERSION;
        }
        
        // 生成随机挑战
        generateRandom(m_challenge, sizeof(m_challenge));
        m_timestamp = getCurrentTimestamp();
        
        // 发送挑战给手机
        ICCEMessage rsp = {
            .type = ICCE_MSG_CHALLENGE,
            .data = buildChallengeMsg(m_challenge, m_timestamp)
        };
        sendICCEProtocolMessage(rsp);
        
        return ICCEError::OK;
    }
    
    ICCEError handlePublicKeyExchange(const ICCEMessage& msg) {
        // 解析手机公钥
        ICCEPublicKey key;
        ICCE_DecodePublicKey(msg.data, &key);
        
        // 保存手机公钥
        memcpy(m_peerPublicKey, key.publicKey, 64);
        
        // 生成车端密钥对
        SM2_GenerateKeyPair(&m_localKeyPair);
        
        // SM2 密钥协商
        SM2_KeyAgreement_Init(&m_kaContext, &m_localKeyPair, key.publicKey);
        
        // 发送车端公钥
        ICCEMessage rsp = {
            .type = ICCE_MSG_PUBLIC_KEY,
            .data = buildPublicKeyMsg(m_localKeyPair.publicKey)
        };
        sendICCEProtocolMessage(rsp);
        
        return ICCEError::OK;
    }
    
    ICCEError handleKeyAgreement(const ICCEMessage& msg) {
        // SM2 密钥协商完成
        SM2_KeyAgreement_Process(&m_kaContext, msg.data, msg.len);
        
        // 生成会话密钥
        uint8_t sessionKey[32];
        SM2_KeyAgreement_GetSessionKey(&m_kaContext, sessionKey);
        
        // 存储会话密钥到 SE
        m_seService->importSessionKey(sessionKey, sizeof(sessionKey));
        
        // 请求云端确认
        CloudConfirmKeyActivation(m_keyId, sessionKey);
        
        return ICCEError::OK;
    }
    
    ICCEError handleActivationConfirm(const ICCEMessage& msg) {
        // 验证云端确认签名
        if (!m_seService->verifySignature(msg.data, msg.signature)) {
            return ICCEError::SIGNATURE_INVALID;
        }
        
        // 激活钥匙
        m_keyManager->activateKey(m_keyId);
        
        return ICCEError::OK;
    }
};
```

### 5.4 CCC 协议栈关键实现

```cpp
// ccc_protocol.h

// CCC Digital Key 3.0 关键流程

class CCCProtocol {
public:
    // ========== 配对流程 ==========
    CCCError startPairing(const CCCPairingRequest& req);
    CCCError handleDeviceAttestation(const CCCAttestation& attestation);
    CCCError handleVehicleCertificate(const CCCCertificate& cert);
    CCCError performSPAKE2Plus(const uint8_t* password, size_t pwLen);
    
    // ========== CCC Auth 认证流程 ==========
    CCCError processAuthChallenge(const CCCChallenge& challenge);
    CCCError verifyDeviceSignature(const uint8_t* signature, size_t sigLen);
    CCCError executeVehicleControl(ControlAction action);
    
    // ========== UWB Secure Ranging ==========
    CCCError startSecureRanging(uint32_t sessionId);
    bool verifyRangingIntegrity(const UWBRangingFrame& frame);

private:
    // spake2+ 上下文
    spake2plus_ctx_t* m_spake2Ctx;
    // 车辆证书
    CCCCertificate m_vehicleCert;
    // 设备公钥
    uint8_t m_devicePublicKey[65];
    // 会话密钥
    uint8_t m_sessionKey[32];
};
```

```cpp
// CCC spake2+ 密码认证密钥协商
CCCError CCCProtocol::performSPAKE2Plus(const uint8_t* password, size_t pwLen) {
    // 1. 初始化 spake2+ 上下文
    m_spake2Ctx = spake2plus_new(
        SPAKE2_ROLE_PROVER,    // 手机侧为 Prover
        P256_CURVE,            // P-256 曲线
        SHA256_HASH            // SHA-256
    );
    
    // 2. 生成 Prover 消息 (P + w·M)
    uint8_t prover_msg[65];
    size_t prover_msg_len;
    
    // 密码哈希
    uint8_t password_scalar[32];
    sm3_hash(password, pwLen, password_scalar);
    
    spake2plus_prover_start(
        m_spake2Ctx,
        password_scalar,
        sizeof(password_scalar),
        prover_msg,
        &prover_msg_len
    );
    
    // 3. 发送 Prover 消息给车端
    sendToVehicle(prover_msg, prover_msg_len);
    
    // 4. 接收 Verifier 消息 (P + w·N)
    uint8_t verifier_msg[65];
    size_t verifier_msg_len;
    receiveFromVehicle(verifier_msg, &verifier_msg_len);
    
    // 5. 生成会话密钥 K 和确认密钥 Ka
    uint8_t session_key[32];
    uint8_t confirm_key[32];
    
    spake2plus_prover_confirm(
        m_spake2Ctx,
        verifier_msg,
        verifier_msg_len,
        session_key,
        confirm_key
    );
    
    // 6. 发送确认值 A，确认会话
    uint8_t confirm_value[32];
    spake2plus_prover_generate_confirm(
        m_spake2Ctx,
        confirm_value
    );
    sendToVehicle(confirm_value, sizeof(confirm_value));
    
    // 7. 接收并验证车端确认值 B
    uint8_t peer_confirm[32];
    receiveFromVehicle(peer_confirm, sizeof(peer_confirm));
    
    if (!spake2plus_verify_confirm(m_spake2Ctx, peer_confirm)) {
        spake2plus_free(m_spake2Ctx);
        return CCCError::AUTHENTICATION_FAILED;
    }
    
    // 8. 存储会话密钥
    memcpy(m_sessionKey, session_key, sizeof(session_key));
    
    spake2plus_free(m_spake2Ctx);
    return CCCError::OK;
}
```

---

## 6. 关键算法实现

### 6.1 防中继攻击算法

```cpp
// anti_relay.h

// 防中继攻击检查流程
class AntiRelayChecker {
public:
    // 主要检查函数
    RelayCheckDecision check(const UWBRangingResult& ranging, 
                             const AuthChallenge& challenge,
                             const AuthResponse& response) {
        // 1. UWB 距离验证（核心）
        if (!checkDistance(ranging.distance_m)) {
            return RelayCheckDecision::REJECT_DISTANCE;
        }
        
        // 2. 时间窗口验证
        if (!checkTimeWindow(challenge.timestamp, response.timestamp)) {
            return RelayCheckDecision::REJECT_TIME_WINDOW;
        }
        
        // 3. 测距结果签名验证
        if (!verifyRangingSignature(ranging)) {
            return RelayCheckDecision::REJECT_SIGNATURE;
        }
        
        // 4. 递增计数器验证
        if (!verifyCounter(response.counter)) {
            return RelayCheckDecision::REJECT_COUNTER;
        }
        
        // 5. Nonce 唯一性验证
        if (!verifyNonce(ranging.nonce)) {
            return RelayCheckDecision::REJECT_NONCE;
        }
        
        return RelayCheckDecision::ALLOW;
    }

private:
    // 距离验证：确保手机在有效范围内
    bool checkDistance(float distance_m) {
        // 配置：最大解锁距离 2 米
        const float MAX_UNLOCK_DISTANCE = 2.0f;
        // 置信度要求
        const float MIN_CONFIDENCE = 0.85f;
        
        if (distance_m > MAX_UNLOCK_DISTANCE) {
            LOG_WARN("Relay attack detected: distance=%.2fm > %.2fm\n", 
                     distance_m, MAX_UNLOCK_DISTANCE);
            return false;
        }
        
        // 需要安全测距（STS）
        if (ranging.is_secure == false) {
            LOG_WARN("Relay attack warning: non-secure ranging\n");
            // 降级：使用更严格的距离阈值
            if (distance_m > 0.5f) {
                return false;
            }
        }
        
        return true;
    }
    
    // 时间窗口验证：确保响应在允许延迟内
    bool checkTimeWindow(uint64_t challengeTs, uint64_t responseTs) {
        // ICCE/CCC 规范：最大允许延迟 100ms
        const uint64_t MAX_ALLOWED_DELAY_MS = 100;
        
        uint64_t delayMs = (responseTs > challengeTs) ? 
                            (responseTs - challengeTs) : 0;
        
        if (delayMs > MAX_ALLOWED_DELAY_MS) {
            LOG_WARN("Relay attack detected: delay=%llums > %llums\n",
                     delayMs, MAX_ALLOWED_DELAY_MS);
            return false;
        }
        
        return true;
    }
    
    // 测距签名验证：确保测距数据未被篡改
    bool verifyRangingSignature(const UWBRangingResult& ranging) {
        // 构造待签名数据
        uint8_t data[sizeof(float)*3 + sizeof(uint64_t)];
        encodeRangingData(data, &ranging);
        
        // SE 验证签名
        return m_seService->verifySignature(
            data, sizeof(data),
            ranging.signature, 64,
            m_vehicleCertificate.publicKey
        );
    }
    
    // 计数器验证：防止重放攻击
    bool verifyCounter(uint32_t receivedCounter) {
        // 计数器必须大于上次值
        if (receivedCounter <= m_lastCounter) {
            LOG_WARN("Relay attack: counter not incrementing (%u <= %u)\n",
                     receivedCounter, m_lastCounter);
            return false;
        }
        
        // 计数器不能跳步太大（防异常）
        if (receivedCounter - m_lastCounter > 1000) {
            LOG_WARN("Relay attack: counter jump too large\n");
            return false;
        }
        
        m_lastCounter = receivedCounter;
        return true;
    }
    
    // Nonce 验证：确保随机数未被重用
    bool verifyNonce(const uint8_t* nonce) {
        // 检查 nonce 是否在已用列表中
        for (const auto& used : m_usedNonces) {
            if (memcmp(nonce, used.data(), 32) == 0) {
                LOG_WARN("Relay attack: nonce reuse detected\n");
                return false;
            }
        }
        
        // 添加到已用列表
        m_usedNonces.push_back(vector<uint8_t>(nonce, nonce + 32));
        
        // 限制列表大小，防止内存膨胀
        if (m_usedNonces.size() > MAX_NONCE_CACHE_SIZE) {
            m_usedNonces.erase(m_usedNonces.begin());
        }
        
        return true;
    }
    
private:
    uint32_t m_lastCounter = 0;
    vector<vector<uint8_t>> m_usedNonces;
    SEService* m_seService;
    Certificate* m_vehicleCertificate;
};
```

### 6.2 ICCE 国密密钥协商

```cpp
// icce_crypto.h

// SM2 密钥协商流程
class ICCEKeyAgreement {
public:
    // 步骤1：初始化（车端）
    void init_as_initiator(
        const uint8_t* selfPrivateKey,  // 自身私钥 (32 bytes)
        const uint8_t* selfPublicKey,    // 自身公钥 (64 bytes)
        const uint8_t* peerPublicKey,     // 对端公钥 (64 bytes)
        const uint8_t* selfId,            // 自身标识
        const uint8_t* peerId             // 对端标识
    ) {
        // 初始化 SM2 密钥协商上下文
        sm2_ka_init(&m_ctx, SM2_KA_MODE_RESPONDER);
        
        // 设置本端密钥
        sm2_ka_set_key(&m_ctx, selfPrivateKey, selfPublicKey);
        
        // 设置对端公钥
        sm2_ka_set_peer_key(&m_ctx, peerPublicKey);
        
        // 设置标识
        sm2_ka_set_id(&m_ctx, selfId, 16, peerId, 16);
        
        m_phase = KA_PHASE_STEP1;
    }
    
    // 步骤2：生成第一轮消息（发送 RA）
    vector<uint8_t> generateStep1Message() {
        // 生成随机数 RA
        generateRandom(m_randomA, 32);
        
        // 计算 RA·P（椭圆曲线标量乘）
        sm2_point_t pointA;
        sm2_scalar_mult(&pointA, &m_ctx.curve->generator, m_randomA);
        
        // 构建消息
        vector<uint8_t> msg;
        msg.push_back(0x80);  // 标识：步骤1
        msg.insert(msg.end(), m_randomA, m_randomA + 32);
        msg.insert(msg.end(), 
                   (uint8_t*)&pointA.x, 
                   (uint8_t*)&pointA.x + 32);
        msg.insert(msg.end(), 
                   (uint8_t*)&pointA.y, 
                   (uint8_t*)&pointA.y + 32);
        
        // SM3 哈希
        uint8_t hash[32];
        sm3_hash(msg.data(), msg.size(), hash);
        
        return msg;
    }
    
    // 步骤3：处理响应（接收 RB）
    bool processStep2Message(const vector<uint8_t>& msg) {
        // 解析 RA
        sm3_hash(&msg[33], 96, m_hashZ);  // 存 ZA
        
        // 计算 SB = h·(dA + e·xA)·(PB + e·RA)
        sm2_scalar_t e;
        sm2_scalar_from_bytes(&e, m_randomA);
        
        sm2_scalar_t tmp1, tmp2;
        sm2_scalar_mul(&tmp1, &e, m_pointA.x);  // e·xA
        sm2_scalar_add(&tmp2, m_ctx.privateKey, &tmp1);  // dA + e·xA
        
        sm2_scalar_t sharedSecret[2];
        sm2_scalar_mul(&sharedSecret[0], &tmp2, &m_pointB.x);  // h·SB.x
        
        // 计算会话密钥：SM3(ZA || ZB || xB || xA || sharedSecret)
        vector<uint8_t> keyData;
        keyData.insert(keyData.end(), m_hashZ, m_hashZ + 32);
        keyData.insert(keyData.end(), m_hashZ + 32, m_hashZ + 64);
        keyData.insert(keyData.end(), m_sharedSecret, m_sharedSecret + 32);
        
        sm3_hash(keyData.data(), keyData.size(), m_sessionKey);
        
        return true;
    }
    
    // 获取会话密钥
    const uint8_t* getSessionKey() const { return m_sessionKey; }
    
private:
    sm2_ka_context m_ctx;
    KA_Phase m_phase;
    uint8_t m_randomA[32];
    sm2_point_t m_pointA;
    sm2_point_t m_pointB;
    uint8_t m_hashZ[64];  // ZA || ZB
    uint8_t m_sessionKey[32];
};
```

---

## 7. 接口调用示例

### 7.1 车辆解锁流程

```cpp
// vehicle_unlock_example.cpp

// 完整解锁流程示例
class UnlockFlow {
public:
    UnlockResult execute(const string& keyId, 
                         const UWBRangingResult& ranging) {
        // ========== 步骤1：验证钥匙权限 ==========
        if (!m_keyManager->verifyPermission(keyId, Permission::UNLOCK)) {
            LOG_WARN("Key %s has no UNLOCK permission\n", keyId.c_str());
            return UnlockResult::PERMISSION_DENIED;
        }
        
        // ========== 步骤2：防中继检查 ==========
        RelayCheckDecision relayCheck = 
            m_antiRelayChecker->check(
                ranging,
                m_currentChallenge,
                m_pendingResponse
            );
        
        if (relayCheck != RelayCheckDecision::ALLOW) {
            LOG_WARN("Relay attack detected, decision=%d\n", relayCheck);
            m_auditLogger->logSecurityEvent(
                SecurityEvent::RELAY_ATTACK_ATTEMPT, 
                keyId, ranging.distance_m
            );
            return UnlockResult::RELAY_ATTACK_BLOCKED;
        }
        
        // ========== 步骤3：执行解锁 ==========
        ControlResult result = m_vehicleCtrl->unlock(
            DoorMask::ALL_DOORS
        );
        
        if (result.success) {
            // ========== 步骤4：记录审计日志 ==========
            m_auditLogger->logKeyOperation(
                KeyOperation::UNLOCK, 
                keyId,
                ranging.distance_m,
                ranging.timestamp_us
            );
            
            // ========== 步骤5：通知云端 ==========
            m_cloudSync->reportUnlockEvent(keyId, result.timestamp);
            
            return UnlockResult::SUCCESS;
        } else {
            return UnlockResult::CONTROL_FAILED;
        }
    }
    
private:
    KeyManager* m_keyManager;
    AntiRelayChecker* m_antiRelayChecker;
    VehicleController* m_vehicleCtrl;
    AuditLogger* m_auditLogger;
    CloudSync* m_cloudSync;
};
```

### 7.2 BLE GATT 数据收发

```c
// ble_data_example.c

// 接收车辆控制指令（GATT Write Request）
void on_vehicle_control_write(
    uint16_t conn_handle,
    uint16_t attr_handle,
    const uint8_t* value,
    uint16_t len
) {
    // 解析控制指令
    VehicleControlCmd cmd = {0};
    if (parseVehicleControlCmd(value, len, &cmd) != 0) {
        send_error_response(conn_handle, attr_handle, 0x06);  // Invalid data
        return;
    }
    
    // 验证认证令牌
    if (!verifyAuthToken(cmd.authToken, sizeof(cmd.authToken))) {
        send_error_response(conn_handle, attr_handle, 0x07);  // Auth failed
        return;
    }
    
    // 执行控制
    ControlResult result = {0};
    switch (cmd.action) {
        case ACTION_UNLOCK:
            result = vehicle_unlock(cmd.doors);
            break;
        case ACTION_LOCK:
            result = vehicle_lock(cmd.doors);
            break;
        case ACTION_START_ENGINE:
            result = vehicle_start_engine(cmd.engineMode);
            break;
        case ACTION_HORN_LIGHTS:
            result = vehicle_find_car(cmd.duration);
            break;
        default:
            send_error_response(conn_handle, attr_handle, 0x01);
            return;
    }
    
    // 发送响应（GATT Indication）
    VehicleControlRsp rsp = {
        .requestId = cmd.requestId,
        .status = result.success ? 0x00 : result.errorCode,
        .executedAt = result.timestamp
    };
    
    uint8_t rsp_data[64];
    size_t rsp_len = buildVehicleControlRsp(&rsp, rsp_data);
    
    ble_gatt_send_indication(
        conn_handle,
        BLE_CHAR_VEHICLE_CONTROL_UUID,
        rsp_data,
        rsp_len
    );
    
    // 记录操作
    LOG_INFO("Vehicle control: action=%d, result=%d\n", 
             cmd.action, result.success);
}
```

### 7.3 OTA 固件升级

```cpp
// ota_example.cpp

// OTA 升级流程
class OTAUpgrade {
public:
    OTAError startUpgrade(const string& downloadUrl, 
                         const string& version,
                         const uint8_t* signature) {
        // 1. 验证签名
        if (!m_seService->verifySignature(downloadUrl, signature)) {
            return OTAError::INVALID_SIGNATURE;
        }
        
        // 2. 创建 OTA 任务
        OTATask task = {
            .version = version,
            .downloadUrl = downloadUrl,
            .status = OTA_STATUS_DOWNLOAD_PENDING
        };
        m_taskId = m_otaManager->createTask(task);
        
        // 3. 启动下载（后台任务）
        xTaskCreate(downloadTask, "OTA_DOWNLOAD", 4096, this, 5, NULL);
        
        return OTAError::OK;
    }
    
private:
    // 下载任务（FreeRTOS 任务）
    static void downloadTask(void* param) {
        auto* self = static_cast<OTAUpgrade*>(param);
        
        // HTTP 下载固件
        auto chunks = self->m_httpClient->download(
            self->m_downloadUrl,
            [](size_t offset, const uint8_t* data, size_t len) {
                // 写入下载缓冲区
                self->m_downloadBuffer.write(data, len);
            }
        );
        
        // 固件完整性校验（SHA-256）
        uint8_t hash[32];
        sha256_hash(self->m_downloadBuffer.data(), 
                    self->m_downloadBuffer.size(), hash);
        
        if (memcmp(hash, self->m_expectedHash, 32) != 0) {
            self->m_otaManager->updateStatus(
                self->m_taskId, OTA_STATUS_VERIFY_FAILED);
            vTaskDelete(NULL);
            return;
        }
        
        // 切换到 B 分区
        self->m_bootManager->switchPartition(B_PARTITION);
        
        // 写入固件到 B 分区
        self->m_flash->write(
            B_PARTITION_ADDR,
            self->m_downloadBuffer.data(),
            self->m_downloadBuffer.size()
        );
        
        // A/B 交换
        self->m_bootManager->setNextBoot(B_PARTITION);
        
        // 重启
        self->m_systemManager->reboot();
        
        vTaskDelete(NULL);
    }
};
```

---

## 8. 测试要点

### 8.1 单元测试

| 模块 | 测试用例数 | 覆盖率目标 | 关键测试点 |
|------|-----------|-----------|-----------|
| UWB 驱动 | 50+ | ≥ 80% | 测距精度、通信稳定性 |
| BLE GATT | 30+ | ≥ 85% | 服务注册、数据收发 |
| ICCE 协议 | 80+ | ≥ 80% | 状态机、消息编解码 |
| CCC 协议 | 100+ | ≥ 80% | spake2+、签名验证 |
| 防中继 | 40+ | ≥ 85% | 中继攻击模拟 |
| 车辆控制 | 60+ | ≥ 80% | CAN 报文、权限检查 |

### 8.2 集成测试

| 测试项 | 方法 | 验收标准 |
|-------|------|---------|
| 解锁响应时间 | 从手机发起到车门解锁的端到端计时 | P99 < 1s |
| 多设备并发 | 同时连接 8 台手机设备 | 无掉线 |
| 低功耗测试 | 车辆静置 48h，电池消耗 | < 3% |
| 地下车库测试 | 信号遮挡环境下解锁 | 成功率 ≥ 99% |
| 中继攻击模拟 | 使用信号放大器模拟中继 | 车门不解锁 |
| 连续压力测试 | 1000 次连续解锁 | 成功率 ≥ 99.5% |

### 8.3 协议合规测试

| 测试项 | 测试机构/工具 | 验收标准 |
|-------|-------------|---------|
| ICCE 协议一致性 | ICCE 认证实验室 | 全部测试用例通过 |
| CCC DK 3.0 一致性 | CCC 认证实验室 | 全部测试用例通过 |
| 国密算法合规 | 国家密码管理局 | SM2/SM3/SM4 验证通过 |
| 安全芯片 EAL5+ | SGS/ITS 认证实验室 | 安全评估通过 |

### 8.4 测试代码示例

```cpp
// test_anti_relay.cpp

// 测试用例：模拟中继攻击（距离超出阈值）
void test_relay_distance_attack() {
    AntiRelayChecker checker;
    UWBRangingResult ranging = {
        .distance_m = 5.0f,  // 攻击者距离 5 米
        .is_secure = true,
        .confidence = 0.95f
    };
    
    AuthChallenge challenge = {
        .timestamp = getCurrentTimestamp()
    };
    
    AuthResponse response = {
        .timestamp = challenge.timestamp + 50,  // 50ms 延迟
        .counter = 100,
        .nonce = generateNonce()
    };
    
    // 验证：应该拒绝
    RelayCheckDecision decision = checker.check(ranging, challenge, response);
    TEST_ASSERT_EQUAL(RelayCheckDecision::REJECT_DISTANCE, decision);
}

// 测试用例：正常解锁（距离在阈值内）
void test_normal_unlock() {
    AntiRelayChecker checker;
    UWBRangingResult ranging = {
        .distance_m = 1.2f,  // 正常距离 1.2 米
        .is_secure = true,
        .confidence = 0.9f
    };
    
    AuthChallenge challenge = {
        .timestamp = getCurrentTimestamp()
    };
    
    AuthResponse response = {
        .timestamp = challenge.timestamp + 80,  // 80ms 延迟
        .counter = 101,
        .nonce = generateNonce()
    };
    
    // 验证：应该放行
    RelayCheckDecision decision = checker.check(ranging, challenge, response);
    TEST_ASSERT_EQUAL(RelayCheckDecision::ALLOW, decision);
}
```

---

## 9. 参考规范

| 规范 | 版本 | 说明 |
|------|------|------|
| T/CA 110-2020 | V1.0 | ICCE 数字钥匙系统技术规范 |
| T/CA 109-2020 | V1.0 | ICCE 数字钥匙系统安全规范 |
| CCC Digital Key 3.0 | R1 | CCC 数字钥匙 3.0 规范 |
| IEEE 802.15.4z | 2020 | UWB 安全测距规范 |
| FiRa MAC | 1.0+ | FiRa MAC 层规范 |
| ISO 14443-4 | 2018 | NFC 协议规范 |
| ISO 7816-4 | 2020 | 安全芯片 APDU 命令规范 |
| GM/T 0003-2012 | 2012 | SM2 椭圆曲线公钥密码算法 |
| GM/T 0004-2012 | 2012 | SM3 密码杂凑算法 |
| GM/T 0002-2012 | 2012 | SM4 分组密码算法 |
| ISO 26262 | 2018 | 道路车辆功能安全 |
| ISO 21434 | 2021 | 道路车辆网络安全工程 |

---

*文档版本：v1.0 | 最后更新：2026-04-26 | 作者：code_designer*
