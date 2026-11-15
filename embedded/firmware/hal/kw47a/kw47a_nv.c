/*
 * kw47a_nv.c — 将 sec_nv 引擎绑定到 KW47A NV Flash / OTP
 *
 * 业务层只通过 sec_nv_ops_t 访问持久化，不出现绝对地址；
 * 本文件负责偏移边界检查与板级 HAL 映射。
 */

#include "kw47a_nv.h"

#include <stddef.h>
#include <string.h>

#include "hal_interface.h"
#include "sec_nv.h"
#if KW47A_HAS_OTP
#include "kw47a_otp_ll.h"
#endif

static bool s_bound;
static bool s_hw_inited;

#if defined(KW47A_MCUX_SDK)
/* MCUX PFLASH 适配层（kw47a_flash_mcux.c，仅 SDK 构建存在）。 */
int kw47a_flash_mcux_bind(void);
#endif

/*
 * 绑定 Flash 硬件端口。非 SDK 构建直接返回 -1（主机保持 RAM-only）。
 * kw47a_nv_init() 只调用一次；失败则保持未绑定。
 */
int kw47a_nv_flash_hw_init(void)
{
#if defined(KW47A_MCUX_SDK)
    return kw47a_flash_mcux_bind();
#else
    return -1;
#endif
}

static int nv_read(uint32_t off, void *buf, uint16_t len)
{
    if (buf == NULL || off >= KW47A_NV_SIZE ||
        (uint32_t)len > (KW47A_NV_SIZE - off)) {
        return -1;
    }
    return hal_nv_flash_read(off, buf, len);
}

static int nv_write(uint32_t off, const void *buf, uint16_t len)
{
    if (buf == NULL || off >= KW47A_NV_SIZE ||
        (uint32_t)len > (KW47A_NV_SIZE - off)) {
        return -1;
    }
    return hal_nv_flash_write(off, buf, len);
}

static int nv_erase(uint32_t off, uint16_t len)
{
    if (off >= KW47A_NV_SIZE || (uint32_t)len > (KW47A_NV_SIZE - off)) {
        return -1;
    }
    return hal_nv_flash_erase(off, len);
}

#if KW47A_HAS_OTP
static bool otp_lock_read_cb(void)
{
    return hal_otp_lock_read() > 0;
}

static int otp_lock_set_cb(void)
{
    return hal_otp_lock_set();
}

static int otp_epoch_read_cb(uint32_t *epoch)
{
    return hal_otp_epoch_read(epoch);
}

static int otp_epoch_raise_cb(uint32_t epoch)
{
    return hal_otp_epoch_raise(epoch);
}

static int otp_keystore_epoch_read_cb(uint32_t *epoch)
{
    if (epoch == NULL) {
        return -1;
    }
    *epoch = kw47a_otp_keystore_epoch_read();
    return 0;
}

static int otp_keystore_epoch_raise_cb(uint32_t epoch)
{
    return kw47a_otp_keystore_epoch_raise(epoch);
}
#endif

static const sec_nv_ops_t s_ops = {
    .read = nv_read,
    .write = nv_write,
    .erase = nv_erase,
#if KW47A_HAS_OTP
    .otp_lock_read = otp_lock_read_cb,
    .otp_lock_set = otp_lock_set_cb,
    .otp_epoch_read = otp_epoch_read_cb,
    .otp_epoch_raise = otp_epoch_raise_cb,
    .otp_keystore_epoch_read = otp_keystore_epoch_read_cb,
    .otp_keystore_epoch_raise = otp_keystore_epoch_raise_cb,
#else
    .otp_lock_read = NULL,
    .otp_lock_set = NULL,
    .otp_epoch_read = NULL,
    .otp_epoch_raise = NULL,
    .otp_keystore_epoch_read = NULL,
    .otp_keystore_epoch_raise = NULL,
#endif
};

int kw47a_nv_init(void)
{
    uint8_t probe[4];

    s_bound = false;

    if (!s_hw_inited) {
        s_hw_inited = true;
        (void)kw47a_nv_flash_hw_init();
    }

    /* 探测：读失败（如 host 桩返回 -1）则保持未绑定，降级 RAM-only。 */
    if (hal_nv_flash_read(0U, probe, sizeof(probe)) != 0) {
        return -1;
    }
#if KW47A_HAS_OTP
    (void)kw47a_otp_ll_bind_default();
#endif
    if (sec_nv_bind(&s_ops) != CCC_OK) {
        return -1;
    }
    if (sec_nv_init() != CCC_OK) {
        return -1;
    }

    s_bound = true;
    return 0;
}

bool kw47a_nv_bound(void)
{
    return s_bound;
}

uint32_t kw47a_nv_base(void)
{
    return KW47A_NV_BASE;
}
