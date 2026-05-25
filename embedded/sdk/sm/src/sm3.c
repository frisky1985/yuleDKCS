/******************************************************************************
 * @file    sm3.c
 * @brief   SM3 密码杂凑算法 (GB/T 32905-2016 / ISO/IEC 10118-3)
 * @note    纯 C 实现，遵循 SM3 标准规范
 *          使用 32-bit 大端字节序
 ******************************************************************************/
#include "sm3.h"
#include <string.h>

/* SM3 初始值 IV */
static const uint32_t SM3_IV[8] = {
    0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
    0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
};

/* SM3 常量 Tj */
static const uint32_t SM3_TJ(int j)
{
    if (j < 16) return 0x79CC4519;
    return 0x7A879D8A;
}

/* 32 位循环左移 */
static inline uint32_t rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

/* FF 函数 */
static inline uint32_t ff(int j, uint32_t x, uint32_t y, uint32_t z)
{
    if (j < 16) return x ^ y ^ z;
    return (x & y) | (x & z) | (y & z);
}

/* GG 函数 */
static inline uint32_t gg(int j, uint32_t x, uint32_t y, uint32_t z)
{
    if (j < 16) return x ^ y ^ z;
    return (x & y) | ((~x) & z);
}

/* P0 置换 */
static inline uint32_t p0(uint32_t x)
{
    return x ^ rotl32(x, 9) ^ rotl32(x, 17);
}

/* P1 置换 */
static inline uint32_t p1(uint32_t x)
{
    return x ^ rotl32(x, 15) ^ rotl32(x, 23);
}

/* 将 4 字节数据按大端读入 uint32 */
static inline uint32_t load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

/* 将 uint32 按大端写入 4 字节 */
static inline void store_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

/* 消息扩展: 从 W[0..15] 计算 W[16..67] 和 W'[0..63] */
static void sm3_message_expand(const uint32_t w[16], uint32_t w_ext[68],
                                uint32_t w_pri[64])
{
    int j;
    for (j = 0; j < 16; j++) {
        w_ext[j] = w[j];
    }
    for (j = 16; j < 68; j++) {
        w_ext[j] = p1(w_ext[j - 16] ^ w_ext[j - 9] ^ rotl32(w_ext[j - 3], 15))
                   ^ rotl32(w_ext[j - 13], 7) ^ w_ext[j - 6];
    }
    for (j = 0; j < 64; j++) {
        w_pri[j] = w_ext[j] ^ w_ext[j + 4];
    }
}

/* SM3 压缩函数 */
static void sm3_compress(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[16];
    uint32_t w_ext[68], w_pri[64];
    uint32_t ss1, ss2, tt1, tt2;
    uint32_t a, b, c, d, e, f, g, h;
    int j;

    /* 加载消息块到 W[0..15] */
    for (j = 0; j < 16; j++) {
        w[j] = load_be32(block + j * 4);
    }

    /* 消息扩展 */
    sm3_message_expand(w, w_ext, w_pri);

    /* 初始化工作变量 */
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    /* 64 轮压缩 */
    for (j = 0; j < 64; j++) {
        ss1 = rotl32(rotl32(a, 12) + e + rotl32(SM3_TJ(j), j), 7);
        ss2 = ss1 ^ rotl32(a, 12);
        tt1 = ff(j, a, b, c) + d + ss2 + w_pri[j];
        tt2 = gg(j, e, f, g) + h + ss1 + w_ext[j];
        d = c;
        c = rotl32(b, 9);
        b = a;
        a = tt1;
        h = g;
        g = rotl32(f, 19);
        f = e;
        e = p0(tt2);
    }

    /* 更新状态 */
    state[0] ^= a; state[1] ^= b; state[2] ^= c; state[3] ^= d;
    state[4] ^= e; state[5] ^= f; state[6] ^= g; state[7] ^= h;
}

void sm3_init(sm3_context_t *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(sm3_context_t));
    memcpy(ctx->state, SM3_IV, sizeof(SM3_IV));
}

void sm3_update(sm3_context_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0) return;

    size_t fill;
    size_t left = (size_t)(ctx->count % SM3_BLOCK_SIZE);

    ctx->count += len;

    if (left > 0) {
        fill = SM3_BLOCK_SIZE - left;
        if (len < fill) {
            memcpy(ctx->buffer + left, data, len);
            return;
        }
        memcpy(ctx->buffer + left, data, fill);
        sm3_compress(ctx->state, ctx->buffer);
        data += fill;
        len -= fill;
    }

    while (len >= SM3_BLOCK_SIZE) {
        sm3_compress(ctx->state, data);
        data += SM3_BLOCK_SIZE;
        len -= SM3_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

void sm3_finish(sm3_context_t *ctx, uint8_t *digest)
{
    if (!ctx || !digest) return;

    uint64_t bit_count = ctx->count * 8;
    size_t left = (size_t)(ctx->count % SM3_BLOCK_SIZE);
    size_t pad_len;

    /* 填充: 先加 0x80 */
    ctx->buffer[left] = 0x80;

    if (left < 56) {
        pad_len = 56 - left;
        memset(ctx->buffer + left + 1, 0, pad_len - 1);
    } else {
        pad_len = SM3_BLOCK_SIZE - left + 56;
        memset(ctx->buffer + left + 1, 0, SM3_BLOCK_SIZE - left - 1);
        sm3_compress(ctx->state, ctx->buffer);
        memset(ctx->buffer, 0, 56);
    }

    /* 追加长度 (大端 64-bit) */
    ctx->buffer[56] = (uint8_t)(bit_count >> 56);
    ctx->buffer[57] = (uint8_t)(bit_count >> 48);
    ctx->buffer[58] = (uint8_t)(bit_count >> 40);
    ctx->buffer[59] = (uint8_t)(bit_count >> 32);
    ctx->buffer[60] = (uint8_t)(bit_count >> 24);
    ctx->buffer[61] = (uint8_t)(bit_count >> 16);
    ctx->buffer[62] = (uint8_t)(bit_count >> 8);
    ctx->buffer[63] = (uint8_t)(bit_count);

    sm3_compress(ctx->state, ctx->buffer);

    /* 输出摘要 */
    for (int i = 0; i < 8; i++) {
        store_be32(digest + i * 4, ctx->state[i]);
    }

    sm3_init(ctx); /* 安全清理 */
}

void sm3_digest(const uint8_t *data, size_t len, uint8_t *digest)
{
    sm3_context_t ctx;
    sm3_init(&ctx);
    sm3_update(&ctx, data, len);
    sm3_finish(&ctx, digest);
}
