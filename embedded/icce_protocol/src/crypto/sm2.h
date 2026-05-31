/**
 * @file sm2.h
 * @brief SM2 椭圆曲线公钥密码算法
 * @version 1.0
 * @date 2026-05-28
 *
 * GB/T 32918-2016 SM2 椭圆曲线公钥密码算法:
 * - 第 1 部分: 总则 (曲线参数 sm2p256v1)
 * - 第 2 部分: 数字签名算法
 * - 第 3 部分: 密钥交换协议
 * - 第 4 部分: 公钥加密算法
 *
 * 参考:
 * - GB/T 32918.2-2016 SM2 数字签名算法
 * - GB/T 32918.3-2016 SM2 密钥交换协议
 * - GM/T 0003-2012 SM2 椭圆曲线公钥密码算法
 */

#ifndef SM2_H
#define SM2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto_types.h"

/* ========================================================================
 *  SM2 数字签名算法 (GB/T 32918.2)
 * ======================================================================== */

/**
 * @brief SM2 签名生成
 *
 * 参考 GB/T 32918.2-2016 第 6 章。
 * 使用 SM3 对消息 ZA || M 计算杂凑值后签名。
 *
 * @param private_key  32 字节私钥 (大端)
 * @param msg          待签名消息
 * @param msg_len      消息长度
 * @param user_id      签名者标识 (ZA 计算用, 默认 "1234567812345678")
 * @param user_id_len  标识长度
 * @param public_key   对应的 64 字节公钥 (用于 ZA 计算)
 * @param signature    输出签名 (r || s, 64 字节)
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm2_sign(const uint8_t private_key[SM2_PRIVATE_KEY_SIZE],
             const uint8_t *msg, size_t msg_len,
             const uint8_t *user_id, size_t user_id_len,
             const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
             uint8_t signature[SM2_SIGNATURE_SIZE]);

/**
 * @brief SM2 签名验证
 *
 * @param public_key   64 字节公钥 (X || Y)
 * @param msg          原始消息
 * @param msg_len      消息长度
 * @param user_id      签名者标识
 * @param user_id_len  标识长度
 * @param signature    待验证签名 (r || s, 64 字节)
 * @return CRYPTO_SUCCESS 签名有效, CRYPTO_ERR_VERIFY_FAILED 无效
 */
int sm2_verify(const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
               const uint8_t *msg, size_t msg_len,
               const uint8_t *user_id, size_t user_id_len,
               const uint8_t signature[SM2_SIGNATURE_SIZE]);

/* ========================================================================
 *  SM2 密钥交换协议 (GB/T 32918.3)
 * ======================================================================== */

/**
 * @brief SM2 密钥交换 — 发起方
 *
 * 参考 GB/T 32918.3-2016 第 6 章。
 * 生成临时密钥对, 计算共享密钥。
 *
 * @param private_key       本方静态私钥 (32B)
 * @param public_key        本方静态公钥 (64B)
 * @param peer_public_key   对方静态公钥 (64B)
 * @param user_id           本方标识
 * @param user_id_len       标识长度
 * @param peer_user_id      对方标识
 * @param peer_user_id_len  对方标识长度
 * @param ephemeral_private 输出: 临时私钥 (32B)
 * @param ephemeral_public  输出: 临时公钥 (64B)
 * @param shared_secret     输出: 共享密钥 (32B)
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm2_key_exchange_initiator(
    const uint8_t private_key[SM2_PRIVATE_KEY_SIZE],
    const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t peer_public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t *user_id, size_t user_id_len,
    const uint8_t *peer_user_id, size_t peer_user_id_len,
    uint8_t ephemeral_private[SM2_PRIVATE_KEY_SIZE],
    uint8_t ephemeral_public[SM2_PUBLIC_KEY_SIZE],
    uint8_t shared_secret[SM2_SHARED_SECRET_SIZE]);

/**
 * @brief SM2 密钥交换 — 响应方
 */
int sm2_key_exchange_responder(
    const uint8_t private_key[SM2_PRIVATE_KEY_SIZE],
    const uint8_t public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t peer_public_key[SM2_PUBLIC_KEY_SIZE],
    const uint8_t *user_id, size_t user_id_len,
    const uint8_t *peer_user_id, size_t peer_user_id_len,
    const uint8_t ephemeral_public[SM2_PUBLIC_KEY_SIZE],
    uint8_t shared_secret[SM2_SHARED_SECRET_SIZE]);

/* ========================================================================
 *  简化接口: 直接对哈希签名 (跳过 ZA 计算)
 * ======================================================================== */

/**
 * @brief SM2 对哈希值直接签名 (内部使用, 跳过了 ZA || M 的 SM3 计算)
 *
 * 适用于 hash 已经是 SM3(ZA || M) 的场景。
 *
 * @param d         私钥
 * @param hash      32 字节杂凑值
 * @param signature 输出 (r, s)
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm2_sign_hash(const uint8_t d[SM2_PRIVATE_KEY_SIZE],
                  const uint8_t hash[SM3_DIGEST_SIZE],
                  uint8_t signature[SM2_SIGNATURE_SIZE]);

/**
 * @brief SM2 对哈希值直接验签
 */
int sm2_verify_hash(const uint8_t P[SM2_PUBLIC_KEY_SIZE],
                    const uint8_t hash[SM3_DIGEST_SIZE],
                    const uint8_t signature[SM2_SIGNATURE_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SM2_H */
