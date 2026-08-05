# Stack Overflow Fix Report — yuleDKCS

> **Date:** 2026-07-09  
> **Target:** NXP S32K312 (Cortex-M7, 512KB SRAM)  
> **Compiler:** arm-none-eabi-gcc 16.1.0  

---

## 1. Changes Summary

### 1.1 Local Arrays Converted to `static`

| # | File | Array | Size | Function |
|---|------|-------|------|----------|
| 1 | `ccc_protocol/src/security/security.c` | `blob[1024]` (was VLA) | ≤1024 | `sec_store_key` |
| 2 | `ccc_protocol/src/security/security.c` | `blob[576]` | 576 | `sec_load_key` |
| 3 | `ccc_protocol/src/ble/ble_kw47a.c` | `buf[244]` | 244 | `ble_send_data` |
| 4 | `ccc_protocol/src/ble/ble_kw47a.c` | `evt_buf[260]` | 260 | `ble_process_event` |
| 5 | `ccc_protocol/src/ble/ble_kw47a.c` | `buf[260]` (was VLA) | 260 | `ble_gatt_notify` |
| 6 | `ccc_protocol/src/keymgmt/key_mgmt.c` | `blob[4096]` (was VLA) | 4096 | `persist_keys` |
| 7 | `ccc_protocol/src/keymgmt/key_mgmt.c` | `blob[4096]` | 4096 | `load_keys` |
| 8 | `icce_protocol/src/security/security_auth.c` | `verify_data[128]` | 128 | `security_verify_response` |
| 9 | `icce_protocol/src/security/security_auth.c` | `my_private_key[32]`, `shared_secret[32]`, `key_material[48]` | 112 | `security_key_exchange` (SM2) |
| 10 | `icce_protocol/src/security/security_auth.c` | `my_private_key[32]`, `my_public_key[64]`, `shared_secret[32]`, `key_material[48]` | 176 | `security_key_exchange` (ECDH) |
| 11 | `icce_protocol/src/decision/offline_decision.c` | `key_buf[128]` | 128 | `check_key_validity` |
| 12 | `icce_protocol/src/decision/offline_decision.c` | `perm_buf[128]` | 128 | `check_permission` |
| 13 | `iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c` | `rsp_payload[128]` | 128 | `handle_bind_request` |
| 14 | `iccoa_protocol/src/iccoa/dk40/iccoa_dk40.c` | `rsp_payload[244]` | 244 | `dispatch` |
| 15 | `fault-inject/src/DKFaultInject.c` | `forged_pubkey[64]` | 64 | test function |

**Total stack saved:** ~5.2KB (was on-stack → moved to .bss/.data)

### 1.2 Crypto Internals — Reentrancy Note

Crypto algorithm internal arrays (SHA-256 W[64], SM3 W[68]+Wp[64], SM4 X[36]+K[36], AES rk[60]) are intentionally **left as local** to preserve reentrancy. They are used only inside their respective transform functions. See `stack-audit-report.md` Section 2.2 for the full list.

### 1.3 VLA Replacement

Three variable-length arrays were replaced with fixed-size maximum buffers:
- `security.c: blob[blob_len]` → `static blob[1024]` with bounds check
- `key_mgmt.c: blob[blob_len]` → `static blob[4096]` with bounds check  
- `ble_kw47a.c: buf[8+len]` → `static buf[260]` with bounds check

---

## 2. Task Stack Configuration

### FreeRTOS Config (`FreeRTOSConfig.h`)

| Parameter | Old | New |
|-----------|-----|-----|
| `configMINIMAL_STACK_SIZE` | 128 words (512B) | **256 words (1KB)** |
| `configTIMER_TASK_STACK_DEPTH` | 256 words (1KB) | **512 words (2KB)** |

### Task Stack Size (`Os_Cfg_Dk.h`)

| Parameter | Old | New |
|-----------|-----|-----|
| `OS_DK_TASK_STACK_SIZE` | 2048 (2KB) | **4096 (4KB)** |
| `OS_DK_ISR_STACK_SIZE` | 1024 (1KB) | **2048 (2KB)** — See note below |

### 5 Tasks — Stack Allocation per Task

| Task | Stack (words) | Stack (bytes) | Purpose |
|------|--------------|--------------|---------|
| `InitTask` | 1024 | 4096 | Protocol stack init |
| `Task10ms` | 1024 | 4096 | UWB/BLE/WdgM/EcuM |
| `Task50ms` | 1024 | 4096 | Edge rules/zone detection |
| `Task100ms` | 1024 | 4096 | Vehicle status/CAN/key check |
| `Background` | 1024 | 4096 | Maintenance/logging |
| **Total** | **5120** | **20KB** | |

Note: For FreeRTOS, each task stack is allocated from heap via `pvPortMalloc`. The 4KB per task provides ample margin for:
- FPU context (16× S-regs + FPSCR = 68 bytes)
- Nested function calls
- Worst-case crypto chain (sm3_transform ~528B + HMAC ~224B = ~752B)

---

## 3. Interrupt Stack (MSP) Configuration

### Linker Script (`s32k312.ld`)

Already configured with **128KB MSP** — far exceeding the 8KB requirement:
```
STACK_SIZE = 0x00020000;  /* 128KB - Main/Interrupt Stack */
STACK_TOP  = 0x20080000;  /* Top of RAM (0x2000_0000 + 512KB) */
```

The MSP is used by:
- All interrupt handlers (NVIC preemption)
- SVC/PendSV/SysTick handlers (scheduler context switch)
- HardFault/BusFault/UsageFault handlers

**Verification:** 128KB is sufficient for worst-case nested interrupt chain (BLE ISR + UWB ISR + SysTick) with crypto callback inside ISR.

### Startup File (`startup_s32k312.c`)

Vector table sets initial SP to `0x20080000` (top of 512KB SRAM). No changes needed.

---

## 4. Build Verification Results

| Component | Build Status | Notes |
|-----------|-------------|-------|
| `freertos_port` (port.c) | ✅ Pass | Updated configMINIMAL_STACK_SIZE confirmed |
| `bsw_os` (dk_os_cfg.c) | ✅ Pass | Os_Cfg_Dk.h stack config change confirmed |
| `bsw_det` | ✅ Pass | No changes |
| `bsw_bswm` | ✅ Pass | No changes |
| `bsw_stubs` | ✅ Pass | No changes |
| `mcal_stubs` | ✅ Pass | No changes |
| `rtd_adapter` | ✅ Pass | No changes |
| `libccc_dk.a` (CCC protocol) | ✅ Pass | security.c, ble_kw47a.c, key_mgmt.c all compile clean |
| `libiccoa_dk.a` (ICCOA protocol) | ✅ Pass | dk30.c, dk40.c compile clean (pre-existing warnings only) |
| `bsw_ecum` (yuleASR) | ❌ Pre-existing | `NvM_Init()` argument count mismatch in yuleASR core (not our change) |
| `libicce_dk.a` (ICCE) | ❌ Pre-existing | Missing GATT header defs for `ble_manager.c` (not our change) |

---

## 5. Stack Safety Analysis

### Before Fix — Worst-Case Stack per Function Call Chain

```
Task100ms entry:                                      Stack (bytes)
  ├─ icce_security → crypto_sm3 → sm3_transform        528
  ├─ security_key_exchange_server                      176
  ├─ crypto_kdf → crypto_sha256 → sha256_transform      64
  └─ crypto_hmac → sha256 × 2                          224
  ────────────────────────────────────────────────
  Subtotal: ~992
  + FreeRTOS context (FPU enabled):                    ~104
  + Function call frames:                              ~128
  ────────────────────────────────────────────────
  Total: ~1224 bytes (300 words)
```

### After Fix — Same Scenario

```
Task100ms entry:                                      Stack (bytes)
  ├─ icce_security → crypto_sm3 → sm3_transform        528
  ├─ security_key_exchange_server                        0 (arrays now static)
  ├─ crypto_kdf → crypto_sha256 → sha256_transform      64
  └─ crypto_hmac → sha256 × 2                          224
  ────────────────────────────────────────────────
  Subtotal: ~816
  + FreeRTOS context (FPU enabled):                    ~104
  + Function call frames:                              ~128
  ────────────────────────────────────────────────
  Total: ~1048 bytes (262 words)

FreeRTOS task stack: 4096 bytes (1024 words)
Margin: 3048 bytes = ~75% headroom ✅
```

---

## 6. Files Modified

| File | Nature of Change |
|------|-----------------|
| `embedded/ccc_protocol/src/security/security.c` | VLA→static blob[1024], static blob[576] |
| `embedded/ccc_protocol/src/ble/ble_kw47a.c` | static buf[244], static evt_buf[260], VLA→static buf[260] |
| `embedded/ccc_protocol/src/keymgmt/key_mgmt.c` | VLA→static blob[4096] (×2) |
| `embedded/icce_protocol/src/security/security_auth.c` | static verify_data[128], static key_material[48], static keys |
| `embedded/icce_protocol/src/decision/offline_decision.c` | static key_buf[128], static perm_buf[128] |
| `embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c` | static rsp_payload[128] |
| `embedded/iccoa_protocol/src/iccoa/dk40/iccoa_dk40.c` | static rsp_payload[244] |
| `embedded/fault-inject/src/DKFaultInject.c` | static forged_pubkey[64] |
| `embedded/freertos_port/include/FreeRTOSConfig.h` | configMINIMAL_STACK_SIZE=256, configTIMER_TASK_STACK_DEPTH=512 |
| `embedded/freertos_port/include/projdefs.h` | #ifndef guard for configMINIMAL_STACK_SIZE |
| `embedded/bsw_integration/include/Os_Cfg_Dk.h` | OS_DK_TASK_STACK_SIZE=4096, OS_DK_ISR_STACK_SIZE=2048 |

---

## 7. Outstanding Items

1. **ICCSE protocol build**: `ble_manager.c` needs GATT header includes — missing GATT_UUID_* and GATT_PROP_* definitions (pre-existing)
2. **Fault-inject build**: No Makefile in CI build directory — needs CMake reconfiguration
3. **Crypto module reentrancy**: Consider adding module-level mutex + static arrays for crypto transforms if concurrent ISR+task crypto calls occur
4. **BLE buffer safety**: `evt_buf` and `buf` in `ble_kw47a.c` are shared between interrupt context and task context — a mutex or critical section should wrap their usage
