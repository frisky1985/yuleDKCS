# Cross-Compile CI Report (yuleDKCS P1-3)

## Objective
Verify that embedded C sources using ARM Cortex-M architecture compile correctly with the ARM GNU Toolchain (`arm-none-eabi-gcc`), catching architecture-specific issues missed by native (Ubuntu gcc) builds.

## Changes Made

### New File: `.github/workflows/cross-compile-ci.yml`

A new GitHub Actions workflow that:

| Item | Detail |
|------|--------|
| **Trigger** | Push/PR to `main`, `develop`, `feat/**` affecting `embedded/` |
| **Runner** | `ubuntu-latest` with `armswdev/arm-none-eabi-gcc` container |
| **Toolchain** | `arm-none-eabi-gcc` (GNU ARM Embedded Toolchain) |
| **CPU target** | `cortex-m4` with `-mthumb` |
| **Language std** | `-std=c99` |
| **Warnings** | `-Wall -Wextra -Werror` |
| **Compile mode** | `-c` (compile only, no link), `-ffreestanding -nostdlib` |
| **Include paths** | All protocol `include/` dirs + `freestanding_includes/` + `system_architecture/` + BSP/MCAL/FreeRTOS headers |
| **Coverage** | All production `.c` files under `embedded/` (excludes `CMakeFiles/`, `build*/`, CMake compiler ID, test suites) |

### Source Modules Compiled

| Module | Directory | Files |
|--------|-----------|-------|
| ICCOA Protocol | `embedded/iccoa_protocol/src/` | 6 source files (core, auth, BLE, service, DK30/40) |
| CCC Protocol | `embedded/ccc_protocol/src/` | 8 source files (core, BLE, NFC, UWB, security, keymgmt) |
| ICCE Protocol | `embedded/icce_protocol/src/` | 13 source files (core, zone, edge, vehicle, UWB, security, crypto, BLE, cache, decision) |
| Unified Protocol | `embedded/unified_protocol/src/` | 1 source file (dk_unified.c) |
| BSW Integration | `embedded/bsw_integration/src/` | 17 source files (startup, configs, stubs, callbacks) |
| MCAL Stubs | `embedded/mcal_stubs/src/` | 2 source files (mcal_stubs, memif_impl) |
| FreeRTOS Port | `embedded/freertos_port/src/` | 1 source file (port.c) |
| Fault Injection | `embedded/fault-inject/src/` | 1 source file (DKFaultInject.c) |
| **Total** | | **~49 source files** |

### Verification
The workflow runs `arm-none-eabi-gcc --version` to confirm the toolchain is operational, then compiles each `.c` file to `.o`. All errors and warnings are captured in a log. Any compilation failure causes the workflow to exit non-zero.

## Impact
- **CI pipeline**: Adds ~2-3 minutes per `embedded/` change
- **No changes to existing workflows**: The `ci.yml` and `misra-ci.yml` remain untouched
- **Detection**: Catches ARM-specific issues: `int` width assumptions, unaligned access, `__attribute__((packed))` usage, implicit `extern int` (C99 forbids), and hardware register access patterns
- **Local testing**:
  ```bash
  docker run --rm -v $(pwd):/src armswdev/arm-none-eabi-gcc \
    arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c99 -Wall -Wextra -c \
    -I/src/embedded/freestanding_includes \
    -I/src/embedded/iccoa_protocol/include \
    /src/embedded/iccoa_protocol/src/iccoa/iccoa_dk_core.c
  ```

## References
- [ARM GNU Toolchain Downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- `docker.io/armswdev/arm-none-eabi-gcc:latest`
- Task: yuleDKCS P1-3
