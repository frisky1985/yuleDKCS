# yuleDKCS 开发者指南

## 概述

yuleDKCS (Yule Digital Key Connectivity Stack) 是一个支持多种汽车数字钥匙协议的连接栈，包括 CCC Digital Key R3、ICCE 和 ICCOA。

## 支持的协议

| 协议 | 版本 | 加密 | UWB | 状态 |
|-------|-------|------|-----|------|
| CCC | R3 | ECDH P-256 + AES-128-GCM | ✓ | 完整 |
| ICCE | 1.0 | SM2/SM4 | - | 完整 |
| ICCOA | 1.0 | P-256 + AES-128 | - | 完整 |

## 快速开始

### 构建

```bash
mkdir build && cd build
cmake .. -DENABLE_SE050=ON -DENABLE_SCP03=ON -DENABLE_UWB=ON
make -j$(nproc)
```

### 基本使用

```c
#include "dkcs.h"

/* 初始化 */
session_config_t config = {
    .protocol = PROTOCOL_CCC,
    .conn_type = CONN_BLE
};
dkcs_init(&config);

/* 执行解锁 */
dkcs_vehicle_unlock(key_id, vin);

/* 清理 */
dkcs_deinit();
```

## 架构概览

```
┌─────────────────────────┐
│   应用层 (App)      │
├─────────────────────────┤
│   统一 API (dkcs)    │
├─────────────────────────┤
│   协议层 (CCC/ICCE/ICCOA)│
├─────────────────────────┤
│   安全层 (SE050/SCP03) │
├─────────────────────────┤
│   HAL 层 (Flash/I2C/SPI)│
└─────────────────────────┘
```

## API 参考

详见生成的 Doxygen 文档: `docs/api/html/index.html`

## 测试

```bash
# 运行所有测试
ctest --output-on-failure

# 运行特定测试
./tests/test_ccc_core

# 模糊测试
cd tests/fuzzing
afl-fuzz -i inputs/ -o outputs/ ./fuzz_dkcs
```

## 贡献指南

1. 遵循 MISRA-C 2012 规范
2. 所有代码必须有单元测试
3. 提交前运行静态分析
4. 保持代码覆盖率 >85%

## 许可证

MIT License - 详见 LICENSE 文件
