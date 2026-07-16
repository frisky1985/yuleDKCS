/**
 * @file crypto_random.h
 * @module EMB-BSW-CRYPTO (ASPICE SWE.4)
 * @brief Cryptographically Secure Random Number Generator — TRNG Abstraction
 * @version 1.0
 * @date 2026-07-16
 *
 * Layer: BSW (Basic Software Layer) — Crypto Services
 *
 * Provides a unified, cryptographically secure random number generator
 * with a multi-tier fallback strategy:
 *
 *   Tier 1 — SE050 Hardware TRNG (via SCP03 GET CHALLENGE APDU)
 *   Tier 2 — MCU Internal TRNG (S32K312 RNGA via hal_trng_read())
 *   Tier 3 — mbedTLS CTR_DRBG (HMAC-based, seeded from OS entropy)
 *   Tier 4 — OS entropy: /dev/urandom (Linux) or arc4random_buf (macOS/BSD)
 *
 * All tiers produce cryptographically secure output. The implementation
 * selects the best available source at runtime.
 *
 * ## Bootstrap Problem ("Who Guards the Guards?")
 *
 * SCP03 session establishment itself needs random bytes (host challenge),
 * creating a chicken-and-egg problem: we need random before SE050 is available.
 *
 * Solution:
 *   - **Pre-SCP03 (bootstrap)**: crypto_random_bytes() uses Tier 2 (MCU TRNG)
 *     on bare-metal S32K3, or Tier 3/4 (mbedTLS/OS) on host builds
 *     — no SE050 dependency.
 *   - **Post-SCP03**: After sec_scp03_open() succeeds, the SE050 RNG is
 *     registered via crypto_random_register_se050(). Subsequent calls
 *     prefer the hardware TRNG.
 *   - **Tier fallback during SCP03**: The host challenge generation explicitly
 *     uses bootstrap sources (MCU TRNG → mbedTLS → OS), so SCP03 can be
 *     established without SE050 RNG.
 *
 * ## Init Validation
 *
 * crypto_random_init() MUST be called once at startup. It probes all available
 * entropy sources and fails if none is operational. This prevents silent
 * fallback to weak entropy in production.
 *
 * ## Safety Guarantees
 *
 * - No hardcoded fallback values
 * - No DEV-ONLY weak entropy paths
 * - All buffers securely zeroed on cleanup
 * - Thread-safe for non-reentrant contexts via single DRBG instance
 *
 * Reference:
 *   - NIST SP 800-90A Rev. 1: DRBG
 *   - NIST SP 800-90B: Entropy Sources
 *   - GlobalPlatform Card Spec v2.3.1: GET CHALLENGE
 *   - FIPS 140-3 IG D.Q: Entropy Source Health Tests
 */

#ifndef CRYPTO_RANDOM_H
#define CRYPTO_RANDOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ========================================================================
 *  Return Codes
 * ======================================================================== */

#define CRYPTO_RANDOM_OK            0   /**< Success */
#define CRYPTO_RANDOM_ERR_NOT_INIT (-1)  /**< crypto_random_init() not called */
#define CRYPTO_RANDOM_ERR_DRBG     (-2)  /**< DRBG generation failure */
#define CRYPTO_RANDOM_ERR_OS       (-3)  /**< OS entropy source failure */
#define CRYPTO_RANDOM_ERR_SE050    (-4)  /**< SE050 RNG failure */
#define CRYPTO_RANDOM_ERR_PARAM    (-5)  /**< Invalid parameters (NULL buf, zero len) */
#define CRYPTO_RANDOM_ERR_NO_SOURCE (-6) /**< No entropy source available */

/* ========================================================================
 *  TRNG Source Type (for diagnostics / auditing)
 * ======================================================================== */

typedef enum {
    CRYPTO_RANDOM_SOURCE_NONE    = 0,    /**< No source initialized */
    CRYPTO_RANDOM_SOURCE_OS      = 1,    /**< /dev/urandom or arc4random */
    CRYPTO_RANDOM_SOURCE_MBEDTLS = 2,    /**< mbedTLS CTR_DRBG */
    CRYPTO_RANDOM_SOURCE_SE050   = 3,    /**< SE050 hardware TRNG */
    CRYPTO_RANDOM_SOURCE_MCU_TRNG = 4    /**< S32K3 internal RNGA (MCU TRNG) */
} crypto_random_source_t;

/* ========================================================================
 *  Public API
 * ======================================================================== */

/**
 * @brief Initialize the TRNG subsystem.
 *
 * Probes all available entropy sources and selects the best operational one.
 * Must be called once early in the boot sequence, before any crypto operation
 * that needs randomness.
 *
 * Side effect: Seeded DRBG is initialized if OS entropy is available.
 *
 * @return CRYPTO_RANDOM_OK on success, CRYPTO_RANDOM_ERR_NO_SOURCE if
 *         no entropy source is operational (product should not boot).
 */
int crypto_random_init(void);

/**
 * @brief Deinitialize the TRNG subsystem.
 *
 * Securely zeroes the DRBG state and internal buffers.
 */
void crypto_random_deinit(void);

/**
 * @brief Check whether a trustworthy entropy source is available.
 *
 * @return true if at least one source is operational and has passed
 *         the init-time health check.
 */
bool crypto_random_is_available(void);

/**
 * @brief Get the currently active entropy source.
 *
 * @return The source in use (or CRYPTO_RANDOM_SOURCE_NONE if not init'd).
 */
crypto_random_source_t crypto_random_get_source(void);

/**
 * @brief Fill a buffer with cryptographically secure random bytes.
 *
 * This is the main API function. Calls the best available entropy source.
 * On pre-SCP03 bootstrap, uses OS or mbedTLS DRBG.
 * On post-SCP03, uses SE050 hardware TRNG.
 *
 * @param buf  [out] Buffer to fill with random bytes
 * @param len  Number of random bytes requested (must be > 0)
 * @return CRYPTO_RANDOM_OK on success, negative on error
 */
int crypto_random_bytes(uint8_t *buf, size_t len);

/**
 * @brief Register the SE050 hardware TRNG as a source.
 *
 * Called by sec_init() / sec_scp03_open() after the SCP03 secure channel
 * is established. Once registered, crypto_random_bytes() prefers this
 * source over Tier 2/3.
 *
 * The callback function must issue GET CHALLENGE APDUs through the
 * established SCP03 channel and return random bytes.
 *
 * @param fn  Pointer to a function that fills buf with len random bytes
 *            using the SE050 secure channel. Must return 0 on success.
 */
void crypto_random_register_se050(int (*fn)(uint8_t *buf, size_t len));

/**
 * @brief Unregister the SE050 TRNG (e.g., on SCP03 session close).
 *
 * Falls back to the next available source (Tier 2 or Tier 3).
 */
void crypto_random_unregister_se050(void);

/* ========================================================================
 *  Self-Test / Health Check
 * ======================================================================== */

/**
 * @brief Run a continuous health test on the active entropy source.
 *
 * Reads a small sample (e.g., 128 bytes) and performs basic statistical
 * checks to detect stuck bits or complete failure. In production, this
 * should be called periodically (every N crypto_random_bytes calls) or
 * by a dedicated health monitoring task.
 *
 * @return CRYPTO_RANDOM_OK on success, negative if the source is degraded.
 */
int crypto_random_health_test(void);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_RANDOM_H */
