# QEMU M33 FreeRTOS Verification

Verifies FreeRTOS scheduling on a real ARMv8-M (Cortex-M33) core inside QEMU,
as a proxy for the S32K312 MCU bring-up.

## Why this exists

The Linux POSIX port of FreeRTOS **deadlocks after ~1.5s** on macOS/Linux
(official minimal demo, reproduced). It is not a reliable execution
environment for verifying the yuleASR OS layer. This QEMU target runs the
real FreeRTOS kernel on a real M33 core model.

## Memory map (verified against QEMU 11.0.2 mps2-an521)

| Region    | Address     | Size   | Notes                                     |
|-----------|-------------|--------|-------------------------------------------|
| ITCM S    | 0x10000000  | 2M     | Code (Secure alias)                       |
| DTCM NS   | 0x20000000  | **128K**| Data + stack — QEMU has only 128KB!       |
| UART0     | 0x40200000  | 4K     | QEMU stdio serial0                        |

## Pitfalls discovered

1. **DTCM is 128KB, not 2MB.** The original linker script declared 2MB,
   making `_estack = 0x20200000` out of bounds → Data Abort on first push.
   Fix: `LENGTH = 128K`, `_estack = 0x20020000`.
2. **Reset_Handler loads SP from a literal pool**, not just the vector table.
   Patching only the vector table's first word is NOT enough — the literal
   `ldr sp, =_estack` must also point inside DTCM.
3. **UART0 is at 0x40200000, not 0x40004000.** The SSE-200 reference address
   is different on the MPS2 AN521 board image. QEMU silently ignores writes
   to unmapped addresses (no fault, no output — very confusing).
4. **QEMU stdio is wired to UART0** (0x40200000), not UART4. Verified by
   probing all 5 UARTs.
5. **CMSDK UART needs BAUDDIV non-zero + CTRL TXEN** before TX works,
   otherwise QEMU logs `Tx enabled with invalid baudrate` and drops chars.
6. **QEMU boots the CPU in Secure state** and reads the reset vector from
   the Secure vector table at 0x10000000. Link the image at 0x10000000
   (Secure ITCM alias), not 0x00000000.
7. **FreeRTOS V11 on a secure-only CPU needs `configRUN_FREERTOS_SECURE_ONLY=1`**.
   Without it, `portINITIAL_EXC_RETURN = 0xFFFFFFBC` (non-secure return) is
   used, which faults on a secure-only core. With it, EXC_RETURN =
   0xFFFFFFFD (thread/PSP/secure) and the first task starts correctly.
8. **QEMU's M33 implements all 8 priority bits**, so
   `configMAX_SYSCALL_INTERRUPT_PRIORITY` must be **even** (bit0 is the
   sub-priority bit). 191 (0xBF) asserts; 190 (0xBE) works.
9. **VTOR reads 0 on QEMU even after writing it.** FreeRTOS' SVC start path
   reads VTOR to find the initial MSP. Mirror the vector table to address
   0x0 in Reset_Handler so `[0x0]` holds the correct initial SP.

## Expected output

```
QEMU_M33_START
B:<tick>:1
A:<tick>
B:<tick>:2
A:<tick>
B:<tick>:3
QEMU_M33_PASS
```

`QEMU_M33_PASS` + BKPT halt means the test passed. Any of
`ASSERT_FAIL` / `MALLOC_FAIL` / `STACK_OVERFLOW` / `TASK_A_FAIL` /
`TASK_B_FAIL` / `SCHED_FAIL` means it failed.

## Build & run

```bash
./build.sh run    # build + run under QEMU
```

Requires: `arm-none-eabi-gcc` (16.x), `qemu-system-arm` (11.x).
FreeRTOS-Kernel V11 vendored under `third_party/`.

## QEMU 版本兼容性

- **验证版本**: QEMU 11.0.2 (mps2-an521) — `QEMU_M33_PASS` 正常输出。
- **QEMU 6.2 (Ubuntu 22.04 apt)**: 曾出现 SysTick 不触发
  (`Timer with period zero, disabling`; 任务卡在 vTaskDelay)。**根因**:
  FreeRTOSConfig.h 定义了 `configSYSTICK_CLOCK_HZ` → port.c 走 `#else` 分支
  用外部时钟 (CLKSOURCE=0), QEMU 6.2 mps2-an521 的 STCLK=32768Hz →
  tick 变 0.6s。**修复**: 删除 configSYSTICK_CLOCK_HZ 定义, SysTick 用内核
  时钟 (20MHz → 1ms tick), 6.2 下 QEMU_M33_PASS 恢复正常。
- HIL 命令通道: 固件支持 UART RX 命令 (HIL:PING/GET_VERSION/LED/STATE/
  GET_TICKS/GET_UPTIME/BLE|NFC|UWB|SE050:STATUS), 见 src/main.c TaskHil。
- 状态机注入: HIL:SM:STATE|SET:<target>|ILLEGAL|RESET —
  状态机 IDLE→MONITORING→UNLOCKED→LOCKED, 非法转换 REJECT + 安全计数
  (FI-05 SIL 验证载体)。
