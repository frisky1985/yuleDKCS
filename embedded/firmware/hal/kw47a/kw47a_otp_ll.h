/*
 * kw47a_otp_ll.h — KW47A eFuse OTP 单调编程逻辑（平台无关）
 *
 * 位定义：
 *   word0: bit0 = PROD_LOCK, bit1 = DEBUG_LOCK
 *   word1: unary epoch 水位, bit i 置位表示 epoch >= i+1
 *
 * eFuse 只能 0→1，因此所有操作都是 new = old | mask，并在编程后回读校验。
 */

#ifndef KW47A_OTP_LL_H
#define KW47A_OTP_LL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KW47A_OTP_WORD_LOCK   0u
#define KW47A_OTP_WORD_EPOCH  1u
#define KW47A_OTP_MAX_EPOCH   31u   /* 水位上限（使用 bit0..bit30） */

typedef struct {
    uint32_t (*read_word)(uint32_t idx);
    int (*program_word)(uint32_t idx, uint32_t value);  /* 仅允许 0→1 */
} kw47a_otp_port_t;

/* 绑定端口（按值复制）；参数非法返回 -1。 */
int kw47a_otp_ll_bind(const kw47a_otp_port_t *port);

/* 读锁定状态；未绑定返回 false。 */
bool kw47a_otp_lock_read(void);
bool kw47a_otp_debug_lock_read(void);

/* 置 PROD_LOCK / DEBUG_LOCK；已置位时幂等返回 0；未绑定返回 -1。 */
int kw47a_otp_lock_set(void);
int kw47a_otp_debug_lock_set(void);

/* 统计 word1 低位连续 1 的个数；未绑定返回 0。 */
uint32_t kw47a_otp_epoch_read(void);

/* 置 bit0..epoch-1；epoch 已达成返回 0；epoch > 31 或未绑定返回 -1。 */
int kw47a_otp_ll_epoch_raise(uint32_t epoch);

/*
 * SDK/板级集成方覆盖的弱符号 OTP 端口。
 * 默认实现安全降级：读返回 0，编程返回 -1。
 */
uint32_t kw47a_sdk_otp_read(uint32_t idx);
int kw47a_sdk_otp_program(uint32_t idx, uint32_t value);

/* 绑定上述弱符号端口（供 hal 层使用）。 */
int kw47a_otp_ll_bind_default(void);

#ifdef __cplusplus
}
#endif

#endif /* KW47A_OTP_LL_H */
