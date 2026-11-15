/*
 * NXP KW47A Linker Script
 * Flash: 512KB, SRAM: 256KB
 * A/B OTA partition layout: Slot B 尾部让出 48KB 作为 NV 分区
 * (keystore / CRL / 产线锁定, 6 x 8KB 物理扇区), OTA 镜像上限 144KB,
 * 见 docs/security/nv-persistence-design.md。
 */

MEMORY
{
    /* Bootloader region (write-protected via SE050) */
    BOOT_HEADER  (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00000400  /* 1KB */
    BOOTLOADER   (rx)  : ORIGIN = 0x00000400, LENGTH = 0x0001FC00  /* 127KB */

    /* Application slots (A/B swap for OTA) */
    FLASH_A      (rx)  : ORIGIN = 0x00020000, LENGTH = 0x00030000  /* 192KB */
    FLASH_B      (rx)  : ORIGIN = 0x00050000, LENGTH = 0x00024000  /* 144KB (尾部 48KB 让给 NV) */

    /* Persistent storage (keystore, CRL, provisioning lock) */
    STORAGE      (rx)  : ORIGIN = 0x00074000, LENGTH = 0x0000C000  /* 48KB */

    SRAM                 : ORIGIN = 0x20000000, LENGTH = 0x00040000  /* 256KB */
}

SECTIONS
{
    .boot_header : ALIGN(4)
    {
        KEEP(*(.boot_header))
        . = ALIGN(4);
    } > BOOT_HEADER

    .text : ALIGN(4)
    {
        KEEP(*(.isr_vector))
        KEEP(*(.isr_handlers))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH_A

    .ARM.extab : ALIGN(4)
    {
        *(.ARM.extab*)
    } > FLASH_A

    .ARM.exidx : ALIGN(4)
    {
        *(.ARM.exidx*)
    } > FLASH_A

    .preinit_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH_A

    .init_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array*)))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH_A

    .fini_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH_A

    _sidata = LOADADDR(.data);

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH_A

    .bss : ALIGN(4)
    {
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > SRAM

    .stack : ALIGN(8)
    {
        . = . + 0x4000;
        _estack = .;
    } > SRAM

    /* Discard unwind info in release builds */
    /DISCARD/ : { *(.ARM.exidx*) }
}
