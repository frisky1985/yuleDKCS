# yuleDKCS 固件安全更新策略

> **版本**：v1.0
> **日期**：2026-07-29
> **状态**：初版
> **适用范围**：嵌入式固件团队、安全工程团队、OTA 运维团队

---

## 目录

1. [概述与目标](#1-概述与目标)
2. [OTA 更新架构](#2-ota-更新架构)
3. [固件更新流程](#3-固件更新流程)
4. [签名与验证机制](#4-签名与验证机制)
5. [回滚保护策略](#5-回滚保护策略)
6. [版本兼容策略](#6-版本兼容策略)
7. [更新通道与传输安全](#7-更新通道与传输安全)
8. [失败恢复与容错机制](#8-失败恢复与容错机制)
9. [发布与部署策略](#9-发布与部署策略)
10. [关键代码与配置](#10-关键代码与配置)
11. [附录](#11-附录)

---

## 1. 概述与目标

### 1.1 文档目标

定义 yuleDKCS 车辆嵌入式固件的安全更新全链路策略，覆盖 OTA 推送、签名验证、回滚保护、版本兼容和发布部署流程，确保固件更新过程中的**机密性、完整性、可用性和不可否认性**。

### 1.2 更新范围

| 组件 | 更新方式 | 更新频率 | 安全级别 |
|------|----------|----------|----------|
| BootLoader (S32G2) | OTA + 回滚保护 | 极少 (≤ 1次/年) | 🔴 关键 |
| Application 固件 | OTA (主要) + USB DFU (备用) | 4-12次/年 | 🔴 关键 |
| BLE 模块固件 (KW47A) | OTA 透传 | 2-4次/年 | 🟠 高 |
| UWB 模块固件 (NCJ29D6) | OTA 透传 | 2-4次/年 | 🟠 高 |
| NFC 模块固件 (ST25R501) | OTA 透传 | 1-2次/年 | 🟡 中 |
| SE050 固件 | NXP 安全更新包 | 极少 (安全补���) | 🔴 关键 |
| 云端服务 | 标准 CI/CD | 持续 | 🟢 常规 |

### 1.3 安全目标

| 目标 | 说明 | 威胁场景 |
|------|------|----------|
| **完整性** | 确保只有签名的、未被篡改的固件可被安装 | 固件篡改、中间人替换 |
| **真实性** | 确保固件来源于受信任的发布方 | 伪造更新、恶意固件注入 |
| **机密性** | 保护传输中的固件内容不被泄露 | 固件逆向、知识产权泄露 |
| **防回滚** | 防止安装低于当前版本的固件 | 降级攻击、利用旧版本漏洞 |
| **原子性** | 更新失败后系统仍可正常启动 | 断电导致变砖、部分更新 |
| **可审计** | 所有更新操作有完整审计记录 | 未经授权的更新操作 |

---

## 2. OTA 更新架构

### 2.1 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      OTA 更新架构                               │
│                                                                  │
│   ┌──────────────┐          ┌──────────────┐                   │
│   │  OTA Server  │          │  OEM Cloud   │                   │
│   │  (云端)       │          │  (TSP)       │                   │
│   │  ·固件存储    │          │  ·车辆注册   │                   │
│   │  ·版本管理    │◄────────►│  ·授权验证   │                   │
│   │  ·签名服务    │          │  ·地理策略   │                   │
│   │  ·分批发布    │          │  ·状态跟踪   │                   │
│   └──────┬───────┘          └──────────────┘                   │
│          │                                                      │
│          │ HTTPS / mTLS                                         │
│          │                                                      │
│   ┌──────▼─────────────────────────────────────────────────┐   │
│   │                   车载端 (Vehicle Side)                 │   │
│   │                                                         │   │
│   │  ┌─────────────────────────────────────────────────┐   │   │
│   │  │           Linux (Cortex-A53)                    │   │   │
│   │  │  ┌─────────┐  ┌────────────┐  ┌─────────────┐  │   │   │
│   │  │  │OTA Agent│  │Update      │  │Dual Bank    │  │   │   │
│   │  │  │(Daemon) │  │Downloader  │  │Manager      │  │   │   │
│   │  │  └────┬────┘  └─────┬──────┘  └──────┬──────┘  │   │   │
│   │  └───────┼─────────────┼─────────────────┼─────────┘   │   │
│   │          │             │                 │              │   │
│   │          │      IPC (RPMSG / Shared Memory)              │   │
│   │          │             │                 │              │   │
│   │  ┌───────▼─────────────▼─────────────────▼─────────┐   │   │
│   │  │           FreeRTOS (Cortex-M7)                  │   │   │
│   │  │  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │   │   │
│   │  │  │Bootloader│  │Update    │  │  SE050       │  │   │   │
│   │  │  │(A/B Slot)│  │Validator │  │ 签名验证     │  │   │   │
│   │  │  └──────────┘  └──────────┘  └──────────────┘  │   │   │
│   │  └─────────────────────────────────────────────────┘   │   │
│   └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 固件存储分区布局

```
┌─────────────────────────────────────────────────────────────┐
│                 Flash 分区布局 (Dual-Bank)                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  0x0000_0000 - 0x0007_FFFF  |  Boot ROM (512 KB, 不可写)    │
│  0x0008_0000 - 0x000F_FFFF  |  BootLoader A (512 KB)        │
│  0x0010_0000 - 0x0017_FFFF  |  BootLoader B (512 KB)        │
│  0x0018_0000 - 0x0019_FFFF  |  Update Metadata (128 KB)     │
│  0x001A_0000 - 0x001F_FFFF  |  Reserved (384 KB)            │
│  ─────────────────────────────────────────────────────       │
│  0x0020_0000 - 0x007F_FFFF  |  Application Slot A (6 MB)    │
│  0x0080_0000 - 0x00DF_FFFF  |  Application Slot B (6 MB)    │
│  0x00E0_0000 - 0x00FF_FFFF  |  Application Metadata (2 MB)  │
│  ─────────────────────────────────────────────────────       │
│  0x0100_0000 - 0x01FF_FFFF  |  BLE Firmware Slot (16 MB)    │
│  0x0200_0000 - 0x02FF_FFFF  |  UWB Firmware Slot (16 MB)    │
│  0x0300_0000 - 0x03FF_FFFF  |  NFC Firmware Slot (16 MB)    │
│  ─────────────────────────────────────────────────────       │
│  0x0400_0000 - 0x06FF_FFFF  |  User Data (48 MB)            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 A/B 双分区 (无缝更新)

```
┌─────────────────────────────────────────────────────────────┐
│                     A/B 分区切换机制                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  当前状态: 从 Slot A 启动                                    │
│                                                              │
│  ┌─────────────┐          ┌─────────────┐                   │
│  │  Slot A     │  运行中  │  Slot B     │    未使用         │
│  │  (ACTIVE)   │─────────►│  (INACTIVE) │                   │
│  │  v2.1.0     │          │             │                   │
│  └─────────────┘          └─────────────┘                   │
│                                                              │
│  收到 v2.2.0 更新包 → 写入 Slot B                           │
│                                                              │
│  ┌─────────────┐          ┌─────────────┐                   │
│  │  Slot A     │  运行中  │  Slot B     │   正在写入        │
│  │  (ACTIVE)   │─────────►│  (UPDATING) │   v2.2.0          │
│  │  v2.1.0     │          │             │                   │
│  └─────────────┘          └─────────────┘                   │
│                                                              │
│  写入完成 + 签名验证 → 重启 → 标记 Slot B 为 ACTIVE         │
│                                                              │
│  ┌─────────────┐          ┌─────────────┐                   │
│  │  Slot A     │  备用    │  Slot B     │   运行中          │
│  │  (INACTIVE) │─────────►│  (ACTIVE)   │   v2.2.0          │
│  │  v2.1.0     │          │             │                   │
│  └─────────────┘          └─────────────┘                   │
│                                                              │
│  如果启动失败 (Watchdog 触发) → 自动回滚到 Slot A            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 固件更新流程

### 3.1 全流程时序图

```
手机 App                   OTA Server                   车载 TCU
   │                          │                          │
   │ 1. 检查更新请求           │                          │
   │─────────────────────────►│                          │
   │                          │ 2. 查询车辆信息          │
   │                          │─────────────────────────►│
   │                          │◄─────────────────────────│
   │                          │  3. 返回当前版本信息     │
   │ 4. 返回可用更新列表       │                          │
   │◄─────────────────────────│                          │
   │                          │                          │
   │ 5. 用户确认更新           │                          │
   │─────────────────────────►│                          │
   │                          │ 6. 下发更新指令          │
   │                          │─────────────────────────►│
   │                          │                          │
   │                          │      7. 下载固件包       │
   │                          │◄═══════════════════════►│
   │                          │   (HTTPS, 断点续传)      │
   │                          │                          │
   │                          │      8. 完整性校验       │
   │                          │       (SHA-256)          │
   │                          │                          │
   │                          │      9. 签名验证         │
   │                          │       (SE050 ECDSA)      │
   │                          │                          │
   │                          │      10. 写入 Slot B     │
   │                          │                          │
   │                          │      11. 重启切换分区    │
   │                          │                          │
   │                          │ 12. 更新结果上报         │
   │                          │◄─────────────────────────│
   │ 13. 更新状态通知          │                          │
   │◄─────────────────────────│                          │
   │                          │                          │
```

### 3.2 更新包结构

```
固件更新包 (yuledkcs-fw-update-v2.2.0.pkg)
│
├── manifest.json              # 更新清单 (JSON)
│   ├── package_version: "2.2.0"
│   ├── target_hardware: "S32G2"
│   ├── target_slot: "B"
│   ├── min_compatible_version: "2.0.0"
│   ├── requires_bootloader: "≥1.5.0"
│   ├── components: [
│   │   { name: "application", size: 4194304, hash_sha256: "a1b2..." },
│   │   { name: "ble_fw",      size: 524288,  hash_sha256: "c3d4..." },
│   │   { name: "uwb_fw",      size: 1048576,  hash_sha256: "e5f6..." }
│   │ ]
│   ├── release_notes_uri: "https://ota.yuledkcs.com/releases/v2.2.0"
│   └── signature: "MEUCIQDFgxr..."  # manifest 外部签名
│
├── firmware/
│   ├── application.bin        # 主固件镜像
│   ├── ble_kw47a.bin          # BLE 模块固件
│   └── uwb_ncj29d6.bin        # UWB 模块固件
│
└── metadata/
    ├── signature.bin          # 固件签名 (2-of-3 multisig)
    ├── cert_chain.pem         # 签名证书链
    └── payload_encrypted_key  # 对称加密密钥 (如有加密)
```

### 3.3 manifest.json 示例

```json
{
  "manifest_version": 1,
  "package_version": "2.2.0",
  "build_id": "20260729.001",
  "target_hardware": "S32G2",
  "target_slot": "B",
  "min_compatible_version": "2.0.0",
  "requires_bootloader": ">=1.5.0",
  "oem_id": "OEM_A",
  "priority": "recommended",
  "components": [
    {
      "name": "application",
      "type": "main",
      "size": 4194304,
      "hash_sha256": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b",
      "offset": 0,
      "sign_required": true
    },
    {
      "name": "ble_fw",
      "type": "peripheral",
      "size": 524288,
      "hash_sha256": "b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c",
      "offset": 4194304,
      "sign_required": true
    },
    {
      "name": "uwb_fw",
      "type": "peripheral",
      "size": 1048576,
      "hash_sha256": "c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d",
      "offset": 4718592,
      "sign_required": true
    }
  ],
  "signature_required": 2,
  "signatures": [
    {
      "signer": "OEM_A",
      "signature": "MEUCIQD...",
      "cert_serial": "12:34:56:78"
    },
    {
      "signer": "yuleDKCS",
      "signature": "MEQCICv...",
      "cert_serial": "87:65:43:21"
    }
  ],
  "release_date": "2026-07-29T00:00:00Z",
  "release_notes": "https://ota.yuledkcs.com/releases/v2.2.0",
  "rollout_strategy": {
    "canary_percent": 5,
    "staged_days": 14,
    "max_concurrent": 10000,
    "geographic_hold": ["CN"],
    "min_battery_percent": 30,
    "max_vehicle_speed_kmh": 0
  }
}
```

---

## 4. 签名与验证机制

### 4.1 多重签名策略

```
┌─────────────────────────────────────────────────────────────┐
│                    多重签名 (2-of-3)                         │
│                                                              │
│  固件更新包需要至少 2 个签名方签署：                          │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  OEM 签名    │  │ yuleDKCS    │  │  审计方签名  │       │
│  │  (OEM_A)     │  │  签名       │  │  (第三方)    │       │
│  │  密钥在 HSM  │  │  密钥在 HSM  │  │  密钥在 HSM  │       │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │
│         │                 │                  │               │
│         └─────────────────┼──────────────────┘               │
│                           │                                   │
│                    ┌──────▼───────┐                          │
│                    │  需要 ≥ 2/3   │                          │
│                    │  签名有效     │                          │
│                    └──────┬───────┘                          │
│                           │                                   │
│                    ┌──────▼───────┐                          │
│                    │  SE050 验证   │                          │
│                    │  签名通过     │                          │
│                    └──────────────┘                          │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 验证流程

```c
/**
 * 固件更新签名验证
 * 在 BootLoader 阶段由 SE050 执行
 */
typedef enum {
    FW_VERIFY_SUCCESS = 0,
    FW_VERIFY_HASH_MISMATCH,
    FW_VERIFY_INVALID_SIGNATURE,
    FW_VERIFY_INSUFFICIENT_SIGNATURES,
    FW_VERIFY_CERT_CHAIN_INVALID,
    FW_VERIFY_ROLLBACK_DETECTED
} FirmwareVerifyResult;

FirmwareVerifyResult verify_firmware_update(
    const UpdatePackage *pkg
) {
    FirmwareVerifyResult result;

    // 1. 校验证书链
    //    SE050 验证签名证书链 → Root CA
    result = verify_cert_chain(pkg->cert_chain);
    if (result != FW_VERIFY_SUCCESS) return result;

    // 2. 计数有效签名 (至少 2 of 3)
    int valid_signatures = 0;
    for (int i = 0; i < pkg->num_signatures; i++) {
        if (verify_signature(
                SE050_KEY_ID_FW_SIGNING,
                pkg->manifest_hash,
                pkg->signatures[i]
            )) {
            valid_signatures++;
        }
    }
    if (valid_signatures < 2) {
        return FW_VERIFY_INSUFFICIENT_SIGNATURES;
    }

    // 3. 校验单个组件哈希
    for (int i = 0; i < pkg->num_components; i++) {
        uint8_t computed_hash[32];
        crypto_sha256(pkg->components[i].data,
                      pkg->components[i].size,
                      computed_hash);
        if (memcmp(computed_hash,
                   pkg->components[i].expected_hash,
                   32) != 0) {
            return FW_VERIFY_HASH_MISMATCH;
        }
    }

    // 4. 防回滚检查 (SE050 单调计数器)
    if (pkg->version <= se050_read_version_counter()) {
        return FW_VERIFY_ROLLBACK_DETECTED;
    }

    return FW_VERIFY_SUCCESS;
}
```

### 4.3 签名证书层级

```
┌─────────────────────────────────────────────────────────────┐
│             固件签名证书链                                    │
│                                                              │
│  Root CA (离线存储, HSM 保护)                                │
│  ├── Subject: CN=yuleDKCS Root CA, O=yuleDKCS               │
│  ├── Validity: 10 years                                      │
│  ├── Key Usage: Key Cert Sign, CRL Sign                     │
│  └── Stored: HSM (离线冷存储)                                 │
│       │                                                      │
│       ├── Firmware Signing CA                                │
│       │   ├── Subject: CN=FW Signing CA, O=yuleDKCS         │
│       │   ├── Validity: 3 years                              │
│       │   ├── Key Usage: Digital Signature                   │
│       │   └── Stored: HSM (在线)                              │
│       │        │                                              │
│       │        ├── OEM_A FW Signing Key                      │
│       │        │   └── 签署固件更新包                         │
│       │        │                                              │
│       │        ├── yuleDKCS FW Signing Key                   │
│       │        │   └── 签署固件更新包                         │
│       │        │                                              │
│       │        └── Auditor FW Signing Key                    │
│       │            └── 签署固件更新包                         │
│       │                                                      │
│       └── Boot Key CA                                        │
│           └── 预烧录到 SE050 的固件验证公钥                    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 4.4 传输加密

```python
# 固件包可选加密传输 (敏感车型要求)

# 1. 生成对称加密密钥 (AES-256-GCM)
encryption_key = os.urandom(32)

# 2. 使用车辆公钥加密对称密钥
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding

encrypted_key = vehicle_public_key.encrypt(
    encryption_key,
    padding.OAEP(
        mgf=padding.MGF1(algorithm=hashes.SHA256()),
        algorithm=hashes.SHA256(),
        label=None
    )
)

# 3. 加密固件包
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

aesgcm = AESGCM(encryption_key)
nonce = os.urandom(12)
encrypted_firmware = aesgcm.encrypt(nonce, firmware_data, None)

# 4. 更新包结构
final_package = {
    "encrypted_key": encrypted_key,
    "nonce": nonce,
    "encrypted_firmware": encrypted_firmware,
    "manifest": signed_manifest
}
```

---

## 5. 回滚保护策略

### 5.1 版本计数器层级

| 层级 | 组件 | 计数器类型 | 存储介质 | 递增时机 |
|------|------|-----------|----------|----------|
| L0 | BootLoader | 8-bit monotonic | SE050 NV-Counter | 每次 BootLoader 更新 |
| L1 | TFM | 16-bit monotonic | SE050 NV-Counter | 每次 TFM 更新 |
| L2 | Application | 32-bit monotonic | SE050 NV-Counter | 每次 App 固件更新 |
| L3 | BLE/UWB/NFC | 16-bit per module | 模块内部 | 每次外设固件更新 |

### 5.2 SE050 单调计数器使用

```c
/**
 * 防回滚保护实现
 * SE050 提供硬件单调计数器，不可回退
 */

// 读���当前版本计数器
uint32_t read_app_version_counter(void) {
    uint32_t counter;
    SE05x_ReadCounter(SE050_COUNTER_ID_APP_VERSION, &counter);
    return counter;
}

// 更新版本计数器 (仅当新版本 > 当前版本)
typedef enum {
    COUNTER_UPDATE_OK = 0,
    COUNTER_UPDATE_ROLLBACK  // 新版本 ≤ 当前版本
} CounterUpdateResult;

CounterUpdateResult update_app_version_counter(uint32_t new_version) {
    uint32_t current = read_app_version_counter();

    if (new_version <= current) {
        // 拒绝降级安装
        log_security_event("ROLLBACK_ATTEMPT_DETECTED",
                          "current", current,
                          "attempted", new_version);
        return COUNTER_UPDATE_ROLLBACK;
    }

    // SE050 单调递增 (硬件保证不可回退)
    SE05x_WriteCounter(SE050_COUNTER_ID_APP_VERSION, new_version);
    return COUNTER_UPDATE_OK;
}

// BootLoader 启动时检查
BootDecision verify_boot_version(void) {
    // 读取 slot 固件的版本号
    uint32_t slot_version = read_slot_version(CURRENT_SLOT);
    // 读取 SE050 最小允许版本
    uint32_t min_version = read_app_version_counter();

    if (slot_version >= min_version) {
        return BOOT_ALLOW;
    } else {
        // 回滚检测 → 尝试另一 slot
        switch_to_alternate_slot();
        return BOOT_FALLBACK;
    }
}
```

### 5.3 回滚处理流程

```
                       ┌─────────────────┐
                       │  系统上电启动    │
                       └────────┬────────┘
                                │
                       ┌────────▼────────┐
                       │  BootLoader 执行 │
                       └────────┬────────┘
                                │
                       ┌────────▼────────┐
                       │  验证 Slot 签名  │
                       └────────┬────────┘
                                │
                  ┌─────────────┼─────────────┐
                  │ 签名有效     │ 签名无效    │
                  ▼              ▼              │
           ┌────────────┐  ┌────────────────┐  │
           │ 验证版本    │  │ 切换到另一 Slot│  │
           │ 计数器      │  └───────┬────────┘  │
           └──────┬─────┘          │            │
                  │                │            │
           ┌──────▼─────┐  ┌───────▼────────┐  │
           │ 版本 ≥ 计数 │  │ 另一 Slot      │  │
           │ 器 → 正常   │  │ 版本验证       │  │
           │ 启动        │  └───────┬────────┘  │
           └──────┬─────┘          │            │
                  │         ┌──────▼──────┐     │
                  │         │  验证通过   │     │
                  │         │ → 启动备用  │     │
                  │         │ Slot并告警  │     │
                  │         └─────────────┘     │
                  │                             │
           ┌──────▼─────┐                       │
           │ 正常启动    │ 如果全部 Slot 失败:   │
           │ Application │ → 进入恢复模式        │
           └────────────┘ → OTA 方式恢复固件     │
```

### 5.4 降级策略例外

| 场景 | 是否允许降级 | 条件 |
|------|-------------|------|
| 安全补丁回退 | ❌ 禁止 | 安全漏洞可能被利用 |
| 功能回归回退 | ⚠️ 受限 | 需 OEM 授权 + 紧急审批 |
| A/B 切换回退 | ✅ 自动 | BootLoader 自动回滚 |
| 开发环境回退 | ✅ 允许 | 调试签名 + 开发版本号 |
| 生产回滚策略 | ⚠️ 1个版本 | 仅允许回退到上一版本 |

---

## 6. 版本兼容策略

### 6.1 兼容矩阵

```
┌─────────────────────────────────────────────────────────────┐
│              固件版本兼容矩阵                                 │
├─────────────┬──────────┬──────────┬──────────┬──────────────┤
│ App版本      │ BL v1.0  │ BL v1.5  │ BL v2.0  │ 说明         │
├─────────────┼──────────┼──────────┼──────────┼──────────────┤
│ App v2.0.x  │ ✅       │ ✅       │ ⚠️ 降级  │ 基础功能     │
│ App v2.1.x  │ ❌       │ ✅       │ ✅       │ 新增协议     │
│ App v2.2.x  │ ❌       │ ✅       │ ✅       │ 安全更新     │
│ App v3.0.x  │ ❌       │ ❌       │ ✅       │ 架构升级     │
├─────────────┼──────────┼──────────┼──────────┼──────────────┤
│ SE050 FW    │ v3.x     │ v4.x     │ v5.x     │ NXP 发布     │
├─────────────┼──────────┼──────────┼──────────┼──────────────┤
│ BLE KW47A   │ v1.x     │ v2.x     │ v2.x     │ BLE 协议     │
├─────────────┼──────────┼──────────┼──────────┼──────────────┤
│ UWB NCJ29D6 │ v2.x     │ v2.x     │ v3.x     │ UWB 测距     │
└─────────────┴──────────┴──────────┴──────────┴──────────────┘
```

### 6.2 兼容性检查流程

```c
/**
 * 深度版本兼容性检查
 * 在更新包下载完成后、安装前执行
 */

typedef struct {
    uint32_t app_version;           // Application 版本
    uint32_t bootloader_version;    // BootLoader 版本
    uint32_t se050_fw_version;      // SE050 固件版本
    uint32_t ble_fw_version;        // BLE 模块版本
    uint32_t uwb_fw_version;        // UWB 模块版本
    uint32_t nfc_fw_version;        // NFC 模块版本
} SystemVersionInfo;

CompatibilityResult check_update_compatibility(
    const UpdatePackage *pkg,
    const SystemVersionInfo *current
) {
    // 1. BootLoader 兼容性
    if (pkg->requires_bootloader > current->bootloader_version) {
        return COMPAT_BOOTLOADER_TOO_OLD;
    }

    // 2. 最小兼容版本检查
    if (current->app_version < pkg->min_compatible_version) {
        return COMPAT_CURRENT_TOO_OLD;
    }

    // 3. 协议栈兼容性 (CCC/ICCOA/ICCE)
    //    检查新固件是否支持当前激活的协议栈
    if (!pkg->supports_protocol(current->active_protocol)) {
        return COMPAT_PROTOCOL_MISMATCH;
    }

    // 4. 硬件兼容性
    if (pkg->target_hardware != current->hardware_revision) {
        return COMPAT_HARDWARE_MISMATCH;
    }

    // 5. SE050 固件兼容性
    if (pkg->requires_se050_fw > current->se050_fw_version) {
        return COMPAT_SE050_FW_MISMATCH;
    }

    // 6. 外设固件兼容性
    for (int i = 0; i < pkg->num_peripheral_updates; i++) {
        PeripheralCompat c = pkg->peripheral_updates[i];
        if (c.min_host_version > current->app_version) {
            return COMPAT_PERIPHERAL_HOST_MISMATCH;
        }
    }

    return COMPAT_OK;
}
```

### 6.3 兼容性错误处理

| 错误码 | 场景 | 处理方式 |
|--------|------|----------|
| COMPAT_BOOTLOADER_TOO_OLD | BootLoader 版本过低 | 先推送 BootLoader 更新，再推送 App |
| COMPAT_CURRENT_TOO_OLD | 当前应用版本过旧 | 推送中间版本作为跳板 |
| COMPAT_PROTOCOL_MISMATCH | 协议栈不兼容 | 保持当前协议栈，标记需迁移 |
| COMPAT_HARDWARE_MISMATCH | 固件与硬件不对应 | 拒绝更新，上报错误 |
| COMPAT_SE050_FW_MISMATCH | SE050 版本不符 | 推送 SE050 安全更新包 |

---

## 7. 更新通道与传输安全

### 7.1 多通道支持

| 通道 | 传输方式 | 速率 | 适用场景 | 安全性 |
|------|----------|------|----------|--------|
| 蜂窝网络 (4G/5G) | HTTPS (TCP) | 1-100 Mbps | 主要 OTA 通道 | TLS 1.3 + mTLS |
| WiFi | HTTPS | 10-100 Mbps | 家庭/公司停车场 | TLS 1.3 + mTLS |
| BLE | L2CAP CoC | 500 Kbps | 手机→车辆直传 | AES-CCM (LE Secure) |
| USB DFU | USB 2.0 | 480 Mbps | 售后/维修 | 物理接触安全 |
| NFC | ISO 7816-4 | 424 Kbps | 极小包应急更新 | SCP03 安全通道 |

### 7.2 HTTPS OTA 传输配置

```nginx
# OTA Server Nginx 配置
server {
    listen 443 ssl http2;
    server_name ota.yuledkcs.com;

    # TLS 1.3 强制
    ssl_protocols TLSv1.3;
    ssl_ciphers TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256;

    # 客户端证书验证 (mTLS)
    ssl_client_certificate /etc/ssl/ca.crt;
    ssl_verify_client on;

    # OCSP Stapling
    ssl_stapling on;
    ssl_stapling_verify on;

    # HSTS
    add_header Strict-Transport-Security "max-age=63072000" always;

    # 固件下载端点
    location /firmware/ {
        root /data/ota/firmware;
        # 断点续传
        add_header Accept-Ranges bytes;
        # 速率限制 (每辆车 5 MB/s)
        limit_rate 5m;
        # 访问日志
        access_log /var/log/nginx/ota_access.log;
    }

    # 更新清单端点
    location /manifest/ {
        proxy_pass http://ota_backend:8080;
        # 速率限制 (每家 OEM 100 req/s)
        limit_req zone=oem_ratelimit burst=20 nodelay;
    }
}
```

### 7.3 断点续传与校验

```bash
# 车载端下载固件 (支持断点续传)
OTA_URL="https://ota.yuledkcs.com/firmware/v2.2.0/application.bin"
TEMP_FILE="/tmp/ota_download.part"

# 使用 curl 断点续传下载
curl -L -C - \
  --cert /etc/certs/client.crt \
  --key /etc/certs/client.key \
  --cacert /etc/certs/ca.crt \
  -o "$TEMP_FILE" \
  "$OTA_URL" \
  --retry 3 \
  --retry-delay 10

# 下载完成后校验 SHA-256
EXPECTED_HASH="a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b"
COMPUTED_HASH=$(sha256sum "$TEMP_FILE" | awk '{print $1}')

if [ "$COMPUTED_HASH" != "$EXPECTED_HASH" ]; then
  echo "HASH MISMATCH! Firmware may be corrupted."
  exit 1
fi

# 移动至暂存区
mv "$TEMP_FILE" /data/ota/staging/application.bin
```

---

## 8. 失败恢复与容错机制

### 8.1 失败场景处理矩阵

| 失败场景 | 检测方式 | 处理措施 | 用户影响 |
|----------|----------|----------|----------|
| 下载中断 (网络断开) | 连接超时检测 | 断点续传 (最多 3 次) | 无 (后台静默重试) |
| 哈希校验失败 | 计算 SHA-256 对比 | 重新下载 (最多 3 次) | 无 |
| 签名验证失败 | SE050 验签失败 | 拒绝安装, 上报错误 | 更新失败通知 |
| 写入 Flash 失败 | Flash 编程返回值 | 标记 Block 为坏块, 重试 | 延迟完成 |
| 写入后验签失败 | 从 Flash 读取+验签 | 切换回原 Slot | 无 (无缝回滚) |
| 新固件启动失败 | Watchdog 超时 (3次) | 自动切回旧 Slot | 短暂重启 |
| 电池电量不足 | 监测电压 | 暂��更新, 等待充电 | 延迟 |
| 车速过快 | CAN 总线车速信号 | 暂停更新 | 无 |

### 8.2 BootLoader 恢复模式

```c
/**
 * BootLoader 恢复模式
 * 当所有 Slot 都无法启动时进入
 */

typedef enum {
    RECOVERY_NONE = 0,
    RECOVERY_WATCHDOG_TRIGGERED,     // Watchdog 连续超时
    RECOVERY_BOTH_SLOTS_INVALID,     // A/B 双 Slot 均失效
    RECOVERY_USER_INITIATED,         // 用户手动触发恢复
    RECOVERY_OTA_AGENT_REBOOT        // OTA Agent 触发热恢复
} RecoveryReason;

void enter_recovery_mode(RecoveryReason reason) {
    // 1. 记录恢复原因到 SE050 审计日志
    SE05x_WriteAuditLog(AUDIT_EVENT_RECOVERY, reason);

    // 2. 启动恢复模式网络服务
    //    BLE 进入配网模式 (Service UUID: 0xFFAB)
    ble_start_recovery_advertising();

    //    WiFi AP 模式 (SSID: yuleDKCS-RECOVERY-{SN})
    wifi_start_softap("yuleDKCS-RECOVERY-%s", get_serial_number());

    // 3. 等待恢复命令 (超时 30 分钟)
    RecoveryCommand cmd = wait_for_recovery_command(1800);

    switch (cmd.type) {
        case RECOVERY_CMD_OTA_REFLASH:
            // 通过 OTA 重新下载最新固件
            download_and_flash_firmware(cmd.ota_url);
            break;

        case RECOVERY_CMD_USB_DFU:
            // 通过 USB DFU 模式等待烧录
            enter_usb_dfu_mode();
            break;

        case RECOVERY_CMD_FACTORY_RESET:
            // 恢复出厂设置 (仅保留 SE050 密钥)
            factory_reset();
            break;
    }
}
```

### 8.3 更新状态机

```
                    ┌────────────────┐
                    │    IDLE       │
                    │   (无更新)    │
                    └───────┬────────┘
                            │ 收到更新通知
                            ▼
                    ┌────────────────┐
               ┌───│  DOWNLOADING   │◄──────────┐
               │   │   (下载中)     │            │
               │   └───────┬────────┘            │
               │           │ 下载完成             │ 重试
               │           ▼                     │
               │   ┌────────────────┐            │
               │   │  VERIFYING     │────────────┘
               │   │   (验证中)     │ 验证失败
               │   └───────┬────────┘
               │           │ 验证通过
               │           ▼
               │   ┌────────────────┐
               │   │  INSTALLING    │
               │   │   (安装中)     │
               │   └───────┬────────┘
               │           │ 安装完成
               │           ▼
               │   ┌────────────────┐
               │   │  REBOOTING     │
               │   │   (重启中)     │
               │   └───────┬────────┘
               │           │
               │    ┌──────┴──────┐
               │    │             │
               │    ▼             ▼
               │  ┌──────┐   ┌──────────┐
               │  │ ACTIVE│   │ ROLLBACK │
               │  │ (新)  │   │ (旧版本) │
               │  └───────┘   └────┬─────┘
               │                   │
               │           ┌───────┴──────┐
               │           │   Reporting  │
               │           │   (上报)     │
               │           └──────────────┘
               │                   │
               └───────────────────┘
                    回到 IDLE
```

---

## 9. 发布与部署策略

### 9.1 发布阶段

```
┌─────────────────────────────────────────────────────────────┐
│                    分阶段发布流程                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  阶段 0: 内部验证                                           │
│  ├── 开发环境: 自动 CI/CD 测试通过                          │
│  ├── QA 环境: HIL 测试完整通过                              │
│  └── 安全审计: 第三方安全审查通过                           │
│  Duration: 1-2 周                                            │
│                                                              │
│  阶段 1: Canary 发布 (5%)                                   │
│  ├── 选择 5% 车辆作为 Canary 组                             │
│  ├── 监控: 24-48h 无异常                                    │
│  ├── 关键指标: 更新成功率, 启动成功率, 崩溃率              │
│  └── 退出条件: 更新成功率 ≥ 99%, 零崩溃                    │
│                                                              │
│  阶段 2: 区域逐步发布                                       │
│  ├── 区域 A (20%): 特定地理区域 2-3 天                      │
│  ├── 区域 B (50%): 扩大至全国 3-5 天                        │
│  ├── 区域 C (100%): 全部开放 2-3 天                         │
│  └── 每个区域间保留 24h 观察窗口                            │
│                                                              │
│  阶段 3: 全球发布 (100%)                                    │
│  ├── 监控持续进行 (7 天)                                    │
│  ├── 触发条件: 所有指标正常                                  │
│  └── 完成标记                                                │
│                                                              │
│  阶段 4: 强制更新 (可选)                                     │
│  ├── 安全补丁强制推送                                        │
│  └── 超过 N 个版本的跨版本更新                               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 9.2 发布策略配置

```json
{
  "rollout_config": {
    "canary": {
      "enabled": true,
      "percent": 5,
      "duration_hours": 48,
      "monitor_metrics": [
        "update_success_rate",
        "boot_success_rate",
        "crash_rate",
        "key_op_success_rate"
      ],
      "pass_thresholds": {
        "update_success_rate": 0.99,
        "boot_success_rate": 0.999,
        "crash_rate": 0.001,
        "key_op_success_rate": 0.998
      }
    },
    "staged_rollout": {
      "waves": [
        {"percent": 20, "duration_hours": 72, "pause_hours": 24},
        {"percent": 50, "duration_hours": 96, "pause_hours": 24},
        {"percent": 100, "duration_hours": 72}
      ]
    },
    "geographic_holds": {
      "CN": {
        "reason": "Regulatory approval pending",
        "hold": true
      }
    },
    "device_filters": {
      "min_battery_percent": 30,
      "max_vehicle_speed_kmh": 0,
      "min_firmware_version": "2.0.0",
      "max_firmware_version": "3.0.0",
      "allowed_hardware_revisions": ["S32G2-revB", "S32G2-revC"]
    },
    "auto_pause_triggers": {
      "error_rate_percent": 5,
      "support_ticket_count": 10,
      "security_incident": true
    }
  }
}
```

### 9.3 发布监控仪表板

```
OTA 发布实时仪表板
┌─────────────────────────────────────────────────────────────┐
│  发布版本: v2.2.0   阶段: 区域B (50%)   状态: 进行中      │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  全局指标:                                                   │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │ 更新成功率  │  │ 启动成功率  │  │ 平均下载时间│            │
│  │  99.3%     │  │  99.95%    │  │  45s       │            │
│  └────────────┘  └────────────┘  └────────────┘            │
│                                                              │
│  区域分布:                                                   │
│  区域A: 99.5% (已通过)                                      │
│  区域B: 99.1% (监控中) ◀──                                  │
│  区域C: — (待开始)                                          │
│                                                              │
│  错误 TOP-5:                                                 │
│  1. 下载超时: 0.3%                                          │
│  2. 电量不足暂停: 0.2%                                      │
│  3. 签名验证失败: 0.05%                                     │
│  4. Flash 写入错误: 0.02%                                   │
│  5. 启动失败 (回滚): 0.01%                                  │
│                                                              │
│  Canary 监控: ✅ 48h 无异常                                  │
│  自动暂停: 未触发                                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 10. 关键代码与配置

### 10.1 OTA Agent 核心配置

```c
// ota_config.h — OTA Agent 配置参数

#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

// === OTA Server ===
#define OTA_SERVER_URL             "https://ota.yuledkcs.com"
#define OTA_CHECK_INTERVAL_SEC     86400       // 24h 检查一次
#define OTA_CHECK_INTERVAL_FAST    3600        // 强制更新时 1h

// === 下载 ===
#define OTA_DOWNLOAD_TIMEOUT_SEC   600         // 10 min 超时
#define OTA_DOWNLOAD_RETRY_MAX     3           // 最大重试次数
#define OTA_DOWNLOAD_RETRY_DELAY   10          // 重试间隔 (s)
#define OTA_DOWNLOAD_CHUNK_SIZE    65536       // 64 KB per chunk
#define OTA_MAX_CONCURRENT_DOWNLOADS 2         // 最大并行下载数

// === 验证 ===
#define OTA_REQUIRED_SIGNATURES    2           // 2-of-3 多重签名
#define OTA_SE050_KEY_ID_FW_SIGN   0x0010      // SE050 固件签名公钥 ID
#define OTA_SE050_COUNTER_APP      0x0001      // 应用版本计数器 ID
#define OTA_SE050_COUNTER_BL       0x0002      // BootLoader 版本计数器 ID

// === A/B 分区 ===
#define OTA_SLOT_A_BASE            0x00200000
#define OTA_SLOT_B_BASE            0x00800000
#define OTA_SLOT_SIZE              0x00600000   // 6 MB
#define OTA_SLOT_METADATA_BASE     0x00E00000
#define OTA_METADATA_SIZE           0x00200000   // 2 MB

// === 恢复 ===
#define OTA_RECOVERY_WATCHDOG_TIMEOUT_MS  5000  // 5s Watchdog
#define OTA_RECOVERY_MAX_BOOT_FAILURES     3     // 连续启动失败次数
#define OTA_RECOVERY_WAIT_TIMEOUT_SEC      1800  // 等待恢复命令超时

// === 安全 ===
#define OTA_UPDATE_CERT_CHAIN_MAX_SIZE     4096
#define OTA_SIGNATURE_MAX_SIZE             256
#define OTA_MAX_MANIFEST_SIZE              65536

// === 更新策略 ===
#define OTA_MIN_BATTERY_PERCENT            30    // 最低电量 %
#define OTA_MAX_VEHICLE_SPEED_KMH          0     // 更新时车速限制
#define OTA_BACKOFF_MULTIPLIER             2     // 失败退避倍数
#define OTA_MAX_BACKOFF_INTERVAL_SEC       86400 // 最大退避间隔 (1天)

#endif // OTA_CONFIG_H
```

### 10.2 SE050 密钥分配

| Key ID | 用途 | 算法 | 位置 | 有效期 |
|--------|------|------|------|--------|
| `0x0001` | Boot 公钥 (固件验签) | ECDSA P-256 | SE050 只读 | 硬件生命周期 |
| `0x0010` | 固件签名公钥 | ECDSA P-256 | SE050 | 证书有效期 |
| `0x0011` | OTA 签名中间 CA | ECDSA P-256 | SE050 | 3 年 |
| `0x0100` | 设备身份私钥 | ECDSA P-256 | SE050 | 设备生命周期 |
| `0x0101` | 设备身份证书 | X.509 | SE050 | 1 年 |
| `0x1000` | 应用版本计数器 | Monotonic | SE050 NV | 不可重置 |
| `0x1001` | BootLoader 版本计数器 | Monotonic | SE050 NV | 不可重置 |

---

## 11. 附录

### A. 参考文档

| 文档 | 说明 |
|------|------|
| [SECURITY_WHITEPAPER.md](SECURITY_WHITEPAPER.md) | 安全白皮书 (安全启动链、密钥层级) |
| [SECURITY_GUIDE.md](SECURITY_GUIDE.md) | 安全运营指南 |
| [EMBEDDED-DEV-GUIDE.md](design/EMBEDDED-DEV-GUIDE.md) | 嵌入式开发指南 |
| [MANUFACTURING-PLAN.md](MANUFACTURING-PLAN.md) | 量产制造计划 |
| [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md) | 部署指南 |

### B. 相关标准

| 标准 | 章节 | 要求 |
|------|------|------|
| ISO 21434:2021 | Clause 10.4 | 更新安全要求 |
| CCC Digital Key 3.0 | §8.5 OTA | OTA 更新规范 |
| UN R155 | CSMS | OTA 安全管理 |
| NIST SP 800-53 | SA-10 | Developer security testing |

### C. 术语表

| 术语 | 定义 |
|------|------|
| OTA | Over-The-Air，空中固件升级 |
| A/B Slot | 双分区更新模式，保持一个可启动副本 |
| Canary | 小比例先行发布，用于验证更新安全性 |
| Multi-Signature | 多重签名，需要多个密钥签署才能生效 |
| Monotonic Counter | 单调递增计数器，硬件保证不可回退 |
| Rollback Protection | 防回滚保护，阻止安装旧版本固件 |
| BootLoader | 引导程序，负责固件验签和启动选择 |
| Watchdog | 硬件看门狗，检测系统是否正常启动 |

### D. 策略更新记录

| 版本 | 日期 | 变更说明 | 作者 |
|------|------|----------|------|
| v1.0 | 2026-07-29 | 初始版本 | yuleDKCS Security Team |

---

*文档版本: v1.0 | 最后更新: 2026-07-29 | 密级: 内部*
