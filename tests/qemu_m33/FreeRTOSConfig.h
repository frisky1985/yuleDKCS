/*
 * FreeRTOSConfig.h - FreeRTOS kernel configuration for QEMU mps2-an521 (Cortex-M33)
 *
 * Purpose: Verify FreeRTOS scheduling on a real ARMv8-M (Cortex-M33) core
 *          inside QEMU, as a proxy for the S32K312 MCU bring-up.
 *
 * Port:     ARM_CM33_NTZ (Non-TrustZone) - we run purely in Secure state.
 * Toolchain: arm-none-eabi-gcc (freestanding, no libc)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ( 20000000UL )
/* 注意: 不定义 configSYSTICK_CLOCK_HZ — 让 port.c 走 #ifndef 分支,
 * SysTick 使用内核时钟 (CLKSOURCE=1, 20MHz → 1ms tick)。
 * 若定义它, port.c 走 #else 分支用外部时钟 (CLKSOURCE=0),
 * QEMU 6.2 mps2-an521 的 STCLK=32768Hz → tick 变 0.6s, 任务卡死。 */
#define configTICK_RATE_HZ                      ( 1000 )
#define configMAX_PRIORITIES                    ( 5 )
#define configMINIMAL_STACK_SIZE                ( 128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 32 * 1024 ) )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TIMERS                        0
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( 256 )
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_STACK_OVERFLOW_CHECK          1
#define configCHECK_FOR_STACK_OVERFLOW          2

/* V11 API inclusion switches (default 0 in FreeRTOS.h - must opt in). */
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetTickCount                1

/*-----------------------------------------------------------
 * ARMv8-M (Cortex-M33) port configuration (V11 kernel)
 *----------------------------------------------------------*/
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0
/* CPU boots and stays in Secure state (QEMU mps2-an521 Secure ITCM).
 * This selects portINITIAL_EXC_RETURN = 0xFFFFFFFD (thread/PSP/secure)
 * instead of 0xFFFFFFBC (non-secure) which faults on a secure-only CPU. */
#define configRUN_FREERTOS_SECURE_ONLY          1
#define configENABLE_ACCESS_CONTROL_LIST        0
#define configENABLE_BTI                        0
#define configENABLE_PAC                        0
#define configENABLE_MVE                        0
#define configNUMBER_OF_CORES                   1
/* TICK_TYPE_WIDTH_32_BITS == 1 (defined in FreeRTOS.h) */
#define configTICK_TYPE_WIDTH_IN_BITS           1
#define configSTACK_DEPTH_TYPE                  uint32_t
#define configSYSTEM_CALL_STACK_SIZE            256
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    190
#define configKERNEL_INTERRUPT_PRIORITY         255
/* Vector table entries 11/14 already point at SVC_Handler/PendSV_Handler,
 * but QEMU keeps VTOR at 0 after reset, so the installation check fails.
 * Disable the check (vector table is correct in the image). */
#define configCHECK_HANDLER_INSTALLATION        0

/*-----------------------------------------------------------
 * Hook functions
 *----------------------------------------------------------*/
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

/*-----------------------------------------------------------
 * Assert
 *----------------------------------------------------------*/
#include <stdint.h>
extern void vAssertCall( void );
#define configASSERT( x )                       \
    if( ( x ) == 0 )                            \
    {                                           \
        vAssertCall();                          \
    }

/*-----------------------------------------------------------
 * Compiler / section definitions (GCC)
 *----------------------------------------------------------*/
#define portasmHANDLE_INTERRUPT                 vPortHandleInterrupt

#endif /* FREERTOS_CONFIG_H */
