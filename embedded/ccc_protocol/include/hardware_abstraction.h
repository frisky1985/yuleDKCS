/**
 * @file hardware_abstraction.h
 * @module EMB-MCAL-HW (ASPICE SWE.4)
 * @brief Hardware Abstraction Layer for CCC Protocol
 * Layer: MCAL (Microcontroller Abstraction Layer)
 */

#ifndef HARDWARE_ABSTRACTION_H
#define HARDWARE_ABSTRACTION_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 *  HW Pin/Port Definitions (stubs for compilation verification)
 * ======================================================================== */

/* NCJ29D6 (UWB) */
#define NCJ29D6_CS_PORT         1
#define NCJ29D6_CS_PIN          1
#define NCJ29D6_RST_PORT        1
#define NCJ29D6_RST_PIN         2
#define NCJ29D6_IRQ_PORT        1
#define NCJ29D6_IRQ_PIN         3

/* KW47A (BLE) */
#define KW47A_CS_PORT           2
#define KW47A_CS_PIN            1
#define KW47A_RST_PORT          2
#define KW47A_RST_PIN           2
#define KW47A_IRQ_PORT          2
#define KW47A_IRQ_PIN           3
#define KW47A_WAKE_PORT         2
#define KW47A_WAKE_PIN          4

/* ST25R501 (NFC) */
#define ST25R501_RST_PORT       3
#define ST25R501_RST_PIN        1
#define ST25R501_IRQ_PORT       3
#define ST25R501_IRQ_PIN        2
#define ST25R501_CS_PORT        3
#define ST25R501_CS_PIN         3

/* UWB Wake */
#define UWB_WAKE_PIN            10

/* CCC BLE Service UUID */
#define CCC_SERVICE_UUID        0xFEF5

/* ========================================================================
 *  SPI / GPIO / Delay — extern stubs (implementations provided by BSP)
 * ======================================================================== */

extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);
extern uint8_t gpio_read(uint8_t port, uint8_t pin);
extern void    gpio_write_wake(uint8_t pin, uint8_t val);
extern void    delay_ms(uint32_t ms);

#endif /* HARDWARE_ABSTRACTION_H */
