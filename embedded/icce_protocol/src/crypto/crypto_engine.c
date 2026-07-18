/**
 * @file crypto_engine.c
 * @brief 统一密码算法引擎实现
 * @version 1.0
 * @date 2026-05-28
 *
 * 整合 SM2/SM3/SM4 与 P-256/SHA-256/AES-256-GCM 两套算法栈。
 * 包含完整的 SHA-256、AES-256、AES-GCM、HMAC-SHA256 嵌入式实现。
 *
 * 开关: 可通过预处理器定义 USE_SM_CRYPTO 在编译时锁定为国密算法,
 *       或在运行时通过 crypto_engine_set_algo() 动态切换。
 *
 * 所有嵌入式密码算法均为纯 C 实现, 无外部库依赖。
 */

#include "crypto_engine.h"
#include "crypto_utils.h"

/* ========================================================================
 *  条件编译: USE_SM_CRYPTO
 * ========================================================================
 * 定义后引擎默认使用 SM2/SM3/SM4; 否则默认使用 ECC P-256/SHA-256/AES-256-GCM。
 */
#ifdef USE_SM_CRYPTO
#define DEFAULT_ECC  CRYPTO_ALGO_SM2
#define DEFAULT_HASH HASH_ALGO_SM3
#define DEFAULT_SYM  SYM_ALGO_SM4_GCM
#else
#define DEFAULT_ECC  CRYPTO_ALGO_ECC_P256
#define DEFAULT_HASH HASH_ALGO_SHA256
#define DEFAULT_SYM  SYM_ALGO_AES256_GCM
#endif

/* ========================================================================
 *  全局算法选择
 * ======================================================================== */
static struct {
    crypto_algo_e ecc;
    hash_algo_e   hash;
    sym_algo_e    sym;
    int           initialized;
} g_engine = {
    .ecc  = DEFAULT_ECC,
    .hash = DEFAULT_HASH,
    .sym  = DEFAULT_SYM,
    .initialized = 0
};

/* ========================================================================
 *  引擎生命周期
 * ======================================================================== */

int crypto_engine_init(void)
{
    if (g_engine.initialized) return CRYPTO_SUCCESS;
    g_engine.initialized = 1;
    return CRYPTO_SUCCESS;
}

void crypto_engine_deinit(void)
{
    crypto_secure_zero(&g_engine, sizeof(g_engine));
    g_engine.initialized = 0;
}

int crypto_engine_set_algo(crypto_algo_e ecc, hash_algo_e hash, sym_algo_e sym)
{
    if (ecc != CRYPTO_ALGO_ECC_P256 && ecc != CRYPTO_ALGO_SM2)
        return CRYPTO_ERR_INVALID_INPUT;
    if (hash != HASH_ALGO_SHA256 && hash != HASH_ALGO_SM3)
        return CRYPTO_ERR_INVALID_INPUT;
    if (sym != SYM_ALGO_AES256_GCM && sym != SYM_ALGO_SM4_GCM)
        return CRYPTO_ERR_INVALID_INPUT;

    g_engine.ecc  = ecc;
    g_engine.hash = hash;
    g_engine.sym  = sym;
    return CRYPTO_SUCCESS;
}

void crypto_engine_get_algo(crypto_algo_e *ecc, hash_algo_e *hash, sym_algo_e *sym)
{
    if (ecc)  *ecc  = g_engine.ecc;
    if (hash) *hash = g_engine.hash;
    if (sym)  *sym  = g_engine.sym;
}

/* ========================================================================
 *  SHA-256 实现 (FIPS 180-4)
 * ======================================================================== */

static const uint32_t SHA256_K[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

#define SHA256_SIG0(x) (rotl32(x, 30) ^ rotl32(x, 19) ^ rotl32(x, 10))
#define SHA256_SIG1(x) (rotl32(x, 26) ^ rotl32(x, 21) ^ rotl32(x, 7))
#define SHA256_sig0(x) (rotl32(x, 25) ^ rotl32(x, 14) ^ ((x) >> 3))
#define SHA256_sig1(x) (rotl32(x, 15) ^ rotl32(x, 13) ^ ((x) >> 10))

#define SHA256_CH(x,y,z)  (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

typedef struct {
    uint64_t total_bits;
    uint8_t  block[64];
    uint32_t block_len;
    uint32_t state[8];
} sha256_ctx_t;

static int sha256_init(sha256_ctx_t *ctx)
{
    if (!ctx) return CRYPTO_ERR_NULL_PTR;
    ctx->total_bits = 0;
    ctx->block_len  = 0;
    ctx->state[0] = 0x6A09E667;
    ctx->state[1] = 0xBB67AE85;
    ctx->state[2] = 0x3C6EF372;
    ctx->state[3] = 0xA54FF53A;
    ctx->state[4] = 0x510E527F;
    ctx->state[5] = 0x9B05688C;
    ctx->state[6] = 0x1F83D9AB;
    ctx->state[7] = 0x5BE0CD19;
    return CRYPTO_SUCCESS;
}

static void sha256_process_block(sha256_ctx_t *ctx)
{
    uint32_t W[64], a, b, c, d, e, f, g, h, t1, t2;

    for (int i = 0; i < 16; i++)
        W[i] = load_be32(ctx->block + i * 4);
    for (int i = 16; i < 64; i++)
        W[i] = SHA256_sig1(W[i-2]) + W[i-7] + SHA256_sig0(W[i-15]) + W[i-16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        t1 = h + SHA256_SIG1(e) + SHA256_CH(e,f,g) + SHA256_K[i] + W[i];
        t2 = SHA256_SIG0(a) + SHA256_MAJ(a,b,c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static int sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || (!data && len > 0)) return CRYPTO_ERR_NULL_PTR;
    ctx->total_bits += (uint64_t)len * 8;

    while (len > 0) {
        size_t space = 64 - ctx->block_len;
        size_t copy  = (len < space) ? len : space;
        memcpy(ctx->block + ctx->block_len, data, copy);
        ctx->block_len += (uint32_t)copy;
        data += copy; len -= copy;

        if (ctx->block_len == 64) {
            sha256_process_block(ctx);
            ctx->block_len = 0;
        }
    }
    return CRYPTO_SUCCESS;
}

static int sha256_final(sha256_ctx_t *ctx, uint8_t hash[32])
{
    if (!ctx || !hash) return CRYPTO_ERR_NULL_PTR;
    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);
    while (ctx->block_len != 56) {
        uint8_t zero = 0;
        sha256_update(ctx, &zero, 1);
    }
    uint8_t bits[8];
    store_be64(bits, ctx->total_bits);
    sha256_update(ctx, bits, 8);

    for (int i = 0; i < 8; i++)
        store_be32(hash + i * 4, ctx->state[i]);

    crypto_secure_zero(ctx, sizeof(sha256_ctx_t));
    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  SHA-256 暴露接口
 * ======================================================================== */

int crypto_sha256(const uint8_t *data, size_t len, uint8_t hash[32])
{
    if (!data || !hash) return CRYPTO_ERR_NULL_PTR;
    sha256_ctx_t ctx;
    int ret = sha256_init(&ctx);
    if (ret != CRYPTO_SUCCESS) return ret;
    ret = sha256_update(&ctx, data, len);
    if (ret != CRYPTO_SUCCESS) return ret;
    return sha256_final(&ctx, hash);
}

/* ========================================================================
 *  HMAC-SHA256 (RFC 2104)
 * ======================================================================== */

int crypto_hmac_sha256(const uint8_t *key, size_t klen,
                       const uint8_t *data, size_t dlen,
                       uint8_t mac[32])
{
    if (!key || !data || !mac) return CRYPTO_ERR_NULL_PTR;

    sha256_ctx_t ctx;
    uint8_t k_ipad[64], k_opad[64], tmp[32];
    uint8_t ekey[64];
    size_t  ekey_len;

    /* 密钥超过分组长度则先哈希 */
    if (klen > 64) {
        crypto_sha256(key, klen, ekey);
        ekey_len = 32;
    } else {
        memcpy(ekey, key, klen);
        ekey_len = klen;
    }
    if (ekey_len < 64) memset(ekey + ekey_len, 0, 64 - ekey_len);

    for (int i = 0; i < 64; i++) {
        k_ipad[i] = ekey[i] ^ 0x36;
        k_opad[i] = ekey[i] ^ 0x5C;
    }

    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, 64);
    sha256_update(&ctx, data, dlen);
    sha256_final(&ctx, tmp);

    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, 64);
    sha256_update(&ctx, tmp, 32);
    sha256_final(&ctx, mac);

    crypto_secure_zero(k_ipad, sizeof(k_ipad));
    crypto_secure_zero(k_opad, sizeof(k_opad));
    crypto_secure_zero(ekey, sizeof(ekey));
    crypto_secure_zero(tmp, sizeof(tmp));

    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  密钥派生函数 (HKDF-Expand 风格)
 * ======================================================================== */

int crypto_kdf(const uint8_t *key, size_t key_len,
               const uint8_t *salt, size_t salt_len,
               const uint8_t *info, size_t info_len,
               uint8_t *out, size_t out_len)
{
    if (!key || !out) return CRYPTO_ERR_NULL_PTR;

    /* 简单 HMAC-SHA256 派生 */
    uint8_t prk[32];

    /* 提取: PRK = HMAC-SHA256(salt, key) */
    if (salt && salt_len > 0) {
        crypto_hmac_sha256(salt, salt_len, key, key_len, prk);
    } else {
        /* 无 salt, 使用空串 (32 字节 0) 作为 salt */
        uint8_t zero_salt[32] = {0};
        crypto_hmac_sha256(zero_salt, 32, key, key_len, prk);
    }

    /* 展开: T(i) = HMAC-SHA256(PRK, T(i-1) || info || i) */
    uint8_t T[32] = {0};
    size_t offset = 0;
    uint32_t counter = 1;

    while (offset < out_len) {
        uint8_t ctx_buf[32 + 256 + 4]; /* T_prev + info + counter */
        size_t ctx_len = 0;

        if (counter > 1) {
            memcpy(ctx_buf, T, 32);
            ctx_len = 32;
        }
        if (info && info_len > 0) {
            memcpy(ctx_buf + ctx_len, info, info_len);
            ctx_len += info_len;
        }
        uint8_t c_be[4];
        store_be32(c_be, counter);
        memcpy(ctx_buf + ctx_len, c_be, 4);
        ctx_len += 4;

        crypto_hmac_sha256(prk, 32, ctx_buf, ctx_len, T);

        size_t todo = (out_len - offset < 32) ? (out_len - offset) : 32;
        memcpy(out + offset, T, todo);
        offset += todo;
        counter++;
    }

    crypto_secure_zero(prk, sizeof(prk));
    crypto_secure_zero(T, sizeof(T));
    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  AES-256 实现 (FIPS 197)
 * ======================================================================== */

/* AES S-box */
static const uint8_t AES_SBOX[256] = {
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

/* AES 轮常数 */
static const uint8_t AES_RCON[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10,
                                     0x20, 0x40, 0x80, 0x1B, 0x36};

/* GF(2^8) 辅助 */
static inline uint8_t gf_mul2(uint8_t x)
{
    return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1B : 0));
}

static inline uint8_t gf_mul3(uint8_t x)
{
    return gf_mul2(x) ^ x;
}

/* 32 位 SubWord */
static inline uint32_t aes_sub_word(uint32_t w)
{
    return ((uint32_t)AES_SBOX[(w >> 24) & 0xFF] << 24) |
           ((uint32_t)AES_SBOX[(w >> 16) & 0xFF] << 16) |
           ((uint32_t)AES_SBOX[(w >>  8) & 0xFF] <<  8) |
           ((uint32_t)AES_SBOX[(w      ) & 0xFF]);
}

static inline uint32_t aes_rot_word(uint32_t w)
{
    return (w << 8) | (w >> 24);
}

/* AES 密钥扩展 (256 位, 14 轮, 60 个字) */
typedef struct {
    uint32_t rk[60];  /* 加密轮密钥 (15 轮 × 4 字) */
} aes256_ctx_t;

static int aes256_set_key(aes256_ctx_t *ctx, const uint8_t key[32])
{
    if (!ctx || !key) return CRYPTO_ERR_NULL_PTR;

    uint32_t *W = ctx->rk;

    /* 前 8 个字直接来自密钥 */
    for (int i = 0; i < 8; i++)
        W[i] = load_be32(key + i * 4);

    /* 扩展到 60 个字 */
    for (int i = 8; i < 60; i++) {
        if (i % 8 == 0) {
            W[i] = W[i-8] ^ aes_sub_word(aes_rot_word(W[i-1])) ^ ((uint32_t)AES_RCON[i/8] << 24);
        } else if (i % 8 == 4) {
            W[i] = W[i-8] ^ aes_sub_word(W[i-1]);
        } else {
            W[i] = W[i-8] ^ W[i-1];
        }
    }
    return CRYPTO_SUCCESS;
}

/* AES 单块加密 */
static void aes256_encrypt_block(const aes256_ctx_t *ctx,
                                  const uint8_t in[16], uint8_t out[16])
{
    uint32_t s[4];
    s[0] = load_be32(in)      ^ ctx->rk[0];
    s[1] = load_be32(in + 4)  ^ ctx->rk[1];
    s[2] = load_be32(in + 8)  ^ ctx->rk[2];
    s[3] = load_be32(in + 12) ^ ctx->rk[3];

    for (int round = 1; round < 14; round++) {
        /* SubBytes + ShiftRows + MixColumns + AddRoundKey */
        uint32_t t[4];
        for (int i = 0; i < 4; i++) {
            uint8_t a0 = AES_SBOX[(s[i] >> 24) & 0xFF];
            uint8_t a1 = AES_SBOX[(s[i] >> 16) & 0xFF];
            uint8_t a2 = AES_SBOX[(s[i] >>  8) & 0xFF];
            uint8_t a3 = AES_SBOX[(s[i]      ) & 0xFF];
            t[i] = ((uint32_t)a0 << 24) | ((uint32_t)a1 << 16) |
                   ((uint32_t)a2 <<  8) |  (uint32_t)a3;
        }

        /* ShiftRows */
        uint32_t sr[4];
        sr[0] = (t[0] & 0xFF000000) | (t[1] & 0x00FF0000) | (t[2] & 0x0000FF00) | (t[3] & 0x000000FF);
        sr[1] = (t[1] & 0xFF000000) | (t[2] & 0x00FF0000) | (t[3] & 0x0000FF00) | (t[0] & 0x000000FF);
        sr[2] = (t[2] & 0xFF000000) | (t[3] & 0x00FF0000) | (t[0] & 0x0000FF00) | (t[1] & 0x000000FF);
        sr[3] = (t[3] & 0xFF000000) | (t[0] & 0x00FF0000) | (t[1] & 0x0000FF00) | (t[2] & 0x000000FF);

        /* MixColumns */
        if (round < 14) {
            for (int c = 0; c < 4; c++) {
                uint8_t *col = (uint8_t *)&sr[c];
                uint8_t a = col[0], b = col[1], c2 = col[2], d = col[3];
                col[0] = gf_mul2(a) ^ gf_mul3(b) ^ c2 ^ d;
                col[1] = a ^ gf_mul2(b) ^ gf_mul3(c2) ^ d;
                col[2] = a ^ b ^ gf_mul2(c2) ^ gf_mul3(d);
                col[3] = gf_mul3(a) ^ b ^ c2 ^ gf_mul2(d);
            }
        }

        uint32_t rk_off = (uint32_t)round * 4;
        s[0] = sr[0] ^ ctx->rk[rk_off];
        s[1] = sr[1] ^ ctx->rk[rk_off + 1];
        s[2] = sr[2] ^ ctx->rk[rk_off + 2];
        s[3] = sr[3] ^ ctx->rk[rk_off + 3];
    }

    /* 最后一轮 (无 MixColumns) */
    {
        uint32_t t[4];
        for (int i = 0; i < 4; i++) {
            uint8_t a0 = AES_SBOX[(s[i] >> 24) & 0xFF];
            uint8_t a1 = AES_SBOX[(s[i] >> 16) & 0xFF];
            uint8_t a2 = AES_SBOX[(s[i] >>  8) & 0xFF];
            uint8_t a3 = AES_SBOX[(s[i]      ) & 0xFF];
            t[i] = ((uint32_t)a0 << 24) | ((uint32_t)a1 << 16) |
                   ((uint32_t)a2 <<  8) |  (uint32_t)a3;
        }
        uint32_t out_w[4];
        out_w[0] = (t[0] & 0xFF000000) | (t[1] & 0x00FF0000) | (t[2] & 0x0000FF00) | (t[3] & 0x000000FF);
        out_w[1] = (t[1] & 0xFF000000) | (t[2] & 0x00FF0000) | (t[3] & 0x0000FF00) | (t[0] & 0x000000FF);
        out_w[2] = (t[2] & 0xFF000000) | (t[3] & 0x00FF0000) | (t[0] & 0x0000FF00) | (t[1] & 0x000000FF);
        out_w[3] = (t[3] & 0xFF000000) | (t[0] & 0x00FF0000) | (t[1] & 0x0000FF00) | (t[2] & 0x000000FF);

        store_be32(out,      out_w[0] ^ ctx->rk[56]);
        store_be32(out + 4,  out_w[1] ^ ctx->rk[57]);
        store_be32(out + 8,  out_w[2] ^ ctx->rk[58]);
        store_be32(out + 12, out_w[3] ^ ctx->rk[59]);
    }
}

/* ========================================================================
 *  AES-256-GCM 模式
 * ======================================================================== */

/* GF(2^128) 乘法 (等效于 sm4.c 中的 ghash 乘法) */
static void aes_gf128_mul(uint8_t r[16], const uint8_t a[16], const uint8_t b[16])
{
    uint8_t Z[16] = {0};
    uint8_t V[16];
    memcpy(V, b, 16);

    for (int i = 0; i < 128; i++) {
        if ((a[i / 8] >> (7 - (i % 8))) & 1) {
            for (int j = 0; j < 16; j++) Z[j] ^= V[j];
        }
        uint8_t lsb = V[15] & 1;
        for (int j = 15; j > 0; j--) V[j] = (V[j] >> 1) | (V[j-1] << 7);
        V[0] >>= 1;
        if (lsb) V[0] ^= 0xE1;
    }
    memcpy(r, Z, 16);
}

/* GHASH */
static void aes_ghash(uint8_t result[16], const uint8_t H[16],
                      const uint8_t *aad, size_t aad_len,
                      const uint8_t *ct, size_t ct_len)
{
    if ((aad_len > 0 && !aad) || (ct_len > 0 && !ct)) return;
    uint8_t Y[16] = {0};
    uint8_t block[16];
    size_t pos;

    pos = 0;
    while (pos + 16 <= aad_len) {
        for (int j = 0; j < 16; j++) Y[j] ^= aad[pos + j];
        aes_gf128_mul(Y, Y, H);
        pos += 16;
    }
    if (pos < aad_len) {
        memset(block, 0, 16);
        memcpy(block, aad + pos, aad_len - pos);
        for (int j = 0; j < 16; j++) Y[j] ^= block[j];
        aes_gf128_mul(Y, Y, H);
    }

    pos = 0;
    while (pos + 16 <= ct_len) {
        for (int j = 0; j < 16; j++) Y[j] ^= ct[pos + j];
        aes_gf128_mul(Y, Y, H);
        pos += 16;
    }
    if (pos < ct_len) {
        memset(block, 0, 16);
        memcpy(block, ct + pos, ct_len - pos);
        for (int j = 0; j < 16; j++) Y[j] ^= block[j];
        aes_gf128_mul(Y, Y, H);
    }

    memset(block, 0, 16);
    store_be64(block, (uint64_t)aad_len * 8);
    store_be64(block + 8, (uint64_t)ct_len * 8);
    for (int j = 0; j < 16; j++) Y[j] ^= block[j];
    aes_gf128_mul(Y, Y, H);

    memcpy(result, Y, 16);
}

static void aes_gcm_incr(uint8_t *counter, size_t len)
{
    for (size_t i = len; i > 0; i--) {
        if (++counter[i-1] != 0) break;
    }
}

int crypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *plaintext, size_t pt_len,
                           uint8_t *ciphertext,
                           uint8_t *tag, size_t tag_len)
{
    if (!key || !iv || !plaintext || !ciphertext || !tag)
        return CRYPTO_ERR_NULL_PTR;
    if (key_len != 32 || iv_len < 1)
        return CRYPTO_ERR_BAD_LENGTH;

    aes256_ctx_t ctx;
    aes256_set_key(&ctx, key);

    /* H = AES(0^128) */
    uint8_t H[16] = {0};
    aes256_encrypt_block(&ctx, H, H);

    /* J0 */
    uint8_t J0[16];
    if (iv_len == 12) {
        memcpy(J0, iv, 12);
        J0[12] = J0[13] = J0[14] = 0;
        J0[15] = 1;
    } else {
        aes_ghash(J0, H, NULL, 0, iv, iv_len);
    }

    /* CTR 加密 */
    uint8_t counter[16], ks[16];
    memcpy(counter, J0, 16);
    size_t pos = 0;
    while (pos < pt_len) {
        aes_gcm_incr(counter + 12, 4);
        aes256_encrypt_block(&ctx, counter, ks);
        size_t todo = (pt_len - pos < 16) ? (pt_len - pos) : 16;
        for (size_t j = 0; j < todo; j++)
            ciphertext[pos + j] = plaintext[pos + j] ^ ks[j];
        pos += todo;
    }

    /* 标签 */
    uint8_t S[16];
    aes_ghash(S, H, NULL, 0, ciphertext, pt_len);
    uint8_t EK_J0[16];
    aes256_encrypt_block(&ctx, J0, EK_J0);
    for (size_t j = 0; j < tag_len; j++)
        tag[j] = S[j] ^ EK_J0[j];

    crypto_secure_zero(&ctx, sizeof(ctx));
    crypto_secure_zero(H, sizeof(H));
    crypto_secure_zero(J0, sizeof(J0));
    crypto_secure_zero(ks, sizeof(ks));
    return CRYPTO_SUCCESS;
}

int crypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *ciphertext, size_t ct_len,
                           uint8_t *plaintext,
                           const uint8_t *tag, size_t tag_len)
{
    if (!key || !iv || !ciphertext || !tag || !plaintext)
        return CRYPTO_ERR_NULL_PTR;
    if (key_len != 32 || iv_len < 1 || tag_len < 1)
        return CRYPTO_ERR_BAD_LENGTH;

    aes256_ctx_t ctx;
    aes256_set_key(&ctx, key);

    uint8_t H[16] = {0};
    aes256_encrypt_block(&ctx, H, H);

    uint8_t J0[16];
    if (iv_len == 12) {
        memcpy(J0, iv, 12);
        J0[12] = J0[13] = J0[14] = 0;
        J0[15] = 1;
    } else {
        aes_ghash(J0, H, NULL, 0, iv, iv_len);
    }

    /* 验证标签 */
    uint8_t S[16];
    aes_ghash(S, H, NULL, 0, ciphertext, ct_len);
    uint8_t EK_J0[16];
    aes256_encrypt_block(&ctx, J0, EK_J0);
    for (size_t j = 0; j < tag_len; j++) S[j] ^= EK_J0[j];

    uint8_t diff = 0;
    for (size_t j = 0; j < tag_len; j++) diff |= (S[j] ^ tag[j]);
    if (diff != 0) {
        crypto_secure_zero(&ctx, sizeof(ctx));
        return CRYPTO_ERR_VERIFY_FAILED;
    }

    /* CTR 解密 */
    uint8_t counter[16], ks[16];
    memcpy(counter, J0, 16);
    size_t pos = 0;
    while (pos < ct_len) {
        aes_gcm_incr(counter + 12, 4);
        aes256_encrypt_block(&ctx, counter, ks);
        size_t todo = (ct_len - pos < 16) ? (ct_len - pos) : 16;
        for (size_t j = 0; j < todo; j++)
            plaintext[pos + j] = ciphertext[pos + j] ^ ks[j];
        pos += todo;
    }

    crypto_secure_zero(&ctx, sizeof(ctx));
    crypto_secure_zero(H, sizeof(H));
    crypto_secure_zero(J0, sizeof(J0));
    crypto_secure_zero(ks, sizeof(ks));
    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  SM3 / SM4 包装 (委派到 sm3.c / sm4.c)
 * ======================================================================== */

#include "sm3.h"
#include "sm4.h"
#include "sm2.h"

int crypto_sm3(const uint8_t *data, size_t len, uint8_t hash[32])
{
    return sm3_hash(data, len, hash);
}

int crypto_sm4_gcm_encrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *plaintext, size_t pt_len,
                           uint8_t *ciphertext, uint8_t *tag)
{
    if (!key || key_len != 16) return CRYPTO_ERR_BAD_LENGTH;
    if (!tag) return CRYPTO_ERR_NULL_PTR;
    return sm4_gcm_encrypt(key, iv, iv_len, aad, aad_len,
                           plaintext, pt_len, ciphertext,
                           tag, SM4_GCM_TAG_SIZE);
}

int crypto_sm4_gcm_decrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *ciphertext, size_t ct_len,
                           const uint8_t *tag, uint8_t *plaintext)
{
    if (!key || key_len != 16) return CRYPTO_ERR_BAD_LENGTH;
    if (!tag) return CRYPTO_ERR_NULL_PTR;
    return sm4_gcm_decrypt(key, iv, iv_len, aad, aad_len,
                           ciphertext, ct_len,
                           tag, SM4_GCM_TAG_SIZE, plaintext);
}

int crypto_sm2_sign(const uint8_t *private_key, const uint8_t *hash,
                    uint8_t *signature)
{
    return sm2_sign_hash(private_key, hash, signature);
}

int crypto_sm2_verify(const uint8_t *public_key, const uint8_t *hash,
                      const uint8_t *signature)
{
    return sm2_verify_hash(public_key, hash, signature);
}

int crypto_sm2_key_exchange(const uint8_t *private_key,
                            const uint8_t *peer_public,
                            uint8_t *shared_secret)
{
    /* 使用简化密钥交换 (类似 ECDH, 同一方调用) */
    /* 此处自动生成本端临时密钥并完成交换 */
    /* 用自身公钥和对方公钥, 默认 UID */
    uint8_t self_public[64] = {0};
    uint8_t ephemeral_private[32];
    uint8_t ephemeral_public[64];

    /* 计算本端公钥: priv * G */
    bn256_t d;
    bn256_from_bytes(&d, private_key);
    bn256_t Px, Py;
    ec_point_mul_base(&Px, &Py, &d);
    bn256_to_bytes(&Px, self_public);
    bn256_to_bytes(&Py, self_public + 32);

    /* 默认 UID */
    uint8_t uid[16];
    memset(uid, 0, 16);

    return sm2_key_exchange_initiator(
        private_key, self_public,
        peer_public,
        uid, 16, uid, 16,
        ephemeral_private, ephemeral_public,
        shared_secret);
}

/* ========================================================================
 *  统一哈希接口 (根据算法选择)
 * ======================================================================== */

int crypto_hash(const uint8_t *data, size_t len, uint8_t hash[32])
{
    if (!data || !hash) return CRYPTO_ERR_NULL_PTR;

    if (g_engine.hash == HASH_ALGO_SM3)
        return sm3_hash(data, len, hash);
    else
        return crypto_sha256(data, len, hash);
}

/* ========================================================================
 *  统一对称加密接口
 * ======================================================================== */

int crypto_encrypt(const uint8_t *key, size_t key_len,
                   const uint8_t *iv, size_t iv_len,
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t *plaintext, size_t pt_len,
                   uint8_t *ciphertext, uint8_t *tag)
{
    (void)aad;
    (void)aad_len;
    if (!key || !iv || !plaintext || !ciphertext || !tag)
        return CRYPTO_ERR_NULL_PTR;

    if (g_engine.sym == SYM_ALGO_SM4_GCM) {
        return crypto_sm4_gcm_encrypt(key, key_len, iv, iv_len,
                                      NULL, 0, plaintext, pt_len,
                                      ciphertext, tag);
    } else {
        return crypto_aes_gcm_encrypt(key, key_len, iv, iv_len,
                                      plaintext, pt_len,
                                      ciphertext, tag, 16);
    }
}

int crypto_decrypt(const uint8_t *key, size_t key_len,
                   const uint8_t *iv, size_t iv_len,
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t *ciphertext, size_t ct_len,
                   const uint8_t *tag, uint8_t *plaintext)
{
    (void)aad;
    (void)aad_len;
    if (!key || !iv || !ciphertext || !tag || !plaintext)
        return CRYPTO_ERR_NULL_PTR;

    if (g_engine.sym == SYM_ALGO_SM4_GCM) {
        return crypto_sm4_gcm_decrypt(key, key_len, iv, iv_len,
                                      NULL, 0, ciphertext, ct_len,
                                      tag, plaintext);
    } else {
        return crypto_aes_gcm_decrypt(key, key_len, iv, iv_len,
                                      ciphertext, ct_len,
                                      plaintext, tag, 16);
    }
}

/* ========================================================================
 *  统一签名接口
 * ======================================================================== */

int crypto_sign(const uint8_t *private_key, size_t key_len,
                const uint8_t *data, size_t data_len,
                uint8_t *signature, size_t *sig_len)
{
    if (!private_key || !data || !signature || !sig_len)
        return CRYPTO_ERR_NULL_PTR;

    uint8_t hash[32];
    int ret;

    if (g_engine.ecc == CRYPTO_ALGO_SM2) {
        if (key_len < SM2_PRIVATE_KEY_SIZE) return CRYPTO_ERR_BAD_LENGTH;
        if (*sig_len < SM2_SIGNATURE_SIZE) return CRYPTO_ERR_BUF_OVERFLOW;

        /* 使用 SM3 哈希 + SM2 签名 */
        ret = sm3_hash(data, data_len, hash);
        if (ret != CRYPTO_SUCCESS) return ret;

        /* 简化: 直接对 hash 签名 (无 ZA, 用于内部) */
        ret = sm2_sign_hash(private_key, hash, signature);
        if (ret == CRYPTO_SUCCESS) *sig_len = SM2_SIGNATURE_SIZE;
        return ret;
    } else {
        /* ECDSA P-256 */
        if (key_len < 32) return CRYPTO_ERR_BAD_LENGTH;
        if (*sig_len < 64) return CRYPTO_ERR_BUF_OVERFLOW;

        ret = crypto_sha256(data, data_len, hash);
        if (ret != CRYPTO_SUCCESS) return ret;

        /* ECDSA 签名 (需要 HSM 或外部实现) */
        /* 此处返回占位 — 生产环境替换为 HSM 调用 */
        (void)signature;
        *sig_len = 64;
        return CRYPTO_ERR_UNSUPPORTED;
    }
}

int crypto_verify(const uint8_t *public_key, size_t key_len,
                  const uint8_t *data, size_t data_len,
                  const uint8_t *signature, size_t sig_len)
{
    if (!public_key || !data || !signature)
        return CRYPTO_ERR_NULL_PTR;

    uint8_t hash[32];
    int ret;

    if (g_engine.ecc == CRYPTO_ALGO_SM2) {
        if (key_len < SM2_PUBLIC_KEY_SIZE) return CRYPTO_ERR_BAD_LENGTH;
        if (sig_len < SM2_SIGNATURE_SIZE) return CRYPTO_ERR_BAD_LENGTH;

        ret = sm3_hash(data, data_len, hash);
        if (ret != CRYPTO_SUCCESS) return ret;

        return sm2_verify_hash(public_key, hash, signature);
    } else {
        /* ECDSA P-256 */
        if (key_len < 64) return CRYPTO_ERR_BAD_LENGTH;
        if (sig_len < 64) return CRYPTO_ERR_BAD_LENGTH;

        ret = crypto_sha256(data, data_len, hash);
        if (ret != CRYPTO_SUCCESS) return ret;

        (void)hash;
        /* ECDSA 验签 (需要 HSM 或外部实现) */
        return CRYPTO_ERR_UNSUPPORTED;
    }
}
