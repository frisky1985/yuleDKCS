/**
 * @file portmacro.h
 * @brief FreeRTOS Cortex-M7 Port Macros for S32K312
 *
 * ARMv7-M architecture with FPU (VFPv5, fpv5-sp-d16)
 * 16 priority levels (4-bit NVIC)
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Type Definitions
 * ========================================================================= */
#define portCHAR            char
#define portFLOAT           float
#define portDOUBLE          double
#define portLONG            long
#define portSHORT           short
#define portSTACK_TYPE      uint32
#define portBASE_TYPE       long

typedef portSTACK_TYPE      StackType_t;
typedef long                BaseType_t;
typedef unsigned long       UBaseType_t;

#if (configUSE_16_BIT_TICKS == 1)
    typedef uint16            TickType_t;
    #define portMAX_DELAY       (TickType_t)0xFFFF
#else
    typedef uint32            TickType_t;
    #define portMAX_DELAY       (TickType_t)0xFFFFFFFFUL
#endif

/* =========================================================================
 * Architecture Definitions
 * ========================================================================= */
#define portSTACK_GROWTH                 -1
#define portTICK_PERIOD_MS              ( (TickType_t) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT              8
#define portDONT_DISCARD                __attribute__((used))
#define portNOP()                       __asm volatile(" nop ")

/* =========================================================================
 * Critical Section Management
 * ========================================================================= */
extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
extern UBaseType_t uxPortSetInterruptMask(void);
extern void vPortClearInterruptMask(UBaseType_t);
extern BaseType_t xPortIsInsideInterrupt(void);

#define portSET_INTERRUPT_MASK()                uxPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK(uxMask)        vPortClearInterruptMask(uxMask)

#define portDISABLE_INTERRUPTS()                __asm volatile(" cpsid i " ::: "memory")
#define portENABLE_INTERRUPTS()                 __asm volatile(" cpsie i " ::: "memory")

/* Critical section macros */
#define portENTER_CRITICAL()                    vPortEnterCritical()
#define portEXIT_CRITICAL()                     vPortExitCritical()

/* =========================================================================
 * Interrupt Priority Configuration
 * ========================================================================= */
#define portINPUT_PRIORITY                      0x50
#define portLOWEST_INTERRUPT_PRIORITY           0xFF
#define portNVIC_SYSPRI2_REG                    (*((volatile uint32*) 0xE000ED20))
#define portNVIC_SHPR3_REG                      (*((volatile uint32*) 0xE000ED24))
#define portNVIC_SYSTICK_PRI                    0x50

/* =========================================================================
 * SVC / PendSV / SysTick priority config
 * ========================================================================= */
#define portNVIC_SVC_PRI                        ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define portNVIC_PENDSV_PRI                     ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define portNVIC_SYSTICK_PRI_VAL                ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* =========================================================================
 * FPU Context
 * ========================================================================= */
#if (configENABLE_FPU == 1)
    #define portFPU_ENABLED                     1
#else
    #define portFPU_ENABLED                     0
#endif

/* =========================================================================
 * Task Context
 * ========================================================================= */
#define portYIELD()                             __asm volatile(" svc 0 " ::: "memory")
#define portYIELD_FROM_ISR(xHigherPriorityTaskWoken)    if(xHigherPriorityTaskWoken != pdFALSE) { __asm volatile(" dsb \n isb \n" ::: "memory"); *(volatile uint32*)0xE000ED04 = (1UL << 28UL); }

/* =========================================================================
 * Function prototypes
 * ========================================================================= */
extern void vPortSVCHandler(void)       __attribute__((naked));
extern void xPortPendSVHandler(void)    __attribute__((naked));
extern void xPortSysTickHandler(void);
extern BaseType_t xPortStartScheduler(void) __attribute__((naked));
extern void vPortEndScheduler(void);

#define portTASK_FUNCTION(vTaskFunction, vParameters)   void vTaskFunction(void *pvParameters)
#define portTASK_FUNCTION_PROTO(vTaskFunction, vParameters)     void vTaskFunction(void *pvParameters)

/* =========================================================================
 * INCLUDE directives — use local headers from FreeRTOS
 * ========================================================================= */
#include "FreeRTOSConfig.h"

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
