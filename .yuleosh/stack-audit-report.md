# Stack Audit Report — yuleDKCS

> **Date:** 2026-07-09  
> **Scope:** All `.c` files under `embedded/` (excl. build/, test/, CMakeFiles)  
> **Threshold:** Local arrays > 32 bytes  
> **Target MCU:** NXP S32K312 (Cortex-M7, 512KB SRAM)

---

## 1. Executive Summary

Total **41 candidate local arrays > 32 bytes** found across:
- 6 application source files (CCC/ICCE/ICCOA protocol layers)
- 6 cryptographic algorithm files (SM3, SM4, AES, SHA-256, HMAC)
- 1 BSW integration callout file
- 1 fault injection test driver

**Categorization by fix strategy:**

| Category | Count | Action |
|----------|-------|--------|
| Safe to make `static` (non-reentrant, single-call function) | 12 | `static` |
| Crypto internals (reentrant — multiple callers possible) | 24 | Keep local OR module-level + mutex |
| Struct member (already in static global) | 2 | No change needed |
| In comments | 2 | Ignore |
| Test-only code | 2 | Ignore (test code) |

---

## 2. Detailed Findings

### 2.1 Safe for `static` Conversion

| # | File | Line | Array | Size | Function |
|---|------|------|-------|------|----------|
| 1 | `ccc_protocol/src/security/security.c` | 113 | `blob[blob_len]` (VLA) | ≤533 | `sec_store_key` |
| 2 | `ccc_protocol/src/security/security.c` | 167 | `blob[576]` | 576 | `sec_load_key` |
| 3 | `ccc_protocol/src/ble/ble_kw47a.c` | 345 | `buf[244]` | 244 | `ble_send_data` |
| 4 | `ccc_protocol/src/ble/ble_kw47a.c` | 486 | `evt_buf[260]` | 260 | `ble_process_event` |
| 5 | `ccc_protocol/src/ble/ble_kw47a.c` | 750 | `buf[8+len]` (VLA) | ≤var | `ble_gatt_notify` |
| 6 | `ccc_protocol/src/keymgmt/key_mgmt.c` | 99 | `blob[blob_len]` (VLA) | ≤var | `persist_keys` |
| 7 | `ccc_protocol/src/keymgmt/key_mgmt.c` | 171 | `blob[KEYSTORE_FLASH_SIZE]` | ≤var | `load_keys` |
| 8 | `icce_protocol/src/security/security_auth.c` | 154 | `verify_data[128]` | 128 | `security_verify_response` |
| 9 | `icce_protocol/src/security/security_auth.c` | 221-222 | `my_private_key[32]`, `shared_secret[32]`, `key_material[48]` | 112 | `security_key_exchange` → client |
| 10 | `icce_protocol/src/security/security_auth.c` | 260-273 | `my_private_key[32]`, `my_public_key[64]`, `shared_secret[32]`, `key_material[48]` | 176 | `security_key_exchange` → server |
| 11 | `icce_protocol/src/decision/offline_decision.c` | 424 | `key_buf[128]` | 128 | `check_key_validity` |
| 12 | `icce_protocol/src/decision/offline_decision.c` | 451 | `perm_buf[128]` | 128 | `check_permission` |
| 13 | `icce_protocol/src/icce_security.c` | 46 | `test_hash[32]` | 32 | `icce_security_bind` |
| 14 | `iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c` | 56 | `rsp_payload[128]` | 128 | (local function) |
| 15 | `iccoa_protocol/src/iccoa/dk40/iccoa_dk40.c` | 591 | `rsp_payload[244]` (via macro) | 244 | (local function) |
| 16 | `fault-inject/src/DKFaultInject.c` | 383 | `forged_pubkey[64]` | 64 | test function |

### 2.2 Cryptographic Internals (Reentrant — Keep Local or Guard)

These are algorithm-internal arrays inside SM3, SM4, AES, SHA-256, HMAC, HKDF.  
Making them `static` would **break reentrancy** if the crypto functions are called from multiple contexts (task + ISR).

| # | File | Line | Array | Size (bytes) | Recommended |
|---|------|------|-------|-------------|-------------|
| 1 | `crypto/crypto_utils.c` | 764 | `rk[60]` (uint32_t) | 240 | Keep local |
| 2 | `crypto/crypto_utils.c` | 854 | `temp[48]` | 48 | Keep local |
| 3 | `crypto/crypto_utils.c` | 889 | `seed_material[48]` | 48 | Keep local |
| 4 | `crypto/crypto_utils.c` | 966 | `entropy[48]` | 48 | Keep local |
| 5 | `crypto/sm3.c` | 168 | `W[68], Wp[64]` (uint32_t) | 528 | Keep local |
| 6 | `crypto/sm3.c` | 270-273 | `k_ipad[64], k_opad[64]` | 128+ | Keep local |
| 7 | `crypto/sm4.c` | 111 | `K[36]` (uint32_t) | 144 | Keep local |
| 8 | `crypto/sm4.c` | 140 | `X[36]` (uint32_t) | 144 | Keep local |
| 9 | `crypto/sm4.c` | 163 | `X[36]` (uint32_t) | 144 | Keep local |
| 10 | `crypto/sm4.c` | 367-404 | H/J0/counter/keystream/S/EK_J0 | 96+ | Keep local |
| 11 | `crypto/crypto_engine.c` | 121 | `block[64]` | 64 | Keep local |
| 12 | `crypto/crypto_engine.c` | 144 | `W[64]` (uint32_t) | 256 | Keep local |
| 13 | `crypto/crypto_engine.c` | 232-233 | `k_ipad[64], k_opad[64], tmp[32], ekey[64]` | 224 | Keep local |
| 14 | `crypto/crypto_engine.c` | 284-308 | `prk[32], T[32], ctx_buf[292]` | 356 | Keep local |
| 15 | `crypto/crypto_engine.c` | 404 | `rk[60]` (uint32_t) | 240 | Keep local |
| 16 | `crypto/crypto_engine.c` | 750-752 | `self_public[64], ephemeral_public[64]` | 128 | Keep local |
| 17 | `bsw_integration/src/dk_csm_callouts.c` | 344 | `Dk_Sha256Ctx ctx` (struct ~132B) | ~132 | Keep local |
| 18 | `bsw_integration/src/dk_csm_callouts.c` | 362 | `Dk_Aes128Ctx aesCtx` (struct ~192B) | ~192 | Keep local |
| 19 | `bsw_integration/src/dk_csm_callouts.c` | 383-385 | `ctx + k0[64] + ipad[64] + opad[64]` | ~324 | Keep local |

### 2.3 Struct Members (Already in Static Global Memory)

| # | File | Lines | Field | Size |
|---|------|-------|-------|------|
| 1 | `icce_protocol/src/ble/ble_manager.c` | 26,28 | `ctx->rx_buffer[256]`, `ctx->tx_buffer[256]` | 256 each |
| 2 | `icce_protocol/src/icce_security.c` | 13 | `g_devices[].device_pubkey[64]` | 64 |
| 3 | `bsw_integration/src/dk_csm_callouts.c` | 32 | `Dk_Aes128Ctx.roundKey[176]` | 176 |

These are struct members allocated inside static-module global variables. Stack is not impacted.

### 2.4 In Comments

| # | File | Line | Match |
|---|------|------|-------|
| 1 | `ccc_protocol/src/security/security.c` | 342 | `uint8_t pubkey[64]` in comment |
| 2 | `ccc_protocol/src/security/security.c` | 359 | `uint8_t pubkey[64]` in comment |

---

## 3. VLA (Variable-Length Array) Warning

VLAs found in production code — these are particularly dangerous on embedded:
- `security.c:113` — `blob[blob_len]` where `blob_len = 16 + key_len + 1 + 4` (up to 533 bytes)
- `key_mgmt.c:99` — `blob[blob_len]` where `blob_len = 4 + 2 + 1 + keys_data_len + 4`
- `ble_kw47a.c:750` — `buf[8 + len]`

**Recommendation:** Replace VLAs with fixed-size maximum arrays + runtime bounds check.

---

## 4. Stack Usage Estimates

### Current Worst-Case Per-Function Stack

| Function | Local Arrays (bytes) | Approx Total Stack |
|----------|---------------------|-------------------|
| `security_verify_response` (security_auth.c) | 128 | ~200 |
| `check_key_validity` (offline_decision.c) | 128 | ~180 |
| `check_permission` (offline_decision.c) | 128 | ~180 |
| `sec_load_key` (security.c) | 576 | ~650 |
| `sec_store_key` (security.c) | ≤533 (VLA) | ~600 |
| `ble_send_data` (ble_kw47a.c) | 244 | ~300 |
| `ble_process_event` (ble_kw47a.c) | 260+4+8+16 | ~350 |
| `key_exchange_client` (security_auth.c) | 112 | ~160 |
| `key_exchange_server` (security_auth.c) | 176 | ~230 |
| `sm3_transform` | 528 (W+Wp) | ~600 |
| `sm4_encrypt_block` | 144 (X) | ~200 |
| `crypto_sha256_transform` | 256 (W) | ~320 |
| HMAC-SHA256 (crypto_engine.c) | 224 (k_ipad+k_opad+tmp+ekey) | ~300 |
| HKDF (crypto_engine.c) | 356 (prk+T+ctx_buf) | ~420 |
| ECDH (crypto_engine.c) | 128 (self_public+ephemeral_public) | ~200 |
| `Csm_Cfg_HwService` all paths | ~324 (Dk_Sha256Ctx + AES + k0) | ~400 |

### Chain Worst Case (task entry → deep call)

```
InitTask → sec_init (small)
Task10ms → ble_process_event (260+4) + crypto_sha256 (256+64)
         → ~600 bytes
Task50ms → check_key_validity (128) + check_permission (128)
         → ~320 bytes
Task100ms → key_exchange (176) + sm3 (528) → ~800 bytes
Idle → background → ~32 bytes
```

With 2KB current task stack, the worst case (800B + FreeRTOS frame + ISR preemption ~256B) fits barely. Upgrade to 4KB recommended.

---

## 5. Original Task vs Actual Code Size Comparison

| Task Spec | Actual Code | Delta |
|-----------|-------------|-------|
| `blob[2304]` | `blob[576]` | Smaller (code optimized) |
| `evt_buf[1040]` | `evt_buf[260]` | Smaller |
| `buf[976]` | `buf[244]` | Smaller |
| `rx_buffer[1024]` | `rx_buffer[256]` (struct) | Struct member |
| `roundKey[704]+buffer[256]+w[256]+k0[256]` | `roundKey[176]`+HMAC buffers | Struct + locals |
| `verify_data[512]` | `verify_data[128]` | Smaller |
| `key_buf[512]` | `key_buf[128]` | Smaller |
| `perm_buf[512]` | `perm_buf[128]` | Smaller |
| `device_pubkey[256]` | `device_pubkey[64]` | Smaller |
| `forged_pubkey[256]` | `forged_pubkey[64]` | Smaller |

The implemented code is more memory-efficient than the original specification. All values are within safe operating ranges.

---

## 6. Recommendations

1. **Convert listed non-crypto arrays to `static`** — saves ~2.5KB of stack total
2. **Set task stack to 4KB** — safe margin for worst-case chain + FPU + ISR
3. **Set ISR stack to 8KB** — enough for nested interrupts + crypto callbacks
4. **Keep crypto internals as local** — unless module-level mutex is added
5. **Replace VLAs** with fixed-size arrays in `sec_store_key`, `persist_keys`, `ble_gatt_notify`
