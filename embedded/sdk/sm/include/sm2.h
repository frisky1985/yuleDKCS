/******************************************************************************
 * @file    sm2.h
 * @brief   SM2 椭圆曲线公钥密码算法 (GB/T 32918-2016 / GM/T 0003-2012)
 * @note    基于 mbedtls ECP + MPI 数学层实现
 *          支持密钥生成、签名、验签
 ******************************************************************************/
#ifndef SM2_H
#define SM2_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SM2_PUBKEY_LEN      65  /* 未压缩公钥: 0x04 + x(32) + y(32) */
#define SM2_PRIVKEY_LEN     32  /* 私钥长度 */
#define SM2_SIGNATURE_LEN   64  /* 签名: r(32) + s(32) */
#define SM2_USER_ID_LEN     16  /* 默认用户标识符长度 */

/** SM2 密钥对 */
typedef struct {
    uint8_t public_key[SM2_PUBKEY_LEN];    /* 公钥 (未压缩) */
    uint8_t private_key[SM2_PRIVKEY_LEN];  /* 私钥 */
} sm2_keypair_t;

/**
 * @brief 生成 SM2 密钥对
 * @param keypair   输出的密钥对 (非 NULL)
 * @return 0 成功, -1 失败
 */
int sm2_generate_keypair(sm2_keypair_t *keypair);

/**
 * @brief SM2 签名
 * @param keypair    密钥对 (私钥用于签名)
 * @param digest     消息哈希 (SM3 输出的 32 字节摘要)
 * @param signature  输出的签名 (64 字节: r || s)
 * @return 0 成功, -1 失败
 */
int sm2_sign(const sm2_keypair_t *keypair,
             const uint8_t digest[32],
             uint8_t signature[64]);

/**
 * @brief SM2 签名验证
 * @param pubkey     公钥 (65 字节未压缩格式)
 * @param digest     消息哈希 (32 字节)
 * @param signature  签名 (64 字节: r || s)
 * @return 0 签名有效, -1 签名无效, -2 参数错误
 */
int sm2_verify(const uint8_t pubkey[65],
               const uint8_t digest[32],
               const uint8_t signature[64]);

/**
 * @brief SM2 签名验证 (使用内部 mbedtls ECP 上下文)
 * @param grp        mbedtls EC 群 (已初始化为 SM2 曲线)
 * @param pubkey_q   mbedtls EC 点 (公钥)
 * @param digest     消息哈希 (32 字节)
 * @param signature  签名 (64 字节)
 * @return 0 签名有效, -1 无效
 */
int sm2_verify_internal(void *grp, void *pubkey_q,
                        const uint8_t digest[32],
                        const uint8_t signature[64]);

/**
 * @brief 从私钥导出公钥
 * @param privkey   私钥 (32 字节)
 * @param pubkey    输出的公钥 (65 字节未压缩)
 * @return 0 成功, -1 失败
 */
int sm2_compute_public_key(const uint8_t privkey[32],
                           uint8_t pubkey[65]);

/**
 * @brief 计算 SM2 用户标识符哈希 Z 值
 *        Z = SM3(ENTL || ID || a || b || xG || yG || xA || yA)
 * @param pubkey     公钥 (65 字节)
 * @param user_id    用户标识符 (可为 NULL, 使用默认 "1234567812345678")
 * @param user_id_len 用户标识符长度
 * @param z_out      输出 Z (32 字节)
 */
void sm2_compute_z(const uint8_t pubkey[65],
                   const uint8_t *user_id, size_t user_id_len,
                   uint8_t z_out[32]);

#ifdef __cplusplus
}
#endif

#endif /* SM2_H */
