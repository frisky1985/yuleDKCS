/**
 * @file sec_nv.h
 * @brief CRL / keystore / 产线锁定状态的掉电保持 (NV 分区, 平台通过 ops 注入)
 *
 * 分区 (512KB Flash, 48KB NV = 6 x 8KB 物理扇区, 基址 0x74000):
 *   +0x0000 8KB  Key store Bank A (由 key_mgmt 使用, 本模块不解析)
 *   +0x2000 8KB  Key store Bank B
 *   +0x4000 8KB  CRL Bank A
 *   +0x6000 8KB  CRL Bank B
 *   +0x8000 8KB  Lock Bank A
 *   +0xA000 8KB  Lock Bank B
 *
 * 每个 Bank 独占一个 8KB 物理扇区, 擦除粒度为扇区; Bank 内的记录只占
 * 头部 + payload, 其余保持 0xFF。
 *
 * 记录格式 (32B 头, 全部小端显式读写):
 *   0  magic "YRNV" (0x564E5259)
 *   4  type (1=CRL, 2=LOCK, 3=KEYSTORE)
 *   5  version = 1
 *   6  flags (u16, CRL/LOCK=0; KEYSTORE=epoch, OTP 1..62 / 无 OTP 1..255)
 *   8  seq (u32)
 *   12 payload_len (u16)
 *   14 reserved (u16)
 *   16 crc32 (覆盖头 [0,16) + payload)
 *   20 commit "COMT" (0x544D4F43, 最后写入)
 *   24 reserved[8]
 */

#ifndef SEC_NV_H
#define SEC_NV_H

#include "ccc_digital_key.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEC_NV_FLASH_BASE      0x00074000u  /* 512KB 方案 A, 6 x 8KB 扇区 */
#define SEC_NV_FLASH_SIZE      0x0000C000u  /* 48KB */
#define SEC_NV_KEYSTORE_OFF    0x0000u
#define SEC_NV_KEYSTORE_A_OFF  0x0000u
#define SEC_NV_KEYSTORE_B_OFF  0x00002000u
#define SEC_NV_KEYSTORE_BANK_SIZE 0x2000u   /* 8KB */
#define SEC_NV_CRL_A_OFF       0x00004000u
#define SEC_NV_CRL_B_OFF       0x00006000u
#define SEC_NV_CRL_BANK_SIZE   0x2000u      /* 8KB */
#define SEC_NV_LOCK_A_OFF      0x00008000u
#define SEC_NV_LOCK_B_OFF      0x0000A000u
#define SEC_NV_LOCK_BANK_SIZE  0x2000u      /* 8KB */
#define SEC_NV_MAX_KEYSTORE    6144u
#define SEC_NV_MAX_CRL         640u
#define SEC_NV_KEYSTORE_MAX_EPOCH 62u   /* OTP word2+word3 一元编码 */

#define SEC_NV_HDR_SIZE        32u
#define SEC_NV_RECORD_MAGIC    0x564E5259u  /* "YRNV" little-endian */
#define SEC_NV_COMMIT_MAGIC    0x544D4F43u  /* "COMT" little-endian */
#define SEC_NV_TYPE_CRL        1u
#define SEC_NV_TYPE_LOCK       2u
#define SEC_NV_TYPE_KEYSTORE   3u
#define SEC_NV_VERSION         1u

/* CRL 记录 payload = 签名 CRL blob + 8B 元数据; Lock payload 固定 24B. */
#define SEC_NV_CRL_META_SIZE   8u
#define SEC_NV_LOCK_PAYLOAD    24u

typedef struct {
    int  (*read)(uint32_t off, void *buf, uint16_t len);
    int  (*write)(uint32_t off, const void *buf, uint16_t len);
    int  (*erase)(uint32_t off, uint16_t len);
    bool (*otp_lock_read)(void);
    int  (*otp_lock_set)(void);
    int  (*otp_epoch_read)(uint32_t *epoch);
    int  (*otp_epoch_raise)(uint32_t epoch);
    int  (*otp_keystore_epoch_read)(uint32_t *epoch);
    int  (*otp_keystore_epoch_raise)(uint32_t epoch);
} sec_nv_ops_t;

/** 绑定平台 NV/OTP 回调 (结构体按值复制)。未绑定时所有读/写返回 CCC_ERR_NOT_INIT。 */
ccc_status_t sec_nv_bind(const sec_nv_ops_t *ops);

/** 校验 ops 与分区尺寸; 不做记录缓存 (每次操作重新扫描双 Bank)。 */
ccc_status_t sec_nv_init(void);

/** 保存签名 CRL blob (含 epoch); 成功后抬高 OTP epoch 水位。 */
ccc_status_t sec_nv_save_crl(const uint8_t *blob, uint16_t len);

/** 加载最新有效 CRL blob; epoch 低于 OTP 水位时返回 CCC_ERR_STATE。 */
ccc_status_t sec_nv_load_crl(uint8_t *blob, uint16_t cap, uint16_t *len);

/** 保存 keystore blob; 记录头 flags 为递增 epoch, Flash 提交后抬高 OTP 水位。 */
ccc_status_t sec_nv_save_keystore(const uint8_t *blob, uint16_t len);

/** 加载最新有效 keystore blob; epoch 低于 OTP 水位时返回 CCC_ERR_SECURITY。 */
ccc_status_t sec_nv_load_keystore(uint8_t *blob, uint16_t cap, uint16_t *len);

/** 保存锁定记录 (station_id 可为 NULL); 成功后置 OTP 量产锁定。 */
ccc_status_t sec_nv_save_lock(const uint8_t station_id[16]);

/** 读取锁定状态: Flash 记录为锁或 OTP 已锁 → *locked = true。 */
ccc_status_t sec_nv_load_lock(bool *locked);

/** 测试用: 解除绑定并清空内部临时状态。 */
void         sec_nv_reset_for_test(void);

#ifdef __cplusplus
}
#endif

#endif /* SEC_NV_H */
