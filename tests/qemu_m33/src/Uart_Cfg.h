/*
 * Uart_Cfg.h - CMSDK APB UART configuration for QEMU mps2-an521
 *
 * mps2-an521 UART0 is at 0x40200000 (NOT the SSE-200 reference address
 * 0x40004000 - QEMU silently ignores writes to unmapped addresses).
 * serial0 (QEMU stdio) is wired to UART0.
 *
 * CMSDK UART registers (base + offset):
 *   0x00 DATA       write = transmit character
 *   0x04 STATE      bit0 TXREADY (1 = ready, 0 = busy)
 *   0x08 CTRL       bit0 TXEN, bit1 RXEN
 *   0x10 BAUDDIV    must be non-zero or QEMU drops all TX
 */
#ifndef UART_CFG_H
#define UART_CFG_H

#include <stdint.h>

#define UART0_BASE          ( 0x40200000UL )
#define UART_REG_DATA       ( 0x00UL )
#define UART_REG_STATE      ( 0x04UL )
#define UART_REG_CTRL       ( 0x08UL )
#define UART_REG_BAUDDIV    ( 0x10UL )

#define UART_CTRL_TXEN      ( 0x01UL )
#define UART_CTRL_RXEN      ( 0x02UL )

#define UART_BAUDDIV_VALUE  ( 1UL )   /* non-zero: required by QEMU */

void Uart_Init( void );
void Uart_WriteByte( uint8_t ch );
void Uart_WriteString( const char * str );
void Uart_WriteDec( uint32_t value );

#endif /* UART_CFG_H */
