# yuleDKCS Fault Injection Integration Guide

## Overview

This document describes how to integrate the yuleDKCS Fault Injection framework
into the digital key protocol stacks (ICCE, CCC, ICCOA).

## Architecture

```
yuleOSH FaultInject (Layer 1+2)
    ├── CPU exception injection (HardFault, BusFault, etc.)
    └── Task-level fault injection (FreeRTOS notifications)
    
yuleDKCS DK FaultInject (Protocol Layer)
    ├── ICCE fault injectors
    ├── CCC fault injectors
    └── ICCOA fault injectors
```

## Compile-Time Guards

| Macro | Build Type | Effect |
|-------|-----------|--------|
| `DK_FAULT_INJECT_ENABLE=0` | Production | All injection code = no-ops |
| `DK_FAULT_INJECT_ENABLE=1` | Test/Debug | Full injection enabled |

## Protocol Integration Points

### ICCE Protocol (`icce_protocol/src/`)

**File: `icce_security.c`** — Signature Verification
```c
// In icce_security_auth(), before real verification:
if (dk_fi_is_active(DK_FI_ICCE_SIGN_TAMPERED)) {
    return ICCE_ERR_SECURITY;  // Simulate forged signature
}
```

**File: `icce_dk_core.c`** — Zone Transitions
```c
// In on_uwb_ranging(), before zone classification:
if (dk_fi_is_active(DK_FI_ICCE_DISTANCE_SPOOF)) {
    session->distance_mm = 500;  // Spoof interior distance
}
```

**File: `icce_edge.c`** — Edge Trigger Evaluation
```c
// In icce_edge_evaluate(), before action dispatch:
if (dk_fi_is_active(DK_FI_ICCE_ILLEGAL_TRANSITION)) {
    // Bypass zone validation check
}
```

### CCC Protocol (`ccc_protocol/src/`)

**File: `security/security.c`** — Secure Channel
```c
// In sec_scp03_open():
if (dk_fi_is_active(DK_FI_CCC_SECURE_CHANNEL_FAIL)) {
    return CCC_ERR_HARDWARE;  // Simulate SCP03 failure
}

// In sec_verify(), before actual verification:
if (dk_fi_is_active(DK_FI_CCC_SIGNATURE_BYPASS)) {
    return VERIFY_OK;  // Bypass — tests detection
}
if (dk_fi_is_active(DK_FI_CCC_CERT_TAMPER)) {
    return VERIFY_CERT_INVALID;
}
```

**File: `core/ccc_dk_core.c`** — State Machine
```c
// In transition_state():
if (dk_fi_is_active(DK_FI_CCC_ILLEGAL_STATE)) {
    // Allow INIT→UNLOCKED skip (normally invalid)
}
```

**File: `nfc/nfc_st25r501.c`** — NFC OOB
```c
// In nfc_oob_exchange():
if (dk_fi_is_active(DK_FI_CCC_NFC_OOB_CORRUPT)) {
    // Corrupt OOB buffer before returning
}
```

### ICCOA Protocol (`iccoa_protocol/src/`)

**File: `auth/iccoa_auth.c`** — Authentication
```c
// In iccoa_auth_verify():
if (dk_fi_is_active(DK_FI_ICCOA_HANDSHAKE_FAIL)) {
    return ICCOA_ERR_SECURITY;  // Force handshake failure
}
if (dk_fi_is_active(DK_FI_ICCOA_AUTH_TIMEOUT)) {
    return ICCOA_ERR_TIMEOUT;
}
```

**File: `iccoa/iccoa_dk_core.c`** — Downgrade Protection
```c
// In ble_data_handler():
if (dk_fi_is_active(DK_FI_ICCOA_DOWNGRADE_ATTACK)) {
    // Accept DK3.0 frame in DK4.0 mode (tests no_downgrade)
    // The real handler will reject it via existing protection
}
if (dk_fi_is_active(DK_FI_ICCOA_NO_DOWNGRADE_OFF)) {
    g_ctx.no_downgrade = 0;  // Disable downgrade protection
}
```

**File: `ble/iccoa_ble.c`** — BLE Data
```c
// In hal_ble_on_write(), before callback:
if (dk_fi_is_active(DK_FI_ICCOA_BLE_DATA_DROP)) {
    return;  // Silently drop received packet
}
if (dk_fi_is_active(DK_FI_ICCOA_HMAC_TAMPER)) {
    data[0] ^= 0xFF;  // Corrupt first byte
}
```

## Test Procedure

### Host-Side Test

```bash
# From embedded/fault-inject/
gcc -DDK_FAULT_INJECT_ENABLE=1 \
    -Iinc \
    -o test_runner \
    test/test_dk_fault_inject.c src/DKFaultInject.c -lm

./test_runner
```

### Embedded Test (arm-none-eabi)

```bash
# CMake build with fault injection enabled
cmake -B build -DDK_FAULT_INJECT_TESTS=ON -DDK_FAULT_INJECT_BUILD_TEST=ON
cmake --build build
# Flash to target and observe test results via UART/semihosting
```

### CMake Subdirectory Integration

```cmake
# In parent CMakeLists.txt:
option(DK_FAULT_INJECT_TESTS "Enable fault injection for test builds" OFF)
add_subdirectory(fault-inject)

target_link_libraries(your_target PRIVATE
    $<$<BOOL:${DK_FAULT_INJECT_TESTS}>:dk-fault-inject-test>
    $<$<NOT:$<BOOL:${DK_FAULT_INJECT_TESTS}>>:dk-fault-inject>
)
```

## Test Report

After running the full suite, results are stored in `dk_fi_get_results()`.
The `dk_fi_print_results()` function outputs to stdout.

For CI integration, results are also written to:
```
.yuleosh/fault-injection-report.md
```

## Safety Notes

- **NEVER** compile `DK_FAULT_INJECT_ENABLE=1` into production firmware.
- The production library target (`dk-fault-inject` without tests) compiles all
  functions to static inline no-ops, ensuring zero code size impact.
- All injector functions use compile-time guards that default to OFF.
