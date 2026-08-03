    .syntax unified
    .cpu cortex-m33
    .thumb

/*==========================================================================
 * Vector table - placed at start of FLASH (0x10000000, Secure ITCM alias)
 *========================================================================*/
    .section .isr_vector, "a"
    .align 4
    .globl _estack
    .word _estack
    .word Reset_Handler
    .word Default_Handler       /* NMI */
    .word Default_Handler       /* HardFault */
    .word Default_Handler       /* MemManage */
    .word Default_Handler       /* BusFault */
    .word Default_Handler       /* UsageFault */
    .word Default_Handler       /* SecureFault */
    .word 0                     /* reserved */
    .word 0                     /* reserved */
    .word 0                     /* reserved */
    .word SVC_Handler           /* SVCall (FreeRTOS) */
    .word Default_Handler       /* DebugMonitor */
    .word 0                     /* reserved */
    .word PendSV_Handler        /* PendSV (FreeRTOS) */
    .word SysTick_Handler       /* SysTick (FreeRTOS) */
    .space 0x80                 /* remaining IRQs -> default */

/*==========================================================================
 * Reset_Handler - C runtime init: data copy + bss zero + call main
 *========================================================================*/
    .section .text
    .thumb_func
    .globl Reset_Handler
Reset_Handler:
    ldr     sp, =_estack

    /* Relocate vector table to Secure ITCM alias (0x10000000).
     * QEMU boots reading the vector table from 0x10000000, but the VTOR
     * register itself resets to 0 and writes to it are ignored by QEMU.
     * FreeRTOS' vStartFirstTask reads VTOR to find the initial MSP, so
     * mirror the vector table to address 0x0 as well - then [0x0] holds
     * the correct initial SP (0x20020000) and SVC_Handler is still taken
     * from the secure vector table at 0x10000000. */
    ldr     r0, =0xE000ED08
    ldr     r1, =0x10000000
    str     r1, [r0]

    ldr     r0, =0x00000000
    ldr     r1, =0x10000000
    movs    r2, #16
vt_mirror_loop:
    ldr     r3, [r1]
    str     r3, [r0]
    adds    r0, #4
    adds    r1, #4
    subs    r2, #1
    bne     vt_mirror_loop

    /* Copy .data from FLASH (load) to RAM (runtime) */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
data_loop:
    cmp     r0, r1
    bge     data_done
    ldr     r3, [r2]
    str     r3, [r0]
    adds    r0, #4
    adds    r2, #4
    b       data_loop
data_done:

    /* Zero .bss */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
bss_loop:
    cmp     r0, r1
    bge     bss_done
    str     r2, [r0]
    adds    r0, #4
    b       bss_loop
bss_done:

    bl      main
loop_forever:
    b       loop_forever

/*==========================================================================
 * Default_Handler - infinite loop (should never be hit in a passing test)
 *========================================================================*/
    .thumb_func
    .globl Default_Handler
Default_Handler:
    b       Default_Handler

    .end
