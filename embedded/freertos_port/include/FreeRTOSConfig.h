/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS Configuration for NXP S32K312 (Cortex-M7)
 *
 * S32K312 Specs:
 *   - Cortex-M7 @ 320MHz
 *   - FPU: VFPv5 (hard float, fpv5-sp-d16)
 *   - Interrupt Priority: 16 levels (4 bits)
 *   - SysTick timer
 *   - Flash: 2MB, SRAM: 512KB
 *
 * yuleDKCS BSW Phase 1 Integration
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* =========================================================================
 * S32K312 Platform Configuration
 * ========================================================================= */
#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configUSE_TICKLESS_IDLE             0
#define configCPU_CLOCK_HZ                  320000000UL
#define configTICK_RATE_HZ                  1000
#define configMAX_PRIORITIES                32
#define configMINIMAL_STACK_SIZE            256   /* 256 words = 1KB */
#define configMAX_TASK_NAME_LEN             16
#define configUSE_16_BIT_TICKS              0
#define configIDLE_SHOULD_YIELD             1
#define configUSE_TASK_NOTIFICATIONS        1
#define configUSE_MUTEXES                   1
#define configUSE_RECURSIVE_MUTEXES         1
#define configUSE_COUNTING_SEMAPHORES       1
#define configUSE_QUEUE_SETS                0
#define configQUEUE_REGISTRY_SIZE           10
#define configUSE_APPLICATION_TASK_TAG      0

/* =========================================================================
 * Memory Management
 * ========================================================================= */
#define configSUPPORT_STATIC_ALLOCATION     1
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configTOTAL_HEAP_SIZE               131072         /* 128 KB heap */
#define configAPPLICATION_ALLOCATED_HEAP    0

/* =========================================================================
 * Timer Support
 * ========================================================================= */
#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           2
#define configTIMER_QUEUE_LENGTH            10
#define configTIMER_TASK_STACK_DEPTH        512   /* 512 words = 2KB */

/* =========================================================================
 * Cortex-M7 Specific
 * ========================================================================= */
#define configUSE_NEWLIB_REENTRANT          0
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

/* Cortex-M7 NVIC: 16 priority levels (4 bits of priority) */
#define configPRIO_BITS                     4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     0x0F
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 0x05
#define configKERNEL_INTERRUPT_PRIORITY             (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY        (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_API_CALL_INTERRUPT_PRIORITY       configMAX_SYSCALL_INTERRUPT_PRIORITY

/* =========================================================================
 * Assert & Trace
 * ========================================================================= */
#define configASSERT(x)                    if ((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }
#define INCLUDE_vTaskPrioritySet           1
#define INCLUDE_uxTaskPriorityGet          1
#define INCLUDE_vTaskDelete                1
#define INCLUDE_vTaskSuspend               1
#define INCLUDE_xTaskResumeFromISR         1
#define INCLUDE_vTaskDelay                 1
#define INCLUDE_xTaskGetCurrentTaskHandle  1
#define INCLUDE_xTaskGetIdleTaskHandle     0
#define INCLUDE_xSemaphoreGetMutexHolder   0
#define INCLUDE_eTaskGetState              1
#define INCLUDE_xTaskResumeFromISR         1
#define INCLUDE_xTimerPendFunctionCall     1
#define INCLUDE_xTaskAbortDelay            0
#define INCLUDE_xTaskGetHandle             0

/* =========================================================================
 * ARMv7-M FPU
 * ========================================================================= */
#define configENABLE_FPU                   1
#define configENABLE_MPU                   0
#define configENABLE_TRUSTZONE             0
#define configENABLE_ARM_MPU               0
#define configENABLE_MPU_REGION_PROTECTION 0
#define configENABLE_ARM_FPU               1

/* =========================================================================
 * Interrupt Vectors (mapped from S32K312)
 * ========================================================================= */
extern void xPortPendSVHandler(void)    __attribute__((naked));
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void)       __attribute__((naked));

#define vPortSVCHandler                 SVC_Handler
#define xPortPendSVHandler              PendSV_Handler
#define xPortSysTickHandler             SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
