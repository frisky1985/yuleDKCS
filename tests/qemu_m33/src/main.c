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

#include <string.h>

#define TASK_A_PRIORITY     ( 1 )
#define TASK_B_PRIORITY     ( 1 )
#define TASK_HIL_PRIORITY   ( 2 )
#define TASK_A_STACK_SIZE   ( 520 )
#define TASK_B_STACK_SIZE   ( 520 )
#define TASK_HIL_STACK_SIZE ( 520 )
#define TASK_B_MAX_ROUNDS   ( 3 )

#define HIL_VERSION_STRING  "1.3.0"

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

/* ---------------------------------------------------------------------
 * TaskHil — HIL 命令通道 (UART RX 轮询)
 *
 * 命令集 (host → 固件):
 *   HIL:PING         → HIL:PONG
 *   HIL:GET_VERSION  → HIL:VERSION:<ver>
 *   HIL:LED:1|0      → HIL:LED:ON|OFF
 *   HIL:STATE        → HIL:STATE:IDLE|<n>
 * 未知命令 → HIL:UNKNOWN:<cmd>
 * ------------------------------------------------------------------- */
static volatile uint32_t g_led_state = 0UL;
static volatile uint32_t g_hil_cmds  = 0UL;

/* -nostdlib 无 libc, 用内联字符串比较替代 strcmp */
static int hil_streq( const char * a, const char * b )
{
    while ( ( *a != '\0' ) && ( *b != '\0' ) && ( *a == *b ) )
    {
        a++;
        b++;
    }
    return ( int )( ( unsigned char ) *a - ( unsigned char ) *b );
}

static void TaskHil( void * arg )
{
    ( void ) arg;
    char line[ 32 ];
    uint8_t idx = 0;

    for ( ;; )
    {
        while ( Uart_RxAvailable() )
        {
            char ch = ( char ) Uart_ReadByte();

            if ( ( ch == '\n' ) || ( ch == '\r' ) )
            {
                line[ idx ] = '\0';
                idx = 0;
                g_hil_cmds++;

                if ( hil_streq( line, "HIL:PING" ) == 0 )
                {
                    Uart_WriteString( "HIL:PONG\n" );
                }
                else if ( hil_streq( line, "HIL:GET_VERSION" ) == 0 )
                {
                    Uart_WriteString( "HIL:VERSION:" HIL_VERSION_STRING "\n" );
                }
                else if ( hil_streq( line, "HIL:LED:1" ) == 0 )
                {
                    g_led_state = 1UL;
                    Uart_WriteString( "HIL:LED:ON\n" );
                }
                else if ( hil_streq( line, "HIL:LED:0" ) == 0 )
                {
                    g_led_state = 0UL;
                    Uart_WriteString( "HIL:LED:OFF\n" );
                }
                else if ( hil_streq( line, "HIL:STATE" ) == 0 )
                {
                    Uart_WriteString( "HIL:STATE:" );
                    Uart_WriteDec( g_led_state );
                    Uart_WriteString( "\n" );
                }
                else
                {
                    Uart_WriteString( "HIL:UNKNOWN:" );
                    Uart_WriteString( line );
                    Uart_WriteString( "\n" );
                }
            }
            else if ( idx < ( sizeof( line ) - 1UL ) )
            {
                line[ idx++ ] = ch;
            }
        }

        vTaskDelay( pdMS_TO_TICKS( 10 ) );
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

    if ( xTaskCreate( TaskHil, "TaskHil", TASK_HIL_STACK_SIZE, NULL,
                      TASK_HIL_PRIORITY, NULL ) != pdPASS )
    {
        Uart_WriteString( "TASK_HIL_FAIL\n" );
        return 1;
    }

    vTaskStartScheduler();

    /* Should never reach here. */
    Uart_WriteString( "SCHED_FAIL\n" );
    for ( ;; )
    {
    }
}
