/**
 * @file test_security_nv.c
 * @brief keystore / CRL / 产线锁定 NV 持久化测试 (48KB 假 Flash + 故障注入 + OTP 模拟)
 *
 * 覆盖: 空白分区、CRL 往返、篡改回退、OTP 回滚保护、四个掉电断点、
 *       锁定持久化/损坏 fail-closed、上电恢复 CRL、未绑定与参数校验,
 *       以及 keystore 双 Bank 往返/最新代/commit 前掉电/损坏回退/参数校验。
 */

#include "unity.h"
#include "ccc_digital_key.h"
#include "sec_nv.h"
#include "sec_revocation.h"
#include "crypto_engine.h"
#include "crypto_random.h"
#include "p256.h"
#include "cert_fixtures.h"

#include <stddef.h>
#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void);
void tearDown(void);
#endif /* TEST_LIB_MODE */

#define CRL_MAX_LEN SEC_REVOCATION_MAX_BLOB_LEN

/* ── 假 Flash ─────────────────────────────────────────────── */

static uint8_t  g_flash[SEC_NV_FLASH_SIZE];
static int      g_fail_after_writes;   /* -1 = 永不失败 */
static bool     g_fail_erase;
static uint32_t g_write_attempts;

static bool     g_otp_lock;
static uint32_t g_otp_epoch;

static int fake_read(uint32_t off, void *buf, uint16_t len)
{
    if (buf == NULL || off > SEC_NV_FLASH_SIZE ||
        (uint32_t)len > SEC_NV_FLASH_SIZE - off) {
        return -1;
    }
    (void)memcpy(buf, &g_flash[off], len);
    return 0;
}

static int fake_write(uint32_t off, const void *buf, uint16_t len)
{
    if (buf == NULL || off > SEC_NV_FLASH_SIZE ||
        (uint32_t)len > SEC_NV_FLASH_SIZE - off) {
        return -1;
    }
    if (g_fail_after_writes >= 0 &&
        g_write_attempts >= (uint32_t)g_fail_after_writes) {
        g_write_attempts++;
        return -1;
    }
    g_write_attempts++;
    (void)memcpy(&g_flash[off], buf, len);
    return 0;
}

static int fake_erase(uint32_t off, uint16_t len)
{
    if (g_fail_erase) {
        return -1;
    }
    if (off > SEC_NV_FLASH_SIZE || (uint32_t)len > SEC_NV_FLASH_SIZE - off) {
        return -1;
    }
    (void)memset(&g_flash[off], 0xFF, len);
    return 0;
}

static bool fake_otp_lock_read(void)
{
    return g_otp_lock;
}

static int fake_otp_lock_set(void)
{
    g_otp_lock = true;
    return 0;
}

static int fake_otp_epoch_read(uint32_t *epoch)
{
    if (epoch == NULL) {
        return -1;
    }
    *epoch = g_otp_epoch;
    return 0;
}

static int fake_otp_epoch_raise(uint32_t epoch)
{
    if (epoch > g_otp_epoch) {
        g_otp_epoch = epoch;
    }
    return 0;
}

static const sec_nv_ops_t k_ops = {
    fake_read, fake_write, fake_erase,
    fake_otp_lock_read, fake_otp_lock_set,
    fake_otp_epoch_read, fake_otp_epoch_raise
};

/* ── 测试环境 ─────────────────────────────────────────────── */

static void nv_reset_env(void)
{
    sec_test_reset_security();
    sec_nv_reset_for_test();
    (void)memset(g_flash, 0xFF, sizeof(g_flash));
    g_fail_after_writes = -1;
    g_fail_erase = false;
    g_write_attempts = 0;
    g_otp_lock = false;
    g_otp_epoch = 0;
}

static void nv_bind(void)
{
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_bind(&k_ops));
}

/* 模拟复位: 清 RAM 侧状态, 保留 Flash 与 OTP。 */
static void nv_simulate_reboot(void)
{
    sec_test_reset_security();
    sec_nv_reset_for_test();
    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_init());
}

/* ── CRL 测试数据 (P-256 测试设备密钥签名) ────────────────── */

static const uint8_t k_rev_device_a[16] = {
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
    0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF
};
static const uint8_t k_rev_device_b[16] = {
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
    0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF
};
static const uint8_t k_device_id[16] = {
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF
};
static const uint8_t k_key_id[16] = {
    0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x01
};
static const uint8_t k_station_id[16] = {
    0x53,0x54,0x41,0x54,0x49,0x4F,0x4E,0x2D,
    0x30,0x30,0x30,0x31,0x00,0x00,0x00,0x00
};

static uint8_t g_crl[CRL_MAX_LEN + 8];

static void crl_put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void crl_put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint16_t build_crl(uint8_t *blob, uint32_t epoch,
                          const uint8_t *id0, const uint8_t *id1)
{
    uint16_t count = (id0 != NULL) ? 1U : 0U;
    uint16_t signed_len;
    uint8_t  hash[32];

    if (id1 != NULL) {
        count = 2U;
    }

    (void)memset(blob, 0, CRL_MAX_LEN);
    (void)memcpy(blob, "YRV1", 4);
    blob[4] = 1U;                           /* format_version */
    blob[5] = SEC_KEY_TYPE_P256;
    crl_put_u32(&blob[8], epoch);
    crl_put_u32(&blob[12], 0x66000000U);    /* issued_at */
    crl_put_u16(&blob[16], count);

    signed_len = (uint16_t)(SEC_REVOCATION_HEADER_LEN + count * 16U);
    if (id0 != NULL) {
        (void)memcpy(&blob[SEC_REVOCATION_HEADER_LEN], id0, 16);
    }
    if (id1 != NULL) {
        (void)memcpy(&blob[SEC_REVOCATION_HEADER_LEN + 16], id1, 16);
    }

    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS, crypto_sha256(blob, signed_len, hash));
    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_p256_sign(k_device_priv, hash, &blob[signed_len]));
    return (uint16_t)(signed_len + SEC_REVOCATION_SIG_LEN);
}

/* ── 1. 空白分区 ──────────────────────────────────────────── */

static void test_blank_nv(void)
{
    uint8_t  buf[32];
    uint16_t len = 0;
    bool     locked = true;

    nv_reset_env();
    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_init());

    TEST_ASSERT_EQUAL(CCC_ERR_NOT_FOUND, sec_nv_load_lock(&locked));
    TEST_ASSERT_FALSE(locked);
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_FOUND, sec_nv_load_crl(buf, (uint16_t)sizeof(buf), &len));
}

/* ── 2. CRL 保存/复位/加载往返 ────────────────────────────── */

static void test_crl_roundtrip(void)
{
    uint8_t  out[CRL_MAX_LEN];
    uint16_t out_len = 0;
    uint16_t len;

    nv_reset_env();
    nv_bind();

    len = build_crl(g_crl, 3U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_crl(g_crl, len));
    TEST_ASSERT_EQUAL_UINT(3U, g_otp_epoch);

    nv_simulate_reboot();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_load_crl(out, (uint16_t)sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT(len, out_len);
    TEST_ASSERT_EQUAL_MEMORY(g_crl, out, len);
}

/* ── 3. 篡改活动 Bank ─────────────────────────────────────── */

static void test_tampered_bank(void)
{
    uint8_t  out[CRL_MAX_LEN];
    uint8_t  old_blob[CRL_MAX_LEN];
    uint16_t old_len;
    uint16_t out_len = 0;
    uint16_t len;

    /* 只有一个有效 Bank: 篡改后无备份 → SECURITY */
    nv_reset_env();
    nv_bind();
    len = build_crl(g_crl, 2U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_crl(g_crl, len));
    g_flash[SEC_NV_CRL_A_OFF + 40U] ^= 0xFFU;
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY,
                      sec_nv_load_crl(out, (uint16_t)sizeof(out), &out_len));

    /* 两个有效 Bank: 篡改活动 Bank B 后回退到 A */
    nv_reset_env();
    nv_bind();
    old_len = build_crl(old_blob, 2U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_crl(old_blob, old_len));

    len = build_crl(g_crl, 3U, k_rev_device_b, NULL);
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_crl(g_crl, len));

    /* 清除 OTP 水位, 以便验证"回退到另一有效 Bank"而不被防回滚拦截 */
    g_otp_epoch = 0U;
    nv_simulate_reboot();
    g_flash[SEC_NV_CRL_B_OFF + 40U] ^= 0xFFU;
    out_len = 0;
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_load_crl(out, (uint16_t)sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT(old_len, out_len);
    TEST_ASSERT_EQUAL_MEMORY(old_blob, out, old_len);
}

/* ── 4. OTP 水位回滚保护 ──────────────────────────────────── */

static void test_rollback_protection(void)
{
    uint8_t  out[CRL_MAX_LEN];
    uint16_t out_len = 0;
    uint16_t len;

    nv_reset_env();
    nv_bind();
    len = build_crl(g_crl, 3U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_crl(g_crl, len));
    TEST_ASSERT_EQUAL_UINT(3U, g_otp_epoch);

    /* Flash 中被回滚为 epoch 3, 但 OTP 水位已到 5 */
    g_otp_epoch = 5U;
    nv_simulate_reboot();
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_nv_load_crl(out, (uint16_t)sizeof(out), &out_len));
}

/* ── 5. 掉电注入: 擦除后 / 写 payload 中 / commit 前 / commit 后 ── */

static void test_power_cut_recovery(void)
{
    uint8_t  old_blob[CRL_MAX_LEN];
    uint8_t  new_blob[CRL_MAX_LEN];
    uint8_t  out[CRL_MAX_LEN];
    uint8_t  flash_snapshot[SEC_NV_FLASH_SIZE];
    uint16_t old_len;
    uint16_t new_len;
    uint16_t out_len;
    uint32_t epoch_snapshot;
    int      fail_after;

    nv_reset_env();
    nv_bind();

    old_len = build_crl(old_blob, 1U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_crl(old_blob, old_len));
    (void)memcpy(flash_snapshot, g_flash, sizeof(g_flash));
    epoch_snapshot = g_otp_epoch;

    new_len = build_crl(new_blob, 2U, k_rev_device_b, NULL);

    for (fail_after = 0; fail_after <= 4; fail_after++) {
        (void)memcpy(g_flash, flash_snapshot, sizeof(g_flash));
        g_otp_epoch = epoch_snapshot;
        sec_test_reset_security();
        sec_nv_reset_for_test();
        nv_bind();

        /* 每个断点: 0=擦除后, 1=写 payload 中, 2=commit 前, 3+=commit 后 */
        g_fail_after_writes = fail_after;
        g_write_attempts = 0;
        (void)sec_nv_save_crl(new_blob, new_len);
        g_fail_after_writes = -1;

        nv_simulate_reboot();
        out_len = 0;
        TEST_ASSERT_EQUAL(CCC_OK,
                          sec_nv_load_crl(out, (uint16_t)sizeof(out), &out_len));
        {
            bool is_old = (out_len == old_len) &&
                          (memcmp(out, old_blob, old_len) == 0);
            bool is_new = (out_len == new_len) &&
                          (memcmp(out, new_blob, new_len) == 0);
            TEST_ASSERT_TRUE(is_old || is_new);
        }
    }
}

/* ── 6. 上电从 NV 恢复 CRL ────────────────────────────────── */

static void test_init_restores_crl(void)
{
    uint16_t len;

    nv_reset_env();
    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_set_revocation_root(SEC_KEY_TYPE_P256, k_device_pub, 64));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_a));

    len = build_crl(g_crl, 7U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_a));

    /* 复位: RAM 丢失 (吊销根需重新 provision), Flash/OTP 保留 */
    nv_simulate_reboot();
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_a));

    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_set_revocation_root(SEC_KEY_TYPE_P256, k_device_pub, 64));
    TEST_ASSERT_EQUAL(CCC_OK, sec_security_init());
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_a));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_b));
}

/* ── 7. 未绑定 / 参数校验 ─────────────────────────────────── */

static void test_unbound_and_params(void)
{
    uint8_t  buf[CRL_MAX_LEN];
    uint8_t  small[16];
    uint16_t len = 0;
    uint16_t out_len = 0;
    bool     locked = false;

    nv_reset_env();     /* 未绑定 */
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_INIT, sec_nv_init());
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_INIT, sec_nv_save_crl(buf, 32U));
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_INIT, sec_nv_load_crl(buf, (uint16_t)sizeof(buf), &len));
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_INIT, sec_nv_save_lock(NULL));
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_INIT, sec_nv_load_lock(&locked));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_bind(NULL));

    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_init());
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_save_crl(NULL, 32U));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_save_crl(buf, 0U));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_nv_save_crl(buf, (uint16_t)(SEC_NV_MAX_CRL + 1U)));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_nv_load_crl(NULL, (uint16_t)sizeof(buf), &len));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_nv_load_crl(buf, (uint16_t)sizeof(buf), NULL));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_load_lock(NULL));

    /* 调用方缓冲区不足 */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_crl(g_crl, len));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_nv_load_crl(small, (uint16_t)sizeof(small), &out_len));
}

/* ── 8. 产线锁定持久化 (最后一个用例, 会置锁定) ──────────── */

static void test_lock_persistence(void)
{
    bool locked = false;

    /* 保存 → 复位 → 仍为锁定 */
    nv_reset_env();
    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_lock(k_station_id));
    TEST_ASSERT_TRUE(g_otp_lock);

    nv_simulate_reboot();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_load_lock(&locked));
    TEST_ASSERT_TRUE(locked);

    /* 上电初始化恢复锁定, 且 provisioning setter 被拒 */
    TEST_ASSERT_FALSE(sec_provisioning_locked());
    TEST_ASSERT_EQUAL(CCC_OK, sec_security_init());
    TEST_ASSERT_TRUE(sec_provisioning_locked());
    TEST_ASSERT_EQUAL(CCC_ERR_STATE, sec_set_device_identity(k_device_id, k_key_id));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_revocation_root(SEC_KEY_TYPE_P256, k_device_pub, 64));

    /* Lock 记录损坏 (魔数): fail closed */
    nv_reset_env();
    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_lock(NULL));
    g_otp_lock = false;                     /* 排除 OTP 兜底, 只看 Flash */
    g_flash[SEC_NV_LOCK_A_OFF] ^= 0xFFU;
    locked = false;
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY, sec_nv_load_lock(&locked));
    TEST_ASSERT_TRUE(locked);

    sec_test_reset_security();
    sec_nv_reset_for_test();
    nv_bind();
    TEST_ASSERT_FALSE(sec_provisioning_locked());
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY, sec_security_init());
    TEST_ASSERT_TRUE(sec_provisioning_locked());

    /* Lock 记录损坏 (CRC 错但魔数完整): 同样 fail closed */
    nv_reset_env();
    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_lock(k_station_id));
    g_otp_lock = false;
    g_flash[SEC_NV_LOCK_A_OFF + SEC_NV_HDR_SIZE + 8U] ^= 0xFFU;
    locked = false;
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY, sec_nv_load_lock(&locked));
    TEST_ASSERT_TRUE(locked);
}

/* ── 9. keystore 保存/加载往返 (模拟复位) ─────────────────── */

static void test_keystore_roundtrip(void)
{
    uint8_t  blob[1024];
    uint8_t  out[1024];
    uint16_t out_len = 0;
    uint16_t i;

    nv_reset_env();
    nv_bind();

    for (i = 0; i < (uint16_t)sizeof(blob); i++) {
        blob[i] = (uint8_t)(i * 7u + 3u);
    }

    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_keystore(blob, (uint16_t)sizeof(blob)));

    nv_simulate_reboot();
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_nv_load_keystore(out, (uint16_t)sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT(sizeof(blob), out_len);
    TEST_ASSERT_EQUAL_MEMORY(blob, out, sizeof(blob));
}

/* ── 10. 覆盖两代后加载最新, 且 seq 递增 ──────────────────── */

static void test_keystore_latest_generation(void)
{
    uint8_t  gen1[256];
    uint8_t  gen2[256];
    uint8_t  gen3[256];
    uint8_t  out[256];
    uint16_t out_len = 0;
    uint32_t seq_a;
    uint32_t seq_b;

    nv_reset_env();
    nv_bind();

    (void)memset(gen1, 0x11, sizeof(gen1));
    (void)memset(gen2, 0x22, sizeof(gen2));
    (void)memset(gen3, 0x33, sizeof(gen3));

    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_keystore(gen1, (uint16_t)sizeof(gen1)));
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_keystore(gen2, (uint16_t)sizeof(gen2)));

    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_nv_load_keystore(out, (uint16_t)sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT(sizeof(gen2), out_len);
    TEST_ASSERT_EQUAL_MEMORY(gen2, out, sizeof(gen2));

    /* 第 2 代记录落在 Bank B, seq = 1 (小端) */
    seq_b = (uint32_t)g_flash[SEC_NV_KEYSTORE_B_OFF + 8U] |
            ((uint32_t)g_flash[SEC_NV_KEYSTORE_B_OFF + 9U] << 8) |
            ((uint32_t)g_flash[SEC_NV_KEYSTORE_B_OFF + 10U] << 16) |
            ((uint32_t)g_flash[SEC_NV_KEYSTORE_B_OFF + 11U] << 24);
    TEST_ASSERT_EQUAL_UINT(1U, seq_b);

    /* 第 3 代回到 Bank A, seq 继续递增 */
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_keystore(gen3, (uint16_t)sizeof(gen3)));
    seq_a = (uint32_t)g_flash[SEC_NV_KEYSTORE_A_OFF + 8U] |
            ((uint32_t)g_flash[SEC_NV_KEYSTORE_A_OFF + 9U] << 8) |
            ((uint32_t)g_flash[SEC_NV_KEYSTORE_A_OFF + 10U] << 16) |
            ((uint32_t)g_flash[SEC_NV_KEYSTORE_A_OFF + 11U] << 24);
    TEST_ASSERT_EQUAL_UINT(2U, seq_a);

    nv_simulate_reboot();
    out_len = 0;
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_nv_load_keystore(out, (uint16_t)sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT(sizeof(gen3), out_len);
    TEST_ASSERT_EQUAL_MEMORY(gen3, out, sizeof(gen3));
}

/* ── 11. commit 前掉电: 仍加载上一代 ──────────────────────── */

static void test_keystore_power_cut_before_commit(void)
{
    uint8_t  gen1[1024];
    uint8_t  gen2[1024];
    uint8_t  out[1024];
    uint16_t out_len = 0;

    nv_reset_env();
    nv_bind();

    (void)memset(gen1, 0xAA, sizeof(gen1));
    (void)memset(gen2, 0x55, sizeof(gen2));
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_keystore(gen1, (uint16_t)sizeof(gen1)));

    /* 头 + payload 写成功, commit 写失败 */
    g_fail_after_writes = 2;
    g_write_attempts = 0;
    TEST_ASSERT_NOT_EQUAL(CCC_OK, sec_nv_save_keystore(gen2, (uint16_t)sizeof(gen2)));
    g_fail_after_writes = -1;

    nv_simulate_reboot();
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_nv_load_keystore(out, (uint16_t)sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT(sizeof(gen1), out_len);
    TEST_ASSERT_EQUAL_MEMORY(gen1, out, sizeof(gen1));
}

/* ── 12. 损坏回退: 活动 Bank 损坏 → 另一 Bank; 双损坏 → SECURITY ── */

static void test_keystore_corruption_fallback(void)
{
    uint8_t  gen1[128];
    uint8_t  gen2[128];
    uint8_t  out[128];
    uint16_t out_len = 0;

    nv_reset_env();
    nv_bind();

    (void)memset(gen1, 0x01, sizeof(gen1));
    (void)memset(gen2, 0x02, sizeof(gen2));
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_keystore(gen1, (uint16_t)sizeof(gen1)));
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_save_keystore(gen2, (uint16_t)sizeof(gen2)));

    /* 活动 Bank B 的 payload 被篡改 → 回退到 Bank A */
    g_flash[SEC_NV_KEYSTORE_B_OFF + SEC_NV_HDR_SIZE + 4U] ^= 0xFFU;
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_nv_load_keystore(out, (uint16_t)sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT(sizeof(gen1), out_len);
    TEST_ASSERT_EQUAL_MEMORY(gen1, out, sizeof(gen1));

    /* 两个 Bank 均损坏 → 无有效记录, fail closed */
    g_flash[SEC_NV_KEYSTORE_A_OFF + SEC_NV_HDR_SIZE + 4U] ^= 0xFFU;
    out_len = 0;
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY,
                      sec_nv_load_keystore(out, (uint16_t)sizeof(out), &out_len));
}

/* ── 13. keystore 参数校验 ────────────────────────────────── */

static void test_keystore_params(void)
{
    uint8_t  blob[128];
    uint8_t  out[SEC_NV_MAX_KEYSTORE];
    uint16_t len = 0;

    nv_reset_env();
    (void)memset(blob, 0x5A, sizeof(blob));

    /* 未绑定 */
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_INIT,
                      sec_nv_save_keystore(blob, (uint16_t)sizeof(blob)));
    TEST_ASSERT_EQUAL(CCC_ERR_NOT_INIT,
                      sec_nv_load_keystore(out, (uint16_t)sizeof(out), &len));

    nv_bind();
    TEST_ASSERT_EQUAL(CCC_OK, sec_nv_init());

    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_save_keystore(NULL, 64U));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_save_keystore(blob, 0U));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_nv_save_keystore(blob, (uint16_t)(SEC_NV_MAX_KEYSTORE + 1U)));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_load_keystore(NULL, 64U, &len));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_load_keystore(out, 0U, &len));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_nv_load_keystore(out, 64U, NULL));

    /* 调用方缓冲区不足 */
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_nv_save_keystore(blob, (uint16_t)sizeof(blob)));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_nv_load_keystore(out, 64U, &len));
}

#ifndef TEST_LIB_MODE
void setUp(void)
{
    nv_reset_env();
}

void tearDown(void)
{
}
#endif /* TEST_LIB_MODE */

int run_security_nv_tests(void)
{
    crypto_random_init();
    UNITY_BEGIN();

    RUN_TEST(test_blank_nv);
    RUN_TEST(test_crl_roundtrip);
    RUN_TEST(test_tampered_bank);
    RUN_TEST(test_rollback_protection);
    RUN_TEST(test_power_cut_recovery);
    RUN_TEST(test_init_restores_crl);
    RUN_TEST(test_unbound_and_params);
    RUN_TEST(test_keystore_roundtrip);
    RUN_TEST(test_keystore_latest_generation);
    RUN_TEST(test_keystore_power_cut_before_commit);
    RUN_TEST(test_keystore_corruption_fallback);
    RUN_TEST(test_keystore_params);
    RUN_TEST(test_lock_persistence);    /* 终态锁定, 必须最后执行 */

    UNITY_END();
}

#ifndef TEST_SECURITY_NV_NO_MAIN
int main(void)
{
    return run_security_nv_tests();
}
#endif /* TEST_SECURITY_NV_NO_MAIN */
