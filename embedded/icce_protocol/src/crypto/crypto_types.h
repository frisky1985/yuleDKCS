/**
 * @file crypto_types.h
 * @brief 国密算法层 — 公共类型与错误码定义
 * @version 1.0
 * @date 2026-05-28
 *
 * ICCE 数字钥匙国密算法层基础类型。
 * 所有 SM 算法文件统一包含此头文件。
 *
 * 参考:
 * - GB/T 32918.1-2016 SM2 椭圆曲线公钥密码算法
 * - GB/T 32905-2016 SM3 密码杂凑算法
 * - GB/T 32907-2016 SM4 分组密码算法
 */

#ifndef CRYPTO_TYPES_H
#define CRYPTO_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ========================================================================
 *  返回值定义
 * ======================================================================== */
#define CRYPTO_SUCCESS              0
#define CRYPTO_ERR_NULL_PTR        (-1)
#define CRYPTO_ERR_BAD_LENGTH      (-2)
#define CRYPTO_ERR_INVALID_INPUT   (-3)
#define CRYPTO_ERR_VERIFY_FAILED   (-4)
#define CRYPTO_ERR_KEY_GEN_FAILED  (-5)
#define CRYPTO_ERR_BUF_OVERFLOW    (-6)
#define CRYPTO_ERR_NOT_INIT        (-7)
#define CRYPTO_ERR_HARDWARE        (-8)
#define CRYPTO_ERR_UNSUPPORTED     (-9)

/* ========================================================================
 *  算法枚举 — 与 crypto_engine.h 保持一致
 * ======================================================================== */
typedef enum {
    CRYPTO_ALGO_ECC_P256 = 0,   /* ECDSA P-256 (默认) */
    CRYPTO_ALGO_SM2      = 1,   /* 国密 SM2             */
} crypto_algo_e;

typedef enum {
    HASH_ALGO_SHA256 = 0,       /* SHA-256 (默认) */
    HASH_ALGO_SM3    = 1,       /* SM3            */
} hash_algo_e;

typedef enum {
    SYM_ALGO_AES256_GCM = 0,    /* AES-256-GCM (默认) */
    SYM_ALGO_SM4_GCM    = 1,    /* SM4-GCM            */
} sym_algo_e;

/* ========================================================================
 *  算法常量
 * ======================================================================== */

/* SM2 / P-256 */
#define SM2_PRIVATE_KEY_SIZE    32   /* 私钥字节数 */
#define SM2_PUBLIC_KEY_SIZE     64   /* 未压缩公钥: X || Y */
#define SM2_SIGNATURE_SIZE      64   /* r || s */
#define SM2_SHARED_SECRET_SIZE  32   /* 密钥交换输出 */

/* SM3 */
#define SM3_DIGEST_SIZE         32   /* 256 位杂凑值 */
#define SM3_BLOCK_SIZE          64   /* 512 位消息分组 */

/* SM4 */
#define SM4_KEY_SIZE            16   /* 128 位密钥 */
#define SM4_BLOCK_SIZE          16   /* 128 位分组 */
#define SM4_GCM_IV_SIZE         12   /* GCM 建议 IV 长度 */
#define SM4_GCM_TAG_SIZE        16   /* GCM 认证标签 */

/* AES-256 */
#define AES256_KEY_SIZE         32
#define AES256_BLOCK_SIZE       16
#define AES256_GCM_IV_SIZE      12
#define AES256_GCM_TAG_SIZE     16

/* 会话 / 派生 */
#define HMAC_SHA256_DIGEST_SIZE 32

/* ========================================================================
 *  256-bit 大数表示
 * ========================================================================
 * 使用 8 × uint32_t 大端序数组表示 256 位整数。
 * bn[0] = 最高有效字 (MSW), bn[7] = 最低有效字 (LSW)。
 * 对应 SM2 域 p 和阶 n 均为 256 位。
 */
typedef struct {
    uint32_t w[8];
} bn256_t;

/* ========================================================================
 *  椭圆曲线点 (雅可比射影坐标)
 * ========================================================================
 * 对于 SM2 曲线 y² = x³ + ax + b (mod p)
 * 雅可比射影坐标: (X, Y, Z) 对应仿射 (x, y) = (X/Z², Y/Z³)
 * Z 为 0 表示无穷远点。
 */
typedef struct {
    bn256_t X;
    bn256_t Y;
    bn256_t Z;
} ec_point_jac_t;

/* ========================================================================
 *  密钥与签名结构
 * ======================================================================== */

/* SM2 密钥对 */
typedef struct {
    uint8_t  private_key[SM2_PRIVATE_KEY_SIZE];   /* 大端私钥 d */
    uint8_t  public_key [SM2_PUBLIC_KEY_SIZE];    /* 未压缩公钥 X || Y */
} sm2_keypair_t;

/* SM2 签名值 */
typedef struct {
    uint8_t r[SM2_PRIVATE_KEY_SIZE];   /* r 分量 */
    uint8_t s[SM2_PRIVATE_KEY_SIZE];   /* s 分量 */
} sm2_signature_t;

/* ========================================================================
 *  内存清零 (安全销毁敏感数据)
 * ======================================================================== */
static inline void crypto_secure_zero(void *ptr, size_t len)
{
    if (ptr) {
        volatile uint8_t *p = (volatile uint8_t *)ptr;
        for (size_t i = 0; i < len; i++) {
            p[i] = 0;
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_TYPES_H */
