/**
 * @file se050_scp03.c
 * @module EMB-BSW-SE050-SCP03 (ASPICE SWE.4)
 * @brief SE050 SCP03 Secure Channel Protocol — Full Implementation
 * @version 1.0
 * @date 2026-07-16
 *
 * Implements GlobalPlatform SCP03 over NXP SE050 via I2C / ISO 7816-4 APDU.
 *
 * ## Protocol Flow
 *
 *                       Host                          SE050
 *                         │                             │
 *   sec_scp03_open()      │                             │
 *     ├─ 1. INIT UPDATE   │──── Host Challenge ────────>│
 *     │                   │<─── Card Challenge +        │
 *     │                   │     Card Cryptogram + Seq   │
 *     ├─ 2. Key Derivation│   (Host computes S-ENC,     │
 *     │                   │    S-MAC, S-RMAC)           │
 *     ├─ 3. EXT AUTH      │──── Host Cryptogram ───────>│
 *     │                   │<─── SW=0x9000                │
 *   │ Secure Channel UP   │============================>│
 *                         │                             │
 *   sec_scp03_apdu()      │──── Secured APDU (C-MAC) ──>│
 *                         │<─── Secured Response ───────│
 *                         │     (R-MAC appended)        │
 *   sec_scp03_close()     │──── RESET or CLOSE ────────>│
 *                         │                             │
 *
 * ## Key Derivation (AES-128)
 *
 * Derivation data D_i (16 bytes):
 *   D_i = 0x01 || counter || 0x00*6 || seq_counter || 0x80 || 0x00*5
 *
 * Session keys:
 *   S-ENC  = AES-128(K_ENC,  D_01)    (counter = 0x01)
 *   S-MAC  = AES-128(K_MAC,  D_02)    (counter = 0x02)
 *   S-RMAC = AES-128(K_RMAC, D_03)    (counter = 0x03)
 *
 * ## Secure APDU Messaging
 *
 * C-MAC computation (ISO/IEC 9797-1 MAC Alg 3 / AES-CMAC):
 *   MAC_Input = CLA || INS || P1 || P2 || Lc || Data || '80' || '00'*padding
 *   C-MAC = AES-CMAC(S-MAC, MAC_Input)
 *   Truncated to SCP03_CMAC_TRUNC bytes, appended to APDU
 *
 * Reference:
 *   - GlobalPlatform Card Spec v2.3.1 Amendment D (SCP03)
 *   - NIST SP 800-38B: AES-CMAC
 *   - ISO/IEC 7816-4: APDU format
 *   - NXP AN12413
 *
 * MISRA Compliance:
 *   - All function definitions have MISRA-style comments
 *   - No dynamic allocation (stack-only)
 *   - Secure zero of all key material
 *   - No undefined behavior
 *   - Explicit cast for integer promotions
 */

#include "se050_scp03.h"
#include "crypto_types.h"   /* For crypto_secure_zero() style utilities */
#include "crypto_random.h"  /* [P1-1] TRNG-backed crypto_random_bytes() */

#include <string.h>

/* ========================================================================
 * SE050 SCP03 secure channel implementation.
 * ======================================================================== */

/* SE050 I2C address (used for session close RESET) */
#define SE050_I2C_ADDR          0x48U
/*
 *  Internal: AES-128 ECB (FIPS 197) — Standalone, no OpenSSL dependency
 * ========================================================================
 * Pure C AES-128 implementation. Only used for SCP03 key derivation
 * and CMAC computation. Does NOT conflict with AES-256 in crypto_engine.
 */

/* AES S-box (FIPS 197 Figure 7) */
static const uint8_t s_scp03_sbox[256] = {
    0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
    0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
    0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
    0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
    0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
    0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
    0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
    0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
    0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
    0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
    0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
    0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
    0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
    0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
    0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
    0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16
};

/** AES round constants */
static const uint8_t s_scp03_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36
};

/** AES-128 round keys: 11 rounds × 4 words = 44 words */
typedef struct {
    uint32_t rk[44];
} scp03_aes128_ctx_t;

/* Internal: uint32_t big-endian load/store for AES */
static inline uint32_t scp03_load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static inline void scp03_store_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

/* GF(2^8) multiply by 2 in AES field */
static inline uint8_t scp03_gf_mul2(uint8_t x)
{
    return (uint8_t)((uint8_t)(x << 1) ^ ((x & 0x80U) ? 0x1BU : 0x00U));
}

/* GF(2^8) multiply by 3 in AES field */
static inline uint8_t scp03_gf_mul3(uint8_t x)
{
    return scp03_gf_mul2(x) ^ x;
}

/* SubWord: apply S-box to each byte of a 32-bit word */
static inline uint32_t scp03_sub_word(uint32_t w)
{
    return ((uint32_t)s_scp03_sbox[(w >> 24) & 0xFFU] << 24) |
           ((uint32_t)s_scp03_sbox[(w >> 16) & 0xFFU] << 16) |
           ((uint32_t)s_scp03_sbox[(w >>  8) & 0xFFU] <<  8) |
           ((uint32_t)s_scp03_sbox[ w        & 0xFFU]);
}

/* RotWord: cyclic left rotation by 1 byte */
static inline uint32_t scp03_rot_word(uint32_t w)
{
    return (w << 8) | (w >> 24);
}

/**
 * @brief AES-128 key expansion (FIPS 197 Section 5.2).
 *
 * Generates 44 round key words from a 16-byte key.
 */
static void scp03_aes128_key_expand(scp03_aes128_ctx_t *ctx, const uint8_t key[16])
{
    uint32_t *W = ctx->rk;

    /* First 4 words directly from key */
    for (int i = 0; i < 4; i++)
    {
        W[i] = scp03_load_be32(key + (uint32_t)i * 4U);
    }

    /* Expand to 44 words */
    for (int i = 4; i < 44; i++)
    {
        uint32_t temp = W[(uint32_t)i - 1U];

        if ((i % 4) == 0)
        {
            temp = scp03_sub_word(scp03_rot_word(temp)) ^
                   ((uint32_t)s_scp03_rcon[(uint32_t)i / 4U] << 24);
        }

        W[i] = W[(uint32_t)i - 4U] ^ temp;
    }
}

/**
 * @brief AES-128 single-block ECB encrypt (FIPS 197).
 *
 * @param ctx  Expanded key schedule
 * @param in   Input block (16 bytes)
 * @param out  Output block (16 bytes, may alias in)
 */
static void scp03_aes128_encrypt(const scp03_aes128_ctx_t *ctx,
                                  const uint8_t in[16], uint8_t out[16])
{
    uint32_t s[4];
    uint8_t  tmp[16];
    uint32_t round;

    /* Initial AddRoundKey */
    s[0] = scp03_load_be32(in)      ^ ctx->rk[0];
    s[1] = scp03_load_be32(in + 4)  ^ ctx->rk[1];
    s[2] = scp03_load_be32(in + 8)  ^ ctx->rk[2];
    s[3] = scp03_load_be32(in + 12) ^ ctx->rk[3];

    /* Main rounds (1..9): SubBytes → ShiftRows → MixColumns → AddRoundKey */
    for (round = 1; round < 10; round++)
    {
        uint8_t t[16];
        uint32_t rk_off;

        /* SubBytes */
        for (int i = 0; i < 4; i++)
        {
            t[0 + (uint32_t)i * 4U] = s_scp03_sbox[(s[i] >> 24) & 0xFFU];
            t[1 + (uint32_t)i * 4U] = s_scp03_sbox[(s[i] >> 16) & 0xFFU];
            t[2 + (uint32_t)i * 4U] = s_scp03_sbox[(s[i] >>  8) & 0xFFU];
            t[3 + (uint32_t)i * 4U] = s_scp03_sbox[ s[i]        & 0xFFU];
        }

        /* ShiftRows — state stored column-major; rows are t[0..3], t[4..7], etc. */
        {
            uint8_t sr[16];

            /* Row 0: no shift */
            sr[0]  = t[0];  sr[4]  = t[4];  sr[8]  = t[8];  sr[12] = t[12];
            /* Row 1: left shift 1 */
            sr[1]  = t[5];  sr[5]  = t[9];  sr[9]  = t[13]; sr[13] = t[1];
            /* Row 2: left shift 2 */
            sr[2]  = t[10]; sr[6]  = t[14]; sr[10] = t[2];  sr[14] = t[6];
            /* Row 3: left shift 3 */
            sr[3]  = t[15]; sr[7]  = t[3];  sr[11] = t[7];  sr[15] = t[11];

            /* MixColumns — operate on columns of sr */
            for (int c = 0; c < 4; c++)
            {
                uint8_t a0 = sr[0 + (uint32_t)c * 4U];
                uint8_t a1 = sr[1 + (uint32_t)c * 4U];
                uint8_t a2 = sr[2 + (uint32_t)c * 4U];
                uint8_t a3 = sr[3 + (uint32_t)c * 4U];

                tmp[0 + (uint32_t)c * 4U] = scp03_gf_mul2(a0) ^ scp03_gf_mul3(a1) ^ a2 ^ a3;
                tmp[1 + (uint32_t)c * 4U] = a0 ^ scp03_gf_mul2(a1) ^ scp03_gf_mul3(a2) ^ a3;
                tmp[2 + (uint32_t)c * 4U] = a0 ^ a1 ^ scp03_gf_mul2(a2) ^ scp03_gf_mul3(a3);
                tmp[3 + (uint32_t)c * 4U] = scp03_gf_mul3(a0) ^ a1 ^ a2 ^ scp03_gf_mul2(a3);
            }
        }

        /* AddRoundKey */
        rk_off = round * 4U;
        s[0] = scp03_load_be32(tmp)      ^ ctx->rk[rk_off];
        s[1] = scp03_load_be32(tmp + 4)  ^ ctx->rk[rk_off + 1U];
        s[2] = scp03_load_be32(tmp + 8)  ^ ctx->rk[rk_off + 2U];
        s[3] = scp03_load_be32(tmp + 12) ^ ctx->rk[rk_off + 3U];
    }

    /* Final round (10): SubBytes → ShiftRows → AddRoundKey (no MixColumns) */
    {
        uint8_t t[16];
        uint32_t rk_off;

        /* SubBytes */
        for (int i = 0; i < 4; i++)
        {
            t[0 + (uint32_t)i * 4U] = s_scp03_sbox[(s[i] >> 24) & 0xFFU];
            t[1 + (uint32_t)i * 4U] = s_scp03_sbox[(s[i] >> 16) & 0xFFU];
            t[2 + (uint32_t)i * 4U] = s_scp03_sbox[(s[i] >>  8) & 0xFFU];
            t[3 + (uint32_t)i * 4U] = s_scp03_sbox[ s[i]        & 0xFFU];
        }

        /* ShiftRows */
        out[0]  = t[0];  out[4]  = t[4];  out[8]  = t[8];  out[12] = t[12];
        out[1]  = t[5];  out[5]  = t[9];  out[9]  = t[13]; out[13] = t[1];
        out[2]  = t[10]; out[6]  = t[14]; out[10] = t[2];  out[14] = t[6];
        out[3]  = t[15]; out[7]  = t[3];  out[11] = t[7];  out[15] = t[11];

        /* AddRoundKey (last round) */
        rk_off = 10U * 4U;
        scp03_store_be32(out,      scp03_load_be32(out)     ^ ctx->rk[rk_off]);
        scp03_store_be32(out + 4,  scp03_load_be32(out + 4) ^ ctx->rk[rk_off + 1U]);
        scp03_store_be32(out + 8,  scp03_load_be32(out + 8) ^ ctx->rk[rk_off + 2U]);
        scp03_store_be32(out + 12, scp03_load_be32(out + 12) ^ ctx->rk[rk_off + 3U]);
    }
}

/* ========================================================================
 *  Internal: AES-CMAC (NIST SP 800-38B)
 * ========================================================================
 * Used for C-MAC/R-MAC computation and cryptogram generation.
 */

/**
 * @brief Generate CMAC subkeys K1 and K2.
 *
 * @param key  AES-128 key (16 bytes)
 * @param k1   [out] Subkey K1 (16 bytes)
 * @param k2   [out] Subkey K2 (16 bytes)
 */
static void scp03_cmac_generate_subkeys(const uint8_t key[16],
                                         uint8_t k1[16], uint8_t k2[16])
{
    scp03_aes128_ctx_t ctx;
    uint8_t zero_block[16];
    uint8_t L[16];
    uint8_t carry;
    int i;

    (void)memset(zero_block, 0, sizeof(zero_block));

    scp03_aes128_key_expand(&ctx, key);
    scp03_aes128_encrypt(&ctx, zero_block, L);

    /* K1 = dbl(L) */
    carry = (uint8_t)((L[0] & 0x80U) ? 1U : 0U);
    for (i = 0; i < 15; i++)
    {
        k1[i] = (uint8_t)((uint8_t)(L[i] << 1) | (uint8_t)(L[(uint32_t)i + 1U] >> 7));
    }
    k1[15] = (uint8_t)(L[15] << 1);
    if (carry != 0U)
    {
        k1[15] ^= 0x87U;
    }

    /* K2 = dbl(K1) */
    carry = (uint8_t)((k1[0] & 0x80U) ? 1U : 0U);
    for (i = 0; i < 15; i++)
    {
        k2[i] = (uint8_t)((uint8_t)(k1[i] << 1) | (uint8_t)(k1[(uint32_t)i + 1U] >> 7));
    }
    k2[15] = (uint8_t)(k1[15] << 1);
    if (carry != 0U)
    {
        k2[15] ^= 0x87U;
    }

    /* Zero sensitive intermediates */
    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(zero_block, 0, sizeof(zero_block));
    (void)memset(L, 0, sizeof(L));
}

/**
 * @brief Compute AES-CMAC (NIST SP 800-38B) over message.
 *
 * @param key     AES-128 key (16 bytes)
 * @param msg     Message input
 * @param msg_len Message length in bytes
 * @param mac     [out] CMAC output (16 bytes)
 */
static void scp03_aes_cmac(const uint8_t key[16],
                            const uint8_t *msg, uint16_t msg_len,
                            uint8_t mac[16])
{
    scp03_aes128_ctx_t ctx;
    uint8_t k1[16], k2[16];
    uint8_t n, last_complete;
    uint16_t offset;
    uint8_t i;

    scp03_cmac_generate_subkeys(key, k1, k2);

    scp03_aes128_key_expand(&ctx, key);

    /* Number of blocks. Zero-length message = 1 block (padded). */
    n = (msg_len == 0) ? 1U : (uint8_t)(((uint32_t)msg_len + 15U) / 16U);
    last_complete = (uint8_t)((uint32_t)msg_len % 16U == 0U) ? 1U : 0U;

    /* Process all blocks except last, CBC-MAC style */
    {
        uint8_t cb[16];
        (void)memset(cb, 0, sizeof(cb));

        for (i = 0; i < (uint8_t)(n - 1U); i++)
        {
            uint8_t block[16];
            uint8_t j;
            offset = (uint16_t)i * 16U;

            for (j = 0; j < 16; j++)
            {
                block[j] = (uint8_t)(msg[(uint32_t)offset + (uint32_t)j] ^ cb[j]);
            }

            scp03_aes128_encrypt(&ctx, block, cb);
        }

        /* Process last block */
        {
            uint8_t last_block[16];
            uint8_t j;
            uint8_t *subkey;

            offset = (uint16_t)(n - 1U) * 16U;
            subkey = (last_complete != 0U) ? k1 : k2;

            /* Copy message data */
            for (j = 0; j < 16; j++)
            {
                if ((uint32_t)offset + (uint32_t)j < (uint32_t)msg_len)
                {
                    last_block[j] = msg[(uint32_t)offset + (uint32_t)j];
                }
                else if ((uint32_t)offset + (uint32_t)j == (uint32_t)msg_len)
                {
                    last_block[j] = 0x80U; /* Padding: append 1 bit */
                }
                else
                {
                    last_block[j] = 0x00U;
                }
            }

            /* XOR with subkey and previous ciphertext */
            for (j = 0; j < 16; j++)
            {
                last_block[j] ^= cb[j] ^ subkey[j];
            }

            scp03_aes128_encrypt(&ctx, last_block, mac);
        }

        (void)memset(cb, 0, sizeof(cb));
    }

    /* Cleanup */
    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(k1, 0, sizeof(k1));
    (void)memset(k2, 0, sizeof(k2));
}

/* ========================================================================
 *  Internal: SCP03 Session Key Derivation (GlobalPlatform SCP03)
 * ========================================================================
 *
 * Derivation data format (single AES block, 16 bytes):
 *   0x01 || counter || 00 * 6 || seq_counter || 0x80 || 00 * 5
 */

/**
 * @brief Derive a single SCP03 session key.
 *
 * @param static_key Static key (K_ENC, K_MAC, or K_RMAC), 16 bytes
 * @param derivation_ctr Derivation counter (0x01=S-ENC, 0x02=S-MAC, 0x03=S-RMAC)
 * @param seq_counter  2-byte sequence counter (big-endian)
 * @param session_key  [out] Derived session key (16 bytes)
 * @return 0 on success
 */
static int scp03_derive_session_key(const uint8_t static_key[16],
                                     uint8_t derivation_ctr,
                                     const uint8_t seq_counter[2],
                                     uint8_t session_key[16])
{
    scp03_aes128_ctx_t ctx;
    uint8_t derive_data[16];

    /* Build derivation data */
    derive_data[0] = 0x01U;
    derive_data[1] = derivation_ctr;
    (void)memset(&derive_data[2], 0x00, 6);    /* bytes 2–7: zero */
    derive_data[8] = seq_counter[0];             /* seq counter MSB */
    derive_data[9] = seq_counter[1];             /* seq counter LSB */
    derive_data[10] = 0x80U;                     /* mandatory padding start */
    (void)memset(&derive_data[11], 0x00, 5);    /* bytes 11–15: zero padding */

    scp03_aes128_key_expand(&ctx, static_key);
    scp03_aes128_encrypt(&ctx, derive_data, session_key);

    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(derive_data, 0, sizeof(derive_data));

    return 0;
}

/**
 * @brief Derive all three SCP03 session keys.
 *
 * @param static_enc  K_ENC (16 bytes)
 * @param static_mac  K_MAC (16 bytes)
 * @param static_rmac K_RMAC (16 bytes)
 * @param seq_counter 2-byte sequence counter
 * @param s_enc       [out] S-ENC (16 bytes)
 * @param s_mac       [out] S-MAC (16 bytes)
 * @param s_rmac      [out] S-RMAC (16 bytes)
 */
static void scp03_derive_session_keys(const uint8_t static_enc[16],
                                       const uint8_t static_mac[16],
                                       const uint8_t static_rmac[16],
                                       const uint8_t seq_counter[2],
                                       uint8_t s_enc[16],
                                       uint8_t s_mac[16],
                                       uint8_t s_rmac[16])
{
    (void)scp03_derive_session_key(static_enc,  SCP03_DERIVE_S_ENC,  seq_counter, s_enc);
    (void)scp03_derive_session_key(static_mac,  SCP03_DERIVE_S_MAC,  seq_counter, s_mac);
    (void)scp03_derive_session_key(static_rmac, SCP03_DERIVE_S_RMAC, seq_counter, s_rmac);
}

/* ========================================================================
 *  Internal: I2C APDU Transport
 * ========================================================================
 * Wraps the platform I2C HAL for SE050 communication.
 *
 * SE050 I2C protocol:
 *   - Write: I2C address (0x48 << 1) + APDU length prefix + APDU bytes
 *   - Read:  I2C address (0x48 << 1 | 1) + response bytes
 *   - Retry: Poll until response available (up to N retries)
 */

/* SE050 I2C transaction timeout (approximate ms per retry) */
#define SCP03_I2C_RETRY_MS      1U
#define SCP03_I2C_MAX_RETRIES   100U    /* 100 ms total timeout */

/* Platform I2C transfer — provided by BSP/HAL layer */
extern int32_t i2c_transfer(uint8_t dev, uint8_t addr,
                             const uint8_t *tx, uint16_t tx_len,
                             uint8_t *rx, uint16_t rx_len);

/**
 * @brief Read raw bytes from SE050 over I2C with retry.
 *
 * The SE050 may need time to process. We retry until data is available.
 *
 * @param i2c_addr   SE050 I2C address
 * @param buffer     [out] Response buffer
 * @param len        Number of bytes to read
 * @return 0 on success, negative on error
 */
static int scp03_i2c_read(uint8_t i2c_addr, uint8_t *buffer, uint16_t len)
{
    uint32_t retries;
    int32_t  result;

    for (retries = 0; retries < SCP03_I2C_MAX_RETRIES; retries++)
    {
        /* Read attempt */
        result = i2c_transfer(i2c_addr, i2c_addr | 0x01U,
                               NULL, 0, buffer, len);
        if (result == 0)
        {
            /* Verify we got non-zero data (SE050 returns 0xFF if not ready) */
            if (buffer[0] != 0xFFU)
            {
                return 0;
            }
        }

        /* Brief delay before retry */
        {
            volatile uint32_t d;
            for (d = 0; d < 1000U; d++) { /* spin — platform_delay_us would be better */ }
            (void)d;
        }
    }

    return SCP03_ERR_HW;
}

/**
 * @brief Write raw bytes to SE050 over I2C.
 *
 * @param i2c_addr  SE050 I2C address
 * @param data      Data to write
 * @param len       Data length
 * @return 0 on success, negative on error
 */
static int scp03_i2c_write(uint8_t i2c_addr, const uint8_t *data, uint16_t len)
{
    int32_t result;

    result = i2c_transfer(i2c_addr, i2c_addr, data, len, NULL, 0);

    return (result == 0) ? 0 : SCP03_ERR_HW;
}

/* ========================================================================
 *  Internal: ISO 7816-4 APDU Construction / Parsing
 * ========================================================================
 */

/**
 * @brief Parse APDU response, extracting data and status word.
 *
 * @param resp      [in] Raw response buffer (including SW)
 * @param resp_len  Total response length
 * @param data      [out] Pointer to start of response data within resp (may be NULL)
 * @param data_len  [out] Length of response data
 * @param sw        [out] Status word (2 bytes, big-endian)
 * @return 0 on success, SCP03_ERR_APDU if too short
 */
static int scp03_parse_response(const uint8_t *resp, uint16_t resp_len,
                                 uint16_t *data_len, uint16_t *sw)
{
    if (resp_len < 2U)
    {
        return SCP03_ERR_APDU;
    }

    /* Last 2 bytes are SW1 SW2 */
    *sw = ((uint16_t)resp[resp_len - 2U] << 8) | (uint16_t)resp[resp_len - 1U];

    if (data_len != NULL)
    {
        *data_len = (resp_len >= 2U) ? (resp_len - 2U) : 0U;
    }

    return 0;
}

/* ========================================================================
 *  Public API Implementation
 * ========================================================================
 */

/* ---- Lifecycle ---- */

void se050_scp03_secure_zero(void *ptr, size_t len)
{
    if (ptr != NULL)
    {
        volatile uint8_t *p = (volatile uint8_t *)ptr;
        size_t i;
        for (i = 0; i < len; i++)
        {
            p[i] = 0;
        }
    }
}

int se050_scp03_init(scp03_session_t *session)
{
    if (session == NULL)
    {
        return SCP03_ERR_NULL;
    }

    (void)memset(session, 0, sizeof(*session));

    /*
     * Default SCP03 transport keys (all zeros).
     * NXP SE050 ships with all-zero default transport keys.
     *
     * PRODUCTION REQUIREMENT [P0-1]:
     *   These MUST be replaced with provisioned keys during manufacturing
     *   via se050_scp03_provision_keys().
     */
    /* session->static_enc_key is all zeros from memset */
    /* session->static_mac_key is all zeros from memset */
    /* session->static_rmac_key is all zeros from memset */

    session->state        = SCP03_STATE_INIT;
    session->key_version  = 0x00;    /* Default key version */
    session->scp_cmd_count = 0;

    return SCP03_OK;
}

void se050_scp03_deinit(scp03_session_t *session)
{
    if (session != NULL)
    {
        se050_scp03_secure_zero(session->static_enc_key, sizeof(session->static_enc_key));
        se050_scp03_secure_zero(session->static_mac_key, sizeof(session->static_mac_key));
        se050_scp03_secure_zero(session->static_rmac_key, sizeof(session->static_rmac_key));
        se050_scp03_secure_zero(session->s_enc, sizeof(session->s_enc));
        se050_scp03_secure_zero(session->s_mac, sizeof(session->s_mac));
        se050_scp03_secure_zero(session->s_rmac, sizeof(session->s_rmac));
        se050_scp03_secure_zero(session->cmac_iv, sizeof(session->cmac_iv));
        se050_scp03_secure_zero(session->rmac_iv, sizeof(session->rmac_iv));
        (void)memset(session, 0, sizeof(*session));
    }
}

int se050_scp03_provision_keys(scp03_session_t *session,
                                const uint8_t enc_key[16],
                                const uint8_t mac_key[16],
                                const uint8_t rmac_key[16])
{
    if (session == NULL)
    {
        return SCP03_ERR_NULL;
    }

    if (session->state != SCP03_STATE_INIT)
    {
        /* Don't overwrite keys during active session; close first */
        return SCP03_ERR_CHANNEL;
    }

    if (enc_key != NULL)
    {
        (void)memcpy(session->static_enc_key, enc_key, SCP03_KEY_SIZE);
    }
    if (mac_key != NULL)
    {
        (void)memcpy(session->static_mac_key, mac_key, SCP03_KEY_SIZE);
    }
    if (rmac_key != NULL)
    {
        (void)memcpy(session->static_rmac_key, rmac_key, SCP03_KEY_SIZE);
    }

    return SCP03_OK;
}

/* ---- APDU Plain (no secure channel) ---- */

int se050_scp03_apdu_plain(uint8_t i2c_addr,
                            uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                            const uint8_t *data, uint16_t data_len,
                            uint8_t *resp, uint16_t *resp_len)
{
    /*
     * ISO 7816-4 APDU structure (short format):
     * [CLA][INS][P1][P2][Lc][Data][Le]
     *
     * We send Case 3/4 extended via short APDU:
     *   Case 3 (no response data): CLA INS P1 P2 Lc Data
     *   Case 4 (response data):    CLA INS P1 P2 Lc Data Le
     */
    uint8_t apdu_buffer[5 + SCP03_MAX_APDU_DATA + 1]; /* header + data + Le */
    uint16_t apdu_len;
    int ret;

    if (resp == NULL || resp_len == NULL)
    {
        return SCP03_ERR_NULL;
    }
    if (data_len > SCP03_MAX_APDU_DATA)
    {
        return SCP03_ERR_PARAM;
    }

    /* Build header */
    apdu_buffer[0] = cla;
    apdu_buffer[1] = ins;
    apdu_buffer[2] = p1;
    apdu_buffer[3] = p2;

    if (data_len > 0)
    {
        apdu_buffer[4] = (uint8_t)data_len;   /* Lc */
        if (data != NULL)
        {
            (void)memcpy(&apdu_buffer[5], data, data_len);
        }
        apdu_len = 5 + data_len;
    }
    else
    {
        /* Case 2: No data, expect response. Le = 0x00 for max 256 bytes. */
        apdu_buffer[4] = 0x00;  /* Le = max 256 */
        apdu_len = 5;
    }

    /* Write APDU to SE050 */
    ret = scp03_i2c_write(i2c_addr, apdu_buffer, apdu_len);
    if (ret != 0)
    {
        return SCP03_ERR_HW;
    }

    /* Read response (first byte tells us how many bytes follow) */
    if (apdu_len > 0 && data_len > 0)
    {
        /* Case 4: expect response data + SW */
        uint16_t max_read = (*resp_len < SCP03_MAX_APDU_RESP) ? *resp_len : SCP03_MAX_APDU_RESP;
        uint8_t read_buffer[SCP03_MAX_APDU_RESP + 2];

        ret = scp03_i2c_read(i2c_addr, read_buffer, max_read);
        if (ret != 0)
        {
            return SCP03_ERR_HW;
        }

        /* Copy response, strip SW for the caller */
        {
            uint16_t rlen = max_read;
            uint16_t sw;

            ret = scp03_parse_response(read_buffer, rlen, &rlen, &sw);
            if (ret != 0)
            {
                return ret;
            }

            if (sw != SCP03_SW_OK)
            {
                return SCP03_ERR_SW;
            }

            if (rlen > *resp_len)
            {
                rlen = *resp_len;
            }

            (void)memcpy(resp, read_buffer, rlen);
            *resp_len = rlen;
        }
    }
    else
    {
        /* Case 2: Read response data */
        uint16_t max_read = (*resp_len < SCP03_MAX_APDU_RESP) ? *resp_len : SCP03_MAX_APDU_RESP;
        uint8_t read_buffer[SCP03_MAX_APDU_RESP + 2];

        ret = scp03_i2c_read(i2c_addr, read_buffer, max_read + 2);
        if (ret != 0)
        {
            return SCP03_ERR_HW;
        }

        {
            uint16_t rlen = max_read + 2;
            uint16_t sw;

            ret = scp03_parse_response(read_buffer, rlen, &rlen, &sw);
            if (ret != 0)
            {
                return ret;
            }

            if (sw != SCP03_SW_OK)
            {
                return SCP03_ERR_SW;
            }

            if (rlen > *resp_len)
            {
                rlen = *resp_len;
            }

            (void)memcpy(resp, read_buffer, rlen);
            *resp_len = rlen;
        }
    }

    return SCP03_OK;
}

/* ---- SCP03 Session Establishment ---- */

int se050_scp03_open_session(scp03_session_t *session, uint8_t i2c_addr)
{
    uint8_t resp[SCP03_MAX_APDU_RESP];
    uint16_t resp_len;
    int ret;

    if (session == NULL)
    {
        return SCP03_ERR_NULL;
    }

    if (session->state != SCP03_STATE_INIT)
    {
        return SCP03_ERR_PARAM;
    }

    /* ----------------------------------------------------------------
     * Step 1: INITIALIZE UPDATE
     *
     * APDU: 80 50 00 00 08 <host_challenge_8bytes>
     * Response (SE050):
     *   [0..9]   Key Diversification Data (10 bytes)
     *   [10]     Key Check Value (1 byte)
     *   [11..12] Sequence Counter (2 bytes, big-endian)
     *   [13..20] Card Challenge (8 bytes)
     *   [21..28] Card Cryptogram (8 bytes)
     *   [29..30] SW1 SW2
     * ---------------------------------------------------------------- */

    /* [P1-4] Generate host challenge: 8 bytes of cryptographically secure random.
     *
     * Pre-SCP03 bootstrap tier chain (defined in crypto_random.c):
     *   Tier 1 — SE050 HW TRNG    (available only after SCP03 established)
     *   Tier 2 — MCU TRNG (RNGA)  (S32K312 internal, no external dependency)
     *   Tier 3 — mbedTLS CTR_DRBG (seeded from OS entropy, host builds)
     *   Tier 4 — OS entropy       (/dev/urandom, arc4random)
     *
     * On bare-metal S32K3 targets, Tier 2 (MCU TRNG) provides hardware entropy
     * without requiring SCP03 — solving the bootstrap chicken-and-egg problem.
     * The MCU RNGA module reads from a free-running oscillator ring for true
     * entropy. On host builds, Tier 3/4 serve as fallback.
     *
     * If ALL tiers fail, TERMINATE the session open attempt.
     * NO hardcoded fallback — weak entropy is a security vulnerability.
     */
    ret = crypto_random_bytes(session->host_challenge, SCP03_CHALLENGE_SIZE);
    if (ret != 0)
    {
        /* [P1-4] All entropy sources exhausted.
         * MCU TRNG (Tier 2), mbedTLS (Tier 3), and OS entropy (Tier 4) all
         * failed. This indicates a critical hardware fault — the device should
         * not continue. sec_init() must have validated entropy before reaching
         * this point, so a failure here suggests a runtime hardware degradation.
         */
        return SCP03_ERR_HW;
    }

    resp_len = sizeof(resp);
    ret = se050_scp03_apdu_plain(i2c_addr,
                                 0x80, SCP03_INS_INIT_UPDATE, 0x00, 0x00,
                                 session->host_challenge, SCP03_CHALLENGE_SIZE,
                                 resp, &resp_len);
    if (ret != 0)
    {
        return ret;
    }

    /* Validate response length */
    if (resp_len < (SCP03_KEY_DIVERS_SIZE + 1 + SCP03_SEQ_COUNTER_SIZE +
                     SCP03_CHALLENGE_SIZE + SCP03_CRYPTOGRAM_SIZE))
    {
        return SCP03_ERR_APDU;
    }

    /* Parse response */
    {
        uint16_t offset = 0;

        /* Key Diversification Data (10 bytes) */
        (void)memcpy(session->key_divers_data, resp + offset, SCP03_KEY_DIVERS_SIZE);
        offset += SCP03_KEY_DIVERS_SIZE;

        /* Key Check Value (1 byte) — SE050-specific */
        session->key_version = (uint8_t)resp[offset];
        offset += 1;

        /* Sequence Counter (2 bytes, big-endian) */
        session->seq_counter[0] = resp[offset];
        session->seq_counter[1] = resp[offset + 1U];
        offset += SCP03_SEQ_COUNTER_SIZE;

        /* Card Challenge (8 bytes) */
        (void)memcpy(session->card_challenge, resp + offset, SCP03_CHALLENGE_SIZE);
        offset += SCP03_CHALLENGE_SIZE;

        /* Card Cryptogram (8 bytes) — first 8 bytes of AES-CMAC */
        (void)memcpy(session->card_cryptogram, resp + offset, SCP03_CRYPTOGRAM_SIZE);
        offset += SCP03_CRYPTOGRAM_SIZE; /* avoid unused-variable */
        (void)offset;
    }

    /* ----------------------------------------------------------------
     * Step 2: Derive Session Keys
     *
     * S-ENC  = AES(K_ENC,  deriv_data(counter=0x01, seq))
     * S-MAC  = AES(K_MAC,  deriv_data(counter=0x02, seq))
     * S-RMAC = AES(K_RMAC, deriv_data(counter=0x03, seq))
     * ---------------------------------------------------------------- */
    scp03_derive_session_keys(
        session->static_enc_key, session->static_mac_key, session->static_rmac_key,
        session->seq_counter,
        session->s_enc, session->s_mac, session->s_rmac);

    /* ----------------------------------------------------------------
     * Step 3: Verify Card Cryptogram
     *
     * Card cryptogram = first 8 bytes of AES-CMAC(S-MAC, data)
     *   data = 01 || 01 || card_challenge || host_challenge
     *
     * (Note: The SE050 response is GMAC/HMAC-based in some revisions,
     * but standard SCP03 uses CMAC. We verify the CMAC.)
     * ---------------------------------------------------------------- */
    {
        uint8_t mac_input[2 + SCP03_CHALLENGE_SIZE + SCP03_CHALLENGE_SIZE]; /* 01 01 + CC + HC */
        uint8_t computed_mac[16];
        uint8_t match;
        uint16_t i;

        mac_input[0] = 0x01U;
        mac_input[1] = 0x01U;
        (void)memcpy(&mac_input[2], session->card_challenge, SCP03_CHALLENGE_SIZE);
        (void)memcpy(&mac_input[2 + SCP03_CHALLENGE_SIZE], session->host_challenge, SCP03_CHALLENGE_SIZE);

        scp03_aes_cmac(session->s_mac, mac_input, sizeof(mac_input), computed_mac);

        /* Verify first 8 bytes */
        match = 0U;
        for (i = 0; i < SCP03_CRYPTOGRAM_SIZE; i++)
        {
            match |= (uint8_t)(computed_mac[i] ^ session->card_cryptogram[i]);
        }

        se050_scp03_secure_zero(mac_input, sizeof(mac_input));
        se050_scp03_secure_zero(computed_mac, sizeof(computed_mac));

        if (match != 0U)
        {
            se050_scp03_close_session(session);
            return SCP03_ERR_CRYPTOGRAM;
        }
    }

    /* ----------------------------------------------------------------
     * Step 4: Compute Host Cryptogram
     *
     * Host cryptogram = first 8 bytes of AES-CMAC(S-MAC, data)
     *   data = 01 || 01 || host_challenge || card_challenge
     * ---------------------------------------------------------------- */
    {
        uint8_t mac_input[2 + SCP03_CHALLENGE_SIZE + SCP03_CHALLENGE_SIZE];
        uint8_t computed_mac[16];

        mac_input[0] = 0x01U;
        mac_input[1] = 0x01U;
        (void)memcpy(&mac_input[2], session->host_challenge, SCP03_CHALLENGE_SIZE);
        (void)memcpy(&mac_input[2 + SCP03_CHALLENGE_SIZE], session->card_challenge, SCP03_CHALLENGE_SIZE);

        scp03_aes_cmac(session->s_mac, mac_input, sizeof(mac_input), computed_mac);

        (void)memcpy(session->host_cryptogram, computed_mac, SCP03_CRYPTOGRAM_SIZE);

        se050_scp03_secure_zero(mac_input, sizeof(mac_input));
        se050_scp03_secure_zero(computed_mac, sizeof(computed_mac));
    }

    /* ----------------------------------------------------------------
     * Step 5: EXTERNAL AUTHENTICATE
     *
     * APDU: 84 82 00 00 08 <host_cryptogram_8bytes>
     * Where CLA = 0x84 means C-MAC only (no encryption for auth).
     *
     * For the EXTERNAL AUTHENTICATE command, we need to compute and
     * append the C-MAC per SCP03:
     *   C-MAC = AES-CMAC(S-MAC, CLA || INS || P1 || P2 || Lc || host_cryptogram)
     *
     * The CLA for the secured command includes MAC flag (0x04).
     * So CLA = 0x80 | 0x04 = 0x84
     *
     * APDU: 84 82 00 00 08 <host_cryptogram> <C-MAC truncated to 8 bytes>
     * ---------------------------------------------------------------- */
    {
        uint8_t secure_apdu[5 + SCP03_CRYPTOGRAM_SIZE + SCP03_CMAC_TRUNC];
        uint16_t secure_apdu_len;
        uint8_t mac_full[16];
        uint16_t mi;

        /* Build secured APDU: CLA(0x84) INS P1 P2 Lc host_cryptogram */
        secure_apdu[0] = SCP03_CLA_CMAC;     /* 0x84: CLA with C-MAC */
        secure_apdu[1] = SCP03_INS_EXT_AUTH; /* 0x82: EXTERNAL AUTHENTICATE */
        secure_apdu[2] = 0x00;               /* P1 */
        secure_apdu[3] = 0x00;               /* P2 */
        secure_apdu[4] = SCP03_CRYPTOGRAM_SIZE; /* Lc = 8 */
        (void)memcpy(&secure_apdu[5], session->host_cryptogram, SCP03_CRYPTOGRAM_SIZE);

        /* Compute C-MAC over the APDU body (5 + 8 = 13 bytes) */
        scp03_aes_cmac(session->s_mac, secure_apdu, 5 + SCP03_CRYPTOGRAM_SIZE, mac_full);

        /* Append truncated C-MAC (first 8 bytes) */
        secure_apdu_len = 5 + SCP03_CRYPTOGRAM_SIZE;
        for (mi = 0; mi < SCP03_CMAC_TRUNC; mi++)
        {
            secure_apdu[secure_apdu_len + mi] = mac_full[mi];
        }
        secure_apdu_len += SCP03_CMAC_TRUNC;

        /* Write secured APDU */
        ret = scp03_i2c_write(i2c_addr, secure_apdu, secure_apdu_len);
        if (ret != 0)
        {
            se050_scp03_close_session(session);
            return SCP03_ERR_HW;
        }

        se050_scp03_secure_zero(mac_full, sizeof(mac_full));

        /* Read response */
        {
            uint8_t read_buffer[4]; /* SW1 SW2 only + possible data */
            ret = scp03_i2c_read(i2c_addr, read_buffer, 4);
            if (ret != 0)
            {
                se050_scp03_close_session(session);
                return SCP03_ERR_HW;
            }

            /* Verify R-MAC will be checked if present; for now just check SW */
            {
                uint16_t sw = ((uint16_t)read_buffer[0] << 8) | (uint16_t)read_buffer[1];
                if (sw != SCP03_SW_OK)
                {
                    se050_scp03_close_session(session);
                    return SCP03_ERR_SECURITY;
                }
            }
        }

        se050_scp03_secure_zero(secure_apdu, secure_apdu_len);
    }

    /* Initialize C-MAC IV for subsequent commands */
    {
        /* Initial C-MAC IV = AES-ECB(S-MAC, 0x00000000000000000000000000000000) */
        scp03_aes128_ctx_t mac_ctx;
        uint8_t zero_block[16] = {0};

        scp03_aes128_key_expand(&mac_ctx, session->s_mac);
        scp03_aes128_encrypt(&mac_ctx, zero_block, session->cmac_iv);

        /* R-MAC IV starts as zero (resets with each session) */
        (void)memset(session->rmac_iv, 0, SCP03_BLOCK_SIZE);

        se050_scp03_secure_zero(&mac_ctx, sizeof(mac_ctx));
        se050_scp03_secure_zero(zero_block, sizeof(zero_block));
    }

    session->state         = SCP03_STATE_OPEN;
    session->scp_cmd_count = 1;

    return SCP03_OK;
}

void se050_scp03_close_session(scp03_session_t *session)
{
    if (session == NULL)
    {
        return;
    }

    /* Send RESET command to SE050 to clear secure channel */
    /* For production, consider sending a CLOSE CHANNEL APDU */
    {
        uint8_t reset_apdu[] = {0x80, 0x50, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00};
        uint8_t resp[4];
        uint16_t rlen = sizeof(resp);

        /*
         * Attempt RESET (INIT-UPDATE with zero challenge = soft reset).
         * This is best-effort; ignore failure.
         */
        (void)i2c_transfer(SE050_I2C_ADDR, SE050_I2C_ADDR,
                           reset_apdu, sizeof(reset_apdu), NULL, 0);
        (void)i2c_transfer(SE050_I2C_ADDR, SE050_I2C_ADDR | 0x01U,
                           NULL, 0, resp, rlen);
    }

    /* Securely zero all key material and session state */
    se050_scp03_secure_zero(session->s_enc, sizeof(session->s_enc));
    se050_scp03_secure_zero(session->s_mac, sizeof(session->s_mac));
    se050_scp03_secure_zero(session->s_rmac, sizeof(session->s_rmac));
    se050_scp03_secure_zero(session->cmac_iv, sizeof(session->cmac_iv));
    se050_scp03_secure_zero(session->rmac_iv, sizeof(session->rmac_iv));
    se050_scp03_secure_zero(session->host_challenge, sizeof(session->host_challenge));
    se050_scp03_secure_zero(session->card_challenge, sizeof(session->card_challenge));
    se050_scp03_secure_zero(session->host_cryptogram, sizeof(session->host_cryptogram));
    se050_scp03_secure_zero(session->card_cryptogram, sizeof(session->card_cryptogram));

    session->state           = SCP03_STATE_INIT;
    session->scp_cmd_count   = 0;
}

bool se050_scp03_is_open(const scp03_session_t *session)
{
    if (session == NULL)
    {
        return false;
    }

    return (session->state == SCP03_STATE_OPEN);
}

/* ---- Secured APDU Communication ---- */

int se050_scp03_apdu(scp03_session_t *session, uint8_t i2c_addr,
                      uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                      const uint8_t *data, uint16_t data_len,
                      uint8_t *resp, uint16_t *resp_len)
{
    int ret;
    uint8_t apdu_header[4];
    uint8_t cmd_buffer[5 + SCP03_MAX_APDU_DATA + SCP03_CMAC_TRUNC]; /* header + data + mac */
    uint16_t cmd_len;
    uint8_t mac_input[5 + SCP03_MAX_APDU_DATA + SCP03_CMAC_SIZE + 1]; /* for CMAC computation */
    uint16_t mac_input_len;
    uint8_t mac_full[SCP03_BLOCK_SIZE]; /* 16-byte CMAC full output */
    uint16_t i;
    uint16_t resp_data_len;
    uint16_t sw;

    if (session == NULL || resp == NULL || resp_len == NULL)
    {
        return SCP03_ERR_NULL;
    }

    if (session->state != SCP03_STATE_OPEN)
    {
        return SCP03_ERR_CHANNEL;
    }

    if (data_len > SCP03_MAX_APDU_DATA)
    {
        return SCP03_ERR_PARAM;
    }

    /* ----------------------------------------------------------------
     * Build C-MAC for the command
     *
     * C-MAC input (for CMAC computation):
     *   CLA (with 0x04 C-MAC flag) || INS || P1 || P2 || Lc ||
     *   data || [padding for CMAC]
     *
     * The CLA byte must include the C-MAC flag: cla | 0x04
     * For encrypted communication, also set encryption flag: cla | 0x0C
     *
     * We use C-MAC only (no encryption for simplicity — encryption
     * at application level via sec_encrypt/decrypt is sufficient).
     *
     * The MAC chaining uses:
     *   C-MAC = AES-CMAC(S-MAC, previous_cmac || new_apdu_body)
     *
     * For the first command, previous_cmac = cmac_iv (from init).
     * ---------------------------------------------------------------- */

    /* Build APDU body for MAC computation */
    apdu_header[0] = (uint8_t)(cla | 0x04U); /* CLA with C-MAC bit */
    apdu_header[1] = ins;
    apdu_header[2] = p1;
    apdu_header[3] = p2;

    /* Build MAC input: previous CMAC || header || Lc || data */
    mac_input_len = 0;

    /* Chain previous C-MAC (or IV for first command) */
    (void)memcpy(mac_input, session->cmac_iv, SCP03_BLOCK_SIZE);
    mac_input_len += SCP03_BLOCK_SIZE;

    /* Append APDU header */
    (void)memcpy(mac_input + mac_input_len, apdu_header, 4);
    mac_input_len += 4;

    /* Append Lc if data present */
    if (data_len > 0)
    {
        mac_input[mac_input_len] = (uint8_t)data_len;
        mac_input_len += 1;

        (void)memcpy(mac_input + mac_input_len, data, data_len);
        mac_input_len += data_len;
    }

    /* Compute C-MAC over entire input */
    scp03_aes_cmac(session->s_mac, mac_input, (uint16_t)mac_input_len, mac_full);

    /* Update C-MAC IV for next command (full 16 bytes) */
    (void)memcpy(session->cmac_iv, mac_full, SCP03_BLOCK_SIZE);

    /* ----------------------------------------------------------------
     * Build and send the secured APDU
     * ---------------------------------------------------------------- */
    cmd_buffer[0] = apdu_header[0];
    cmd_buffer[1] = apdu_header[1];
    cmd_buffer[2] = apdu_header[2];
    cmd_buffer[3] = apdu_header[3];

    if (data_len > 0)
    {
        cmd_buffer[4] = (uint8_t)data_len;
        (void)memcpy(&cmd_buffer[5], data, data_len);
        cmd_len = 5 + data_len;
    }
    else
    {
        cmd_len = 4;
    }

    /* Append truncated C-MAC (first 8 bytes) */
    for (i = 0; i < SCP03_CMAC_TRUNC; i++)
    {
        cmd_buffer[cmd_len + i] = mac_full[i];
    }
    cmd_len += SCP03_CMAC_TRUNC;

    /* Write secured APDU */
    ret = scp03_i2c_write(i2c_addr, cmd_buffer, cmd_len);
    if (ret != 0)
    {
        return SCP03_ERR_HW;
    }

    se050_scp03_secure_zero(mac_full, sizeof(mac_full));

    /* Read response */
    {
        uint8_t read_buffer[SCP03_MAX_APDU_RESP + 2];
        uint16_t read_len;

        read_len = (*resp_len < SCP03_MAX_APDU_RESP) ? *resp_len : SCP03_MAX_APDU_RESP;

        ret = scp03_i2c_read(i2c_addr, read_buffer, read_len + 2);
        if (ret != 0)
        {
            return SCP03_ERR_HW;
        }

        /* Parse response: data + SW */
        {
            uint16_t total_len = read_len + 2;

            ret = scp03_parse_response(read_buffer, total_len, &resp_data_len, &sw);
            if (ret != 0)
            {
                return ret;
            }

            if (sw != SCP03_SW_OK)
            {
                return SCP03_ERR_SW;
            }

            if (resp_data_len > *resp_len)
            {
                resp_data_len = *resp_len;
            }

            (void)memcpy(resp, read_buffer, resp_data_len);
            *resp_len = resp_data_len;
        }
    }

    session->scp_cmd_count++;

    return SCP03_OK;
}

/* ---- Key Rotation ---- */

int se050_scp03_rotate_keys(scp03_session_t *session, uint8_t i2c_addr)
{
    int ret;

    if (session == NULL)
    {
        return SCP03_ERR_NULL;
    }

    if (session->state != SCP03_STATE_OPEN)
    {
        return SCP03_ERR_CHANNEL;
    }

    /*
     * Key rotation: close current session and re-establish with incremented
     * sequence counter. The SE050 maintains its own sequence counter;
     * re-opening will naturally advance it.
     */

    /* Step 1: Store a snapshot of static keys */
    uint8_t saved_enc[SCP03_KEY_SIZE];
    uint8_t saved_mac[SCP03_KEY_SIZE];
    uint8_t saved_rmac[SCP03_KEY_SIZE];

    (void)memcpy(saved_enc,  session->static_enc_key,  SCP03_KEY_SIZE);
    (void)memcpy(saved_mac,  session->static_mac_key,  SCP03_KEY_SIZE);
    (void)memcpy(saved_rmac, session->static_rmac_key, SCP03_KEY_SIZE);

    /* Step 2: Close existing session (securely zeroes session keys) */
    se050_scp03_close_session(session);

    /* Step 3: Restore static keys (close_session also zeroes static keys) */
    (void)memcpy(session->static_enc_key,  saved_enc,  SCP03_KEY_SIZE);
    (void)memcpy(session->static_mac_key,  saved_mac,  SCP03_KEY_SIZE);
    (void)memcpy(session->static_rmac_key, saved_rmac, SCP03_KEY_SIZE);

    se050_scp03_secure_zero(saved_enc,  sizeof(saved_enc));
    se050_scp03_secure_zero(saved_mac,  sizeof(saved_mac));
    se050_scp03_secure_zero(saved_rmac, sizeof(saved_rmac));

    /* Step 4: Re-establish session */
    session->state = SCP03_STATE_INIT;
    ret = se050_scp03_open_session(session, i2c_addr);

    return ret;
}

/* ========================================================================
 *  End of se050_scp03.c
 * ======================================================================== */
