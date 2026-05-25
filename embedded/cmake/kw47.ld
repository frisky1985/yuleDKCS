/* KW47B42Z 链接脚本 */
/* 内存配置: 512KB SRAM, 2MB Flash */

MEMORY
{
    /* 内核SRAM (512KB) */
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
    
    /* 内核Flash (2MB) */
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 2048K
    
    /* FlexNVM (如果有) */
    FLEX_NVM (rwx) : ORIGIN = 0x10000000, LENGTH = 256K
}

/* 堆栈大小 */
STACK_SIZE = 0x10000; /* 64KB */

SECTIONS
{
    /* 中断向量表 */
    .vectors :
    {
        KEEP(*(.vectors))
        KEEP(*(.vectors.*))
    } > FLASH

    /* 代码段 */
    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        *(.rodata.*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    /* ARM unwinding 段 */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* 预初始化数据段 */
    __data_flash_start = LOADADDR(.data);
    
    .data : AT(__data_flash_start)
    {
        . = ALIGN(4);
        __data_start = .;
        *(.data*)
        *(.data.*)
        KEEP(*(.jcr*))
        . = ALIGN(4);
        __data_end = .;
    } > RAM

    /* BSS段 */
    .bss :
    {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /* 堆栈 */
    /* ARM惯例: __stack_top 指向内存最高地址, __stack_bottom 指向栈底(较低地址) */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __stack_bottom = __stack_top - STACK_SIZE;

    /* 堆 */
    end = __bss_end;
    __heap_start = __bss_end;
    __heap_end = __stack_top;

    /* 加载器排除段 */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}

/* 入口点 */
ENTRY(Reset_Handler)
