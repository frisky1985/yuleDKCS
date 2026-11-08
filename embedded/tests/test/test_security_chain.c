/**
 * @file test_security_chain.c
 * @brief 认证链路测试：P-256 验签、X.509 证书链、远程证明
 *
 * 证书夹具由 OpenSSL 生成（见 support/cert_fixtures.h）：
 *   root(CA) ← intermediate(CA) ← leaf；device 证书由 root 直签。
 */

#include "unity.h"
#include "ccc_digital_key.h"
#include "crypto_engine.h"
#include "crypto_random.h"
#include "p256.h"
#include "sec_x509.h"
#include "cert_fixtures.h"

#include <stddef.h>
#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* SM2 基点 G —— 开发密钥 d=1 对应的公钥。 */
static const uint8_t k_dev_pub_sm2[64] = {
    0x32,0xC4,0xAE,0x2C,0x1F,0x19,0x81,0x19,
    0x5F,0x99,0x04,0x46,0x6A,0x39,0xC9,0x94,
    0x8F,0xE3,0x0B,0xBF,0xF2,0x66,0x0B,0xE1,
    0x71,0x5A,0x45,0x89,0x33,0x4C,0x74,0xC7,
    0xBC,0x37,0x36,0xA2,0xF4,0xF6,0x77,0x9C,
    0x59,0xBD,0xCE,0xE3,0x6B,0x69,0x21,0x53,
    0xD0,0xA9,0x87,0x7C,0xC6,0x2A,0x47,0x40,
    0x02,0xDF,0x32,0xE5,0x21,0x39,0xF0,0xA0
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

/* 2027-01-15, within the fixture validity window. */
#define T_VALID   1800000000U
/* 2050-01-01, after notAfter. */
#define T_FUTURE  2524608000U
/* 2020-01-01, before notBefore. */
#define T_PAST    1577836800U

static void reset_security(void)
{
    sec_clear_peer_public_key();
    sec_set_trusted_time(0);
    (void)sec_init();
}

static uint16_t build_leaf_int_chain(uint8_t *out, size_t cap)
{
    TEST_ASSERT_TRUE(cap >= sizeof(k_leaf_der) + sizeof(k_int_der));
    (void)memcpy(out, k_leaf_der, sizeof(k_leaf_der));
    (void)memcpy(out + sizeof(k_leaf_der), k_int_der, sizeof(k_int_der));
    return (uint16_t)(sizeof(k_leaf_der) + sizeof(k_int_der));
}

/* ── 1. ECDSA P-256 KAT ───────────────────────────────────── */

static void test_p256_sign_verify(void)
{
    uint8_t hash[32];
    uint8_t sig[64];

    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_sha256(k_auth_msg, K_AUTH_MSG_LEN, hash));
    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_p256_sign(k_device_priv, hash, sig));
    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_p256_verify(k_device_pub, hash, sig));

    hash[0] ^= 0x01U;
    TEST_ASSERT_NOT_EQUAL(CRYPTO_SUCCESS,
                          crypto_p256_verify(k_device_pub, hash, sig));
}

static void test_p256_rejects_invalid_inputs(void)
{
    uint8_t hash[32] = {0};
    uint8_t sig[64] = {0};
    uint8_t zero_pub[64] = {0};
    uint8_t bad_sig[64];

    TEST_ASSERT_NOT_EQUAL(CRYPTO_SUCCESS,
                          crypto_p256_verify(zero_pub, hash, sig));

    (void)memcpy(bad_sig, k_leaf_auth_sig, sizeof(bad_sig));
    bad_sig[63] ^= 0xFFU;
    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_sha256(k_auth_msg, K_AUTH_MSG_LEN, hash));
    TEST_ASSERT_NOT_EQUAL(CRYPTO_SUCCESS,
                          crypto_p256_verify(k_leaf_pub, hash, bad_sig));
    TEST_ASSERT_EQUAL(CRYPTO_ERR_NULL_PTR,
                      crypto_p256_verify(NULL, hash, bad_sig));
}

/* ── 2. X.509 解析 ────────────────────────────────────────── */

static void test_x509_parse_leaf(void)
{
    sec_x509_cert_t cert;

    TEST_ASSERT_EQUAL(0, sec_x509_parse(k_leaf_der, sizeof(k_leaf_der), &cert));
    TEST_ASSERT_EQUAL_MEMORY(k_leaf_pub, cert.pub, sizeof(k_leaf_pub));
    TEST_ASSERT_TRUE(cert.has_validity);
    TEST_ASSERT_TRUE(cert.not_after > cert.not_before);
    TEST_ASSERT_TRUE(cert.issuer_len > 0);
    TEST_ASSERT_TRUE(cert.subject_len > 0);
    TEST_ASSERT_EQUAL(sizeof(k_leaf_der),
                      sec_x509_total_len(k_leaf_der, sizeof(k_leaf_der)));
}

/* ── 3. 证书链 ────────────────────────────────────────────── */

static void test_chain_ok_and_registers_peer(void)
{
    uint8_t chain[sizeof(k_leaf_der) + sizeof(k_int_der)];
    uint16_t chain_len = build_leaf_int_chain(chain, sizeof(chain));

    reset_security();
    sec_set_trusted_time(T_VALID);

    /* 未经验证链前，没有对端公钥，验签必须失败 */
    TEST_ASSERT_EQUAL(VERIFY_SIGN_INVALID,
                      sec_verify(k_auth_msg, K_AUTH_MSG_LEN, k_leaf_auth_sig, 64));

    TEST_ASSERT_EQUAL(VERIFY_OK,
                      sec_verify_cert_chain(chain, chain_len, k_root_pub, 64));

    /* 链验证成功后，叶子公钥自动成为对端公钥 */
    TEST_ASSERT_EQUAL(VERIFY_OK,
                      sec_verify(k_auth_msg, K_AUTH_MSG_LEN, k_leaf_auth_sig, 64));

    sec_clear_peer_public_key();
    TEST_ASSERT_EQUAL(VERIFY_SIGN_INVALID,
                      sec_verify(k_auth_msg, K_AUTH_MSG_LEN, k_leaf_auth_sig, 64));
}

static void test_chain_rejects_wrong_root(void)
{
    uint8_t chain[sizeof(k_leaf_der) + sizeof(k_int_der)];
    uint16_t chain_len = build_leaf_int_chain(chain, sizeof(chain));

    reset_security();
    sec_set_trusted_time(T_VALID);
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID,
                      sec_verify_cert_chain(chain, chain_len, k_wrong_root_pub, 64));
}

static void test_chain_rejects_tampered_cert(void)
{
    uint8_t chain[sizeof(k_leaf_der) + sizeof(k_int_der)];
    uint16_t chain_len = build_leaf_int_chain(chain, sizeof(chain));

    reset_security();
    sec_set_trusted_time(T_VALID);
    chain[sizeof(k_leaf_der) - 1U] ^= 0x01U; /* corrupt leaf signature */
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID,
                      sec_verify_cert_chain(chain, chain_len, k_root_pub, 64));
}

static void test_chain_enforces_validity(void)
{
    uint8_t chain[sizeof(k_leaf_der) + sizeof(k_int_der)];
    uint16_t chain_len = build_leaf_int_chain(chain, sizeof(chain));

    reset_security();

    sec_set_trusted_time(T_FUTURE);
    TEST_ASSERT_EQUAL(VERIFY_KEY_EXPIRED,
                      sec_verify_cert_chain(chain, chain_len, k_root_pub, 64));

    sec_set_trusted_time(T_PAST);
    TEST_ASSERT_EQUAL(VERIFY_KEY_EXPIRED,
                      sec_verify_cert_chain(chain, chain_len, k_root_pub, 64));

    sec_set_trusted_time(T_VALID);
    TEST_ASSERT_EQUAL(VERIFY_OK,
                      sec_verify_cert_chain(chain, chain_len, k_root_pub, 64));
}

static void test_chain_rejects_broken_issuer_link(void)
{
    uint8_t chain[sizeof(k_leaf_der) + sizeof(k_device_der)];
    uint16_t chain_len;

    reset_security();
    sec_set_trusted_time(T_VALID);

    (void)memcpy(chain, k_leaf_der, sizeof(k_leaf_der));
    (void)memcpy(chain + sizeof(k_leaf_der), k_device_der, sizeof(k_device_der));
    chain_len = (uint16_t)(sizeof(k_leaf_der) + sizeof(k_device_der));

    /* leaf.issuer(Intermediate) != device.subject(Device) */
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID,
                      sec_verify_cert_chain(chain, chain_len, k_root_pub, 64));
}

/* ── 4. 远程证明 ──────────────────────────────────────────── */

static void test_attestation_requires_provisioning(void)
{
    ccc_attestation_t att;

    reset_security();
    TEST_ASSERT_EQUAL(CCC_ERR_SECURITY, sec_attestation(&att));
    TEST_ASSERT_EQUAL(VERIFY_SIGN_INVALID, sec_verify_attestation(&att));
}

static void test_attestation_device_key_roundtrip(void)
{
    ccc_attestation_t att;
    uint8_t nonce_zero[16] = {0};

    reset_security();
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_device_identity(k_device_id, k_key_id));
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_firmware_measurement(k_fw_hash));
    TEST_ASSERT_EQUAL(CCC_OK,
                      sec_set_device_public_key(SEC_KEY_TYPE_SM2, k_dev_pub_sm2, 64));

    TEST_ASSERT_EQUAL(CCC_OK, sec_attestation(&att));
    TEST_ASSERT_EQUAL_MEMORY(k_device_id, att.device_id, 16);
    TEST_ASSERT_EQUAL_MEMORY(k_key_id, att.key_id, 16);
    TEST_ASSERT_EQUAL_MEMORY(k_fw_hash, att.firmware_hash, 32);
    TEST_ASSERT_EQUAL(SEC_KEY_TYPE_SM2, att.key_type);
    TEST_ASSERT_NOT_EQUAL(0, memcmp(att.nonce, nonce_zero, sizeof(nonce_zero)));

    TEST_ASSERT_EQUAL(VERIFY_OK, sec_verify_attestation(&att));

    att.firmware_hash[0] ^= 0x01U;
    TEST_ASSERT_EQUAL(VERIFY_FW_TAMPERED, sec_verify_attestation(&att));
    att.firmware_hash[0] ^= 0x01U;

    att.signature[0] ^= 0x01U;
    TEST_ASSERT_EQUAL(VERIFY_SIGN_INVALID, sec_verify_attestation(&att));
    att.signature[0] ^= 0x01U;

    att.security_state = 0x00;
    TEST_ASSERT_EQUAL(VERIFY_PERMISSION_DENIED, sec_verify_attestation(&att));
    att.security_state = 0x01;

    att.device_id[0] ^= 0x01U;
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID, sec_verify_attestation(&att));
}

static void test_attestation_with_cert_chain(void)
{
    ccc_attestation_t att;
    uint8_t hash[32];

    reset_security();
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_device_identity(k_device_id, k_key_id));
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_firmware_measurement(k_fw_hash));
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_attestation_root(k_root_pub, 64));

    (void)memset(&att, 0, sizeof(att));
    att.version = 0x02;
    att.key_type = SEC_KEY_TYPE_P256;
    (void)memcpy(att.device_id, k_device_id, 16);
    (void)memcpy(att.key_id, k_key_id, 16);
    (void)memcpy(att.firmware_hash, k_fw_hash, 32);
    att.security_state = 0x01;
    (void)memcpy(att.attestation_cert, k_device_der, sizeof(k_device_der));
    att.attestation_cert_len = (uint16_t)sizeof(k_device_der);

    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_sha256((const uint8_t *)&att,
                                    (size_t)offsetof(ccc_attestation_t, signature),
                                    hash));
    TEST_ASSERT_EQUAL(CRYPTO_SUCCESS,
                      crypto_p256_sign(k_device_priv, hash, att.signature));

    TEST_ASSERT_EQUAL(VERIFY_OK, sec_verify_attestation(&att));

    /* 证书被篡改 -> 链验证失败 */
    att.attestation_cert[sizeof(k_device_der) - 1U] ^= 0x01U;
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID, sec_verify_attestation(&att));
    att.attestation_cert[sizeof(k_device_der) - 1U] ^= 0x01U;

    /* 根公钥不匹配 -> 链验证失败 */
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_attestation_root(k_wrong_root_pub, 64));
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID, sec_verify_attestation(&att));
    TEST_ASSERT_EQUAL(CCC_OK, sec_set_attestation_root(k_root_pub, 64));

    TEST_ASSERT_EQUAL(VERIFY_OK, sec_verify_attestation(&att));
}

/* ── 5. 参数校验 ──────────────────────────────────────────── */

static void test_provisioning_arg_validation(void)
{
    uint8_t buf[600] = {0};

    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM,
                      sec_set_peer_public_key(SEC_KEY_TYPE_P256, NULL, 64));
    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM,
                      sec_set_peer_public_key(SEC_KEY_TYPE_P256, buf, 63));
    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM,
                      sec_set_peer_public_key(0, buf, 64));

    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM, sec_set_attestation_cert(NULL, 10));
    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM, sec_set_attestation_cert(buf, 0));
    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM, sec_set_attestation_cert(buf, 513));

    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM, sec_set_attestation_root(NULL, 64));
    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM, sec_set_attestation_root(buf, 63));
    TEST_ASSERT_EQUAL(CCC_ERR_INVALID_PARAM, sec_set_device_public_key(0, buf, 64));

    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID, sec_verify_attestation(NULL));
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID,
                      sec_verify_cert_chain(NULL, 10, k_root_pub, 64));
    TEST_ASSERT_EQUAL(VERIFY_CERT_INVALID,
                      sec_verify_cert_chain(k_leaf_der, sizeof(k_leaf_der), NULL, 64));
}

int run_security_chain_tests(void)
{
    crypto_random_init();
    UNITY_BEGIN();

    RUN_TEST(test_p256_sign_verify);
    RUN_TEST(test_p256_rejects_invalid_inputs);
    RUN_TEST(test_x509_parse_leaf);
    RUN_TEST(test_chain_ok_and_registers_peer);
    RUN_TEST(test_chain_rejects_wrong_root);
    RUN_TEST(test_chain_rejects_tampered_cert);
    RUN_TEST(test_chain_enforces_validity);
    RUN_TEST(test_chain_rejects_broken_issuer_link);
    RUN_TEST(test_attestation_requires_provisioning);
    RUN_TEST(test_attestation_device_key_roundtrip);
    RUN_TEST(test_attestation_with_cert_chain);
    RUN_TEST(test_provisioning_arg_validation);

    UNITY_END();
}

#ifndef TEST_SECURITY_CHAIN_NO_MAIN
int main(void)
{
    return run_security_chain_tests();
}
#endif /* TEST_SECURITY_CHAIN_NO_MAIN */
