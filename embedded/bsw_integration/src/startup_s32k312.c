/**
 * @file startup_s32k312.c
 * @brief S32K312 startup — Reset_Handler, vector table
 */
#include "Std_Types.h"

/* Forward declarations for handlers used in __Vectors[] */
void Reset_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* Default handler — overridable by port.c (SVC/PendSV/SysTick) */
__attribute__((weak)) void Default_Handler(void) { while(1); }

/* Aliases for handlers NOT provided elsewhere */
void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)__attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)  __attribute__((weak, alias("Default_Handler")));

/* SVC/PendSV/SysTick — provided by freertos_port/port.c (via xPort macros) */
void SVC_Handler(void)     __attribute__((weak));
void PendSV_Handler(void)  __attribute__((weak));
void SysTick_Handler(void) __attribute__((weak));

/**
 * Vector table — must map to flash_vector (0x400)
 */
__attribute__((section(".isr_vector"), used))
const uint32 __Vectors[16] = {
    0x20080000,
    (uint32)Reset_Handler,
    (uint32)NMI_Handler,
    (uint32)HardFault_Handler,
    (uint32)MemManage_Handler,
    (uint32)BusFault_Handler,
    (uint32)UsageFault_Handler,
    0, 0, 0, 0,
    (uint32)SVC_Handler,
    (uint32)DebugMon_Handler,
    0,
    (uint32)PendSV_Handler,
    (uint32)SysTick_Handler,
};

/**
 * Reset Handler
 */
void Reset_Handler(void) __attribute__((section(".text.startup"), naked, used));
void Reset_Handler(void) {
    /* Enable FPU on Cortex-M7 via CPACR at 0xE000ED88 */
    __asm volatile(
        "ldr r0, =0xE000ED88\n"
        "ldr r1, [r0]\n"
        "orr r1, r1, #(0xF << 20)\n"
        "str r1, [r0]\n"
        "dsb\n"
        "isb\n"
        ::: "r0", "r1", "memory");

    extern uint32 __data_load, __data_start, __data_end;
    extern uint32 __bss_start, __bss_end;
    for (uint32 *s = &__data_load, *d = &__data_start; d < &__data_end; ) *d++ = *s++;
    for (uint32 *d = &__bss_start; d < &__bss_end; ) *d++ = 0;

    __asm volatile("bl main" ::: "lr");
    while(1);
}
