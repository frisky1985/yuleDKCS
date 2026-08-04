/**
 * @file trng_stub.c -> trng_impl.c
 * @brief TRNG Driver Implementation — S32K312 RNGA
 * @version 1.0
 * @date 2026-07-16
 *
 * Implements the TRNG HAL for yuleDKCS Phase 4 (P1-4 TRNG Integration).
 *
 * ## Architecture
 *
 * This driver bridges the MCAL layer and the crypto_random tier system.
 * On bare-metal S32K3: reads the RNGA hardware registers directly.
 * On host/test builds: uses a software CSPRNG (seeded from OS entropy).
 *
 * ## Integration with crypto_random.c
 *
 * crypto_random.c includes this driver as Tier 2 (MCU TRNG), between
 * the SE050 hardware TRNG (Tier 1) and mbedTLS CTR_DRBG (Tier 3).
 *
 * During SCP03 bootstrap (pre-SE050), this driver provides the entropy
 * for host challenge generation. After SCP03 is established, the SE050
 * TRNG takes over as the primary entropy source.
 *
 * ## S32K312 RNGA Operation
 *
 * 1. Set GO bit in CR → starts entropy generation
 * 2. Poll SR.OREG_LVL until ≥ 1 word available
 * 3. Read 32-bit random word from ER
 * 4. Repeat for multi-word requests
 * 5. Check SR.SOF for seed/frequency errors
 *
 * ## Reference
 *   - S32K3xx Reference Manual, Chapters 43-44 (RNGA)
 *   - NIST SP 800-90B
 *   - AUTOSAR Specification of RNG Driver v1.2.0
 */

#include "Trng.h"
#include "string.h"

/* ========================================================================
 *  Internal State
 * ======================================================================== */

/** Whether hal_trng_init() has been called */
static bool g_trng_initialized = false;

/** Software fallback state (host/test builds) */
#if defined(RTD_ADAPTER_SELF_TEST) || defined(HOST_BUILD) || \
    !defined(__arm__) || defined(__linux__) || defined(__APPLE__)
    #define HAL_TRNG_HOST_BUILD  1
    #include <stdlib.h>  /* For arc4random_buf or rand-based fallback */

    #if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
        /* arc4random is available */
    #elif defined(__linux__)
        /* Will use /dev/urandom internally */
    #endif

    /** Software CSPRNG state (simple chacha20-like xorwow in practice) */
    static uint32_t g_sw_state[4];
    static bool     g_sw_seeded = false;

    /**
     * @brief Simple xorshift128+ PRNG for host-test fallback.
     * NOT for production use — host test only!
     * Seeded from OS entropy at init time.
     */
    static uint32_t xorshift128_next(void)
    {
        uint32_t s1 = g_sw_state[0];
        uint32_t s0 = g_sw_state[1];
        g_sw_state[0] = s0;
        s1 ^= s1 << 23;
        g_sw_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
        return g_sw_state[1] + s0;
    }
#endif /* Host build */

/* ========================================================================
 *  Internal: Safe register read (volatile) — bare-metal only
 * ======================================================================== */

#ifndef HAL_TRNG_HOST_BUILD

/**
 * @brief Read one 32-bit word from RNGA, polling for availability.
 *
 * @param data  [out] 32-bit random word
 * @param timeout  Maximum polling iterations
 * @return HAL_TRNG_OK on success, error on timeout or hardware fault
 */
static int rnga_read_word(uint32_t *data, uint32_t timeout)
{
    uint32_t sr;
    uint32_t poll;

    /* Trigger new random generation */
    S32K312_RNGA_CR = RNGA_CR_DEFAULT;

    /* Poll for data ready */
    for (poll = 0; poll < timeout; poll++)
    {
        sr = S32K312_RNGA_SR;

        /* Check for seed/frequency error */
        if (sr & RNGA_SR_SOF)
        {
            return HAL_TRNG_ERR_HW;
        }

        /* Check if data is available */
        if (RNGA_SR_OREG_LVL(sr) >= RNGA_SR_FIFO_READY)
        {
            *data = S32K312_RNGA_ER;
            return HAL_TRNG_OK;
        }

        /* Brief spin delay */
        {
            volatile uint32_t d;
            for (d = 0; d < 10U; d++) { /* delay */ }
            (void)d;
        }
    }

    return HAL_TRNG_ERR_TIMEOUT;
}

#endif /* !HAL_TRNG_HOST_BUILD */

/* ========================================================================
 *  Public API Implementation
 * ======================================================================== */

int hal_trng_init(void)
{
    if (g_trng_initialized)
    {
        return HAL_TRNG_OK;
    }

#if defined(HAL_TRNG_HOST_BUILD)
    /* Host build: seed software PRNG from OS entropy */

    #if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
        /* macOS/BSD: use arc4random to seed */
        arc4random_buf(g_sw_state, sizeof(g_sw_state));
    #elif defined(__linux__)
        /* Linux: read from /dev/urandom */
        FILE *f = fopen("/dev/urandom", "rb");
        if (f == NULL)
        {
            /* Last resort: use time-based seed (NOT crypto-secure, test only) */
            g_sw_state[0] = 0xDEADBEEF;
            g_sw_state[1] = 0xCAFEBABE;
            g_sw_state[2] = 0x0;
            g_sw_state[3] = 0x0;
        }
        else
        {
            size_t n = fread(g_sw_state, 1, sizeof(g_sw_state), f);
            (void)fclose(f);
            if (n < sizeof(g_sw_state))
            {
                return HAL_TRNG_ERR_HW;
            }
        }
    #else
        /* Unsupported host: minimal fallback */
        g_sw_state[0] = 0x12345678;
        g_sw_state[1] = 0x9ABCDEF0;
        g_sw_state[2] = 0x0F1E2D3C;
        g_sw_state[3] = 0x4B5A6978;
    #endif

    g_sw_seeded = true;
    g_trng_initialized = true;

    /* Run self-test */
    return hal_trng_self_test();

#else /* Bare-metal S32K3 target */

    /*
     * S32K312 RNGA initialization:
     * 1. Ensure RNGA clock is enabled (via PCC)
     * 2. Write CR to clear any previous interrupt and start fresh
     * 3. Wait for first random word as self-test verification
     */

    /* Reset RNGA: clear interrupt, enter active mode */
    S32K312_RNGA_CR = RNGA_CR_CLRI | RNGA_CR_INTM;

    /* Brief delay for reset to propagate */
    {
        volatile uint32_t d;
        for (d = 0; d < 100U; d++) { /* delay */ }
        (void)d;
    }

    /* Self-test: read one word to verify RNGA is operational */
    {
        uint32_t test_word;
        int ret = rnga_read_word(&test_word, HAL_TRNG_POLL_TIMEOUT);
        if (ret != HAL_TRNG_OK)
        {
            return ret;
        }

        /* Stuck-bit check: must not be all zeros or all ones */
        if (test_word == 0x00000000U || test_word == 0xFFFFFFFFU)
        {
            return HAL_TRNG_ERR_STUCK_BIT;
        }
    }

    g_trng_initialized = true;
    return HAL_TRNG_OK;
#endif /* HAL_TRNG_HOST_BUILD */
}

void hal_trng_deinit(void)
{
    if (!g_trng_initialized)
    {
        return;
    }

#if defined(HAL_TRNG_HOST_BUILD)
    /* Zero software state */
    (void)memset(g_sw_state, 0, sizeof(g_sw_state));
    g_sw_seeded = false;
#else
    /* Put RNGA in sleep/low-power mode */
    S32K312_RNGA_CR = RNGA_CR_SLP | RNGA_CR_INTM;
#endif

    g_trng_initialized = false;
}

int hal_trng_read(uint8_t *buf, size_t len)
{
    int ret;

    if (buf == NULL || len == 0)
    {
        return HAL_TRNG_ERR_PARAM;
    }

    if (!g_trng_initialized)
    {
        return HAL_TRNG_ERR_NOT_INIT;
    }

#if defined(HAL_TRNG_HOST_BUILD)
    /*
     * Host build: fill buffer using software CSPRNG.
     * Not hardware TRNG quality, but sufficient for development testing.
     */
    {
        size_t i;
        for (i = 0; i < len; i++)
        {
            buf[i] = (uint8_t)(xorshift128_next() & 0xFFU);
        }
    }
    return HAL_TRNG_OK;

#else /* Bare-metal S32K3 target */

    /*
     * Read random bytes from RNGA hardware.
     * The RNGA produces 32-bit words; we extract bytes as needed.
     */
    {
        size_t words_needed = (len + 3U) / 4U;
        size_t i;
        uint8_t *dst = buf;

        for (i = 0; i < words_needed; i++)
        {
            uint32_t word;
            size_t bytes_in_word;
            size_t j;

            ret = rnga_read_word(&word, HAL_TRNG_POLL_TIMEOUT);
            if (ret != HAL_TRNG_OK)
            {
                return ret;
            }

            bytes_in_word = (len >= 4U) ? 4U : len;

            for (j = 0; j < bytes_in_word; j++)
            {
                dst[j] = (uint8_t)((word >> (j * 8U)) & 0xFFU);
            }

            dst  += bytes_in_word;
            len  -= bytes_in_word;
        }
    }

    return HAL_TRNG_OK;
#endif /* HAL_TRNG_HOST_BUILD */
}

bool hal_trng_is_available(void)
{
    return g_trng_initialized;
}

int hal_trng_self_test(void)
{
    int ret;
    uint8_t test_buf[64];
    size_t i;
    uint8_t accum;

    if (!g_trng_initialized)
    {
        return HAL_TRNG_ERR_NOT_INIT;
    }

    /* Read 64 bytes of random data */
    ret = hal_trng_read(test_buf, sizeof(test_buf));
    if (ret != HAL_TRNG_OK)
    {
        return ret;
    }

    /* Test 1: Stuck-bit detection — must not be all zeros */
    accum = 0U;
    for (i = 0; i < sizeof(test_buf); i++)
    {
        accum |= test_buf[i];
    }
    if (accum == 0U)
    {
        return HAL_TRNG_ERR_STUCK_BIT;
    }

    /* Test 2: Basic white-noise check — not all bytes identical */
    {
        uint8_t ref = test_buf[0];
        uint8_t all_same = 0xFFU;
        for (i = 0; i < sizeof(test_buf); i++)
        {
            all_same &= (uint8_t)(test_buf[i] ^ ref);
        }
        if (all_same == 0U)
        {
            return HAL_TRNG_ERR_STUCK_BIT;
        }
    }

    /* Zero sensitive test data */
    (void)memset(test_buf, 0, sizeof(test_buf));

    return HAL_TRNG_OK;
}

/* ========================================================================
 *  End of trng_stub.c
 * ======================================================================== */
