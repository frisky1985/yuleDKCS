# CCC数字钥匙协议栈

基于CCC Digital Key Release 3.0规范的车端协议栈实现。

## 硬件平台

| 组件 | 型号 | 接口 |
|------|------|------|
| BLE | NXP KW47A | SPI1 |
| UWB | NXP NCJ29D6 | SPI3 |
| NFC | ST ST25R501 | SPI2 |
| 安全 | NXP SE050 | I2C |

## 目录结构

```
ccc_protocol/
├── docs/
│   └── SPEC.md              # 技术规格书
├── include/
│   └── ccc_digital_key.h    # 公共头文件
├── src/
│   ├── core/
│   │   └── ccc_dk_core.c    # 核心状态机
│   ├── nfc/
│   │   └── nfc_st25r501.c   # NFC驱动 (ST25R501)
│   ├── ble/
│   │   └── ble_kw47a.c      # BLE驱动 (KW47A)
│   ├── uwb/
│   │   └── uwb_ncj29d6.c    # UWB驱动 (NCJ29D6)
│   ├── keymgmt/
│   │   └── key_mgmt.c       # 密钥管理
│   └── security/
│       └── security.c       # 安全模块 (SE050)
├── test/                    # 单元测试
├── build/                   # 构建输出
└── CMakeLists.txt           # 构建配置
```

## 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=stm32l5.cmake
make
```

## 依赖

- STM32 HAL 1.27+
- RFAL (NFC) 2.4+
- KW47A SDK 2.x
- NCJ29D6 SDK 1.x
- SE050 Plug & Trust 4.x
