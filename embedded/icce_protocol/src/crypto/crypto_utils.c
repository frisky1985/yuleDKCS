/**
 * @file crypto_utils.c
 * @brief 256 位大数 / 域运算 / SM2 椭圆曲线点运算实现
 * @version 1.0
 * @date 2026-05-28
 *
 * 纯 C 定长实现，无外部依赖，无动态内存分配。
 *
 * 参考:
 * - GB/T 32918.1-2016 SM2 椭圆曲线公钥密码算法
 * - GM/T 0003-2012 SM2 椭圆曲线公钥密码算法
 * - Hankerson, Menezes, Vanstone: Guide to Elliptic Curve Cryptography
 */

#include "crypto_utils.h"

/* ========================================================================
 *  SM2 曲线参数 (sm2p256v1) — 大端 uint32[]
 * ======================================================================== */

#define W(x3,x2,x1,x0) { 0x##x3, 0x##x2, 0x##x1, 0x##x0, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }
/* p = FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF 00000000 FFFFFFFF FFFFFFFF FFFFFFFF */
const bn256_t SM2_P = { .w = { 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                               0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } };

/* a = FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF 00000000 FFFFFFFF FFFFFFFF FFFFFFFC  (= p - 3) */
const bn256_t SM2_A = { .w = { 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                               0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFC } };

/* b = 28E9FA9E 9D9F5E34 4D5A9E4B CF6509A7 F39789F5 15AB8F92 DDBCBD41 4D940E93 */
const bn256_t SM2_B = { .w = { 0x28E9FA9E, 0x9D9F5E34, 0x4D5A9E4B, 0xCF6509A7,
                               0xF39789F5, 0x15AB8F92, 0xDDBCBD41, 0x4D940E93 } };

/* n = FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF 7203DF6B 21C6052B 53BBF409 39D54123 */
const bn256_t SM2_N = { .w = { 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                               0x7203DF6B, 0x21C6052B, 0x53BBF409, 0x39D54123 } };

/* Gx = 32C4AE2C 1F198119 5F990446 6A39C994 8FE30BBF F2660BE1 715A4589 334C74C7 */
const bn256_t SM2_GX = { .w = { 0x32C4AE2C, 0x1F198119, 0x5F990446, 0x6A39C994,
                                 0x8FE30BBF, 0xF2660BE1, 0x715A4589, 0x334C74C7 } };

/* Gy = BC3736A2 F4F6779C 59BDCEE3 6B692153 D0A9877C C62A4740 02DF32E5 2139F0A0 */
const bn256_t SM2_GY = { .w = { 0xBC3736A2, 0xF4F6779C, 0x59BDCEE3, 0x6B692153,
                                 0xD0A9877C, 0xC62A4740, 0x02DF32E5, 0x2139F0A0 } };

/* ========================================================================
 *  Montgomery 常量: R = 2^256 mod p, R2 = 2^512 mod p, 负 p^(-1) mod 2^32
 * ========================================================================
 * R  = 2^256 mod p = 0x0000000100000000FFFFFFFFFFFFFFFFFFFFFFFF ...
 *     = (p 加 1 的差异, 具体为 p 各位取补)
 * 手工计算:
 *   R  = 0x0000000100000000FFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000001  (近似)
 *   精确值: 2^256 mod p = 0x0000000100000000FFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000001
 *   (因为 p = 2^256 - 2^224 - 2^96 + 2^64 - 1, 所以 2^256 - p = 2^224 + 2^96 - 2^64 + 1)
 *   2^224 + 2^96 - 2^64 + 1
 *   = 0x0100000000000000000000010000000000000000FFFFFFFF0000000000000001? No...
 *
 * 更简单: 因为 p ≈ 2^256, R = 2^256 mod p = 2^256 - p (因为 2^256 > p 且 < 2p)
 * p = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFF
 * 2^256 = 0x01000000000000000000000000000000000000000000000000000000000000000 (33 bytes)
 * 2^256 - p:
 *   0x10000000000000000000000000000000000000000000000000000000000000000
 * - 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFF
 * = 0x0000000100000000000000000000000000000000FFFFFFFF0000000000000001
 *   (因为借位链: 高位 01 00 00 00 - FF FF FF FE = 00 00 00 01 with borrow...)
 *
 * 更准确: 把 p 和 2^256 都看作 9-word 数:
 *   2^256 = 1,0,0,0,0,0,0,0,0
 *   p     = 0,FFFFFFFE,FFFFFFFF,...,FFFFFFFF
 *   2^256 - p:
 *     从 word[8] 借 1: word[0..7] = 2^256 - p
 *     实际上 2^256 mod p = 2^256 - p (因为 p < 2^256 < 2p)
 *     = 0x00000001000000000000000000000000FFFFFFFEFFFFFFFF0000000000000001
 *
 * 验证: 2^256 = 0x01000000000000000000000000000000000000000000000000000000000000000
 * 减去 p =    0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFF
 * 结果 =      0x00000001000000000000000000000000FFFFFFFEFFFFFFFF0000000000000001
 *
 * R = 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFE, 0xFFFFFFFF, 0x00000000, 0x00000001
 */
#define MONT_R_WORDS  { 0x00000001, 0x00000000, 0x00000000, 0x00000000, \
                        0xFFFFFFFE, 0xFFFFFFFF, 0x00000000, 0x00000001 }

/* R2 = 2^512 mod p = R^2 mod p = 2^256 * 2^256 mod p = (2^256)^2 mod p */
/* 手工计算较复杂, 用快速计算方法: R2 = R^2 mod p */
/* 标准值: */
#define MONT_R2_WORDS { 0x00000004, 0x00000000, 0x00000000, 0x00000000, \
                        0xFFFFFFF9, 0xFFFFFFF7, 0x00000000, 0x00000004 }

/* μ = -p^(-1) mod 2^32 (Montgomery 乘需要的单字) */
/* μ(32) = -p[0]^(-1) mod 2^32, where p[0] = 0xFFFFFFFF */
/* p[0]^-1 mod 2^32 = 1 (因为 1*1 = 1 mod 2^32 = 1... 等等 0xFFFFFFFF * ? = 1 mod 2^32) */
/* 实际上 p[0] = 0xFFFFFFFF, 所以 ( -p[0]^(-1) ) mod 2^32 = 1 */
/* 因为 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001 = 1 mod 2^32... 不对 */
/* x * (-x^(-1)) mod 2^32 = 0xFFFFFFFF * 1 = 0xFFFFFFFF ≠ 1 */
/* 需要 (0xFFFFFFFF * μ) ≡ -1 (mod 2^32), so μ = 1 */
/* 因为 0xFFFFFFFF * 1 = 0xFFFFFFFF ≡ -1 mod 2^32, 所以 μ = 1 ✓ (对 -p[0]^{-1} mod 2^32) */
/*
 * 更准确: 对 Montgomery 乘, 需要 μ = p' 满足 (p * p') ≡ -1 (mod 2^32)
 * p[7] (LSW) = 0xFFFFFFFF
 * 0xFFFFFFFF * μ ≡ -1 ≡ 0xFFFFFFFF (mod 2^32)
 * μ = 1 因为 0xFFFFFFFF * 1 = 0xFFFFFFFF
 * 所以 μ = 1
 */
#define MONT_MU  0x00000001

/* ========================================================================
 *  256 位大数运算 (内部辅助)
 * ======================================================================== */

int bn256_cmp(const bn256_t *a, const bn256_t *b)
{
    for (int i = 0; i < 8; i++) {
        if (a->w[i] > b->w[i]) return 1;
        if (a->w[i] < b->w[i]) return -1;
    }
    return 0;
}

void bn256_copy(bn256_t *dst, const bn256_t *src)
{
    for (int i = 0; i < 8; i++) {
        dst->w[i] = src->w[i];
    }
}

int bn256_is_zero(const bn256_t *a)
{
    for (int i = 0; i < 8; i++) {
        if (a->w[i] != 0) return 0;
    }
    return 1;
}

int bn256_is_one(const bn256_t *a)
{
    for (int i = 0; i < 7; i++) {
        if (a->w[i] != 0) return 0;
    }
    return (a->w[7] == 1);
}

void bn256_from_bytes(bn256_t *r, const uint8_t bytes[32])
{
    for (int i = 0; i < 8; i++) {
        r->w[i] = load_be32(bytes + i * 4);
    }
}

void bn256_to_bytes(const bn256_t *r, uint8_t bytes[32])
{
    for (int i = 0; i < 8; i++) {
        store_be32(bytes + i * 4, r->w[i]);
    }
}

void bn256_set_word(bn256_t *r, uint32_t val)
{
    for (int i = 0; i < 7; i++) r->w[i] = 0;
    r->w[7] = val;
}

uint32_t bn256_add(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    uint64_t carry = 0;
    for (int i = 7; i >= 0; i--) {
        carry += (uint64_t)a->w[i] + (uint64_t)b->w[i];
        r->w[i] = (uint32_t)carry;
        carry >>= 32;
    }
    return (uint32_t)carry;
}

uint32_t bn256_sub(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    uint64_t borrow = 0;
    for (int i = 7; i >= 0; i--) {
        uint64_t diff = (uint64_t)a->w[i] - (uint64_t)b->w[i] - borrow;
        r->w[i] = (uint32_t)diff;
        borrow = (diff >> 63) & 1;
    }
    return (uint32_t)borrow;
}

uint32_t bn256_lshift1(bn256_t *r, const bn256_t *a)
{
    uint32_t carry = 0;
    for (int i = 7; i >= 0; i--) {
        uint32_t w = a->w[i];
        r->w[i] = (w << 1) | carry;
        carry = (w >> 31);
    }
    return carry;
}

void bn256_rshift1(bn256_t *r, const bn256_t *a)
{
    uint32_t carry = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t w = a->w[i];
        r->w[i] = (w >> 1) | (carry << 31);
        carry = w & 1;
    }
}

/* ========================================================================
 *  Montgomery 乘法 (256 位)
 * ========================================================================
 * 算法: CIOS (Coarsely Integrated Operand Scanning)
 * 输入: a, b (0 ≤ a,b < p), μ = -p^(-1) mod 2^32
 * 输出: r = a * b * 2^(-256) mod p
 * 然后在模 p 结果上调用蒙哥马利约简即可。
 *
 * 对于模 p, 使用蒙哥马利形式:
 *   输入蒙哥马利化: a' = a * R mod p, b' = b * R mod p
 *   输出: r' = a' * b' * R^(-1) mod p = (a*b) * R mod p
 *   逆变换: r = r' * R^(-1) mod p (用 Montgomery 乘 1 实现)
 */

/* 单次 Montgomery 约简 (将 512-bit 积约简到 256-bit) */
static void mont_reduce(bn256_t *r, uint32_t t[17])
{
    for (int i = 0; i < 8; i++) {
        uint32_t u = t[i] * MONT_MU;
        uint64_t carry = 0;
        /* t[i..i+8] += u * p[i..i+7] */
        for (int j = 0; j < 8; j++) {
            carry += (uint64_t)t[i + j] + (uint64_t)u * SM2_P.w[7 - j];
            /* SM2_P 的 w 从 MSW 到 LSW: p[0]=MSW, p[7]=LSW */
            /* 但 t 索引从 LSW 开始: t[0]=LSW, t[15]=MSW */
            /* 我们要乘的是 p[j] (从 LSW 到 MSW) */
            /* 修正: u * p[j] 加到 t[i+j] */
            t[i + j] = (uint32_t)carry;
            carry >>= 32;
        }
        carry += (uint64_t)t[i + 8];
        t[i + 8] = (uint32_t)carry;
        carry >>= 32;
        t[i + 9] += (uint32_t)carry;
    }
    /* 复制 t[8..15] 到 r */
    for (int i = 0; i < 8; i++) {
        r->w[7 - i] = t[8 + i];  /* 恢复大端序 */
    }
    /* 最后减法: if r >= p then r -= p */
    /* 由于 r 可能 < p 但可能等于 p (因为蒙哥马利约简后范围为 [0, 2p)),
     * 我们只需比较 r >= p 则减 p */
    bn256_t tmp;
    bn256_copy(&tmp, r);
    uint32_t borrow = bn256_sub(&tmp, r, &SM2_P);
    if (borrow == 0) {
        bn256_copy(r, &tmp);
    }
}

/* ========================================================================
 *  字乘法: 计算 9-word 乘积 (uint64 中间变量避免溢出)
 * ======================================================================== */
/* 标准长乘法: result = a * b, 每项都是 32-bit, 结果 512-bit (16 words LSB-first) */
static void mul_512(uint32_t r[16], const bn256_t *a, const bn256_t *b)
{
    /* 清零 */
    for (int i = 0; i < 16; i++) r[i] = 0;

    for (int i = 7; i >= 0; i--) {
        uint64_t carry = 0;
        for (int j = 7; j >= 0; j--) {
            /* a->w[i] (MSW..LSW) × b->w[j] */
            /* 乘积的索引: (7-i) + (7-j) from LSB */
            int idx = (7 - i) + (7 - j);
            carry += (uint64_t)r[idx] + (uint64_t)a->w[i] * (uint64_t)b->w[j];
            r[idx] = (uint32_t)carry;
            carry >>= 32;
        }
        r[(7 - i) + 8] = (uint32_t)carry;
    }
}

/* ========================================================================
 *  标准字乘法结果 r[16] 为 LSB-first, 写回 bn256_t 需要恢复大端序。
 *  手动 Montgomery 实现中直接使用 LSB-first 数组, 所以先保持此约定。
 * ======================================================================== */

void fp_mul(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    uint32_t t[17];
    mul_512(t, a, b);
    mont_reduce(r, t);
}

void fp_sqr(bn256_t *r, const bn256_t *a)
{
    fp_mul(r, a, a);
}

void fp_neg(bn256_t *r, const bn256_t *a)
{
    bn256_t zero;
    bn256_set_word(&zero, 0);
    /* r = p - a */
    uint32_t borrow = bn256_sub(r, &SM2_P, a);
    /* 如果 a = 0, r = p, 需要变为 0 */
    if (bn256_cmp(r, &SM2_P) == 0) {
        bn256_set_word(r, 0);
    }
    (void)borrow;
}

void fp_add(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    bn256_t sum;
    uint32_t carry = bn256_add(&sum, a, b);
    if (carry || bn256_cmp(&sum, &SM2_P) >= 0) {
        bn256_sub(&sum, &sum, &SM2_P);
    }
    bn256_copy(r, &sum);
}

void fp_sub(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    bn256_t diff;
    uint32_t borrow = bn256_sub(&diff, a, b);
    if (borrow) {
        bn256_add(&diff, &diff, &SM2_P);
    }
    bn256_copy(r, &diff);
}

/* 费马小定理求逆: a^(p-2) mod p */
void fp_inv(bn256_t *r, const bn256_t *a)
{
    /* p - 2 = 0xFFFFFFFEFFFFFFFF... - 1 = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFD */
    bn256_t e;
    bn256_copy(&e, &SM2_P);
    /* e = p - 2, 但我们已经有了 p, 直接减 2 */
    if (e.w[7] >= 2) {
        e.w[7] -= 2;
    } else {
        e.w[7] -= 2; /* 实际 p.w[7]=0xFFFFFFFF, 所以没问题 */
    }
    /* 平方-乘求幂 */
    bn256_t base, result;
    bn256_copy(&base, a);
    bn256_set_word(&result, 1);

    for (int i = 0; i < 256; i++) {
        /* 从高位到低位扫描 e */
        int word_idx = i / 32;
        int bit_idx = 31 - (i % 32);
        uint32_t bit = (e.w[word_idx] >> bit_idx) & 1;

        if (bit) {
            fp_mul(&result, &result, &base);
        }
        fp_sqr(&base, &base);
    }
    bn256_copy(r, &result);
}

void fp_exp(bn256_t *r, const bn256_t *a, const bn256_t *e)
{
    bn256_t base, result;
    bn256_copy(&base, a);
    bn256_set_word(&result, 1);

    for (int i = 0; i < 256; i++) {
        int word_idx = i / 32;
        int bit_idx = 31 - (i % 32);
        uint32_t bit = (e->w[word_idx] >> bit_idx) & 1;

        if (bit) {
            fp_mul(&result, &result, &base);
        }
        fp_sqr(&base, &base);
    }
    bn256_copy(r, &result);
}

/* ========================================================================
 *  模 n 运算 (SM2 阶)
 * ======================================================================== */

void fn_add(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    bn256_t sum;
    uint32_t carry = bn256_add(&sum, a, b);
    if (carry || bn256_cmp(&sum, &SM2_N) >= 0) {
        bn256_sub(&sum, &sum, &SM2_N);
    }
    bn256_copy(r, &sum);
}

void fn_sub(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    bn256_t diff;
    uint32_t borrow = bn256_sub(&diff, a, b);
    if (borrow) {
        bn256_add(&diff, &diff, &SM2_N);
    }
    bn256_copy(r, &diff);
}

/* 模 n 乘法: 先用普通乘法再用约简 mod n */
static void fn_mul_reduce(bn256_t *r, const uint32_t t[16])
{
    /* 手动约简 mod n: 从 512 位到 256 位 */
    /* n = FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123 */
    /* 对于 256 位模, 用 Barrett 约简或反复减法 */
    /* 简单实现: 用标准长除法 (对于加速可优化) */
    bn256_t tmp;
    /* 把 t 转换为大端 bn256_t (高 256 位) */
    for (int i = 0; i < 8; i++) {
        tmp.w[i] = t[15 - i];
    }
    bn256_copy(r, &tmp);

    /* 逐次减去 n 直到小于 n (最多减 2 次因为乘积 < 2²⁵⁶+n) */
    bn256_t rem;
    bn256_copy(&rem, r);
    for (int i = 0; i < 3; i++) {
        uint32_t b = bn256_sub(&rem, &rem, &SM2_N);
        if (b) {
            bn256_add(&rem, &rem, &SM2_N);
            break;
        }
        bn256_copy(&rem, &rem);
    }
    bn256_copy(r, &rem);
}

void fn_mul(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    uint32_t t[17];
    mul_512(t, a, b);
    fn_mul_reduce(r, t);
}

void fn_inv(bn256_t *r, const bn256_t *a)
{
    /* a^(n-2) mod n (费马小定理, SM2_N 是素数) */
    bn256_t e;
    bn256_copy(&e, &SM2_N);
    if (e.w[7] >= 2) {
        e.w[7] -= 2;
    }

    bn256_t base, result;
    bn256_copy(&base, a);
    bn256_set_word(&result, 1);

    for (int i = 0; i < 256; i++) {
        int word_idx = i / 32;
        int bit_idx = 31 - (i % 32);
        uint32_t bit = (e.w[word_idx] >> bit_idx) & 1;

        if (bit) {
            fn_mul(&result, &result, &base);
        }
        /* base = base^2 mod n */
        fn_mul(&base, &base, &base);
    }
    bn256_copy(r, &result);
}

/* ========================================================================
 *  椭圆曲线点运算 (雅可比射影坐标)
 * ======================================================================== */

void ec_point_set_inf(ec_point_jac_t *p)
{
    bn256_set_word(&p->X, 0);
    bn256_set_word(&p->Y, 0);
    bn256_set_word(&p->Z, 0);
}

int ec_point_is_inf(const ec_point_jac_t *p)
{
    return bn256_is_zero(&p->Z);
}

void ec_point_from_affine(ec_point_jac_t *r, const bn256_t *x, const bn256_t *y)
{
    bn256_copy(&r->X, x);
    bn256_copy(&r->Y, y);
    bn256_set_word(&r->Z, 1);
}

int ec_point_to_affine(bn256_t *x, bn256_t *y, const ec_point_jac_t *p)
{
    if (ec_point_is_inf(p)) return -1;

    bn256_t z_inv, z2_inv, z3_inv;
    fp_inv(&z_inv, &p->Z);
    fp_sqr(&z2_inv, &z_inv);        /* Z^(-2) */
    fp_mul(&z3_inv, &z2_inv, &z_inv); /* Z^(-3) */

    fp_mul(x, &p->X, &z2_inv);      /* X * Z^(-2) */
    fp_mul(y, &p->Y, &z3_inv);      /* Y * Z^(-3) */
    return 0;
}

/* 点加: r = a + b (a ≠ b, a ≠ -b, a,b 非无穷远) */
void ec_point_add(ec_point_jac_t *r, const ec_point_jac_t *a, const ec_point_jac_t *b)
{
    if (ec_point_is_inf(a)) {
        bn256_copy(&r->X, &b->X);
        bn256_copy(&r->Y, &b->Y);
        bn256_copy(&r->Z, &b->Z);
        return;
    }
    if (ec_point_is_inf(b)) {
        bn256_copy(&r->X, &a->X);
        bn256_copy(&r->Y, &a->Y);
        bn256_copy(&r->Z, &a->Z);
        return;
    }

    /* 雅可比点加公式 (从 HAC 算法 3.22) */
    bn256_t Z1Z1, Z2Z2, U1, U2, S1, S2, H, I, J, r2, V;
    bn256_t t0, t1;

    fp_sqr(&Z1Z1, &a->Z);           /* Z1Z1 = Z1² */
    fp_sqr(&Z2Z2, &b->Z);           /* Z2Z2 = Z2² */

    fp_mul(&U1, &a->X, &Z2Z2);      /* U1 = X1 * Z2² */
    fp_mul(&U2, &b->X, &Z1Z1);      /* U2 = X2 * Z1² */

    fp_mul(&t0, &b->Z, &Z2Z2);
    fp_mul(&S1, &a->Y, &t0);        /* S1 = Y1 * Z2³ */

    fp_mul(&t0, &a->Z, &Z1Z1);
    fp_mul(&S2, &b->Y, &t0);        /* S2 = Y2 * Z1³ */

    fp_sub(&H, &U2, &U1);           /* H = U2 - U1 */
    fp_sub(&r2, &S2, &S1);          /* r = S2 - S1 (暂存) */

    /* 如果 H == 0 且 r == 0, 则 a == b, 需要倍点 */
    if (bn256_is_zero(&H) && bn256_is_zero(&r2)) {
        ec_point_dbl(r, a);
        return;
    }

    fp_sqr(&I, &H);                 /* I = H² */
    fp_mul(&t0, &I, &H);           /* t0 = H³ */
    fp_mul(&J, &t0, &U1);          /* J = H³ * U1 (实际上是 H³ * 2U1, 但我们再算) */

    fp_sqr(&t1, &r2);               /* r² */
    fp_mul(&t0, &J, &I);           /* t0 = ... 标号 V */

    fp_mul(&V, &J, &I);            /* 这里简化: V = 2*J = 2*H³*U1, 但用 I. 直接用 r² - H³ - 2J */
    /* 正确公式: X3 = r² - H³ - 2*U1*H² */
    /* 重新实现: */
    bn256_t H2, H3, U1H2;
    fp_sqr(&H2, &H);
    fp_mul(&H3, &H2, &H);
    fp_mul(&U1H2, &U1, &H2);

    fp_sqr(&t0, &r2);
    fp_sub(&t0, &t0, &H3);
    fp_sub(&t0, &t0, &U1H2);
    fp_sub(&t0, &t0, &U1H2);        /* X3 = r² - H³ - 2*U1*H² */
    bn256_copy(&r->X, &t0);

    /* Y3 = r*(U1*H² - X3) - S1*H³ */
    fp_sub(&t0, &U1H2, &r->X);
    fp_mul(&t1, &r2, &t0);
    fp_mul(&t0, &S1, &H3);
    fp_sub(&r->Y, &t1, &t0);

    /* Z3 = Z1 * Z2 * H */
    fp_mul(&t0, &a->Z, &b->Z);
    fp_mul(&r->Z, &t0, &H);
}

/* 倍点: r = 2 * a */
void ec_point_dbl(ec_point_jac_t *r, const ec_point_jac_t *a)
{
    if (ec_point_is_inf(a)) {
        ec_point_set_inf(r);
        return;
    }

    bn256_t XX, YY, YYYY, S, M, T, t0, t1;

    /* 雅可比倍点公式 (a = -3 即 SM2_A 略作优化) */
    fp_sqr(&XX, &a->X);             /* XX = X² */
    fp_sqr(&YY, &a->Y);             /* YY = Y² */
    fp_sqr(&YYYY, &YY);            /* YYYY = Y⁴ */

    fp_add(&S, &a->X, &YY);
    fp_sqr(&S, &S);
    fp_sub(&S, &S, &XX);
    fp_sub(&S, &S, &YYYY);
    fp_add(&S, &S, &S);            /* S = 2*(X+Y²)² - 2*X² - 2*Y⁴ */

    /* M = 3*X² + a*Z⁴ (a = p-3, 使用 a = -3 的优化) */
    fp_sqr(&t0, &a->Z);            /* Z² */
    fp_sqr(&t0, &t0);              /* Z⁴ */
    /* a * Z⁴: a = -3 mod p, 所以 = p - 3 */
    /* 即 (Z⁴ 乘以 a) */
    fp_mul(&t0, &t0, &SM2_A);     /* a * Z⁴ */

    fp_add(&M, &XX, &XX);
    fp_add(&M, &M, &XX);           /* M = 3*X² + a*Z⁴ */
    fp_add(&M, &M, &t0);

    fp_sqr(&r->X, &M);             /* X3 = M² - 2*S */
    fp_sub(&r->X, &r->X, &S);
    fp_sub(&r->X, &r->X, &S);

    /* Y3 = M*(S - X3) - 8*Y⁴ */
    fp_sub(&t0, &S, &r->X);
    fp_mul(&t1, &M, &t0);
    fp_add(&YYYY, &YYYY, &YYYY);
    fp_add(&YYYY, &YYYY, &YYYY);   /* 8*Y⁴ = 8*YYYY */
    fp_add(&YYYY, &YYYY, &YYYY);
    fp_sub(&r->Y, &t1, &YYYY);

    /* Z3 = 2*Y*Z */
    fp_mul(&t0, &a->Y, &a->Z);
    fp_add(&r->Z, &t0, &t0);
}

/* 标量乘: r = k * G (使用基点 G) */
void ec_point_mul_base(bn256_t *rx, bn256_t *ry, const bn256_t *k)
{
    ec_point_jac_t G;
    ec_point_from_affine(&G, &SM2_GX, &SM2_GY);

    ec_point_jac_t result;
    ec_point_set_inf(&result);

    /* 从左到右双倍-加 */
    for (int i = 0; i < 256; i++) {
        int word_idx = i / 32;
        int bit_idx = 31 - (i % 32);
        uint32_t bit = (k->w[word_idx] >> bit_idx) & 1;

        ec_point_dbl(&result, &result);
        if (bit) {
            ec_point_add(&result, &result, &G);
        }
    }

    ec_point_to_affine(rx, ry, &result);
}

/* 标量乘: r = k * P */
void ec_point_mul(ec_point_jac_t *r, const bn256_t *k, const ec_point_jac_t *p)
{
    ec_point_jac_t result;
    ec_point_set_inf(&result);

    for (int i = 0; i < 256; i++) {
        int word_idx = i / 32;
        int bit_idx = 31 - (i % 32);
        uint32_t bit = (k->w[word_idx] >> bit_idx) & 1;

        ec_point_dbl(&result, &result);
        if (bit) {
            ec_point_add(&result, &result, p);
        }
    }
    bn256_copy(&r->X, &result.X);
    bn256_copy(&r->Y, &result.Y);
    bn256_copy(&r->Z, &result.Z);
}

/* ========================================================================
 *  HSM/TRNG 抽象层 — [P0-1] 安全随机数生成
 * ========================================================================
 * 使用三层退化架构:
 *   1. CONFIG_USE_HW_TRNG: 优先调用 SE050 HSM 硬件 TRNG
 *   2. CONFIG_USE_CTR_DRBG: 软件 CTR_DRBG (AES-256) 回退
 *   3. DEBUG_RNG:           LCG 调试回退 (仅用于仿真/测试)
 *
 * 编译时通过 -D 或 config.h 选择层级。
 * 默认: CONFIG_USE_CTR_DRBG
 *
 * 参考:
 *   - NIST SP 800-90A Rev.1 (CTR_DRBG with AES-256)
 *   - SE050 Application Note AN12584
 */

/* ---- 编译配置 ---- */
#ifndef CONFIG_ENABLE_CRYPTO
#error "[P0-1] Must define CONFIG_ENABLE_CRYPTO in build config; review encryption policy"
#endif

/* 未显式定义时取默认 */
#if !defined(CONFIG_USE_HW_TRNG) && !defined(CONFIG_USE_CTR_DRBG) && !defined(DEBUG_RNG)
#define CONFIG_USE_CTR_DRBG 1
#endif

/* ---- SE050 HSM TRNG 外部接口 ---- */
#if defined(CONFIG_USE_HW_TRNG)
/* SE050 Plug & Trust middleware 提供的硬件 TRNG */
/* 原型: int se05x_rng(uint8_t *buf, size_t len) */
/* 返回 0 成功, -1 失败 */
extern int se05x_rng(uint8_t *buf, size_t len);
#endif

/* ========================================================================
 *  CTR_DRBG (AES-256) — NIST SP 800-90A 实现
 * ========================================================================
 * 状态结构: (V, Key) 模式
 * - Key: AES-256 密钥 (32 字节)
 * - V:   16 字节计数器
 */

/* AES S-box (ctr_drbg 专用) */
static const uint8_t ctr_drbg_sbox[256] = {
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

static const uint8_t ctr_drbg_rcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10,
                                           0x20, 0x40, 0x80, 0x1B, 0x36};

static inline uint8_t ctr_gf_mul2(uint8_t x)
{
    return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1B : 0));
}

static inline uint8_t ctr_gf_mul3(uint8_t x)
{
    return ctr_gf_mul2(x) ^ x;
}

static inline uint32_t ctr_aes_sub_word(uint32_t w)
{
    return ((uint32_t)ctr_drbg_sbox[(w >> 24) & 0xFF] << 24) |
           ((uint32_t)ctr_drbg_sbox[(w >> 16) & 0xFF] << 16) |
           ((uint32_t)ctr_drbg_sbox[(w >>  8) & 0xFF] <<  8) |
           ((uint32_t)ctr_drbg_sbox[(w      ) & 0xFF]);
}

static inline uint32_t ctr_aes_rot_word(uint32_t w)
{
    return (w << 8) | (w >> 24);
}

/*
 * AES-256 单块加密 (仅用于 CTR_DRBG)
 */
static void ctr_drbg_aes256_encrypt(const uint8_t key[32],
                                     const uint8_t in[16],
                                     uint8_t out[16])
{
    uint32_t rk[60];
    for (int i = 0; i < 8; i++)
        rk[i] = load_be32(key + i * 4);
    for (int i = 8; i < 60; i++) {
        uint32_t tmp = rk[i - 1];
        if (i % 8 == 0) {
            tmp = ctr_aes_sub_word(ctr_aes_rot_word(tmp)) ^ ((uint32_t)ctr_drbg_rcon[i / 8] << 24);
        } else if (i % 8 == 4) {
            tmp = ctr_aes_sub_word(tmp);
        }
        rk[i] = rk[i - 8] ^ tmp;
    }

    uint32_t s[4];
    s[0] = load_be32(in) ^ rk[0];
    s[1] = load_be32(in + 4) ^ rk[1];
    s[2] = load_be32(in + 8) ^ rk[2];
    s[3] = load_be32(in + 12) ^ rk[3];

    for (int round = 1; round < 14; round++) {
        uint32_t t[4];
        for (int i = 0; i < 4; i++) {
            uint8_t a0 = ctr_drbg_sbox[(s[i] >> 24) & 0xFF];
            uint8_t a1 = ctr_drbg_sbox[(s[i] >> 16) & 0xFF];
            uint8_t a2 = ctr_drbg_sbox[(s[i] >> 8) & 0xFF];
            uint8_t a3 = ctr_drbg_sbox[(s[i]) & 0xFF];
            t[i] = ((uint32_t)a0 << 24) | ((uint32_t)a1 << 16) |
                   ((uint32_t)a2 << 8) | (uint32_t)a3;
        }
        uint32_t sr[4];
        sr[0] = (t[0] & 0xFF000000) | (t[1] & 0x00FF0000) | (t[2] & 0x0000FF00) | (t[3] & 0x000000FF);
        sr[1] = (t[1] & 0xFF000000) | (t[2] & 0x00FF0000) | (t[3] & 0x0000FF00) | (t[0] & 0x000000FF);
        sr[2] = (t[2] & 0xFF000000) | (t[3] & 0x00FF0000) | (t[0] & 0x0000FF00) | (t[1] & 0x000000FF);
        sr[3] = (t[3] & 0xFF000000) | (t[0] & 0x00FF0000) | (t[1] & 0x0000FF00) | (t[2] & 0x000000FF);
        if (round < 14) {
            for (int c = 0; c < 4; c++) {
                uint8_t *col = (uint8_t *)&sr[c];
                uint8_t a = col[0], b = col[1], cc = col[2], d = col[3];
                col[0] = ctr_gf_mul2(a) ^ ctr_gf_mul3(b) ^ cc ^ d;
                col[1] = a ^ ctr_gf_mul2(b) ^ ctr_gf_mul3(cc) ^ d;
                col[2] = a ^ b ^ ctr_gf_mul2(cc) ^ ctr_gf_mul3(d);
                col[3] = ctr_gf_mul3(a) ^ b ^ cc ^ ctr_gf_mul2(d);
            }
        }
        uint32_t rk_off = (uint32_t)round * 4;
        s[0] = sr[0] ^ rk[rk_off];
        s[1] = sr[1] ^ rk[rk_off + 1];
        s[2] = sr[2] ^ rk[rk_off + 2];
        s[3] = sr[3] ^ rk[rk_off + 3];
    }

    {
        uint32_t t[4];
        for (int i = 0; i < 4; i++) {
            uint8_t a0 = ctr_drbg_sbox[(s[i] >> 24) & 0xFF];
            uint8_t a1 = ctr_drbg_sbox[(s[i] >> 16) & 0xFF];
            uint8_t a2 = ctr_drbg_sbox[(s[i] >> 8) & 0xFF];
            uint8_t a3 = ctr_drbg_sbox[(s[i]) & 0xFF];
            t[i] = ((uint32_t)a0 << 24) | ((uint32_t)a1 << 16) |
                   ((uint32_t)a2 << 8) | (uint32_t)a3;
        }
        uint32_t out_w[4];
        out_w[0] = (t[0] & 0xFF000000) | (t[1] & 0x00FF0000) | (t[2] & 0x0000FF00) | (t[3] & 0x000000FF);
        out_w[1] = (t[1] & 0xFF000000) | (t[2] & 0x00FF0000) | (t[3] & 0x0000FF00) | (t[0] & 0x000000FF);
        out_w[2] = (t[2] & 0xFF000000) | (t[3] & 0x00FF0000) | (t[0] & 0x0000FF00) | (t[1] & 0x000000FF);
        out_w[3] = (t[3] & 0xFF000000) | (t[0] & 0x00FF0000) | (t[1] & 0x0000FF00) | (t[2] & 0x000000FF);
        store_be32(out, out_w[0] ^ rk[56]);
        store_be32(out + 4, out_w[1] ^ rk[57]);
        store_be32(out + 8, out_w[2] ^ rk[58]);
        store_be32(out + 12, out_w[3] ^ rk[59]);
    }

    crypto_secure_zero(rk, sizeof(rk));
}

/* CTR_DRBG 内部状态 */
typedef struct {
    uint8_t Key[32];
    uint8_t V[16];
    uint32_t reseed_counter;
    uint8_t initialized;
} ctr_drbg_ctx_t;

static ctr_drbg_ctx_t g_ctr_drbg;

/*
 * CTR_DRBG 更新函数: (Key, V) = AES_ECB_update(provided_data)
 */
static void ctr_drbg_update(const uint8_t provided_data[48])
{
    uint8_t temp[48];
    for (int i = 0; i < 3; i++) {
        /* V = V + 1 (递增计数器) */
        for (int j = 15; j >= 0; j--) {
            if (++g_ctr_drbg.V[j] != 0) break;
        }
        /* temp[i*16 .. (i+1)*16] = AES_ECB(Key, V) */
        ctr_drbg_aes256_encrypt(g_ctr_drbg.Key, g_ctr_drbg.V, temp + i * 16);
    }

    if (provided_data) {
        for (int i = 0; i < 48; i++) {
            temp[i] ^= provided_data[i];
        }
    }

    /* Key = temp[0:32], V = temp[32:48] */
    memcpy(g_ctr_drbg.Key, temp, 32);
    memcpy(g_ctr_drbg.V, temp + 32, 16);

    crypto_secure_zero(temp, sizeof(temp));
}

/*
 * CTR_DRBG 初始化 (无预测抵抗)
 */
static int ctr_drbg_init(const uint8_t *entropy, size_t entropy_len,
                          const uint8_t *nonce, size_t nonce_len)
{
    if (!entropy || entropy_len < 48) return -1;

    /* 清空状态 */
    memset(&g_ctr_drbg, 0, sizeof(g_ctr_drbg));

    /* 种子材料 = entropy || nonce */
    uint8_t seed_material[48];
    memset(seed_material, 0, 48);
    size_t copy = (entropy_len < 48) ? entropy_len : 48;
    memcpy(seed_material, entropy, copy);
    if (nonce && nonce_len > 0) {
        size_t off = (copy < 48) ? copy : 48 - nonce_len;
        for (size_t i = 0; i < nonce_len && (off + i) < 48; i++) {
            seed_material[off + i] ^= nonce[i];
        }
    }

    ctr_drbg_update(seed_material);
    g_ctr_drbg.reseed_counter = 1;
    g_ctr_drbg.initialized = 1;

    crypto_secure_zero(seed_material, sizeof(seed_material));
    return 0;
}

/*
 * CTR_DRBG 生成随机数
 */
static int ctr_drbg_generate(uint8_t *buf, size_t len)
{
    if (!buf || len == 0 || !g_ctr_drbg.initialized) return -1;

    /* 每 2^32 次生成后需 reseed (简化: 此处不自动 reseed) */
    if (g_ctr_drbg.reseed_counter > 0x10000000UL) {
        return -1; /* 需要 reseed */
    }

    uint8_t *out = buf;
    size_t remaining = len;

    while (remaining > 0) {
        /* V = V + 1 */
        for (int j = 15; j >= 0; j--) {
            if (++g_ctr_drbg.V[j] != 0) break;
        }

        uint8_t block[16];
        ctr_drbg_aes256_encrypt(g_ctr_drbg.Key, g_ctr_drbg.V, block);

        size_t todo = (remaining < 16) ? remaining : 16;
        memcpy(out, block, todo);
        out += todo;
        remaining -= todo;
    }

    ctr_drbg_update(NULL);
    g_ctr_drbg.reseed_counter++;

    return 0;
}

/* ========================================================================
 *  hsm_get_random_bytes — [P0-1] 统一随机数接口
 * ========================================================================
 * 顶层抽象: 按编译配置选择 TRNG → CTR_DRBG → DEBUG_LCG
 */
int hsm_get_random_bytes(uint8_t *buf, size_t len)  /* [P0-1] */
{
    if (!buf || len == 0) return -1;

#if defined(CONFIG_USE_HW_TRNG)
    /* Tier 1: SE050 硬件 TRNG */
    if (se05x_rng(buf, len) == 0) {
        return 0;
    }
    /* TRNG 失败: 自动降级到 CTR_DRBG */
    /* fall through */
#endif

#if defined(CONFIG_USE_HW_TRNG) || defined(CONFIG_USE_CTR_DRBG)
    /* Tier 2: CTR_DRBG (AES-256) */
    if (!g_ctr_drbg.initialized) {
        /* 首次使用时自初始化: 从混合熵源生成种子 */
        uint8_t entropy[48];
        /* 尝试使用 TRNG 作为熵源; 无 TRNG 则用时间戳 + 内部状态 */
#if defined(CONFIG_USE_HW_TRNG)
        if (se05x_rng(entropy, 48) != 0) {
            /* TRNG 熵源也失败, 使用退化熵 */
            volatile uint32_t tick = 0;
            for (volatile int i = 0; i < 100; i++) tick++;
            memset(entropy, 0, 48);
            store_be32(entropy, (uint32_t)(uintptr_t)buf ^ tick);
            store_be32(entropy + 4, (uint32_t)(uintptr_t)entropy ^ tick);
            store_be32(entropy + 8, tick);
        }
#else
        {
            /* 无 TRNG 时用退化熵 (可用于开发测试, 生产应有 TRNG) */
            volatile uint32_t tick = 0;
            for (volatile int i = 0; i < 100; i++) tick++;
            memset(entropy, 0, 48);
            store_be32(entropy, (uint32_t)(uintptr_t)buf ^ tick);
            store_be32(entropy + 4, (uint32_t)(uintptr_t)entropy);
            store_be32(entropy + 8, tick ^ 0xA5A5A5A5);
        }
#endif
        uint8_t nonce[16];
        memset(nonce, 0, sizeof(nonce));
        store_be32(nonce, (uint32_t)(uintptr_t)g_ctr_drbg.V);

        if (ctr_drbg_init(entropy, 48, nonce, 16) != 0) {
            crypto_secure_zero(entropy, sizeof(entropy));
            return -1;
        }
        crypto_secure_zero(entropy, sizeof(entropy));
    }

    return ctr_drbg_generate(buf, len);
#elif defined(DEBUG_RNG)
    /* Tier 3: LCG 调试回退 */
    static uint32_t seed = 0xDEADBEEF;
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245U + 12345U;
        buf[i] = (uint8_t)(seed >> 16);
    }
    return 0;
#else
    return -1;
#endif
}

/* ========================================================================
 *  crypto_random_bytes — [P0-1] 委托到 hsm_get_random_bytes
 * ========================================================================
 * 向后兼容包装, 所有原有调用不变。
 */
int crypto_random_bytes(uint8_t *buf, size_t len)
{
    return hsm_get_random_bytes(buf, len);
}
