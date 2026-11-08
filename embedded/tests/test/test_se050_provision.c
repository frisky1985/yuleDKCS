/**
 * @file test_se050_provision.c
 * @brief 产线 HSM 密钥派生工具单元测试
 *
 * 测试向量由 OpenSSL 独立生成（不依赖被测实现）：
 *   - K-ENC/K-MAC/K-RMAC: AES-256-CMAC(MK, 0x01‖key_id‖UID‖ver)
 *   - fingerprint: SHA-256("YULE-DK-TRANSPORT-V1"‖UID‖K-ENC‖K-MAC‖K-RMAC)
 */

#include "unity.h"
#include "se050_hsm_provision.h"

#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* AES-256 MK */
static const uint8_t V_MK[HSM_PROV_MK_MAX_LEN] = {
    0x60,0x3D,0xEB,0x10,0x15,0xCA,0x71,0xBE,
    0x2B,0x73,0xAE,0xF0,0x85,0x7D,0x77,0x81,
    0x1F,0x35,0x2C,0x07,0x3B,0x61,0x08,0xD7,
    0x2D,0x98,0x10,0xA3,0x09,0x14,0xDF,0xF4
};

static const uint8_t V_UID[HSM_PROV_UID_LEN] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11
};

/* version = 0x01 */
static const uint8_t V_ENC[HSM_PROV_KEY_LEN] = {
    0x1C,0x0F,0x88,0xB6,0x1F,0x3A,0xF4,0x9E,
    0x67,0x2E,0xB5,0xAB,0xEE,0xB2,0x2D,0xE0
};
static const uint8_t V_MAC[HSM_PROV_KEY_LEN] = {
    0xA0,0xF7,0x8E,0xD2,0x4B,0x40,0x7A,0x68,
    0xD8,0x97,0x16,0xCD,0x40,0x1A,0x96,0xF4
};
static const uint8_t V_RMAC[HSM_PROV_KEY_LEN] = {
    0xA9,0x19,0xEB,0x4C,0xE2,0x1F,0x6E,0xEC,
    0x61,0x92,0xCE,0x82,0xAE,0x3F,0xFB,0x33
};
static const uint8_t V_FP[HSM_PROV_FINGERPRINT_LEN] = {
    0x5E,0x87,0x42,0x21,0x40,0x3E,0xE9,0x97,
    0x32,0x54,0x9A,0xA3,0xE7,0x1C,0x8F,0xBF,
    0x87,0xF0,0x86,0x69,0x4D,0x38,0x8F,0x63,
    0xFC,0xC1,0x2B,0x5E,0x3D,0x79,0x5F,0x3A
};

static hsm_prov_config_t make_config(void)
{
    hsm_prov_config_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.backend = HSM_PROV_BACKEND_SOFTWARE;
    cfg.mk = V_MK;
    cfg.mk_len = sizeof(V_MK);
    cfg.allow_software = 1U;
    return cfg;
}

static void init_ctx(hsm_prov_ctx_t *ctx)
{
    hsm_prov_config_t cfg = make_config();

    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_init(ctx, &cfg));
}

static void assert_all_zero(const void *ptr, size_t len)
{
    const uint8_t *p = (const uint8_t *)ptr;
    size_t i;

    for (i = 0; i < len; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00U, p[i]);
    }
}

/* ========================================================================
 * 初始化
 * ======================================================================== */

static void test_init_rejects_null(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_config_t cfg = make_config();

    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_init(&ctx, NULL));
}

static void test_init_software_requires_optin(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_config_t cfg = make_config();

    cfg.allow_software = 0U;
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_DENIED, hsm_provision_init(&ctx, &cfg));
    TEST_ASSERT_EQUAL_UINT8(0U, ctx.ready);
}

static void test_init_rejects_bad_mk(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_config_t cfg = make_config();

    cfg.mk = NULL;
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_init(&ctx, &cfg));

    cfg = make_config();
    cfg.mk_len = 17U;
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_init(&ctx, &cfg));
}

static void test_init_pkcs11_unavailable(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_config_t cfg = make_config();

    cfg.backend = HSM_PROV_BACKEND_PKCS11;
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_UNAVAILABLE, hsm_provision_init(&ctx, &cfg));
    TEST_ASSERT_EQUAL_UINT8(0U, ctx.ready);
}

static void test_init_rejects_unknown_backend(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_config_t cfg = make_config();

    cfg.backend = (hsm_prov_backend_e)99;
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_init(&ctx, &cfg));
}

/* ========================================================================
 * 派生
 * ======================================================================== */

static void test_derive_known_answer(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;

    init_ctx(&ctx);
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK,
                          hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &set));

    TEST_ASSERT_EQUAL_MEMORY(V_ENC, set.k_enc, HSM_PROV_KEY_LEN);
    TEST_ASSERT_EQUAL_MEMORY(V_MAC, set.k_mac, HSM_PROV_KEY_LEN);
    TEST_ASSERT_EQUAL_MEMORY(V_RMAC, set.k_rmac, HSM_PROV_KEY_LEN);
    TEST_ASSERT_EQUAL_UINT8(0x01U, set.key_version);

    hsm_provision_key_set_clear(&set);
    hsm_provision_deinit(&ctx);
}

static void test_derive_is_deterministic(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t a;
    hsm_prov_key_set_t b;

    init_ctx(&ctx);
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &a));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &b));
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));

    hsm_provision_key_set_clear(&a);
    hsm_provision_key_set_clear(&b);
    hsm_provision_deinit(&ctx);
}

static void test_derive_domain_separation(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t v1;
    hsm_prov_key_set_t v2;
    uint8_t other_uid[HSM_PROV_UID_LEN];

    init_ctx(&ctx);

    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &v1));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_derive_transport(&ctx, V_UID, 0x02U, &v2));
    TEST_ASSERT_NOT_EQUAL(0, memcmp(&v1, &v2, sizeof(v1)));

    (void)memcpy(other_uid, V_UID, sizeof(other_uid));
    other_uid[HSM_PROV_UID_LEN - 1U] ^= 0x01U;
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK,
                          hsm_provision_derive_transport(&ctx, other_uid, 0x01U, &v2));
    TEST_ASSERT_NOT_EQUAL(0, memcmp(&v1, &v2, sizeof(v1)));

    hsm_provision_key_set_clear(&v1);
    hsm_provision_key_set_clear(&v2);
    hsm_provision_deinit(&ctx);
}

static void test_derive_rejects_zero_uid(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;
    uint8_t zero_uid[HSM_PROV_UID_LEN];

    init_ctx(&ctx);
    (void)memset(zero_uid, 0, sizeof(zero_uid));
    (void)memset(&set, 0xAA, sizeof(set));

    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM,
                          hsm_provision_derive_transport(&ctx, zero_uid, 0x01U, &set));
    assert_all_zero(&set, sizeof(set));
    hsm_provision_deinit(&ctx);
}

static void test_derive_rejects_version_zero(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;

    init_ctx(&ctx);
    (void)memset(&set, 0xAA, sizeof(set));

    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM,
                          hsm_provision_derive_transport(&ctx, V_UID, 0x00U, &set));
    assert_all_zero(&set, sizeof(set));
    hsm_provision_deinit(&ctx);
}

static void test_derive_requires_init(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;

    (void)memset(&ctx, 0, sizeof(ctx));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_STATE,
                          hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &set));

    init_ctx(&ctx);
    hsm_provision_deinit(&ctx);
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_STATE,
                          hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &set));
}

static void test_derive_rejects_null(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;

    init_ctx(&ctx);
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM,
                          hsm_provision_derive_transport(NULL, V_UID, 0x01U, &set));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM,
                          hsm_provision_derive_transport(&ctx, NULL, 0x01U, &set));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM,
                          hsm_provision_derive_transport(&ctx, V_UID, 0x01U, NULL));
    hsm_provision_deinit(&ctx);
}

/* ========================================================================
 * 指纹与清零
 * ======================================================================== */

static void test_fingerprint_known_answer(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;
    uint8_t fp[HSM_PROV_FINGERPRINT_LEN];

    init_ctx(&ctx);
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &set));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_fingerprint(V_UID, &set, fp));
    TEST_ASSERT_EQUAL_MEMORY(V_FP, fp, HSM_PROV_FINGERPRINT_LEN);

    hsm_provision_key_set_clear(&set);
    hsm_provision_deinit(&ctx);
}

static void test_fingerprint_rejects_null(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;
    uint8_t fp[HSM_PROV_FINGERPRINT_LEN];

    init_ctx(&ctx);
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &set));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_fingerprint(NULL, &set, fp));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_fingerprint(V_UID, NULL, fp));
    TEST_ASSERT_EQUAL_INT(HSM_PROV_ERR_PARAM, hsm_provision_fingerprint(V_UID, &set, NULL));
    hsm_provision_key_set_clear(&set);
    hsm_provision_deinit(&ctx);
}

static void test_key_set_clear(void)
{
    hsm_prov_ctx_t ctx;
    hsm_prov_key_set_t set;

    init_ctx(&ctx);
    TEST_ASSERT_EQUAL_INT(HSM_PROV_OK, hsm_provision_derive_transport(&ctx, V_UID, 0x01U, &set));
    hsm_provision_key_set_clear(&set);
    assert_all_zero(&set, sizeof(set));

    hsm_provision_key_set_clear(NULL); /* 必须安全 */
    hsm_provision_deinit(&ctx);
}

static void test_deinit_zeroizes_context(void)
{
    hsm_prov_ctx_t ctx;

    init_ctx(&ctx);
    TEST_ASSERT_EQUAL_UINT8(1U, ctx.ready);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)sizeof(V_MK), ctx.mk_len);

    hsm_provision_deinit(&ctx);
    assert_all_zero(&ctx, sizeof(ctx));

    hsm_provision_deinit(NULL); /* 必须安全 */
}

/* ========================================================================
 * 运行入口
 * ======================================================================== */

int run_se050_provision_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_null);
    RUN_TEST(test_init_software_requires_optin);
    RUN_TEST(test_init_rejects_bad_mk);
    RUN_TEST(test_init_pkcs11_unavailable);
    RUN_TEST(test_init_rejects_unknown_backend);
    RUN_TEST(test_derive_known_answer);
    RUN_TEST(test_derive_is_deterministic);
    RUN_TEST(test_derive_domain_separation);
    RUN_TEST(test_derive_rejects_zero_uid);
    RUN_TEST(test_derive_rejects_version_zero);
    RUN_TEST(test_derive_requires_init);
    RUN_TEST(test_derive_rejects_null);
    RUN_TEST(test_fingerprint_known_answer);
    RUN_TEST(test_fingerprint_rejects_null);
    RUN_TEST(test_key_set_clear);
    RUN_TEST(test_deinit_zeroizes_context);
    UNITY_END();
}

#ifndef TEST_SE050_PROVISION_NO_MAIN
int main(void)
{
    return run_se050_provision_tests();
}
#endif /* TEST_SE050_PROVISION_NO_MAIN */
