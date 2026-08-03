/*
 * Uart_Cfg.c - CMSDK APB UART driver for QEMU mps2-an521 (test stub)
 *
 * Implements the minimal TX path required for QEMU verification.
 * Not production AUTOSAR code - this is a QEMU test harness UART.
 */
#include "Uart_Cfg.h"

static volatile uint32_t * const Uart_DataReg =
    ( volatile uint32_t * const )( UART0_BASE + UART_REG_DATA );
static volatile uint32_t * const Uart_StateReg =
    ( volatile uint32_t * const )( UART0_BASE + UART_REG_STATE );
static volatile uint32_t * const Uart_CtrlReg =
    ( volatile uint32_t * const )( UART0_BASE + UART_REG_CTRL );
static volatile uint32_t * const Uart_BaudDivReg =
    ( volatile uint32_t * const )( UART0_BASE + UART_REG_BAUDDIV );

void Uart_Init( void )
{
    *Uart_BaudDivReg = UART_BAUDDIV_VALUE;
    *Uart_CtrlReg    = ( UART_CTRL_TXEN | UART_CTRL_RXEN );
}

void Uart_WriteByte( uint8_t ch )
{
    while ( ( *Uart_StateReg & 0x01UL ) != 0UL )
    {
        /* wait for TXREADY */
    }
    *Uart_DataReg = ( uint32_t ) ch;
}

void Uart_WriteString( const char * str )
{
    while ( *str != '\0' )
    {
        if ( *str == '\n' )
        {
            Uart_WriteByte( '\r' );
        }
        Uart_WriteByte( ( uint8_t ) *str );
        str++;
    }
}

void Uart_WriteDec( uint32_t value )
{
    char buf[ 11 ];
    int  i = 10;

    buf[ 10 ] = '\0';

    if ( value == 0UL )
    {
        Uart_WriteByte( '0' );
        return;
    }

    while ( ( value > 0UL ) && ( i > 0 ) )
    {
        i--;
        buf[ i ] = ( char )( '0' + ( value % 10UL ) );
        value /= 10UL;
    }

    Uart_WriteString( &buf[ i ] );
}
