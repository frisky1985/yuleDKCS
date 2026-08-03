/*
 * hooks.c - FreeRTOS application hooks for static allocation build
 */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "Uart_Cfg.h"

/* Static allocation for the Idle task. */
static StackType_t IdleTaskStack[ configMINIMAL_STACK_SIZE ];
static StaticTask_t IdleTaskTCB;

void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE * puxIdleTaskStackSize )
{
    *ppxIdleTaskTCBBuffer   = &IdleTaskTCB;
    *ppxIdleTaskStackBuffer = IdleTaskStack;
    *puxIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

#if ( configUSE_TIMERS == 1 )
/* Static allocation for the Timer service task. */
static StackType_t TimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];
static StaticTask_t TimerTaskTCB;

void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     configSTACK_DEPTH_TYPE * puxTimerTaskStackSize )
{
    *ppxTimerTaskTCBBuffer   = &TimerTaskTCB;
    *ppxTimerTaskStackBuffer = TimerTaskStack;
    *puxTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}
#endif /* configUSE_TIMERS */

void vApplicationMallocFailedHook( void )
{
    Uart_WriteString( "MALLOC_FAIL\n" );
    __asm volatile ( "bkpt #0" );
    for ( ;; )
    {
    }
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;

    Uart_WriteString( "STACK_OVERFLOW\n" );
    __asm volatile ( "bkpt #0" );
    for ( ;; )
    {
    }
}
