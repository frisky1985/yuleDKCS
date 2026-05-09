/**
 * @file scp03_session.c
 * @brief SCP03 Session Management Implementation
 *
 * Implements session establishment, key derivation, and secure channel
 * operations for GlobalPlatform SCP03 protocol.
 *
 * @copyright yuleDKCS
 * @version 1.0
 */

#include "scp03.h"
#include <string.h>
#include <stdbool.h>

/*==============================================================================
 * KDF LABELS (GlobalPlatform SCP03 Specification)
 *============================================================================*/

/** KDF label for ENC key derivation (identifier 00 00 01) */
#define SCP03_KDF_LABEL_ENC     "\x00\x00\x01"
#define SCP03_KDF_LABEL_ENC_LEN 3

/** KDF label for MAC key derivation (identifier 00 00 02) */
#define SCP03_KDF_LABEL_MAC     "\x00\x00\x02"
#define SCP03_KDF_LABEL_MAC_LEN 3

/** KDF label for DEK key derivation (identifier 00 00 03) */
#define SCP03_KDF_LABEL_DEK     "\x00\x00\x03"
#define SCP03_KDF_LABEL_DEK_LEN 3

/** KDF constant for key derivation */
#define SCP03_KDF_CONSTANT      "\x00\x00\x00\x00\x00\x00\x00\x00"
#define SCP03_KDF_CONSTANT_LEN  8

/*==============================================================================
 * INTERNAL CONSTANTS
 *============================================================================*/

/** APDU command offsets */
#define APDU_CLA_OFFSET         0
#define APDU_INS_OFFSET         1
#define APDU_P1_OFFSET          2
#define APDU_P2_OFFSET          3
#define APDU_LC_OFFSET          4
#define APDU_DATA_OFFSET        5

/** SCP03 APDU header lengths */
#define SCP03_MAC_DATA_LEN      16
#define SCP03_IV_LEN            16

/*==============================================================================
 * STATIC FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief AES-CMAC computation (RFC 4493)
 *
 * @param key       AES key
 * @param key_len   Key length in bytes (16, 24, or 32)
 * @param data      Input data
 * @param data_len  Data length
 * @param mac       Output MAC (16 bytes)
 * @return 0 on success, -1 on failure
 */
static int aes_cmac(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *data,
    size_t data_len,
    uint8_t *mac
);

/**
 * @brief AES-ECB encryption
 *
 * @param key       AES key
 * @param key_len   Key length in bytes
 * @param plaintext Input block (16 bytes)
 * @param ciphertext Output block (16 bytes)
 * @return 0 on success, -1 on failure
 */
static int aes_ecb_encrypt(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *plaintext,
    uint8_t *ciphertext
);

/**
 * @brief Generate subkeys for CMAC
 *
 * @param key       AES key
 * @param key_len   Key length
 * @param k1        Output subkey K1 (16 bytes)
 * @param k2        Output subkey K2 (16 bytes)
 * @return 0 on success, -1 on failure
 */
static int cmac_generate_subkeys(
    const uint8_t *key,
    size_t key_len,
    uint8_t *k1,
    uint8_t *k2
);

/**
 * @brief Left shift a 16-byte block by 1 bit
 *
 * @param input     Input block
 * @param output    Output block
 */
static void block_left_shift(const uint8_t *input, uint8_t *output);

/**
 * @brief XOR two 16-byte blocks
 *
 * @param a         First block
 * @param b         Second block
 * @param out       Output block
 */
static void block_xor(const uint8_t *a, const uint8_t *b, uint8_t *out);

/**
 * @brief SCP03 Key Derivation Function (KDF)
 *
 * Derives session keys using AES-CMAC as per GlobalPlatform spec.
 *
 * @param master_key    Master/static key
 * @param key_len       Key length
 * @param label         Derivation label
 * @param label_len     Label length
 * @param context       Derivation context
 * @param context_len   Context length
 * @param output        Derived key output
 * @param output_len    Desired output length
 * @return 0 on success, -1 on failure
 */
static int scp03_kdf(
    const uint8_t *master_key,
    size_t key_len,
    const uint8_t *label,
    size_t label_len,
    const uint8_t *context,
    size_t context_len,
    uint8_t *output,
    size_t output_len
);

/**
 * @brief Derive all session keys (ENC, MAC, DEK)
 *
 * @param session       Session context
 * @param static_keys   Static keys
 * @param context       Derivation context (host_challenge || card_challenge)
 * @return 0 on success, -1 on failure
 */
static int derive_session_keys(
    scp03_session_t *session,
    const scp03_static_keys_t *static_keys,
    const uint8_t *context
);

/**
 * @brief Secure memory clear
 *
 * @param ptr   Pointer to memory
 * @param len   Length to clear
 */
static void secure_clear(void *ptr, size_t len);

/**
 * @brief Check if security level requires encryption
 *
 * @param level     Security level
 * @return true if encryption required
 */
static bool level_requires_encryption(scp03_security_level_t level);

/**
 * @brief Check if security level requires MAC
 *
 * @param level     Security level
 * @return true if MAC required
 */
static bool level_requires_mac(scp03_security_level_t level);

/**
 * @brief Mock function to get key version from SE
 *
 * In real implementation, this would communicate with the SE
 * via INITIALIZE UPDATE command.
 *
 * @param key_version   Output key version
 * @param card_challenge Output card challenge
 * @return 0 on success, -1 on failure
 */
static int se_get_key_version(uint8_t *key_version, uint8_t *card_challenge);

/*==============================================================================
 * API IMPLEMENTATION
 *============================================================================*/

void scp03_session_init(scp03_session_t *session)
{
    if (session == NULL) {
        return;
    }

    memset(session, 0, sizeof(scp03_session_t));
    session->session_open = 0U;
    session->first_command = 1U;
    session->security_level = SCP03_I_00;
}

void scp03_session_clear(scp03_session_t *session)
{
    if (session == NULL) {
        return;
    }

    /* Securely wipe session keys */
    secure_clear(session->session_keys, sizeof(session->session_keys));

    /* Clear cryptographic state */
    secure_clear(session->mac_chaining_value, sizeof(session->mac_chaining_value));
    secure_clear(session->icv, sizeof(session->icv));

    /* Clear session identifiers */
    secure_clear(session->host_challenge, sizeof(session->host_challenge));
    secure_clear(session->card_challenge, sizeof(session->card_challenge));
    secure_clear(session->session_id, sizeof(session->session_id));

    /* Reset counters and state */
    session->mac_counter = 0U;
    session->session_open = 0U;
    session->first_command = 0U;
    session->security_level = SCP03_I_00;
}

int scp03_open_session(scp03_session_t *session, const scp03_config_t *config)
{
    int result = -1;
    uint8_t key_version = 0U;
    uint8_t card_challenge[SCP03_CHALLENGE_SIZE];
    uint8_t derivation_context[SCP03_CHALLENGE_SIZE * 2];

    /* Validate parameters */
    if ((session == NULL) || (config == NULL)) {
        return -1;  /* Invalid parameter */
    }

    /* Validate key length */
    if ((config->static_keys.key_len != SCP03_KEY_SIZE_16) &&
        (config->static_keys.key_len != SCP03_KEY_SIZE_24) &&
        (config->static_keys.key_len != SCP03_KEY_SIZE_32)) {
        return -1;  /* Invalid key length */
    }

    /* Validate security level */
    if ((config->security_level != SCP03_I_04) &&
        (config->security_level != SCP03_I_0C) &&
        (config->security_level != SCP03_I_14)) {
        return -1;  /* Invalid security level */
    }

    /* Initialize session context */
    scp03_session_init(session);

    /* Store host challenge */
    memcpy(session->host_challenge, config->host_challenge, SCP03_CHALLENGE_SIZE);

    /* Store key length */
    session->key_len = config->static_keys.key_len;

    /* Store security level */
    session->security_level = config->security_level;

    /* Get key version and card challenge from SE */
    result = se_get_key_version(&key_version, card_challenge);
    if (result != 0) {
        return -4;  /* Communication error */
    }

    /* Store card challenge */
    memcpy(session->card_challenge, card_challenge, SCP03_CHALLENGE_SIZE);

    /* Build derivation context: host_challenge || card_challenge */
    memcpy(derivation_context, session->host_challenge, SCP03_CHALLENGE_SIZE);
    memcpy(&derivation_context[SCP03_CHALLENGE_SIZE], session->card_challenge, SCP03_CHALLENGE_SIZE);

    /* Derive session keys */
    result = derive_session_keys(session, &config->static_keys, derivation_context);
    if (result != 0) {
        scp03_session_clear(session);
        return -2;  /* Key derivation failed */
    }

    /* Initialize ICV (all zeros for first command) */
    memset(session->icv, 0, SCP03_ICV_SIZE);

    /* Initialize MAC chaining value */
    memset(session->mac_chaining_value, 0, SCP03_ICV_SIZE);

    /* Initialize sequence counter */
    session->mac_counter = 0U;

    /* Mark session as open and first command */
    session->session_open = 1U;
    session->first_command = 1U;

    return 0;  /* Success */
}

int scp03_close_session(scp03_session_t *session)
{
    if (session == NULL) {
        return -1;  /* Invalid parameter */
    }

    if (!session->session_open) {
        return 0;  /* Already closed */
    }

    /* Securely clear all session data */
    scp03_session_clear(session);

    return 0;  /* Success */
}

int scp03_wrap_apdu(scp03_session_t *session,
                    const scp03_apdu_t *apdu_in,
                    uint8_t *apdu_out,
                    size_t *out_len)
{
    /* Placeholder for APDU wrapping - would be implemented in separate file */
    (void)session;
    (void)apdu_in;
    (void)apdu_out;
    (void)out_len;
    return 0;
}

int scp03_unwrap_apdu(scp03_session_t *session,
                      const uint8_t *resp_in,
                      size_t resp_len,
                      uint8_t *resp_out,
                      size_t *out_len)
{
    /* Placeholder for APDU unwrapping - would be implemented in separate file */
    (void)session;
    (void)resp_in;
    (void)resp_len;
    (void)resp_out;
    (void)out_len;
    return 0;
}

/*==============================================================================
 * STATIC FUNCTION IMPLEMENTATIONS
 *============================================================================*/

static void secure_clear(void *ptr, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)ptr;

    if ((p == NULL) || (len == 0U)) {
        return;
    }

    /* Write zeros to memory */
    while (len > 0U) {
        *p = 0U;
        p++;
        len--;
    }

    /* Memory barrier to prevent optimization */
    __asm__ volatile ("" ::: "memory");
}

static bool level_requires_encryption(scp03_security_level_t level)
{
    return ((level == SCP03_I_0C) || (level == SCP03_I_14));
}

static bool level_requires_mac(scp03_security_level_t level)
{
    return ((level == SCP03_I_04) || (level == SCP03_I_0C) || (level == SCP03_I_14));
}

static void block_left_shift(const uint8_t *input, uint8_t *output)
{
    uint8_t i;
    uint8_t carry = 0U;

    for (i = 16U; i > 0U; i--) {
        output[i - 1U] = (input[i - 1U] << 1) | carry;
        carry = (input[i - 1U] & 0x80U) ? 1U : 0U;
    }
}

static void block_xor(const uint8_t *a, const uint8_t *b, uint8_t *out)
{
    uint8_t i;

    for (i = 0U; i < 16U; i++) {
        out[i] = a[i] ^ b[i];
    }
}

static int aes_ecb_encrypt(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *plaintext,
    uint8_t *ciphertext)
{
    /* 
     * This is a simplified AES-ECB implementation for key derivation.
     * In production, this should use hardware acceleration via SE050
     * or a proper software AES implementation.
     */
    
    /* Validate parameters */
    if ((key == NULL) || (plaintext == NULL) || (ciphertext == NULL)) {
        return -1;
    }

    if ((key_len != 16U) && (key_len != 24U) && (key_len != 32U)) {
        return -1;
    }

    /* 
     * TODO: Replace with actual AES-ECB implementation
     * For now, this is a stub that would need to be connected to:
     * 1. SE050 AES operations, or
     * 2. A software AES library (like mbedTLS)
     */
    
    (void)key;
    (void)key_len;
    
    /* Copy plaintext to ciphertext (placeholder) */
    memcpy(ciphertext, plaintext, 16);
    
    return 0;
}

static int cmac_generate_subkeys(
    const uint8_t *key,
    size_t key_len,
    uint8_t *k1,
    uint8_t *k2)
{
    uint8_t zero[16] = {0};
    uint8_t l[16];
    uint8_t rb[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87};
    int result;

    if ((key == NULL) || (k1 == NULL) || (k2 == NULL)) {
        return -1;
    }

    /* L = AES-CMAC(K, 0^128) - actually just AES encryption for subkey gen */
    result = aes_ecb_encrypt(key, key_len, zero, l);
    if (result != 0) {
        return -1;
    }

    /* K1 = L << 1 */
    block_left_shift(l, k1);
    if ((l[0] & 0x80U) != 0U) {
        block_xor(k1, rb, k1);
    }

    /* K2 = K1 << 1 */
    block_left_shift(k1, k2);
    if ((k1[0] & 0x80U) != 0U) {
        block_xor(k2, rb, k2);
    }

    return 0;
}

static int aes_cmac(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *data,
    size_t data_len,
    uint8_t *mac)
{
    uint8_t k1[16];
    uint8_t k2[16];
    uint8_t x[16] = {0};
    uint8_t y[16];
    uint8_t m_last[16];
    uint8_t padded[16];
    size_t n;
    size_t i;
    size_t j;
    int result;
    bool complete;

    if ((key == NULL) || (data == NULL) || (mac == NULL)) {
        return -1;
    }

    /* Generate subkeys */
    result = cmac_generate_subkeys(key, key_len, k1, k2);
    if (result != 0) {
        return -1;
    }

    /* Calculate number of blocks */
    if (data_len == 0U) {
        n = 1U;
        complete = false;
    } else {
        n = (data_len + 15U) / 16U;
        complete = ((data_len % 16U) == 0U);
    }

    /* Process all blocks except last */
    for (i = 0U; i < (n - 1U); i++) {
        block_xor(x, &data[i * 16U], y);
        result = aes_ecb_encrypt(key, key_len, y, x);
        if (result != 0) {
            return -1;
        }
    }

    /* Process last block */
    if (complete) {
        /* Complete block: XOR with K1 */
        block_xor(&data[(n - 1U) * 16U], k1, m_last);
    } else {
        /* Incomplete block: pad and XOR with K2 */
        size_t rem = data_len % 16U;
        memcpy(padded, &data[(n - 1U) * 16U], rem);
        padded[rem] = 0x80U;
        for (j = rem + 1U; j < 16U; j++) {
            padded[j] = 0x00U;
        }
        block_xor(padded, k2, m_last);
    }

    block_xor(x, m_last, y);
    result = aes_ecb_encrypt(key, key_len, y, mac);
    if (result != 0) {
        return -1;
    }

    return 0;
}

static int scp03_kdf(
    const uint8_t *master_key,
    size_t key_len,
    const uint8_t *label,
    size_t label_len,
    const uint8_t *context,
    size_t context_len,
    uint8_t *output,
    size_t output_len)
{
    uint8_t derivation_data[64];
    size_t dd_len = 0U;
    size_t i;
    uint8_t mac[16];
    int result;

    if ((master_key == NULL) || (label == NULL) || (context == NULL) || (output == NULL)) {
        return -1;
    }

    /* 
     * SCP03 KDF using AES-CMAC:
     * Derivation Data = Label || 00 || Context || L
     * where L is the desired output length in bits (16 bits, big-endian)
     */

    /* Build derivation data */
    memset(derivation_data, 0, sizeof(derivation_data));

    /* Add label */
    memcpy(&derivation_data[dd_len], label, label_len);
    dd_len += label_len;

    /* Add separator byte (0x00) */
    derivation_data[dd_len] = 0x00;
    dd_len += 1U;

    /* Add context */
    memcpy(&derivation_data[dd_len], context, context_len);
    dd_len += context_len;

    /* Add length (in bits, big-endian) - 128 bits for AES key */
    derivation_data[dd_len] = 0x00;
    derivation_data[dd_len + 1U] = 0x80U;  /* 128 bits = 16 bytes */
    dd_len += 2U;

    /* Derive key using AES-CMAC */
    result = aes_cmac(master_key, key_len, derivation_data, dd_len, mac);
    if (result != 0) {
        return -1;
    }

    /* Copy output (truncated if necessary) */
    if (output_len > 16U) {
        output_len = 16U;
    }
    memcpy(output, mac, output_len);

    /* Clear sensitive data */
    secure_clear(derivation_data, sizeof(derivation_data));
    secure_clear(mac, sizeof(mac));

    return 0;
}

static int derive_session_keys(
    scp03_session_t *session,
    const scp03_static_keys_t *static_keys,
    const uint8_t *context)
{
    int result;

    if ((session == NULL) || (static_keys == NULL) || (context == NULL)) {
        return -1;
    }

    /* Derive ENC key using identifier 00 00 01 */
    result = scp03_kdf(
        static_keys->enc_key,
        static_keys->key_len,
        (const uint8_t *)SCP03_KDF_LABEL_ENC,
        SCP03_KDF_LABEL_ENC_LEN,
        context,
        SCP03_CHALLENGE_SIZE * 2,
        session->session_keys[SCP03_KEY_ENC],
        static_keys->key_len
    );
    if (result != 0) {
        return -1;
    }

    /* Derive MAC key using identifier 00 00 02 */
    result = scp03_kdf(
        static_keys->mac_key,
        static_keys->key_len,
        (const uint8_t *)SCP03_KDF_LABEL_MAC,
        SCP03_KDF_LABEL_MAC_LEN,
        context,
        SCP03_CHALLENGE_SIZE * 2,
        session->session_keys[SCP03_KEY_MAC],
        static_keys->key_len
    );
    if (result != 0) {
        return -1;
    }

    /* Derive DEK key using identifier 00 00 03 */
    result = scp03_kdf(
        static_keys->dek_key,
        static_keys->key_len,
        (const uint8_t *)SCP03_KDF_LABEL_DEK,
        SCP03_KDF_LABEL_DEK_LEN,
        context,
        SCP03_CHALLENGE_SIZE * 2,
        session->session_keys[SCP03_KEY_DEK],
        static_keys->key_len
    );
    if (result != 0) {
        return -1;
    }

    return 0;
}

static int se_get_key_version(uint8_t *key_version, uint8_t *card_challenge)
{
    /*
     * Mock implementation - in real scenario this would:
     * 1. Send INITIALIZE UPDATE APDU to SE
     * 2. Receive key version and card challenge from SE response
     * 
     * Response format (29 bytes):
     * - Key diversification data (10 bytes)
     * - Key version (1 byte)
     * - Secure Element serial number (8 bytes)
     * - Card challenge (8 bytes)
     * - Card cryptogram (8 bytes)
     */

    if ((key_version == NULL) || (card_challenge == NULL)) {
        return -1;
    }

    /* Mock key version - typically 0x01 or higher */
    *key_version = 0x01;

    /* Mock card challenge - in reality from SE RNG */
    card_challenge[0] = 0x01;
    card_challenge[1] = 0x02;
    card_challenge[2] = 0x03;
    card_challenge[3] = 0x04;
    card_challenge[4] = 0x05;
    card_challenge[5] = 0x06;
    card_challenge[6] = 0x07;
    card_challenge[7] = 0x08;

    return 0;
}
