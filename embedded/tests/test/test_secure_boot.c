/**
 * @file test_secure_boot.c
 * @brief Secure-boot manifest, signature, anti-rollback and policy tests.
 */

#include "unity.h"
#include "secure_boot.h"
#include "crypto_random.h"
#include "../support/cert_fixtures.h"

#include <stddef.h>
#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

static const uint8_t k_image[] = "YULE-DEMO-FIRMWARE-v1";
static const uint8_t k_key_id[SB_KEY_ID_SIZE] = {
    0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x01,
};

/* SM2 development key d = 1, public key = base point G. */
static const uint8_t k_sm2_priv[32] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
};
static const uint8_t k_sm2_pub[64] = {
    0x32,0xC4,0xAE,0x2C,0x1F,0x19,0x81,0x19,
    0x5F,0x99,0x04,0x46,0x6A,0x39,0xC9,0x94,
    0x8F,0xE3,0x0B,0xBF,0xF2,0x66,0x0B,0xE1,
    0x71,0x5A,0x45,0x89,0x33,0x4C,0x74,0xC7,
    0xBC,0x37,0x36,0xA2,0xF4,0xF6,0x77,0x9C,
    0x59,0xBD,0xCE,0xE3,0x6B,0x69,0x21,0x53,
    0xD0,0xA9,0x87,0x7C,0xC6,0x2A,0x47,0x40,
    0x02,0xDF,0x32,0xE5,0x21,0x39,0xF0,0xA0,
};

static int make_manifest(uint32_t version, uint32_t counter,
                         uint8_t *out, size_t *out_len)
{
    return sb_manifest_sign(k_device_priv, SB_KEY_TYPE_P256, version, counter,
                            k_image, sizeof(k_image), k_key_id,
                            out, SB_MANIFEST_SIZE, out_len);
}

/* ── Manifest serialisation ───────────────────────────────── */
static void test_manifest_roundtrip(void)
{
    uint8_t  buf[SB_MANIFEST_SIZE];
    size_t   len = 0;
    sb_manifest_t m;
    int i;

    TEST_ASSERT_EQUAL(SB_OK, make_manifest(3U, 2U, buf, &len));
    TEST_ASSERT_EQUAL_UINT(SB_MANIFEST_SIZE, len);

    TEST_ASSERT_EQUAL(SB_OK, sb_manifest_parse(buf, len, &m));
    TEST_ASSERT_EQUAL_MEMORY("YSBM", m.magic, 4);
    TEST_ASSERT_EQUAL_UINT8(SB_FORMAT_VERSION, m.format_version);
    TEST_ASSERT_EQUAL_UINT8(SB_KEY_TYPE_P256, m.key_type);
    TEST_ASSERT_EQUAL_UINT(3U, m.image_version);
    TEST_ASSERT_EQUAL_UINT(2U, m.anti_rollback);
    TEST_ASSERT_EQUAL_UINT((uint32_t)sizeof(k_image), m.image_len);
    TEST_ASSERT_EQUAL_MEMORY(k_key_id, m.key_id, SB_KEY_ID_SIZE);

    for (i = 0; i < (int)SB_SIGNATURE_SIZE; i++) {
        if (m.signature[i] != 0U) break;
    }
    TEST_ASSERT_TRUE(i < (int)SB_SIGNATURE_SIZE);
}

static void test_manifest_rejects_bad_structure(void)
{
    uint8_t buf[SB_MANIFEST_SIZE];
    size_t  len = 0;
    uint8_t bad[SB_MANIFEST_SIZE];
    sb_manifest_t m;

    TEST_ASSERT_EQUAL(SB_OK, make_manifest(1U, 1U, buf, &len));

    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM, sb_manifest_parse(NULL, len, &m));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM, sb_manifest_parse(buf, len, NULL));

    /* Truncated / oversized records are rejected. */
    TEST_ASSERT_EQUAL(SB_ERR_BAD_FORMAT, sb_manifest_parse(buf, 0U, &m));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_FORMAT, sb_manifest_parse(buf, len - 1U, &m));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_FORMAT, sb_manifest_parse(buf, len + 1U, &m));

    (void)memcpy(bad, buf, sizeof(bad));
    bad[0] ^= 0xFFU;
    TEST_ASSERT_EQUAL(SB_ERR_BAD_MAGIC, sb_manifest_parse(bad, sizeof(bad), &m));

    (void)memcpy(bad, buf, sizeof(bad));
    bad[4] = 2U; /* unsupported format_version */
    TEST_ASSERT_EQUAL(SB_ERR_BAD_FORMAT, sb_manifest_parse(bad, sizeof(bad), &m));

    (void)memcpy(bad, buf, sizeof(bad));
    bad[5] = 3U; /* unsupported key_type */
    TEST_ASSERT_EQUAL(SB_ERR_BAD_FORMAT, sb_manifest_parse(bad, sizeof(bad), &m));
}

/* ── Image verification ───────────────────────────────────── */
static void test_verify_ok(void)
{
    uint8_t  buf[SB_MANIFEST_SIZE];
    size_t   len = 0;
    sb_verify_result_t res;

    (void)memset(&res, 0, sizeof(res));
    TEST_ASSERT_EQUAL(SB_OK, make_manifest(3U, 2U, buf, &len));

    TEST_ASSERT_EQUAL(SB_OK,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 2U, &res));
    TEST_ASSERT_EQUAL_UINT8(SB_KEY_TYPE_P256, res.key_type);
    TEST_ASSERT_EQUAL_UINT(3U, res.image_version);
    TEST_ASSERT_EQUAL_UINT(2U, res.anti_rollback);
    TEST_ASSERT_EQUAL_UINT((uint32_t)sizeof(k_image), res.image_len);
    TEST_ASSERT_EQUAL_MEMORY(k_key_id, res.key_id, SB_KEY_ID_SIZE);

    /* A NULL result is allowed (verification only). */
    TEST_ASSERT_EQUAL(SB_OK,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 0U, NULL));
}

static void test_verify_rejects_tampered_image(void)
{
    uint8_t buf[SB_MANIFEST_SIZE];
    uint8_t image[sizeof(k_image)];
    size_t  len = 0;

    TEST_ASSERT_EQUAL(SB_OK, make_manifest(1U, 1U, buf, &len));
    (void)memcpy(image, k_image, sizeof(image));
    image[0] ^= 0x01U;

    TEST_ASSERT_EQUAL(SB_ERR_BAD_HASH,
                      sb_verify_image(image, sizeof(image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 0U, NULL));
}

static void test_verify_rejects_bad_signature(void)
{
    uint8_t buf[SB_MANIFEST_SIZE];
    size_t  len = 0;

    TEST_ASSERT_EQUAL(SB_OK, make_manifest(1U, 1U, buf, &len));

    buf[SB_MANIFEST_SIZE - 1U] ^= 0x01U;
    TEST_ASSERT_EQUAL(SB_ERR_BAD_SIGNATURE,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 0U, NULL));

    /* Correct manifest, wrong public key. */
    TEST_ASSERT_EQUAL(SB_OK, make_manifest(1U, 1U, buf, &len));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_SIGNATURE,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_wrong_root_pub, SB_KEY_TYPE_P256, 0U, NULL));
}

static void test_verify_enforces_rollback(void)
{
    uint8_t buf[SB_MANIFEST_SIZE];
    size_t  len = 0;

    TEST_ASSERT_EQUAL(SB_OK, make_manifest(4U, 5U, buf, &len));

    TEST_ASSERT_EQUAL(SB_ERR_ROLLBACK,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 6U, NULL));
    TEST_ASSERT_EQUAL(SB_OK,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 5U, NULL));
    TEST_ASSERT_EQUAL(SB_OK,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 4U, NULL));
}

static void test_verify_rejects_key_mismatch(void)
{
    uint8_t buf[SB_MANIFEST_SIZE];
    size_t  len = 0;

    TEST_ASSERT_EQUAL(SB_OK, make_manifest(1U, 1U, buf, &len));

    TEST_ASSERT_EQUAL(SB_ERR_KEY_MISMATCH,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_SM2, 0U, NULL));
}

static void test_verify_sm2_roundtrip(void)
{
    uint8_t buf[SB_MANIFEST_SIZE];
    uint8_t image[sizeof(k_image)];
    size_t  len = 0;

    TEST_ASSERT_EQUAL(SB_OK,
                      sb_manifest_sign(k_sm2_priv, SB_KEY_TYPE_SM2, 2U, 3U,
                                       k_image, sizeof(k_image), k_key_id,
                                       buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SB_OK,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_sm2_pub, SB_KEY_TYPE_SM2, 3U, NULL));

    (void)memcpy(image, k_image, sizeof(image));
    image[0] ^= 0x01U;
    TEST_ASSERT_EQUAL(SB_ERR_BAD_HASH,
                      sb_verify_image(image, sizeof(image), buf, len,
                                      k_sm2_pub, SB_KEY_TYPE_SM2, 0U, NULL));
}

static void test_verify_rejects_bad_params(void)
{
    uint8_t buf[SB_MANIFEST_SIZE];
    size_t  len = 0;

    TEST_ASSERT_EQUAL(SB_OK, make_manifest(1U, 1U, buf, &len));

    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_verify_image(NULL, sizeof(k_image), buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 0U, NULL));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_verify_image(k_image, sizeof(k_image), NULL, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 0U, NULL));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      NULL, SB_KEY_TYPE_P256, 0U, NULL));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_verify_image(k_image, sizeof(k_image), buf, len,
                                      k_device_pub, 0U, 0U, NULL));

    /* Signed image_len does not match the supplied buffer. */
    TEST_ASSERT_EQUAL(SB_ERR_BAD_FORMAT,
                      sb_verify_image(k_image, sizeof(k_image) - 1U, buf, len,
                                      k_device_pub, SB_KEY_TYPE_P256, 0U, NULL));

    /* Malformed manifest length is rejected before any crypto. */
    TEST_ASSERT_EQUAL(SB_ERR_BAD_FORMAT,
                      sb_verify_image(k_image, sizeof(k_image), buf, len - 1U,
                                      k_device_pub, SB_KEY_TYPE_P256, 0U, NULL));
}

static void test_sign_rejects_bad_params(void)
{
    uint8_t out[SB_MANIFEST_SIZE];
    size_t  out_len = 0;

    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_manifest_sign(NULL, SB_KEY_TYPE_P256, 1U, 1U,
                                       k_image, sizeof(k_image), k_key_id,
                                       out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_manifest_sign(k_device_priv, SB_KEY_TYPE_P256, 1U, 1U,
                                       NULL, sizeof(k_image), k_key_id,
                                       out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_manifest_sign(k_device_priv, SB_KEY_TYPE_P256, 1U, 1U,
                                       k_image, sizeof(k_image), NULL,
                                       out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_manifest_sign(k_device_priv, SB_KEY_TYPE_P256, 1U, 1U,
                                       k_image, sizeof(k_image), k_key_id,
                                       NULL, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_manifest_sign(k_device_priv, SB_KEY_TYPE_P256, 1U, 1U,
                                       k_image, sizeof(k_image), k_key_id,
                                       out, sizeof(out), NULL));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_manifest_sign(k_device_priv, 0U, 1U, 1U,
                                       k_image, sizeof(k_image), k_key_id,
                                       out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM,
                      sb_manifest_sign(k_device_priv, SB_KEY_TYPE_P256, 1U, 1U,
                                       k_image, sizeof(k_image), k_key_id,
                                       out, sizeof(out) - 1U, &out_len));
}

/* ── Anti-rollback counter store ──────────────────────────── */
static uint32_t g_counter;
static int      g_write_count;
static int      g_read_fail;
static int      g_write_fail;

static int mock_read(uint32_t *out)
{
    if (g_read_fail != 0) return -1;
    *out = g_counter;
    return 0;
}

static int mock_write(uint32_t v)
{
    if (g_write_fail != 0) return -1;
    g_counter = v;
    g_write_count++;
    return 0;
}

static void reset_store(uint32_t value)
{
    g_counter = value;
    g_write_count = 0;
    g_read_fail = 0;
    g_write_fail = 0;
}

static void test_counter_commit(void)
{
    static const sb_counter_store_t store = { mock_read, mock_write };

    reset_store(10U);
    TEST_ASSERT_EQUAL(SB_OK, sb_counter_commit(&store, 11U));
    TEST_ASSERT_EQUAL_UINT(11U, g_counter);
    TEST_ASSERT_EQUAL_INT(1, g_write_count);

    /* Equal value is accepted and written. */
    TEST_ASSERT_EQUAL(SB_OK, sb_counter_commit(&store, 11U));
    TEST_ASSERT_EQUAL_INT(2, g_write_count);

    /* Backwards move is refused and nothing is written. */
    TEST_ASSERT_EQUAL(SB_ERR_ROLLBACK, sb_counter_commit(&store, 9U));
    TEST_ASSERT_EQUAL_UINT(11U, g_counter);
    TEST_ASSERT_EQUAL_INT(2, g_write_count);

    /* Read failure: no write. */
    g_read_fail = 1;
    TEST_ASSERT_EQUAL(SB_ERR_IO, sb_counter_commit(&store, 12U));
    TEST_ASSERT_EQUAL_INT(2, g_write_count);

    /* Write failure: reported as IO error. */
    g_read_fail = 0;
    g_write_fail = 1;
    TEST_ASSERT_EQUAL(SB_ERR_IO, sb_counter_commit(&store, 12U));
    TEST_ASSERT_EQUAL_INT(2, g_write_count);
}

static void test_counter_rejects_bad_store(void)
{
    static const sb_counter_store_t no_read  = { NULL, mock_write };
    static const sb_counter_store_t no_write = { mock_read, NULL };

    reset_store(1U);
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM, sb_counter_commit(NULL, 2U));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM, sb_counter_commit(&no_read, 2U));
    TEST_ASSERT_EQUAL(SB_ERR_BAD_PARAM, sb_counter_commit(&no_write, 2U));
    TEST_ASSERT_EQUAL_INT(0, g_write_count);
}

/* ── Debug-port policy ────────────────────────────────────── */
static void test_debug_port_policy(void)
{
    TEST_ASSERT_EQUAL_INT(1, sb_debug_port_allowed(SB_LIFECYCLE_DEV));
    TEST_ASSERT_EQUAL_INT(0, sb_debug_port_allowed(SB_LIFECYCLE_PROD));
    TEST_ASSERT_EQUAL_INT(0, sb_debug_port_allowed(SB_LIFECYCLE_RMA));
    TEST_ASSERT_EQUAL_INT(0, sb_debug_port_allowed(0U));
    TEST_ASSERT_EQUAL_INT(0, sb_debug_port_allowed(0xFFU));
}

int run_secure_boot_tests(void)
{
    (void)crypto_random_init();
    UNITY_BEGIN();

    RUN_TEST(test_manifest_roundtrip);
    RUN_TEST(test_manifest_rejects_bad_structure);
    RUN_TEST(test_verify_ok);
    RUN_TEST(test_verify_rejects_tampered_image);
    RUN_TEST(test_verify_rejects_bad_signature);
    RUN_TEST(test_verify_enforces_rollback);
    RUN_TEST(test_verify_rejects_key_mismatch);
    RUN_TEST(test_verify_sm2_roundtrip);
    RUN_TEST(test_verify_rejects_bad_params);
    RUN_TEST(test_sign_rejects_bad_params);
    RUN_TEST(test_counter_commit);
    RUN_TEST(test_counter_rejects_bad_store);
    RUN_TEST(test_debug_port_policy);

    UNITY_END();
}

#ifndef TEST_SECURE_BOOT_NO_MAIN
int main(void)
{
    return run_secure_boot_tests();
}
#endif /* TEST_SECURE_BOOT_NO_MAIN */
