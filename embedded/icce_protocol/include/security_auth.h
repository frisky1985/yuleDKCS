/**
 * @file security_auth.h
 * @brief 安全认证模块接口
 * @version 1.0
 * @date 2026-05-28
 *
 * 提供数字钥匙的安全认证功能:
 * - 密钥管理与存储
 * - 挑战-响应认证
 * - 会话密钥协商
 * - 签名生成与验证
 * - 国密 SM2/SM3/SM4 支持 (USE_SM_CRYPTO)
 */

#ifndef SECURITY_AUTH_H
#define SECURITY_AUTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 *  安全错误码
 * ======================================================================== */
typedef enum {
    SEC_SUCCESS = 0,
    SEC_ERR_KEY_NOT_FOUND,
    SEC_ERR_SIGNATURE_INVALID,
    SEC_ERR_CHALLENGE_EXPIRED,
    SEC_ERR_NONCE_REUSE,
    SEC_ERR_ENCRYPTION_FAILED,
    SEC_ERR_DECRYPTION_FAILED,
    SEC_ERR_KEY_GENERATION_FAILED,
    SEC_ERR_HARDWARE_FAULT,
    SEC_ERR_TAMPER_DETECTED,
    SEC_ERR_INVALID_PARAM,      /* 参数无效 */
    SEC_ERR_BUFFER_OVERFLOW,    /* 缓冲区不足 */
} security_result_t;

/* ========================================================================
 *  密钥类型与结构
 * ======================================================================== */
typedef enum {
    KEY_TYPE_AES_128,
    KEY_TYPE_AES_256,
    KEY_TYPE_ECC_P256_PRIVATE,
    KEY_TYPE_ECC_P256_PUBLIC,
    KEY_TYPE_HMAC_SHA256,
    KEY_TYPE_SM2_PRIVATE,       /* SM2 私钥 */
    KEY_TYPE_SM2_PUBLIC,        /* SM2 公钥 */
    KEY_TYPE_SM4,               /* SM4 密钥 */
} key_type_t;

typedef struct {
    uint32_t key_id;
    key_type_t type;
    uint16_t length;
    uint8_t data[64];
    uint32_t creation_time;
    uint32_t expiry_time;
    uint8_t flags;
} crypto_key_t;

/* ========================================================================
 *  认证挑战与响应
 * ======================================================================== */
typedef struct {
    uint8_t nonce[16];
    uint8_t server_random[32];
    uint32_t timestamp;
    uint32_t expiry;
} auth_challenge_t;

typedef struct {
    uint8_t client_random[32];
    uint8_t signature[64];
    uint8_t public_key[64];
    uint32_t timestamp;
} auth_response_t;

/* ========================================================================
 *  会话信息
 * ======================================================================== */
typedef struct {
    uint32_t session_id;
    uint16_t conn_handle;
    uint8_t session_key[32];
    uint8_t session_id_key[16];
    uint32_t creation_time;
    uint32_t expiry_time;
    uint8_t is_encrypted;
} session_info_t;

/* ========================================================================
 *  主要 API
 * ======================================================================== */

/**
 * @brief 初始化安全模块
 * @return SEC_SUCCESS 成功
 */
security_result_t security_init(void);

/**
 * @brief 生成认证挑战
 * @param challenge 输出挑战
 * @return SEC_SUCCESS 成功
 */
security_result_t security_generate_challenge(auth_challenge_t *challenge);

/**
 * @brief 验证认证响应
 * @param challenge 原始挑战
 * @param response  认证响应
 * @param session   输出会话信息
 * @return SEC_SUCCESS 成功
 */
security_result_t security_verify_response(
    const auth_challenge_t *challenge,
    const auth_response_t *response,
    session_info_t *session);

/**
 * @brief 建立会话密钥
 * @param conn_handle  连接句柄
 * @param public_key   对端公钥
 * @param session      输出会话
 * @return SEC_SUCCESS 成功
 */
security_result_t security_establish_session(
    uint16_t conn_handle,
    const uint8_t *public_key,
    session_info_t *session);

/**
 * @brief 加密数据 (IV + Ciphertext + Tag 格式)
 * @param session        会话信息
 * @param plaintext      明文
 * @param plaintext_len  明文长度
 * @param ciphertext     输出密文缓冲区
 * @param ciphertext_len 输入缓冲区大小, 输出长度
 * @return SEC_SUCCESS 成功
 */
security_result_t security_encrypt(
    const session_info_t *session,
    const uint8_t *plaintext,
    uint16_t plaintext_len,
    uint8_t *ciphertext,
    uint16_t *ciphertext_len);

/**
 * @brief 解密数据
 * @param session        会话信息
 * @param ciphertext     密文 (IV + Ciphertext + Tag)
 * @param ciphertext_len 密文长度
 * @param plaintext      输出明文
 * @param plaintext_len  输出明文长度
 * @return SEC_SUCCESS 成功
 */
security_result_t security_decrypt(
    const session_info_t *session,
    const uint8_t *ciphertext,
    uint16_t ciphertext_len,
    uint8_t *plaintext,
    uint16_t *plaintext_len);

/**
 * @brief 签名数据
 * @param private_key 私钥
 * @param data        待签名数据
 * @param data_len    数据长度
 * @param signature   输出签名
 * @param sig_len     输入缓冲区大小, 输出签名长度
 * @return SEC_SUCCESS 成功
 */
security_result_t security_sign(
    const crypto_key_t *private_key,
    const uint8_t *data,
    uint16_t data_len,
    uint8_t *signature,
    uint16_t *sig_len);

/**
 * @brief 验证签名
 * @param public_key 公钥
 * @param data       原始数据
 * @param data_len   数据长度
 * @param signature  签名
 * @param sig_len    签名长度
 * @return SEC_SUCCESS 验证通过
 */
security_result_t security_verify_signature(
    const crypto_key_t *public_key,
    const uint8_t *data,
    uint16_t data_len,
    const uint8_t *signature,
    uint16_t sig_len);

/**
 * @brief 存储密钥到安全存储
 * @param key_id 密钥 ID
 * @param key    密钥结构
 * @return SEC_SUCCESS 成功
 */
security_result_t security_store_key(uint32_t key_id, const crypto_key_t *key);

/**
 * @brief 从安全存储加载密钥
 * @param key_id 密钥 ID
 * @param key    输出密钥结构
 * @return SEC_SUCCESS 成功
 */
security_result_t security_load_key(uint32_t key_id, crypto_key_t *key);

/**
 * @brief 销毁会话
 * @param session_id 会话 ID
 * @return SEC_SUCCESS 成功
 */
security_result_t security_destroy_session(uint32_t session_id);

#ifdef __cplusplus
}
#endif

#endif /* SECURITY_AUTH_H */
