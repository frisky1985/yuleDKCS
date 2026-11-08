/**
 * @file vse050_sim.c
 * @brief Virtual SE050: asynchronous T=1' card + SCP03 card-side model.
 *
 * See vse050_sim.h for scope and limitations.
 */

#include "vse050_sim.h"
#include "se050_apdu.h"

#include <string.h>

#define VSE050_MAX_CMD     1024U
#define VSE050_MAX_RESP     512U
#define VSE050_QUEUE_DEPTH    2U

/** Only key object 0x02 (device key) is provisioned in the model. */
#define VSE050_DEVICE_KEY_ID 0x02U

/** PUT KEY payload: K-ENC || K-MAC || K-RMAC (reduced GP form). */
#define VSE050_PUT_KEY_DATA_SIZE (3U * 16U)

/* ========================================================================
 *  Internal: AES-128 (FIPS 197) — encryption only, standalone
 * ======================================================================== */

static const uint8_t s_vs_sbox[256] = {
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

static const uint8_t s_vs_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36
};

typedef struct {
    uint32_t rk[44];
} vs_aes_ctx_t;

static uint32_t vs_load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static uint8_t vs_gf_mul2(uint8_t x)
{
    return (uint8_t)((uint8_t)(x << 1) ^ ((x & 0x80U) ? 0x1BU : 0x00U));
}

static uint32_t vs_sub_word(uint32_t w)
{
    return ((uint32_t)s_vs_sbox[(w >> 24) & 0xFFU] << 24) |
           ((uint32_t)s_vs_sbox[(w >> 16) & 0xFFU] << 16) |
           ((uint32_t)s_vs_sbox[(w >>  8) & 0xFFU] <<  8) |
           ((uint32_t)s_vs_sbox[ w        & 0xFFU]);
}

static void vs_aes_expand(vs_aes_ctx_t *ctx, const uint8_t key[16])
{
    int i;

    for (i = 0; i < 4; i++) {
        ctx->rk[i] = vs_load_be32(&key[(uint32_t)i * 4U]);
    }
    for (i = 4; i < 44; i++) {
        uint32_t temp = ctx->rk[i - 1];
        if ((i % 4) == 0) {
            temp = vs_sub_word((temp << 8) | (temp >> 24)) ^
                   ((uint32_t)s_vs_rcon[(uint32_t)i / 4U] << 24);
        }
        ctx->rk[i] = ctx->rk[i - 4] ^ temp;
    }
}

static void vs_aes_encrypt(const vs_aes_ctx_t *ctx,
                           const uint8_t in[16], uint8_t out[16])
{
    uint32_t s[4];
    uint8_t  tmp[16];
    int round;

    s[0] = vs_load_be32(in)      ^ ctx->rk[0];
    s[1] = vs_load_be32(in + 4)  ^ ctx->rk[1];
    s[2] = vs_load_be32(in + 8)  ^ ctx->rk[2];
    s[3] = vs_load_be32(in + 12) ^ ctx->rk[3];

    for (round = 1; round < 10; round++) {
        uint8_t t[16];
        uint8_t sr[16];
        uint32_t rk_off = (uint32_t)round * 4U;
        int i, c;

        for (i = 0; i < 4; i++) {
            t[0 + (uint32_t)i * 4U] = s_vs_sbox[(s[i] >> 24) & 0xFFU];
            t[1 + (uint32_t)i * 4U] = s_vs_sbox[(s[i] >> 16) & 0xFFU];
            t[2 + (uint32_t)i * 4U] = s_vs_sbox[(s[i] >>  8) & 0xFFU];
            t[3 + (uint32_t)i * 4U] = s_vs_sbox[ s[i]        & 0xFFU];
        }

        sr[0]  = t[0];  sr[4]  = t[4];  sr[8]  = t[8];  sr[12] = t[12];
        sr[1]  = t[5];  sr[5]  = t[9];  sr[9]  = t[13]; sr[13] = t[1];
        sr[2]  = t[10]; sr[6]  = t[14]; sr[10] = t[2];  sr[14] = t[6];
        sr[3]  = t[15]; sr[7]  = t[3];  sr[11] = t[7];  sr[15] = t[11];

        for (c = 0; c < 4; c++) {
            uint8_t a0 = sr[0 + (uint32_t)c * 4U];
            uint8_t a1 = sr[1 + (uint32_t)c * 4U];
            uint8_t a2 = sr[2 + (uint32_t)c * 4U];
            uint8_t a3 = sr[3 + (uint32_t)c * 4U];

            tmp[0 + (uint32_t)c * 4U] = vs_gf_mul2(a0) ^ (vs_gf_mul2(a1) ^ a1) ^ a2 ^ a3;
            tmp[1 + (uint32_t)c * 4U] = a0 ^ vs_gf_mul2(a1) ^ (vs_gf_mul2(a2) ^ a2) ^ a3;
            tmp[2 + (uint32_t)c * 4U] = a0 ^ a1 ^ vs_gf_mul2(a2) ^ (vs_gf_mul2(a3) ^ a3);
            tmp[3 + (uint32_t)c * 4U] = (vs_gf_mul2(a0) ^ a0) ^ a1 ^ a2 ^ vs_gf_mul2(a3);
        }

        s[0] = vs_load_be32(tmp)      ^ ctx->rk[rk_off];
        s[1] = vs_load_be32(tmp + 4)  ^ ctx->rk[rk_off + 1U];
        s[2] = vs_load_be32(tmp + 8)  ^ ctx->rk[rk_off + 2U];
        s[3] = vs_load_be32(tmp + 12) ^ ctx->rk[rk_off + 3U];
    }

    {
        uint8_t t[16];
        uint32_t rk_off = 40U;
        int i;

        for (i = 0; i < 4; i++) {
            t[0 + (uint32_t)i * 4U] = s_vs_sbox[(s[i] >> 24) & 0xFFU];
            t[1 + (uint32_t)i * 4U] = s_vs_sbox[(s[i] >> 16) & 0xFFU];
            t[2 + (uint32_t)i * 4U] = s_vs_sbox[(s[i] >>  8) & 0xFFU];
            t[3 + (uint32_t)i * 4U] = s_vs_sbox[ s[i]        & 0xFFU];
        }

        out[0]  = t[0];  out[4]  = t[4];  out[8]  = t[8];  out[12] = t[12];
        out[1]  = t[5];  out[5]  = t[9];  out[9]  = t[13]; out[13] = t[1];
        out[2]  = t[10]; out[6]  = t[14]; out[10] = t[2];  out[14] = t[6];
        out[3]  = t[15]; out[7]  = t[3];  out[11] = t[7];  out[15] = t[11];

        s[0] = vs_load_be32(out)      ^ ctx->rk[rk_off];
        s[1] = vs_load_be32(out + 4)  ^ ctx->rk[rk_off + 1U];
        s[2] = vs_load_be32(out + 8)  ^ ctx->rk[rk_off + 2U];
        s[3] = vs_load_be32(out + 12) ^ ctx->rk[rk_off + 3U];

        out[0]  = (uint8_t)(s[0] >> 24); out[1]  = (uint8_t)(s[0] >> 16);
        out[2]  = (uint8_t)(s[0] >> 8);  out[3]  = (uint8_t)(s[0]);
        out[4]  = (uint8_t)(s[1] >> 24); out[5]  = (uint8_t)(s[1] >> 16);
        out[6]  = (uint8_t)(s[1] >> 8);  out[7]  = (uint8_t)(s[1]);
        out[8]  = (uint8_t)(s[2] >> 24); out[9]  = (uint8_t)(s[2] >> 16);
        out[10] = (uint8_t)(s[2] >> 8);  out[11] = (uint8_t)(s[2]);
        out[12] = (uint8_t)(s[3] >> 24); out[13] = (uint8_t)(s[3] >> 16);
        out[14] = (uint8_t)(s[3] >> 8);  out[15] = (uint8_t)(s[3]);
    }
}

void vse050_sim_aes128_encrypt_block(const uint8_t key[16],
                                     const uint8_t in[16], uint8_t out[16])
{
    vs_aes_ctx_t ctx;

    if (key == NULL || in == NULL || out == NULL) return;
    vs_aes_expand(&ctx, key);
    vs_aes_encrypt(&ctx, in, out);
    (void)memset(&ctx, 0, sizeof(ctx));
}

/* ---- AES-128 decryption (card side SCP03 data encryption) ---- */

static const uint8_t s_vs_inv_sbox[256] = {
    0x52,0x09,0x6A,0xD5,0x30,0x36,0xA5,0x38,0xBF,0x40,0xA3,0x9E,0x81,0xF3,0xD7,0xFB,
    0x7C,0xE3,0x39,0x82,0x9B,0x2F,0xFF,0x87,0x34,0x8E,0x43,0x44,0xC4,0xDE,0xE9,0xCB,
    0x54,0x7B,0x94,0x32,0xA6,0xC2,0x23,0x3D,0xEE,0x4C,0x95,0x0B,0x42,0xFA,0xC3,0x4E,
    0x08,0x2E,0xA1,0x66,0x28,0xD9,0x24,0xB2,0x76,0x5B,0xA2,0x49,0x6D,0x8B,0xD1,0x25,
    0x72,0xF8,0xF6,0x64,0x86,0x68,0x98,0x16,0xD4,0xA4,0x5C,0xCC,0x5D,0x65,0xB6,0x92,
    0x6C,0x70,0x48,0x50,0xFD,0xED,0xB9,0xDA,0x5E,0x15,0x46,0x57,0xA7,0x8D,0x9D,0x84,
    0x90,0xD8,0xAB,0x00,0x8C,0xBC,0xD3,0x0A,0xF7,0xE4,0x58,0x05,0xB8,0xB3,0x45,0x06,
    0xD0,0x2C,0x1E,0x8F,0xCA,0x3F,0x0F,0x02,0xC1,0xAF,0xBD,0x03,0x01,0x13,0x8A,0x6B,
    0x3A,0x91,0x11,0x41,0x4F,0x67,0xDC,0xEA,0x97,0xF2,0xCF,0xCE,0xF0,0xB4,0xE6,0x73,
    0x96,0xAC,0x74,0x22,0xE7,0xAD,0x35,0x85,0xE2,0xF9,0x37,0xE8,0x1C,0x75,0xDF,0x6E,
    0x47,0xF1,0x1A,0x71,0x1D,0x29,0xC5,0x89,0x6F,0xB7,0x62,0x0E,0xAA,0x18,0xBE,0x1B,
    0xFC,0x56,0x3E,0x4B,0xC6,0xD2,0x79,0x20,0x9A,0xDB,0xC0,0xFE,0x78,0xCD,0x5A,0xF4,
    0x1F,0xDD,0xA8,0x33,0x88,0x07,0xC7,0x31,0xB1,0x12,0x10,0x59,0x27,0x80,0xEC,0x5F,
    0x60,0x51,0x7F,0xA9,0x19,0xB5,0x4A,0x0D,0x2D,0xE5,0x7A,0x9F,0x93,0xC9,0x9C,0xEF,
    0xA0,0xE0,0x3B,0x4D,0xAE,0x2A,0xF5,0xB0,0xC8,0xEB,0xBB,0x3C,0x83,0x53,0x99,0x61,
    0x17,0x2B,0x04,0x7E,0xBA,0x77,0xD6,0x26,0xE1,0x69,0x14,0x63,0x55,0x21,0x0C,0x7D
};

static uint8_t vs_gf_mul(uint8_t a, uint8_t b)
{
    uint8_t p = 0U;
    uint8_t i;

    for (i = 0; i < 8U; i++) {
        if ((b & 0x01U) != 0U) p ^= a;
        a = (uint8_t)(((a & 0x80U) != 0U) ? (uint8_t)((uint8_t)(a << 1) ^ 0x1BU)
                                          : (uint8_t)(a << 1));
        b >>= 1;
    }
    return p;
}

static void vs_add_round_key(uint8_t st[16], const uint32_t *rk, uint32_t round)
{
    uint32_t c;

    for (c = 0; c < 4U; c++) {
        uint32_t w = rk[round * 4U + c];

        st[c * 4U + 0U] ^= (uint8_t)(w >> 24);
        st[c * 4U + 1U] ^= (uint8_t)(w >> 16);
        st[c * 4U + 2U] ^= (uint8_t)(w >> 8);
        st[c * 4U + 3U] ^= (uint8_t)(w);
    }
}

static void vs_inv_shift_rows(const uint8_t in[16], uint8_t out[16])
{
    out[0]  = in[0];  out[4]  = in[4];  out[8]  = in[8];  out[12] = in[12];
    out[1]  = in[13]; out[5]  = in[1];  out[9]  = in[5];  out[13] = in[9];
    out[2]  = in[10]; out[6]  = in[14]; out[10] = in[2];  out[14] = in[6];
    out[3]  = in[7];  out[7]  = in[11]; out[11] = in[15]; out[15] = in[3];
}

static void vs_aes_decrypt(const vs_aes_ctx_t *ctx,
                           const uint8_t in[16], uint8_t out[16])
{
    uint8_t st[16];
    uint8_t tmp[16];
    uint32_t round;
    uint32_t c;
    uint32_t i;

    (void)memcpy(st, in, 16U);
    vs_add_round_key(st, ctx->rk, 10U);

    for (round = 9U; round >= 1U; round--) {
        vs_inv_shift_rows(st, tmp);
        for (i = 0; i < 16U; i++) tmp[i] = s_vs_inv_sbox[tmp[i]];
        vs_add_round_key(tmp, ctx->rk, round);

        for (c = 0; c < 4U; c++) {
            uint8_t a0 = tmp[c * 4U + 0U];
            uint8_t a1 = tmp[c * 4U + 1U];
            uint8_t a2 = tmp[c * 4U + 2U];
            uint8_t a3 = tmp[c * 4U + 3U];

            st[c * 4U + 0U] = (uint8_t)(vs_gf_mul(a0, 0x0EU) ^ vs_gf_mul(a1, 0x0BU) ^
                                        vs_gf_mul(a2, 0x0DU) ^ vs_gf_mul(a3, 0x09U));
            st[c * 4U + 1U] = (uint8_t)(vs_gf_mul(a0, 0x09U) ^ vs_gf_mul(a1, 0x0EU) ^
                                        vs_gf_mul(a2, 0x0BU) ^ vs_gf_mul(a3, 0x0DU));
            st[c * 4U + 2U] = (uint8_t)(vs_gf_mul(a0, 0x0DU) ^ vs_gf_mul(a1, 0x09U) ^
                                        vs_gf_mul(a2, 0x0EU) ^ vs_gf_mul(a3, 0x0BU));
            st[c * 4U + 3U] = (uint8_t)(vs_gf_mul(a0, 0x0BU) ^ vs_gf_mul(a1, 0x0DU) ^
                                        vs_gf_mul(a2, 0x09U) ^ vs_gf_mul(a3, 0x0EU));
        }
    }

    vs_inv_shift_rows(st, tmp);
    for (i = 0; i < 16U; i++) tmp[i] = s_vs_inv_sbox[tmp[i]];
    vs_add_round_key(tmp, ctx->rk, 0U);

    (void)memcpy(out, tmp, 16U);
    (void)memset(st, 0, sizeof(st));
    (void)memset(tmp, 0, sizeof(tmp));
}

void vse050_sim_aes128_decrypt_block(const uint8_t key[16],
                                     const uint8_t in[16], uint8_t out[16])
{
    vs_aes_ctx_t ctx;

    if (key == NULL || in == NULL || out == NULL) return;
    vs_aes_expand(&ctx, key);
    vs_aes_decrypt(&ctx, in, out);
    (void)memset(&ctx, 0, sizeof(ctx));
}

static uint16_t vs_padded_len(uint16_t len)
{
    return (uint16_t)(((uint16_t)(len / 16U) + 1U) * 16U);
}

static void vs_pad(const uint8_t *in, uint16_t len, uint8_t *out, uint16_t padded)
{
    (void)memcpy(out, in, len);
    out[len] = 0x80U;
    (void)memset(&out[(uint16_t)(len + 1U)], 0, (uint16_t)(padded - len - 1U));
}

/* @return unpadded length, or -1 on malformed padding */
static int vs_unpad(const uint8_t *buf, uint16_t padded)
{
    uint16_t n = padded;

    while ((n > 0U) && (buf[n - 1U] == 0x00U)) n--;
    if ((n == 0U) || (buf[n - 1U] != 0x80U)) return -1;
    return (int)(n - 1U);
}

static void vs_cbc_encrypt(const uint8_t key[16], const uint8_t iv[16],
                           const uint8_t *in, uint8_t *out, uint16_t len)
{
    vs_aes_ctx_t ctx;
    uint8_t prev[16];
    uint16_t off;

    vs_aes_expand(&ctx, key);
    (void)memcpy(prev, iv, 16U);

    for (off = 0; off < len; off = (uint16_t)(off + 16U)) {
        uint8_t block[16];
        uint8_t j;

        for (j = 0; j < 16U; j++) block[j] = (uint8_t)(in[off + j] ^ prev[j]);
        vs_aes_encrypt(&ctx, block, &out[off]);
        (void)memcpy(prev, &out[off], 16U);
        (void)memset(block, 0, sizeof(block));
    }

    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(prev, 0, sizeof(prev));
}

static void vs_cbc_decrypt(const uint8_t key[16], const uint8_t iv[16],
                           const uint8_t *in, uint8_t *out, uint16_t len)
{
    vs_aes_ctx_t ctx;
    uint8_t prev[16];
    uint8_t cur[16];
    uint16_t off;

    vs_aes_expand(&ctx, key);
    (void)memcpy(prev, iv, 16U);

    for (off = 0; off < len; off = (uint16_t)(off + 16U)) {
        uint8_t j;

        (void)memcpy(cur, &in[off], 16U);
        vs_aes_decrypt(&ctx, cur, &out[off]);
        for (j = 0; j < 16U; j++) out[off + j] ^= prev[j];
        (void)memcpy(prev, cur, 16U);
    }

    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(prev, 0, sizeof(prev));
    (void)memset(cur, 0, sizeof(cur));
}

static void vs_cmac_subkeys(const uint8_t key[16], uint8_t k1[16], uint8_t k2[16])
{
    vs_aes_ctx_t ctx;
    uint8_t zero[16];
    uint8_t l[16];
    uint8_t carry;
    int i;

    (void)memset(zero, 0, sizeof(zero));
    vs_aes_expand(&ctx, key);
    vs_aes_encrypt(&ctx, zero, l);

    carry = (uint8_t)((l[0] & 0x80U) ? 1U : 0U);
    for (i = 0; i < 15; i++) {
        k1[i] = (uint8_t)((uint8_t)(l[i] << 1) | (uint8_t)(l[i + 1] >> 7));
    }
    k1[15] = (uint8_t)(l[15] << 1);
    if (carry != 0U) k1[15] ^= 0x87U;

    carry = (uint8_t)((k1[0] & 0x80U) ? 1U : 0U);
    for (i = 0; i < 15; i++) {
        k2[i] = (uint8_t)((uint8_t)(k1[i] << 1) | (uint8_t)(k1[i + 1] >> 7));
    }
    k2[15] = (uint8_t)(k1[15] << 1);
    if (carry != 0U) k2[15] ^= 0x87U;

    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(zero, 0, sizeof(zero));
    (void)memset(l, 0, sizeof(l));
}

void vse050_sim_aes_cmac(const uint8_t key[16],
                         const uint8_t *msg, uint16_t msg_len,
                         uint8_t mac[16])
{
    vs_aes_ctx_t ctx;
    uint8_t k1[16], k2[16];
    uint8_t cb[16];
    uint8_t n;
    uint8_t last_complete;
    uint16_t offset = 0;
    uint8_t i, j;

    if (key == NULL || mac == NULL || (msg == NULL && msg_len > 0U)) return;

    vs_cmac_subkeys(key, k1, k2);
    vs_aes_expand(&ctx, key);

    n = (msg_len == 0U) ? 1U : (uint8_t)(((uint32_t)msg_len + 15U) / 16U);
    last_complete = (uint8_t)((msg_len % 16U == 0U) ? 1U : 0U);

    (void)memset(cb, 0, sizeof(cb));

    for (i = 0; i < (uint8_t)(n - 1U); i++) {
        uint8_t block[16];
        offset = (uint16_t)i * 16U;
        for (j = 0; j < 16U; j++) {
            block[j] = (uint8_t)(msg[(uint32_t)offset + j] ^ cb[j]);
        }
        vs_aes_encrypt(&ctx, block, cb);
    }

    {
        uint8_t last_block[16];
        uint8_t *subkey = (last_complete != 0U) ? k1 : k2;

        offset = (uint16_t)(n - 1U) * 16U;
        for (j = 0; j < 16U; j++) {
            uint32_t pos = (uint32_t)offset + j;
            if (pos < (uint32_t)msg_len) {
                last_block[j] = msg[pos];
            } else if (pos == (uint32_t)msg_len) {
                last_block[j] = 0x80U;
            } else {
                last_block[j] = 0x00U;
            }
        }
        for (j = 0; j < 16U; j++) {
            last_block[j] ^= (uint8_t)(cb[j] ^ subkey[j]);
        }
        vs_aes_encrypt(&ctx, last_block, mac);
    }

    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(k1, 0, sizeof(k1));
    (void)memset(k2, 0, sizeof(k2));
    (void)memset(cb, 0, sizeof(cb));
}

/* ========================================================================
 *  Device model
 * ======================================================================== */

typedef struct {
    uint8_t  data[SE050_T1_MAX_FRAME];
    uint16_t len;
    uint16_t pos;
    uint32_t ready_at;
} vs_frame_t;

typedef struct {
    vse050_config_t cfg;
    vse050_state_e  state;

    uint32_t now_us;
    uint32_t busy_polls;
    uint32_t nack_polls;
    uint32_t wtx_count;
    uint32_t crc_errors;
    uint32_t auth_failures;
    uint16_t apdu_count;

    /* Command reassembly */
    uint8_t  cmd[VSE050_MAX_CMD];
    uint16_t cmd_len;
    uint8_t  last_cmd_frame[SE050_T1_MAX_FRAME];
    uint16_t last_cmd_len;

    /* Response being transmitted */
    uint8_t  resp[VSE050_MAX_RESP];
    uint16_t resp_len;
    uint16_t resp_off;

    /* Last transmitted block, for host retransmission */
    uint8_t  last_resp_frame[SE050_T1_MAX_FRAME];
    uint16_t last_resp_len;
    uint16_t last_chunk_end;
    uint8_t  have_last_resp;

    /* TX queue */
    vs_frame_t fifo[VSE050_QUEUE_DEPTH];
    uint8_t    fifo_head;
    uint8_t    fifo_count;

    /* SCP03 card-side state */
    uint8_t  host_challenge[8];
    uint8_t  card_challenge[8];
    uint8_t  s_enc[16];
    uint8_t  s_mac[16];
    uint8_t  s_rmac[16];
    uint8_t  cmac_iv[16];
    uint8_t  rmac_iv[16];
    uint8_t  cmd_icv[16];
    uint8_t  resp_icv[16];
    uint32_t sign_count;
    uint32_t put_key_count;
    uint32_t encrypted_cmds;
    uint16_t last_cipher_len;
    uint8_t  wtx_pending;
} vs_dev_t;

static vs_dev_t s_dev;
static int      s_ready;

/* ── Frame queue ──────────────────────────────────────────── */
static void vs_fifo_reset(void)
{
    s_dev.fifo_head  = 0;
    s_dev.fifo_count = 0;
    (void)memset(s_dev.fifo, 0, sizeof(s_dev.fifo));
}

static int vs_fifo_empty(void)
{
    return (s_dev.fifo_count == 0U) ? 1 : 0;
}

static vs_frame_t *vs_fifo_head(void)
{
    if (s_dev.fifo_count == 0U) return NULL;
    return &s_dev.fifo[s_dev.fifo_head];
}

static void vs_fifo_pop(void)
{
    if (s_dev.fifo_count == 0U) return;
    s_dev.fifo_head = (uint8_t)((s_dev.fifo_head + 1U) % VSE050_QUEUE_DEPTH);
    s_dev.fifo_count--;
}

static int vs_fifo_push(const uint8_t *frame, uint16_t len, uint32_t ready_at)
{
    uint8_t idx;

    if (frame == NULL || len == 0U || len > SE050_T1_MAX_FRAME) return -1;
    if (s_dev.fifo_count >= VSE050_QUEUE_DEPTH) return -1;

    idx = (uint8_t)((s_dev.fifo_head + s_dev.fifo_count) % VSE050_QUEUE_DEPTH);
    (void)memcpy(s_dev.fifo[idx].data, frame, len);
    s_dev.fifo[idx].len      = len;
    s_dev.fifo[idx].pos      = 0;
    s_dev.fifo[idx].ready_at = ready_at;
    s_dev.fifo_count++;
    return 0;
}

/* ── SCP03 card side ──────────────────────────────────────── */
static void vs_derive_key(const uint8_t static_key[16], uint8_t ctr,
                          uint8_t out[16])
{
    vs_aes_ctx_t ctx;
    uint8_t d[16];

    d[0] = 0x01U;
    d[1] = ctr;
    (void)memset(&d[2], 0, 6U);
    d[8] = s_dev.cfg.seq_counter[0];
    d[9] = s_dev.cfg.seq_counter[1];
    d[10] = 0x80U;
    (void)memset(&d[11], 0, 5U);

    vs_aes_expand(&ctx, static_key);
    vs_aes_encrypt(&ctx, d, out);

    (void)memset(&ctx, 0, sizeof(ctx));
    (void)memset(d, 0, sizeof(d));
}

static void vs_derive_session_keys(void)
{
    vs_derive_key(s_dev.cfg.enc_key,  0x01U, s_dev.s_enc);
    vs_derive_key(s_dev.cfg.mac_key,  0x02U, s_dev.s_mac);
    vs_derive_key(s_dev.cfg.rmac_key, 0x03U, s_dev.s_rmac);
}

static void vs_cryptogram(const uint8_t first[8], const uint8_t second[8],
                          uint8_t out[8])
{
    uint8_t input[18];
    uint8_t mac[16];

    input[0] = 0x01U;
    input[1] = 0x01U;
    (void)memcpy(&input[2], first, 8U);
    (void)memcpy(&input[10], second, 8U);

    vse050_sim_aes_cmac(s_dev.s_mac, input, sizeof(input), mac);
    (void)memcpy(out, mac, 8U);

    (void)memset(input, 0, sizeof(input));
    (void)memset(mac, 0, sizeof(mac));
}

/* Append "90 00" (or an error SW) to the response. */
static void vs_set_sw(uint16_t sw)
{
    s_dev.resp[s_dev.resp_len]      = (uint8_t)(sw >> 8);
    s_dev.resp[s_dev.resp_len + 1U] = (uint8_t)(sw & 0xFFU);
    s_dev.resp_len = (uint16_t)(s_dev.resp_len + 2U);
}

static void vs_handle_init_update(void)
{
    uint8_t card_crypto[8];

    if (s_dev.cmd_len < 13U || s_dev.cmd[4] != 8U) {
        vs_set_sw(0x6700U);
        return;
    }

    (void)memcpy(s_dev.host_challenge, &s_dev.cmd[5], 8U);

    /*
     * Deterministic card challenge derived from the configured base and the
     * APDU counter, so successive sessions are distinguishable but replayable.
     */
    {
        uint8_t i;
        for (i = 0; i < 8U; i++) {
            s_dev.card_challenge[i] =
                (uint8_t)(s_dev.cfg.card_challenge[i] + (uint8_t)s_dev.apdu_count);
        }
    }

    s_dev.state = VSE050_STATE_AUTH_PENDING;
    vs_derive_session_keys();
    vs_cryptogram(s_dev.card_challenge, s_dev.host_challenge, card_crypto);

    (void)memcpy(s_dev.resp, s_dev.cfg.key_divers_data, 10U);
    s_dev.resp[10] = s_dev.cfg.key_version;
    s_dev.resp[11] = s_dev.cfg.seq_counter[0];
    s_dev.resp[12] = s_dev.cfg.seq_counter[1];
    (void)memcpy(&s_dev.resp[13], s_dev.card_challenge, 8U);
    (void)memcpy(&s_dev.resp[21], card_crypto, 8U);
    s_dev.resp_len = 29U;
    vs_set_sw(0x9000U);

    (void)memset(card_crypto, 0, sizeof(card_crypto));
}

static void vs_handle_ext_auth(void)
{
    uint8_t mac[16];
    uint8_t host_crypto[8];

    if (s_dev.state != VSE050_STATE_AUTH_PENDING) {
        vs_set_sw(0x6985U);
        return;
    }
    if (s_dev.cmd_len < 21U || s_dev.cmd[4] != 8U) {
        vs_set_sw(0x6700U);
        return;
    }
    if (s_dev.cfg.reject_ext_auth != 0U) {
        s_dev.auth_failures++;
        s_dev.state = VSE050_STATE_RESET;
        vs_set_sw(0x6300U);
        return;
    }

    /* C-MAC over CLA INS P1 P2 Lc || host cryptogram (13 bytes). */
    vse050_sim_aes_cmac(s_dev.s_mac, s_dev.cmd, 13U, mac);
    if (memcmp(mac, &s_dev.cmd[13], 8U) != 0) {
        s_dev.auth_failures++;
        s_dev.state = VSE050_STATE_RESET;
        vs_set_sw(0x6982U);
        (void)memset(mac, 0, sizeof(mac));
        return;
    }
    (void)memset(mac, 0, sizeof(mac));

    vs_cryptogram(s_dev.host_challenge, s_dev.card_challenge, host_crypto);
    if (memcmp(host_crypto, &s_dev.cmd[5], 8U) != 0) {
        s_dev.auth_failures++;
        s_dev.state = VSE050_STATE_RESET;
        vs_set_sw(0x6300U);
        (void)memset(host_crypto, 0, sizeof(host_crypto));
        return;
    }
    (void)memset(host_crypto, 0, sizeof(host_crypto));

    /* SCP03 chaining state: C-MAC IV = AES-ECB(S-MAC, 0^16), R-MAC IV = 0 */
    {
        vs_aes_ctx_t ctx;
        uint8_t zero_block[16];

        (void)memset(zero_block, 0, sizeof(zero_block));
        vs_aes_expand(&ctx, s_dev.s_mac);
        vs_aes_encrypt(&ctx, zero_block, s_dev.cmac_iv);
        (void)memset(s_dev.rmac_iv, 0, sizeof(s_dev.rmac_iv));
        (void)memset(s_dev.cmd_icv, 0, sizeof(s_dev.cmd_icv));
        (void)memset(s_dev.resp_icv, 0, sizeof(s_dev.resp_icv));

        (void)memset(&ctx, 0, sizeof(ctx));
        (void)memset(zero_block, 0, sizeof(zero_block));
    }

    s_dev.state = VSE050_STATE_OPEN;
    vs_set_sw(0x9000U);
}

/* Append the SCP03 R-MAC (8 bytes) to the current response payload. */
static void vs_append_rmac(void)
{
    uint8_t  input[16U + VSE050_MAX_RESP];
    uint8_t  mac[16];
    uint16_t len = s_dev.resp_len;

    if ((uint32_t)len + SCP03_CMAC_TRUNC > VSE050_MAX_RESP - 2U) return;

    (void)memcpy(input, s_dev.rmac_iv, 16U);
    (void)memcpy(&input[16], s_dev.resp, len);
    vse050_sim_aes_cmac(s_dev.s_rmac, input, (uint16_t)(16U + len), mac);
    (void)memcpy(s_dev.rmac_iv, mac, 16U);
    (void)memcpy(&s_dev.resp[len], mac, SCP03_CMAC_TRUNC);

    if (s_dev.cfg.corrupt_rmac != 0U) {
        s_dev.resp[len] ^= 0xFFU;   /* one-shot integrity fault */
        s_dev.cfg.corrupt_rmac = 0;
    }

    s_dev.resp_len = (uint16_t)(len + SCP03_CMAC_TRUNC);

    (void)memset(input, 0, sizeof(input));
    (void)memset(mac, 0, sizeof(mac));
}

static void vs_handle_sign(void)
{
    uint8_t  mac[16];
    uint8_t  input[16U + 64U + 16U];
    uint8_t  plain[VSE050_MAX_RESP];
    uint16_t body_len;
    uint16_t payload_len;
    int      plain_len;
    uint32_t key_id;
    uint8_t  diff;
    uint16_t i;
    uint8_t  b;
    uint8_t  encrypted = (uint8_t)((s_dev.cmd[0] & 0x08U) != 0U);

    if (s_dev.state != VSE050_STATE_OPEN) {
        vs_set_sw(0x6985U);
        return;
    }

    /* cmd = CLA INS P1 P2 Lc data... C-MAC(8), with Lc = data_len + 8 */
    if (s_dev.cmd_len < (uint16_t)(5U + SCP03_CMAC_TRUNC) ||
        s_dev.cmd[2] != 0x00U || s_dev.cmd[3] != 0x00U ||
        s_dev.cmd[4] != (uint8_t)(s_dev.cmd_len - 5U)) {
        vs_set_sw(0x6700U);
        return;
    }

    body_len    = (uint16_t)(s_dev.cmd_len - SCP03_CMAC_TRUNC);
    payload_len = (uint16_t)(body_len - 5U);

    if (s_dev.cfg.corrupt_ciphertext != 0U && payload_len > 0U) {
        s_dev.cmd[5] ^= 0xFFU;      /* one-shot: break C-MAC coverage */
        s_dev.cfg.corrupt_ciphertext = 0;
    }

    /* Verify the host C-MAC over the on-the-wire bytes. */
    (void)memcpy(input, s_dev.cmac_iv, 16U);
    (void)memcpy(&input[16], s_dev.cmd, body_len);
    vse050_sim_aes_cmac(s_dev.s_mac, input, (uint16_t)(16U + body_len), mac);

    diff = 0U;
    for (i = 0; i < SCP03_CMAC_TRUNC; i++) {
        diff |= (uint8_t)(mac[i] ^ s_dev.cmd[body_len + i]);
    }
    (void)memcpy(s_dev.cmac_iv, mac, 16U);
    (void)memset(input, 0, sizeof(input));

    if (diff != 0U) {
        (void)memset(mac, 0, sizeof(mac));
        s_dev.auth_failures++;
        s_dev.state = VSE050_STATE_RESET;
        vs_set_sw(0x6982U);
        return;
    }

    /* Response encryption ICV = full C-MAC of this command */
    (void)memcpy(s_dev.resp_icv, mac, 16U);
    (void)memset(mac, 0, sizeof(mac));

    if (encrypted != 0U) {
        if ((payload_len == 0U) || ((payload_len % 16U) != 0U)) {
            vs_set_sw(0x6700U);
            return;
        }

        vs_cbc_decrypt(s_dev.s_enc, s_dev.cmd_icv, &s_dev.cmd[5], plain, payload_len);
        plain_len = vs_unpad(plain, payload_len);

        if ((plain_len < 0) || (plain_len != (int)SE050_APDU_SIGN_INPUT_SIZE)) {
            (void)memset(plain, 0, sizeof(plain));
            vs_set_sw(0x6700U);
            return;
        }

        s_dev.encrypted_cmds++;
        s_dev.last_cipher_len = payload_len;
    } else {
        if (payload_len != SE050_APDU_SIGN_INPUT_SIZE) {
            vs_set_sw(0x6700U);
            return;
        }
        (void)memcpy(plain, &s_dev.cmd[5], payload_len);
    }

    if (s_dev.cfg.sign_disabled != 0U) {
        (void)memset(plain, 0, sizeof(plain));
        vs_set_sw(0x6982U);
        return;
    }

    key_id = ((uint32_t)plain[0] << 24) | ((uint32_t)plain[1] << 16) |
             ((uint32_t)plain[2] << 8) | (uint32_t)plain[3];
    if (key_id != VSE050_DEVICE_KEY_ID) {
        (void)memset(plain, 0, sizeof(plain));
        vs_set_sw(0x6A88U);
        return;
    }

    /*
     * Deterministic placeholder output: 4 x CMAC(S-ENC, block_idx || id || hash).
     * NOT a valid ECDSA signature — the model has no private-key arithmetic.
     * It exists so the host side can validate framing, C-MAC chaining, R-MAC
     * verification, CBC decryption and response parsing.
     */
    for (b = 0; b < 4U; b++) {
        uint8_t seed[1U + SE050_APDU_SIGN_INPUT_SIZE];
        seed[0] = b;
        (void)memcpy(&seed[1], plain, SE050_APDU_SIGN_INPUT_SIZE);
        vse050_sim_aes_cmac(s_dev.s_enc, seed, sizeof(seed),
                            &s_dev.resp[(uint16_t)b * 16U]);
        (void)memset(seed, 0, sizeof(seed));
    }
    (void)memset(plain, 0, sizeof(plain));

    if (encrypted != 0U) {
        uint8_t  padded[SE050_APDU_SIGNATURE_SIZE + 16U];
        uint8_t  cipher[SE050_APDU_SIGNATURE_SIZE + 16U];
        uint16_t padded_len = vs_padded_len(SE050_APDU_SIGNATURE_SIZE);

        vs_pad(s_dev.resp, SE050_APDU_SIGNATURE_SIZE, padded, padded_len);
        vs_cbc_encrypt(s_dev.s_enc, s_dev.resp_icv, padded, cipher, padded_len);
        (void)memcpy(s_dev.resp, cipher, padded_len);
        s_dev.resp_len = padded_len;

        (void)memset(padded, 0, sizeof(padded));
        (void)memset(cipher, 0, sizeof(cipher));
    } else {
        s_dev.resp_len = SE050_APDU_SIGNATURE_SIZE;
    }

    vs_append_rmac();
    (void)memcpy(s_dev.cmd_icv, s_dev.rmac_iv, 16U);
    vs_set_sw(0x9000U);
    s_dev.sign_count++;
}

static void vs_handle_put_key(void)
{
    uint8_t  mac[16];
    uint8_t  input[16U + 5U + VSE050_MAX_CMD / 8U];
    uint8_t  plain[VSE050_MAX_RESP];
    uint16_t body_len;
    uint16_t payload_len;
    int      plain_len;
    uint8_t  diff;
    uint16_t i;

    if (s_dev.state != VSE050_STATE_OPEN) {
        vs_set_sw(0x6985U);
        return;
    }

    /* cmd = CLA INS P1(key version) P2(0x00) Lc ciphertext... C-MAC(8) */
    if (s_dev.cmd_len < (uint16_t)(5U + SCP03_CMAC_TRUNC) ||
        s_dev.cmd[3] != 0x00U ||
        s_dev.cmd[4] != (uint8_t)(s_dev.cmd_len - 5U)) {
        vs_set_sw(0x6700U);
        return;
    }

    body_len    = (uint16_t)(s_dev.cmd_len - SCP03_CMAC_TRUNC);
    payload_len = (uint16_t)(body_len - 5U);

    /* Verify the host C-MAC over the on-the-wire (encrypted) bytes. */
    (void)memcpy(input, s_dev.cmac_iv, 16U);
    (void)memcpy(&input[16], s_dev.cmd, body_len);
    vse050_sim_aes_cmac(s_dev.s_mac, input, (uint16_t)(16U + body_len), mac);

    diff = 0U;
    for (i = 0; i < SCP03_CMAC_TRUNC; i++) {
        diff |= (uint8_t)(mac[i] ^ s_dev.cmd[body_len + i]);
    }
    (void)memcpy(s_dev.cmac_iv, mac, 16U);
    (void)memcpy(s_dev.resp_icv, mac, 16U);
    (void)memset(input, 0, sizeof(input));

    if (diff != 0U) {
        (void)memset(mac, 0, sizeof(mac));
        s_dev.auth_failures++;
        s_dev.state = VSE050_STATE_RESET;
        vs_set_sw(0x6982U);
        return;
    }
    (void)memset(mac, 0, sizeof(mac));

    /* Key material must be confidential: unencrypted PUT KEY is refused. */
    if ((s_dev.cmd[0] & 0x08U) == 0U) {
        vs_set_sw(0x6982U);
        return;
    }
    if ((payload_len == 0U) || ((payload_len % 16U) != 0U)) {
        vs_set_sw(0x6700U);
        return;
    }

    vs_cbc_decrypt(s_dev.s_enc, s_dev.cmd_icv, &s_dev.cmd[5], plain, payload_len);
    plain_len = vs_unpad(plain, payload_len);

    if ((plain_len < 0) || (plain_len != (int)VSE050_PUT_KEY_DATA_SIZE)) {
        (void)memset(plain, 0, sizeof(plain));
        vs_set_sw(0x6700U);
        return;
    }

    if (s_dev.cfg.reject_put_key != 0U) {
        (void)memset(plain, 0, sizeof(plain));
        vs_set_sw(0x6985U);
        return;
    }

    /* Commit the new key set and advance the sequence counter. */
    (void)memcpy(s_dev.cfg.enc_key,  &plain[0],  16U);
    (void)memcpy(s_dev.cfg.mac_key,  &plain[16], 16U);
    (void)memcpy(s_dev.cfg.rmac_key, &plain[32], 16U);
    s_dev.cfg.key_version = s_dev.cmd[2];
    {
        uint16_t seq = (uint16_t)(((uint16_t)s_dev.cfg.seq_counter[0] << 8) |
                                  (uint16_t)s_dev.cfg.seq_counter[1]);
        seq = (uint16_t)(seq + 1U);
        s_dev.cfg.seq_counter[0] = (uint8_t)(seq >> 8);
        s_dev.cfg.seq_counter[1] = (uint8_t)(seq & 0xFFU);
    }
    s_dev.put_key_count++;
    (void)memset(plain, 0, sizeof(plain));

    /* PUT KEY has no response data: SW only, no R-MAC. */
    vs_set_sw(0x9000U);
}

static void vs_process_apdu(void)
{
    uint8_t cla;
    uint8_t ins;

    s_dev.apdu_count++;
    s_dev.resp_len = 0;
    s_dev.resp_off = 0;

    if (s_dev.cfg.drop_next_response != 0U) {
        s_dev.cfg.drop_next_response = 0;
        return;   /* no block at all: host read times out */
    }

    if (s_dev.cmd_len < 4U) {
        vs_set_sw(0x6700U);
        return;
    }

    cla = s_dev.cmd[0];
    ins = s_dev.cmd[1];

    if (cla == 0x80U && ins == 0x50U) {
        vs_handle_init_update();
    } else if ((cla & 0x04U) != 0U && ins == 0x82U) {
        vs_handle_ext_auth();
    } else if ((cla & 0x04U) != 0U && ins == SE050_APDU_INS_ECDSA_SIGN) {
        vs_handle_sign();
    } else if ((cla & 0x04U) != 0U && ins == 0xD8U) {
        vs_handle_put_key();
    } else {
        vs_set_sw(0x6D00U);
    }
}

static void vs_send_next_chunk(uint32_t ready_at)
{
    uint8_t  frame[SE050_T1_MAX_FRAME];
    uint16_t remain;
    uint16_t chunk;
    uint8_t  pcb;
    int      n;

    if (s_dev.resp_off >= s_dev.resp_len) return;

    remain = (uint16_t)(s_dev.resp_len - s_dev.resp_off);
    chunk  = (remain > SE050_T1_MAX_INF) ? SE050_T1_MAX_INF : remain;
    pcb    = (remain > chunk) ? SE050_T1_PCB_CHAINING : 0x00U;

    n = se050_t1_encode(SE050_T1_NAD_SE, pcb, &s_dev.resp[s_dev.resp_off],
                        chunk, frame, sizeof(frame));
    if (n < 0) return;

    (void)memcpy(s_dev.last_resp_frame, frame, (size_t)n);
    s_dev.last_resp_len  = (uint16_t)n;
    s_dev.resp_off       = (uint16_t)(s_dev.resp_off + chunk);
    s_dev.last_chunk_end = s_dev.resp_off;
    s_dev.have_last_resp = 1U;

    if (s_dev.cfg.corrupt_next_response != 0U) {
        frame[n - 1] ^= 0xFFU;
        s_dev.cfg.corrupt_next_response = 0;
        s_dev.crc_errors++;
    }

    (void)vs_fifo_push(frame, (uint16_t)n, ready_at);
}

static void vs_queue_wtx(void)
{
    uint8_t frame[5];
    int n = se050_t1_encode(SE050_T1_NAD_SE, SE050_T1_PCB_S_WTX_REQ,
                            NULL, 0, frame, sizeof(frame));

    if (n < 0) return;
    (void)vs_fifo_push(frame, (uint16_t)n, s_dev.now_us);
    s_dev.wtx_count++;
    s_dev.wtx_pending = 1U;
}

/* ── T=1' I/O hooks ───────────────────────────────────────── */
static int vs_write(uint8_t addr, const uint8_t *buf, uint16_t len)
{
    uint8_t  pcb;
    uint16_t inf_len;
    uint16_t crc;

    (void)addr;

    if (!s_ready || buf == NULL || len < 5U) return -1;
    if (buf[0] != SE050_T1_NAD_HOST) return -1;

    crc = (uint16_t)buf[len - 2U] | ((uint16_t)buf[len - 1U] << 8);
    if (crc != se050_t1_crc16(buf, (uint16_t)(len - 2U))) return -1;

    pcb = buf[1];

    /* S-block from host */
    if ((pcb & SE050_T1_PCB_S_BLOCK) == SE050_T1_PCB_S_BLOCK) {
        if (pcb == SE050_T1_PCB_S_WTX_RSP && s_dev.wtx_pending != 0U) {
            s_dev.wtx_pending = 0;
            vs_send_next_chunk(s_dev.now_us + s_dev.cfg.latency_us);
        }
        return 0;
    }

    /* R-block: host acknowledges a chained response block */
    if ((pcb & SE050_T1_PCB_R_BLOCK) == SE050_T1_PCB_R_BLOCK) {
        if (s_dev.resp_off < s_dev.resp_len) {
            vs_send_next_chunk(s_dev.now_us);
        }
        return 0;
    }

    inf_len = (buf[2] == 0U) ? 256U : (uint16_t)buf[2];
    if ((uint16_t)(inf_len + 5U) != len) return -1;

    /* Host retransmitted the previous command after a CRC error: resend. */
    if (vs_fifo_empty() != 0 && s_dev.have_last_resp != 0U &&
        len == s_dev.last_cmd_len &&
        memcmp(buf, s_dev.last_cmd_frame, len) == 0) {
        s_dev.resp_off = s_dev.last_chunk_end;
        (void)vs_fifo_push(s_dev.last_resp_frame, s_dev.last_resp_len, s_dev.now_us);
        return 0;
    }

    if (s_dev.cmd_len == 0U) {
        s_dev.resp_len = 0;
        s_dev.resp_off = 0;
        s_dev.have_last_resp = 0;
    }

    if ((uint32_t)s_dev.cmd_len + inf_len > VSE050_MAX_CMD) return -1;
    (void)memcpy(&s_dev.cmd[s_dev.cmd_len], &buf[3], inf_len);
    s_dev.cmd_len = (uint16_t)(s_dev.cmd_len + inf_len);

    (void)memcpy(s_dev.last_cmd_frame, buf, len);
    s_dev.last_cmd_len = len;

    if ((pcb & SE050_T1_PCB_CHAINING) != 0U) {
        uint8_t frame[5];
        int n = se050_t1_encode(SE050_T1_NAD_SE,
                                (uint8_t)(SE050_T1_PCB_R_BLOCK | (pcb & 0x01U)),
                                NULL, 0, frame, sizeof(frame));
        if (n >= 0) (void)vs_fifo_push(frame, (uint16_t)n, s_dev.now_us);
        return 0;
    }

    /* Complete command received */
    vs_process_apdu();

    if (s_dev.cfg.wtx_on_init_update != 0U &&
        s_dev.cmd_len >= 2U && s_dev.cmd[0] == 0x80U && s_dev.cmd[1] == 0x50U &&
        s_dev.resp_len > 0U) {
        vs_queue_wtx();
    } else if (s_dev.resp_len > 0U) {
        vs_send_next_chunk(s_dev.now_us + s_dev.cfg.latency_us);
    }

    s_dev.cmd_len = 0;   /* command consumed; last_cmd_frame keeps the raw block */
    return 0;
}

static int vs_read(uint8_t addr, uint8_t *buf, uint16_t len)
{
    vs_frame_t *f;

    (void)addr;

    if (!s_ready || buf == NULL || len == 0U) return -1;

    f = vs_fifo_head();
    if (f == NULL) {
        s_dev.nack_polls++;
        s_dev.now_us += s_dev.cfg.poll_step_us;
        return -1;
    }
    if (s_dev.now_us < f->ready_at) {
        s_dev.busy_polls++;
        s_dev.now_us += s_dev.cfg.poll_step_us;
        return -1;
    }
    if ((uint16_t)(f->len - f->pos) < len) {
        s_dev.nack_polls++;
        s_dev.now_us += s_dev.cfg.poll_step_us;
        return -1;
    }

    (void)memcpy(buf, &f->data[f->pos], len);
    f->pos = (uint16_t)(f->pos + len);
    if (f->pos >= f->len) vs_fifo_pop();

    return 0;
}

void vse050_sim_delay_us(uint32_t us)
{
    if (s_ready) s_dev.now_us += us;
}

static const se050_t1_io_t s_vs_io = {
    vs_write,
    vs_read,
    vse050_sim_delay_us
};

/* ========================================================================
 *  Public API
 * ======================================================================== */

void vse050_sim_default_config(vse050_config_t *cfg)
{
    if (cfg == NULL) return;

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->key_version = 0x00U;
    cfg->seq_counter[0] = 0x00U;
    cfg->seq_counter[1] = 0x01U;
    {
        static const uint8_t kdd[10] = {
            'S', 'E', '0', '5', '0', 'A', '0', '1', '0', '0'
        };
        (void)memcpy(cfg->key_divers_data, kdd, 10U);
    }
    {
        static const uint8_t cc[8] = {
            0xC0U, 0x53U, 0x45U, 0x30U, 0x35U, 0x30U, 0x01U, 0x00U
        };
        (void)memcpy(cfg->card_challenge, cc, 8U);
    }
    cfg->latency_us    = 3000U;
    cfg->poll_step_us  = 500U;
}

void vse050_sim_init(const vse050_config_t *cfg)
{
    (void)memset(&s_dev, 0, sizeof(s_dev));
    if (cfg != NULL) {
        s_dev.cfg = *cfg;
    } else {
        vse050_sim_default_config(&s_dev.cfg);
    }
    if (s_dev.cfg.poll_step_us == 0U) s_dev.cfg.poll_step_us = 500U;

    s_dev.state = VSE050_STATE_RESET;
    s_ready = 1;
    vs_fifo_reset();
    vse050_sim_bind();
}

void vse050_sim_bind(void)
{
    se050_t1_set_io(&s_vs_io);
}

const se050_t1_io_t *vse050_sim_io(void)
{
    return &s_vs_io;
}

vse050_state_e vse050_sim_state(void)  { return s_dev.state; }
uint32_t vse050_sim_now_us(void)       { return s_dev.now_us; }
uint32_t vse050_sim_busy_polls(void)   { return s_dev.busy_polls; }
uint32_t vse050_sim_nack_polls(void)   { return s_dev.nack_polls; }
uint32_t vse050_sim_wtx_count(void)    { return s_dev.wtx_count; }
uint32_t vse050_sim_crc_errors(void)   { return s_dev.crc_errors; }
uint32_t vse050_sim_auth_failures(void){ return s_dev.auth_failures; }
uint32_t vse050_sim_sign_count(void)   { return s_dev.sign_count; }
uint32_t vse050_sim_put_key_count(void){ return s_dev.put_key_count; }
uint8_t  vse050_sim_key_version(void)  { return s_dev.cfg.key_version; }
uint32_t vse050_sim_encrypted_cmds(void){ return s_dev.encrypted_cmds; }
uint16_t vse050_sim_last_cipher_len(void){ return s_dev.last_cipher_len; }
uint16_t vse050_sim_apdu_count(void)   { return s_dev.apdu_count; }

/*
 * The model is T=1'-only: a raw ISO 7816 APDU (legacy close/reset path) is
 * not accepted, matching the real device behaviour without T=1' framing.
 */
int32_t i2c_transfer(uint8_t dev, uint8_t addr,
                     const uint8_t *tx, uint16_t tx_len,
                     uint8_t *rx, uint16_t rx_len)
{
    (void)dev; (void)addr; (void)tx; (void)tx_len; (void)rx; (void)rx_len;
    return -1;
}
