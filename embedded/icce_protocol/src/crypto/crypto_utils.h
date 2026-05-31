/**
 * @file crypto_utils.h
 * @brief 国密算法层 — 256 位大数 / 域运算 / 椭圆曲线点运算
 * @version 1.0
 * @date 2026-05-28
 *
 * 提供定长 256 位大数运算 (大端序)、模 p 运算、SM2 椭圆曲线点加/倍/标量乘。
 * 不引入动态内存分配，所有结果写回 caller 提供的缓冲区。
 *
 * 参考:
 * - GB/T 32918.1-2016 SM2 椭圆曲线公钥密码算法 第 5 章
 * - GM/T 0003-2012 SM2 椭圆曲线公钥密码算法
 */

#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto_types.h"

/* ========================================================================
 *  SM2 曲线参数 (sm2p256v1)
 * ========================================================================
 * 域:  y² = x³ + ax + b  (mod p)
 * p = FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF 00000000 FFFFFFFF FFFFFFFF FFFFFFFF
 * a = FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF 00000000 FFFFFFFF FFFFFFFF FFFFFFFC  (= p - 3)
 * b = 28E9FA9E 9D9F5E34 4D5A9E4B CF6509A7 F39789F5 15AB8F92 DDBCBD41 4D940E93
 * G = (Gx, Gy)
 * n = FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF 7203DF6B 21C6052B 53BBF409 39D54123  (阶)
 *
 * 注: SM2 与 secp256r1 使用相同的域 p 和系数 a, 但 b、生成元 G、阶 n 不同。
 */
extern const bn256_t SM2_P;   /* 域模数 p */
extern const bn256_t SM2_A;   /* 曲线系数 a */
extern const bn256_t SM2_B;   /* 曲线系数 b */
extern const bn256_t SM2_N;   /* 基点阶 n */
extern const bn256_t SM2_GX;  /* 基点 X 坐标 */
extern const bn256_t SM2_GY;  /* 基点 Y 坐标 */

/* ========================================================================
 *  256 位大数运算
 * ======================================================================== */

/** 比较: 返回 -1/0/1 (大端字典序) */
int  bn256_cmp(const bn256_t *a, const bn256_t *b);

/** 赋值: dst = src */
void bn256_copy(bn256_t *dst, const bn256_t *src);

/** 判断是否为零 */
int  bn256_is_zero(const bn256_t *a);

/** 判断是否为 1 */
int  bn256_is_one(const bn256_t *a);

/** 加载 uint8[32] 大端字节串 → bn256 */
void bn256_from_bytes(bn256_t *r, const uint8_t bytes[32]);

/** bn256 → uint8[32] 大端字节串 */
void bn256_to_bytes(const bn256_t *r, uint8_t bytes[32]);

/** 设定常数 (低 32 位) */
void bn256_set_word(bn256_t *r, uint32_t val);

/** 加法: r = a + b (无模约简，返回进位) */
uint32_t bn256_add(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** 减法: r = a - b (无借位返回 0，有借位返回 1) */
uint32_t bn256_sub(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** 左移 1 位: r = a << 1 (返回溢出位) */
uint32_t bn256_lshift1(bn256_t *r, const bn256_t *a);

/** 右移 1 位: r = a >> 1 */
void bn256_rshift1(bn256_t *r, const bn256_t *a);

/* ========================================================================
 *  模 p 运算 (SM2 域)
 * ======================================================================== */

/** r = (a + b) mod p */
void fp_add(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** r = (a - b) mod p */
void fp_sub(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** r = (a * b) mod p — 使用 Montgomery 乘法优化 */
void fp_mul(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** r = a² mod p */
void fp_sqr(bn256_t *r, const bn256_t *a);

/** r = (-a) mod p */
void fp_neg(bn256_t *r, const bn256_t *a);

/** r = a^(-1) mod p (费马小定理: a^(p-2) mod p) */
void fp_inv(bn256_t *r, const bn256_t *a);

/** r = a^e mod p (平方-乘) */
void fp_exp(bn256_t *r, const bn256_t *a, const bn256_t *e);

/* ========================================================================
 *  模 n 运算 (SM2 阶)
 * ======================================================================== */

/** r = (a + b) mod n */
void fn_add(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** r = (a - b) mod n */
void fn_sub(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** r = (a * b) mod n */
void fn_mul(bn256_t *r, const bn256_t *a, const bn256_t *b);

/** r = a^(-1) mod n */
void fn_inv(bn256_t *r, const bn256_t *a);

/* ========================================================================
 *  椭圆曲线点运算 (雅可比射影坐标)
 * ======================================================================== */

/** 置为无穷远点 (Z = 0) */
void ec_point_set_inf(ec_point_jac_t *p);

/** 判断是否为无穷远点 */
int  ec_point_is_inf(const ec_point_jac_t *p);

/** 从仿射坐标 (x, y) 加载 */
void ec_point_from_affine(ec_point_jac_t *r, const bn256_t *x, const bn256_t *y);

/** 转换回仿射坐标 (x, y); 无穷远点返回 -1 */
int  ec_point_to_affine(bn256_t *x, bn256_t *y, const ec_point_jac_t *p);

/** 点加: r = a + b (不处理 a == b 或 a == -b, 调用方保证不同) */
void ec_point_add(ec_point_jac_t *r, const ec_point_jac_t *a, const ec_point_jac_t *b);

/** 倍点: r = 2 * a */
void ec_point_dbl(ec_point_jac_t *r, const ec_point_jac_t *a);

/** 标量乘: r = k * G (G 为 SM2 基点) */
void ec_point_mul_base(bn256_t *rx, bn256_t *ry, const bn256_t *k);

/** 标量乘: r = k * P (任意点) */
void ec_point_mul(ec_point_jac_t *r, const bn256_t *k, const ec_point_jac_t *p);

/* ========================================================================
 *  随机数生成
 * ========================================================================
 * 为了可移植性，通过外部熵源接口生成。
 * 生产环境下应替换为硬件 TRNG 或平台 CSPRNG。
 * 返回 0 成功, -1 失败。
 */
int crypto_random_bytes(uint8_t *buf, size_t len);

/* ========================================================================
 *  大端字节序工具
 * ======================================================================== */

/** 从大端字节流加载 uint32 */
static inline uint32_t load_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/** uint32 写入大端字节流 */
static inline void store_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

/** 从大端字节流加载 uint64 */
static inline uint64_t load_be64(const uint8_t *p)
{
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] <<  8) |  (uint64_t)p[7];
}

/** uint64 写入大端字节流 */
static inline void store_be64(uint8_t *p, uint64_t v)
{
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >>  8);
    p[7] = (uint8_t)(v);
}

/** 左环移 (32位字) */
static inline uint32_t rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_UTILS_H */
