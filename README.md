# Yule Digital Key Connectivity Stack (yuleDKCS)

Yule数字钥匙连接协议栈 - 支持多协议的车载数字钥匙解决方案

## 特性

- **多协议支持**: CCC R3 / ICCE / ICCOA
- **统一 API**: 一套接口适配多种协议
- **安全架构**: 支持硬件SE、TEE、SIM等安全元件
- **多连接方式**: BLE / NFC / UWB
- **跨平台**: C99 标准，可移植到各种嵌入式平台

## 协议支持

| 协议 | 版本 | 连接方式 | 加密算法 | SE 支持 |
|-------|-------|---------|----------|---------|
| CCC | R3 | BLE/NFC/UWB | P-256/AES-128-GCM | eSE/TEE/SIM |
| ICCE | 2.0 | BLE/NFC | SM2/SM4 | 国产SE |
| ICCOA | 1.2 | BLE/NFC | P-256/AES-128 | eSE/TEE |

## 快速开始

### 构建

```bash
mkdir build && cd build
cmake ..
make
make test
sudo make install
```

### 基本使用

```c
#include "dkcs.h"

// 初始化
session_config_t config = {
    .protocol = PROTOCOL_CCC,
    .conn_type = CONN_BLE,
    .se_type = SE_ESE
};
dkcs_init(&config);

// 配对
uint8_t vin[17] = "LSVNV2182E2100001";
dkcs_pairing_start(PROTOCOL_CCC, vin, callback, NULL);

// 解锁车辆
uint8_t key_id[16] = {...};
dkcs_vehicle_unlock(key_id, vin);

// 清理
dkcs_deinit();
```

## 目录结构

```
yuleDKCS/
├── include/          # 头文件
│   ├── dkcs.h    # 主头文件
│   ├── ccc.h     # CCC 协议
│   ├── icce.h    # ICCE 协议
│   └── iccoa.h   # ICCOA 协议
├── src/              # 源码
│   ├── ccc/           # CCC 实现
│   ├── icce/          # ICCE 实现
│   ├── iccoa/         # ICCOA 实现
│   └── common/        # 通用实现
├── tests/            # 测试
└── examples/         # 示例
```

## 文档

- [API 参考](docs/api_reference.md)
- [CCC 协议实现](docs/ccc_implementation.md)
- [ICCE 协议实现](docs/icce_implementation.md)
- [ICCOA 协议实现](docs/iccoa_implementation.md)
- [SE 接口指南](docs/se_interface_guide.md)

## 版本历史

- v1.0.0 (2026-05-08): 初始版本，支持 CCC R3、ICCE 2.0、ICCOA 1.2

## 许可

MIT License - 上海予乐电子科技有限公司
