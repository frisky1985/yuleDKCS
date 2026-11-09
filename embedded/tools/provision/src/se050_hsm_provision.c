/**
 * @file se050_hsm_provision.c
 * @brief SE050 产线 HSM 密钥派生工具 — 参考实现
 *
 * 自包含实现（AES-128/192/256、AES-CMAC、SHA-256），不依赖 OpenSSL 或
 * 固件密码库，便于产线工具独立部署与交叉验证。
 *
 * 仅 SOFTWARE 后端在此实现；PKCS#11 后端通过调用方注入的 ops vtable
 * （@ref hsm_prov_pkcs11_ops_t）委托给厂商适配层，MK 不出 HSM。
 */

#include "se050_hsm_provision.h"

#include <string.h>

/* ========================================================================
 * 安全清零
 * ======================================================================== */

static void hsm_zeroize(void *ptr, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)ptr;

    if (ptr == NULL) {
        return;
    }
    while (len > 0U) {
        *p = 0U;
        p++;
        len--;
    }
}

/* ========================================================================
 * AES-128 / AES-192 / AES-256 (FIPS 197)
 * ======================================================================== */

static const uint8_t s_hsm_sbox[256] = {
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

static const uint8_t s_hsm_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36
};

typedef struct {
    uint32_t rk[60];  /* 最多 AES-256: 4 × 15 = 60 words */
    uint8_t  rounds;
} hsm_aes_ctx_t;

static uint32_t hsm_load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void hsm_store_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint8_t hsm_gf_mul2(uint8_t x)
{
    return (uint8_t)((uint8_t)(x << 1) ^ (((x & 0x80U) != 0U) ? 0x1BU : 0x00U));
}

static uint8_t hsm_gf_mul3(uint8_t x)
{
    return (uint8_t)(hsm_gf_mul2(x) ^ x);
}

static uint32_t hsm_sub_word(uint32_t w)
{
    return ((uint32_t)s_hsm_sbox[(w >> 24) & 0xFFU] << 24) |
           ((uint32_t)s_hsm_sbox[(w >> 16) & 0xFFU] << 16) |
           ((uint32_t)s_hsm_sbox[(w >> 8)  & 0xFFU] << 8)  |
           ((uint32_t)s_hsm_sbox[w         & 0xFFU]);
}

static uint32_t hsm_rot_word(uint32_t w)
{
    return (w << 8) | (w >> 24);
}

/**
 * @brief AES 密钥扩展，支持 128/192/256 位密钥。
 * @return 0 成功，-1 密钥长度非法。
 */
static int hsm_aes_key_expand(hsm_aes_ctx_t *ctx, const uint8_t *key, size_t key_len)
{
    int nk;
    int nr;
    int total;
    int i;

    if (key_len == 16U) {
        nk = 4;
        nr = 10;
    } else if (key_len == 24U) {
        nk = 6;
        nr = 12;
    } else if (key_len == 32U) {
        nk = 8;
        nr = 14;
    } else {
        return -1;
    }

    total = 4 * (nr + 1);
    for (i = 0; i < nk; i++) {
        ctx->rk[i] = hsm_load_be32(&key[(size_t)i * 4U]);
    }
    for (i = nk; i < total; i++) {
        uint32_t temp = ctx->rk[i - 1];

        if ((i % nk) == 0) {
            temp = hsm_sub_word(hsm_rot_word(temp)) ^
                   ((uint32_t)s_hsm_rcon[i / nk] << 24);
        } else if ((nk > 6) && ((i % nk) == 4)) {
            temp = hsm_sub_word(temp);
        }
        ctx->rk[i] = ctx->rk[i - nk] ^ temp;
    }
    ctx->rounds = (uint8_t)nr;
    return 0;
}

static void hsm_aes_encrypt_block(const hsm_aes_ctx_t *ctx,
                                  const uint8_t in[16], uint8_t out[16])
{
    uint32_t s[4];
    uint8_t  tmp[16];
    int round;
    int i;
    int c;

    s[0] = hsm_load_be32(in)      ^ ctx->rk[0];
    s[1] = hsm_load_be32(in + 4)  ^ ctx->rk[1];
    s[2] = hsm_load_be32(in + 8)  ^ ctx->rk[2];
    s[3] = hsm_load_be32(in + 12) ^ ctx->rk[3];

    for (round = 1; round < (int)ctx->rounds; round++) {
        uint8_t t[16];
        uint8_t sr[16];
        uint32_t off = (uint32_t)round * 4U;

        for (i = 0; i < 4; i++) {
            t[(size_t)i * 4U]      = s_hsm_sbox[(s[i] >> 24) & 0xFFU];
            t[(size_t)i * 4U + 1U] = s_hsm_sbox[(s[i] >> 16) & 0xFFU];
            t[(size_t)i * 4U + 2U] = s_hsm_sbox[(s[i] >> 8)  & 0xFFU];
            t[(size_t)i * 4U + 3U] = s_hsm_sbox[s[i]         & 0xFFU];
        }

        sr[0]  = t[0];  sr[4]  = t[4];  sr[8]  = t[8];  sr[12] = t[12];
        sr[1]  = t[5];  sr[5]  = t[9];  sr[9]  = t[13]; sr[13] = t[1];
        sr[2]  = t[10]; sr[6]  = t[14]; sr[10] = t[2];  sr[14] = t[6];
        sr[3]  = t[15]; sr[7]  = t[3];  sr[11] = t[7];  sr[15] = t[11];

        for (c = 0; c < 4; c++) {
            uint8_t a0 = sr[(size_t)c * 4U];
            uint8_t a1 = sr[(size_t)c * 4U + 1U];
            uint8_t a2 = sr[(size_t)c * 4U + 2U];
            uint8_t a3 = sr[(size_t)c * 4U + 3U];

            tmp[(size_t)c * 4U]      = hsm_gf_mul2(a0) ^ hsm_gf_mul3(a1) ^ a2 ^ a3;
            tmp[(size_t)c * 4U + 1U] = a0 ^ hsm_gf_mul2(a1) ^ hsm_gf_mul3(a2) ^ a3;
            tmp[(size_t)c * 4U + 2U] = a0 ^ a1 ^ hsm_gf_mul2(a2) ^ hsm_gf_mul3(a3);
            tmp[(size_t)c * 4U + 3U] = hsm_gf_mul3(a0) ^ a1 ^ a2 ^ hsm_gf_mul2(a3);
        }

        s[0] = hsm_load_be32(tmp)      ^ ctx->rk[off];
        s[1] = hsm_load_be32(tmp + 4)  ^ ctx->rk[off + 1U];
        s[2] = hsm_load_be32(tmp + 8)  ^ ctx->rk[off + 2U];
        s[3] = hsm_load_be32(tmp + 12) ^ ctx->rk[off + 3U];
    }

    {
        uint8_t t[16];
        uint32_t off = (uint32_t)ctx->rounds * 4U;

        for (i = 0; i < 4; i++) {
            t[(size_t)i * 4U]      = s_hsm_sbox[(s[i] >> 24) & 0xFFU];
            t[(size_t)i * 4U + 1U] = s_hsm_sbox[(s[i] >> 16) & 0xFFU];
            t[(size_t)i * 4U + 2U] = s_hsm_sbox[(s[i] >> 8)  & 0xFFU];
            t[(size_t)i * 4U + 3U] = s_hsm_sbox[s[i]         & 0xFFU];
        }

        out[0]  = t[0];  out[4]  = t[4];  out[8]  = t[8];  out[12] = t[12];
        out[1]  = t[5];  out[5]  = t[9];  out[9]  = t[13]; out[13] = t[1];
        out[2]  = t[10]; out[6]  = t[14]; out[10] = t[2];  out[14] = t[6];
        out[3]  = t[15]; out[7]  = t[3];  out[11] = t[7];  out[15] = t[11];

        s[0] = hsm_load_be32(out)      ^ ctx->rk[off];
        s[1] = hsm_load_be32(out + 4)  ^ ctx->rk[off + 1U];
        s[2] = hsm_load_be32(out + 8)  ^ ctx->rk[off + 2U];
        s[3] = hsm_load_be32(out + 12) ^ ctx->rk[off + 3U];

        hsm_store_be32(out,      s[0]);
        hsm_store_be32(out + 4,  s[1]);
        hsm_store_be32(out + 8,  s[2]);
        hsm_store_be32(out + 12, s[3]);
    }
}

/* ========================================================================
 * AES-CMAC (NIST SP 800-38B)
 * ======================================================================== */

static void hsm_cmac_dbl(const uint8_t in[16], uint8_t out[16])
{
    uint8_t i;
    uint8_t msb = (uint8_t)(in[0] & 0x80U);

    for (i = 0; i < 15U; i++) {
        out[i] = (uint8_t)((uint8_t)(in[i] << 1) | (uint8_t)(in[i + 1U] >> 7));
    }
    out[15] = (uint8_t)(in[15] << 1);
    if (msb != 0U) {
        out[15] ^= 0x87U;
    }
}

static void hsm_xor16(uint8_t dst[16], const uint8_t src[16])
{
    uint8_t i;

    for (i = 0; i < 16U; i++) {
        dst[i] ^= src[i];
    }
}

/**
 * @brief AES-CMAC 计算。
 * @return 0 成功，-1 密钥长度非法。
 */
static int hsm_cmac(const uint8_t *key, size_t key_len,
                    const uint8_t *msg, size_t msg_len,
                    uint8_t mac[16])
{
    hsm_aes_ctx_t aes;
    uint8_t l[16];
    uint8_t k1[16];
    uint8_t k2[16];
    uint8_t x[16];
    uint8_t blk[16];
    size_t  n;
    size_t  i;
    int     complete;

    if ((key == NULL) || (mac == NULL)) {
        return -1;
    }
    if ((msg == NULL) && (msg_len != 0U)) {
        return -1;
    }
    if (hsm_aes_key_expand(&aes, key, key_len) != 0) {
        return -1;
    }

    (void)memset(l, 0, sizeof(l));
    hsm_aes_encrypt_block(&aes, l, l);
    hsm_cmac_dbl(l, k1);
    hsm_cmac_dbl(k1, k2);

    n = (msg_len + 15U) / 16U;
    if (n == 0U) {
        n = 1U;
    }
    complete = ((msg_len > 0U) && ((msg_len % 16U) == 0U)) ? 1 : 0;

    (void)memset(x, 0, sizeof(x));
    for (i = 0; i + 1U < n; i++) {
        hsm_xor16(x, &msg[i * 16U]);
        hsm_aes_encrypt_block(&aes, x, x);
    }

    (void)memset(blk, 0, sizeof(blk));
    if (complete != 0) {
        (void)memcpy(blk, &msg[(n - 1U) * 16U], 16U);
        hsm_xor16(blk, k1);
    } else {
        size_t rem = msg_len - ((n - 1U) * 16U);

        if (rem > 0U) {
            (void)memcpy(blk, &msg[(n - 1U) * 16U], rem);
        }
        blk[rem] = 0x80U;
        hsm_xor16(blk, k2);
    }
    hsm_xor16(x, blk);
    hsm_aes_encrypt_block(&aes, x, x);

    (void)memcpy(mac, x, 16U);

    hsm_zeroize(&aes, sizeof(aes));
    hsm_zeroize(l, sizeof(l));
    hsm_zeroize(k1, sizeof(k1));
    hsm_zeroize(k2, sizeof(k2));
    hsm_zeroize(x, sizeof(x));
    hsm_zeroize(blk, sizeof(blk));
    return 0;
}

/* ========================================================================
 * SHA-256 (FIPS 180-4)
 * ======================================================================== */

static const uint32_t s_hsm_sha256_k[64] = {
    0x428A2F98UL,0x71374491UL,0xB5C0FBCFUL,0xE9B5DBA5UL,
    0x3956C25BUL,0x59F111F1UL,0x923F82A4UL,0xAB1C5ED5UL,
    0xD807AA98UL,0x12835B01UL,0x243185BEUL,0x550C7DC3UL,
    0x72BE5D74UL,0x80DEB1FEUL,0x9BDC06A7UL,0xC19BF174UL,
    0xE49B69C1UL,0xEFBE4786UL,0x0FC19DC6UL,0x240CA1CCUL,
    0x2DE92C6FUL,0x4A7484AAUL,0x5CB0A9DCUL,0x76F988DAUL,
    0x983E5152UL,0xA831C66DUL,0xB00327C8UL,0xBF597FC7UL,
    0xC6E00BF3UL,0xD5A79147UL,0x06CA6351UL,0x14292967UL,
    0x27B70A85UL,0x2E1B2138UL,0x4D2C6DFCUL,0x53380D13UL,
    0x650A7354UL,0x766A0ABBUL,0x81C2C92EUL,0x92722C85UL,
    0xA2BFE8A1UL,0xA81A664BUL,0xC24B8B70UL,0xC76C51A3UL,
    0xD192E819UL,0xD6990624UL,0xF40E3585UL,0x106AA070UL,
    0x19A4C116UL,0x1E376C08UL,0x2748774CUL,0x34B0BCB5UL,
    0x391C0CB3UL,0x4ED8AA4AUL,0x5B9CCA4FUL,0x682E6FF3UL,
    0x748F82EEUL,0x78A5636FUL,0x84C87814UL,0x8CC70208UL,
    0x90BEFFFAUL,0xA4506CEBUL,0xBEF9A3F7UL,0xC67178F2UL
};

static uint32_t hsm_rotr32(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32U - n));
}

static void hsm_sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t i;

    for (i = 0; i < 16U; i++) {
        w[i] = hsm_load_be32(&block[i * 4U]);
    }
    for (i = 16U; i < 64U; i++) {
        uint32_t s0 = hsm_rotr32(w[i - 15U], 7U) ^ hsm_rotr32(w[i - 15U], 18U) ^ (w[i - 15U] >> 3);
        uint32_t s1 = hsm_rotr32(w[i - 2U], 17U) ^ hsm_rotr32(w[i - 2U], 19U) ^ (w[i - 2U] >> 10);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (i = 0; i < 64U; i++) {
        uint32_t s1  = hsm_rotr32(e, 6U) ^ hsm_rotr32(e, 11U) ^ hsm_rotr32(e, 25U);
        uint32_t ch  = (e & f) ^ ((~e) & g);
        uint32_t t1  = h + s1 + ch + s_hsm_sha256_k[i] + w[i];
        uint32_t s0  = hsm_rotr32(a, 2U) ^ hsm_rotr32(a, 13U) ^ hsm_rotr32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2  = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;

    hsm_zeroize(w, sizeof(w));
}

static void hsm_sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    uint32_t state[8] = {
        0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
        0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
    };
    uint8_t  buf[64];
    size_t   offset = 0U;
    size_t   rem;
    uint64_t bits;
    uint32_t i;

    while ((len - offset) >= 64U) {
        hsm_sha256_transform(state, &data[offset]);
        offset += 64U;
    }

    rem = len - offset;
    (void)memset(buf, 0, sizeof(buf));
    if (rem > 0U) {
        (void)memcpy(buf, &data[offset], rem);
    }
    buf[rem] = 0x80U;

    if (rem >= 56U) {
        hsm_sha256_transform(state, buf);
        (void)memset(buf, 0, sizeof(buf));
    }

    bits = (uint64_t)len * 8U;
    for (i = 0; i < 8U; i++) {
        buf[63U - i] = (uint8_t)(bits >> (i * 8U));
    }
    hsm_sha256_transform(state, buf);

    for (i = 0; i < 8U; i++) {
        hsm_store_be32(&out[i * 4U], state[i]);
    }

    hsm_zeroize(state, sizeof(state));
    hsm_zeroize(buf, sizeof(buf));
}

/* ========================================================================
 * 内部辅助
 * ======================================================================== */

#define HSM_DERIVE_LABEL      0x01U
#define HSM_REPLACE_LABEL     0x02U  /* 仅供文档对齐，本模块不执行轮换 */
#define HSM_KEY_ID_ENC        0x01U
#define HSM_KEY_ID_MAC        0x02U
#define HSM_KEY_ID_RMAC       0x03U
#define HSM_DERIVE_INPUT_LEN  (1U + 1U + HSM_PROV_UID_LEN + 1U)

static const uint8_t s_hsm_fp_domain[20] = {
    'Y','U','L','E','-','D','K','-','T','R',
    'A','N','S','P','O','R','T','-','V','1'
};

/* HSM 内密钥标签（与软件后端 key_id 一一对应） */
static const char *const s_hsm_key_labels[3] = {
    "K-ENC", "K-MAC", "K-RMAC"
};

static int hsm_is_zero(const uint8_t *p, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (p[i] != 0U) {
            return 0;
        }
    }
    return 1;
}

static int hsm_valid_mk_len(size_t len)
{
    return ((len == 16U) || (len == 24U) || (len == 32U)) ? 1 : 0;
}

/* ========================================================================
 * 公开接口
 * ======================================================================== */

hsm_prov_status_e hsm_provision_init(hsm_prov_ctx_t *ctx,
                                     const hsm_prov_config_t *cfg)
{
    if ((ctx == NULL) || (cfg == NULL)) {
        return HSM_PROV_ERR_PARAM;
    }

    hsm_zeroize(ctx, sizeof(*ctx));

    if (cfg->backend == HSM_PROV_BACKEND_SOFTWARE) {
        if (cfg->allow_software == 0U) {
            return HSM_PROV_ERR_DENIED;
        }
        if ((cfg->mk == NULL) || (hsm_valid_mk_len(cfg->mk_len) == 0)) {
            return HSM_PROV_ERR_PARAM;
        }
        (void)memcpy(ctx->mk, cfg->mk, cfg->mk_len);
        ctx->mk_len = (uint8_t)cfg->mk_len;
        ctx->backend = HSM_PROV_BACKEND_SOFTWARE;
        ctx->ready = 1U;
        return HSM_PROV_OK;
    }

    if (cfg->backend == HSM_PROV_BACKEND_PKCS11) {
        /* 厂商适配层由调用方注入；缺少任一必需项即视为后端不可用。 */
        if ((cfg->pkcs11 == NULL) || (cfg->pkcs11->aes_cmac == NULL) ||
            (cfg->pkcs11->session == NULL)) {
            return HSM_PROV_ERR_UNAVAILABLE;
        }
        ctx->backend = HSM_PROV_BACKEND_PKCS11;
        ctx->pkcs11 = cfg->pkcs11;
        ctx->ready = 1U;
        return HSM_PROV_OK;
    }

    return HSM_PROV_ERR_PARAM;
}

void hsm_provision_deinit(hsm_prov_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    hsm_zeroize(ctx, sizeof(*ctx));
}

hsm_prov_status_e hsm_provision_derive_transport(
    const hsm_prov_ctx_t *ctx,
    const uint8_t uid[HSM_PROV_UID_LEN],
    uint8_t key_version,
    hsm_prov_key_set_t *out)
{
    uint8_t input[HSM_DERIVE_INPUT_LEN];
    uint8_t *dest[3];
    uint8_t  key_ids[3];
    size_t   i;

    if (out != NULL) {
        hsm_zeroize(out, sizeof(*out));
    }
    if ((ctx == NULL) || (uid == NULL) || (out == NULL)) {
        return HSM_PROV_ERR_PARAM;
    }
    if (ctx->ready == 0U) {
        return HSM_PROV_ERR_STATE;
    }
    if (key_version < HSM_PROV_KEY_VERSION_MIN) {
        return HSM_PROV_ERR_PARAM;
    }
    if (hsm_is_zero(uid, HSM_PROV_UID_LEN) != 0) {
        return HSM_PROV_ERR_PARAM;
    }

    input[0] = HSM_DERIVE_LABEL;
    (void)memcpy(&input[2], uid, HSM_PROV_UID_LEN);
    input[2U + HSM_PROV_UID_LEN] = key_version;

    dest[0] = out->k_enc;
    dest[1] = out->k_mac;
    dest[2] = out->k_rmac;
    key_ids[0] = HSM_KEY_ID_ENC;
    key_ids[1] = HSM_KEY_ID_MAC;
    key_ids[2] = HSM_KEY_ID_RMAC;

    for (i = 0; i < 3U; i++) {
        int rc;

        input[1] = key_ids[i];
        if (ctx->backend == HSM_PROV_BACKEND_PKCS11) {
            if ((ctx->pkcs11 == NULL) || (ctx->pkcs11->aes_cmac == NULL)) {
                hsm_zeroize(input, sizeof(input));
                hsm_zeroize(out, sizeof(*out));
                return HSM_PROV_ERR_CRYPTO;
            }
            rc = ctx->pkcs11->aes_cmac(ctx->pkcs11->session, s_hsm_key_labels[i],
                                       input, sizeof(input), dest[i]);
        } else {
            rc = hsm_cmac(ctx->mk, (size_t)ctx->mk_len, input, sizeof(input), dest[i]);
        }
        if (rc != 0) {
            hsm_zeroize(input, sizeof(input));
            hsm_zeroize(out, sizeof(*out));
            return HSM_PROV_ERR_CRYPTO;
        }
    }

    out->key_version = key_version;
    hsm_zeroize(input, sizeof(input));
    return HSM_PROV_OK;
}

hsm_prov_status_e hsm_provision_fingerprint(
    const uint8_t uid[HSM_PROV_UID_LEN],
    const hsm_prov_key_set_t *set,
    uint8_t out[HSM_PROV_FINGERPRINT_LEN])
{
    uint8_t buf[sizeof(s_hsm_fp_domain) + HSM_PROV_UID_LEN + (3U * HSM_PROV_KEY_LEN)];
    size_t  off = 0U;

    if (out != NULL) {
        hsm_zeroize(out, HSM_PROV_FINGERPRINT_LEN);
    }
    if ((uid == NULL) || (set == NULL) || (out == NULL)) {
        return HSM_PROV_ERR_PARAM;
    }

    (void)memcpy(&buf[off], s_hsm_fp_domain, sizeof(s_hsm_fp_domain));
    off += sizeof(s_hsm_fp_domain);
    (void)memcpy(&buf[off], uid, HSM_PROV_UID_LEN);
    off += HSM_PROV_UID_LEN;
    (void)memcpy(&buf[off], set->k_enc, HSM_PROV_KEY_LEN);
    off += HSM_PROV_KEY_LEN;
    (void)memcpy(&buf[off], set->k_mac, HSM_PROV_KEY_LEN);
    off += HSM_PROV_KEY_LEN;
    (void)memcpy(&buf[off], set->k_rmac, HSM_PROV_KEY_LEN);
    off += HSM_PROV_KEY_LEN;

    hsm_sha256(buf, off, out);
    hsm_zeroize(buf, sizeof(buf));
    return HSM_PROV_OK;
}

void hsm_provision_key_set_clear(hsm_prov_key_set_t *set)
{
    if (set == NULL) {
        return;
    }
    hsm_zeroize(set, sizeof(*set));
}
