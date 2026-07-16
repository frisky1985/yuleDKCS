/**
 * @file sm2.c
 * @brief SM2 椭圆曲线公钥密码算法实现
 * @version 1.0
 * @date 2026-05-28
 *
 * GB/T 32918-2016 SM2 数字签名 + 密钥交换实现。
 * 使用 crypto_utils 提供的定长大数与椭圆曲线运算。
 *
 * 测试向量 (GB/T 32918.2-2016 附录 A):
 *   私钥 d: 128B2FA8 BD433C6C 068C8D80 3DFF7979 2A519A55 171B1B65 0C23661D 15897263
 *   公钥 P: 04 (未压缩) + 0AE4C779 8AA0F119 471BEE11 825BE462 02BB79E2 A5844495 ...
 *   消息: "message digest"
 *   签名: ...
 */

#include "sm2.h"
#include "sm3.h"
#include "crypto_utils.h"
#include <string.h>

/* ========================================================================
 *  默认用户标识 (16 字节, 国家标准推荐)
 * ======================================================================== */
static const uint8_t SM2_DEFAULT_UID[16] = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
};
#define SM2_UID_LEN 16

/* ========================================================================
 *  ZA = SM3(ENTLA || IDA || a || b || Gx || Gy || Px || Py)
 *  ENTLA = 用户标识长度 (bit) 编码为 2 字节大端
 * ======================================================================== */
static int sm2_compute_ZA(const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
                          const uint8_t *user_id, size_t user_id_len,
                          uint8_t za[SM3_DIGEST_SIZE])
{
    sm3_ctx_t ctx;
    uint8_t entla[2];
    uint16_t entla_val = (uint16_t)(user_id_len * 8);
    entla[0] = (uint8_t)(entla_val >> 8);
    entla[1] = (uint8_t)(entla_val & 0xFF);

    sm3_init(&ctx);
    sm3_update(&ctx, entla, 2);
    sm3_update(&ctx, user_id, user_id_len);

    /* a, b, Gx, Gy — 大端字节 */
    uint8_t buf[32];
    bn256_to_bytes(&SM2_A, buf);  sm3_update(&ctx, buf, 32);
    bn256_to_bytes(&SM2_B, buf);  sm3_update(&ctx, buf, 32);
    bn256_to_bytes(&SM2_GX, buf); sm3_update(&ctx, buf, 32);
    bn256_to_bytes(&SM2_GY, buf); sm3_update(&ctx, buf, 32);

    /* 公钥 X || Y */
    sm3_update(&ctx, public_key, SM2_PUBLIC_KEY_SIZE);

    return sm3_final(&ctx, za);
}

/* ========================================================================
 *  内部函数: 将消息转换为 e = SM3(ZA || M)
 * ======================================================================== */
static int sm2_msg_to_hash(const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
                           const uint8_t *msg, size_t msg_len,
                           const uint8_t *user_id, size_t user_id_len,
                           uint8_t hash[SM3_DIGEST_SIZE])
{
    uint8_t za[SM3_DIGEST_SIZE];
    int ret = sm2_compute_ZA(public_key, user_id, user_id_len, za);
    if (ret != CRYPTO_SUCCESS) return ret;

    sm3_ctx_t ctx;
    sm3_init(&ctx);
    sm3_update(&ctx, za, SM3_DIGEST_SIZE);
    sm3_update(&ctx, msg, msg_len);
    return sm3_final(&ctx, hash);
}

/* ========================================================================
 *  SM2 数字签名生成 (GB/T 32918.2 第 6 章)
 * ========================================================================
 * 签名者 A 拥有私钥 d, 公钥 P。
 * 步骤:
 * 1. 计算 e = SM3(ZA || M)
 * 2. 生成随机数 k ∈ [1, n-1]
 * 3. 计算 (x1, y1) = k * G
 * 4. 计算 r = (e + x1) mod n
 * 5. 若 r == 0 或 r + k == n, 回到 2
 * 6. 计算 s = ((1 + d)^(-1) * (k - r * d)) mod n
 * 7. 若 s == 0, 回到 2
 * 8. 输出签名 (r, s)
 */

int sm2_sign_hash(const uint8_t d[SM2_PRIVATE_KEY_SIZE],
                  const uint8_t hash[SM3_DIGEST_SIZE],
                  uint8_t signature[SM2_SIGNATURE_SIZE])
{
    if (!d || !hash || !signature) return CRYPTO_ERR_NULL_PTR;

    bn256_t e, k, r, s, x1, y1;
    bn256_t d_bn, k_bn, one, tmp, inv;

    bn256_from_bytes(&e, hash);
    bn256_from_bytes(&d_bn, d);
    bn256_set_word(&one, 1);

    int max_attempts = 100;
    int ret = CRYPTO_ERR_KEY_GEN_FAILED;

    while (max_attempts-- > 0) {
        /* 生成随机 k ∈ [1, n-1] */
        uint8_t k_bytes[32];
        crypto_random_bytes(k_bytes, 32);
        /* 确保 k < n */
        bn256_from_bytes(&k, k_bytes);
        if (bn256_cmp(&k, &SM2_N) >= 0 || bn256_is_zero(&k)) {
            continue;
        }

        /* (x1, y1) = k * G */
        ec_point_mul_base(&x1, &y1, &k);

        /* r = (e + x1) mod n */
        fn_add(&r, &e, &x1);
        if (bn256_is_zero(&r)) continue;

        /* 检查 r + k != n */
        fn_add(&tmp, &r, &k);
        if (bn256_cmp(&tmp, &SM2_N) == 0) continue;

        /* s = ((1 + d)^(-1) * (k - r * d)) mod n */
        fn_add(&inv, &one, &d_bn);        /* 1 + d */
        fn_inv(&inv, &inv);               /* (1 + d)^(-1) mod n */

        fn_mul(&tmp, &r, &d_bn);          /* r * d */
        fn_sub(&tmp, &k, &tmp);           /* k - r*d */
        fn_mul(&s, &inv, &tmp);           /* (1+d)^(-1) * (k - r*d) */

        if (bn256_is_zero(&s)) continue;

        ret = CRYPTO_SUCCESS;
        break;
    }

    if (ret != CRYPTO_SUCCESS) return ret;

    /* 输出 r || s */
    bn256_to_bytes(&r, signature);
    bn256_to_bytes(&s, signature + SM2_PRIVATE_KEY_SIZE);

    return CRYPTO_SUCCESS;
}

int sm2_sign(const uint8_t private_key[SM2_PRIVATE_KEY_SIZE],
             const uint8_t *msg, size_t msg_len,
             const uint8_t *user_id, size_t user_id_len,
             const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
             uint8_t signature[SM2_SIGNATURE_SIZE])
{
    if (!private_key || !msg || !public_key || !signature)
        return CRYPTO_ERR_NULL_PTR;

    if (!user_id || user_id_len == 0) {
        user_id = SM2_DEFAULT_UID;
        user_id_len = SM2_UID_LEN;
    }

    uint8_t hash[SM3_DIGEST_SIZE];
    int ret = sm2_msg_to_hash(public_key, msg, msg_len,
                              user_id, user_id_len, hash);
    if (ret != CRYPTO_SUCCESS) return ret;

    return sm2_sign_hash(private_key, hash, signature);
}

/* ========================================================================
 *  SM2 签名验证 (GB/T 32918.2 第 7 章)
 * ========================================================================
 * 验证者 B 拥有公钥 P。
 * 1. 检查 r, s ∈ [1, n-1]
 * 2. 计算 e = SM3(ZA || M)
 * 3. 计算 t = (r + s) mod n, 若 t == 0 则拒绝
 * 4. 计算点 (x1, y1) = s * G + t * P
 * 5. 计算 R = (e + x1) mod n
 * 6. 若 R == r, 接受; 否则拒绝
 */

int sm2_verify_hash(const uint8_t P[SM2_PUBLIC_KEY_SIZE],
                    const uint8_t hash[SM3_DIGEST_SIZE],
                    const uint8_t signature[SM2_SIGNATURE_SIZE])
{
    if (!P || !hash || !signature) return CRYPTO_ERR_NULL_PTR;

    bn256_t e, r, s, t, x1, y1, R;
    bn256_t s_bn, t_bn;

    bn256_from_bytes(&r, signature);
    bn256_from_bytes(&s, signature + SM2_PRIVATE_KEY_SIZE);
    bn256_from_bytes(&e, hash);

    /* 检查 r, s ∈ [1, n-1] */
    if (bn256_is_zero(&r) || bn256_is_zero(&s)) return CRYPTO_ERR_VERIFY_FAILED;
    if (bn256_cmp(&r, &SM2_N) >= 0 || bn256_cmp(&s, &SM2_N) >= 0)
        return CRYPTO_ERR_VERIFY_FAILED;

    /* t = (r + s) mod n, 若 t == 0 则拒绝 */
    fn_add(&t, &r, &s);
    if (bn256_is_zero(&t)) return CRYPTO_ERR_VERIFY_FAILED;

    /* s * G */
    bn256_t sGx, sGy;
    ec_point_mul_base(&sGx, &sGy, &s);

    /* t * P (公钥是未压缩的 X || Y) */
    bn256_t Px, Py;
    bn256_from_bytes(&Px, P);
    bn256_from_bytes(&Py, P + 32);
    ec_point_jac_t P_jac, tP;
    ec_point_from_affine(&P_jac, &Px, &Py);
    ec_point_mul(&tP, &t, &P_jac);

    /* s*G + t*P */
    ec_point_jac_t sum;
    ec_point_jac_t sG_jac;
    ec_point_from_affine(&sG_jac, &sGx, &sGy);
    ec_point_add(&sum, &sG_jac, &tP);

    /* (x1, y1) */
    bn256_t sum_x, sum_y;
    if (ec_point_to_affine(&sum_x, &sum_y, &sum) != 0) {
        return CRYPTO_ERR_VERIFY_FAILED;  /* 无穷远点 */
    }

    /* R = (e + x1) mod n */
    fn_add(&R, &e, &sum_x);

    return (bn256_cmp(&R, &r) == 0) ? CRYPTO_SUCCESS : CRYPTO_ERR_VERIFY_FAILED;
}

int sm2_verify(const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
               const uint8_t *msg, size_t msg_len,
               const uint8_t *user_id, size_t user_id_len,
               const uint8_t signature[SM2_SIGNATURE_SIZE])
{
    if (!public_key || !msg || !signature) return CRYPTO_ERR_NULL_PTR;

    if (!user_id || user_id_len == 0) {
        user_id = SM2_DEFAULT_UID;
        user_id_len = SM2_UID_LEN;
    }

    uint8_t hash[SM3_DIGEST_SIZE];
    int ret = sm2_msg_to_hash(public_key, msg, msg_len,
                              user_id, user_id_len, hash);
    if (ret != CRYPTO_SUCCESS) return ret;

    return sm2_verify_hash(public_key, hash, signature);
}

/* ========================================================================
 *  SM2 密钥交换协议 (GB/T 32918.3 第 6 章)
 * ========================================================================
 * 简化单边版本 (类似 ECDH with SM2 mutual auth):
 *
 * 设 A 为发起方, B 为响应方。
 * 双方协商共享密钥 K。
 *
 * 发起方步骤:
 * 1. 生成临时密钥对 (rA, RA = rA * G)
 * 2. 计算 tA = (dA + rA) mod n
 * 3. 计算 x1 = 2^w + (x_RA & (2^w - 1)), w = ceil(ceil(log2(n))/2) - 1  (约 128)
 * 4. 计算 U = h * tA * (dB * G + x1 * RB)  =  h * tA * (PB + x1 * RB)
 *   (h = 1 对 SM2 曲线, 即 cofactor)
 * 5. K = KDF(xU || yU || ZA || ZB, klen)
 *
 * 为简化实现, 提供类似 ECDH 的对称密钥交换,
 * 使用 SM2 密钥交换协议的简化版本。
 */
/* ========================================================================
 *  简化 SM2 密钥交换 (单向密钥协商)
 * ========================================================================
 * 使用双方向量协商一个共享密钥。
 * 返回 32 字节共享密钥。
 */

static int sm2_kdf(const uint8_t *z, size_t zlen, size_t klen,
                   uint8_t *key)
{
    /* SM2 密钥派生函数: KDF(Z, klen) = SM3(Z || ct) 重复 */
    uint32_t ct = 1;
    size_t offset = 0;
    sm3_ctx_t ctx;
    int ret;

    while (offset < klen) {
        uint8_t ct_be[4];
        store_be32(ct_be, ct);
        uint8_t hash[SM3_DIGEST_SIZE];

        ret = sm3_init(&ctx);
        if (ret != CRYPTO_SUCCESS) return ret;
        ret = sm3_update(&ctx, z, zlen);
        if (ret != CRYPTO_SUCCESS) return ret;
        ret = sm3_update(&ctx, ct_be, 4);
        if (ret != CRYPTO_SUCCESS) return ret;
        ret = sm3_final(&ctx, hash);
        if (ret != CRYPTO_SUCCESS) return ret;

        size_t todo = (klen - offset < SM3_DIGEST_SIZE) ? (klen - offset) : SM3_DIGEST_SIZE;
        memcpy(key + offset, hash, todo);
        offset += todo;
        ct++;
    }
    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  密钥交换 — 发起方 (生成临时密钥对, 并计算共享密钥)
 * ======================================================================== */
int sm2_key_exchange_initiator(
    const uint8_t private_key[SM2_PRIVATE_KEY_SIZE],
    const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t peer_public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t *user_id, size_t user_id_len,
    const uint8_t *peer_user_id, size_t peer_user_id_len,
    uint8_t ephemeral_private[SM2_PRIVATE_KEY_SIZE],
    uint8_t ephemeral_public[SM2_PUBLIC_KEY_SIZE],
    uint8_t shared_secret[SM2_SHARED_SECRET_SIZE])
{
    if (!private_key || !public_key || !peer_public_key ||
        !ephemeral_private || !ephemeral_public || !shared_secret)
        return CRYPTO_ERR_NULL_PTR;

    if (!user_id || user_id_len == 0) {
        user_id = SM2_DEFAULT_UID;
        user_id_len = SM2_UID_LEN;
    }
    if (!peer_user_id || peer_user_id_len == 0) {
        peer_user_id = SM2_DEFAULT_UID;
        peer_user_id_len = SM2_UID_LEN;
    }

    /* 1. 生成临时密钥对 (rA, RA) */
    uint8_t rA_bytes[32];
    crypto_random_bytes(rA_bytes, 32);
    bn256_t rA;
    bn256_from_bytes(&rA, rA_bytes);
    if (bn256_cmp(&rA, &SM2_N) >= 0 || bn256_is_zero(&rA)) {
        rA.w[7] |= 1;  /* 确保非零 */
    }

    bn256_t RAx, RAy;
    ec_point_mul_base(&RAx, &RAy, &rA);
    bn256_to_bytes(&RAx, ephemeral_public);       /* RAx */
    bn256_to_bytes(&RAy, ephemeral_public + 32);  /* RAy */
    bn256_to_bytes(&rA, ephemeral_private);        /* rA */

    /* 2. 计算 ZA, ZB */
    uint8_t ZA[SM3_DIGEST_SIZE], ZB[SM3_DIGEST_SIZE];
    int ret = sm2_compute_ZA(public_key, user_id, user_id_len, ZA);
    if (ret != CRYPTO_SUCCESS) return ret;
    ret = sm2_compute_ZA(peer_public_key, peer_user_id, peer_user_id_len, ZB);
    if (ret != CRYPTO_SUCCESS) return ret;

    /* 3. 计算 tA = (dA + rA) mod n */
    bn256_t dA, tA;
    bn256_from_bytes(&dA, private_key);
    fn_add(&tA, &dA, &rA);

    /* 4. 计算 w = RAx + 2^128 的修正 (x1 = 2^w + (RAx & (2^w - 1))) */
    /* 简化: 直接使用 RAx */
    bn256_t x1;
    bn256_copy(&x1, &RAx);

    /* 5. 计算 tA * (PB + x1 * RB) */
    /*   先计算 x1 * RB  */
    bn256_t PBx, PBy;
    bn256_from_bytes(&PBx, peer_public_key);
    bn256_from_bytes(&PBy, peer_public_key + 32);

    ec_point_jac_t RB_jac;
    bn256_t RBx, RBy;
    bn256_from_bytes(&RBx, ephemeral_public);
    bn256_from_bytes(&RBy, ephemeral_public + 32);
    ec_point_from_affine(&RB_jac, &RBx, &RBy);

    ec_point_jac_t x1_RB;
    ec_point_mul(&x1_RB, &x1, &RB_jac);

    /* PB + x1*RB */
    ec_point_jac_t PB_jac;
    ec_point_from_affine(&PB_jac, &PBx, &PBy);
    ec_point_jac_t sum;
    ec_point_add(&sum, &PB_jac, &x1_RB);

    /* U = tA * sum */
    ec_point_jac_t U;
    ec_point_mul(&U, &tA, &sum);

    /* 6. K = KDF(xU || yU || ZA || ZB, klen) */
    bn256_t Ux, Uy;
    if (ec_point_to_affine(&Ux, &Uy, &U) != 0) {
        return CRYPTO_ERR_KEY_GEN_FAILED;
    }

    uint8_t xU[32], yU[32];
    bn256_to_bytes(&Ux, xU);
    bn256_to_bytes(&Uy, yU);

    uint8_t z_buf[32 + 32 + SM3_DIGEST_SIZE + SM3_DIGEST_SIZE];
    memcpy(z_buf, xU, 32);
    memcpy(z_buf + 32, yU, 32);
    memcpy(z_buf + 64, ZA, SM3_DIGEST_SIZE);
    memcpy(z_buf + 64 + SM3_DIGEST_SIZE, ZB, SM3_DIGEST_SIZE);

    ret = sm2_kdf(z_buf, sizeof(z_buf), SM2_SHARED_SECRET_SIZE, shared_secret);

    /* [P0-04 FIX] 清除临时私钥和中间缓冲区 (发起方) */
    crypto_secure_zero(rA_bytes, sizeof(rA_bytes));
    crypto_secure_zero(xU, sizeof(xU));
    crypto_secure_zero(yU, sizeof(yU));
    crypto_secure_zero(ZA, sizeof(ZA));
    crypto_secure_zero(ZB, sizeof(ZB));
    crypto_secure_zero(z_buf, sizeof(z_buf));

    if (ret != CRYPTO_SUCCESS) {
        /* [P0-04 FIX] 错误路径清除 ephemeral_private (已写入输出参数) */
        crypto_secure_zero(ephemeral_private, SM2_PRIVATE_KEY_SIZE);
        crypto_secure_zero(ephemeral_public, SM2_PUBLIC_KEY_SIZE);
        crypto_secure_zero(shared_secret, SM2_SHARED_SECRET_SIZE);
    }

    return ret;
}

/* ========================================================================
 *  密钥交换 — 响应方
 * ======================================================================== */
int sm2_key_exchange_responder(
    const uint8_t private_key[SM2_PRIVATE_KEY_SIZE],
    const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t peer_public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t *user_id, size_t user_id_len,
    const uint8_t *peer_user_id, size_t peer_user_id_len,
    const uint8_t ephemeral_public[SM2_PUBLIC_KEY_SIZE],
    uint8_t shared_secret[SM2_SHARED_SECRET_SIZE])
{
    if (!private_key || !public_key || !peer_public_key ||
        !ephemeral_public || !shared_secret)
        return CRYPTO_ERR_NULL_PTR;

    if (!user_id || user_id_len == 0) {
        user_id = SM2_DEFAULT_UID;
        user_id_len = SM2_UID_LEN;
    }
    if (!peer_user_id || peer_user_id_len == 0) {
        peer_user_id = SM2_DEFAULT_UID;
        peer_user_id_len = SM2_UID_LEN;
    }

    /* 1. 计算 ZA, ZB (交换角色: 对方是 A, 自己是 B) */
    uint8_t ZA[SM3_DIGEST_SIZE], ZB[SM3_DIGEST_SIZE];
    int ret = sm2_compute_ZA(peer_public_key, peer_user_id, peer_user_id_len, ZA);
    if (ret != CRYPTO_SUCCESS) return ret;
    ret = sm2_compute_ZA(public_key, user_id, user_id_len, ZB);
    if (ret != CRYPTO_SUCCESS) return ret;

    /* 2. 临时密钥 rB (响应方自己生成) */
    uint8_t rB_bytes[32];
    crypto_random_bytes(rB_bytes, 32);
    bn256_t rB;
    bn256_from_bytes(&rB, rB_bytes);
    if (bn256_cmp(&rB, &SM2_N) >= 0 || bn256_is_zero(&rB)) {
        rB.w[7] |= 1;
    }
    /* [P0-04 FIX] 清除局部随机缓冲区 (已在 bn256_t 中备份) */
    crypto_secure_zero(rB_bytes, sizeof(rB_bytes));

    /* 3. tB = (dB + rB) mod n */
    bn256_t dB, tB;
    bn256_from_bytes(&dB, private_key);
    fn_add(&tB, &dB, &rB);

    /* 4. 使用对方的临时公钥 RA */
    bn256_t RAx, RAy;
    bn256_from_bytes(&RAx, ephemeral_public);
    bn256_from_bytes(&RAy, ephemeral_public + 32);

    /* x1 = RAx (简化) */
    bn256_t x1;
    bn256_copy(&x1, &RAx);

    /* 计算 x1 * RA */
    ec_point_jac_t RA_jac;
    ec_point_from_affine(&RA_jac, &RAx, &RAy);
    ec_point_jac_t x1_RA;
    ec_point_mul(&x1_RA, &x1, &RA_jac);

    /* PA + x1*RA */
    bn256_t PAx, PAy;
    bn256_from_bytes(&PAx, peer_public_key);
    bn256_from_bytes(&PAy, peer_public_key + 32);
    ec_point_jac_t PA_jac;
    ec_point_from_affine(&PA_jac, &PAx, &PAy);
    ec_point_jac_t sum;
    ec_point_add(&sum, &PA_jac, &x1_RA);

    /* V = tB * sum */
    ec_point_jac_t V;
    ec_point_mul(&V, &tB, &sum);

    /* 5. K = KDF(xV || yV || ZA || ZB, klen) */
    bn256_t Vx, Vy;
    if (ec_point_to_affine(&Vx, &Vy, &V) != 0) {
        return CRYPTO_ERR_KEY_GEN_FAILED;
    }

    uint8_t xV[32], yV[32];
    bn256_to_bytes(&Vx, xV);
    bn256_to_bytes(&Vy, yV);

    uint8_t z_buf[32 + 32 + SM3_DIGEST_SIZE + SM3_DIGEST_SIZE];
    memcpy(z_buf, xV, 32);
    memcpy(z_buf + 32, yV, 32);
    memcpy(z_buf + 64, ZA, SM3_DIGEST_SIZE);
    memcpy(z_buf + 64 + SM3_DIGEST_SIZE, ZB, SM3_DIGEST_SIZE);

    ret = sm2_kdf(z_buf, sizeof(z_buf), SM2_SHARED_SECRET_SIZE, shared_secret);

    /* [P0-04 FIX] 清除中间缓冲区 (响应方) */
    crypto_secure_zero(ZA, sizeof(ZA));
    crypto_secure_zero(ZB, sizeof(ZB));
    crypto_secure_zero(z_buf, sizeof(z_buf));
    crypto_secure_zero(xV, sizeof(xV));
    crypto_secure_zero(yV, sizeof(yV));

    if (ret != CRYPTO_SUCCESS) {
        /* [P0-04 FIX] 错误路径清除共享密钥 */
        crypto_secure_zero(shared_secret, SM2_SHARED_SECRET_SIZE);
    }

    return ret;
}
