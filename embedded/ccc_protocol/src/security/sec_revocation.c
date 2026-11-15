/**
 * @file sec_revocation.c
 * @brief 签名设备吊销列表 (CRL) — 安装/查询实现
 *
 * 所有多字节字段显式按小端解析, 从不把 blob 直接映射为结构体。
 * 安装是原子的: 先完整解析/校验到临时结构, 全部通过后才提交到全局状态。
 */

#include "sec_revocation.h"
#include "crypto_engine.h"   /* crypto_sha256 */
#include "p256.h"            /* crypto_p256_verify */
#include "sm2.h"             /* sm2_verify_hash */
#include "sm3.h"             /* sm3_hash */

#include <stddef.h>
#include <string.h>

#define SEC_REV_MAGIC_LEN   4U
#define SEC_REV_VERSION     1U

/* 已安装状态 (静态分配, 无动态内存) */
static uint8_t  g_rev_entries[SEC_REVOCATION_MAX_ENTRIES][SEC_REVOCATION_DEVICE_ID_LEN];
static uint16_t g_rev_count;
static uint32_t g_rev_epoch;
static uint8_t  g_rev_key_type;
static bool     g_rev_installed;

static uint16_t sec_rev_read_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t sec_rev_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void sec_rev_zero(void *ptr, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    size_t i;

    if (ptr == NULL) {
        return;
    }
    for (i = 0; i < len; i++) {
        p[i] = 0;
    }
}

ccc_status_t sec_revocation_install(const uint8_t *blob, uint16_t len,
                                    const uint8_t root_pub[64], uint8_t key_type)
{
    uint8_t  tmp_entries[SEC_REVOCATION_MAX_ENTRIES][SEC_REVOCATION_DEVICE_ID_LEN];
    uint8_t  hash[32];
    uint16_t entry_count;
    uint32_t signed_len;
    uint32_t epoch;
    uint16_t i;
    int      rc;

    /* ── 参数校验 ─────────────────────────────────────────── */
    if (blob == NULL || root_pub == NULL) {
        return CCC_ERR_PARAM;
    }
    if (key_type != SEC_KEY_TYPE_P256 && key_type != SEC_KEY_TYPE_SM2) {
        return CCC_ERR_PARAM;
    }
    if (len < (uint16_t)(SEC_REVOCATION_HEADER_LEN + SEC_REVOCATION_SIG_LEN) ||
        len > (uint16_t)SEC_REVOCATION_MAX_BLOB_LEN) {
        return CCC_ERR_PARAM;
    }

    /* ── 头部字段 ─────────────────────────────────────────── */
    if (blob[0] != (uint8_t)'Y' || blob[1] != (uint8_t)'R' ||
        blob[2] != (uint8_t)'V' || blob[3] != (uint8_t)'1') {
        return CCC_ERR_PARAM;
    }
    if (blob[4] != SEC_REV_VERSION) {
        return CCC_ERR_PARAM;
    }
    if (blob[5] != key_type) {
        return CCC_ERR_PARAM;
    }
    if (blob[6] != 0U || blob[7] != 0U) {
        return CCC_ERR_PARAM;
    }

    entry_count = sec_rev_read_u16(&blob[16]);
    if (entry_count > SEC_REVOCATION_MAX_ENTRIES) {
        return CCC_ERR_PARAM;
    }
    if (blob[18] != 0U || blob[19] != 0U) {
        return CCC_ERR_PARAM;
    }

    signed_len = (uint32_t)SEC_REVOCATION_HEADER_LEN +
                 (uint32_t)entry_count * (uint32_t)SEC_REVOCATION_DEVICE_ID_LEN;
    if ((uint32_t)len != signed_len + (uint32_t)SEC_REVOCATION_SIG_LEN) {
        return CCC_ERR_PARAM;
    }

    epoch = sec_rev_read_u32(&blob[8]);

    /* ── 验签 (blob 全部签名前字节) ───────────────────────── */
    if (key_type == SEC_KEY_TYPE_SM2) {
        rc = sm3_hash(blob, (size_t)signed_len, hash);
        if (rc == CRYPTO_SUCCESS) {
            rc = sm2_verify_hash(root_pub, hash, &blob[signed_len]);
        }
    } else {
        rc = crypto_sha256(blob, (size_t)signed_len, hash);
        if (rc == CRYPTO_SUCCESS) {
            rc = crypto_p256_verify(root_pub, hash, &blob[signed_len]);
        }
    }
    sec_rev_zero(hash, sizeof(hash));

    if (rc != CRYPTO_SUCCESS) {
        return CCC_ERR_SECURITY;
    }

    /* ── 防重放: 只接受严格更新的 epoch ───────────────────── */
    if (g_rev_installed && epoch <= g_rev_epoch) {
        return CCC_ERR_STATE;
    }

    /* ── 解析到临时结构 (此时全部校验已通过) ──────────────── */
    for (i = 0; i < entry_count; i++) {
        (void)memcpy(tmp_entries[i],
                     &blob[SEC_REVOCATION_HEADER_LEN +
                           (uint32_t)i * SEC_REVOCATION_DEVICE_ID_LEN],
                     SEC_REVOCATION_DEVICE_ID_LEN);
    }

    /* ── 原子提交 ─────────────────────────────────────────── */
    sec_rev_zero(g_rev_entries, sizeof(g_rev_entries));
    if (entry_count > 0U) {
        (void)memcpy(g_rev_entries, tmp_entries,
                     (size_t)entry_count * SEC_REVOCATION_DEVICE_ID_LEN);
    }
    g_rev_count = entry_count;
    g_rev_epoch = epoch;
    g_rev_key_type = key_type;
    g_rev_installed = true;

    sec_rev_zero(tmp_entries, sizeof(tmp_entries));
    return CCC_OK;
}

bool sec_revocation_is_revoked(const uint8_t device_id[16])
{
    uint16_t i;

    if (device_id == NULL || !g_rev_installed) {
        return false;
    }

    for (i = 0; i < g_rev_count; i++) {
        if (memcmp(g_rev_entries[i], device_id, SEC_REVOCATION_DEVICE_ID_LEN) == 0) {
            return true;
        }
    }
    return false;
}

ccc_status_t sec_revocation_epoch(uint32_t *epoch)
{
    if (epoch == NULL) {
        return CCC_ERR_PARAM;
    }
    if (!g_rev_installed) {
        return CCC_ERR_STATE;
    }
    *epoch = g_rev_epoch;
    return CCC_OK;
}

void sec_revocation_reset(void)
{
    sec_rev_zero(g_rev_entries, sizeof(g_rev_entries));
    g_rev_count = 0;
    g_rev_epoch = 0;
    g_rev_key_type = 0;
    g_rev_installed = false;
}
