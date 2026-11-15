/*
 * kw47a_nv.h — KW47A NV 分区绑定（sec_nv_ops_t）
 *
 * 把 sec_nv 的双 Bank 记录引擎绑定到板级 NV Flash/OTP。
 * 分区布局见 docs/security/nv-persistence-design.md：
 *   0x7C000..0x7FFFF (16KB)，keystore / CRL A/B / LOCK A/B。
 */

#ifndef KW47A_NV_H
#define KW47A_NV_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KW47A_NV_BASE   0x0007C000u
#define KW47A_NV_SIZE   0x00004000u

/* 1 = 使用 eFuse 作为锁定/epoch 根；0 = 仅双 Bank Flash。 */
#ifndef KW47A_HAS_OTP
#define KW47A_HAS_OTP   0
#endif

/*
 * 探测并绑定 NV 后端。
 * 返回 0 表示已绑定，-1 表示硬件不可用（调用方应降级为 RAM-only）。
 * 必须在 sec_security_init() 之前调用。
 */
int      kw47a_nv_init(void);
bool     kw47a_nv_bound(void);
uint32_t kw47a_nv_base(void);

#ifdef __cplusplus
}
#endif

#endif /* KW47A_NV_H */
