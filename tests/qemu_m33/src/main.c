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

/* -nostdlib 无 libc, 用内联字符串比较替代 strcmp */
static int hil_streq( const char * a, const char * b );

static int hil_starts_with( const char * s, const char * prefix )
{
    while ( *prefix != '\0' )
    {
        if ( *s != *prefix )
        {
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

/* ---------------------------------------------------------------------
 * 系统状态机 (HIL 可注入) — FI-05 非法转换检测的 SIL 验证载体
 *
 * 状态: IDLE(0) → MONITORING(1) → UNLOCKED(2) → LOCKED(3)
 * 合法转换表:
 *   IDLE       → MONITORING          (开始检测)
 *   MONITORING → UNLOCKED | IDLE     (认证成功 / 超时回退)
 *   UNLOCKED   → LOCKED              (上锁)
 *   LOCKED     → MONITORING          (重新检测)
 * 非法转换: 拒绝 (状态保持) + 非法计数++ (安全日志模拟)
 * ------------------------------------------------------------------- */
#define SM_IDLE         0
#define SM_MONITORING   1
#define SM_UNLOCKED     2
#define SM_LOCKED       3

static volatile int      g_sm_state   = SM_IDLE;
static volatile uint32_t g_sm_illegal = 0UL;

static int sm_transition_allowed( int from, int target )
{
    switch ( from )
    {
        case SM_IDLE:       return ( target == SM_MONITORING );
        case SM_MONITORING: return ( ( target == SM_UNLOCKED ) || ( target == SM_IDLE ) );
        case SM_UNLOCKED:   return ( target == SM_LOCKED );
        case SM_LOCKED:     return ( target == SM_MONITORING );
        default:            return 0;
    }
}

static const char * sm_state_name( int s )
{
    switch ( s )
    {
        case SM_IDLE:       return "IDLE";
        case SM_MONITORING: return "MONITORING";
        case SM_UNLOCKED:   return "UNLOCKED";
        case SM_LOCKED:     return "LOCKED";
        default:            return "UNKNOWN";
    }
}

/* 解析 "SET:<name|num>" 中的目标状态, 失败返回 -1 */
static int sm_parse_target( const char * s )
{
    if ( hil_streq( s, "IDLE" ) == 0 )       return SM_IDLE;
    if ( hil_streq( s, "MONITORING" ) == 0 ) return SM_MONITORING;
    if ( hil_streq( s, "UNLOCKED" ) == 0 )   return SM_UNLOCKED;
    if ( hil_streq( s, "LOCKED" ) == 0 )     return SM_LOCKED;
    return -1;
}

/* ---------------------------------------------------------------------
 * TaskHil — HIL 命令通道 (UART RX 轮询)
 *
 * 命令集 (host → 固件):
 *   HIL:PING           → HIL:PONG
 *   HIL:GET_VERSION    → HIL:VERSION:<ver>
 *   HIL:LED:1|0        → HIL:LED:ON|OFF
 *   HIL:STATE          → HIL:STATE:<led>
 *   HIL:GET_TICKS      → HIL:TICKS:<n>      (真实 tick 计数)
 *   HIL:GET_UPTIME     → HIL:UPTIME:<ms>    (真实运行时间)
 *   HIL:<DOMAIN>:STATUS → HIL:<DOMAIN>:NOT_AVAILABLE
 *                       (BLE/NFC/UWB/SE050: QEMU 无 RF/SE 硬件, 诚实报告)
 * 未知命令 → HIL:UNKNOWN:<cmd>
 * ------------------------------------------------------------------- */
static volatile uint32_t g_led_state = 0UL;
static volatile uint32_t g_hil_cmds  = 0UL;
static volatile TickType_t g_hil_start_tick = 0UL;

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
                else if ( hil_streq( line, "HIL:GET_TICKS" ) == 0 )
                {
                    Uart_WriteString( "HIL:TICKS:" );
                    Uart_WriteDec( ( uint32_t ) xTaskGetTickCount() );
                    Uart_WriteString( "\n" );
                }
                else if ( hil_streq( line, "HIL:GET_UPTIME" ) == 0 )
                {
                    Uart_WriteString( "HIL:UPTIME:" );
                    Uart_WriteDec( ( uint32_t ) xTaskGetTickCount() - g_hil_start_tick );
                    Uart_WriteString( "\n" );
                }
                else if ( ( hil_streq( line, "HIL:BLE:STATUS" ) == 0 ) ||
                          ( hil_streq( line, "HIL:NFC:STATUS" ) == 0 ) ||
                          ( hil_streq( line, "HIL:UWB:STATUS" ) == 0 ) ||
                          ( hil_streq( line, "HIL:SE050:STATUS" ) == 0 ) )
                {
                    /* QEMU 无 RF/SE 硬件 — 诚实报告不可用 (host 端应 SKIP) */
                    Uart_WriteString( "HIL:NOT_AVAILABLE\n" );
                }
                else if ( hil_streq( line, "HIL:SM:STATE" ) == 0 )
                {
                    Uart_WriteString( "HIL:SM:STATE:" );
                    Uart_WriteString( sm_state_name( g_sm_state ) );
                    Uart_WriteString( "\n" );
                }
                else if ( hil_streq( line, "HIL:SM:ILLEGAL" ) == 0 )
                {
                    Uart_WriteString( "HIL:SM:ILLEGAL:" );
                    Uart_WriteDec( g_sm_illegal );
                    Uart_WriteString( "\n" );
                }
                else if ( hil_streq( line, "HIL:SM:RESET" ) == 0 )
                {
                    g_sm_state = SM_IDLE;
                    g_sm_illegal = 0UL;
                    Uart_WriteString( "HIL:SM:RESET:OK\n" );
                }
                else if ( hil_starts_with( line, "HIL:SM:SET:" ) )
                {
                    int target = sm_parse_target( &line[ 11 ] );
                    if ( target < 0 )
                    {
                        Uart_WriteString( "HIL:SM:SET:INVALID_TARGET\n" );
                    }
                    else if ( sm_transition_allowed( g_sm_state, target ) )
                    {
                        Uart_WriteString( "HIL:SM:OK:" );
                        Uart_WriteString( sm_state_name( g_sm_state ) );
                        Uart_WriteString( "->" );
                        g_sm_state = target;
                        Uart_WriteString( sm_state_name( g_sm_state ) );
                        Uart_WriteString( "\n" );
                    }
                    else
                    {
                        /* 非法转换: 拒绝 + 状态保持 + 计数 (安全日志) */
                        g_sm_illegal++;
                        Uart_WriteString( "HIL:SM:REJECT:" );
                        Uart_WriteString( sm_state_name( g_sm_state ) );
                        Uart_WriteString( "->" );
                        Uart_WriteString( sm_state_name( target ) );
                        Uart_WriteString( "\n" );
                    }
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
