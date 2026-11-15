/**
 * @file sec_revocation.h
 * @brief 签名设备吊销列表 (CRL) — 安装/查询
 *
 * 序列化格式 (全部小端, 固定布局, 禁止直接 memcpy 结构体):
 *
 *   offset  size  field
 *   0       4     magic "YRV1"
 *   4       1     format_version = 1
 *   5       1     key_type (1 = P256, 2 = SM2)
 *   6       2     reserved = 0
 *   8       4     epoch (u32)
 *   12      4     issued_at (u32)
 *   16      2     entry_count (u16, <= 32)
 *   18      2     reserved = 0
 *   20      16*n  device_id[]
 *   20+16n  64    signature (对前面全部字节的哈希签名)
 *
 * 签名前的数据用 SHA-256 (P-256) 或 SM3 (SM2) 做哈希。
 */

#ifndef SEC_REVOCATION_H
#define SEC_REVOCATION_H

#include "ccc_digital_key.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEC_REVOCATION_MAX_ENTRIES      32U
#define SEC_REVOCATION_DEVICE_ID_LEN    16U
#define SEC_REVOCATION_SIG_LEN          64U
#define SEC_REVOCATION_HEADER_LEN       20U
#define SEC_REVOCATION_MAX_BLOB_LEN     \
    (SEC_REVOCATION_HEADER_LEN + \
     SEC_REVOCATION_MAX_ENTRIES * SEC_REVOCATION_DEVICE_ID_LEN + \
     SEC_REVOCATION_SIG_LEN)

/**
 * @brief 安装/更新签名吊销列表。
 *
 * @param blob     序列化列表
 * @param len      实际长度 (必须与 entry_count 严格一致)
 * @param root_pub 签发者公钥 X||Y (64 字节), 由设备预置, 绝不来自对端
 * @param key_type 签名算法 (SEC_KEY_TYPE_P256 / SEC_KEY_TYPE_SM2)
 *
 * @retval CCC_OK           安装成功, 原子替换已安装列表
 * @retval CCC_ERR_PARAM    魔数/版本/算法/entry_count/长度/保留位非法
 * @retval CCC_ERR_SECURITY 验签失败 (已安装列表保持不变)
 * @retval CCC_ERR_STATE    epoch <= 已安装 epoch (防重放, 已安装列表保持不变)
 */
ccc_status_t sec_revocation_install(const uint8_t *blob, uint16_t len,
                                    const uint8_t root_pub[64], uint8_t key_type);

/**
 * @brief 上电恢复: 与 install 相同的校验, 但绝不写回 NV。
 *
 * @retval CCC_OK           恢复成功
 * @retval CCC_ERR_PARAM    格式非法
 * @retval CCC_ERR_SECURITY 验签失败
 * @retval CCC_ERR_STATE    epoch <= 已恢复 epoch
 */
ccc_status_t sec_revocation_restore(const uint8_t *blob, uint16_t len,
                                    const uint8_t root_pub[64], uint8_t key_type);

/** 查询 device_id 是否在已安装列表中; 未安装任何列表时恒为 false。 */
bool         sec_revocation_is_revoked(const uint8_t device_id[16]);

/** 读取已安装列表的 epoch; 未安装时返回 CCC_ERR_STATE 且不写 *epoch。 */
ccc_status_t sec_revocation_epoch(uint32_t *epoch);

/** 工厂返修/测试用: 清零已安装列表与 epoch。 */
void         sec_revocation_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SEC_REVOCATION_H */
