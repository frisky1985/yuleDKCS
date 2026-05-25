/******************************************************************************
 * @file    sm2.c
 * @brief   SM2 椭圆曲线公钥密码算法 (GB/T 32918-2016)
 * @note    基于 mbedtls ECP + MPI 数学层
 *          SM2 曲线参数 (sm2p256v1):
 *          - 素数 p: FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF 00000000 FFFFFFFF FFFFFFFF
 *          - 阶  n: FFFFFFFE FFFFFFFF FFFFFFFF FFFFFFFF 7203DF6B 21C6052B 53BBF409 39D54123
 *          - 生成元 G: (32C4AE2C 1F198119 5F990446 6A39C994 ...)
 ******************************************************************************/
#include "sm2.h"
#include "sm3.h"
#include <string.h>
#include <stdlib.h>

/* mbedtls 头文件 */
#include <mbedtls/bignum.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

/* ====================================================================
 * SM2 曲线参数 (sm2p256v1) — GB/T 32918.5-2017
 * ==================================================================== */

/* 素数 p = 2^256 - 2^224 - 2^96 + 2^64 - 1 */
static const uint8_t SM2_P_BIN[32] = {
    0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/* 系数 a = p - 3 (SM2 特性) */
static const uint8_t SM2_A_BIN[32] = {
    0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
};

/* 系数 b = 28E9FA9E 9D9F5E34 4D5A9E4B CF6509A7 F39789F5 15AB8F92 DDBCBD41 4D940E93 */
static const uint8_t SM2_B_BIN[32] = {
    0x28, 0xE9, 0xFA, 0x9E, 0x9D, 0x9F, 0x5E, 0x34,
    0x4D, 0x5A, 0x9E, 0x4B, 0xCF, 0x65, 0x09, 0xA7,
    0xF3, 0x97, 0x89, 0xF5, 0x15, 0xAB, 0x8F, 0x92,
    0xDD, 0xBC, 0xBD, 0x41, 0x4D, 0x94, 0x0E, 0x93
};

/* 生成元 Gx */
static const uint8_t SM2_GX_BIN[32] = {
    0x32, 0xC4, 0xAE, 0x2C, 0x1F, 0x19, 0x81, 0x19,
    0x5F, 0x99, 0x04, 0x46, 0x6A, 0x39, 0xC9, 0x94,
    0x8F, 0xE3, 0x0B, 0xBF, 0xF2, 0x66, 0x0B, 0xE1,
    0x71, 0x5A, 0x45, 0x89, 0x14, 0x2C, 0x74, 0xAF
};

/* 生成元 Gy */
static const uint8_t SM2_GY_BIN[32] = {
    0x46, 0xBC, 0xE6, 0x35, 0x46, 0x5D, 0xBC, 0x32,
    0xD2, 0xC4, 0xE5, 0x3B, 0x05, 0x0E, 0xD7, 0x04,
    0xD8, 0x9B, 0x69, 0xBC, 0xDD, 0xC5, 0x63, 0x01,
    0x3E, 0x4C, 0x34, 0xE9, 0xC0, 0xC0, 0x5A, 0x4B
};

/* 阶 n */
static const uint8_t SM2_N_BIN[32] = {
    0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x72, 0x03, 0xDF, 0x6B, 0x21, 0xC6, 0x05, 0x2B,
    0x53, 0xBB, 0xF4, 0x09, 0x39, 0xD5, 0x41, 0x23
};

/* ====================================================================
 * SM2 曲线初始化
 * ==================================================================== */

/**
 * @brief 初始化 mbedtls ECP 群为 SM2 曲线
 * @param grp   mbedtls EC 群 (未初始化)
 * @return 0 成功, mbedtls 错误码
 */
static int sm2_ecp_group_init(mbedtls_ecp_group *grp)
{
    int ret;
    mbedtls_mpi p, a, b, gx, gy, n;

    mbedtls_mpi_init(&p);  mbedtls_mpi_init(&a);
    mbedtls_mpi_init(&b);  mbedtls_mpi_init(&gx);
    mbedtls_mpi_init(&gy); mbedtls_mpi_init(&n);

    /* 读取 SM2 曲线参数 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&p,  SM2_P_BIN,  32));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&a,  SM2_A_BIN,  32));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&b,  SM2_B_BIN,  32));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&gx, SM2_GX_BIN, 32));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&gy, SM2_GY_BIN, 32));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&n,  SM2_N_BIN,  32));

    mbedtls_ecp_group_init(grp);
    grp->id = MBEDTLS_ECP_DP_NONE; /* 自定义曲线 */

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&grp->P, &p));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&grp->A, &a));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&grp->B, &b));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&grp->N, &n));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&grp->G.Z, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&grp->G.X, &gx));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&grp->G.Y, &gy));

    /* 设置模数大小 (256 bit) */
    grp->pbits = 256;
    grp->nbits = 256;
    grp->h = 1;

cleanup:
    mbedtls_mpi_free(&p);  mbedtls_mpi_free(&a);
    mbedtls_mpi_free(&b);  mbedtls_mpi_free(&gx);
    mbedtls_mpi_free(&gy); mbedtls_mpi_free(&n);

    return ret;
}

/* ====================================================================
 * 随机数生成器 (基于 mbedtls CTR_DRBG)
 * ==================================================================== */

static int sm2_rng(void *ctx, unsigned char *out, size_t len)
{
    return mbedtls_ctr_drbg_random(ctx, out, len);
}

/* ====================================================================
 * SM2 密钥生成
 * ==================================================================== */

int sm2_generate_keypair(sm2_keypair_t *keypair)
{
    int ret = -1;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point pub;
    mbedtls_mpi priv;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;

    if (!keypair) return -1;
    memset(keypair, 0, sizeof(sm2_keypair_t));

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&pub);
    mbedtls_mpi_init(&priv);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_entropy_init(&entropy);

    /* 初始化 SM2 曲线 */
    ret = sm2_ecp_group_init(&grp);
    if (ret != 0) goto cleanup;

    /* 初始化 RNG */
    ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                NULL, 0);
    if (ret != 0) goto cleanup;

    /* 生成密钥对 */
    ret = mbedtls_ecp_gen_keypair(&grp, &priv, &pub, sm2_rng, &drbg);
    if (ret != 0) goto cleanup;

    /* 导出私钥 */
    ret = mbedtls_mpi_write_binary(&priv, keypair->private_key, 32);
    if (ret != 0) goto cleanup;

    /* 导出公钥 (未压缩) */
    ret = mbedtls_ecp_point_write_binary(&grp, &pub,
                                         MBEDTLS_ECP_PF_UNCOMPRESSED,
                                         NULL, keypair->public_key, 65);
    if (ret != 0) goto cleanup;

    ret = 0;

cleanup:
    mbedtls_mpi_free(&priv);
    mbedtls_ecp_point_free(&pub);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);

    if (ret != 0) {
        memset(keypair, 0, sizeof(sm2_keypair_t));
    }
    return ret;
}

int sm2_compute_public_key(const uint8_t privkey[32],
                           uint8_t pubkey[65])
{
    int ret;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point pub;
    mbedtls_mpi priv;

    if (!privkey || !pubkey) return -1;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&pub);
    mbedtls_mpi_init(&priv);

    ret = sm2_ecp_group_init(&grp);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&priv, privkey, 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_ecp_mul(&grp, &pub, &priv, &grp.G, NULL, NULL);
    if (ret != 0) goto cleanup;

    ret = mbedtls_ecp_point_write_binary(&grp, &pub,
                                         MBEDTLS_ECP_PF_UNCOMPRESSED,
                                         NULL, pubkey, 65);
cleanup:
    mbedtls_mpi_free(&priv);
    mbedtls_ecp_point_free(&pub);
    mbedtls_ecp_group_free(&grp);
    return ret;
}

/* ====================================================================
 * SM2 用户标识符 Z 值计算
 * ==================================================================== */

void sm2_compute_z(const uint8_t pubkey[65],
                   const uint8_t *user_id, size_t user_id_len,
                   uint8_t z_out[32])
{
    sm3_context_t sm3;
    uint8_t entl[2];

    if (!pubkey || !z_out) return;

    /* 默认用户标识: "1234567812345678" (16字节) */
    if (!user_id) {
        user_id = (const uint8_t *)"1234567812345678";
        user_id_len = 16;
    }

    /* ENTL = user_id_len * 8 (大端2字节) */
    uint16_t entl_val = (uint16_t)(user_id_len * 8);
    entl[0] = (uint8_t)(entl_val >> 8);
    entl[1] = (uint8_t)(entl_val);

    sm3_init(&sm3);
    sm3_update(&sm3, entl, 2);             /* ENTL */
    sm3_update(&sm3, user_id, user_id_len); /* ID  */
    sm3_update(&sm3, SM2_A_BIN, 32);       /* a   */
    sm3_update(&sm3, SM2_B_BIN, 32);       /* b   */
    sm3_update(&sm3, SM2_GX_BIN, 32);      /* xG  */
    sm3_update(&sm3, SM2_GY_BIN, 32);      /* yG  */
    sm3_update(&sm3, pubkey + 1, 32);      /* xA  */
    sm3_update(&sm3, pubkey + 33, 32);     /* yA  */
    sm3_finish(&sm3, z_out);
}

/* ====================================================================
 * SM2 签名
 * ==================================================================== */

int sm2_sign(const sm2_keypair_t *keypair,
             const uint8_t digest[32],
             uint8_t signature[64])
{
    int ret = -1;
    mbedtls_ecp_group grp;
    mbedtls_mpi d, k, e, r, s, t, tmp, n_minus_1, one;
    mbedtls_ecp_point point;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;

    if (!keypair || !digest || !signature) return -1;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);  mbedtls_mpi_init(&k);
    mbedtls_mpi_init(&e);  mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);  mbedtls_mpi_init(&t);
    mbedtls_mpi_init(&tmp); mbedtls_mpi_init(&n_minus_1);
    mbedtls_mpi_init(&one);
    mbedtls_ecp_point_init(&point);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_entropy_init(&entropy);

    ret = sm2_ecp_group_init(&grp);
    if (ret != 0) goto cleanup;

    ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) goto cleanup;

    /* 读取私钥 */
    ret = mbedtls_mpi_read_binary(&d, keypair->private_key, 32);
    if (ret != 0) goto cleanup;

    /* 计算 e = H(Z || M) — 外部已计算完整消息哈希, 此处直接用 */
    ret = mbedtls_mpi_read_binary(&e, digest, 32);
    if (ret != 0) goto cleanup;

    /* n_minus_1 = n - 1 */
    ret = mbedtls_mpi_read_binary(&n_minus_1, SM2_N_BIN, 32);
    if (ret != 0) goto cleanup;
    ret = mbedtls_mpi_sub_int(&n_minus_1, &n_minus_1, 1);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_lset(&one, 1);

    /* 签名循环: 生成随机 k, 直到 r != 0 且 s != 0 */
    do {
        /* 生成随机数 k ∈ [1, n-1] */
        do {
            ret = mbedtls_ecp_gen_keypair(&grp, &k, &point, sm2_rng, &drbg);
            if (ret != 0) goto cleanup;
        } while (mbedtls_mpi_cmp_int(&k, 0) == 0);

        /* (x1, y1) = k * G */
        ret = mbedtls_ecp_mul(&grp, &point, &k, &grp.G, sm2_rng, &drbg);
        if (ret != 0) goto cleanup;

        /* r = (e + x1) mod n */
        ret = mbedtls_mpi_read_binary(&tmp, SM2_N_BIN, 32);
        if (ret != 0) goto cleanup;
        ret = mbedtls_mpi_mod_mpi(&r, &point.X, &tmp);
        if (ret != 0) goto cleanup;
        ret = mbedtls_mpi_add_mpi(&r, &e, &r);
        if (ret != 0) goto cleanup;
        ret = mbedtls_mpi_mod_mpi(&r, &r, &tmp);
        if (ret != 0) goto cleanup;

        /* 检查 r != 0 */
        if (mbedtls_mpi_cmp_int(&r, 0) == 0) continue;

        /* s = ((1 + d)^(-1) * (k - r * d)) mod n */
        ret = mbedtls_mpi_add_mpi(&t, &one, &d);  /* t = 1 + d */
        if (ret != 0) goto cleanup;
        ret = mbedtls_mpi_inv_mod(&t, &t, &tmp);   /* t = (1+d)^(-1) */
        if (ret != 0) goto cleanup;

        ret = mbedtls_mpi_mul_mpi(&s, &r, &d);     /* s = r * d */
        if (ret != 0) goto cleanup;
        ret = mbedtls_mpi_sub_mpi(&s, &k, &s);     /* s = k - r*d */
        if (ret != 0) goto cleanup;
        ret = mbedtls_mpi_mul_mpi(&s, &t, &s);     /* s = (1+d)^(-1) * (k - r*d) */
        if (ret != 0) goto cleanup;
        ret = mbedtls_mpi_mod_mpi(&s, &s, &tmp);   /* s = s mod n */
        if (ret != 0) goto cleanup;

    } while (mbedtls_mpi_cmp_int(&r, 0) == 0 ||
             mbedtls_mpi_cmp_int(&s, 0) == 0);

    /* 输出签名 r || s */
    ret = mbedtls_mpi_write_binary(&r, signature, 32);
    if (ret != 0) goto cleanup;
    ret = mbedtls_mpi_write_binary(&s, signature + 32, 32);
    if (ret != 0) goto cleanup;

    ret = 0;

cleanup:
    mbedtls_mpi_free(&d);  mbedtls_mpi_free(&k);
    mbedtls_mpi_free(&e);  mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);  mbedtls_mpi_free(&t);
    mbedtls_mpi_free(&tmp); mbedtls_mpi_free(&n_minus_1);
    mbedtls_mpi_free(&one);
    mbedtls_ecp_point_free(&point);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}

/* ====================================================================
 * SM2 签名验证
 * ==================================================================== */

int sm2_verify(const uint8_t pubkey[65],
               const uint8_t digest[32],
               const uint8_t signature[64])
{
    int ret;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point pub_q, point;
    mbedtls_mpi r, s, e, t, tmp, n;

    if (!pubkey || !digest || !signature) return -2;
    if (pubkey[0] != 0x04) return -2;  /* 必须是未压缩格式 */

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&pub_q);
    mbedtls_ecp_point_init(&point);
    mbedtls_mpi_init(&r);  mbedtls_mpi_init(&s);
    mbedtls_mpi_init(&e);  mbedtls_mpi_init(&t);
    mbedtls_mpi_init(&tmp); mbedtls_mpi_init(&n);

    ret = sm2_ecp_group_init(&grp);
    if (ret != 0) goto cleanup;

    /* 读取公钥点 */
    ret = mbedtls_ecp_point_read_binary(&grp, &pub_q, pubkey, 65);
    if (ret != 0) { ret = -2; goto cleanup; }

    /* 验证公钥在曲线上 */
    ret = mbedtls_ecp_check_pubkey(&grp, &pub_q);
    if (ret != 0) { ret = -1; goto cleanup; }

    /* 读取签名 r, s */
    ret = mbedtls_mpi_read_binary(&r, signature, 32);
    if (ret != 0) goto cleanup;
    ret = mbedtls_mpi_read_binary(&s, signature + 32, 32);
    if (ret != 0) goto cleanup;

    /* 读取阶 n */
    ret = mbedtls_mpi_read_binary(&n, SM2_N_BIN, 32);
    if (ret != 0) goto cleanup;

    /* 验证 r, s ∈ [1, n-1] */
    if (mbedtls_mpi_cmp_int(&r, 1) < 0 || mbedtls_mpi_cmp_mpi(&r, &n) >= 0) {
        ret = -1; goto cleanup;
    }
    if (mbedtls_mpi_cmp_int(&s, 1) < 0 || mbedtls_mpi_cmp_mpi(&s, &n) >= 0) {
        ret = -1; goto cleanup;
    }

    /* 计算 e = H(Z || M) — 外部已传入摘要 */
    ret = mbedtls_mpi_read_binary(&e, digest, 32);
    if (ret != 0) goto cleanup;

    /* t = (r + s) mod n */
    ret = mbedtls_mpi_add_mpi(&t, &r, &s);
    if (ret != 0) goto cleanup;
    ret = mbedtls_mpi_mod_mpi(&t, &t, &n);
    if (ret != 0) goto cleanup;

    if (mbedtls_mpi_cmp_int(&t, 0) == 0) {
        ret = -1; goto cleanup;
    }

    /* (x1, y1) = s * G + t * P_A */
    ret = mbedtls_ecp_muladd(&grp, &point, &s, &grp.G, &t, &pub_q);
    if (ret != 0) { ret = -1; goto cleanup; }

    /* R = (e + x1) mod n */
    ret = mbedtls_mpi_mod_mpi(&tmp, &point.X, &n);
    if (ret != 0) goto cleanup;
    ret = mbedtls_mpi_add_mpi(&tmp, &e, &tmp);
    if (ret != 0) goto cleanup;
    ret = mbedtls_mpi_mod_mpi(&tmp, &tmp, &n);
    if (ret != 0) goto cleanup;

    /* 验证 R == r */
    ret = (mbedtls_mpi_cmp_mpi(&tmp, &r) == 0) ? 0 : -1;

cleanup:
    mbedtls_mpi_free(&r);  mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&e);  mbedtls_mpi_free(&t);
    mbedtls_mpi_free(&tmp); mbedtls_mpi_free(&n);
    mbedtls_ecp_point_free(&pub_q);
    mbedtls_ecp_point_free(&point);
    mbedtls_ecp_group_free(&grp);
    return ret;
}

int sm2_verify_internal(void *grp, void *pubkey_q,
                        const uint8_t digest[32],
                        const uint8_t signature[64])
{
    /* 暂未实现 — 调用方可直接使用 sm2_verify */
    (void)grp; (void)pubkey_q;
    (void)digest; (void)signature;
    return -1;
}
