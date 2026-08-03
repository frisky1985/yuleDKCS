/*==========================================================================
 * qemu_m33.ld - Linker script for FreeRTOS on QEMU mps2-an521 (Cortex-M33)
 *
 * Verified memory map (QEMU 11.0.2 mps2-an521, from `info mtree`):
 *   ITCM (code, Secure alias): 0x10000000 - 0x1FFFFFFF  (QEMU: 2MB region)
 *   DTCM (data, NS alias):     0x20000000 - 0x2001FFFF  (QEMU: 128KB! NOT 2MB)
 *   UART0:                     0x40200000 (QEMU stdio serial0)
 *
 * CRITICAL: The DTCM is only 128KB in QEMU. Declaring 2MB here (as the
 * original script did) makes _estack = 0x20200000, which is OUT OF BOUNDS
 * and causes a Data Abort on the first push. Keep LENGTH = 128K.
 *========================================================================*/
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 2M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 0x20020000: top of DTCM */

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    . = ALIGN(4);
    . = . + 0x1000;              /* Heap (4 KB) */
    . = ALIGN(4);
    _end = .;

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.comment)
        *(.note*)
        *(.debug*)
    }
}
