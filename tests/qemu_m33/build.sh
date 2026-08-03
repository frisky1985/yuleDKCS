#!/bin/bash
#============================================================================
# build.sh - Build FreeRTOS QEMU M33 verification image and run it
#
# Usage:
#   ./build.sh          build only
#   ./build.sh run      build + run under QEMU (expect QEMU_M33_PASS)
#============================================================================
set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

CROSS=arm-none-eabi-gcc
CFLAGS="-mcpu=cortex-m33 -mthumb -mfloat-abi=softfp -mfpu=fpv5-sp-d16 -ffreestanding -nostdlib -O2 -Wall -Werror"
INCLUDES="-I. -Iinclude -Isrc -Ithird_party/FreeRTOS-Kernel/include -Ithird_party/FreeRTOS-Kernel/portable/GCC/ARM_CM33_NTZ/non_secure"

KERNEL_SRCS="
    third_party/FreeRTOS-Kernel/tasks.c
    third_party/FreeRTOS-Kernel/queue.c
    third_party/FreeRTOS-Kernel/list.c
    third_party/FreeRTOS-Kernel/timers.c
    third_party/FreeRTOS-Kernel/event_groups.c
    third_party/FreeRTOS-Kernel/stream_buffer.c
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
    third_party/FreeRTOS-Kernel/portable/MemMang/heap_4.c
"

APP_SRCS="
    src/main.c
    src/hooks.c
    src/Uart_Cfg.c
    src/libc_stubs.c
    src/startup_m33.s
"

echo "==> Building qemu_m33.elf ..."
$CROSS $CFLAGS $INCLUDES -T qemu_m33.ld $KERNEL_SRCS $APP_SRCS -o qemu_m33.elf

echo "==> Verify ELF ..."
arm-none-eabi-readelf -h qemu_m33.elf | grep -E "Entry|Machine"
arm-none-eabi-readelf -l qemu_m33.elf | grep LOAD

if [ "${1:-}" = "run" ]; then
    echo "==> Running under QEMU (mps2-an521) ..."
    qemu-system-arm -machine mps2-an521 -cpu cortex-m33 \
        -kernel qemu_m33.elf -nographic -serial stdio 2>&1 | head -40
fi

echo "==> Build OK"
