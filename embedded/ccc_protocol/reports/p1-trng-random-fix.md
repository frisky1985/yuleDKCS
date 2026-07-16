# P1-1: TRNG 硬件随机数接入 — 报告

**日期**: 2026-07-16  
**P1**: yuleDKCS  
**模块**: EMB-BSW-CRYPTO / EMB-BSW-SE050-SCP03  
**文件**:
- `include/crypto_random.h` (新)
- `src/security/crypto_random.c` (新)
- `src/security/se050_scp03.c` (修改)
- `src/security/security.c` (修改)

---

## 1. 问题描述

`se050_scp03.c` 中 `crypto_random_bytes()` 失败时回退到硬编码 tick 值（`0x12345678`），标注为 **DEV ONLY**。`security.c` 中 IV 生成失败时回退到固定值 `0xAB`。生产环境必须使用真正的 TRNG（真随机数发生器）。

### 安全问题

| 位置 | 原始代码 | 风险 |
|------|---------|------|
| `se050_scp03_open_session()` host_challenge | `memcpy(buf, &0x12345678, 8)` | SCP03 会话密钥完全可预测 |
| `sec_encrypt()` IV 生成 | `memset(iv, 0xAB, 12)` | AES-GCM nonce 重用 → 加密彻底失效 |
| `sec_init()` 无 TRNG 验证 | 无检查 | 设备可能在无熵源时启动 |

---

## 2. 架构设计

### 2.1 三层熵源策略

```
┌──────────────────────────────────────────────────┐
│                crypto_random_bytes()              │
├──────────────────────────────────────────────────┤
│  Tier 1: SE050 HW TRNG (via SCP03 GET CHALLENGE) │ ← 最高优先级
│  Tier 2: mbedTLS CTR_DRBG (seeded from OS)        │
│  Tier 3: /dev/urandom (Linux) / arc4random (BSD)  │ ← 最低优先级
└──────────────────────────────────────────────────┘
```

### 2.2 启动时序（解决样板问题）

SCP03 建立本身需要随机数（host challenge），但 SE050 RNG 需要 SCP03 通道才能访问，形成**信赖链的引导问题**。

```
时间线:
  crypto_random_init()        ← TRNG 初始化 (Tier 2/3)
       │
  crypto_random_bytes()       ← SCP03 引导阶段用 OS/mbedTLS
       │                           (Tier 2/3, 没有 SE050 依赖)
       │
  se050_scp03_open_session()  ← 建立 SCP03 通道
       │
  sec_scp03_open()            ← 注册 SE050 RNG 回调
       │
  crypto_random_bytes()       ← 正常运行阶段用 SE050 HW TRNG
                                    (Tier 1, 硬件真随机数)
```

### 2.3 文件组织

```
ccc_protocol/
├── include/
│   └── crypto_random.h       ← 统一 API 声明
└── src/security/
    ├── crypto_random.c        ← 三层熵源实现
    ├── se050_scp03.c          ← 修改: 移除外部队列声明 + 硬编码回退
    └── security.c             ← 修改: TRNG 初始化 + SE050 注册/注销
```

---

## 3. 改动详情

### 3.1 新增 `include/crypto_random.h`

定义统一 API:

| API | 用途 |
|-----|------|
| `crypto_random_init()` | 初始化 TRNG 子系统，探测可用熵源，运行自检 |
| `crypto_random_deinit()` | 安全销毁 DRBG 状态和内部缓冲区 |
| `crypto_random_is_available()` | 查询熵源可用性 |
| `crypto_random_get_source()` | 返回当前活跃熵源类型 |
| `crypto_random_bytes()` | 填充缓冲区为加密安全随机字节 |
| `crypto_random_register_se050()` | 注册 SE050 RNG 回调（安全模块调用） |
| `crypto_random_unregister_se050()` | 注销 SE050 RNG 回调 |
| `crypto_random_health_test()` | 运行健康检查（频数检测 + stuck-bit 检测） |

错误码:
- `CRYPTO_RANDOM_OK` (0)
- `CRYPTO_RANDOM_ERR_NOT_INIT` (-1)
- `CRYPTO_RANDOM_ERR_DRBG` (-2)
- `CRYPTO_RANDOM_ERR_OS` (-3)
- `CRYPTO_RANDOM_ERR_SE050` (-4)
- `CRYPTO_RANDOM_ERR_PARAM` (-5)
- `CRYPTO_RANDOM_ERR_NO_SOURCE` (-6)

### 3.2 新增 `src/security/crypto_random.c`

#### Tier 1: SE050 HW TRNG
- 通过 `crypto_random_register_se050()` 注册的回调实现
- 回调通过 SCP03 安全通道发送 GET CHALLENGE (INS=0x84) APDU
- 每调用最多返回 256 字节硬件随机数
- 大于 256 字节的请求自动分块

#### Tier 2: mbedTLS CTR_DRBG
- 编译时检测 `MBEDTLS_CTR_DRBG_C` / `USE_MBEDTLS`
- 由 OS 熵源（`/dev/urandom`）提供种子
- 100 万次请求后自动重新播种
- DRBG 失效时自动重新播种一次

#### Tier 3: OS 熵源
- Linux: `/dev/urandom`（非阻塞内核 CSPRNG）
- macOS/BSD: `arc4random_buf()`（保证可用）
- Windows: `BCryptGenRandom()`

#### 健康检测
- **初始化自检**: 生成 128 字节样本 → stuck-bit 检查 + 单比特频数检测
- **运行时检测**: 每 1024 次调用执行 stuck-bit 检查
- **periodic 健康检测 API**: `crypto_random_health_test()` — 256 字节样本 + NIST SP 800-22 基本子集

### 3.3 修改 `se050_scp03.c`

| 位置 | 原代码 | 现代码 |
|------|--------|--------|
| 文件头 | `extern int crypto_random_bytes(...)` | `#include "crypto_random.h"` |
| host_challenge 生成 | 失败后 `memcpy(buf, &0x12345678, 8)` | `return SCP03_ERR_HW` — 直接失败 |

### 3.4 修改 `security.c`

| 位置 | 原代码 | 现代码 |
|------|--------|--------|
| 文件头 | `extern int crypto_random_bytes(...)` | `#include "crypto_random.h"` |
| `sec_init()` | 无 TRNG 检查 | 调用 `crypto_random_init()` + `crypto_random_health_test()` |
| `sec_init()` | 启动成功后不注册 SE050 | 启动后注册 `se050_rng_via_scp03` 回调 |
| `sec_deinit()` | 无清理 | 调用 `crypto_random_unregister_se050()` + `crypto_random_deinit()` |
| `sec_encrypt()` IV | 失败后 `memset(iv, 0xAB, 12)` | `return CCC_ERR_HARDWARE` |
| `sec_scp03_open()` | 无注册 | SCP03 建立后注册 SE050 RNG |
| `sec_scp03_open()` 出错 | 不清理 | SCP03 打开失败时注销 SE050 RNG |
| `sec_scp03_close()` | 无注销 | 关闭前注销 SE050 RNG |

新增函数 `se050_rng_via_scp03()`:
- 签名: `static int se050_rng_via_scp03(uint8_t *buf, size_t len)`
- 通过 `se050_scp03_apdu()` 发送 GET CHALLENGE 命令
- 支持任意长度（自动分块 256 字节/次）

---

## 4. 回退行为矩阵

| 场景 | Tier 1 (SE050) | Tier 2 (mbedTLS) | Tier 3 (OS) | 结果 |
|------|----------------|------------------|-------------|------|
| 完全正常 | ✅ | ✅ | ✅ | 使用 SE050 |
| SCP03 未建立/已关闭 | ❌ | ✅ | ✅ | 使用 mbedTLS |
| 无 mbedTLS | ✅ | ❌ | ✅ | 使用 SE050 |
| 嵌入式纯 OS | ❌ | ❌ | ✅ | 使用 `/dev/urandom` |
| 所有源不可用 | ❌ | ❌ | ❌ | **返回错误，设备拒绝启动** |

**无硬编码回退。所有路径返回明确错误码。**

---

## 5. 初始化验证逻辑

`sec_init()` 中的 TRNG 检查流程:

```
sec_init()
  ├─ crypto_random_init()
  │    ├─ [Probe mbedTLS]       → 初始化 CTR_DRBG + 自检
  │    ├─ [Probe OS entropy]    → os_get_random() + 自检
  │    └─ All failed?           → return CRYPTO_RANDOM_ERR_NO_SOURCE
  │
  ├─ crypto_random_health_test()
  │    ├─ 读 256 字节
  │    ├─ Stuck-bit 检测
  │    ├─ 单比特频数检测 (±15%)
  │    └─ 失败?                 → crypto_random_deinit(); return CCC_ERR_HARDWARE
  │
  ├─ crypto_engine_init()
  ├─ se05x_open_session()
  ├─ crypto_random_register_se050()
  └─ OK
```

如果 `crypto_random_init()` 失败, `sec_init()` 返回 `CCC_ERR_HARDWARE`, 上层 `ccc_dk_init()` 传递该错误, 系统不启动。

---

## 6. 遗留问题

### 6.1 SCP03 密钥材料回退 (非本 P1-1 范围)

`security.c` 中 `sec_encrypt()` 和 `sec_decrypt()` 在 SCP03 会话未打开时使用 `static const uint8_t fallback_key[16] = {0}`：

```c
static const uint8_t fallback_key[16] = {0};    /* DEV ONLY */
key_material = (uint8_t *)fallback_key;
```

这是 SCP03 通道状态的问题, 而非随机数产生的问题。当 SCP03 未建立时, 此路径不应执行, 但若执行则使用全零密钥。

**建议**: 通过 P0-1 (SCP03 强制启用) 或 P1-2 解决。修改方向:
- `sec_encrypt()` 和 `sec_decrypt()` 在 SCP03 未建立时直接返回 `CCC_ERR_CHANNEL`
- 或要求 SCP03 会话必须在所有加密操作前建立

### 6.2 无 FIPS 140-2 认证检测

`crypto_random_health_test()` 提供了 stuck-bit 检测和基本频数检测, 但并非 FIPS 140-2 连续随机数检测 (CRNGT) 的完整实现。

**建议**: 如果要求 FIPS 认证, 应:
- 在 `crypto_random_bytes()` 中实现全量 CRNGT
- 添加健康测试计数器, 每 2^20 次调用强制重新播种
- 添加故障计数器, 连续 3 次健康测试失败则 panic

### 6.3 线程安全

当前实现假设单线程执行（嵌入式 FreeRTOS 单任务或中断受保护上下文）。如果 `crypto_random_bytes()` 将在多线程上下文调用, 需要添加 mutex 保护。

---

## 7. 测试建议

| 测试场景 | 测试方法 | 预期结果 |
|---------|---------|---------|
| TRNG 初始化成功 | 调用 `sec_init()` | 返回 `CCC_OK` |
| 无熵源时拒绝启动 | 模拟所有熵源失效 | `sec_init()` 返回 `CCC_ERR_HARDWARE` |
| SCP03 引导阶段随机 | 调用 `se050_scp03_open_session()` | host_challenge 非确定性 |
| SE050 回退到 mbedTLS | 断开 SE050, 调用 `crypto_random_bytes()` | 成功, 源为 `CRYPTO_RANDOM_SOURCE_MBEDTLS` |
| mbedTLS 回退到 OS | 禁用 mbedTLS + 断开 SE050 | 成功, 源为 `CRYPTO_RANDOM_SOURCE_OS` |
| 所有源不可用 | 禁用所有源 | `crypto_random_bytes()` 返回 `CRYPTO_RANDOM_ERR_NO_SOURCE` |
| 健康检测 → stuck bit | 模拟熵源返回全零 | `crypto_random_health_test()` 返回负值 |
| 长请求 (>256 bytes) | 请求 1024 字节 | 正确填充, SE050 自动分块 |
| IV 生成失败 | 强制 `crypto_random_bytes()` 失败 | `sec_encrypt()` 返回 `CCC_ERR_HARDWARE` |

---

## 8. 文件变更清单

```
A  include/crypto_random.h          (6583 bytes)  — 新文件
A  src/security/crypto_random.c     (18280 bytes) — 新文件
M  src/security/se050_scp03.c       — 移除 extern 声明 + 硬编码回退
M  src/security/security.c          — 添加 TRNG init/health/callback/unregister
```

---

**报告结束** — P1-1 TRNG 硬件随机数接入完成。
