/**
 * @file Trng.h
 * @module EMB-BSW-MCAL-TRNG (ASPICE SWE.4)
 * @brief TRNG (True Random Number Generator) HAL Abstraction — S32K312 RNGA
 * @version 1.0
 * @date 2026-07-16
 *
 * Layer: MCAL (Microcontroller Abstraction Layer)
 *
 * Provides a hardware abstraction for the S32K312 RNGA (Random Number
 * Generator Accelerator) module. The RNGA is a true hardware random
 * number generator that complies with NIST SP 800-90B.
 *
 * ## S32K312 RNGA Overview
 *
 * The RNGA produces 32-bit random values using a free-running oscillator
 * ring as the entropy source. Key features:
 *   - 32-bit random data per read
 *   - Automatic self-test on reset
 *   - Continuous health monitoring (stuck bit detection)
 *   - Programmable oscillator frequency
 *   - Low-power mode support
 *
 * ## Integration
 *
 * The TRNG driver is used by:
 *   - crypto_random.c — MCU TRNG fallback tier (between SE050 and mbedTLS)
 *   - se050_scp03.c — SCP03 bootstrap (pre-SE050 host challenge generation)
 *   - Health monitoring / periodic RNG self-test
 *
 * ## Register Map (S32K312 RNGA)
 *
 *   Base: 0x40029000
 *   CR    (0x00) — Control Register
 *   SR    (0x04) — Status Register
 *   ER    (0x08) — Entropy Register (read random data)
 *
 * ## Reference
 *   - S32K3xx Reference Manual, Chapter: RNGA
 *   - NIST SP 800-90B: Entropy Sources
 *   - AUTOSAR MCAL Specification: RNG Driver
 */

#ifndef TRNG_H
#define TRNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "Compiler.h"

/* ========================================================================
 *  S32K312 RNGA Register Map
 * ========================================================================
 * Used directly on bare-metal targets. On host (test/stub) builds,
 * the implementation falls back to a software PRNG seeded from OS entropy.
 */

/** RNGA base address (S32K312 memory map) */
#define S32K312_RNGA_BASE          0x40029000UL

/** RNGA Control Register (CR) — offset 0x00 */
#define S32K312_RNGA_CR            (*(volatile uint32 *)(S32K312_RNGA_BASE + 0x00UL))
#define RNGA_CR_GO                 (1U << 4)   /**< Start random generation */
#define RNGA_CR_HA                 (1U << 3)   /**< High assurance mode */
#define RNGA_CR_INTM               (1U << 2)   /**< Interrupt mask */
#define RNGA_CR_CLRI               (1U << 1)   /**< Clear interrupt */
#define RNGA_CR_SLP                (1U << 0)   /**< Sleep (low power) */

/** RNGA Status Register (SR) — offset 0x04 */
#define S32K312_RNGA_SR            (*(volatile uint32 *)(S32K312_RNGA_BASE + 0x04UL))
#define RNGA_SR_OREG_LVL(reg)      (((reg) >> 8) & 0xFFU)  /**< Number of 32-bit words in output FIFO */
#define RNGA_SR_OREG_LVL_SHIFT     8U
#define RNGA_SR_OREG_LVL_MASK      0xFFU
#define RNGA_SR_SOF                 (1U << 6)   /**< Seed or frequency error */
#define RNGA_SR_LRD                 (1U << 5)   /**< Last read data valid */
#define RNGA_SR_FIFO_EMPTY         0x00U       /**< FIFO empty */
#define RNGA_SR_FIFO_READY         0x01U        /**< One or more words available */

/** RNGA Entropy Register (ER) — offset 0x08 (read: random data) */
#define S32K312_RNGA_ER            (*(volatile uint32 *)(S32K312_RNGA_BASE + 0x08UL))

/** RNGA Default Configuration: no interrupts, active mode, one-shot GO */
#define RNGA_CR_DEFAULT            (RNGA_CR_GO | RNGA_CR_INTM)

/* ========================================================================
 *  Return Codes
 * ======================================================================== */

#define HAL_TRNG_OK                 0      /**< Operation successful */
#define HAL_TRNG_ERR_NOT_INIT     (-1)     /**< TRNG not initialized */
#define HAL_TRNG_ERR_PARAM        (-2)     /**< Invalid parameter */
#define HAL_TRNG_ERR_STUCK_BIT    (-3)     /**< Stuck bit detected (stuck at zero/one) */
#define HAL_TRNG_ERR_TIMEOUT      (-4)     /**< Random data not available within timeout */
#define HAL_TRNG_ERR_HW           (-5)     /**< RNGA hardware error (seed/freq fault) */

/* ========================================================================
 *  Configuration
 * ======================================================================== */

/** Maximum timeout when polling for random data (in polling iterations) */
#define HAL_TRNG_POLL_TIMEOUT       1000U

/** Size of the internal software fallback buffer (host builds) */
#define HAL_TRNG_SW_BUF_SIZE        256U

/* ========================================================================
 *  Public API
 * ======================================================================== */

/**
 * @brief Initialize the MCU TRNG (RNGA) module.
 *
 * Enables the RNGA block, starts the self-test, and verifies the
 * entropy source is operational. On bare-metal S32K3, configures
 * the RNGA control register and waits for initial seed completion.
 *
 * On host (test/simulation) builds, seeds a software PRNG from OS
 * entropy as a stand-in for the hardware RNG.
 *
 * @return HAL_TRNG_OK on success, negative on error
 */
int hal_trng_init(void);

/**
 * @brief Deinitialize the TRNG module and put it into low-power mode.
 *
 * On S32K3, sets the RNGA sleep bit. On host, zeros internal state.
 */
void hal_trng_deinit(void);

/**
 * @brief Read random bytes from the MCU hardware TRNG.
 *
 * Fills the buffer with entropy from the S32K312 RNGA module.
 * Each call reads one or more 32-bit words from the RNGA Entropy
 * Register (ER) to satisfy the requested length.
 *
 * On host builds (non-embedded / RTD_ADAPTER_SELF_TEST), falls back
 * to a software CSPRNG seeded from OS entropy — this is suitable for
 * testing but NOT cryptographically secure in the same sense as
 * the hardware TRNG.
 *
 * @param buf   [out] Buffer to fill with random bytes
 * @param len   Number of random bytes requested (must be > 0)
 * @return HAL_TRNG_OK on success, negative on error:
 *         - HAL_TRNG_ERR_NOT_INIT   if hal_trng_init() not called
 *         - HAL_TRNG_ERR_PARAM      if buf is NULL or len == 0
 *         - HAL_TRNG_ERR_STUCK_BIT  if RNGA reports a stuck bit fault
 *         - HAL_TRNG_ERR_TIMEOUT    if RNGA does not produce data in time
 *         - HAL_TRNG_ERR_HW         if RNGA SOF flag indicates hardware error
 */
int hal_trng_read(uint8_t *buf, size_t len);

/**
 * @brief Query whether the TRNG is initialized and operational.
 *
 * @return true if hal_trng_init() has been called and the source
 *         passed self-test. false otherwise.
 */
bool hal_trng_is_available(void);

/**
 * @brief Run a self-test on the TRNG module.
 *
 * Reads a sample and validates it with basic health checks
 * (stuck-bit, continuity test).
 *
 * @return HAL_TRNG_OK on success, negative on failure
 */
int hal_trng_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* TRNG_H */
