/******************************************************************************
 * @file    test_scp03.c
 * @brief   SCP03 安全通道单元测试
 *
 * Tests session lifecycle, key derivation, APDU wrapping/unwrapping,
 * and security level checks for the SCP03 secure channel protocol.
 ******************************************************************************/

#include "unity.h"
#include "scp03.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*==============================================================================
 * Helpers
 *============================================================================*/

/** Static test keys (AES-128) matching GlobalPlatform test vectors */
static const uint8_t TEST_ENC_KEY[16] = {
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
};

static const uint8_t TEST_MAC_KEY[16] = {
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F
};

static const uint8_t TEST_DEK_KEY[16] = {
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F
};

static const uint8_t TEST_HOST_CHALLENGE[8] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
};

static void build_default_config(scp03_config_t *config, scp03_security_level_t level)
{
    memset(config, 0, sizeof(scp03_config_t));
    memcpy(config->static_keys.enc_key, TEST_ENC_KEY, 16);
    memcpy(config->static_keys.mac_key, TEST_MAC_KEY, 16);
    memcpy(config->static_keys.dek_key, TEST_DEK_KEY, 16);
    config->static_keys.key_len = 16;
    memcpy(config->host_challenge, TEST_HOST_CHALLENGE, 8);
    config->key_type = SCP03_KEY_STATIC;
    config->security_level = level;
}

/*==============================================================================
 * Test: Session Init/Clear
 *============================================================================*/

void test_scp03_session_init_clear(void)
{
    scp03_session_t session;
    uint8_t zeros_32[32] = {0};
    uint8_t zeros_16[16] = {0};
    uint8_t zeros_8[8] = {0};

    scp03_session_init(&session);

    /* Verify all session keys are zeroed */
    TEST_ASSERT_EQUAL_MEMORY(zeros_32, session.session_keys[SCP03_KEY_ENC], 32);
    TEST_ASSERT_EQUAL_MEMORY(zeros_32, session.session_keys[SCP03_KEY_MAC], 32);
    TEST_ASSERT_EQUAL_MEMORY(zeros_32, session.session_keys[SCP03_KEY_DEK], 32);

    /* Verify challenges are zeroed */
    TEST_ASSERT_EQUAL_MEMORY(zeros_8, session.host_challenge, 8);
    TEST_ASSERT_EQUAL_MEMORY(zeros_8, session.card_challenge, 8);

    /* Verify state */
    TEST_ASSERT_EQUAL(0, session.session_open);
    TEST_ASSERT_EQUAL(1, session.first_command);

    /* Fill with non-zero to test clear */
    memset(&session, 0xFF, sizeof(session));
    scp03_session_clear(&session);

    /* Verify all sensitive data cleared */
    TEST_ASSERT_EQUAL_MEMORY(zeros_32, session.session_keys[SCP03_KEY_ENC], 32);
    TEST_ASSERT_EQUAL_MEMORY(zeros_32, session.session_keys[SCP03_KEY_MAC], 32);
    TEST_ASSERT_EQUAL_MEMORY(zeros_32, session.session_keys[SCP03_KEY_DEK], 32);
    TEST_ASSERT_EQUAL_MEMORY(zeros_16, session.mac_chaining_value, 16);
    TEST_ASSERT_EQUAL_MEMORY(zeros_8, session.host_challenge, 8);
    TEST_ASSERT_EQUAL_MEMORY(zeros_8, session.card_challenge, 8);
    TEST_ASSERT_EQUAL(0, session.session_open);
    TEST_ASSERT_EQUAL(0, session.first_command);

    /* Test NULL-safety */
    scp03_session_init(NULL);
    scp03_session_clear(NULL);
}

/*==============================================================================
 * Test: Security Level Checks
 *============================================================================*/

void test_scp03_security_level_checks(void)
{
    /* i=00: No security */
    TEST_ASSERT_FALSE(scp03_is_encryption_required(SCP03_I_00));
    TEST_ASSERT_FALSE(scp03_is_mac_required(SCP03_I_00));

    /* i=04: C-MAC only */
    TEST_ASSERT_FALSE(scp03_is_encryption_required(SCP03_I_04));
    TEST_ASSERT_TRUE(scp03_is_mac_required(SCP03_I_04));

    /* i=0C: C-MAC + C-ENC */
    TEST_ASSERT_TRUE(scp03_is_encryption_required(SCP03_I_0C));
    TEST_ASSERT_TRUE(scp03_is_mac_required(SCP03_I_0C));

    /* i=14: Full security */
    TEST_ASSERT_TRUE(scp03_is_encryption_required(SCP03_I_14));
    TEST_ASSERT_TRUE(scp03_is_mac_required(SCP03_I_14));
}

/*==============================================================================
 * Test: Session Open/Close
 *============================================================================*/

void test_scp03_session_open_close(void)
{
    scp03_session_t session;
    scp03_config_t config;
    int ret;

    build_default_config(&config, SCP03_I_0C);

    /* Test NULL parameter */
    ret = scp03_open_session(NULL, &config);
    TEST_ASSERT_EQUAL(-1, ret);

    ret = scp03_open_session(&session, NULL);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test invalid key length */
    config.static_keys.key_len = 0;
    ret = scp03_open_session(&session, &config);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test invalid security level */
    build_default_config(&config, SCP03_I_0C);
    config.security_level = (scp03_security_level_t)0xFF;
    ret = scp03_open_session(&session, &config);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test successful open */
    build_default_config(&config, SCP03_I_0C);
    ret = scp03_open_session(&session, &config);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_EQUAL(1, session.session_open);

    /* Verify session keys were derived (non-zero) */
    uint8_t zeros[32] = {0};
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, session.session_keys[SCP03_KEY_ENC], 16);
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, session.session_keys[SCP03_KEY_MAC], 16);
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, session.session_keys[SCP03_KEY_DEK], 16);

    /* Verify challenges stored */
    TEST_ASSERT_EQUAL_MEMORY(TEST_HOST_CHALLENGE, session.host_challenge, 8);
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, session.card_challenge, 8);

    /* Test NULL close */
    ret = scp03_close_session(NULL);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test close */
    ret = scp03_close_session(&session);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_EQUAL(0, session.session_open);
    TEST_ASSERT_EQUAL_MEMORY(zeros, session.session_keys[SCP03_KEY_ENC], 32);

    /* Test double close */
    ret = scp03_close_session(&session);
    TEST_ASSERT_EQUAL(0, ret);
}

/*==============================================================================
 * Test: APDU Wrap/Unwrap (i=0C: C-MAC + C-ENC)
 *============================================================================*/

void test_scp03_apdu_wrap_unwrap(void)
{
    scp03_session_t session;
    scp03_config_t config;
    uint8_t apdu_out[SCP03_MAX_APDU_SIZE];
    size_t out_len;
    int ret;

    build_default_config(&config, SCP03_I_0C);
    ret = scp03_open_session(&session, &config);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);

    /* Build a test APDU: GET CHALLENGE */
    scp03_apdu_t apdu;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    apdu.cla = 0x00;
    apdu.ins = 0x84;
    apdu.p1 = 0x00;
    apdu.p2 = 0x00;
    apdu.data = data;
    apdu.data_len = sizeof(data);
    apdu.le = 0x08;

    /* Test wrap */
    ret = scp03_wrap_apdu(&session, &apdu, apdu_out, &out_len);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_TRUE(out_len > 0);

    /* Verify CLA has secure messaging bit set */
    TEST_ASSERT_EQUAL(0x0C, apdu_out[0] & 0x0F);

    /* Verify MAC is present (16 bytes at end before Le) */
    if (out_len >= SCP03_MAC_SIZE + 2) {
        uint8_t zeros_mac[SCP03_MAC_SIZE] = {0};
        TEST_ASSERT_NOT_EQUAL_MEMORY(zeros_mac,
            apdu_out + out_len - SCP03_MAC_SIZE - 1, SCP03_MAC_SIZE);
    }

    /* Test wrap with NULL session */
    ret = scp03_wrap_apdu(NULL, &apdu, apdu_out, &out_len);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test wrap after close */
    scp03_close_session(&session);
    ret = scp03_wrap_apdu(&session, &apdu, apdu_out, &out_len);
    TEST_ASSERT_EQUAL(-2, ret);
}

/*==============================================================================
 * Test: APDU Unwrap with response data
 *============================================================================*/

void test_scp03_apdu_unwrap(void)
{
    scp03_session_t session;
    scp03_config_t config;
    uint8_t resp_out[SCP03_MAX_APDU_SIZE];
    size_t out_len;
    int ret;

    build_default_config(&config, SCP03_I_0C);
    ret = scp03_open_session(&session, &config);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);

    /* Test unwrap with NULL params */
    uint8_t resp[] = {0x90, 0x00};
    ret = scp03_unwrap_apdu(NULL, resp, sizeof(resp), resp_out, &out_len);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test unwrap with minimal response (no security) */
    ret = scp03_unwrap_apdu(&session, resp, sizeof(resp), resp_out, &out_len);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(2, out_len);
    TEST_ASSERT_EQUAL(0x90, resp_out[0]);
    TEST_ASSERT_EQUAL(0x00, resp_out[1]);

    /* Test unwrap after close */
    scp03_close_session(&session);
    ret = scp03_unwrap_apdu(&session, resp, sizeof(resp), resp_out, &out_len);
    TEST_ASSERT_EQUAL(-2, ret);
}

/*==============================================================================
 * Test: AES-CMAC computation
 *============================================================================*/

void test_scp03_aes_cmac(void)
{
    /* Test vector from RFC 4493 */
    uint8_t key[16] = {
        0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
    };
    uint8_t message[16] = {
        0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
        0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A
    };
    uint8_t expected_mac[16] = {
        0x07, 0x0A, 0x16, 0xB4, 0x6B, 0x4D, 0x41, 0x44,
        0xF7, 0x9B, 0xDD, 0x9D, 0xD0, 0x4A, 0x28, 0x7C
    };
    uint8_t mac[16];
    int ret;

    /* Test CMAC of full block */
    ret = scp03_aes_cmac(key, 16, message, 16, mac, 16);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_EQUAL_MEMORY(expected_mac, mac, 16);

    /* Test with NULL parameters */
    ret = scp03_aes_cmac(NULL, 16, message, 16, mac, 16);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test with zero length output */
    ret = scp03_aes_cmac(key, 16, message, 16, mac, 0);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test with invalid key length */
    ret = scp03_aes_cmac(key, 10, message, 16, mac, 16);
    TEST_ASSERT_EQUAL(-1, ret);
}

/*==============================================================================
 * Test: AES-CTR encrypt/decrypt
 *============================================================================*/

void test_scp03_aes_ctr(void)
{
    uint8_t key[16] = {
        0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
    };
    uint8_t iv[16] = {0};
    uint8_t plaintext[32] = "Hello, SCP03 AES-CTR Mode!";
    uint8_t ciphertext[32];
    uint8_t decrypted[32];
    int ret;

    /* Test encryption */
    ret = scp03_aes_ctr_encrypt(key, 16, iv, 16, plaintext, ciphertext, 32);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_NOT_EQUAL_MEMORY(plaintext, ciphertext, 32);

    /* Test decryption (same as encryption in CTR mode) */
    ret = scp03_aes_ctr_decrypt(key, 16, iv, 16, ciphertext, decrypted, 32);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, decrypted, 32);

    /* Test NULL params */
    ret = scp03_aes_ctr_encrypt(NULL, 16, iv, 16, plaintext, ciphertext, 32);
    TEST_ASSERT_EQUAL(-1, ret);

    /* Test invalid IV length */
    ret = scp03_aes_ctr_encrypt(key, 16, iv, 8, plaintext, ciphertext, 32);
    TEST_ASSERT_EQUAL(-1, ret);
}

/*==============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_scp03_session_init_clear);
    RUN_TEST(test_scp03_security_level_checks);
    RUN_TEST(test_scp03_session_open_close);
    RUN_TEST(test_scp03_apdu_wrap_unwrap);
    RUN_TEST(test_scp03_apdu_unwrap);
    RUN_TEST(test_scp03_aes_cmac);
    RUN_TEST(test_scp03_aes_ctr);

    return UNITY_END();
}
