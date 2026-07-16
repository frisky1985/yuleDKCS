/**
 * @file sm3.c
 * @brief SM3 密码杂凑算法实现
 * @version 1.0
 * @date 2026-05-28
 *
 * GB/T 32905-2016 SM3 密码杂凑算法。
 * 输出 256 位 (32 字节), 消息分组 512 位 (64 字节), 64 轮压缩函数。
 *
 * 测试向量 (摘自 GB/T 32905-2016 附录 A):
 *  消息: "abc" (0x616263)
 *  杂凑: 66C7F0F4 62EEEDD9 D1F2D46B DC10E4E2 4167C487 5CF2F7A2 297DA02B 8F4BA8E0
 *
 *  消息: "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd" (64 B)
 *  杂凑: DEBE9FF9 2275B8A1 38604889 C18E5A4D 6FDB70E5 387E5765 293DCBA3 9C0C5732
 */

#include "sm3.h"
#include "crypto_utils.h"

/* ========================================================================
 *  SM3 初始值 IV
 * ======================================================================== */
static const uint32_t SM3_IV[8] = {
    0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
    0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
};

/* ========================================================================
 *  SM3 常量 T
 * ========================================================================
 * 第 1~16 轮: T = 0x79CC4519
 * 第 17~64 轮: T = 0x7A879D8A
 */
static inline uint32_t sm3_t(uint32_t round)
{
    return (round < 16) ? 0x79CC4519 : 0x7A879D8A;
}

/* ========================================================================
 *  布尔函数
 * ========================================================================
 * FF_j(X,Y,Z):
 *   j ∈ [0,15]: FF = X ⊕ Y ⊕ Z
 *   j ∈ [16,63]: FF = (X ∧ Y) ∨ (X ∧ Z) ∨ (Y ∧ Z)
 *
 * GG_j(X,Y,Z):
 *   j ∈ [0,15]: GG = X ⊕ Y ⊕ Z
 *   j ∈ [16,63]: GG = (X ∧ Y) ∨ (¬X ∧ Z)
 */
static inline uint32_t ff(uint32_t x, uint32_t y, uint32_t z, uint32_t round)
{
    if (round < 16)
        return x ^ y ^ z;
    return (x & y) | (x & z) | (y & z);
}

static inline uint32_t gg(uint32_t x, uint32_t y, uint32_t z, uint32_t round)
{
    if (round < 16)
        return x ^ y ^ z;
    return (x & y) | ((~x) & z);
}

/* ========================================================================
 *  置换函数
 * ========================================================================
 * P_0(X) = X ⊕ (X ≪ 9) ⊕ (X ≪ 17)
 * P_1(X) = X ⊕ (X ≪ 15) ⊕ (X ≪ 23)
 */
static inline uint32_t p0(uint32_t x)
{
    return x ^ rotl32(x, 9) ^ rotl32(x, 17);
}

static inline uint32_t p1(uint32_t x)
{
    return x ^ rotl32(x, 15) ^ rotl32(x, 23);
}

/* ========================================================================
 *  消息扩展: 16 个 W 字扩展为 68 + 64 个 W' 字
 * ========================================================================
 * W_j  = M_j                                (j = 0..15)
 * W_j  = P_1(W_{j-16} ⊕ W_{j-9} ⊕ (W_{j-3} ≪ 15))
 *        ⊕ (W_{j-13} ≪ 7) ⊕ W_{j-6}        (j = 16..67)
 * W'_j = W_j ⊕ W_{j+4}                     (j = 0..63)
 */
static void sm3_expand(uint32_t W[68], uint32_t Wp[64], const uint8_t block[64])
{
    int j;

    /* W[0..15] = 消息分组直接加载 (大端) */
    for (j = 0; j < 16; j++) {
        W[j] = load_be32(block + j * 4);
    }

    /* W[16..67] 扩展 */
    for (j = 16; j < 68; j++) {
        W[j] = p1(W[j-16] ^ W[j-9] ^ rotl32(W[j-3], 15))
             ^ rotl32(W[j-13], 7)
             ^ W[j-6];
    }

    /* W'[0..63] = W[j] ⊕ W[j+4] */
    for (j = 0; j < 64; j++) {
        Wp[j] = W[j] ^ W[j+4];
    }
}

/* ========================================================================
 *  压缩函数 CF
 * ========================================================================
 * 输入: V (8 × 32), 消息分组扩展后 W/W'
 * 输出: 更新后的 V
 *
 * SS1 ≪ 12
 * 算法:
 *   SS1 = ((A ≪ 12) + E + (T_j ≪ (j mod 32))) ≪ 7
 *   SS2 = SS1 ⊕ (A ≪ 12)
 *   TT1 = FF_j(A,B,C) + D + SS2 + W'_j
 *   TT2 = GG_j(E,F,G) + H + SS1 + W_j
 *   D   = C
 *   C   = B ≪ 9
 *   B   = A
 *   A   = TT1
 *   H   = G
 *   G   = F ≪ 19
 *   F   = E
 *   E   = P_0(TT2)
 */
static void sm3_compress(uint32_t V[8], const uint32_t W[68], const uint32_t Wp[64])
{
    uint32_t A = V[0], B = V[1], C = V[2], D = V[3];
    uint32_t E = V[4], F = V[5], G = V[6], H = V[7];
    uint32_t SS1, SS2, TT1, TT2;

    for (int j = 0; j < 64; j++) {
        SS1 = rotl32(rotl32(A, 12) + E + rotl32(sm3_t(j), j % 32), 7);
        SS2 = SS1 ^ rotl32(A, 12);
        TT1 = ff(A, B, C, j) + D + SS2 + Wp[j];
        TT2 = gg(E, F, G, j) + H + SS1 + W[j];
        D   = C;
        C   = rotl32(B, 9);
        B   = A;
        A   = TT1;
        H   = G;
        G   = rotl32(F, 19);
        F   = E;
        E   = p0(TT2);
    }

    V[0] ^= A;
    V[1] ^= B;
    V[2] ^= C;
    V[3] ^= D;
    V[4] ^= E;
    V[5] ^= F;
    V[6] ^= G;
    V[7] ^= H;
}

/* ========================================================================
 *  核心处理: 处理一个 64 字节消息分组
 * ======================================================================== */
static void sm3_process_block(sm3_ctx_t *ctx)
{
    uint32_t W[68], Wp[64];
    sm3_expand(W, Wp, ctx->block);
    sm3_compress(ctx->state, W, Wp);
}

/* ========================================================================
 *  公开 API
 * ======================================================================== */

int sm3_init(sm3_ctx_t *ctx)
{
    if (!ctx) return CRYPTO_ERR_NULL_PTR;

    ctx->total_bits = 0;
    ctx->block_len  = 0;
    for (int i = 0; i < 8; i++) {
        ctx->state[i] = SM3_IV[i];
    }
    return CRYPTO_SUCCESS;
}

int sm3_update(sm3_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || (!data && len > 0)) return CRYPTO_ERR_NULL_PTR;
    if (len == 0) return CRYPTO_SUCCESS;

    ctx->total_bits += (uint64_t)len * 8;

    /* 填充缓冲区 */
    while (len > 0) {
        size_t space = SM3_BLOCK_SIZE - ctx->block_len;
        size_t copy  = (len < space) ? len : space;

        memcpy(ctx->block + ctx->block_len, data, copy);
        ctx->block_len += (uint32_t)copy;
        data += copy;
        len  -= copy;

        if (ctx->block_len == SM3_BLOCK_SIZE) {
            sm3_process_block(ctx);
            ctx->block_len = 0;
        }
    }
    return CRYPTO_SUCCESS;
}

int sm3_final(sm3_ctx_t *ctx, uint8_t hash[SM3_DIGEST_SIZE])
{
    if (!ctx || !hash) return CRYPTO_ERR_NULL_PTR;

    /* 保存原始消息长度 (填充不应计入长度字段) */
    uint64_t orig_bits = ctx->total_bits;

    /* 填充: 先补 0x80, 再补 0x00 直到剩余 8 字节存长度 */
    uint8_t pad = 0x80;
    sm3_update(ctx, &pad, 1);

    /* 补 0 直到 block 内剩余 8 字节 */
    while (ctx->block_len != (SM3_BLOCK_SIZE - 8)) {
        uint8_t zero = 0;
        sm3_update(ctx, &zero, 1);
    }

    /* 写入原始消息总位数 (大端 64 位) */
    uint8_t bits[8];
    store_be64(bits, orig_bits);
    sm3_update(ctx, bits, 8);

    /* 输出 state → hash */
    for (int i = 0; i < 8; i++) {
        store_be32(hash + i * 4, ctx->state[i]);
    }

    /* 安全清理 */
    crypto_secure_zero(ctx, sizeof(sm3_ctx_t));
    return CRYPTO_SUCCESS;
}

int sm3_hash(const uint8_t *data, size_t len, uint8_t hash[SM3_DIGEST_SIZE])
{
    sm3_ctx_t ctx;
    int ret;

    ret = sm3_init(&ctx);
    if (ret != CRYPTO_SUCCESS) return ret;

    ret = sm3_update(&ctx, data, len);
    if (ret != CRYPTO_SUCCESS) return ret;

    return sm3_final(&ctx, hash);
}

/* ========================================================================
 *  SM3-HMAC (RFC 2104 风格)
 * ======================================================================== */
int sm3_hmac(const uint8_t *key, size_t klen,
             const uint8_t *data, size_t dlen,
             uint8_t mac[SM3_DIGEST_SIZE])
{
    if (!key || !data || !mac) return CRYPTO_ERR_NULL_PTR;

    sm3_ctx_t ctx;
    uint8_t k_ipad[SM3_BLOCK_SIZE];
    uint8_t k_opad[SM3_BLOCK_SIZE];
    uint8_t tmp_hash[SM3_DIGEST_SIZE];
    uint8_t effective_key[SM3_BLOCK_SIZE];
    size_t  effective_klen;

    /* 密钥长于分组则先哈希 */
    if (klen > SM3_BLOCK_SIZE) {
        sm3_hash(key, klen, effective_key);
        effective_klen = SM3_DIGEST_SIZE;
    } else {
        if (klen > 0) memcpy(effective_key, key, klen);
        effective_klen = klen;
    }

    /* 补齐到分组长度 */
    if (effective_klen < SM3_BLOCK_SIZE) {
        memset(effective_key + effective_klen, 0, SM3_BLOCK_SIZE - effective_klen);
    }

    /* ipad = key ⊕ 0x36 */
    for (size_t i = 0; i < SM3_BLOCK_SIZE; i++) {
        k_ipad[i] = effective_key[i] ^ 0x36;
    }

    /* opad = key ⊕ 0x5C */
    for (size_t i = 0; i < SM3_BLOCK_SIZE; i++) {
        k_opad[i] = effective_key[i] ^ 0x5C;
    }

    /* H(k_ipad || message) */
    sm3_init(&ctx);
    sm3_update(&ctx, k_ipad, SM3_BLOCK_SIZE);
    sm3_update(&ctx, data, dlen);
    sm3_final(&ctx, tmp_hash);

    /* H(k_opad || H(k_ipad || message)) */
    sm3_init(&ctx);
    sm3_update(&ctx, k_opad, SM3_BLOCK_SIZE);
    sm3_update(&ctx, tmp_hash, SM3_DIGEST_SIZE);
    sm3_final(&ctx, mac);

    crypto_secure_zero(k_ipad, sizeof(k_ipad));
    crypto_secure_zero(k_opad, sizeof(k_opad));
    crypto_secure_zero(effective_key, sizeof(effective_key));
    crypto_secure_zero(tmp_hash, sizeof(tmp_hash));

    return CRYPTO_SUCCESS;
}
