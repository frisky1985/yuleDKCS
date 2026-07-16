# P1-4: MCU TRNG 集成 — S32K312 RNGA 报告

**日期**: 2026-07-16  
**P1**: yuleDKCS  
**模块**: EMB-BSW-MCAL-TRNG / EMB-BSW-CRYPTO / EMB-BSW-SE050-SCP03  
**版本**: 1.0

---

## 1. 概述

在 P1-1 的基础上，将 S32K312 MCU 内置硬件 TRNG（RNGA 模块）集成到随机数生成框架中，作为 **Tier 2** 熵源，填补 SE050 硬件 TRNG（Tier 1，需要 SCP03 通道）与软件熵源（Tier 3/4）之间的空白。

### 1.1 解决的核心问题

**SCP03 信赖链引导问题（chicken-and-egg）**:

- SCP03 建立需要 host challenge（随机数）
- SE050 TRNG 需要 SCP03 通道才能访问
- 旧方案：硬编码 tick 值（DEV ONLY），或依赖 OS 熵源（无 OS 的裸机无效）
- **新方案**：S32K312 RNGA 提供硬件熵，无需 SCP03 依赖

### 1.2 四层熵源策略

```
┌──────────────────────────────────────────────────────┐
│                  crypto_random_bytes()                 │
├──────────────────────────────────────────────────────┤
│ Tier 1: SE050 HW TRNG (via SCP03 GET CHALLENGE)       │ ← SCP03 建立后
│ Tier 2: MCU TRNG   (S32K312 RNGA via hal_trng_read())  │ ← SCP03 引导阶段
│ Tier 3: mbedTLS CTR_DRBG (seeded from OS entropy)      │ ← 主机开发
│ Tier 4: OS entropy (/dev/urandom / arc4random)          │ ← 主机开发
└──────────────────────────────────────────────────────┘
```

### 1.3 硬件信息：S32K312 RNGA

| 参数 | 值 |
|------|-----|
| 基地址 | `0x40029000` |
| 输出宽度 | 每次读取 32 位 (4 字节) |
| 熵源 | 自由振荡振荡器环 (free-running oscillator ring) |
| 自检 | 硬件复位后自动运行 |
| 连续监测 | 内置 stuck bit 检测 |
| 合规性 | NIST SP 800-90B |
| 参考手册 | S32K3xx RM Chapter 43-44 |

---

## 2. 新增文件

### 2.1 `mcal_stubs/include/Trng.h` (新)

TRNG HAL 抽象接口，提供 MCU 独立于操作系统的 TRNG API：

| API | 用途 |
|-----|------|
| `hal_trng_init()` | 初始化 RNGA 模块；主机构建时用 OS 熵播种软件 PRNG |
| `hal_trng_deinit()` | 使 RNGA 进入休眠模式；主机构建时清零软件状态 |
| `hal_trng_read(buf, len)` | 从 MCU TRNG 读取随机字节；主机构建时用软件 CSPRNG 模拟 |
| `hal_trng_is_available()` | 查询 TRNG 是否初始化且可运行 |
| `hal_trng_self_test()` | 自检：stuck-bit 检测 + 白噪声检验 |

**S32K312 RNGA 寄存器**:

```c
#define S32K312_RNGA_BASE          0x40029000UL
#define S32K312_RNGA_CR            (*(volatile uint32 *)(S32K312_RNGA_BASE + 0x00UL))
#define S32K312_RNGA_SR            (*(volatile uint32 *)(S32K312_RNGA_BASE + 0x04UL))
#define S32K312_RNGA_ER            (*(volatile uint32 *)(S32K312_RNGA_BASE + 0x08UL))
```

| 寄存器 | 偏移 | 描述 |
|--------|------|------|
| CR | 0x00 | 控制寄存器 (GO, HA, INTM, CLRI, SLP) |
| SR | 0x04 | 状态寄存器 (OREG_LVL, SOF, LRD) |
| ER | 0x08 | 熵寄存器（读取随机数据） |

错误码：

- `HAL_TRNG_OK` (0)
- `HAL_TRNG_ERR_NOT_INIT` (-1)
- `HAL_TRNG_ERR_PARAM` (-2)
- `HAL_TRNG_ERR_STUCK_BIT` (-3)
- `HAL_TRNG_ERR_TIMEOUT` (-4)
- `HAL_TRNG_ERR_HW` (-5)

### 2.2 `mcal_stubs/src/trng_stub.c` (新)

TRNG 驱动实现，支持双模式编译：

**裸机 S32K3 模式**:
- 直接操作 RNGA 硬件寄存器
- `rnga_read_word()`: 设置 GO 位 → 轮询 OREG_LVL → 读取 ER → 检查 SOF 错误
- 超时保护（1000 次轮询迭代）

**主机/测试模式** (`RTD_ADAPTER_SELF_TEST` / `HOST_BUILD`):
- xorshift128+ 软件 CSPRNG（由 OS 熵 /dev/urandom 或 arc4random 给种子）
- 注意：仅用于开发测试，不具备硬件的真随机性质

---

## 3. 修改文件

### 3.1 `mcal_stubs/include/Mcal.h` (修改)

在已有 include 之后添加 `#include "Trng.h"`，使 MCAL 层统一包含 TRNG 接口。

### 3.2 `include/crypto_random.h` (修改)

- 新增枚举值 `CRYPTO_RANDOM_SOURCE_MCU_TRNG = 4`
- 更新文件注释：文档化四层熵源策略

### 3.3 `src/security/crypto_random.c` (修改)

**添加 MCU TRNG 作为 Tier 2**（位于 SE050 之后、mbedTLS 之前）：

```c
int crypto_random_bytes(uint8_t *buf, size_t len) {
    // Tier 1: SE050 HW TRNG (如果已注册)
    if (g_se050_rng_fn != NULL) {
        ret = g_se050_rng_fn(buf, len);
        if (ret == 0) { g_active_source = CRYPTO_RANDOM_SOURCE_SE050; return OK; }
    }

    // Tier 2: MCU TRNG (S32K312 RNGA) ← NEW
    #if defined(CRYPTO_RANDOM_HAVE_MCU_TRNG)
        ret = mcu_trng_get_random(buf, len);
        if (ret == 0) { g_active_source = CRYPTO_RANDOM_SOURCE_MCU_TRNG; return OK; }
    #endif

    // Tier 3: mbedTLS CTR_DRBG
    // Tier 4: OS entropy
    // ...
    // 所有源失败 → 返回错误
}
```

**`crypto_random_init()` 变更**:
- 新探测顺序：MCU TRNG → mbedTLS → OS 熵
- MCU TRNG 探测：`hal_trng_init()` → 读取 64 字节自检 → stuck-bit 检查
- MCU TRNG 自检通过即设为活跃源

**`crypto_random_deinit()` 变更**:
- 添加 `hal_trng_deinit()` 调用

**`crypto_random_unregister_se050()` 变更**:
- SE050 注销后优先回退到 `CRYPTO_RANDOM_SOURCE_MCU_TRNG`

**添加条件编译**:
```c
#if defined(TARGET_S32K3) || defined(USE_MCU_TRNG) || defined(__arm__) || ...
    #define CRYPTO_RANDOM_HAVE_MCU_TRNG  1
    #include "Trng.h"
#endif
```

### 3.4 `src/security/se050_scp03.c` (修改)

更新 host challenge 生成注释，文档化引导阶段的 Tier 2 (MCU TRNG) 行为。原代码已正确处理 `crypto_random_bytes()` 失败时的错误返回。

**变更**: 注释更新 [P1-1] → [P1-4]，明确 MCU TRNG 是引导阶段的熵源。

---

## 4. 启动时序

```
阶段 1: 系统上电
  │
  ├─ hal_trng_init()           ← MCU TRNG 初始化
  │    └─ RNGA 寄存器配置 + 自检
  │
  ├─ crypto_random_init()
  │    ├─ [Probe MCU TRNG]     ← Tier 2 = S32K312 RNGA
  │    ├─ [Probe mbedTLS]      ← Tier 3 (if available)
  │    └─ [Probe OS entropy]   ← Tier 4 (if available)
  │
阶段 2: SCP03 引导 (无 SE050 依赖)
  │
  ├─ crypto_random_bytes()     ← 使用 Tier 2 (MCU TRNG)
  │    └─ hal_trng_read()
  │         └─ RNGA_ER 寄存器 (硬件真随机数)
  │
  ├─ se050_scp03_open_session()
  │    ├─ 用 host_challenge 发送 INIT UPDATE
  │    ├─ 推导会话密钥
  │    └─ EXTERNAL AUTHENTICATE → SCP03 通道建立
  │
阶段 3: 正常操作
  │
  ├─ crypto_random_register_se050()
  │
  ├─ crypto_random_bytes()     ← 使用 Tier 1 (SE050 HW TRNG)
  │    └─ SCP03 GET CHALLENGE APDU
  │
阶段 4: SCP03 关闭
  │
  ├─ crypto_random_unregister_se050()
  │    └─ 回退到 Tier 2 (MCU TRNG)
  │
  ├─ crypto_random_deinit()
  │    └─ hal_trng_deinit()
```

---

## 5. 回退行为矩阵

| 场景 | SE050 | MCU RNG | mbedTLS | OS | 结果 |
|------|-------|---------|---------|----|------|
| 正常裸机 (S32K3) | ✅ | ✅ | ❌ | ❌ | MCU TRNG (引导) / SE050 (运行) |
| 引导阶段 (SCP03 前) | ❌ | ✅ | ❌ | ❌ | MCU TRNG (Tier 2) |
| SE050 故障 (运行时) | ❌ | ✅ | ❌ | ❌ | MCU TRNG (Tier 2) |
| 主机开发 (macOS) | ✅ | ✅* | ✅ | ✅ | SE050 (Tier 1) |
| 主机开发 (无 SE050) | ❌ | ✅* | ✅ | ✅ | MCU TRNG (Tier 2) |
| 所有硬件 TRNG 故障 | ❌ | ❌ | ✅ | ✅ | mbedTLS (Tier 3) |
| 纯软件 (无 TRNG) | ❌ | ❌ | ❌ | ✅ | OS 熵 (Tier 4) |
| **所有源不可用** | ❌ | ❌ | ❌ | ❌ | **返回错误，设备不启动** |

*\* 主机构建时 MCU TRNG 通过软件 CSPRNG 模拟，不具备硬件熵质量*

---

## 6. 安全属性

### 6.1 消除的弱点

| 旧问题 | 旧值 | 新行为 |
|--------|------|--------|
| SCP03 host_challenge 硬编码回退 | `memcpy(buf, &0x12345678, 8)` | MCU TRNG (RNGA) 硬件随机数 |
| 无裸机 TRNG 路径 | 依赖 OS 或 mbedTLS | S32K312 RNGA 硬件 TRNG |
| 单点故障 | SE050 挂起则无熵源 | 四层自动回退 |

### 6.2 保留的安全措施

- **自检**: MCU TRNG 初始化时运行 stuck-bit + 白噪声检测
- **运行时健康检测**: 每 1024 次 crypto_random_bytes 调用运行一次
- **周期性 health_test API**: 可供安全监控任务调用
- **无弱回退**: 所有熵源耗尽时返回错误

---

## 7. 编译配置

MCU TRNG 的启用由以下预处理器定义控制：

| 定义 | 效果 |
|------|------|
| `TARGET_S32K3` | 强制启用 MCU TRNG（裸机 S32K3） |
| `USE_MCU_TRNG` | 显式启用 MCU TRNG 支持 |
| `__arm__` / `__ARMCC_VERSION` | ARM 目标自动启用 |
| `CRYPTO_RANDOM_NO_MCU_TRNG` | 禁用 MCU TRNG（覆盖自动检测） |

在 CMakeLists.txt 或 Makefile 中:

```cmake
# S32K3 裸机构建
add_definitions(-DTARGET_S32K3)

# 或通过 MCAL stubs 集成
target_include_directories(ccc_protocol PRIVATE
    ${MCAL_STUBS_DIR}/include
)
```

---

## 8. 测试矩阵

| 测试场景 | 方法 | 预期 |
|---------|------|------|
| MCU TRNG 初始化 | 调用 `hal_trng_init()` | 返回 `HAL_TRNG_OK` |
| MCU TRNG 读取 | 调用 `hal_trng_read(buf, 32)` | buf 填充 32 非零字节 |
| MCU TRNG 自检通过 | 调用 `hal_trng_self_test()` | 返回 `HAL_TRNG_OK` |
| crypto_random MCU TRNG tier | 在 `crypto_random_init()` 后检查源 | `CRYPTO_RANDOM_SOURCE_MCU_TRNG` |
| SCP03 引导使用 MCU TRNG | 在 `se050_scp03_open_session()` 调用前，验证熵源 | Tier 2 (MCU TRNG) |
| MCU TRNG 回退到 mbedTLS | 禁用 RNGA + 调用 `crypto_random_bytes()` | 成功，源为 mbedTLS |
| RNGA 寄存器读取 | 手动检查 `S32K312_RNGA_SR` | 自检后稳定 |
| RNGA 超时测试 | 阻止 RNGA 响应、强制超时 | `HAL_TRNG_ERR_TIMEOUT` |
| TRNG 未初始化调用 | `hal_trng_read()` 前不调 init | `HAL_TRNG_ERR_NOT_INIT` |
| NULL buf 调用 | `hal_trng_read(NULL, 32)` | `HAL_TRNG_ERR_PARAM` |
| zero length 调用 | `hal_trng_read(buf, 0)` | `HAL_TRNG_ERR_PARAM` |
| stuck-bit 检测 | 模拟 RNGA 返回全零 | `HAL_TRNG_ERR_STUCK_BIT` |
| RNGA SOF 错误 | 模拟 SR.SOF 置位 | `HAL_TRNG_ERR_HW` |

---

## 9. 文件变更清单

```
A  mcal_stubs/include/Trng.h                (6787 bytes)  — 新 TRNG HAL 头文件
A  mcal_stubs/src/trng_stub.c               (9670 bytes)  — 新 TRNG 驱动实现
M  mcal_stubs/include/Mcal.h                — 添加 #include "Trng.h"
M  include/crypto_random.h                   — 添加 CRYPTO_RANDOM_SOURCE_MCU_TRNG + 注释
M  src/security/crypto_random.c              — 添加 MCU TRNG Tier 2 + init/deinit 探测
M  src/security/se050_scp03.c               — 更新注释 [P1-1]→[P1-4], 确认无硬编码回退
```

---

## 10. 遗留问题

### 10.1 SCP03 回退密钥 (非本 P1-4 范围)

`security.c` 中 `sec_encrypt()`/`sec_decrypt()` 在 SCP03 未建立时使用全零密钥：

```c
static const uint8_t fallback_key[16] = {0};    /* DEV ONLY */
```

这是 SCP03 通道状态问题，非 TRNG 问题。建议 P0-1 修复：SCP03 未建立时直接返回错误。

### 10.2 主机 TRNG 仿真的安全性

主机构建时 `hal_trng_read()` 使用 xorshift128+ 软件 PRNG，不具备硬件的真随机性质。此模式**仅用于开发测试**，生产构建必须设置 `TARGET_S32K3` 以启用寄存器级 RNGA 访问。

### 10.3 FIPS 140-2 连续随机数检测 (CRNGT)

当前自检仅包含 stuck-bit 检测和白噪声检查。若需 FIPS 认证，应在 `hal_trng_read()` 中实现全量 CRNGT（逐次比较 + 故障计数器）。

---

**报告结束** — P1-4 MCU TRNG (S32K312 RNGA) 集成完成。
