/*
 * main.c - FreeRTOS scheduling verification on QEMU mps2-an521 (Cortex-M33)
 *
 * Test: two tasks with different periods (500ms / 1000ms). After task B
 * completes 3 iterations, print PASS marker and halt via BKPT.
 *
 * Expected UART output:
 *   QEMU_M33_START
 *   B:<tick>:1
 *   A:<tick>
 *   B:<tick>:2
 *   A:<tick>
 *   B:<tick>:3
 *   QEMU_M33_PASS
 */
#include "FreeRTOS.h"
#include "task.h"
#include "Uart_Cfg.h"

#define TASK_A_PRIORITY     ( 1 )
#define TASK_B_PRIORITY     ( 1 )
#define TASK_A_STACK_SIZE   ( 520 )
#define TASK_B_STACK_SIZE   ( 520 )
#define TASK_B_MAX_ROUNDS   ( 3 )

static volatile uint32_t TaskB_Rounds = 0UL;

static void TaskA( void * arg )
{
    ( void ) arg;

    for ( ;; )
    {
        Uart_WriteString( "A:" );
        Uart_WriteDec( ( uint32_t ) xTaskGetTickCount() );
        Uart_WriteString( "\n" );
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
    }
}

static void TaskB( void * arg )
{
    ( void ) arg;

    for ( ;; )
    {
        TaskB_Rounds++;

        Uart_WriteString( "B:" );
        Uart_WriteDec( ( uint32_t ) xTaskGetTickCount() );
        Uart_WriteString( ":" );
        Uart_WriteDec( TaskB_Rounds );
        Uart_WriteString( "\n" );

        if ( TaskB_Rounds > TASK_B_MAX_ROUNDS )
        {
            Uart_WriteString( "QEMU_M33_PASS\n" );
            __asm volatile ( "bkpt #0" );
        }

        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

void vAssertCall( void )
{
    Uart_WriteString( "ASSERT_FAIL@" );
    Uart_WriteDec( ( uint32_t ) __builtin_return_address( 0 ) );
    Uart_WriteString( "\n" );
    __asm volatile ( "bkpt #0" );
    for ( ;; )
    {
    }
}

int main( void )
{
    Uart_Init();
    Uart_WriteString( "QEMU_M33_START\n" );

    if ( xTaskCreate( TaskA, "TaskA", TASK_A_STACK_SIZE, NULL,
                      TASK_A_PRIORITY, NULL ) != pdPASS )
    {
        Uart_WriteString( "TASK_A_FAIL\n" );
        return 1;
    }

    if ( xTaskCreate( TaskB, "TaskB", TASK_B_STACK_SIZE, NULL,
                      TASK_B_PRIORITY, NULL ) != pdPASS )
    {
        Uart_WriteString( "TASK_B_FAIL\n" );
        return 1;
    }

    vTaskStartScheduler();

    /* Should never reach here. */
    Uart_WriteString( "SCHED_FAIL\n" );
    for ( ;; )
    {
    }
}
