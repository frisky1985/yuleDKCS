# yuleDKCS 量产制造计划

> **版本**：v1.0
> **日期**：2026-07-29
> **状态**：初版
> **适用对象**：生产制造团队、质量工程团队、固件工程团队

---

## 目录

1. [概述与范围](#1-概述与范围)
2. [固件生产编译与构建](#2-固件生产编译与构建)
3. [固件签名与安全交付](#3-固件签名与安全交付)
4. [版本管理策略](#4-版本管理策略)
5. [多平台并行构建策略](#5-多平台并行构建策略)
6. [设备预置 (Provisioning)](#6-设备预置-provisioning)
7. [安全元件 (SE) 初始化](#7-安全元件-se-初始化)
8. [PKI 证书签发与设备身份管理](#8-pki-证书签发与设备身份管理)
9. [烧录工具链](#9-烧录工具链)
10. [工厂测试流程](#10-工厂测试流程)
11. [MES 系统对接](#11-mes-系统对接)
12. [测试数据采集与良率管理](#12-测试数据采集与良率管理)
13. [附录](#13-附录)

---

## 1. 概述与范围

### 1.1 文档目标

本文档定义 yuleDKCS 数字钥匙产品从固件编译、签名、版本管理到设备预置、工厂测试的全链路量产制造流程。确保产品在百万级出货量下的一致性、可追溯性和质量稳定性。

### 1.2 产品范围

| 组件 | 描述 | 协议栈 |
|------|------|--------|
| 车辆嵌入式固件 (TCU) | 主控 MCU 固件 (NXP S32K312) | CCC 3.0 / ICCOA 2.0 / ICCE 1.5 |
| BLE 模块固件 | NXP KW47A BLE 5.0 通信固件 | CCC BLE GATT Profile |
| NFC 模块固件 | ST ST25R501 NFC 读写器固件 | ISO 14443 A/B |
| UWB 模块固件 | NXP NCJ29D6 UWB 测距固件 | FiRa MAC / IEEE 802.15.4z |
| 安全元件固件 | NXP SE050 安全芯片固件 | GlobalPlatform SCP03 |

### 1.3 工艺流程图

```
┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
│   SMT 贴片组装   │───▶│   PCBA 测试      │───▶│   固件烧录       │
│   (PCBA)         │    │   (Flying Probe) │    │   (Firmware Flash)│
└──────────────────┘    └──────────────────┘    └──────────────────┘
                                                        │
                                                        ▼
┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
│   包装出货       │◀───│   最终检验 (OQC)  │◀───│   老化与系统测试 │
│   (Packing)      │    │   (Final QC)     │    │   (Burn-in & SIT)│
└──────────────────┘    └──────────────────┘    └──────────────────┘
```

---

## 2. 固件生产编译与构建

### 2.1 构建系统概览

yuleDKCS 采用 **CMake + Makefile** 多层构建系统，支撑三套协议栈的并行编译。

#### 2.1.1 嵌入式固件构建

| 协议栈 | 构建目标 | 编译器 | 构建配置 |
|--------|----------|--------|----------|
| CCC | `ccc_dk` (静态库) | ARM GCC 10.x / IAR EWARM | `embedded/ccc_protocol/CMakeLists.txt` |
| ICCOA | `iccoa_dk` (静态库) | ARM GCC 10.x | `embedded/iccoa_protocol/CMakeLists.txt` |
| ICCE | `icce_dk` (静态库) | ARM GCC 10.x | `embedded/icce_protocol/CMakeLists.txt` |
| Unified | `dk_unified` (统一库) | ARM GCC 10.x | `embedded/unified_protocol/CMakeLists.txt` |

#### 2.1.2 生产构建命令

```bash
# 生产 Release 构建
mkdir build-release && cd build-release
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DTOOLCHAIN_PREFIX=/opt/gcc-arm-10.3.2021.10-x86_64-arm-none-eabi \
  -DENABLE_TESTS=OFF \
  -DENABLE_DEBUG_LOG=OFF \
  -DUSE_SM_CRYPTO=ON           # ICCE 协议栈启用国密算法
cmake --build . -j$(nproc)

# 构建输出产物
# build-release/firmware/bootloader.bin        # 安全启动引导程序
# build-release/firmware/s32k312_firmware.bin     # 主固件
# build-release/firmware/ccc_protocol.bin      # CCC 协议栈固件
# build-release/firmware/iccoa_protocol.bin    # ICCOA 协议栈固件
# build-release/firmware/icce_protocol.bin     # ICCE 协议栈固件
# build-release/firmware/manifest.bin          # 固件签名清单
```

#### 2.1.3 Docker 容器化构建

```bash
# 使用 Dockerfile 进行可重现构建
docker build -t yuledkcs/firmware-builder:1.0 -f Dockerfile .

# 提取构建产物
docker run --rm -v $(pwd)/output:/output yuledkcs/firmware-builder:1.0 \
  /bin/sh -c "cp /app/build/firmware/*.bin /output/"
```

### 2.2 构建校验与完整性检查

| 检查项 | 方法 | 通过标准 |
|--------|------|----------|
| 编译器版本校验 | `arm-none-eabi-gcc --version` 对比 manifest | 版本号完全匹配 |
| 编译无警告 | `-Wall -Wextra -Werror` | 零 Warning |
| 二进制大小检查 | `size <binary>` 对比基线 | 偏差 < 5% |
| 构建哈希校验 | SHA-256 产物哈希 | 与 CI manifest 一致 |
| 链接符号检查 | `nm <binary>` 检查关键符号 | 所有预期符号存在 |

---

## 3. 固件签名与安全交付

### 3.1 签名密钥体系

```
┌─────────────────────────────────────────────────────────────┐
│                   固件签名密钥层级                            │
│                                                              │
│  Root CA Key (离线冷存储, HSM 保护)                          │
│      │                                                       │
│      ├── Firmware Signing Key (FSK)                          │
│      │       ├── CCC 固件签名子密钥                          │
│      │       ├── ICCOA 固件签名子密钥                        │
│      │       └── ICCE 固件签名子密钥                         │
│      │                                                       │
│      ├── Bootloader Signing Key (BSK)                        │
│      │       └── 预烧录至 SE050 (Key ID 0x0001)             │
│      │                                                       │
│      └── OEM 证书签发密钥                                    │
│              └── 设备身份证书签发                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 签名流程

```bash
# 1. 计算固件哈希
sha256sum s32k312_firmware.bin > s32k312_firmware.sha256

# 2. 使用 HSM 签名
# 生产环境使用 Thales Luna / AWS CloudHSM
pkcs11-tool --module /usr/lib/librainbow.so \
  --slot 0 --id 01 \
  --sign --mechanism ECDSA \
  --input-file s32k312_firmware.sha256 \
  --output-file s32k312_firmware.sig

# 3. 生成签名清单 manifest
# manifest.bin 包含: 固件哈希 + 签名 + 版本号 + 签名证书链
generate_manifest \
  --firmware s32k312_firmware.bin \
  --signature s32k312_firmware.sig \
  --version 2.1.0 \
  --cert-chain firmware_signing_chain.pem \
  --output manifest.bin
```

### 3.3 签名验证（SE050 端）

```c
// 安全启动时 SE050 验证固件签名
// 引用: SECURITY_WHITEPAPER.md §4.2
secure_boot_status_t verify_firmware_signature(
    const uint8_t *firmware,
    size_t         fw_len,
    const uint8_t *signature,
    size_t         sig_len
) {
    uint8_t hash[32];
    crypto_sha256(firmware, fw_len, hash);

    // SE050 使用预烧录公钥 (Key ID 0x0001) 验签
    se05x_status_t status = SE05x_ECDSASignVerify(
        KEY_ID_BOOT_PUBLIC,
        kSE05x_Algorithm_ECDSA_SHA256,
        hash, 32,
        signature, sig_len
    );
    return (status == kSE05x_Status_Success)
        ? SECURE_BOOT_SUCCESS
        : SECURE_BOOT_SIGNATURE_INVALID;
}
```

### 3.4 签名安全管理

| 要求 | 说明 |
|------|------|
| HSM 签名密钥 | 存储在 FIPS 140-2 Level 3 认证的 HSM 中 |
| 多重签名策略 | 生产固件要求 ≥ 2 of 3 签名 (OEM / yuleDKCS / 审计方) |
| 密钥轮换 | 固件签名密钥每 12 个月轮换一次 |
| 访问控制 | 签名操作需要 2 人授权 (4-eyes principle) |
| 审计日志 | 每次签名操作记录至不可篡改审计日志 |

---

## 4. 版本管理策略

### 4.1 版本号规范

遵循 **SemVer 2.0** + 协议栈标识：

```
<主版本>.<次版本>.<补丁>-[协议标识][构建号]
示例: 2.1.0-CCC.20260729.001
      2.1.0-ICCOA.20260729.001
      2.1.0-ICCE.20260729.001
```

| 段 | 说明 |
|----|------|
| 主版本 | API/协议不兼容变更 |
| 次版本 | 向后兼容的功能新增 |
| 补丁 | 向后兼容的 Bug 修复 |
| 协议标识 | CCC / ICCOA / ICCE |
| 构建号 | 日期 + 当日构建序号 |

### 4.2 版本追溯（SBOM + Git Tag）

```bash
# Git Tag 关联
git tag -a v2.1.0-CCC.20260729 -m "CCC firmware v2.1.0 production release"

# 生成软件物料清单 (SBOM)
cd embedded
cyclonedx-bom -o sbom-ccc-v2.1.0.json \
  --component ccc_protocol \
  --version 2.1.0 \
  --output-format json

# SBOM 内容
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.4",
  "version": 1,
  "components": [
    { "name": "ccc_protocol", "version": "2.1.0",
      "hashes": [{"alg": "SHA-256", "content": "a1b2c3..."}],
      "licenses": [{"license": {"id": "Apache-2.0"}}]
    },
    { "name": "FreeRTOS", "version": "202406.01", ... },
    { "name": "mbedTLS", "version": "3.6.0", ... }
  ]
}
```

### 4.3 版本验证（SE050 防回滚）

| 组件 | 版本计数器 | 存储位置 | 回滚行为 |
|------|-----------|----------|----------|
| BootLoader | 8-bit monotonic | SE050 NV counter | 固件版本 < 计数器 → HALT |
| TFM | 16-bit monotonic | SE050 NV counter | 固件版本 < 计数器 → HALT |
| Application | 32-bit monotonic | SE050 NV counter | 固件版本 < 计数器 → HALT + 通知 |

---

## 5. 多平台并行构建策略

### 5.1 三协议栈构建矩阵

```
┌────────────────────────────────────────────────────┐
│               CI/CD 并行构建流水线                   │
├────────────────────────────────────────────────────┤
│                                                      │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐     │
│  │ CCC 协议栈 │  │ICCOA 协议栈│  │ ICCE 协议栈│     │
│  │            │  │            │  │            │     │
│  │ ICC 3.0    │  │ICCOA 2.0  │  │T/CA 110-2020│    │
│  │ ECDSA P-256│  │ECDSA P-256│  │SM2 国密    │     │
│  │ AES-256-GCM│  │AES-256-GCM│  │SM4-GCM     │     │
│  └──────┬─────┘  └──────┬─────┘  └──────┬─────┘     │
│         │               │               │            │
│         └───────────────┼───────────────┘            │
│                         ▼                            │
│              ┌────────────────────┐                  │
│              │  Unified 固件打包   │                  │
│              │  协议路由 + 统一API │                  │
│              └────────────────────┘                  │
└──────────────────────────────────────────────────────┘
```

### 5.2 构建参数配置表

| 参数 | CCC | ICCOA | ICCE |
|------|-----|-------|------|
| 加密算法 | ECDSA P-256 / AES-256 | ECDSA P-256 / AES-256 | SM2 / SM3 / SM4 |
| GATT UUID | 0xFFD1 (CCC DK) | 0xFFE0 (ICCOA DK) | 0xFFF0 (ICCE DK) |
| NFC AID | D2760000850101 | D2760000850102 | D2760000850103 |
| UWB 通道 | CH5 (6.5 GHz) | CH9 (8.0 GHz) | CH5 + CH9 |
| 安全通道 | ECDH + HKDF | ECDH + HKDF | SM2 ECDH + HKDF |

### 5.3 构建产物管理

```bash
# 并行构建配置 (Makefile)
# 参考: embedded/tests/Makefile 的并行编译模式

build-all:
	@echo "=== 并行构建所有协议栈 ==="
	$(MAKE) -j3 build-ccc build-iccoa build-icce

build-ccc:
	cd build/ccc && cmake ../../ccc_protocol && make -j$(nproc)

build-iccoa:
	cd build/iccoa && cmake ../../iccoa_protocol && make -j$(nproc)

build-icce:
	cd build/icce && cmake ../../icce_protocol && make -j$(nproc)
```

---

## 6. 设备预置 (Provisioning)

### 6.1 预置流程图

```
┌─────────────────────────────────────────────────────────────┐
│                     设备预置流程                             │
│                                                              │
│  步骤 1: SMT 组装                                           │
│  步骤 2: PCBA 飞针测试                                      │
│  步骤 3: 固件烧录 (BootLoader + Application)                │
│  步骤 4: SE050 初始化 (注入 Root Key / 设备证书)              │
│  步骤 5: 产线功能测试                                        │
│  步骤 6: 通信测试 (BLE/UWB/NFC RF 校准)                     │
│  步骤 7: 老化测试 (48h)                                      │
│  步骤 8: 最终检验 (OQC)                                      │
│  步骤 9: 包装出货                                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. 安全元件 (SE) 初始化

### 7.1 SE050 初始化流程

```bash
# 1. 物理连接校验
JLinkExe -device S32K312 -if SWD -speed 4000
# 检测 SE050 I2C 通信
SE05x_Detect() → OK

# 2. 注入 Root Key (由 NXP 安全工厂预置或在产线安全环境注入)
#   生产环境使用 HSM 生成 Root Key 并通过加密通道注入 SE050
SE05x_WriteKey(
    KEY_ID_ROOT,       // 0x0001
    rootKeyData,       // 256-bit AES
    ROOT_KEY_LEN,
    SE05X_KEY_TYPE_AES_256
)

# 3. 派生 Master Key
uint8_t masterKey[32];
HKDF_SHA256(rootKey, "yuledkcs-master-key-v1", NULL, masterKey);
SE05x_DeriveKey(KEY_ID_MASTER, masterKey, SE05X_KEY_TYPE_AES_256);

# 4. 注入 Boot 公钥 (固件签名验证用)
SE05x_WritePublicKey(
    KEY_ID_BOOT_PUBLIC,     // 0x0002
    bootPubKey,             // ECDSA P-256 公钥
    PUBLIC_KEY_LEN
)
```

### 7.2 安全注入要求

| 项目 | 要求 |
|------|------|
| 注入环境 | ISO 7级洁净室 + 防静电工作台 |
| 网络隔离 | 注入设备网络与产线网络物理隔离 |
| 密钥传输 | 密钥通过 HSM 加密传输，永不解密至主机内存 |
| 注入记录 | 每次注入记录 SE050 唯一序列号 + 密钥哈希 |
| 防重放 | 每次注入使用唯一随机挑战 (nonce) |

### 7.3 SE050 状态验证

```c
// 产线验证脚本
typedef struct {
    uint8_t  se_serial[16];       // SE050 芯片唯一序列号
    uint8_t  root_key_hash[32];   // Root Key 哈希 (用于验证)
    uint32_t free_key_slots;      // 剩余可用密钥槽数量
    uint8_t  boot_pubkey_hash[32];// Boot 公钥哈希
} SEProvisioningStatus;

// 验证 SE050 预置正确性
bool verify_se_provisioning(void) {
    SEProvisioningStatus status;
    SE05x_GetStatus(&status);

    // 检查 Root Key 存在性
    if (status.root_key_hash == EXPECTED_ROOT_KEY_HASH &&
        status.boot_pubkey_hash == EXPECTED_BOOT_PUBKEY_HASH) {
        return true;
    }
    return false;
}
```

---

## 8. PKI 证书签发与设备身份管理

### 8.1 证书体系

```
┌──────────────────────────────────────────────────┐
│            yuleDKCS PKI 证书层级                   │
│                                                    │
│  Root CA (离线存储, 密钥在 HSM 中)                 │
│    ├── OEM CA (每家 OEM 独立签发子 CA)             │
│    │     ├── 设备身份证书 (Device Identity Cert)   │
│    │     │     ├── TCU 设备证书                     │
│    │     │     ├── SE050 身份证书                   │
│    │     │     └── BLE 模块证书                     │
│    │     ├── 固件签名证书                          │
│    │     └── OTA 签名证书                          │
│    │                                                │
│    └── 云端服务证书                                │
│          ├── API Gateway 证书                       │
│          ├── Hub Service 证书                       │
│          └── gRPC mTLS 证书                         │
└──────────────────────────────────────────────────┘
```

### 8.2 设备身份证书签发

```bash
# 1. 设备生成密钥对 (在 SE050 内部)
SE05x_GenerateKeyPair(KEY_ID_DEVICE_ECC, kSE05x_Algorithm_ECDSA_P256);
SE05x_GetPublicKey(KEY_ID_DEVICE_ECC, devicePubKey);

# 2. 将公钥提交至签发服务
curl -X POST https://pki.yuledkcs.com/v1/certificates/enroll \
  -H "Authorization: Bearer $PROVISIONING_TOKEN" \
  -d '{
    "device_id": "TCU-SN-20260729-00001",
    "public_key": "'$(base64 devicePubKey)'",
    "cert_type": "device_identity",
    "oem_id": "OEM_A"
  }'

# 3. 签发服务返回设备证书
{
  "certificate": "-----BEGIN CERTIFICATE-----\n...",
  "ca_chain": ["-----BEGIN CERTIFICATE-----\n..."],
  "valid_from": "2026-07-29T00:00:00Z",
  "valid_until": "2027-07-29T00:00:00Z"
}

# 4. 将证书导入 SE050
SE05x_WriteCertificate(KEY_ID_DEVICE_CERT, certData, certLen);
```

### 8.3 序列号 / 设备 ID 管理

| 标识符 | 格式 | 长度 | 说明 |
|--------|------|------|------|
| 设备序列号 (SN) | `YULE-{OEM}-{YYYYMMDD}-{NNNNNN}` | 24 chars | 产线唯一标识 |
| 设备 ID (UUID) | UUID v4 | 36 chars | 云端设备身份 |
| SE050 芯片 ID | NXP 出厂序列号 | 16 bytes | 硬件身份锚点 |
| BLE MAC 地址 | 48-bit IEEE MAC | 6 bytes | 蓝牙通信标识 |

### 8.4 设备注册上云

```bash
# 产线完成时，设备注册至云端
curl -X POST https://api.yuledkcs.com/v1/devices/register \
  -H "Authorization: Bearer $PROVISIONING_TOKEN" \
  -d '{
    "serial_number": "YULE-OEM_A-20260729-000001",
    "se_id": "a1b2c3d4e5f6...",
    "ble_mac": "AA:BB:CC:DD:EE:FF",
    "firmware_version": "2.1.0-CCC.20260729.001",
    "manufacturing_date": "2026-07-29",
    "oem_id": "OEM_A",
    "protocol_stack": ["CCC", "ICCOA", "ICCE"]
  }'
```

---

## 9. 烧录工具链

### 9.1 烧录方案对比

| 方案 | 接口 | 速度 | 适用阶段 | 工具 |
|------|------|------|----------|------|
| J-Link / J-Flash | SWD/JTAG | 4 MHz | 开发/小批量 | JLinkExe, J-Flash SPI |
| Lauterbach TRACE32 | JTAG/SWD | 50 MHz | 调试/量产 | Trace32 |
| 离线编程器 | Socket | 并行8路 | 大批量量产 | Dedicated Gang Programmer |
| 边界扫描 (JTAG) | IEEE 1149.1 | 并行 | PCBA 在线烧录 | JTAG 链 |
| UART DFU | UART | 115200 bps | OTA / 售后 | dfu-util |

### 9.2 推荐量产方案

**方案：J-Link + 离线编程器混合方案**

```bash
# 小批量烧录 (< 1000 片/批)
# 使用 J-Link Commander 脚本自动烧录
cat > flash.jlink << 'EOF'
device S32K312
si SWD
speed 4000
connect
loadbin bootloader.bin 0x00000000
loadbin s32k312_firmware.bin 0x00080000
verifybin bootloader.bin 0x00000000
verifybin s32k312_firmware.bin 0x00080000
reset
go
exit
EOF

JLinkExe -CommanderScript flash.jlink

# 大批量量产 (> 10000 片/批)
# 使用离线编程器 (支持 8 路并行烧录)
# 1. 将固件加载到编程器缓存
gang-programmer load --device S32K312 --firmware s32k312_firmware.bin
# 2. 批量烧录 (8 路并行)
gang-programmer flash --batch-size 8 --count 10000
```

### 9.3 烧录校验

| 校验项 | 方法 | 标准 |
|--------|------|------|
| 固件完整性 | SHA-256 校验 | 与 manifest 一致 |
| SE050 状态 | 读取 SE050 状态寄存器 | 全部初始化完成 |
| 签名验证 | SE050 内部验证 BootLoader 签名 | 验证通过 |
| 启动测试 | 上电后检查 BootLoader 执行日志 | 成功进入 App |
| 证书有效性 | 验证设备证书链 | 证书链完整 |

---

## 10. 工厂测试流程

### 10.1 全流程测试阶段

```
┌─────────────────────────────────────────────────────────────────┐
│                    产线测试流水线                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  阶段 1: 功能测试 (FT1)                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ · MCU 启动测试       · SE050 通信测试                   │    │
│  │ · 电源轨电压检测      · 时钟频率测量                    │    │
│  │ · Flash 读写验证      · RAM 读写验证                   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              ▼                                   │
│  阶段 2: 通信测试 (CT)                                           │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ · BLE 发射功率校准    · BLE 接收灵敏度测试              │    │
│  │ · NFC 场强测试        · NFC 读写距离测试               │    │
│  │ · UWB 测距精度校准    · UWB 通道质量测试               │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              ▼                                   │
│  阶段 3: 老化测试 (Burn-in)                                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ · 48 小时连续运行      · 温度循环 ( -20°C ~ +70°C )     │    │
│  │ · 功耗监测             · 1000 次解锁/锁车循环           │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              ▼                                   │
│  阶段 4: 最终检验 (OQC)                                         │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ · 外观检查             · 标签/序列号一致性              │    │
│  │ · 固件版本确认         · 证书有效期校验                 │    │
│  │ · 包装完整性检查       · 文档/配件完整性                │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### 10.2 详细测试项

#### 10.2.1 功能测试 (FT1)

| 测试项 | 测试方法 | 通过标准 | 时间 |
|--------|----------|----------|------|
| MCU 启动 | 上电后读取启动状态寄存器 | 启动正常 | 1s |
| SE050 通信 | I2C 扫描 SE050 地址 | 设备应答 | 1s |
| 电源电压 | ADC 读取各电源轨 | 3.3V ± 5% | 1s |
| Flash 校验 | 读取已烧录固件 SHA-256 | 与 manifest 一致 | 5s |
| 晶振频率 | 频率计数器测量 | ± 50ppm | 2s |
| CAN 通信 | CAN 回环测试 | TX/RX 正常 | 3s |
| GPIO 测试 | 遍历测试 GPIO 输入输出 | 全部正常 | 2s |

#### 10.2.2 通信测试 (CT)

| 测试项 | 设备 | 方法 | 通过标准 |
|--------|------|------|----------|
| BLE 发射功率 | 频谱分析仪 | 测量各信道 TX 功率 | -20 ~ +4 dBm |
| BLE RX 灵敏度 | BLE 测试仪 | BER 测试 @ -70dBm | BER < 0.1% |
| NFC 场强 | NFC 分析仪 | 测量 13.56MHz 场强 | ≥ 1.5 A/m (rms) |
| NFC 读写距离 | NFC 测试卡 | 读取距离测试 | ≥ 20mm |
| UWB 测距精度 | UWB 参考锚点 | 测量 1m/5m/10m 距离 | 误差 < 10cm |
| UWB 信道质量 | 频谱分析仪 | CH5/CH9 信道质量 | SNR ≥ 20dB |

#### 10.2.3 老化测试 (Burn-in)

| 项目 | 条件 | 时长 | 监控指标 |
|------|------|------|----------|
| 温度循环 | -20°C → +70°C, 4 cycles | 24h | 功能正常 |
| 解锁循环 | 每分钟 1 次 | 1000 次 | 成功率 ≥ 99.9% |
| BLE 连接稳定性 | 每秒连接/断开 | 500 次 | 成功率 ≥ 99% |
| 功耗监测 | 各状态测量 | 48h | 符合规格 |

---

## 11. MES 系统对接

### 11.1 数据接口

```json
// MES → 产线测试系统: 批次信息
{
  "batch_id": "BATCH-20260729-001",
  "oem_id": "OEM_A",
  "product_type": "TCU-DKCS-V2",
  "quantity": 5000,
  "firmware_version": "2.1.0-CCC.20260729.001",
  "provisioning_config": {
    "root_key_source": "HSM-01",
    "cert_profile": "device_identity_v2"
  },
  "test_spec": "TS-CCC-2.1.0"
}

// 产线测试系统 → MES: 设备测试结果
{
  "serial_number": "YULE-OEM_A-20260729-000001",
  "batch_id": "BATCH-20260729-001",
  "test_results": {
    "ft1": {"status": "PASS", "timestamp": "2026-07-29T10:00:00Z"},
    "ct":  {"status": "PASS", "timestamp": "2026-07-29T10:05:00Z",
            "ble_tx_power": -2.5, "nfc_field_strength": 2.1},
    "burn_in": {"status": "PASS", "cycles": 1000, "failures": 0},
    "oqc": {"status": "PASS", "inspector": "QC-007"}
  },
  "final_status": "PASS",
  "defects": []
}
```

### 11.2 MES 关键数据字段

| 字段 | 类型 | 说明 |
|------|------|------|
| serial_number | string | 设备序列号 (唯一) |
| batch_id | string | 生产批次号 |
| oem_id | string | OEM 客户标识 |
| firmware_version | string | 烧录固件版本 |
| se_serial | string | SE050 芯片序列号 |
| ble_mac | string | BLE MAC 地址 |
| provisioning_date | datetime | 预置完成时间 |
| test_station_id | string | 测试工位编号 |
| test_operator | string | 操作员 ID |
| test_results | json | 各阶段测试结果 |
| defects | array | 缺陷记录 |
| final_status | PASS/FAIL/REWORK | 最终判定 |

---

## 12. 测试数据采集与良率管理

### 12.1 数据采集架构

```
┌──────────┐   ┌──────────┐   ┌──────────┐
│ FT1 工位 │   │ CT 工位  │   │Burn-in   │
│  数据    │   │  数据    │   │  数据    │
└────┬─────┘   └────┬─────┘   └────┬─────┘
     │              │              │
     └──────────────┼──────────────┘
                    ▼
          ┌──────────────────┐
          │  Data Collector  │
          │  (Kafka Stream)  │
          └────────┬─────────┘
                   │
          ┌────────▼─────────┐
          │  TimescaleDB     │
          │  (产线时序数据)   │
          └──────────────────┘
```

### 12.2 良率管理指标 (KPI)

| 指标 | 目标值 | 告警阈值 | 说明 |
|------|--------|----------|------|
| FT1 通过率 | ≥ 98% | < 95% | 功能测试一次通过率 |
| CT 通过率 | ≥ 97% | < 93% | 通信测试一次通过率 |
| 老化通过率 | ≥ 99% | < 97% | 老化测试通过率 |
| OQC 通过率 | ≥ 99.5% | < 98% | 最终检验通过率 |
| 总良率 (YRT) | ≥ 94% | < 90% | 滚动良率 |
| 返修率 | < 3% | > 5% | 可修复缺陷比例 |
| 报废率 | < 1% | > 2% | 不可修复缺陷比例 |

### 12.3 缺陷分类与追踪

| 缺陷类别 | 代码 | 说明 | 处理方式 |
|----------|------|------|----------|
| MCU 故障 | D-MCU-01 | MCU 启动失败 | 报废 |
| SE 通信故障 | D-SE-01 | SE050 I2C 无应答 | 返修 (检查焊接) |
| FLASH 校验错 | D-FL-01 | 固件哈希不匹配 | 重烧录 |
| BLE 功率异常 | D-BLE-01 | 发射功率超标 | 校准/更换模块 |
| NFC 场强不足 | D-NFC-01 | 场强 < 1.0 A/m | 返修 |
| UWB 测距偏差 | D-UWB-01 | 误差 > 10cm | 校准/更换 |
| 老化失败 | D-AGE-01 | 老化过程中功能失效 | 报废 |
| 外观缺陷 | D-OQC-01 | 外壳破损/标签错误 | 返修 |

---

## 13. 附录

### A. 关键工具清单

| 工具 | 版本 | 用途 |
|------|------|------|
| ARM GCC Toolchain | 10.3.1 | 交叉编译 |
| J-Link Software | 7.94 | 固件烧录 |
| Lauterbach TRACE32 | 2024.x | 调试/批量烧录 |
| Thales Luna HSM | 7.x | 密钥管理与签名 |
| OpenSC / pkcs11-tool | 0.24 | HSM 接口工具 |
| CycloneDX CLI | 0.8.x | SBOM 生成 |

### B. 环境配置清单

- [ ] ARM GCC 工具链安装与路径配置
- [ ] J-Link / Lauterbach 驱动安装
- [ ] HSM 客户端驱动与 PKCS#11 模块安装
- [ ] Docker 容器化构建环境
- [ ] CI/CD 流水线配置 (GitHub Actions)
- [ ] 产线测试工位网络配置
- [ ] MES 系统接口配置

### C. 文档参考

| 文档 | 说明 |
|------|------|
| [EMBEDDED-DEV-GUIDE.md](design/EMBEDDED-DEV-GUIDE.md) | 嵌入式端开发指南 |
| [SECURITY_WHITEPAPER.md](SECURITY_WHITEPAPER.md) | 安全白皮书 (密钥层级、安全启动) |
| [SECURITY_GUIDE.md](SECURITY_GUIDE.md) | 安全运营配置指南 |
| [TEST-PLAN.md](design/TEST-PLAN.md) | 三端测试计划 |
| [TEST_COVERAGE_REPORT.md](TEST-COVERAGE-REPORT.md) | 测试覆盖率报告 |
| [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md) | 系统架构文档 |

---

*文档版本: v1.0 | 最后更新: 2026-07-29 | 密级: 内部*
