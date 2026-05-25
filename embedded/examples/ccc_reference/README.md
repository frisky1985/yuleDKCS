# CCC 数字钥匙参考示例 (Reference Implementation)

本示例展示了在 **yuleDKCS** 嵌入式平台上实现 CCC (Car Connectivity Consortium) 数字钥匙协议的完整参考实现。

![MEDIA:/home/admin/yuleDKCS/embedded/examples/ccc_reference/docs/architecture_overview.png](MEDIA:/home/admin/yuleDKCS/embedded/examples/ccc_reference/docs/architecture_overview.png)

> **注意:** 本示例源自 `yuleASR/examples/ccc_digital_key/` 的 CCC 数字钥匙核心示例,
> 已增强整合到 yuleDKCS 中,同时引用 yuleDKCS `ccc_core` 协议栈和 yuleASR BSW 模块。

---

## 快速导航

- [概述](#概述)
- [功能特性](#功能特性)
- [架构说明](#架构说明)
- [文件结构](#文件结构)
- [构建方式](#构建方式)
- [yuleDKCS + yuleASR 集成](#yuledkcs--yuleasr-集成)
- [使用示例](#使用示例)
- [API 对照表](#api-对照表)
- [常见问题](#常见问题)

---

## 概述

CCC 数字钥匙是一种车辆访问技术,允许用户使用智能手机或其他移动设备作为数字钥匙来解锁和启动车辆。
本示例实现了 CCC Digital Key 规范 3.0 中的核心功能,可与 yuleDKCS 的完整协议栈配合使用。

### 三种使用方式

本 README 和示例代码覆盖以下三种使用方式:

| 方式 | 接口 | 适用场景 | 复杂度 |
|:----|:-----|:---------|:------|
| **A: 统一接口** | `dkcs_*()` | 快速集成,应用层开发 | 低 |
| **B: 协议栈直调** | `ccc_*()` | 深度控制,协议定制 | 中 |
| **C: 教学示例** | `Ccc_*()` | 学习协议原理,调试 | 高 (本示例) |

---

## 功能特性

### 1. 密钥协商 (ECDH P-256)
- 使用椭圆曲线密钥交换算法安全地生成共享密钥
- 支持临时密钥对生成
- 符合 NIST P-256 标准

### 2. 身份认证 (ECDSA P-256)
- 基于 X.509 证书的身份验证
- 挑战-响应认证机制
- 证书链验证
- 证书有效期检查

### 3. 加密通信 (AES-128-GCM)
- 认证加密的安全通信
- 消息完整性保护
- 重放攻击防护

### 4. 密钥派生 (HKDF-SHA256)
- 基于 HMAC 的提取和扩展函数
- 从共享密钥派生会话密钥
- 支持多个密钥素材派生

---

## 架构说明

```
┌─────────────────────────────────────────────────────────────────────┐
│                      CCC Reference Application                       │
│  main.c                                                              │
│    ├── DemoPairing()           ─── 配对流程                          │
│    ├── DemoAuthentication()    ─── 认证流程                          │
│    └── DemoSecureChannel()     ─── 安全通信                          │
├─────────────────────────────────────────────────────────────────────┤
│                     CCC Core (本示例)                                 │
│  CccDigitalKey.c      ─── 核心管理 (初始化/模式/会话)                 │
│  CccKeyAgreement.c    ─── 密钥协商 (ECDH/HKDF)                       │
│  CccAuthentication.c  ─── 身份认证 (证书/挑战-响应)                   │
│  CccSecureChannel.c   ─── 安全通信 (AES-GCM/MAC)                     │
├─────────────────────────────────────────────────────────────────────┤
│  yuleDKCS Protocol Stack  │  yuleASR BSW Modules                     │
│  ┌──────────────────────┐ │  ┌─────────────────────────────────┐     │
│  │ ccc.h / ccc_core.c   │ │  │ Csm (加密服务管理)                │     │
│  │ ccc_digital_key.h    │ │  │ Det (开发错误追踪)                │     │
│  │ key_mgmt.c           │ │  │ Os (任务/超时管理)                │     │
│  └──────────────────────┘ │  │ NvM (非易失性存储)                │     │
│  ┌──────────────────────┐ │  │ Dcm (诊断通信管理)                │     │
│  │ BLE (KW47A)          │ │  │ EcuM (ECU状态管理)                │     │
│  │ NFC (ST25R501)       │ │  └─────────────────────────────────┘     │
│  │ UWB (NCJ29D6)        │ │                                          │
│  │ SE050 (安全元件)     │ │                                          │
│  └──────────────────────┘ │                                          │
├─────────────────────────────────────────────────────────────────────┤
│                          Hardware Abstraction                         │
│  NXP KW47A MCU | ST ST25R501 | NXP NCJ29D6 | NXP SE050               │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 文件结构

```
examples/ccc_reference/
├── CMakeLists.txt          # 构建文件 (支持独立编译和子项目模式)
├── README.md               # 本文件
├── include/
│   ├── CccDigitalKey.h     # 主头文件,API 声明
│   ├── CccTypes.h          # CCC 专有类型定义
│   └── CccIntegration.h    # yuleDKCS + yuleASR 集成说明
└── src/
    ├── main.c              # 示例入口,演示完整流程
    ├── CccDigitalKey.c     # 核心实现 (初始化/模式/会话管理)
    ├── CccKeyAgreement.c   # 密钥协商实现 (ECDH/HKDF)
    ├── CccAuthentication.c # 身份认证实现 (证书/挑战-响应)
    └── CccSecureChannel.c  # 安全通信实现 (AES-GCM/MAC)
```

---

## 构建方式

### 方式 1: 作为 yuleDKCS 子项目 (推荐)

```bash
cd yuleDKCS/embedded
mkdir -p build && cd build
cmake .. -DBUILD_EXAMPLES=ON -DENABLE_CCC=ON
make ccc_reference
./examples/ccc_reference/ccc_reference
```

### 方式 2: 独立构建

```bash
cd yuleDKCS/embedded/examples/ccc_reference
mkdir -p build && cd build
cmake .. -DDKCS_ROOT=/path/to/yuleDKCS/embedded
make
./ccc_reference
```

### 方式 3: 带 yuleASR BSW 集成构建

```bash
cd yuleDKCS/embedded/examples/ccc_reference
mkdir -p build && cd build
cmake .. \
    -DDKCS_ROOT=/path/to/yuleDKCS/embedded \
    -DYULE_ASR_ROOT=/path/to/yuleASR
make
```

---

## yuleDKCS + yuleASR 集成

### yuleDKCS ccc_core 协议栈

yuleDKCS 在 `embedded/src/ccc/` 目录下提供了完整的 CCC Digital Key R3 协议栈实现,
包含以下模块:

| 模块 | 路径 | 说明 |
|:----|:-----|:-----|
| 协议栈核心 | `src/ccc/protocol/ccc_core.c` | CCC 协议栈主逻辑 |
| BLE 传输 | `src/ccc/ble/ble_kw47a.c` | NXP KW47A BLE 适配 |
| NFC 传输 | `src/ccc/nfc/` | NFC 通信 (ST25R501) |
| UWB 测距 | `src/ccc/uwb/` | 精确测距 (NCJ29D6) |
| 安全元件 | `src/ccc/security/security.c` | SE050 安全芯片适配 |
| 密钥管理 | `src/ccc/keymgmt/key_mgmt.c` | 数字钥匙生命周期管理 |

**公共头文件:** `embedded/include/ccc.h` 和 `embedded/include/ccc/ccc_digital_key.h`

### yuleASR BSW 模块集成

本示例通过以下方式调用 yuleASR BSW 服务:

| BSW 模块 | 头文件 | 使用场景 | 在示例中的位置 |
|:---------|:-------|:---------|:--------------|
| **CSM** | `Csm.h` | 密钥生成、签名、加解密、哈希、MAC | CccDigitalKey.c, CccKeyAgreement.c, CccAuthentication.c, CccSecureChannel.c |
| **Det** | `Det.h` | 开发阶段错误追踪与报告 | CccDigitalKey.c |
| **OS** | `Os.h` | 任务创建、定时器管理、中断处理 | 示例注释中说明集成方式 |
| **NvM** | `NvM.h` | 钥匙和证书的持久化存储 | 示例注释中说明集成方式 |
| **DCM** | `Dcm.h` | 诊断服务、安全访问、钥匙状态查询 | 示例注释中说明集成方式 |
| **EcuM** | `EcuM.h` | ECU 唤醒管理、运行状态切换 | 示例注释中说明集成方式 |

### 在 yuleDKCS 中使用 yuleASR 的典型配置

```c
/* 顶层 CMakeLists.txt 中添加 yuleASR 头文件路径 */
target_include_directories(your_target PRIVATE
    ${YULE_ASR_ROOT}/src/bsw/csm     /* CSM 头文件 */
    ${YULE_ASR_ROOT}/src/bsw/det     /* Det 头文件 */
    ${YULE_ASR_ROOT}/src/bsw/os      /* OS 头文件 */
    ${YULE_ASR_ROOT}/src/bsw/nvm     /* NvM 头文件 */
    ${YULE_ASR_ROOT}/src/bsw/dcm     /* DCM 头文件 */
    ${YULE_ASR_ROOT}/src/bsw/ecum    /* EcuM 头文件 */
)

/* 编译定义 */
add_definitions(-DCCC_USE_CSM=STD_ON)
add_definitions(-DCCC_DEV_ERROR_DETECT=STD_ON)
add_definitions(-DCCC_VERSION_INFO_API=STD_ON)
```

### 集成代码示例

```c
/* ========================================================================
 * 完整的 yuleDKCS + yuleASR 集成初始化流程
 * ======================================================================== */

#include "dkcs.h"           /* yuleDKCS 统一接口 */
#include "ccc.h"            /* yuleDKCS CCC 协议栈 */
#include "Csm.h"            /* yuleASR CSM */
#include "Det.h"            /* yuleASR Det */
#include "Os.h"             /* yuleASR OS */
#include "NvM.h"            /* yuleASR NvM */

/* 1. 配置会话 */
session_config_t config = {
    .protocol = PROTOCOL_CCC,
    .se_type = SE_ESE,             /* NXP SE050 安全元件 */
    .timeout_ms = 5000,
    .enable_mitm_protection = true,
    .enable_replay_protection = true,
};

/* 2. 初始化 DKCS 统一接口 */
error_t err = dkcs_init(&config);
if (err != OK) {
    Det_ReportError(CCC_MODULE_ID, 0, CCC_API_INIT, err);
    return err;
}

/* 3. 通过 yuleASR NvM 读取已存储的钥匙 */
ccc_digital_key_t stored_key;
NvM_ReadBlock(NVM_BLOCK_CCC_KEY, (uint8*)&stored_key, sizeof(stored_key));

/* 4. 初始化 yuleDKCS CCC 协议栈 */
const ccc_se_interface_t se_if = {
    .init = sec_init,
    .sign = sec_sign,
    .verify = sec_verify,
    .derive_shared_secret = sec_ecdh,
    .secure_store = sec_store,
    .secure_read = sec_read,
};
err = ccc_init(&se_if);

/* 5. 创建 yuleASR OS 任务 */
Os_CreateTask(TASK_CCC_PROCESS, CccProcessTask, 10);
```

---

## 使用示例

### 初始化

```c
#include "CccDigitalKey.h"

void main(void)
{
    Ccc_ReturnType result;

    /* 初始化 CCC 模块 */
    result = Ccc_Init(&cccConfig);
    if (result != CCC_E_OK) {
        /* 处理错误 */
    }

    /* ... 应用代码 ... */

    /* 去初始化 */
    Ccc_DeInit();
}
```

### 配对流程

```c
/* 开始配对 */
uint8 localPublicKey[CCC_ECC_P256_PUBLIC_KEY_SIZE];
uint32 publicKeyLength = sizeof(localPublicKey);
Ccc_DeviceIdType mobileDevice;

result = Ccc_PairingStart(&mobileDevice, localPublicKey, &publicKeyLength);
if (result != CCC_E_OK) {
    /* 处理错误 */
}

/* 发送 localPublicKey 到移动设备 */

/* 接收远程公钥和证书后完成配对 */
Ccc_CertificateType remoteCert;
/* ... 接收远程证书 ... */

result = Ccc_PairingComplete(remotePublicKey, remotePublicKeyLength, &remoteCert);
if (result != CCC_E_OK) {
    /* 处理错误 */
}
```

### 建立安全会话

```c
/* 建立会话 */
result = Ccc_SessionEstablish(
    TRUE,  /* 作为发起方 */
    remotePublicKey,
    remotePublicKeyLength
);

if (result != CCC_E_OK) {
    /* 处理错误 */
}

/* 现在可以进行安全通信 */
```

### 安全通信

```c
/* 发送安全消息 */
uint8 plaintext[] = "Unlock command";
uint8 ciphertext[256];
uint32 ciphertextLength = sizeof(ciphertext);
uint8 authTag[CCC_AES_TAG_SIZE];

result = Ccc_EncryptMessage(
    plaintext,
    sizeof(plaintext),
    ciphertext,
    &ciphertextLength,
    authTag
);

if (result == CCC_E_OK) {
    /* 发送密文和认证标签 */
}

/* 接收并解密消息 */
uint8 decrypted[256];
uint32 decryptedLength = sizeof(decrypted);

result = Ccc_DecryptMessage(
    receivedCiphertext,
    receivedCiphertextLength,
    receivedAuthTag,
    decrypted,
    &decryptedLength
);

if (result == CCC_E_OK) {
    /* 处理解密后的数据 */
}
```

---

## API 对照表

| 本示例 (教学用途) | yuleDKCS ccc_core (生产级) | yuleDKCS 统一接口 (推荐) |
|:-----------------|:--------------------------|:------------------------|
| `Ccc_Init()` | `ccc_init()` | `dkcs_init()` |
| `Ccc_DeInit()` | `ccc_deinit()` | `dkcs_deinit()` |
| `Ccc_PairingStart()` | `ccc_create_pairing_session()` + `ccc_start_pairing()` | `dkcs_pairing_start()` |
| `Ccc_PairingComplete()` | `ccc_process_pairing_response()` + `ccc_complete_pairing()` | (回调) |
| `Ccc_AuthenticationStart()` | `ccc_generate_challenge()` | (内部自动) |
| `Ccc_AuthenticationComplete()` | 挑战-响应组合流程 | (内部自动) |
| `Ccc_SessionEstablish()` | `ccc_establish_session()` | (内部自动) |
| `Ccc_EncryptMessage()` | `ccc_encrypt_message()` | `dkcs_vehicle_unlock()` |
| `Ccc_DecryptMessage()` | `ccc_decrypt_message()` | (内部自动) |
| `Ccc_VerifyCertificate()` | `ccc_verify_certificate()` | (内部自动) |
| `Ccc_GenerateRandom()` | `ccc_generate_challenge()` | `sec_attestation()` |
| `Ccc_SignData()` | `sec_sign()` | (内部自动) |
| `Ccc_VerifySignature()` | `sec_verify()` | (内部自动) |

---

## 错误处理

示例中定义了以下错误码:

| 错误码 | 说明 | 对应 yuleDKCS 错误码 |
|:-------|:-----|:--------------------|
| CCC_E_OK | 成功 | `CCC_OK` |
| CCC_E_NOT_INITIALIZED | 未初始化 | `CCC_ERR_NOT_INIT` |
| CCC_E_CRYPTO_FAILURE | 加密操作失败 | `ERROR_CRYPTO_FAILURE` |
| CCC_E_KEY_INVALID | 密钥无效 | `CCC_ERR_DENIED` |
| CCC_E_CERT_INVALID | 证书无效 | `CCC_ERROR_CERT_INVALID` |
| CCC_E_SIGNATURE_INVALID | 签名无效 | `CCC_ERROR_SIGNATURE_INVALID` |
| CCC_E_AUTHENTICATION_FAILED | 认证失败 | `ERROR_AUTH_FAILURE` |
| CCC_E_SESSION_NOT_ESTABLISHED | 会话未建立 | `CCC_ERROR_SESSION_ESTABLISH_FAILED` |
| CCC_E_REPLAY_DETECTED | 检测到重放攻击 | (内置重放防护) |
| CCC_E_MESSAGE_INVALID | 消息无效 | `CCC_ERROR_INVALID_MESSAGE` |

---

## 安全考虑

### 密钥管理
- 所有敏感密钥数据在使用后立即清除
- 通过 NXP SE050 硬件安全元件存储长期密钥
- 密钥定期更新机制

### 重放防护
- 使用序列号防止重放攻击
- 支持 32 位重放防护窗口
- 检测到重放攻击时立即关闭会话

### 会话管理
- 会话有效期限制 (5 分钟)
- 定期心跳检测
- 异常情况自动关闭会话

---

## 注意事项

1. **本示例用于教学和参考目的**, 生产环境中应使用 yuleDKCS 的 `ccc_core` 生产级协议栈
2. 本示例中的加密操作既展示通过 yuleASR CSM API 调用,也提供无 CSM 的模拟实现 (`CCC_USE_CSM=STD_OFF`)
3. 实际产品推荐使用方式 A (yuleDKCS 统一接口 `dkcs_*()` ) 进行应用开发
4. 密钥存储应使用硬件安全存储区域 (NXP SE050 / SCP03 安全通道)
5. 所有加密操作可经由 yuleASR CSM 服务层调度,确保硬件独立性
6. 通过 yuleASR OS 的任务管理可实现多优先级调度
7. 钥匙持久化应通过 yuleASR NvM 管理,支持 CRC 校验和写计数器

---

## 参考资料

- [CCC Digital Key Specification 3.0](https://carconnectivity.org/)
- [yuleDKCS 文档](/docs/)
- [yuleASR BSW 文档](/docs/bsw/)
- [NIST SP 800-56A - Recommendation for Pair-Wise Key Establishment Schemes](https://csrc.nist.gov/publications/detail/sp/800-56a/rev-3/final)
- [NIST SP 800-38D - Recommendation for Block Cipher Modes of Operation: GCM](https://csrc.nist.gov/publications/detail/sp/800-38d/final)
- [RFC 5869 - HMAC-based Extract-and-Expand Key Derivation Function (HKDF)](https://tools.ietf.org/html/rfc5869)

---

## 许可

Copyright (c) 2024-2026 上海予乐电子科技有限公司
保留所有权利。
