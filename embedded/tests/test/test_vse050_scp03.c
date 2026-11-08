/*
 * test_vse050_scp03.c — End-to-end asynchronous SCP03 handshake over T=1'
 *
 * Drives the REAL host stack (se050_t1oi2c.c + se050_scp03.c) against the
 * vSE050 device model (tests/support/vse050_sim.c). Verifies:
 *   - AES-128 / AES-CMAC known-answer vectors (card-side crypto)
 *   - Happy-path handshake with BUSY/poll semantics
 *   - S(WTX) handling
 *   - Recovery from a corrupted response block (CRC retry)
 *   - Timeout when the card answers nothing
 *   - Cryptogram mismatch (wrong static keys)
 *   - EXTERNAL AUTHENTICATE rejection
 *   - PUT KEY personalization (old keys rejected, new keys accepted)
 *   - REPLACE KEYS rotation (derived keys, chained rotations, old keys rejected)
 */

#include "se050_scp03.h"
#include "se050_apdu.h"
#include "se050_t1oi2c.h"
#include "crypto_random.h"
#include "vse050_sim.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else      { g_fail++; printf("  FAIL: %s\n", msg); } \
} while (0)

static int hexeq(const uint8_t *buf, const char *hex, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        unsigned v;
        if (sscanf(&hex[i * 2U], "%2x", &v) != 1) return 0;
        if ((uint8_t)v != buf[i]) return 0;
    }
    return 1;
}

static int open_fresh_session(scp03_session_t *s)
{
    if (se050_scp03_init(s) != SCP03_OK) return -1;
    return se050_scp03_open_session(s, 0x48);
}

static void test_crypto_kat(void)
{
    static const uint8_t key[16] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    static const uint8_t pt[16] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
    };
    uint8_t out[16];

    printf("\n[1] AES-128 / AES-CMAC known-answer\n");
    vse050_sim_aes128_encrypt_block(key, pt, out);
    CHECK(hexeq(out, "69c4e0d86a7b0430d8cdb78070b4c55a", 16),
          "AES-128 FIPS-197 vector");

    {
        uint8_t back[16];

        vse050_sim_aes128_decrypt_block(key, out, back);
        CHECK(hexeq(back, "00112233445566778899aabbccddeeff", 16),
              "AES-128 decrypt FIPS-197 vector");
    }

    {
        static const uint8_t cmac_key[16] = {
            0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
            0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
        };
        static const uint8_t msg16[16] = {
            0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
            0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
        };
        static const uint8_t msg40[40] = {
            0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
            0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
            0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,
            0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51,
            0x30,0xc8,0x1c,0x46,0xa3,0x5c,0xe4,0x11
        };

        vse050_sim_aes_cmac(cmac_key, msg16, sizeof(msg16), out);
        CHECK(hexeq(out, "070a16b46b4d4144f79bdd9dd04a287c", 16),
              "AES-CMAC NIST vector (16 bytes)");

        vse050_sim_aes_cmac(cmac_key, msg40, sizeof(msg40), out);
        CHECK(hexeq(out, "dfa66747de9ae63030ca32611497c827", 16),
              "AES-CMAC NIST vector (40 bytes)");
    }
}

static void test_happy_path(void)
{
    vse050_config_t cfg;
    scp03_session_t s;

    printf("\n[2] Asynchronous handshake (BUSY polling)\n");
    vse050_sim_default_config(&cfg);
    vse050_sim_init(&cfg);

    CHECK(open_fresh_session(&s) == SCP03_OK, "open_session returns SCP03_OK");
    CHECK(se050_scp03_is_open(&s), "host session is OPEN");
    CHECK(vse050_sim_state() == VSE050_STATE_OPEN, "card session is OPEN");
    CHECK(vse050_sim_apdu_count() == 2, "card processed INIT UPDATE + EXT AUTH");
    CHECK(vse050_sim_busy_polls() > 0, "at least one BUSY poll observed");
    CHECK(vse050_sim_now_us() >= (cfg.latency_us * 2U),
          "virtual clock advanced by both APDU latencies");
    CHECK(vse050_sim_nack_polls() == 0, "no NACK polls on happy path");

    se050_scp03_deinit(&s);
}

static void test_wtx(void)
{
    vse050_config_t cfg;
    scp03_session_t s;

    printf("\n[3] S(WTX) before INITIALIZE UPDATE response\n");
    vse050_sim_default_config(&cfg);
    cfg.wtx_on_init_update = 1;
    vse050_sim_init(&cfg);

    CHECK(open_fresh_session(&s) == SCP03_OK, "open_session succeeds across WTX");
    CHECK(vse050_sim_wtx_count() == 1, "exactly one S(WTX request) issued");
    CHECK(vse050_sim_busy_polls() > 0, "BUSY polls after the WTX handshake");
    CHECK(vse050_sim_state() == VSE050_STATE_OPEN, "card session is OPEN");

    se050_scp03_deinit(&s);
}

static void test_crc_retry(void)
{
    vse050_config_t cfg;
    scp03_session_t s;

    printf("\n[4] Corrupted response block recovery\n");
    vse050_sim_default_config(&cfg);
    cfg.corrupt_next_response = 1;
    vse050_sim_init(&cfg);

    CHECK(open_fresh_session(&s) == SCP03_OK, "handshake recovers from bad CRC");
    CHECK(vse050_sim_crc_errors() == 1, "exactly one CRC fault injected");
    CHECK(vse050_sim_state() == VSE050_STATE_OPEN, "card session is OPEN");

    se050_scp03_deinit(&s);
}

static void test_timeout(void)
{
    vse050_config_t cfg;
    scp03_session_t s;
    int rc;

    printf("\n[5] No response (transport timeout)\n");
    vse050_sim_default_config(&cfg);
    cfg.drop_next_response = 1;
    vse050_sim_init(&cfg);

    rc = open_fresh_session(&s);
    CHECK(rc == SCP03_ERR_HW, "open_session reports SCP03_ERR_HW");
    CHECK(vse050_sim_nack_polls() >= SE050_T1_READ_RETRY_MAX,
          "read retry budget exhausted");
    CHECK(vse050_sim_state() != VSE050_STATE_OPEN, "card session never opened");
}

static void test_wrong_keys(void)
{
    vse050_config_t cfg;
    scp03_session_t s;
    int rc;

    printf("\n[6] Card cryptogram mismatch (wrong static keys)\n");
    vse050_sim_default_config(&cfg);
    (void)memset(cfg.mac_key, 0x11, sizeof(cfg.mac_key));
    vse050_sim_init(&cfg);

    rc = open_fresh_session(&s);
    CHECK(rc == SCP03_ERR_CRYPTOGRAM, "host rejects the card cryptogram");
    CHECK(vse050_sim_state() == VSE050_STATE_AUTH_PENDING,
          "card stayed in AUTH_PENDING");
    CHECK(!se050_scp03_is_open(&s), "host session is not open");
}

static void test_ext_auth_rejected(void)
{
    vse050_config_t cfg;
    scp03_session_t s;
    int rc;

    printf("\n[7] EXTERNAL AUTHENTICATE rejected\n");
    vse050_sim_default_config(&cfg);
    cfg.reject_ext_auth = 1;
    vse050_sim_init(&cfg);

    rc = open_fresh_session(&s);
    CHECK(rc == SCP03_ERR_SW, "open_session surfaces the error status word");
    CHECK(vse050_sim_auth_failures() == 1, "card counted one auth failure");
    CHECK(vse050_sim_state() != VSE050_STATE_OPEN, "card session never opened");
}

static void test_session_restart(void)
{
    vse050_config_t cfg;
    scp03_session_t s;

    printf("\n[8] Session re-establishment\n");
    vse050_sim_default_config(&cfg);
    vse050_sim_init(&cfg);

    CHECK(open_fresh_session(&s) == SCP03_OK, "first session opens");
    se050_scp03_close_session(&s);
    CHECK(open_fresh_session(&s) == SCP03_OK, "second session opens");
    CHECK(vse050_sim_apdu_count() == 4, "card processed four handshake APDUs");

    se050_scp03_deinit(&s);
}

static void test_sign_apdu(void)
{
    vse050_config_t cfg;
    scp03_session_t s;
    uint8_t hash[32];
    uint8_t sig[64];
    uint8_t sig2[64];
    int rc;

    printf("\n[9] ECDSA sign APDU over the secure channel\n");
    vse050_sim_default_config(&cfg);
    vse050_sim_init(&cfg);
    CHECK(open_fresh_session(&s) == SCP03_OK, "session open for signing");

    (void)memset(hash, 0xA5, sizeof(hash));

    rc = se050_apdu_sign(&s, 0x48, 0x02, hash, sig);
    CHECK(rc == SE050_APDU_OK, "sign APDU accepted");
    CHECK(vse050_sim_sign_count() == 1, "card produced one signature (74-byte reply)");
    CHECK(vse050_sim_encrypted_cmds() == 1, "command data arrived AES-CBC encrypted");
    CHECK(vse050_sim_last_cipher_len() == 48U, "36-byte plaintext padded to 48-byte block");

    rc = se050_apdu_sign(&s, 0x48, 0x02, hash, sig2);
    CHECK(rc == SE050_APDU_OK, "second sign accepted (C-MAC chaining)");
    CHECK(memcmp(sig, sig2, sizeof(sig)) == 0, "model output is deterministic");
    CHECK(vse050_sim_sign_count() == 2, "card produced two signatures");

    hash[0] ^= 0xFFU;
    rc = se050_apdu_sign(&s, 0x48, 0x02, hash, sig2);
    CHECK(rc == SE050_APDU_OK, "sign accepted for a different digest");
    CHECK(memcmp(sig, sig2, sizeof(sig)) != 0, "signature depends on the digest");

    rc = se050_apdu_sign(&s, 0x48, 0x7F, hash, sig2);
    CHECK(rc == SE050_APDU_ERR_SW, "unknown key id surfaces SW 6A88");
    CHECK(se050_scp03_is_open(&s), "channel survives a command error");

    CHECK(se050_apdu_sign(&s, 0x48, 0x02, NULL, sig2) == SE050_APDU_ERR_PARAM,
          "NULL hash rejected");
    CHECK(se050_apdu_sign(&s, 0x48, 0x02, hash, NULL) == SE050_APDU_ERR_PARAM,
          "NULL signature buffer rejected");

    se050_scp03_close_session(&s);
    CHECK(se050_apdu_sign(&s, 0x48, 0x02, hash, sig2) == SE050_APDU_ERR_CHANNEL,
          "sign rejected without an open channel");

    se050_scp03_deinit(&s);
}

static void test_sign_rmac_tamper(void)
{
    vse050_config_t cfg;
    scp03_session_t s;
    uint8_t hash[32];
    uint8_t sig[64];

    printf("\n[10] Response integrity (tampered R-MAC)\n");
    vse050_sim_default_config(&cfg);
    cfg.corrupt_rmac = 1;
    vse050_sim_init(&cfg);
    CHECK(open_fresh_session(&s) == SCP03_OK, "session open before tamper");

    (void)memset(hash, 0x5A, sizeof(hash));
    CHECK(se050_apdu_sign(&s, 0x48, 0x02, hash, sig) == SE050_APDU_ERR_INTEGRITY,
          "tampered R-MAC rejected");
    CHECK(!se050_scp03_is_open(&s), "channel closed after integrity failure");

    se050_scp03_deinit(&s);
}

static void test_sign_ciphertext_tamper(void)
{
    vse050_config_t cfg;
    scp03_session_t s;
    uint8_t hash[32];
    uint8_t sig[64];

    printf("\n[11] Command encryption integrity (tampered ciphertext)\n");
    vse050_sim_default_config(&cfg);
    cfg.corrupt_ciphertext = 1;
    vse050_sim_init(&cfg);
    CHECK(open_fresh_session(&s) == SCP03_OK, "session open before tamper");

    (void)memset(hash, 0x77, sizeof(hash));
    CHECK(se050_apdu_sign(&s, 0x48, 0x02, hash, sig) == SE050_APDU_ERR_SW,
          "tampered ciphertext rejected (SW=6982)");
    CHECK(vse050_sim_auth_failures() == 1, "card detected the C-MAC mismatch");
    CHECK(vse050_sim_encrypted_cmds() == 0, "no plaintext reached the sign handler");

    se050_scp03_deinit(&s);
}

static void test_put_key_personalization(void)
{
    static const uint8_t k_enc[16] = {
        0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
        0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF
    };
    static const uint8_t k_mac[16] = {
        0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
        0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF
    };
    static const uint8_t k_rmac[16] = {
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,
        0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF
    };
    vse050_config_t cfg;
    scp03_session_t s;
    scp03_session_t old_keys;

    printf("\n[12] PUT KEY personalization\n");
    vse050_sim_default_config(&cfg);
    vse050_sim_init(&cfg);
    CHECK(open_fresh_session(&s) == SCP03_OK, "default transport keys open the channel");

    CHECK(se050_scp03_put_key(&s, 0x48, 0x02, NULL, k_mac, k_rmac) == SCP03_ERR_NULL,
          "NULL key material rejected");
    CHECK(se050_scp03_put_key(&s, 0x48, 0x02, k_enc, k_mac, k_rmac) == SCP03_OK,
          "PUT KEY accepted on the secure channel");
    CHECK(vse050_sim_put_key_count() == 1, "card committed one key set");
    CHECK(vse050_sim_key_version() == 0x02, "card key version advanced to 0x02");
    CHECK(se050_scp03_is_open(&s), "channel survives PUT KEY");

    /* Reconnect with the personalized keys (close_session keeps static keys). */
    se050_scp03_close_session(&s);
    CHECK(se050_scp03_open_session(&s, 0x48) == SCP03_OK,
          "reconnect with personalized keys succeeds");

    /* The default (all-zero) transport keys must no longer authenticate. */
    CHECK(se050_scp03_init(&old_keys) == SCP03_OK, "old-key session initialised");
    CHECK(se050_scp03_open_session(&old_keys, 0x48) == SCP03_ERR_CRYPTOGRAM,
          "default transport keys rejected after personalization");
    se050_scp03_deinit(&old_keys);

    se050_scp03_deinit(&s);
}

static void test_put_key_rejected(void)
{
    static const uint8_t k_enc[16] = { 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
                                       0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F };
    static const uint8_t k_mac[16] = { 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
                                       0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F };
    static const uint8_t k_rmac[16] = { 0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
                                        0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F };
    vse050_config_t cfg;
    scp03_session_t s;

    printf("\n[13] PUT KEY rejection\n");
    vse050_sim_default_config(&cfg);
    vse050_sim_init(&cfg);

    CHECK(se050_scp03_init(&s) == SCP03_OK, "session initialised");
    CHECK(se050_scp03_put_key(&s, 0x48, 0x02, k_enc, k_mac, k_rmac) == SCP03_ERR_CHANNEL,
          "PUT KEY refused without an open channel");

    cfg.reject_put_key = 1;
    vse050_sim_init(&cfg);
    CHECK(open_fresh_session(&s) == SCP03_OK, "channel open before rejected PUT KEY");
    CHECK(se050_scp03_put_key(&s, 0x48, 0x02, k_enc, k_mac, k_rmac) == SCP03_ERR_SW,
          "card rejection surfaces SW 6985");
    CHECK(vse050_sim_put_key_count() == 0, "card committed no key set");
    CHECK(se050_scp03_is_open(&s), "channel survives a rejected PUT KEY");

    se050_scp03_close_session(&s);
    CHECK(se050_scp03_open_session(&s, 0x48) == SCP03_OK,
          "old keys still valid after rejected PUT KEY");

    se050_scp03_deinit(&s);
}

static void test_replace_keys_rotation(void)
{
    vse050_config_t cfg;
    scp03_session_t s;
    scp03_session_t old_keys;
    uint8_t hash[32];
    uint8_t sig[64];

    printf("\n[14] REPLACE KEYS rotation\n");
    vse050_sim_default_config(&cfg);
    vse050_sim_init(&cfg);
    CHECK(open_fresh_session(&s) == SCP03_OK, "session open before rotation");
    CHECK(vse050_sim_key_version() == 0x00, "card starts at key version 0");

    CHECK(se050_scp03_rotate_keys(&s, 0x48) == SCP03_OK,
          "rotate_keys sends REPLACE KEYS and re-opens the channel");
    CHECK(vse050_sim_replace_key_count() == 1, "card performed one rotation");
    CHECK(vse050_sim_key_version() == 0x01, "card key version advanced to 1");
    CHECK(se050_scp03_is_open(&s), "host channel is OPEN after rotation");

    /* The re-opened channel must work end-to-end. */
    (void)memset(hash, 0x3C, sizeof(hash));
    CHECK(se050_apdu_sign(&s, 0x48, 0x02, hash, sig) == SE050_APDU_OK,
          "sign APDU works on the rotated channel");
    CHECK(vse050_sim_sign_count() == 1, "card accepted the signature");

    /* Pre-rotation (default) keys must no longer authenticate. */
    CHECK(se050_scp03_init(&old_keys) == SCP03_OK, "old-key session initialised");
    CHECK(se050_scp03_open_session(&old_keys, 0x48) == SCP03_ERR_CRYPTOGRAM,
          "pre-rotation keys rejected");
    se050_scp03_deinit(&old_keys);

    /* Re-establish the rotated session, then chain a second rotation. */
    se050_scp03_close_session(&s);
    CHECK(se050_scp03_open_session(&s, 0x48) == SCP03_OK,
          "rotated key session re-established");
    CHECK(se050_scp03_rotate_keys(&s, 0x48) == SCP03_OK, "second rotation succeeds");
    CHECK(vse050_sim_replace_key_count() == 2, "card performed two rotations");
    CHECK(vse050_sim_key_version() == 0x02, "card key version advanced to 2");

    (void)memset(hash, 0x4D, sizeof(hash));
    CHECK(se050_apdu_sign(&s, 0x48, 0x02, hash, sig) == SE050_APDU_OK,
          "sign APDU works after the second rotation");

    se050_scp03_deinit(&s);
}

static void test_replace_keys_rejected(void)
{
    vse050_config_t cfg;
    scp03_session_t s;

    printf("\n[15] REPLACE KEYS rejection\n");
    vse050_sim_default_config(&cfg);
    vse050_sim_init(&cfg);

    CHECK(se050_scp03_init(&s) == SCP03_OK, "session initialised");
    CHECK(se050_scp03_replace_keys(&s, 0x48, 0x01) == SCP03_ERR_CHANNEL,
          "REPLACE KEYS refused without an open channel");

    cfg.reject_replace_keys = 1;
    vse050_sim_init(&cfg);
    CHECK(open_fresh_session(&s) == SCP03_OK, "channel open before rejected rotation");
    CHECK(se050_scp03_replace_keys(&s, 0x48, 0x01) == SCP03_ERR_SW,
          "card rejection surfaces SW 6985");
    CHECK(vse050_sim_replace_key_count() == 0, "no rotation committed");
    CHECK(se050_scp03_is_open(&s), "channel survives a rejected rotation");

    se050_scp03_close_session(&s);
    CHECK(se050_scp03_open_session(&s, 0x48) == SCP03_OK,
          "old keys remain valid after rejected rotation");

    se050_scp03_deinit(&s);
}

int main(void)
{
    printf("=== vSE050 asynchronous SCP03 simulation ===\n");

    CHECK(crypto_random_init() == CRYPTO_RANDOM_OK, "entropy source initialised");

    test_crypto_kat();
    test_happy_path();
    test_wtx();
    test_crc_retry();
    test_timeout();
    test_wrong_keys();
    test_ext_auth_rejected();
    test_session_restart();
    test_sign_apdu();
    test_sign_rmac_tamper();
    test_sign_ciphertext_tamper();
    test_put_key_personalization();
    test_put_key_rejected();
    test_replace_keys_rotation();
    test_replace_keys_rejected();

    printf("\n=== 结果: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
