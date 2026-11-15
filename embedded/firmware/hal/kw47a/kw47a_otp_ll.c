/*
 * kw47a_otp_ll.c — KW47A eFuse OTP 单调编程逻辑（平台无关）
 *
 * 规则：
 *   - 编程值 new = old | mask（只允许 0→1）；
 *   - 编程后回读必须等于 new，否则 -1；
 *   - epoch > 31 拒绝；未绑定读返回 false/0、写返回 -1。
 */

#include "kw47a_otp_ll.h"

#include <string.h>

#define KW47A_OTP_BIT_LOCK   0x1u
#define KW47A_OTP_BIT_DEBUG  0x2u

static kw47a_otp_port_t s_port;
static bool s_bound;

int kw47a_otp_ll_bind(const kw47a_otp_port_t *port)
{
    if (port == NULL || port->read_word == NULL || port->program_word == NULL) {
        return -1;
    }
    s_port = *port;
    s_bound = true;
    return 0;
}

/* 先编程再回读校验。 */
static int otp_program_checked(uint32_t idx, uint32_t value)
{
    if (!s_bound) {
        return -1;
    }
    if (s_port.program_word(idx, value) != 0) {
        return -1;
    }
    return (s_port.read_word(idx) == value) ? 0 : -1;
}

bool kw47a_otp_lock_read(void)
{
    if (!s_bound) {
        return false;
    }
    return (s_port.read_word(KW47A_OTP_WORD_LOCK) & KW47A_OTP_BIT_LOCK) != 0u;
}

bool kw47a_otp_debug_lock_read(void)
{
    if (!s_bound) {
        return false;
    }
    return (s_port.read_word(KW47A_OTP_WORD_LOCK) & KW47A_OTP_BIT_DEBUG) != 0u;
}

static int otp_set_bits(uint32_t bit)
{
    uint32_t old;
    uint32_t new_value;

    if (!s_bound) {
        return -1;
    }
    old = s_port.read_word(KW47A_OTP_WORD_LOCK);
    new_value = old | bit;
    if (new_value == old) {
        return 0;                           /* 幂等 */
    }
    return otp_program_checked(KW47A_OTP_WORD_LOCK, new_value);
}

int kw47a_otp_lock_set(void)
{
    return otp_set_bits(KW47A_OTP_BIT_LOCK);
}

int kw47a_otp_debug_lock_set(void)
{
    return otp_set_bits(KW47A_OTP_BIT_DEBUG);
}

uint32_t kw47a_otp_epoch_read(void)
{
    uint32_t word;
    uint32_t n = 0;

    if (!s_bound) {
        return 0;
    }
    word = s_port.read_word(KW47A_OTP_WORD_EPOCH);
    while (n < KW47A_OTP_MAX_EPOCH && ((word >> n) & 0x1u) != 0u) {
        n++;
    }
    return n;
}

int kw47a_otp_ll_epoch_raise(uint32_t epoch)
{
    uint32_t mask;
    uint32_t old;
    uint32_t new_value;

    if (!s_bound || epoch > KW47A_OTP_MAX_EPOCH) {
        return -1;
    }
    if (epoch == 0u) {
        return 0;
    }
    mask = ((uint32_t)1u << epoch) - 1u;
    old = s_port.read_word(KW47A_OTP_WORD_EPOCH);
    if ((old & mask) == mask) {
        return 0;                           /* 水位已达成 */
    }
    new_value = old | mask;
    return otp_program_checked(KW47A_OTP_WORD_EPOCH, new_value);
}

/* ── 默认弱符号端口（板级/SDK 集成方可覆盖）────────────────── */

__attribute__((weak)) uint32_t kw47a_sdk_otp_read(uint32_t idx)
{
    (void)idx;
    return 0u;
}

__attribute__((weak)) int kw47a_sdk_otp_program(uint32_t idx, uint32_t value)
{
    (void)idx;
    (void)value;
    return -1;                              /* 未接入 eFuse 驱动，安全降级 */
}

int kw47a_otp_ll_bind_default(void)
{
    const kw47a_otp_port_t port = {
        .read_word = kw47a_sdk_otp_read,
        .program_word = kw47a_sdk_otp_program,
    };

    return kw47a_otp_ll_bind(&port);
}
