/**
 * @file sec_revocation.c
 * @brief 签名设备吊销列表 (CRL) — 安装/查询实现
 *
 * 所有多字节字段显式按小端解析, 从不把 blob 直接映射为结构体。
 * 安装是原子的: 先完整解析/校验到临时结构, 全部通过后才提交到全局状态。
 */

#include "sec_revocation.h"
#include "sec_nv.h"          /* CRL 持久化 */
#include "crypto_engine.h"   /* crypto_sha256 */
#include "p256.h"            /* crypto_p256_verify */
#include "sm2.h"             /* sm2_verify_hash */
#include "sm3.h"             /* sm3_hash */

#include <stddef.h>
#include <string.h>

/* 已安装状态 (静态分配, 无动态内存) */
static uint8_t  g_rev_entries[SEC_REVOCATION_MAX_ENTRIES][SEC_REVOCATION_DEVICE_ID_LEN];
static uint16_t g_rev_count;
static uint32_t g_rev_epoch;
static uint8_t  g_rev_key_type;
static uint8_t  g_rev_format;      /* 1 = raw device_id, 2 = token */
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

/* v2 令牌匹配: token = SHA-256(domain || data)[0..15]。 */
static bool sec_rev_token_matches(const uint8_t token[SEC_REVOCATION_TOKEN_LEN],
                                  uint8_t domain, const uint8_t *data, uint8_t len)
{
    uint8_t buf[1U + 64U];
    uint8_t hash[32];
    bool    hit;

    if (token == NULL || data == NULL || len == 0U || len > 64U) {
        return false;
    }

    buf[0] = domain;
    (void)memcpy(&buf[1], data, (size_t)len);
    if (crypto_sha256(buf, (size_t)len + 1U, hash) != CRYPTO_SUCCESS) {
        sec_rev_zero(buf, sizeof(buf));
        return false;
    }
    hit = (memcmp(hash, token, SEC_REVOCATION_TOKEN_LEN) == 0);
    sec_rev_zero(hash, sizeof(hash));
    sec_rev_zero(buf, sizeof(buf));
    return hit;
}

static ccc_status_t install_impl(const uint8_t *blob, uint16_t len,
                                 const uint8_t root_pub[64], uint8_t key_type,
                                 bool persist)
{
    uint8_t  tmp_entries[SEC_REVOCATION_MAX_ENTRIES][SEC_REVOCATION_DEVICE_ID_LEN];
    uint8_t  hash[32];
    uint8_t  format;
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
    if (memcmp(blob, "YRV1", 4) == 0) {
        format = SEC_REVOCATION_VERSION_V1;
    } else if (memcmp(blob, "YRV2", 4) == 0) {
        format = SEC_REVOCATION_VERSION_V2;
    } else {
        return CCC_ERR_PARAM;
    }
    if (blob[4] != format) {
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
    g_rev_format = format;
    g_rev_installed = true;

    sec_rev_zero(tmp_entries, sizeof(tmp_entries));

    /* RAM 已原子更新; 持久化失败时返回该错误 (未绑定 NV 视为降级). */
    if (persist) {
        ccc_status_t nv = sec_nv_save_crl(blob, len);
        if (nv != CCC_OK && nv != CCC_ERR_NOT_INIT) {
            return nv;
        }
    }
    return CCC_OK;
}

ccc_status_t sec_revocation_install(const uint8_t *blob, uint16_t len,
                                    const uint8_t root_pub[64], uint8_t key_type)
{
    return install_impl(blob, len, root_pub, key_type, true);
}

ccc_status_t sec_revocation_restore(const uint8_t *blob, uint16_t len,
                                    const uint8_t root_pub[64], uint8_t key_type)
{
    return install_impl(blob, len, root_pub, key_type, false);
}

bool sec_revocation_is_revoked(const uint8_t device_id[16])
{
    uint16_t i;

    if (device_id == NULL || !g_rev_installed) {
        return false;
    }

    for (i = 0; i < g_rev_count; i++) {
        if (g_rev_format == SEC_REVOCATION_VERSION_V2) {
            if (sec_rev_token_matches(g_rev_entries[i],
                                      SEC_REVOCATION_DOMAIN_DEVICE_ID,
                                      device_id,
                                      SEC_REVOCATION_DEVICE_ID_LEN)) {
                return true;
            }
        } else if (memcmp(g_rev_entries[i], device_id,
                          SEC_REVOCATION_DEVICE_ID_LEN) == 0) {
            return true;
        }
    }
    return false;
}

bool sec_revocation_is_serial_revoked(const uint8_t *serial, uint8_t len)
{
    uint16_t i;

    if (serial == NULL || len == 0U || len > 20U) {
        return false;
    }
    if (!g_rev_installed || g_rev_format != SEC_REVOCATION_VERSION_V2) {
        return false;
    }

    for (i = 0; i < g_rev_count; i++) {
        if (sec_rev_token_matches(g_rev_entries[i],
                                  SEC_REVOCATION_DOMAIN_SERIAL,
                                  serial, len)) {
            return true;
        }
    }
    return false;
}

bool sec_revocation_is_pubkey_revoked(const uint8_t pub[64])
{
    uint16_t i;

    if (pub == NULL) {
        return false;
    }
    if (!g_rev_installed || g_rev_format != SEC_REVOCATION_VERSION_V2) {
        return false;
    }

    for (i = 0; i < g_rev_count; i++) {
        if (sec_rev_token_matches(g_rev_entries[i],
                                  SEC_REVOCATION_DOMAIN_PUBKEY,
                                  pub, 64U)) {
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
    g_rev_format = 0;
    g_rev_installed = false;
}
