/******************************************************************************
 * @file    se050_crypto.h
 * @brief   SE050 加密操作内部头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 ******************************************************************************/

#ifndef SE050_CRYPTO_H
#define SE050_CRYPTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dkcs.h"
#include "se050_adapter.h"

/******************************************************************************
 * 初始化与反初始化
 ******************************************************************************/

/**
 * @brief 初始化加密模块
 * @return OK 成功，其他错误码
 */
error_t se050_crypto_init(void);

/**
 * @brief 反初始化加密模块
 * @return OK 成功，其他错误码
 */
error_t se050_crypto_deinit(void);

/******************************************************************************
 * CCC 封装函数
 ******************************************************************************/

/**
 * @brief CCC AES-GCM 加密函数
 * @param[in] key           16字节 AES 密钥
 * @param[in] iv            12字节初始化向量
 * @param[in] aad           附加认证数据 (可为 NULL)
 * @param[in] aad_len       AAD 长度
 * @param[in] plaintext     明文数据
 * @param[in] plaintext_len 明文长度
 * @param[out] ciphertext   密文输出缓冲区
 * @param[out] tag          16字节认证标签输出
 * @return OK 成功，其他错误码
 */
error_t ccc_encrypt_aes_gcm(
    const uint8_t *key,
    const uint8_t *iv,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *plaintext,
    size_t plaintext_len,
    uint8_t *ciphertext,
    uint8_t *tag
);

/**
 * @brief CCC AES-GCM 解密函数
 * @param[in] key           16字节 AES 密钥
 * @param[in] iv            12字节初始化向量
 * @param[in] aad           附加认证数据 (可为 NULL)
 * @param[in] aad_len       AAD 长度
 * @param[in] ciphertext    密文数据
 * @param[in] ciphertext_len 密文长度
 * @param[in] tag           16字节认证标签
 * @param[out] plaintext    明文输出缓冲区
 * @return OK 成功，其他错误码
 */
error_t ccc_decrypt_aes_gcm(
    const uint8_t *key,
    const uint8_t *iv,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *tag,
    uint8_t *plaintext
);

/**
 * @brief 生成 AES 密钥
 * @param[in] key_id 密钥标识符
 * @param[in] type 密钥类型 (AES_128/AES_256)
 * @param[in] permissions 密钥权限
 * @param[in] persistent 是否持久化存储
 * @return OK 成功，其他错误码
 */
error_t ccc_generate_key(
    uint32_t key_id,
    se050_key_type_t type,
    uint16_t permissions,
    bool persistent
);

/******************************************************************************
 * 辅助函数
 ******************************************************************************/

/**
 * @brief 生成随机 IV (12字节)
 * @param[out] iv IV 缓冲区 (12字节)
 * @return OK 成功，其他错误码
 */
error_t se050_generate_iv(uint8_t *iv);

/**
 * @brief 获取当前密钥数量
 * @return 当前已分配的密钥数量
 */
int se050_get_key_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SE050_CRYPTO_H */
