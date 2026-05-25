/******************************************************************************
 * @file    sm4.h
 * @brief   SM4 分组密码算法 (GB/T 32907-2016 / GM/T 0002-2012)
 * @note    支持 ECB/CBC 模式，128-bit 密钥，128-bit 分组
 ******************************************************************************/
#ifndef SM4_H
#define SM4_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SM4_BLOCK_SIZE  16  /* 128-bit */
#define SM4_KEY_SIZE    16  /* 128-bit */

/** SM4 密码模式 */
typedef enum {
    SM4_MODE_ECB = 0,  /**< 电子密码本模式 */
    SM4_MODE_CBC       /**< 密码分组链接模式 */
} sm4_mode_t;

/** SM4 上下文 */
typedef struct {
    uint32_t rk[32];    /* 轮密钥 */
    sm4_mode_t mode;    /* 加密模式 */
    uint8_t iv[16];     /* CBC 模式初始向量 */
} sm4_context_t;

/**
 * @brief 初始化 SM4 上下文并设置密钥
 * @param ctx    SM4 上下文 (非 NULL)
 * @param key    16 字节密钥
 * @param key_len 密钥长度 (必须为 16)
 * @param mode   加密模式 (ECB / CBC)
 * @param iv     CBC 模式初始向量 (ECB 模式可为 NULL)
 * @return 0  成功
 *         -1 参数无效
 */
int sm4_init(sm4_context_t *ctx, const uint8_t *key, size_t key_len,
             sm4_mode_t mode, const uint8_t *iv);

/**
 * @brief 加密数据
 * @param ctx      SM4 上下文
 * @param in       输入明文
 * @param out      输出密文 (可与 in 重叠)
 * @param len      数据长度 (必须是 16 的倍数)
 * @return 0  成功
 *         -1 参数无效
 */
int sm4_encrypt(sm4_context_t *ctx, const uint8_t *in,
                uint8_t *out, size_t len);

/**
 * @brief 解密数据
 * @param ctx      SM4 上下文
 * @param in       输入密文
 * @param out      输出明文 (可与 in 重叠)
 * @param len      数据长度 (必须是 16 的倍数)
 * @return 0  成功
 *         -1 参数无效
 */
int sm4_decrypt(sm4_context_t *ctx, const uint8_t *in,
                uint8_t *out, size_t len);

/**
 * @brief 单次 SM4-CBC 加密 (带 PKCS7 填充)
 * @param key    密钥 (16 字节)
 * @param iv     初始向量 (16 字节)
 * @param in     输入明文
 * @param in_len 明文长度
 * @param out    输出密文 (长度 = ((in_len/16)+1)*16)
 * @param out_len 输出长度
 * @return 0 成功
 */
int sm4_cbc_encrypt_pkcs7(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t *out_len);

/**
 * @brief 单次 SM4-CBC 解密 (带 PKCS7 填充移除)
 * @param key    密钥 (16 字节)
 * @param iv     初始向量 (16 字节)
 * @param in     输入密文
 * @param in_len 密文长度
 * @param out    输出明文
 * @param out_len 输出长度
 * @return 0 成功
 */
int sm4_cbc_decrypt_pkcs7(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* SM4_H */
