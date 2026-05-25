/******************************************************************************
 * @file    sm4.c
 * @brief   SM4 分组密码算法实现 (GB/T 32907-2016)
 * @note    128-bit 密钥, 32 轮 Feistel 网络
 *          纯 C 实现，无外部依赖
 ******************************************************************************/
#include "sm4.h"
#include <string.h>

/* SM4 S 盒 */
static const uint8_t SM4_SBOX[256] = {
    0xD6, 0x90, 0xE9, 0xFE, 0xCC, 0xE1, 0x3D, 0xB7,
    0x16, 0xB6, 0x14, 0xC2, 0x28, 0xFB, 0x2C, 0x05,
    0x2B, 0x67, 0x9A, 0x76, 0x2A, 0xBE, 0x04, 0xC3,
    0xAA, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
    0x9C, 0x42, 0x50, 0xF4, 0x91, 0xEF, 0x98, 0x7A,
    0x33, 0x54, 0x0B, 0x43, 0xED, 0xCF, 0xAC, 0x62,
    0xE4, 0xB3, 0x1C, 0xA9, 0xC9, 0x08, 0xE8, 0x95,
    0x80, 0xDF, 0x94, 0xFA, 0x75, 0x8F, 0x3F, 0xA6,
    0x47, 0x07, 0xA7, 0xFC, 0xF3, 0x73, 0x17, 0xBA,
    0x83, 0x59, 0x3C, 0x19, 0xE6, 0x85, 0x4F, 0xA8,
    0x68, 0x6B, 0x81, 0xB2, 0x71, 0x64, 0xDA, 0x8B,
    0xF8, 0xEB, 0x0F, 0x4B, 0x70, 0x56, 0x9D, 0x35,
    0x1E, 0x24, 0x0E, 0x5E, 0x63, 0x58, 0xD1, 0xA2,
    0x25, 0x22, 0x7C, 0x3B, 0x01, 0x21, 0x78, 0x87,
    0xD4, 0x00, 0x46, 0x57, 0x9F, 0xD3, 0x27, 0x52,
    0x4C, 0x36, 0x02, 0xE7, 0xA0, 0xC4, 0xC8, 0x9E,
    0xEA, 0xBF, 0x8A, 0xD2, 0x40, 0xC7, 0x38, 0xB5,
    0xA3, 0xF7, 0xF2, 0xCE, 0xF9, 0x61, 0x15, 0xA1,
    0xE0, 0xAE, 0x5D, 0xA4, 0x9B, 0x34, 0x1A, 0x55,
    0xAD, 0x93, 0x32, 0x30, 0xF5, 0x8C, 0xB1, 0xE3,
    0x1D, 0xF6, 0xE2, 0x2E, 0x82, 0x66, 0xCA, 0x60,
    0xC0, 0x29, 0x23, 0xAB, 0x0D, 0x53, 0x4E, 0x6F,
    0xD5, 0xDB, 0x37, 0x45, 0xDE, 0xFD, 0x8E, 0x2F,
    0x03, 0xFF, 0x6A, 0x72, 0x6D, 0x6C, 0x5B, 0x51,
    0x8D, 0x1B, 0xAF, 0x92, 0xBB, 0xDD, 0xBC, 0x7F,
    0x11, 0xD9, 0x5C, 0x41, 0x1F, 0x10, 0x5A, 0xD8,
    0x0A, 0xC1, 0x31, 0x88, 0xA5, 0xCD, 0x7B, 0xBD,
    0x2D, 0x74, 0xD0, 0x12, 0xB8, 0xE5, 0xB4, 0xB0,
    0x89, 0x69, 0x97, 0x4A, 0x0C, 0x96, 0x77, 0x7E,
    0x65, 0xB9, 0xF1, 0x09, 0xC5, 0x6E, 0xC6, 0x84,
    0x18, 0xF0, 0x7D, 0xEC, 0x3A, 0xDC, 0x4D, 0x20,
    0x79, 0xEE, 0x5F, 0x3E, 0xD7, 0xCB, 0x39, 0x48
};

/* 固定密钥 CK */
static const uint32_t SM4_CK[32] = {
    0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269,
    0x70777E85, 0x8C939AA1, 0xA8AFB6BD, 0xC4CBD2D9,
    0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249,
    0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9,
    0xC0C7CED5, 0xDCE3EAF1, 0xF8FF060D, 0x141B2229,
    0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299,
    0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209,
    0x10171E25, 0x2C333A41, 0x484F565D, 0x646B7279
};

/* 循环左移 */
static inline uint32_t rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

/* 非线性变换 τ: 4 个 S 盒并行 */
static inline uint32_t sm4_tau(uint32_t a)
{
    uint8_t b[4];
    b[0] = SM4_SBOX[(a >> 24) & 0xFF];
    b[1] = SM4_SBOX[(a >> 16) & 0xFF];
    b[2] = SM4_SBOX[(a >> 8) & 0xFF];
    b[3] = SM4_SBOX[a & 0xFF];
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

/* 线性变换 L */
static inline uint32_t sm4_l(uint32_t b)
{
    return b ^ rotl32(b, 2) ^ rotl32(b, 10) ^
           rotl32(b, 18) ^ rotl32(b, 24);
}

/* 密钥扩展线性变换 L' */
static inline uint32_t sm4_l_prime(uint32_t b)
{
    return b ^ rotl32(b, 13) ^ rotl32(b, 23);
}

/* 轮函数 F */
static inline uint32_t sm4_f(uint32_t x0, uint32_t x1,
                             uint32_t x2, uint32_t x3, uint32_t rk)
{
    return x0 ^ sm4_l(sm4_tau(x1 ^ x2 ^ x3 ^ rk));
}

/* 字节序加载 */
static inline uint32_t load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static inline void store_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

/* 密钥扩展 */
static void sm4_key_expand(const uint8_t key[16], uint32_t rk[32])
{
    uint32_t k[36];
    const uint32_t fk[4] = {
        0xA3B1BAC6, 0x56AA3350, 0x677D9197, 0xB27022DC
    };

    k[0] = load_be32(key + 0) ^ fk[0];
    k[1] = load_be32(key + 4) ^ fk[1];
    k[2] = load_be32(key + 8) ^ fk[2];
    k[3] = load_be32(key + 12) ^ fk[3];

    for (int i = 0; i < 32; i++) {
        k[i + 4] = k[i] ^ sm4_l_prime(sm4_tau(k[i + 1] ^ k[i + 2] ^ k[i + 3] ^ SM4_CK[i]));
        rk[i] = k[i + 4];
    }
}

/* 单块加密/解密 */
static void sm4_crypt_block(const uint32_t rk[32], const uint8_t in[16],
                            uint8_t out[16])
{
    uint32_t x[36];
    x[0] = load_be32(in + 0);
    x[1] = load_be32(in + 4);
    x[2] = load_be32(in + 8);
    x[3] = load_be32(in + 12);

    for (int i = 0; i < 32; i++) {
        x[i + 4] = sm4_f(x[i], x[i + 1], x[i + 2], x[i + 3], rk[i]);
    }

    store_be32(out + 0, x[35]);
    store_be32(out + 4, x[34]);
    store_be32(out + 8, x[33]);
    store_be32(out + 12, x[32]);
}

int sm4_init(sm4_context_t *ctx, const uint8_t *key, size_t key_len,
             sm4_mode_t mode, const uint8_t *iv)
{
    if (!ctx || !key || key_len != SM4_KEY_SIZE) return -1;
    if (mode == SM4_MODE_CBC && !iv) return -1;

    sm4_key_expand(key, ctx->rk);
    ctx->mode = mode;
    if (mode == SM4_MODE_CBC) {
        memcpy(ctx->iv, iv, SM4_BLOCK_SIZE);
    }
    return 0;
}

int sm4_encrypt(sm4_context_t *ctx, const uint8_t *in,
                uint8_t *out, size_t len)
{
    if (!ctx || !in || !out || len == 0 || len % SM4_BLOCK_SIZE != 0)
        return -1;

    uint8_t block[SM4_BLOCK_SIZE];

    if (ctx->mode == SM4_MODE_CBC) {
        for (size_t i = 0; i < len; i += SM4_BLOCK_SIZE) {
            for (int j = 0; j < SM4_BLOCK_SIZE; j++) {
                block[j] = in[i + j] ^ ctx->iv[j];
            }
            sm4_crypt_block(ctx->rk, block, out + i);
            memcpy(ctx->iv, out + i, SM4_BLOCK_SIZE);
        }
    } else {
        for (size_t i = 0; i < len; i += SM4_BLOCK_SIZE) {
            sm4_crypt_block(ctx->rk, in + i, out + i);
        }
    }

    return 0;
}

int sm4_decrypt(sm4_context_t *ctx, const uint8_t *in,
                uint8_t *out, size_t len)
{
    if (!ctx || !in || !out || len == 0 || len % SM4_BLOCK_SIZE != 0)
        return -1;

    uint32_t dec_rk[32];

    /* 解密使用逆序轮密钥 */
    for (int i = 0; i < 32; i++) {
        dec_rk[i] = ctx->rk[31 - i];
    }

    if (ctx->mode == SM4_MODE_CBC) {
        uint8_t block[SM4_BLOCK_SIZE];
        for (size_t i = 0; i < len; i += SM4_BLOCK_SIZE) {
            sm4_crypt_block(dec_rk, in + i, block);
            for (int j = 0; j < SM4_BLOCK_SIZE; j++) {
                out[i + j] = block[j] ^ ctx->iv[j];
            }
            memcpy(ctx->iv, in + i, SM4_BLOCK_SIZE);
        }
    } else {
        for (size_t i = 0; i < len; i += SM4_BLOCK_SIZE) {
            sm4_crypt_block(dec_rk, in + i, out + i);
        }
    }

    return 0;
}

int sm4_cbc_encrypt_pkcs7(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t *out_len)
{
    if (!key || !iv || !in || !out || !out_len) return -1;

    size_t blocks = (in_len / SM4_BLOCK_SIZE) + 1;
    size_t padded_len = blocks * SM4_BLOCK_SIZE;
    uint8_t pad_val = (uint8_t)(SM4_BLOCK_SIZE - (in_len % SM4_BLOCK_SIZE));
    uint8_t *padded = (uint8_t *)calloc(padded_len, 1);
    if (!padded) return -1;

    memcpy(padded, in, in_len);
    memset(padded + in_len, pad_val, pad_val);

    sm4_context_t ctx;
    sm4_init(&ctx, key, SM4_KEY_SIZE, SM4_MODE_CBC, iv);
    int ret = sm4_encrypt(&ctx, padded, out, padded_len);

    *out_len = padded_len;
    memset(padded, 0, padded_len);
    free(padded);
    return ret;
}

int sm4_cbc_decrypt_pkcs7(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t *out_len)
{
    if (!key || !iv || !in || !out || !out_len) return -1;
    if (in_len == 0 || in_len % SM4_BLOCK_SIZE != 0) return -1;

    sm4_context_t ctx;
    sm4_init(&ctx, key, SM4_KEY_SIZE, SM4_MODE_CBC, iv);
    int ret = sm4_decrypt(&ctx, in, out, in_len);
    if (ret != 0) return ret;

    /* 移除 PKCS7 填充 */
    uint8_t pad_val = out[in_len - 1];
    if (pad_val == 0 || pad_val > SM4_BLOCK_SIZE) return -1;
    for (size_t i = in_len - pad_val; i < in_len; i++) {
        if (out[i] != pad_val) return -1;
    }
    *out_len = in_len - pad_val;
    return 0;
}
