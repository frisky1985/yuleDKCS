/*
 * kw47a_board.h — NXP KW47A Board Support Package
 *
 * Pin mux, clock configuration, and peripheral initialization.
 */

#ifndef KW47A_BOARD_H
#define KW47A_BOARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Clock Configuration ─────────────────────────────────── */
#define KW47A_SYSTEM_CLOCK_HZ     96000000UL  /* 96 MHz */
#define KW47A_BLE_CLOCK_HZ        32000000UL  /* 32 MHz */
#define KW47A_UWB_CLOCK_HZ        48000000UL  /* 48 MHz */

/* ── Memory Sizes ────────────────────────────────────────── */
#define KW47A_FLASH_SIZE          (512 * 1024)
#define KW47A_SRAM_SIZE           (256 * 1024)

/* ── UART (Debug Console) ────────────────────────────────── */
#define UART_BASE                 ((void *)0x40002000)
#define UART_BAUDRATE             115200

/* ── BLE (KW47A integrated) ──────────────────────────────── */
#define BLE_IRQ_PRIORITY          5
#define BLE_TX_BUF_SIZE           256
#define BLE_RX_BUF_SIZE           256

/* ── NFC (ST25R501 via SPI) ──────────────────────────────── */
#define NFC_SPI_INSTANCE          1
#define NFC_SPI_BAUDRATE          8000000
#define NFC_IRQ_PIN               16
#define NFC_IRQ_PORT              0

/* ── UWB (NCJ29D6 via SPI) ───────────────────────────────── */
#define UWB_SPI_INSTANCE          2
#define UWB_SPI_BAUDRATE          12000000
#define UWB_IRQ_PIN               24
#define UWB_IRQ_PORT              1

/* ── SE050 (I2C) ─────────────────────────────────────────── */
#define SE050_I2C_INSTANCE        0
#define SE050_I2C_BAUDRATE        400000
#define SE050_I2C_ADDR            0x48

/* ── Button / LED / Debug ────────────────────────────────── */
#define LED_STATUS_PIN            3
#define LED_STATUS_PORT           1
#define BTN_FORCE_PIN             8
#define BTN_FORCE_PORT            0

/* ── Board Initialization ────────────────────────────────── */
void kw47a_board_init(void);
void kw47a_clock_init(void);
void kw47a_pinmux_init(void);
void kw47a_uart_init(uint32_t baud);
void kw47a_spi_init(int instance, uint32_t baud);
void kw47a_i2c_init(int instance, uint32_t baud);

#ifdef __cplusplus
}
#endif

#endif /* KW47A_BOARD_H */
