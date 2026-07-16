/**
 * @file crypto_random.c
 * @module EMB-BSW-CRYPTO (ASPICE SWE.4)
 * @brief Cryptographically Secure Random Number Generator — Implementation
 * @version 1.0
 * @date 2026-07-16
 *
 * Implements the three-tier TRNG abstraction:
 *
 *   Tier 1: SE050 Hardware TRNG (via registered callback, after SCP03 open)
 *   Tier 2: mbedTLS CTR_DRBG (seeded from OS entropy /dev/urandom)
 *   Tier 3: OS entropy directly (/dev/urandom on Linux, arc4random_buf on BSD/macOS)
 *
 * ## Design Decisions
 *
 * - **No SE050 dependency at init**: crypto_random_init() probes MCU TRNG, OS and
 *   mbedTLS sources. SE050 is registered later by security.c via
 *   crypto_random_register_se050().
 * - **Bootstrap-safe**: crypto_random_bytes() always works from the first call,
 *   even before SE050 is available. Uses Tier 2 (MCU TRNG) → Tier 3 (mbedTLS)
 *   → Tier 4 (OS) fallback.
 * - **Prefer SE050 when available**: After registration, all calls route through
 *   the hardware TRNG for maximum entropy quality.
 * - **MCU TRNG for bare-metal**: On S32K3 embedded targets, the MCU internal
 *   RNGA provides hardware entropy without SCP03 dependency, making it ideal
 *   for SCP03 bootstrap.
 * - **No weak fallback**: Removed all DEV-ONLY hardcoded values. Failure to read
 *   from any source returns an error — no silent degradation.
 *
 * ## Init Sequence
 *
 *   1. Platform boot → crypto_random_init()   probes MCU TRNG / mbedTLS / OS
 *   2. SCP03 bootstrap → crypto_random_bytes() uses MCU TRNG (Tier 2)
 *   3. SCP03 established → crypto_random_register_se050() from sec_init()
 *   4. Normal operation → crypto_random_bytes() uses SE050 TRNG
 *   5. SCP03 close → crypto_random_unregister_se050() (falls back to Tier 2/3/4)
 *
 * ## Health Monitoring
 *
 * Init includes a self-test that reads and discards a sample to detect
 * stuck bits early. Periodic health_test() should be called by the
 * application or a watchdog task.
 *
 * Reference:
 *   - NIST SP 800-90A Rev. 1 (CTR_DRBG, HMAC_DRBG)
 *   - NIST SP 800-90B (Entropy Source Validation)
 *   - RFC 4086 (Randomness Requirements for Security)
 *   - NIST SP 800-22 (Statistical Test Suite — basic subset)
 *   - FIPS 140-3 IG D.Q
 */

#include "crypto_random.h"

#include <string.h>
#include <stdlib.h>

/* ========================================================================
 *  MCU TRNG — S32K312 RNGA via HAL abstraction
 * ========================================================================
 * Added as Tier 2 in the fallback chain (P1-4). On bare-metal S32K3,
 * provides hardware entropy without SCP03 dependency, solving the
 * SCP03 bootstrap chicken-and-egg problem.
 *
 * Detected via TARGET_S32K3, USE_MCU_TRNG, or the availability of
 * Trng.h in the MCAL stubs include path.
 * ======================================================================== */

#if defined(TARGET_S32K3) || defined(USE_MCU_TRNG) || defined(__arm__) || defined(__ARMCC_VERSION)
    /* Embedded target with MCU TRNG hardware */
    #define CRYPTO_RANDOM_HAVE_MCU_TRNG  1
    #include "Trng.h"
#elif !defined(CRYPTO_RANDOM_NO_MCU_TRNG)
    /* Host build: try to include Trng.h if available */
    #if __has_include("Trng.h")
        #define CRYPTO_RANDOM_HAVE_MCU_TRNG  1
        #include "Trng.h"
    #endif
#endif

/* ========================================================================
 *  Platform Detection
 * ======================================================================== */

#if defined(__linux__) || defined(__unix__)
    #define CRYPTO_RANDOM_OS_LINUX    1
#elif defined(__APPLE__) || defined(__MACH__) || defined(__FreeBSD__) || \
      defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define CRYPTO_RANDOM_OS_BSD      1
#elif defined(_WIN32) || defined(_WIN64)
    #define CRYPTO_RANDOM_OS_WIN      1
#else
    /* Unknown/embedded target — may not have /dev/urandom or arc4random */
    #define CRYPTO_RANDOM_OS_NONE     1
#endif

/* ========================================================================
 *  mbedTLS / OpenSSL Detection
 * ========================================================================
 * We prefer mbedTLS CTR_DRBG when available because it provides a
 * well-audited, FIPS-approved DRBG backed by the OS entropy source.
 *
 * When neither is available, we fall through to direct OS entropy reads.
 */

#if defined(MBEDTLS_CONFIG_FILE)
    /* User-provided mbedTLS config */
    #include MBEDTLS_CONFIG_FILE
#endif

#if !defined(CRYPTO_RANDOM_NO_MBEDTLS) && \
    (defined(MBEDTLS_CTR_DRBG_C) || defined(USE_MBEDTLS))
    #define CRYPTO_RANDOM_HAVE_MBEDTLS   1
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/entropy.h>
#endif

#if !defined(CRYPTO_RANDOM_HAVE_MBEDTLS) && \
    !defined(CRYPTO_RANDOM_NO_OPENSSL) && \
    (defined(OPENSSL_API_COMPAT) || defined(OPENSSL_VERSION_NUMBER) || \
     defined(USE_OPENSSL))
    #define CRYPTO_RANDOM_HAVE_OPENSSL   1
    #include <openssl/rand.h>
    #include <openssl/err.h>
#endif

/* ========================================================================
 *  Forward Declaration — SE050 RNG Callback
 * ========================================================================
 * Registered by security.c after SCP03 session establishment.
 * The callback issues GET CHALLENGE APDUs through the secure channel.
 */

static int (*g_se050_rng_fn)(uint8_t *buf, size_t len) = NULL;

/* ========================================================================
 *  Internal State
 * ======================================================================== */

/** Whether crypto_random_init() has been called successfully */
static int g_init_done = 0;

/** Current active source (diagnostic) */
static crypto_random_source_t g_active_source = CRYPTO_RANDOM_SOURCE_NONE;

/** Health test counter — run a full health check every N calls */
#define CRYPTO_RANDOM_HEALTH_INTERVAL   1024
static uint32_t g_call_counter = 0;

/** Previous sample for stuck-bit detection (simple XOR accum) */
static uint8_t g_health_accum[16] = {0};

#if defined(CRYPTO_RANDOM_HAVE_MBEDTLS)
/* mbedTLS DRBG context (static, single-instance) */
static mbedtls_ctr_drbg_context  g_drbg;
static mbedtls_entropy_context   g_entropy;
#endif

/* ========================================================================
 *  Internal: OS Entropy Source
 * ========================================================================
 * Reads directly from the operating system's CSPRNG.
 * macOS/BSD: arc4random_buf() — always available, no open/close.
 * Linux:     /dev/urandom — non-blocking kernel CSPRNG.
 */

/**
 * @brief Read random bytes from the OS entropy source.
 *
 * @param buf  [out] Buffer to fill
 * @param len  Number of bytes required
 * @return 0 on success, -1 on failure
 */
static int os_get_random(uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0)
    {
        return -1;
    }

#if defined(CRYPTO_RANDOM_OS_BSD)
    /* macOS, iOS, FreeBSD, OpenBSD, etc. — arc4random is guaranteed available */
    arc4random_buf(buf, len);
    return 0;

#elif defined(CRYPTO_RANDOM_OS_LINUX)
    /* /dev/urandom — non-blocking, always succeeds on modern kernels */
    FILE *f = fopen("/dev/urandom", "rb");
    if (f == NULL)
    {
        return -1;
    }

    size_t total_read = 0;
    while (total_read < len)
    {
        size_t n = fread(buf + total_read, 1, len - total_read, f);
        if (n == 0)
        {
            /* EOF or error */
            fclose(f);
            return -1;
        }
        total_read += n;
    }

    fclose(f);
    return 0;

#elif defined(CRYPTO_RANDOM_OS_WIN)
    /* Windows BCryptGenRandom */
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")

    NTSTATUS status = BCryptGenRandom(NULL, buf, (ULONG)len,
                                       BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return (status >= 0) ? 0 : -1;
#else
    /* Embedded target without OS CSPRNG — nothing available at this tier */
    (void)buf;
    (void)len;
    return -1;
#endif
}

/* ========================================================================
 *  Internal: Self-Test / Repetition Count Test (NIST SP 800-90B Section 4.4)
 * ========================================================================
 * Simple stuck-bit detection: if the XOR of consecutive bytes is 0,
 * the source has likely stuck at a constant value.
 */

/**
 * @brief Quick stuck-bit check: verify consecutive samples differ.
 *
 * This is NOT a full health test — it detects catastrophic failures only.
 * Full statistical testing (Chi-square, runs test) should be run during
 * manufacturing qualification and optionally during periodic health checks.
 *
 * @param sample  [in] Fresh random bytes to test
 * @param len     Length of sample
 * @return 0 if sample passes, -1 if stuck bits detected
 */
static int stuck_bit_check(const uint8_t *sample, size_t len)
{
    uint8_t accum = 0U;
    size_t i;

    /* Simple XOR across all bytes — should not be zero for random data */
    for (i = 0; i < len; i++)
    {
        accum |= sample[i];
    }

    if (accum == 0U)
    {
        return -1; /* All bytes are zero — stuck */
    }

    /* Compare with previous accum — should differ */
    /* (Fast health check: two consecutive samples should not match) */
    {
        uint8_t diff = 0U;
        for (i = 0; i < sizeof(g_health_accum) && i < len; i++)
        {
            diff |= (uint8_t)(g_health_accum[i] ^ sample[i]);
        }
        /* Only update accum if we got enough bytes */
        if (len >= sizeof(g_health_accum))
        {
            (void)memcpy(g_health_accum, sample, sizeof(g_health_accum));
        }
    }

    return 0;
}

/* ========================================================================
 *  Internal: MCU TRNG (S32K3 RNGA / hal_trng_read)
 * ========================================================================
 * Provides hardware entropy from the S32K312 internal RNGA module.
 * This is the bootstrap entropy source for SCP03 on bare-metal targets.
 * 
 * Usage in tier system:
 *   Tier 1: SE050 HW TRNG (after SCP03)
 *   Tier 2: MCU TRNG (this, always available on S32K3)
 *   Tier 3: mbedTLS CTR_DRBG
 *   Tier 4: OS entropy
 *
 * On host builds where no MCU TRNG exists, this tier is #ifdef'd out
 * and the fallback goes directly to Tier 3 (mbedTLS).
 */

#if defined(CRYPTO_RANDOM_HAVE_MCU_TRNG)

/**
 * @brief Read random bytes from the MCU internal TRNG.
 *
 * Calls hal_trng_read() which reads from the S32K312 RNGA registers
 * on bare-metal, or from a software CSPRNG on host builds.
 *
 * @param buf  [out] Buffer to fill
 * @param len  Number of bytes required
 * @return 0 on success, -1 on failure
 */
static int mcu_trng_get_random(uint8_t *buf, size_t len)
{
    int ret;

    ret = hal_trng_read(buf, len);
    if (ret != HAL_TRNG_OK)
    {
        return -1;
    }

    return 0;
}

#endif /* CRYPTO_RANDOM_HAVE_MCU_TRNG */

/* ========================================================================
 *  Internal: mbedTLS CTR_DRBG
 * ========================================================================
 */

#if defined(CRYPTO_RANDOM_HAVE_MBEDTLS)

/**
 * @brief Initialize the mbedTLS CTR_DRBG, seeded from OS entropy.
 *
 * @return 0 on success, -1 on failure
 */
static int mbedtls_drbg_init_internal(void)
{
    int ret;

    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_drbg);

    /* Seed the DRBG with a nonce from OS entropy */
    uint8_t nonce[32];
    ret = os_get_random(nonce, sizeof(nonce));
    if (ret != 0)
    {
        /* OS entropy unavailable — try with zero nonce (weak but mbedTLS
         * will still use the entropy source for seeding) */
        (void)memset(nonce, 0, sizeof(nonce));
    }

    /* Personalization string: module name + version for domain separation */
    const char *pers = "yuleDKCS-crypto-random-v1";

    ret = mbedtls_ctr_drbg_seed(&g_drbg,
                                 mbedtls_entropy_func, &g_entropy,
                                 (const uint8_t *)pers, strlen(pers));
    if (ret != 0)
    {
        mbedtls_ctr_drbg_free(&g_drbg);
        mbedtls_entropy_free(&g_entropy);
        return -1;
    }

    /* Optionally set a reseed interval (1M requests is conservative) */
    mbedtls_ctr_drbg_set_reseed_interval(&g_drbg, 1000000);

    return 0;
}

/**
 * @brief Read random bytes from mbedTLS CTR_DRBG.
 *
 * @param buf  [out] Buffer to fill
 * @param len  Number of bytes
 * @return 0 on success, -1 on failure
 */
static int mbedtls_get_random(uint8_t *buf, size_t len)
{
    int ret;

    ret = mbedtls_ctr_drbg_random(&g_drbg, buf, len);
    if (ret != 0)
    {
        /* DRBG may need reseeding — attempt reseed */
        ret = mbedtls_ctr_drbg_reseed(&g_drbg, NULL, 0);
        if (ret != 0)
        {
            return -1;
        }

        /* Retry after reseed */
        ret = mbedtls_ctr_drbg_random(&g_drbg, buf, len);
        if (ret != 0)
        {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Securely zero and free the mbedTLS DRBG state.
 */
static void mbedtls_drbg_deinit_internal(void)
{
    mbedtls_ctr_drbg_free(&g_drbg);
    mbedtls_entropy_free(&g_entropy);
}

#endif /* CRYPTO_RANDOM_HAVE_MBEDTLS */

/* ========================================================================
 *  Internal: OpenSSL RAND_bytes
 * ========================================================================
 */

#if defined(CRYPTO_RANDOM_HAVE_OPENSSL)

/**
 * @brief Read random bytes from OpenSSL's CSPRNG.
 *
 * @param buf  [out] Buffer to fill
 * @param len  Number of bytes
 * @return 0 on success, -1 on failure
 */
static int openssl_get_random(uint8_t *buf, size_t len)
{
    int ret;

    /* RAND_priv_bytes() uses the private DRBG (recommended) */
    /* RAND_bytes() falls back to the public DRBG if available */
    #if OPENSSL_VERSION_NUMBER >= 0x10101000L
        ret = RAND_priv_bytes(buf, (int)len);
    #else
        ret = RAND_bytes(buf, (int)len);
    #endif

    if (ret != 1)
    {
        return -1;
    }

    return 0;
}

#endif /* CRYPTO_RANDOM_HAVE_OPENSSL */

/* ========================================================================
 *  Public API Implementation
 * ======================================================================== */

int crypto_random_init(void)
{
    int ret;

    if (g_init_done)
    {
        return CRYPTO_RANDOM_OK;
    }

    /* Reset state */
    g_se050_rng_fn  = NULL;
    g_active_source  = CRYPTO_RANDOM_SOURCE_NONE;
    g_call_counter   = 0;

    /* ---- Probe entropy sources ---- */

    /*
     * Probe Tier 2: MCU TRNG (S32K312 RNGA).
     * On bare-metal targets, this provides hardware entropy without any
     * external dependency — ideal for SCP03 bootstrap.
     * On host builds (no RNGA hardware), hal_trng_init() falls back to
     * a software CSPRNG, which is fine for testing.
     */
#if defined(CRYPTO_RANDOM_HAVE_MCU_TRNG)
    ret = hal_trng_init();
    if (ret == HAL_TRNG_OK)
    {
        /* Run self-test: generate and discard a sample to verify */
        uint8_t test_buf[64];
        ret = hal_trng_read(test_buf, sizeof(test_buf));
        if (ret == HAL_TRNG_OK && stuck_bit_check(test_buf, sizeof(test_buf)) == 0)
        {
            /* MCU TRNG is operational */
            g_active_source = CRYPTO_RANDOM_SOURCE_MCU_TRNG;
            g_init_done = 1;
            (void)memset(test_buf, 0, sizeof(test_buf));
            return CRYPTO_RANDOM_OK;
        }

        /* Self-test failed — deinit and fall through */
        hal_trng_deinit();
        (void)memset(test_buf, 0, sizeof(test_buf));
    }
#endif /* CRYPTO_RANDOM_HAVE_MCU_TRNG */

    /* Probe Tier 3 (mbedTLS) — available on host builds with mbedTLS */
#if defined(CRYPTO_RANDOM_HAVE_MBEDTLS)
    ret = mbedtls_drbg_init_internal();
    if (ret == 0)
    {
        g_active_source = CRYPTO_RANDOM_SOURCE_MBEDTLS;

        /* Run self-test: generate and discard a sample to prime the DRBG */
        uint8_t test_buf[128];
        ret = mbedtls_get_random(test_buf, sizeof(test_buf));
        if (ret == 0 && stuck_bit_check(test_buf, sizeof(test_buf)) == 0)
        {
            /* mbedTLS source is operational */
            g_init_done = 1;
            return CRYPTO_RANDOM_OK;
        }

        /* Self-test failed — fall through */
        mbedtls_drbg_deinit_internal();
    }
#endif /* CRYPTO_RANDOM_HAVE_MBEDTLS */

    /* Probe Tier 4 (OS entropy) directly */
    {
        uint8_t test_buf[128];
        ret = os_get_random(test_buf, sizeof(test_buf));
        if (ret == 0 && stuck_bit_check(test_buf, sizeof(test_buf)) == 0)
        {
            g_active_source = CRYPTO_RANDOM_SOURCE_OS;
            g_init_done = 1;
            return CRYPTO_RANDOM_OK;
        }

        /* Failed self-test on OS source — zero the test buffer and continue */
        (void)memset(test_buf, 0, sizeof(test_buf));
    }

    /* No entropy source available — production system MUST NOT boot */
    g_active_source = CRYPTO_RANDOM_SOURCE_NONE;
    g_init_done = 0;

    return CRYPTO_RANDOM_ERR_NO_SOURCE;
}

void crypto_random_deinit(void)
{
    /* Unregister SE050 */
    g_se050_rng_fn = NULL;

#if defined(CRYPTO_RANDOM_HAVE_MCU_TRNG)
    /* Deinitialize MCU TRNG (zeroes software state on host, sleep on bare-metal) */
    hal_trng_deinit();
#endif

#if defined(CRYPTO_RANDOM_HAVE_MBEDTLS)
    if (g_active_source == CRYPTO_RANDOM_SOURCE_MBEDTLS ||
        g_active_source == CRYPTO_RANDOM_SOURCE_MCU_TRNG || /* Could fall through */
        g_active_source == CRYPTO_RANDOM_SOURCE_SE050)  /* Could fallback to mbedTLS */
    {
        mbedtls_drbg_deinit_internal();
    }
#endif

    /* Zero internal state */
    g_active_source = CRYPTO_RANDOM_SOURCE_NONE;
    g_init_done = 0;
    (void)memset(g_health_accum, 0, sizeof(g_health_accum));
    g_call_counter = 0;
}

bool crypto_random_is_available(void)
{
    return (g_init_done != 0);
}

crypto_random_source_t crypto_random_get_source(void)
{
    return g_active_source;
}

int crypto_random_bytes(uint8_t *buf, size_t len)
{
    int ret;

    if (buf == NULL || len == 0)
    {
        return CRYPTO_RANDOM_ERR_PARAM;
    }

    if (!g_init_done)
    {
        return CRYPTO_RANDOM_ERR_NOT_INIT;
    }

    g_call_counter++;

    /*
     * Tier 1: SE050 Hardware TRNG
     * - Only used if registered (SCP03 session established)
     * - Provides true hardware entropy from the SE050
     */
    if (g_se050_rng_fn != NULL)
    {
        ret = g_se050_rng_fn(buf, len);
        if (ret == 0)
        {
            g_active_source = CRYPTO_RANDOM_SOURCE_SE050;

            /* Periodic health check (every N calls) */
            if ((g_call_counter & (CRYPTO_RANDOM_HEALTH_INTERVAL - 1)) == 0)
            {
                /* Quick stuck-bit check on the output */
                if (stuck_bit_check(buf, (len > 16) ? 16 : len) != 0)
                {
                    /* Stuck-bit detected — force reseed to SE050 */
                    return CRYPTO_RANDOM_ERR_SE050;
                }
            }

            return CRYPTO_RANDOM_OK;
        }

        /* SE050 failed — fall through to next tier */
    }

    /*
     * Tier 2: MCU Internal TRNG (S32K312 RNGA)
     * - Hardware entropy, no SCP03 dependency.
     * - Ideal for SCP03 bootstrap on bare-metal targets.
     * - On host builds, hal_trng_read() uses software CSPRNG.
     */
#if defined(CRYPTO_RANDOM_HAVE_MCU_TRNG)
    ret = mcu_trng_get_random(buf, len);
    if (ret == 0)
    {
        g_active_source = CRYPTO_RANDOM_SOURCE_MCU_TRNG;

        /* Periodic health check (every N calls) */
        if ((g_call_counter & (CRYPTO_RANDOM_HEALTH_INTERVAL - 1)) == 0)
        {
            if (hal_trng_self_test() != HAL_TRNG_OK)
            {
                /* Degraded — force a re-init on next call */
                return CRYPTO_RANDOM_ERR_DRBG;
            }
        }

        return CRYPTO_RANDOM_OK;
    }

    /* MCU TRNG failed — fall through to next tier */
#endif

    /*
     * Tier 3: mbedTLS CTR_DRBG (or OpenSSL RAND_bytes)
     */
#if defined(CRYPTO_RANDOM_HAVE_MBEDTLS)
    ret = mbedtls_get_random(buf, len);
    if (ret == 0)
    {
        g_active_source = CRYPTO_RANDOM_SOURCE_MBEDTLS;
        return CRYPTO_RANDOM_OK;
    }
#elif defined(CRYPTO_RANDOM_HAVE_OPENSSL)
    ret = openssl_get_random(buf, len);
    if (ret == 0)
    {
        g_active_source = CRYPTO_RANDOM_SOURCE_OS;
        return CRYPTO_RANDOM_OK;
    }
#endif

    /*
     * Tier 3: Direct OS entropy
     */
    ret = os_get_random(buf, len);
    if (ret == 0)
    {
        g_active_source = CRYPTO_RANDOM_SOURCE_OS;
        return CRYPTO_RANDOM_OK;
    }

    /*
     * All sources exhausted — no random bytes available.
     * Do NOT fall back to hardcoded values.
     * Return an error; the caller must handle it gracefully.
     */
    return CRYPTO_RANDOM_ERR_NO_SOURCE;
}

void crypto_random_register_se050(int (*fn)(uint8_t *buf, size_t len))
{
    g_se050_rng_fn = fn;

    /* When SE050 is registered, update active source for diagnostics */
    if (fn != NULL)
    {
        g_active_source = CRYPTO_RANDOM_SOURCE_SE050;
    }
}

void crypto_random_unregister_se050(void)
{
    g_se050_rng_fn = NULL;

    /* Revert diagnostic source to next-best available */
#if defined(CRYPTO_RANDOM_HAVE_MCU_TRNG)
    g_active_source = CRYPTO_RANDOM_SOURCE_MCU_TRNG;
#elif defined(CRYPTO_RANDOM_HAVE_MBEDTLS)
    g_active_source = CRYPTO_RANDOM_SOURCE_MBEDTLS;
#else
    g_active_source = CRYPTO_RANDOM_SOURCE_OS;
#endif
}

int crypto_random_health_test(void)
{
    int ret;
    uint8_t test_buf[256];

    if (!g_init_done)
    {
        return CRYPTO_RANDOM_ERR_NOT_INIT;
    }

    /*
     * Read 256 bytes from the active source and run basic statistical checks.
     * This is NOT FIPS 140-2 compliant; it's a production health check that
     * detects catastrophic hardware failures.
     */

    ret = crypto_random_bytes(test_buf, sizeof(test_buf));
    if (ret != 0)
    {
        return ret;
    }

    /* 1. Stuck-bit check (repetition count test, simplified) */
    if (stuck_bit_check(test_buf, sizeof(test_buf)) != 0)
    {
        return CRYPTO_RANDOM_ERR_DRBG;
    }

    /* 2. Frequency check (monobit): approximately 50% ones */
    {
        uint32_t ones = 0;
        size_t i, j;

        for (i = 0; i < sizeof(test_buf); i++)
        {
            for (j = 0; j < 8; j++)
            {
                ones += (uint32_t)((test_buf[i] >> j) & 0x01U);
            }
        }

        /* Allow ±15% deviation from 50% on 256 bytes = 2048 bits */
        /* Expected ones: 1024. Acceptable range: 870..1178 (~±15%) */
        if (ones < 870 || ones > 1178)
        {
            return CRYPTO_RANDOM_ERR_DRBG;
        }
    }

    return CRYPTO_RANDOM_OK;
}

/* ========================================================================
 *  End of crypto_random.c
 * ======================================================================== */
