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
 *  允许命令行 -D 覆盖 (测试/Makefile 中有 KW47A_CS_PORT 等 -D 定义)
 * ======================================================================== */

/* NCJ29D6 (UWB) */
#ifndef NCJ29D6_CS_PORT
#define NCJ29D6_CS_PORT         1
#endif
#ifndef NCJ29D6_CS_PIN
#define NCJ29D6_CS_PIN          1
#endif
#ifndef NCJ29D6_RST_PORT
#define NCJ29D6_RST_PORT        1
#endif
#ifndef NCJ29D6_RST_PIN
#define NCJ29D6_RST_PIN         2
#endif
#ifndef NCJ29D6_IRQ_PORT
#define NCJ29D6_IRQ_PORT        1
#endif
#ifndef NCJ29D6_IRQ_PIN
#define NCJ29D6_IRQ_PIN         3
#endif

/* KW47A (BLE) */
#ifndef KW47A_CS_PORT
#define KW47A_CS_PORT           2
#endif
#ifndef KW47A_CS_PIN
#define KW47A_CS_PIN            1
#endif
#ifndef KW47A_RST_PORT
#define KW47A_RST_PORT          2
#endif
#ifndef KW47A_RST_PIN
#define KW47A_RST_PIN           2
#endif
#ifndef KW47A_IRQ_PORT
#define KW47A_IRQ_PORT          2
#endif
#ifndef KW47A_IRQ_PIN
#define KW47A_IRQ_PIN           3
#endif
#ifndef KW47A_WAKE_PORT
#define KW47A_WAKE_PORT         2
#endif
#ifndef KW47A_WAKE_PIN
#define KW47A_WAKE_PIN          4
#endif

/* ST25R501 (NFC) */
#ifndef ST25R501_RST_PORT
#define ST25R501_RST_PORT       3
#endif
#ifndef ST25R501_RST_PIN
#define ST25R501_RST_PIN        1
#endif
#ifndef ST25R501_IRQ_PORT
#define ST25R501_IRQ_PORT       3
#endif
#ifndef ST25R501_IRQ_PIN
#define ST25R501_IRQ_PIN        2
#endif
#ifndef ST25R501_CS_PORT
#define ST25R501_CS_PORT        3
#endif
#ifndef ST25R501_CS_PIN
#define ST25R501_CS_PIN         3
#endif

/* UWB Wake */
#ifndef UWB_WAKE_PIN
#define UWB_WAKE_PIN            10
#endif

/* ========================================================================
 *  BLE Service UUIDs (单一来源, 手机端严格对齐)
 *  规范依据:
 *    CCC Digital Key v4.0 (CCC-TS-101 Table 19-6)        → 0xFFF5
 *    ICCOA Digital Key 4.0                                → 0xFEF5
 *    ICCE T/CA 110-2020                                   → 0xFEFA
 *    CCC R3.0 存量兼容 (旧参考实现)                       → 0xFFD1
 * ======================================================================== */
#define CCC_SERVICE_UUID        0xFFF5   /* CCC v4.0 主 (规范) */
#define CCC_SERVICE_UUID_R30   0xFFD1   /* CCC R3.0 兼容 (旧参考实现) */
#define ICCOA_SERVICE_UUID      0xFEF5   /* ICCOA Digital Key 4.0 */
#define ICCE_SERVICE_UUID       0xFEFA   /* ICCE T/CA 110-2020 */

/* ========================================================================
 *  SPI / GPIO / Delay — extern stubs (implementations provided by BSP)
 * ======================================================================== */

extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);
extern uint8_t gpio_read(uint8_t port, uint8_t pin);
extern void    gpio_write_wake(uint8_t pin, uint8_t val);
extern void    delay_ms(uint32_t ms);

#endif /* HARDWARE_ABSTRACTION_H */
