/******************************************************************************
 * @file    sm3.h
 * @brief   SM3 密码杂凑算法 (GB/T 32905-2016 / GM/T 0004-2012)
 * @note    纯 C 实现，无外部依赖
 *          输出 256-bit (32 字节) 哈希值
 ******************************************************************************/
#ifndef SM3_H
#define SM3_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SM3_DIGEST_LEN  32  /* 256-bit */
#define SM3_BLOCK_SIZE  64  /* 512-bit */

/** SM3 上下文 */
typedef struct {
    uint32_t state[8];      /* 中间状态 (A,B,C,D,E,F,G,H) */
    uint64_t count;         /* 已处理字节数 */
    uint8_t  buffer[64];    /* 当前块缓冲区 */
} sm3_context_t;

/**
 * @brief 初始化 SM3 上下文
 * @param ctx   SM3 上下文 (非 NULL)
 */
void sm3_init(sm3_context_t *ctx);

/**
 * @brief 更新 SM3 哈希 (可多次调用以处理流式数据)
 * @param ctx    SM3 上下文
 * @param data   输入数据
 * @param len    数据长度
 */
void sm3_update(sm3_context_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief 完成 SM3 哈希，输出摘要
 * @param ctx     SM3 上下文
 * @param digest  输出摘要缓冲区 (至少 SM3_DIGEST_LEN 字节)
 */
void sm3_finish(sm3_context_t *ctx, uint8_t *digest);

/**
 * @brief 单次 SM3 哈希
 * @param data     输入数据
 * @param len      数据长度
 * @param digest   输出摘要缓冲区 (至少 SM3_DIGEST_LEN 字节)
 */
void sm3_digest(const uint8_t *data, size_t len, uint8_t *digest);

#ifdef __cplusplus
}
#endif

#endif /* SM3_H */
