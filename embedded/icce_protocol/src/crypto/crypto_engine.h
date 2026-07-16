/**
 * @file crypto_engine.h
 * @module EMB-BSW-CRYPTO (ASPICE SWE.4)
 * @brief 统一密码算法引擎接口
 * @version 1.0
 * @date 2026-05-28
 * Layer: BSW (Basic Software Layer)
 *
 * 提供 ICCE 协议栈统一的密码学接口。
 * 支持国密 SM2/SM3/SM4 与标准 P-256/SHA-256/AES-256-GCM 两套算法栈,
 * 通过 crypto_engine_set_algo() 在运行时切换。
 *
 * 默认配置: P-256 + SHA-256 + AES-256-GCM (兼容现有系统)
 * SM 配置: SM2 + SM3 + SM4-GCM (国密合规)
 *
 * 所有函数提供 NULL 指针和长度检查。
 * 敏感数据用 crypto_secure_zero() 清理。
 */

#ifndef CRYPTO_ENGINE_H
#define CRYPTO_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto_types.h"

/* ========================================================================
 *  算法选择
 * ======================================================================== */

/**
 * @brief 设置全局算法族
 * @param ecc  非对称算法 (默认 CRYPTO_ALGO_ECC_P256)
 * @param hash 哈希算法 (默认 HASH_ALGO_SHA256)
 * @param sym  对称算法 (默认 SYM_ALGO_AES256_GCM)
 * @return CRYPTO_SUCCESS 或错误码
 */
int crypto_engine_set_algo(crypto_algo_e ecc, hash_algo_e hash, sym_algo_e sym);

/**
 * @brief 查询当前算法选择
 */
void crypto_engine_get_algo(crypto_algo_e *ecc, hash_algo_e *hash, sym_algo_e *sym);

/* ========================================================================
 *  引擎初始化 / 生命周期
 * ======================================================================== */

/**
 * @brief 初始化加密引擎。必须先于任何其他 crypto_* 调用。
 * @return CRYPTO_SUCCESS 或错误码
 */
int crypto_engine_init(void);

/**
 * @brief 反初始化, 清理内部状态
 */
void crypto_engine_deinit(void);

/* ========================================================================
 *  哈希接口
 * ======================================================================== */

/**
 * @brief 计算哈希 (依据当前 hash_algo_e 选择 SM3 或 SHA-256)
 * @param data  输入数据
 * @param len   数据长度
 * @param hash  输出 32 字节哈希值
 * @return CRYPTO_SUCCESS 或错误码
 */
int crypto_hash(const uint8_t *data, size_t len, uint8_t hash[32]);

/**
 * @brief SHA-256 哈希 (始终使用, 不受算法选择影响)
 */
int crypto_sha256(const uint8_t *data, size_t len, uint8_t hash[32]);

/**
 * @brief SM3 哈希 (始终使用, 不受算法选择影响)
 */
int crypto_sm3(const uint8_t *data, size_t len, uint8_t hash[32]);

/* ========================================================================
 *  对称加密接口
 * ======================================================================== */

/**
 * @brief AES-256-GCM 加密 (始终使用 AES, 不受算法选择影响)
 */
int crypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *plaintext, size_t pt_len,
                           uint8_t *ciphertext,
                           uint8_t *tag, size_t tag_len);

/**
 * @brief AES-256-GCM 解密
 */
int crypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *ciphertext, size_t ct_len,
                           uint8_t *plaintext,
                           const uint8_t *tag, size_t tag_len);

/**
 * @brief SM4-GCM 加密
 */
int crypto_sm4_gcm_encrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *plaintext, size_t pt_len,
                           uint8_t *ciphertext, uint8_t *tag);

/**
 * @brief SM4-GCM 解密
 */
int crypto_sm4_gcm_decrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *ciphertext, size_t ct_len,
                           const uint8_t *tag, uint8_t *plaintext);

/**
 * @brief 对称加密 (依据当前 sym_algo_e 选择 SM4-GCM 或 AES-256-GCM)
 */
int crypto_encrypt(const uint8_t *key, size_t key_len,
                   const uint8_t *iv, size_t iv_len,
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t *plaintext, size_t pt_len,
                   uint8_t *ciphertext, uint8_t *tag);

/**
 * @brief 对称解密 (依据当前 sym_algo_e)
 */
int crypto_decrypt(const uint8_t *key, size_t key_len,
                   const uint8_t *iv, size_t iv_len,
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t *ciphertext, size_t ct_len,
                   const uint8_t *tag, uint8_t *plaintext);

/* ========================================================================
 *  非对称 (签名) 接口
 * ======================================================================== */

/**
 * @brief 签名 (依据当前 ecc_algo_e 选择 SM2 或 ECDSA)
 * @param private_key 私钥 (SM2: 32B; P-256: 32B)
 * @param key_len     私钥长度
 * @param data        待签名数据
 * @param data_len    数据长度
 * @param signature   输出签名
 * @param sig_len     输出签名长度
 * @return CRYPTO_SUCCESS 或错误码
 */
int crypto_sign(const uint8_t *private_key, size_t key_len,
                const uint8_t *data, size_t data_len,
                uint8_t *signature, size_t *sig_len);

/**
 * @brief 验签 (依据当前 ecc_algo_e)
 * @param public_key 公钥 (SM2: 64B 未压缩; P-256: 64B)
 * @param key_len    公钥长度
 * @param data       原始数据
 * @param data_len   数据长度
 * @param signature  签名
 * @param sig_len    签名长度
 * @return CRYPTO_SUCCESS 签名有效, CRYPTO_ERR_VERIFY_FAILED 无效
 */
int crypto_verify(const uint8_t *public_key, size_t key_len,
                  const uint8_t *data, size_t data_len,
                  const uint8_t *signature, size_t sig_len);

/**
 * @brief SM2 直接签名 (hash 是 SM3(ZA||M) 的结果)
 */
int crypto_sm2_sign(const uint8_t *private_key, const uint8_t *hash,
                    uint8_t *signature);

/**
 * @brief SM2 直接验签
 */
int crypto_sm2_verify(const uint8_t *public_key, const uint8_t *hash,
                      const uint8_t *signature);

/**
 * @brief SM2 密钥交换 (类似 ECDH)
 */
int crypto_sm2_key_exchange(const uint8_t *private_key,
                            const uint8_t *peer_public,
                            uint8_t *shared_secret);

/* ========================================================================
 *  密钥派生
 * ======================================================================== */

/**
 * @brief 密钥派生函数 (HMAC-based)
 * @param key      输入密钥材料
 * @param key_len  密钥长度
 * @param salt     盐值 (可 NULL)
 * @param salt_len 盐值长度
 * @param info     上下文信息 (可 NULL)
 * @param info_len 上下文长度
 * @param out      输出派生密钥
 * @param out_len  输出长度
 * @return CRYPTO_SUCCESS 或错误码
 */
int crypto_kdf(const uint8_t *key, size_t key_len,
               const uint8_t *salt, size_t salt_len,
               const uint8_t *info, size_t info_len,
               uint8_t *out, size_t out_len);

/**
 * @brief HMAC-SHA256 (与算法选择无关, 始终使用 SHA-256)
 */
int crypto_hmac_sha256(const uint8_t *key, size_t klen,
                       const uint8_t *data, size_t dlen,
                       uint8_t mac[32]);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_ENGINE_H */
