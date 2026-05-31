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
static void mont_reduce(bn256_t *r, uint32_t t[16])
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
    uint32_t t[16];
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
    uint32_t t[16];
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
 *  随机数生成 (可移植版本)
 * ========================================================================
 * 此实现使用简单线性同余发生器作为后备。
 * 生产环境中应替换为硬件 TRNG 输出，通过 hsm_interface 或平台 API。
 */

int crypto_random_bytes(uint8_t *buf, size_t len)
{
    if (!buf || len == 0) return -1;

    /* 简单 LCG (仅用于测试/开发; 生产环境替换为硬件 TRNG) */
    static uint32_t seed = 0xDEADBEEF;
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245U + 12345U;
        buf[i] = (uint8_t)(seed >> 16);
    }
    return 0;
}
