/**
 * @file sm4.h
 * @brief SM4 分组密码算法
 * @version 1.0
 * @date 2026-05-28
 *
 * GB/T 32907-2016 / GM/T 0002-2012 SM4 分组密码算法。
 * 128 位密钥, 128 位分组, 32 轮 Feistel 结构。
 *
 * 参考:
 * - GB/T 32907-2016 信息安全技术 SM4 分组密码算法
 * - NIST SP 800-38D (GCM 模式)
 *
 * 测试向量:
 *  密钥: 0123456789ABCDEFFEDCBA9876543210
 *  明文: 0123456789ABCDEFFEDCBA9876543210
 *  密文: 681EDF34D206965E86B3E94F536E4246
 */

#ifndef SM4_H
#define SM4_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto_types.h"

/** SM4 密钥扩展上下文 */
typedef struct {
    uint32_t rk[32];     /* 32 轮轮密钥 (大端) */
} sm4_key_t;

/* ========================================================================
 *  基础加解密
 * ======================================================================== */

/**
 * @brief SM4 密钥扩展
 * @param key  128 位 (16 字节) 密钥
 * @param skey 输出密钥上下文
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm4_set_key(const uint8_t key[SM4_KEY_SIZE], sm4_key_t *skey);

/**
 * @brief SM4 ECB 模式加密
 * @param skey  密钥上下文
 * @param plain  明文 (16 字节倍数)
 * @param pt_len 明文长度
 * @param cipher 输出密文
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm4_ecb_encrypt(const sm4_key_t *skey,
                    const uint8_t *plain, size_t pt_len,
                    uint8_t *cipher);

/**
 * @brief SM4 ECB 模式解密
 */
int sm4_ecb_decrypt(const sm4_key_t *skey,
                    const uint8_t *cipher, size_t ct_len,
                    uint8_t *plain);

/**
 * @brief SM4 CBC 模式加密
 * @param skey   密钥上下文
 * @param iv     初始向量 (16 字节)
 * @param plain  明文 (16 字节倍数)
 * @param pt_len 明文长度
 * @param cipher 输出密文
 */
int sm4_cbc_encrypt(const sm4_key_t *skey,
                    const uint8_t iv[SM4_BLOCK_SIZE],
                    const uint8_t *plain, size_t pt_len,
                    uint8_t *cipher);

/**
 * @brief SM4 CBC 模式解密
 */
int sm4_cbc_decrypt(const sm4_key_t *skey,
                    const uint8_t iv[SM4_BLOCK_SIZE],
                    const uint8_t *cipher, size_t ct_len,
                    uint8_t *plain);

/* ========================================================================
 *  GCM 模式
 * ======================================================================== */

/**
 * @brief SM4-GCM 加密
 * @param key        128 位密钥
 * @param iv         初始向量 (建议 12 字节)
 * @param iv_len     IV 长度
 * @param aad        附加认证数据 (可空)
 * @param aad_len    AAD 长度
 * @param plaintext  明文
 * @param pt_len     明文长度
 * @param ciphertext 输出密文 (与明文同长)
 * @param tag        输出认证标签 (至少 16 字节)
 * @param tag_len    标签长度 (建议 16)
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm4_gcm_encrypt(const uint8_t key[SM4_KEY_SIZE],
                    const uint8_t *iv, size_t iv_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *plaintext, size_t pt_len,
                    uint8_t *ciphertext,
                    uint8_t *tag, size_t tag_len);

/**
 * @brief SM4-GCM 解密
 */
int sm4_gcm_decrypt(const uint8_t key[SM4_KEY_SIZE],
                    const uint8_t *iv, size_t iv_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *tag, size_t tag_len,
                    uint8_t *plaintext);

#ifdef __cplusplus
}
#endif

#endif /* SM4_H */
