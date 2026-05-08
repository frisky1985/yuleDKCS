# S32K3+HSE 平台迁移指南

> **版本**：v1.0  
> **日期**：2026-04-26  
> **分支**：`feature/s32k3-hse`  
> **目标平台**：NXP S32K3 + HSE (Hardware Security Engine)  
> **源平台**：NXP S32G2 + SE050  

---

## 目录

1. [概述](#1-概述)
2. [硬件变更清单](#2-硬件变更清单)
3. [软件变更清单](#3-软件变更清单)
4. [HSE API 映射](#4-hse-api-映射)
5. [HSE 驱动层设计](#5-hse-驱动层设计)
6. [代码修改示例](#6-代码修改示例)
7. [构建配置](#7-构建配置)
8. [验证清单](#8-验证清单)

---

## 1. 概述

### 1.1 迁移背景

NXP S32K3 系列 MCU 集成了片载 Hardware Security Engine（HSE），可替代外挂的 SE050 安全芯片，带来以下优势：

- **成本降低**：无需外挂 SE050，减少 BOM 成本
- **安全等级提升**：HSE 支持 ISO 26262 ASIL-D（SE050 为 EAL5+）
- **性能提升**：HSE 通过内部总线访问，延迟更低
- **简化设计**：减少 PCB 布线（无需 SPI/I2C 到 SE050）

### 1.2 迁移原则

1. **保持 API 兼容**：HSE 驱动层提供与 SE050 相同的接口，通过编译宏切换
2. **不删除原代码**：main 分支保留 SE050 方案，feature/s32k3-hse 分支使用 HSE
3. **增量修改**：通过 `#ifdef S32K3_HSE` 条件编译实现双平台支持
4. **代码风格一致**：新代码遵循项目现有代码风格

### 1.3 迁移范围

| 层级 | 模块 | 变更类型 |
|------|------|---------|
| 驱动层 | HSE 驱动 | 新建 |
| 驱动层 | 安全元件抽象层 | 新建 |
| 安全模块 | 安全通道 | 修改 |
| 安全模块 | 密钥管理 | 修改 |
| OTA | 安全启动 | 修改 |
| 加密引擎 | 加密引擎封装 | 修改 |
| 构建 | CMakeLists.txt | 修改 |

---

## 2. 硬件变更清单

### 2.1 MCU 变更

| 项目 | S32G2 方案 | S32K3 方案 |
|------|-----------|----------|
| 型号 | NXP S32G2/G3 | NXP S32K3xx (S32K312/S32K322/S32K342 等) |
| 架构 | Cortex-A53 + Cortex-M7 异构 | Cortex-M7 (单核/双核/锁步) |
| 主频 | A53: 1GHz, M7: 240MHz | 最高 160MHz (根据型号) |
| 通信外设 | UART, SPI, I2C, CAN, FlexRay | UART, SPI, I2C, CAN, LIN |
| 安全特性 | HSE (有限) | HSE (完整功能, ASIL-D) |
| ISO 26262 | ASIL-B | ASIL-D |

### 2.2 安全芯片变更

| 项目 | S32G2+SE050 | S32K3+HSE |
|------|-----------|----------|
| 安全芯片 | NXP SE050（外挂，I2C/SPI） | 片载 HSE（固件） |
| 接口 | SPI/I2C（外部通信） | 内部 API（无外部通信） |
| 安全认证 | CC EAL5+ | ASIL-D |
| 算法 | SM2/SM3/SM4, ECC/RSA/AES | AES-128/256, ECC, RSA, SHA, HMAC, TRNG |
| 密钥存储 | SE050 NVM (外部) | HSE NVM (内部) |
| 随机数 | SE050 TRNG | HSE TRNG |
| 安全通道 | SCP03（需建立） | 不需要（片载） |

### 2.3 外设变更

| 组件 | S32G2 方案 | S32K3 方案 | 变更类型 |
|------|-----------|-----------|---------|
| MCU | S32G2 | S32K3xx | 替换 |
| 安全芯片 | SE050（外挂I2C） | HSE（片载） | 替换 |
| BLE | KW38（UART） | KW38 / NCF29A1 | 可能保留/替换 |
| NFC | PN5180（SPI） | PN5180 / 集成 | 可能保留/替换 |
| UWB | SR250（SPI） | SR250 | 保留 |

---

## 3. 软件变更清单

### 3.1 驱动层变更

| 原文件 | 新文件 | 变更内容 | 优先级 |
|--------|--------|---------|--------|
| `se050_driver.c/h` | `hse_driver.c/h` | HSE 驱动实现 | P0 |
| - | `secure_element_interface.h` | 统一抽象接口 | P0 |
| `crypto_engine.c/h` | `crypto_engine.c/h` | 改用 HSE API | P0 |

### 3.2 安全模块变更

| 文件 | 变更内容 | 优先级 |
|------|---------|--------|
| `secure_channel.c` | 移除 SCP03，HSE 直接加解密 | P0 |
| `key_manager.c` | 改用 HSE NVM API | P0 |
| `secure_boot.c` | 使用 HSE Secure Boot | P0 |
| `anti_relay.c` | 适配 HSE 测距签名 | P1 |

### 3.3 构建配置变更

| 文件 | 变更内容 | 优先级 |
|------|---------|--------|
| `CMakeLists.txt` | 添加 S32K3+HSE 配置选项 | P0 |
| `config.h` | 添加 HSE 配置宏 | P1 |

---

## 4. HSE API 映射

### 4.1 SE050 → HSE API 对照表

| SE050 操作 | HSE 等效 API | 备注 |
|-----------|-------------|------|
| `se050_init()` | `hse_init()` | HSE 固件初始化 |
| `se050_reset()` | `hse_reset()` | HSE 复位 |
| `se050_verify_card()` | `hse_self_test()` | HSE 自检 |
| `se050_generate_keypair()` | `hse_generate_keypair()` | HSE ECC 密钥对生成 |
| `se050_sign()` | `hse_ecc_sign()` | HSE ECC 签名 |
| `se050_verify_signature()` | `hse_ecc_verify()` | HSE ECC 验签 |
| `se050_encrypt()` | `hse_aes_encrypt()` | HSE AES 加密 |
| `se050_decrypt()` | `hse_aes_decrypt()` | HSE AES 解密 |
| `se050_import_key()` | `hse_store_key()` | HSE 密钥导入 |
| `se050_delete_key()` | `hse_destroy_key()` | HSE 密钥删除 |
| `se050_generate_random()` | `hse_get_random()` | HSE TRNG |
| `se050_get_uid()` | `hse_get_chip_id()` | HSE 芯片 ID |
| `se050_open_secure_channel()` | **不需要** | 片载引擎，无通道概念 |

### 4.2 算法类型映射

| SE050 算法 | SE050 值 | HSE 算法 | HSE 值 |
|-----------|---------|---------|--------|
| SM2 | 0x01 | HSE_ALGO_ECC_P256 | 0x01 |
| SM3 | 0x02 | HSE_ALGO_SHA_256 | 0x02 |
| SM4 | 0x03 | HSE_ALGO_AES_256 | 0x03 |
| ECDSA P-256 | 0x04 | HSE_ALGO_ECC_P256 | 0x01 |
| SHA-256 | 0x05 | HSE_ALGO_SHA_256 | 0x02 |
| AES-128 | 0x06 | HSE_ALGO_AES_128 | 0x04 |
| AES-256 | 0x07 | HSE_ALGO_AES_256 | 0x03 |
| HMAC-SHA256 | 0x08 | HSE_ALGO_HMAC_SHA256 | 0x05 |

### 4.3 密钥槽映射

| 用途 | SE050 Slot | HSE Slot |
|------|-----------|----------|
| 主密钥 | 0x00000001 | HSE_KEY_SLOT_MASTER (0x01) |
| 车辆证书 | 0x00000002 | HSE_KEY_SLOT_VEHICLE_CERT (0x02) |
| 固件签名密钥 | 0x00000003 | HSE_KEY_SLOT_FW_SIGN (0x03) |
| ICCE 钥匙基础槽 | 0x10000000 | HSE_KEY_SLOT_ICCE_BASE (0x10000000) |
| CCC 钥匙基础槽 | 0x20000000 | HSE_KEY_SLOT_CCC_BASE (0x20000000) |
| 会话密钥 | 0x30000001 | HSE_KEY_SLOT_SESSION (0x30000001) |

---

## 5. HSE 驱动层设计

### 5.1 统一抽象接口

```c
/**
 * @file secure_element_interface.h
 * @brief 安全元件统一抽象接口
 * @note SE050 和 HSE 都实现此接口，通过编译宏切换
 */
#ifndef SECURE_ELEMENT_INTERFACE_H
#define SECURE_ELEMENT_INTERFACE_H

#ifdef S32K3_HSE
    #include "hse_driver.h"
    #define g_se_driver g_hse_driver
#else
    #include "se050_driver.h"
    #define g_se_driver g_se050_driver
#endif

#endif /* SECURE_ELEMENT_INTERFACE_H */
```

### 5.2 HSE 驱动目录结构

```
embedded/src/drivers/hse/
├── hse_driver.h      # HSE 驱动头文件
├── hse_driver.c      # HSE 驱动实现
├── hse_types.h       # HSE 类型定义
└── hse_config.h      # HSE 配置（可选）
```

### 5.3 HSE 固件服务

HSE 通过 RTD (Real-Time Drivers) 提供的服务访问：

| 服务 | 说明 |
|------|------|
| `HSE_SRV_GET随机数` | 获取真随机数 |
| `HSE_SRV_ECC_GENERATE_KEY` | 生成 ECC 密钥对 |
| `HSE_SRV_ECC_SIGN` | ECC 签名 |
| `HSE_SRV_ECC_VERIFY` | ECC 验签 |
| `HSE_SRV_AES_ENCRYPT` | AES 加密 |
| `HSE_SRV_AES_DECRYPT` | AES 解密 |
| `HSE_SRV_HASH` | 哈希计算 |
| `HSE_SRV_MAC` | 消息认证码 |
| `HSE_SRV_KEY_EXPORT` | 导出公钥 |
| `HSE_SRV_IMPORT_KEY` | 导入密钥 |
| `HSE_SRV_DESTROY_KEY` | 销毁密钥 |
| `HSE_SRV_GET_INFO` | 获取 HSE 信息 |
| `HSE_SRV_BOOT_SCHEME` | 安全启动 |

---

## 6. 代码修改示例

### 6.1 安全通道修改

**原 SE050 代码：**
```c
// S32G2+SE050: 使用 SCP03 建立安全通道
static int se050_open_secure_channel(void) {
    // 生成主机挑战（TRNG）
    uint8_t host_challenge[8];
    se050_generate_random(host_challenge, 8);
    // SCP03 协议...
    return 0;
}
```

**新 HSE 代码：**
```c
// S32K3+HSE: 无需建立安全通道，HSE 片载直接访问
static int hse_init(void) {
    // HSE 固件自检
    int ret = hse_self_test();
    if (ret != 0) {
        return ret;
    }
    // 获取芯片 ID
    uint8_t chip_id[32];
    uint16_t id_len = sizeof(chip_id);
    return hse_get_chip_id(chip_id, &id_len);
}
```

### 6.2 密钥管理修改

**原 SE050 代码：**
```c
// S32G2+SE050: 导入密钥到 SE050
int se050_import_key(se050_object_id_t key_id, se050_algo_t algo,
                     const uint8_t* key_data, uint16_t key_len) {
    // SPI 传输到 SE050...
}
```

**新 HSE 代码：**
```c
// S32K3+HSE: 存储密钥到 HSE NVM
int hse_store_key(hse_key_slot_t key_slot, hse_algo_t algo,
                  const uint8_t* key_data, uint16_t key_len) {
    // 通过 HSE 服务 API 存储...
}
```

### 6.3 条件编译示例

```c
/**
 * @brief 生成随机数
 */
int crypto_engine_random(uint8_t* random, uint16_t len) {
    if (!random || len == 0) return DK_ERR_INVALID_PARAM;

#ifdef S32K3_HSE
    return hse_get_random(random, len);
#else
    return g_se050_driver.generate_random(random, len);
#endif
}

/**
 * @brief 签名
 */
int crypto_engine_sm2_sign(se050_object_id_t key_id,
                            const uint8_t* data, uint16_t data_len,
                            uint8_t* signature, uint16_t* sig_len) {
    if (!data || !signature || !sig_len) return DK_ERR_INVALID_PARAM;

#ifdef S32K3_HSE
    hse_sign_result_t result;
    int ret = hse_ecc_sign(key_id, HSE_ALGO_ECC_P256,
                           data, data_len, &result);
#else
    se050_sign_result_t result;
    int ret = g_se050_driver.sign(key_id, SE050_ALGO_SM2,
                                  data, data_len, &result);
#endif
    if (ret == 0) {
        uint16_t copy_len = (ret == 0) ? result.signature_len : 0;
        if (copy_len > *sig_len) return DK_ERR_NO_MEMORY;
        memcpy(signature, result.signature, copy_len);
        *sig_len = copy_len;
    }
    return ret;
}
```

---

## 7. 构建配置

### 7.1 CMake 选项

```cmake
# S32K3+HSE 构建配置
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-none-eabi.cmake \
    -DTARGET_MCU=S32K3 \
    -DSE_CHIP=HSE \
    -DENABLE_ICCE=ON \
    -DENABLE_CCC=ON
```

### 7.2 编译宏

| 宏 | 说明 |
|------|------|
| `S32K3_HSE` | 启用 S32K3+HSE 平台 |
| `TARGET_S32K3` | 目标为 S32K3 MCU |
| `SE_CHIP_HSE` | 安全芯片为 HSE |
| `HSE_FW_VERSION` | HSE 固件版本 |

### 7.3 头文件包含

```c
// 统一包含安全元件头文件
#include "secure_element_interface.h"
```

---

## 8. 验证清单

### 8.1 功能验证

- [ ] HSE 初始化成功
- [ ] TRNG 随机数生成正常
- [ ] ECC 密钥对生成正常
- [ ] ECC 签名/验签正常
- [ ] AES 加密/解密正常
- [ ] SHA-256 哈希正常
- [ ] 密钥存储/读取/删除正常
- [ ] HSE 芯片 ID 读取正常

### 8.2 安全验证

- [ ] HSE 自检通过
- [ ] 安全启动验证正常
- [ ] 密钥隔离正常（不同槽位密钥不互通）
- [ ] 防回滚计数器正常
- [ ] 会话密钥每次不同

### 8.3 性能验证

- [ ] 加密/解密延迟在可接受范围内
- [ ] 签名/验签延迟在可接受范围内
- [ ] 启动时间符合要求

### 8.4 兼容性验证

- [ ] 与原 S32G2+SE050 分支功能一致
- [ ] ICCE 协议兼容
- [ ] CCC 协议兼容
- [ ] OTA 升级流程正常

---

## 附录 A：快速参考

### A.1 文件清单

**新建文件：**
- `docs/S32K3-MIGRATION-GUIDE.md`（本文件）
- `embedded/src/drivers/hse/hse_driver.h`
- `embedded/src/drivers/hse/hse_driver.c`
- `embedded/src/drivers/hse/hse_types.h`

**修改文件：**
- `docs/ARCHITECTURE.md`（新增第9章）
- `embedded/README.md`
- `README.md`
- `embedded/src/drivers/se/crypto_engine.h`
- `embedded/src/drivers/se/crypto_engine.c`
- `embedded/src/security/secure_channel.c`
- `embedded/src/security/key_manager.c`
- `embedded/src/ota/secure_boot.c`
- `embedded/CMakeLists.txt`

### A.2 关键宏

```c
// S32K3+HSE 编译宏
#define S32K3_HSE           1
#define TARGET_S32K3        1
#define SE_CHIP_HSE        1

// HSE 密钥槽定义
#define HSE_KEY_SLOT_MASTER         0x01
#define HSE_KEY_SLOT_VEHICLE_CERT   0x02
#define HSE_KEY_SLOT_FW_SIGN        0x03
#define HSE_KEY_SLOT_ICCE_BASE      0x10000000
#define HSE_KEY_SLOT_CCC_BASE       0x20000000
#define HSE_KEY_SLOT_SESSION        0x30000001
```

---

*文档版本：v1.0 | 创建日期：2026-04-26 | 作者：arch*
