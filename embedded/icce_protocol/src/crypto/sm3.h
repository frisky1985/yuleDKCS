/**
 * @file sm3.h
 * @brief SM3 密码杂凑算法
 * @version 1.0
 * @date 2026-05-28
 *
 * GB/T 32905-2016 / GM/T 0004-2012 SM3 密码杂凑算法。
 * 输入消息长度 ≤ 2^64 位, 输出 256 位 (32 字节) 杂凑值。
 *
 * 参考:
 * - GB/T 32905-2016 信息安全技术 SM3 密码杂凑算法
 * - 测试向量见 sm3.c 顶部注释
 */

#ifndef SM3_H
#define SM3_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto_types.h"

/** SM3 上下文 (可栈分配) */
typedef struct {
    uint64_t   total_bits;               /* 已处理位数 */
    uint8_t    block[SM3_BLOCK_SIZE];    /* 64 字节缓冲区 */
    uint32_t   block_len;                /* 缓冲区内字节数 */
    uint32_t   state[8];                 /* 8 × 32 位工作变量 V */
} sm3_ctx_t;

/**
 * @brief 初始化 SM3 上下文
 * @param ctx 上下文指针 (非空)
 * @return CRYPTO_SUCCESS 或 CRYPTO_ERR_NULL_PTR
 */
int sm3_init(sm3_ctx_t *ctx);

/**
 * @brief 更新 SM3 计算 (增量输入)
 * @param ctx  上下文
 * @param data 输入数据
 * @param len  字节长度
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm3_update(sm3_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief 完成 SM3 计算, 输出 32 字节杂凑值
 * @param ctx  上下文 (完成后被清零)
 * @param hash 输出缓冲区 (至少 SM3_DIGEST_SIZE 字节)
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm3_final(sm3_ctx_t *ctx, uint8_t hash[SM3_DIGEST_SIZE]);

/**
 * @brief 单次调用 SM3 杂凑
 * @param data 输入数据
 * @param len  字节长度
 * @param hash 输出 32 字节杂凑值
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm3_hash(const uint8_t *data, size_t len, uint8_t hash[SM3_DIGEST_SIZE]);

/**
 * @brief SM3-HMAC 计算
 * @param key  密钥
 * @param klen 密钥长度
 * @param data 消息
 * @param dlen 消息长度
 * @param mac  输出 MAC (32 字节)
 * @return CRYPTO_SUCCESS 或错误码
 */
int sm3_hmac(const uint8_t *key, size_t klen,
             const uint8_t *data, size_t dlen,
             uint8_t mac[SM3_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SM3_H */
