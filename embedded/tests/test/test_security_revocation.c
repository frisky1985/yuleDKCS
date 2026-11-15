/**
 * @file test_security_revocation.c
 * @brief 签名设备吊销列表 (CRL) 与产线配置锁定测试
 *
 * CRL 用测试夹具中的 k_device_priv / k_device_pub (P-256) 作为签发密钥：
 * 组装小端 blob -> SHA-256 -> crypto_p256_sign。
 */

#include "unity.h"
#include "ccc_digital_key.h"
#include "sec_revocation.h"
#include "crypto_engine.h"
#include "crypto_random.h"
#include "p256.h"
#include "cert_fixtures.h"

#include <stddef.h>
#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

#define CRL_MAX_LEN SEC_REVOCATION_MAX_BLOB_LEN

static const uint8_t k_rev_device_a[16] = {
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
    0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF
};
static const uint8_t k_rev_device_b[16] = {
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
    0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF
};
static const uint8_t k_rev_device_c[16] = {
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,
    0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF
};

static const uint8_t k_device_id[16] = {
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF
};
static const uint8_t k_key_id[16] = {
    0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x01
};
static const uint8_t k_fw_hash[32] = {
    0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,
    0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,
    0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,
    0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A
};

/* 2050-01-01, after the fixture notAfter. */
#define T_FUTURE  2524608000U

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

/* 写入头部 (不含签名), 返回签名前长度。 */
static uint16_t crl_begin(uint8_t *blob, uint32_t epoch, uint16_t count)
{
    (void)memset(blob, 0, CRL_MAX_LEN);
    (void)memcpy(blob, "YRV1", 4);
    blob[4] = 1U;                       /* format_version */
    blob[5] = SEC_KEY_TYPE_P256;        /* key_type */
    crl_put_u32(&blob[8], epoch);
    crl_put_u32(&blob[12], 0x66000000U);/* issued_at */
    crl_put_u16(&blob[16], count);
    return (uint16_t)(SEC_REVOCATION_HEADER_LEN + (uint32_t)count * 16U);
}

/* 对签名前字节做 SHA-256 并用测试设备私钥签名, 返回最终 blob 长度。 */
static uint16_t crl_seal(uint8_t *blob, uint16_t signed_len)
{
    uint8_t hash[32];

    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS, crypto_sha256(blob, signed_len, hash));
    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_p256_sign(k_device_priv, hash, &blob[signed_len]));
    return (uint16_t)(signed_len + SEC_REVOCATION_SIG_LEN);
}

/* 便捷构造: 0/1/2 条吊销记录。 */
static uint16_t build_crl(uint8_t *blob, uint32_t epoch,
                          const uint8_t *id0, const uint8_t *id1)
{
    uint16_t count = (id1 != NULL) ? 2U : ((id0 != NULL) ? 1U : 0U);
    uint16_t signed_len = crl_begin(blob, epoch, count);

    if (id0 != NULL) {
        (void)memcpy(&blob[SEC_REVOCATION_HEADER_LEN], id0, 16);
    }
    if (id1 != NULL) {
        (void)memcpy(&blob[SEC_REVOCATION_HEADER_LEN + 16], id1, 16);
    }
    return crl_seal(blob, signed_len);
}

/* ── 1. 安装与查询 ────────────────────────────────────────── */

static void test_install_and_query(void)
{
    uint32_t epoch = 0;
    uint16_t len;

    sec_revocation_reset();
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_a));

    len = build_crl(g_crl, 5U, k_rev_device_a, k_rev_device_b);
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    TEST_ASSERT_EQUAL(CCC_OK, sec_revocation_epoch(&epoch));
    TEST_ASSERT_EQUAL_UINT(5U, epoch);
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_a));
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_b));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_c));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(NULL));
}

/* ── 2. 篡改条目 / 篡改签名 / 错误公钥 ───────────────────── */

static void test_install_rejects_tampered(void)
{
    uint32_t epoch = 0;
    uint16_t len;

    sec_revocation_reset();
    len = build_crl(g_crl, 5U, k_rev_device_a, k_rev_device_b);
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 篡改条目内容 (签名不再匹配) */
    g_crl[SEC_REVOCATION_HEADER_LEN] ^= 0xFFU;
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 篡改签名 */
    len = build_crl(g_crl, 5U, k_rev_device_a, k_rev_device_b);
    g_crl[len - 1U] ^= 0x01U;
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 用错误公钥验签 */
    len = build_crl(g_crl, 5U, k_rev_device_a, k_rev_device_b);
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY,
                      sec_revocation_install(g_crl, len, k_wrong_root_pub,
                                             SEC_KEY_TYPE_P256));

    /* 原列表保持不变 */
    TEST_ASSERT_EQUAL(CCC_OK, sec_revocation_epoch(&epoch));
    TEST_ASSERT_EQUAL_UINT(5U, epoch);
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_a));
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_b));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_c));
}

/* ── 3. epoch 防重放 ──────────────────────────────────────── */

static void test_epoch_replay_rejected(void)
{
    uint32_t epoch = 0;
    uint16_t len;

    sec_revocation_reset();

    len = build_crl(g_crl, 5U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 相同 epoch: 拒绝, 列表不变 */
    len = build_crl(g_crl, 5U, k_rev_device_b, NULL);
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_b));
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_a));

    /* 更低 epoch: 拒绝 */
    len = build_crl(g_crl, 4U, k_rev_device_b, NULL);
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));
    TEST_ASSERT_EQUAL(CCC_OK, sec_revocation_epoch(&epoch));
    TEST_ASSERT_EQUAL_UINT(5U, epoch);

    /* 更高 epoch: 成功, 原子替换 */
    len = build_crl(g_crl, 6U, k_rev_device_b, NULL);
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));
    TEST_ASSERT_EQUAL(CCC_OK, sec_revocation_epoch(&epoch));
    TEST_ASSERT_EQUAL_UINT(6U, epoch);
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_b));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_a));
}

/* ── 4. 格式与参数校验 ────────────────────────────────────── */

static void test_format_and_arg_validation(void)
{
    uint32_t epoch = 0;
    uint16_t len;

    sec_revocation_reset();
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);

    /* NULL / 零长度 */
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(NULL, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, len, NULL,
                                             SEC_KEY_TYPE_P256));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, 0U, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 坏魔数 */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    g_crl[0] = (uint8_t)'X';
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 坏版本 */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    g_crl[4] = 2U;
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 坏 key_type (头部) */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    g_crl[5] = 3U;
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, len, k_device_pub, 3U));

    /* 头部 key_type 与参数不一致 */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_SM2));

    /* 保留位非零 */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    g_crl[6] = 1U;
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 截断 */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, (uint16_t)(len - 1U),
                                             k_device_pub, SEC_KEY_TYPE_P256));

    /* entry_count > 32 */
    len = build_crl(g_crl, 1U, NULL, NULL);
    crl_put_u16(&g_crl[16], 33U);
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    /* 超长 */
    len = build_crl(g_crl, 1U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM,
                      sec_revocation_install(g_crl,
                                             (uint16_t)(SEC_REVOCATION_MAX_BLOB_LEN + 1U),
                                             k_device_pub, SEC_KEY_TYPE_P256));

    /* 未安装任何列表: epoch 不可读 */
    TEST_ASSERT_EQUAL(CCC_ERR_STATE, sec_revocation_epoch(&epoch));
    TEST_ASSERT_EQUAL(CCC_ERR_PARAM, sec_revocation_epoch(NULL));
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_a));
}

/* ── 5. reset ─────────────────────────────────────────────── */

static void test_reset_clears_state(void)
{
    uint32_t epoch = 0;
    uint16_t len;

    sec_revocation_reset();
    len = build_crl(g_crl, 3U, k_rev_device_a, NULL);
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));
    TEST_ASSERT_TRUE(sec_revocation_is_revoked(k_rev_device_a));

    sec_revocation_reset();
    TEST_ASSERT_FALSE(sec_revocation_is_revoked(k_rev_device_a));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE, sec_revocation_epoch(&epoch));
}

/* ── 6. 吊销 device_id 的 attestation 被拒 ───────────────── */

static void build_attestation(ccc_attestation_t *att)
{
    uint8_t hash[32];

    (void)memset(att, 0, sizeof(*att));
    att->version = 0x02;
    att->key_type = SEC_KEY_TYPE_P256;
    (void)memcpy(att->device_id, k_device_id, sizeof(k_device_id));
    (void)memcpy(att->key_id, k_key_id, sizeof(k_key_id));
    (void)memcpy(att->firmware_hash, k_fw_hash, sizeof(k_fw_hash));
    att->security_state = 0x01;

    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_sha256((const uint8_t *)att,
                                    (size_t)offsetof(ccc_attestation_t, signature),
                                    hash));
    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_p256_sign(k_device_priv, hash, att->signature));
}

static void test_attestation_revoked_device(void)
{
    ccc_attestation_t att;
    uint16_t len;

    sec_revocation_reset();
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_device_identity(k_device_id, k_key_id));
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_firmware_measurement(k_fw_hash));
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_set_device_public_key(SEC_KEY_TYPE_P256,
                                                k_device_pub, 64));

    build_attestation(&att);
    TEST_ASSERT_EQUAL(VERIFY_OK, sec_verify_attestation(&att));

    /* 吊销该 device_id */
    len = build_crl(g_crl, 1U, k_device_id, NULL);
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_revocation_install(g_crl, len, k_device_pub,
                                             SEC_KEY_TYPE_P256));

    TEST_ASSERT_EQUAL(VERIFY_KEY_REVOKED, sec_verify_attestation(&att));

    /* 撤销列表命中前必须先验签通过: 坏签名仍是 SIGN_INVALID */
    att.signature[0] ^= 0x01U;
    TEST_ASSERT_EQUAL(VERIFY_SIGN_INVALID, sec_verify_attestation(&att));
    att.signature[0] ^= 0x01U;
}

/* ── 7. 产线配置锁定 (必须是最后一个用例) ────────────────── */

static void test_provisioning_lock(void)
{
    uint8_t chain[sizeof(k_leaf_der) + sizeof(k_int_der)];
    uint16_t chain_len;

    /* 锁定前注册对端公钥与信任锚, 锁定后必须仍然可用。 */
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_set_peer_public_key(SEC_KEY_TYPE_P256, k_leaf_pub, 64));
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_set_peer_trust_root(SEC_KEY_TYPE_P256, k_root_pub, 64));
    TEST_ASSERT_EQUAL(VERIFY_OK,
                      sec_verify(k_auth_msg, K_AUTH_MSG_LEN, k_leaf_auth_sig, 64));

    TEST_ASSERT_FALSE(sec_provisioning_locked());
    sec_lock_provisioning();
    TEST_ASSERT_TRUE(sec_provisioning_locked());

    /* 所有 provisioning setter 一律 CCC_ERR_STATE */
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_peer_public_key(SEC_KEY_TYPE_P256, k_device_pub, 64));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_peer_trust_root(SEC_KEY_TYPE_P256, k_root_pub, 64));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_device_identity(k_device_id, k_key_id));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_firmware_measurement(k_fw_hash));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_attestation_cert(k_leaf_der,
                                               (uint16_t)sizeof(k_leaf_der)));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_attestation_root(k_root_pub, 64));
    TEST_ASSERT_EQUAL(CCC_ERR_STATE,
                      sec_set_device_public_key(SEC_KEY_TYPE_P256, k_device_pub, 64));

    /* clear 变为 no-op: 已注册的对端公钥仍可用于 sec_verify */
    sec_clear_peer_public_key();
    sec_clear_peer_trust_root();
    TEST_ASSERT_EQUAL(1, sec_peer_public_key_provisioned());
    TEST_ASSERT_EQUAL(VERIFY_OK,
                      sec_verify(k_auth_msg, K_AUTH_MSG_LEN, k_leaf_auth_sig, 64));

    /* sec_set_trusted_time() 不受锁定影响: 设为远期后链校验应判定过期 */
    (void)memcpy(chain, k_leaf_der, sizeof(k_leaf_der));
    (void)memcpy(chain + sizeof(k_leaf_der), k_int_der, sizeof(k_int_der));
    chain_len = (uint16_t)(sizeof(k_leaf_der) + sizeof(k_int_der));

    sec_set_trusted_time(T_FUTURE);
    TEST_ASSERT_EQUAL(VERIFY_KEY_EXPIRED,
                      sec_verify_cert_chain(chain, chain_len, k_root_pub, 64));
    sec_set_trusted_time(0);
}

int run_security_revocation_tests(void)
{
    crypto_random_init();
    UNITY_BEGIN();

    RUN_TEST(test_install_and_query);
    RUN_TEST(test_install_rejects_tampered);
    RUN_TEST(test_epoch_replay_rejected);
    RUN_TEST(test_format_and_arg_validation);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_attestation_revoked_device);
    RUN_TEST(test_provisioning_lock);   /* 终态锁定, 必须最后执行 */

    UNITY_END();
}

#ifndef TEST_SECURITY_REVOCATION_NO_MAIN
int main(void)
{
    return run_security_revocation_tests();
}
#endif /* TEST_SECURITY_REVOCATION_NO_MAIN */
