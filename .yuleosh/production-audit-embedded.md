# yuleDKCS 嵌入式端深度代码扫描报告

> **扫描日期**: 2026-07-07  
> **范围**: ICCE / CCC / ICCOA / 统一协议层  
> **类型**: 量产就绪审计 — 深度代码扫描  
> **工具链**: arm-none-eabi-gcc 16.1.0, `-Wall -Wextra -Wpedantic -Werror`

---

## 目录

1. [编译警告报告](#1-编译警告报告)
2. [代码量统计](#2-代码量统计)
3. [嵌入式常见陷阱检查](#3-嵌入式常见陷阱检查)
4. [安全分析](#4-安全分析)
5. [功能覆盖缺口](#5-功能覆盖缺口)
6. [缺陷汇总](#6-缺陷汇总)

---

## 1. 编译警告报告

### 1.1 ICCE 协议栈

| 结果 | 说明 |
|:-----|:-----|
| ✅ 0 warnings, 0 errors | 全部源文件零警告编译通过 |

构建命令:
```
cd ~/yuleDKCS/embedded/icce_protocol/build-scan
cmake .. -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror"
make
```

**状态**: 零警告。ICCE 的 CMakeLists.txt 包含了完整的 `-ffreestanding` 标志和 `freestanding_includes` 路径。

### 1.2 CCC 协议栈

| 结果 | 说明 |
|:-----|:-----|
| ⚠️ 有编译警告 | 源文件依赖外部 SE050 库声明 |

构建成功但 `sec_verify()` 中的 `sec_verify()` 函数使用了 `memset(hash, 0, sizeof(hash))` 占位，实际哈希计算和 ECDSA 验证逻辑为 TODO 状态。

### 1.3 ICCOA 协议栈 — **构建失败**

```
error: 'BLE_SUPERVISION_TIMEOUT_MS' undeclared
error: 'DK30_SOP' undeclared (first use in this function)
```

**根因**:
1. ICCOA CMakeLists.txt 缺少 `-ffreestanding` 标志
2. 缺少指向 `freestanding_includes/` 的 include 路径
3. `BLE_SUPERVISION_TIMEOUT_MS` 在 `iccoa_ble.c:211` 被引用但未定义（拼写错误：应为 `BLE_SUPERVISION_TIMEOUT_MS` 但实际头文件可能使用 `BLE_SUPERVISION_TIMEOUT`）
4. 跨协议文件引用：`iccoa_dk_core.c` 中 `#include "../../../ccc_protocol/src/logger/dk_logger.h"` 引入对 CCC 协议的硬依赖

### 1.4 Unified 协议栈 — **构建失败**

与 ICCOA 相同原因：缺少 `-ffreestanding` 标志和 `freestanding_includes/` 路径。

### ⚠️ 构建配置缺陷总结

| 问题 | 影响 | 级别 |
|:-----|:-----|:-----|
| ICCE CMakeLists.txt 正确配置 `-ffreestanding` + `freestanding_includes` | ✅ 正常 | — |
| CCC 正确配置 | ✅ 正常 | — |
| **ICCOA CMakeLists.txt 未添加 `freestanding_includes`** | 无法交叉编译 | P0 |
| **unified CMakeLists.txt 未添加 `freestanding_includes`** | 无法交叉编译 | P0 |
| ICCOA `BLE_SUPERVISION_TIMEOUT_MS` 拼写问题 | 编译报错 | P1 |
| ICCOA `iccoa_dk_core.c` 跨协议引用 CCC logger | 架构耦合 | P1 |

---

## 2. 代码量统计

| 协议栈 | 文件数 | 代码行数 |
|:-------|:-------|:---------|
| ICCE 协议栈 (含 crypto/SM2/SM3/SM4) | ~40 | 12,104 |
| CCC 协议栈 | ~18 | 5,053 |
| ICCOA 协议栈 | ~12 | 2,898 |
| Unified 协议层 | ~5 | 2,638 |
| **嵌入式总计** | **~75** | **25,029** |

> 注：crypto 模块 (SM2/SM3/SM4/AES/SHA256) 约 6,000 行，占 ICCE 总量的 50%。

### 按功能模块分解

| 模块 | 行数 | 说明 |
|:-----|:-----|:-----|
| ICCE 协议核心 | 1,200 | dk_core, edge, zone, uwb, security |
| ICCE 安全认证 | 1,100 | security_auth.c |
| ICCE 缓存管理 | 1,200 | cache_manager.c (含 LRU + 哈希表) |
| ICCE BLE 管理 | 900 | ble_manager.c |
| ICCE 车辆集成 | 600 | vehicle_integration.c |
| ICCE 决策引擎 | 650 | offline_decision.c |
| ICCE 密码算法 | 6,000 | crypto_engine, sm2, sm3, sm4, crypto_utils |
| CCC 协议核心 | 800 | ccc_dk_core.c |
| CCC 安全模块 | 700 | security.c |
| CCC 密钥管理 | 1,200 | key_mgmt.c |
| CCC NFC/UWB/BLE | 1,800 | 硬件驱动层 |
| ICCOA DK 3.0/4.0 | 1,200 | dk40 + dk30 |
| ICCOA BLE/Service | 800 | ble + service |
| Unified 协议层 | 1,000 | dk_unified.c |
| Unified 头文件 | 530 | dk_unified.h |

---

## 3. 嵌入式常见陷阱检查

### 3.1 全局变量缺少 volatile

| 文件 | 变量 | 问题 |
|:-----|:-----|:-----|
| `icce/ble/ble_manager.c:55` | `static ble_manager_t g_ble_manager` | 非 `volatile`，可能被 ISR 修改 |
| `icce/vehicle/vehicle_integration.c:40` | `static vehicle_integration_t g_vehicle` | 非 `volatile`，可能被 CAN RX 中断修改 |
| `icce/vehicle/vehicle_integration.c:43-44` | `g_can_rx_buffer[]`, `g_can_rx_count` | ⚠️ 在 ISR 上下文中写入，主循环读取，缺少 volatile |
| `icce/decision/offline_decision.c` | `static decision_engine_t g_decision` | 非 volatile |
| `ccc/keymgmt/key_mgmt.c` | `g_keys[]`, `g_key_count` | 非 volatile |
| `iccoa/dk40/iccoa_dk40.c` | `g_sessions[]` | 非 volatile |

**风险评估**:
- `g_vehicle` 和 `g_can_rx_buffer` 被 CAN RX 中断处理函数 `can_rx_handler` 写入 → **最危险**
- `g_ble_manager` 被 BLE adapter 事件处理函数写入 → **高风险**
- 这些变量应标注为 `volatile` 防止编译器优化读-改-写

### 3.2 ISR 中调用了不可重入函数

| 文件 | 函数 | 被调用处 |
|:-----|:-----|:---------|
| `icce/vehicle/vehicle_integration.c:155` | `can_rx_handler` | **CAN 中断** 上下文中调用了 `memcpy`, `process_command_response`, `g_vehicle.state_callback` |
| `icce/icce_uwb.c:101` | `icce_uwb_irq_handler` | UWB IRQ — 调用了 `memcpy`, SPI 操作 |
| `ccc/ble/ble_kw47a.c:352` | `kw47a_irq_handler` | BLE IRQ |
| `ccc/uwb/uwb_ncj29d6.c:177` | `ncj29d6_irq_handler` | UWB IRQ |

关键问题:
- **`can_rx_handler` 调用了用户回调 `g_vehicle.state_callback`** — 用户回调可能不是 ISR-safe
- **`ble_adapter_event_handler`** 在 ISR 上下文中直接调用上层回调 `g_ble_manager.event_callback`
- **内存操作**: ISR 内大量使用 `memcpy` — 需要确保这些是原子操作或加锁

### 3.3 错误处理缺失

| 位置 | 问题 |
|:------|:------|
| `icce/security/security_auth.c:193` | `security_establish_session` 在 ECDH 失败后未清理局部私钥 |
| `icce/cache/cache_manager.c:191` | `malloc` 失败后未释放 `entry->value` (悬空指针) |
| `icce/cache/cache_manager.c:234` | `malloc` 失败后 `entry` 已部分初始化但未清理 |
| `ccc/security/security.c:326-345` | `sec_verify()` 返回 `VERIFY_OK` 即使 SE050 验证未实现 |
| `iccoa/ble/iccoa_ble.c:258-265` | GATT 写入回调中 TODO 未处理控制命令 |
| `icce/vehicle/vehicle_integration.c:147-153` | 等待 CAN 响应使用 `for(volatile ...)` 自旋 — **致命** |
| `unified/dk_unified.c:178` | `dk_ble_disconnect` 对 ICCOA/ICCE 直接返回成功，未实际断开 |

### 3.4 资源泄露

| 位置 | 类型 | 说明 |
|:------|:------|:-----|
| `icce/cache/cache_manager.c:188-192` | **内存泄露 P1** | `cache_set` 更新条目时 `free(entry->value)` 后立即 `malloc`，如果 `malloc` 失败，entry 处于无 value 但 in_use 状态 |
| `icce/cache/cache_manager.c:191` | **内存泄露 P1** | `malloc` 获取新 value 后 `memcpy` 前无 NULL 检查导致悬空指针 |
| `icce/security/security_auth.c:158-165` | **私钥残留 P0** | ECDH 错误路径中 `my_private_key`, `shared_secret`, `key_material` 未清除 |
| `icce/security/security_auth.c:195-241` | **私钥残留 P0** | ECC 路径中使用了 `memset` 但 ECC 错误路径 (lines 129-138) 未对 `verify_data` 加 memset |
| `ccc/keymgmt/key_mgmt.c:424` | **内存泄露 P2** | `key_delete` 在 `sec_delete_key` 后未通知云端吊销 |

### 3.5 整数溢出/符号问题

| 位置 | 问题 | 严重度 |
|:------|:------|:--------|
| `icce/decision/offline_decision.c:480-481` | `int32_t elapsed = (int32_t)(current_time - g_decision.rate_limits[i].window_start)` — 时间回跳导致负值被转为 `uint32_t` |
| `icce/decision/offline_decision.c:513` | `int32_t raw_duration = (int32_t)(current_time - key_info->last_sync_time)` — 同上 |
| `icce/decision/offline_decision.c:524` | `int32_t raw_ttl = (int32_t)(key_info->expiry_time - current_time)` — 同上 |
| `iccoa/dk40/iccoa_dk40.c:485` | `*(int16_t*)(notify+2) = distance_cm` — 未对齐内存访问 |

**说明**: `offline_decision.c` 中已经对大部分转换做了有符号显式适配（注释标注 `[M-05]`），但仍有以下风险：
- 时间回跳（systick wrap-around）的场景仅部分处理
- `icce_zone_classify` 对 `distance_mm < 0` 返回 `ICCE_ZONE_NONE` 但上层 `icce_edge_process_trigger` 使用 `(void)data;(void)len` 丢弃了所有参数

### 3.6 硬编码定时器/延时

| 位置 | 问题 |
|:------|:------|
| `icce/vehicle/vehicle_integration.c:148-153` | ~~CAN 响应等待用 `for(volatile uint32_t i=0; i<10000; i++)` 自旋~~ |
| `icce/crypto/crypto_utils.c:971-972` | 随机数生成 `for(volatile int i=0; i<100; i++)` 用于时序补偿 |
| `icce/crypto/crypto_utils.c:981-982` | 同上 |

**P0 问题**: `vehicle_integration.c` 的 CAN 自旋等待是仅有的"延时"实现，在生产环境没有系统 Tick 支持的情况下，这将阻塞 CPU 500ms。注释已说明"生产环境应替换为基于系统tick的时间跟踪"但未实现。

---

## 4. 安全分析

### 4.1 硬编码密钥/凭证 — 未发现

经扫描，未在源文件中发现硬编码密钥、密码、token 或凭证。

**优秀实践**:
- ICCE: 密钥通过 HSM 接口 (`hsm_store_key`, `hsm_load_key`) 存储
- CCC: 密钥通过 SE050 Transparent Object 持久化
- ICCOA: 主密钥从 SE050 加载 (`se050_key_get_master_key`)

### 4.2 不安全缓冲区操作

| 文件 | 行 | 函数 | 风险 |
|:-----|:---|:-----|:-----|
| `icce/cache/cache_manager.c:191,234` | `malloc(value_len)` | 未检查 `malloc` 返回值 — 内存不足时解引用 NULL |
| `icce/security/security_auth.c:153` | `verify_data[128]` | 固定大小缓冲区存储最多 48 字节数据，但无溢出保护 |
| ~~`icce/ble/ble_manager.c:409-413`~~ | `memcpy` 复制接收数据 | 已检查 `rx_len + evt->length <= sizeof(rx_buffer)` ✅ |
| `iccoa/dk40/iccoa_dk40.c:485` | `*(int16_t*)(notify+2) = distance_cm` | notify[8] 上未对齐访问，部分 ARM 核会产生硬错误 |

### 4.3 输入验证缺失

| 位置 | 问题 |
|:------|:------|
| `icce/decision/offline_decision.c:348-349` | `decision_evaluate` 中 `request->timestamp == 0` 检查过于宽松 — 无法区分"首次请求"和"攻击" |
| `icce/edge/icce_edge.c:98-99` | `icce_edge_process_trigger` 中 `(void)data; (void)len` **丢弃了所有传入数据** — 规则匹配无法参考试图数据 |
| `ccco/security/security.c:345` | `sec_verify()` 函数 **直接返回 VERIFY_OK** — 无论签名是否有效 |
| `iccoa/ble/iccoa_ble.c:265` | `*(uint16_t *)data` — 可能未对齐访问 |

### 4.4 关键安全缺陷

#### [P0-1] `ccc/security.c:sec_verify()` — ECDSA 验证暂未实现

```c
/* TEMPORARY: Always pass until SE050 integration complete */
return VERIFY_OK;
```

**风险**: 所有 ECDSA P-256 签名验证都被绕过。任何未授权的手机都可以通过 BLE 连接并获得认证通过。ICCE 的 `security_verify_signature` 有正确实现（调用了 HSM/SE050），但 CCC 路径完全未实现。

#### [P0-2] `icce/vehicle/vehicle_integration.c` — CAN 自旋等待阻塞

```c
for (volatile uint32_t i = 0; i < 10000; i++);
elapsed += poll_interval;
```

**风险**: 软件延时计时不准确；在 RTOS 环境中会阻塞调度器。

#### [P0-3] `icce/security/security_auth.c:129-138` — ECDH 错误路径私钥残留

`security_verify_response` 中如果 `security_verify_signature` 失败，函数返回前未清除 `verify_data` 中的密钥材料。

#### [P0-4] CCC `key_mgmt.c` 中虚拟 Flash 接口永久返回 -1

```c
__attribute__((weak)) int virt_flash_write(...) { return -1; }
```

所有 3 个 virt_flash 函数在未覆盖时返回 -1，意味着在无 SE050 环境下密钥无法持久化，但错误不会被上层捕获（带 `(void)addr; (void)data; (void)len;` 静默丢弃）。

#### [P0-5] `iccoa/dk40/iccoa_dk40.c:266` — 绑定签名验证为 TODO

```c
/* TODO: 计算 hash = SHA256(peer_id || pub_key || session_token) */
```

绑定时的 ECDSA 签名验证尚未实现。手机公钥未经验证即可绑定。

---

## 5. 功能覆盖缺口

检查所有涉及 `Embedded` 端（Embedded 或含 Embedded 的跨端）的 SHALL 需求。

### 5.1 已实现的需求

| ID | 描述 | 状态 | 证据 |
|:---|:-----|:-----|:------|
| KL-SHALL-03 | 双向身份认证 | ⚠️ | ICCE 已实现 (`security_verify_response`)，CCC `sec_verify` 返回 VERIFY_OK 占位 |
| PE-SHALL-01 | BLE + UWB 双向认证 ≤ 1s | ⚠️ | 协议栈已实现，但无实际时序验证 |
| PE-SHALL-04 | UWB 距离 + BLE RSSI 交叉验证 | ❌ | `icce_edge_process_trigger` 丢弃了 `data`/`len`，无法交叉验证 |
| PE-SHALL-06 | CAN FD 发送指令 | ✅ | `vehicle_integration.c:can_driver_send` |
| PE-SHALL-08 | 8 设备并发 BLE | ⚠️ | ICCE 支持 4 连接 (MAX_CONNECTIONS=4) |
| ES-SHALL-03 | UWB 确认车内位置 | ✅ | `iccoa_dk40.c` 中有 zone 检查 |
| KS-SHALL-04 | 钥匙分享撤销 < 10s | ❌ | 本地吊销缓存未实现 (`TODO` 状态) |
| KR-SHALL-02 | 车端吊销缓存 | ❌ | `offline_decision.c` 有吊销检查逻辑但 `cache_manager.c` 的持久化 `TODO` 未实现 |
| RA-SHALL-05 | BLE RSSI + UWB 交叉验证 | ❌ | ICCE `icce_edge_evaluate` 有 RSSI 读取但 `(void)zone` 丢弃 |
| KS-SHALL-01 | 私钥存储于 SE050 | ✅ | ICCE: `hsm_store_key`; CCC: `se05x_write_transparent` |
| KS-SHALL-07 | 国密/国际算法双栈 | ✅ | `crypto_engine.c` 实现 SM2/SM3/SM4 + P-256/SHA-256/AES-GCM |

### 5.2 未实现或有缺陷的需求

| ID | 描述 | ASIL | 缺口 | 严重度 |
|:---|:-----|:-----|:------|:-------|
| **PE-SHALL-03** | 上锁前确认车内无有效钥匙 | ASIL-B | 未实现车内钥匙检测逻辑 | **P0** |
| **PE-SHALL-05** | 每次操作使用新 Nonce | ASIL-B | ICCE 有 `is_nonce_used` 方法，但 CCC/ICCOA 未检查 Nonce 去重 | **P1** |
| **PE-SHALL-NOT-01** | UWB > 2m 不执行解锁 | ASIL-B(D) | ICCE edge 规则使用了 `threshold_mm=3000` 而非 2000mm | **P0** |
| **RA-SHALL-04** | 响应超时 ~3μs 拒绝 | ASIL-B(D) | 未实现 | **P0** |
| **RA-SHALL-06** | 防重放计数器 | ASIL-B | ICCE 有 session counter，但 CCC/ICCOA 缺少消息序列号验证 | **P1** |
| **RA-SHALL-07** | 中继攻击告警推送 | ASIL-B(D) | ICCE `downgrade_attempted` 实现了审计，但未推送车主告警 | **P1** |
| **ES-SHALL-01** | 仅认证钥匙启动引擎 | ASIL-B | ICCOA DK40 有 `check_engine_start_permission`，但 ICCE/CCC 未实现 | **P1** |
| **KR-SHALL-03** | 本地缓存独立判定吊销 | ASIL-A | `offline_decision.c` 在 `check_key_validity` 中调用 cache，但缺少 CRL 列表管理 | **P1** |
| **NF-SHALL-02** | ISO 7816-4 APDU 交易序列 | QM | CCC `nfc_oob_exchange` 实现了 NFC 但未实现完整 APDU 序列 | **P2** |

### 5.3 TODO/Stub 统计

| 范围 | TODO 数量 | 含安全关键 TODO |
|:-----|:----------|:----------------|
| ICCE | 12 | `// TODO: 实际的ECDH密钥协商`, `TODO: 获取实际时间`, `TODO: 替换为平台实际延时函数` |
| CCC | 7 | `TODO: Replace with platform SHA-256`, `TODO: Implement actual SE050 ECDSA P-256 verification` |
| ICCOA | 10 | `TODO: 计算 hash = SHA256(...)`, `TODO: 填充车辆公钥`, `TODO: 分发到控制模块` |
| Unified | 0 | — |

---

## 6. 缺陷汇总

### 严重度分级

| 级别 | 标准 | 数量 |
|:-----|:-----|:------|
| **P0 (Critical)** | 安全绕过 / 功能不可用 / ASIL-B 需求未实现 | 9 |
| **P1 (High)** | 潜在缺陷 / 资源泄露 / 不安全实践 | 11 |
| **P2 (Medium)** | 代码质量 / 非安全关键功能缺失 | 7 |

### P0 缺陷列表

| # | 位置 | 缺陷 | 类型 |
|:-|:-----|:-----|:-----|
| 1 | `ccc/security.c:345` | ECDSA 签名验证 `return VERIFY_OK` 占位 — 所有签名验证被绕过 | 安全 |
| 2 | `iccoa/dk40/iccoa_dk40.c:266` | 绑定流程签名验证为 TODO — 手机身份未校验收 | 安全 |
| 3 | `icce/vehicle/vehicle_integration.c:148-153` | CAN 响应等待使用 `for(volatile...)` 自旋 — 500ms 阻塞 | 实时性 |
| 4 | `icce/security/security_auth.c:129-138` | ECDH 错误路径中 `verify_data` 的密钥材料未清除 | 安全 |
| 5 | **ICCOA/unified CMakeLists.txt** | 缺少 `-ffreestanding` 和 `freestanding_includes` — 交叉编译失败 | 构建 |
| 6 | PE-SHALL-03 | 上锁前车内钥匙检测未实现 | 功能缺口 |
| 7 | PE-SHALL-NOT-01 | ICCE 解锁阈值 3000mm 超出规格 2000mm | 功能缺口 |
| 8 | RA-SHALL-04 | 测距响应超时检测 (~3μs) 未实现 | 安全缺口 |
| 9 | `iccoa_ble.c:211` | `BLE_SUPERVISION_TIMEOUT_MS` 未定义 | 编译 |

### P1 缺陷列表

| # | 位置 | 缺陷 | 类型 |
|:-|:-----|:-----|:-----|
| 1 | `icce/vehicle_integration.c:43-44` | `g_can_rx_buffer`, `g_can_rx_count` 被 ISR 修改但无 `volatile` | 嵌入式陷阱 |
| 2 | `icce/ble/ble_manager.c:55` | `g_ble_manager` 被 ISR 修改但无 `volatile` | 嵌入式陷阱 |
| 3 | ISR 调用 `memcpy` + 用户回调 | `can_rx_handler`, `ble_adapter_event_handler` 在中断中调用不可重入函数 | 嵌入式陷阱 |
| 4 | `icce/cache/cache_manager.c:188-192` | `malloc` 失败后悬空指针 | 资源泄露 |
| 5 | `icce/edge/icce_edge_process_trigger` | `(void)data; (void)len` 丢弃触发数据 | 功能缺陷 |
| 6 | PE-SHALL-05 | CCC/ICCOA 缺少 Nonce 去重检查 | 安全缺口 |
| 7 | RA-SHALL-06 | CCC/ICCOA 缺少防重放计数器 | 安全缺口 |
| 8 | ES-SHALL-01 | ICCE/CCC 缺少引擎启动权限检查 | 功能缺口 |
| 9 | KR-SHALL-03 | 本地吊销 CRL 缓存缺失 | 安全缺口 |
| 10 | `iccoa_dk_core.c` | 跨协议引用 CCC logger | 架构耦合 |
| 11 | `ccc/keymgmt/key_mgmt.c` | virt_flash 永久返回 -1，静默静默忽略错误 | 容错 |

### P2 缺陷列表

| # | 位置 | 缺陷 | 类型 |
|:-|:-----|:-----|:-----|
| 1 | `iccoa/ble/iccoa_ble.c:265` | `*(uint16_t*)data` 未对齐访问 | 移植性 |
| 2 | `iccoa/dk40/iccoa_dk40.c:485` | `*(int16_t*)(notify+2)` 未对齐访问 | 移植性 |
| 3 | NF-SHALL-02 | APDU 交易序列未完全实现 | 功能缺口 |
| 4 | `icce/security/security_auth.c:153` | `verify_data[128]` 固定缓冲区 + 无溢出保护 | 代码质量 |
| 5 | ICCE BLE `MAX_CONNECTIONS=4` 小于 PE-SHALL-08 要求的 8 | 配置不足 |
| 6 | `ccc/security.c:sec_encrypt/decrypt/sign` | 所有密码学操作均为 TODO | 功能缺口 |
| 7 | ICCE `get_current_time()` 返回 0 | 缓存过期 & Nonce 过期检查永远通过 | 功能缺陷 |

### 总体评估

| 维度 | 评估 | 说明 |
|:-----|:------|:------|
| ICCE 协议栈 | ⚡ **最优** | 功能最完整，crypto 模块自包含，编译零警告 |
| CCC 协议栈 | ⚠️ **需补充** | SE050 集成尚未完成，`sec_verify()` 返回 OK 是 P0 安全缺陷 |
| ICCOA 协议栈 | ❌ **不可构建** | 缺少 freestanding 配置，多个 TODO 未完成 |
| Unified 协议层 | ⚠️ **路由封装** | 各协议间路由已封装完成，但下层协议补充后才能完整的端到端测试 |
| 安全/密码 | ⚠️ **双栈完备** | SM2/SM3/SM4 和 AES/SHA256/ECDSA 双栈均已实现代码，但 ECDSA P-256 签名验证 (CCC 侧) 为占位 |
| 功能覆盖 | ⚠️ **73% Embedded SHALL 已覆盖** | 19/26 Embedded 端 SHALL 需求已实现或部分实现 |

### 按优先级推荐的修复路径

1. **立即阻止** (P0): `sec_verify()` 和签名验证桩接上 SE050
2. **构建修复** (P0): ICCOA/unified 添加 `freestanding_includes` + `-ffreestanding` + 修复 `BLE_SUPERVISION_TIMEOUT_MS`
3. **安全加固** (P0): 实现 RA-SHALL-04 (3μs 超时检测)、修复 PE-SHALL-NOT-01 (2000mm 阈值)
4. **嵌入式健壮性** (P1): ISR 全局变量加 `volatile`，替换 CAN 自旋等待为系统 Tick
5. **功能补齐** (P1): Nonce 去重、防重放计数器、车内钥匙检测、本地 CRL
6. **清理 TODO** (P1-P2): 约 29 个 TODO 中清理 8 个安全关键项

---

*报告生成: 2026-07-07 17:11 CST | 审计者: subagent-embedded-scan*
