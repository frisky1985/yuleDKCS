/**
 * @file scp03_crypto.c
 * @brief SCP03 (Secure Channel Protocol '03') Cryptographic Implementation
 *
 * Implementation of SCP03 cryptographic primitives:
 * - AES-CMAC calculation
 * - AES-CTR encryption/decryption
 * - APDU security wrapping/unwrapping
 *
 * @copyright yuleDKCS
 * @version 1.0
 */

#include <string.h>
#include "scp03.h"
#include "mbedtls/cipher.h"
#include "mbedtls/cmac.h"

/*==============================================================================
 * LOCAL CONSTANTS
 *============================================================================*/

/** Block size for AES (128 bits) */
#define AES_BLOCK_SIZE      16

/** CMAC constant for subkey generation */
static const uint8_t CMAC_CONST_RB[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

/*==============================================================================
 * LOCAL FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Left shift a 128-bit block by 1 bit
 */
static void block_left_shift_1(const uint8_t *in, uint8_t *out);

/**
 * @brief XOR two 128-bit blocks
 */
static void block_xor(const uint8_t *a, const uint8_t *b, uint8_t *out);

/**
 * @brief Generate CMAC subkeys K1 and K2
 */
static int cmac_generate_subkeys(const uint8_t *key, size_t key_len,
                                 uint8_t *k1, uint8_t *k2);

/**
 * @brief Pad data to AES block size using ISO/IEC 9797-1 padding method 2
 */
static void cmac_pad_block(const uint8_t *data, size_t data_len, uint8_t *out);

/**
 * @brief Get mbedtls cipher type from key length
 */
static mbedtls_cipher_type_t get_cipher_type(size_t key_len);

/**
 * @brief Calculate MAC for APDU command/response
 */
static int calculate_mac(scp03_session_t *session,
                         const uint8_t *data, size_t data_len,
                         uint8_t *mac_out, int is_response);

/**
 * @brief Check if a security level has C-MAC enabled
 */
static int has_c_mac(scp03_security_level_t level);

/**
 * @brief Check if a security level has C-ENC enabled
 */
static int has_c_enc(scp03_security_level_t level);

/**
 * @brief Check if a security level has R-MAC enabled
 */
static int has_r_mac(scp03_security_level_t level);

/**
 * @brief Check if a security level has R-ENC enabled
 */
static int has_r_enc(scp03_security_level_t level);

/*==============================================================================
 * AES-CMAC IMPLEMENTATION
 *============================================================================*/

static void block_left_shift_1(const uint8_t *in, uint8_t *out)
{
    int i;
    uint8_t carry = 0;
    
    for (i = 15; i >= 0; i--) {
        uint8_t new_carry = (in[i] & 0x80) ? 1 : 0;
        out[i] = (in[i] << 1) | carry;
        carry = new_carry;
    }
}

static void block_xor(const uint8_t *a, const uint8_t *b, uint8_t *out)
{
    int i;
    for (i = 0; i < 16; i++) {
        out[i] = a[i] ^ b[i];
    }
}

static int cmac_generate_subkeys(const uint8_t *key, size_t key_len,
                                 uint8_t *k1, uint8_t *k2)
{
    mbedtls_cipher_context_t ctx;
    const mbedtls_cipher_info_t *cipher_info;
    uint8_t zero[16] = {0};
    uint8_t l[16];
    int ret;
    mbedtls_cipher_type_t cipher_type;
    
    mbedtls_cipher_init(&ctx);
    
    cipher_type = get_cipher_type(key_len);
    if (cipher_type == 0) {
        return -1;
    }
    
    cipher_info = mbedtls_cipher_info_from_type(cipher_type);
    if (cipher_info == NULL) {
        return -1;
    }
    
    ret = mbedtls_cipher_setup(&ctx, cipher_info);
    if (ret != 0) {
        return ret;
    }
    
    ret = mbedtls_cipher_setkey(&ctx, key, (int)(key_len * 8), MBEDTLS_ENCRYPT);
    if (ret != 0) {
        mbedtls_cipher_free(&ctx);
        return ret;
    }
    
    /* L = AES-CMAC(K, 0^128) */
    ret = mbedtls_cipher_update(&ctx, zero, 16, l, NULL);
    if (ret != 0) {
        mbedtls_cipher_free(&ctx);
        return ret;
    }
    
    mbedtls_cipher_free(&ctx);
    
    /* K1 = L << 1 */
    block_left_shift_1(l, k1);
    if (l[0] & 0x80) {
        block_xor(k1, CMAC_CONST_RB, k1);
    }
    
    /* K2 = K1 << 1 */
    block_left_shift_1(k1, k2);
    if (k1[0] & 0x80) {
        block_xor(k2, CMAC_CONST_RB, k2);
    }
    
    return 0;
}

static void cmac_pad_block(const uint8_t *data, size_t data_len, uint8_t *out)
{
    size_t i;
    
    for (i = 0; i < data_len; i++) {
        out[i] = data[i];
    }
    
    out[data_len] = 0x80;
    
    for (i = data_len + 1; i < 16; i++) {
        out[i] = 0x00;
    }
}

static mbedtls_cipher_type_t get_cipher_type(size_t key_len)
{
    switch (key_len) {
        case 16:
            return MBEDTLS_CIPHER_AES_128_ECB;
        case 24:
            return MBEDTLS_CIPHER_AES_192_ECB;
        case 32:
            return MBEDTLS_CIPHER_AES_256_ECB;
        default:
            return 0;
    }
}

int scp03_aes_cmac(const uint8_t *key, size_t key_len,
                   const uint8_t *data, size_t data_len,
                   uint8_t *mac, size_t mac_len)
{
    mbedtls_cipher_context_t ctx;
    const mbedtls_cipher_info_t *cipher_info;
    uint8_t k1[16], k2[16];
    uint8_t block[16];
    uint8_t x[16] = {0};
    uint8_t y[16];
    size_t n, i, j;
    int ret;
    size_t full_mac[16];
    mbedtls_cipher_type_t cipher_type;
    
    if (key == NULL || data == NULL || mac == NULL) {
        return -1;
    }
    
    if (mac_len == 0 || mac_len > 16) {
        return -1;
    }
    
    if (data_len == 0) {
        /* Special case: empty message */
        uint8_t padded[16];
        cmac_pad_block(NULL, 0, padded);
        
        ret = cmac_generate_subkeys(key, key_len, k1, k2);
        if (ret != 0) {
            return ret;
        }
        
        block_xor(padded, k2, y);
        
        mbedtls_cipher_init(&ctx);
        cipher_type = get_cipher_type(key_len);
        cipher_info = mbedtls_cipher_info_from_type(cipher_type);
        mbedtls_cipher_setup(&ctx, cipher_info);
        mbedtls_cipher_setkey(&ctx, key, (int)(key_len * 8), MBEDTLS_ENCRYPT);
        mbedtls_cipher_update(&ctx, y, 16, (uint8_t *)full_mac, NULL);
        mbedtls_cipher_free(&ctx);
        
        memcpy(mac, full_mac, mac_len);
        return 0;
    }
    
    /* Generate subkeys */
    ret = cmac_generate_subkeys(key, key_len, k1, k2);
    if (ret != 0) {
        return ret;
    }
    
    n = (data_len + 15) / 16;
    
    mbedtls_cipher_init(&ctx);
    cipher_type = get_cipher_type(key_len);
    cipher_info = mbedtls_cipher_info_from_type(cipher_type);
    
    ret = mbedtls_cipher_setup(&ctx, cipher_info);
    if (ret != 0) {
        return ret;
    }
    
    ret = mbedtls_cipher_setkey(&ctx, key, (int)(key_len * 8), MBEDTLS_ENCRYPT);
    if (ret != 0) {
        mbedtls_cipher_free(&ctx);
        return ret;
    }
    
    /* Process full blocks */
    for (i = 0; i < n - 1; i++) {
        block_xor(x, &data[i * 16], y);
        mbedtls_cipher_update(&ctx, y, 16, x, NULL);
    }
    
    /* Process last block */
    if (data_len % 16 == 0 && data_len > 0) {
        /* Complete block: XOR with K1 */
        block_xor(x, &data[(n - 1) * 16], y);
        block_xor(y, k1, y);
    } else {
        /* Incomplete block: pad and XOR with K2 */
        cmac_pad_block(&data[(n - 1) * 16], data_len % 16, block);
        block_xor(x, block, y);
        block_xor(y, k2, y);
    }
    
    mbedtls_cipher_update(&ctx, y, 16, (uint8_t *)full_mac, NULL);
    mbedtls_cipher_free(&ctx);
    
    memcpy(mac, full_mac, mac_len);
    
    return 0;
}

/*==============================================================================
 * AES-CTR IMPLEMENTATION
 *============================================================================*/

int scp03_aes_ctr_encrypt(const uint8_t *key, size_t key_len,
                          const uint8_t *iv, size_t iv_len,
                          const uint8_t *input, uint8_t *output,
                          size_t data_len)
{
    mbedtls_cipher_context_t ctx;
    const mbedtls_cipher_info_t *cipher_info;
    uint8_t stream[16];
    uint8_t counter[16];
    size_t nc_off = 0;
    size_t i, j;
    int ret;
    mbedtls_cipher_type_t cipher_type;
    
    if (key == NULL || iv == NULL || input == NULL || output == NULL) {
        return -1;
    }
    
    if (iv_len != 16) {
        return -1;
    }
    
    cipher_type = get_cipher_type(key_len);
    if (cipher_type == 0) {
        return -1;
    }
    
    mbedtls_cipher_init(&ctx);
    
    cipher_info = mbedtls_cipher_info_from_type(cipher_type);
    if (cipher_info == NULL) {
        return -1;
    }
    
    ret = mbedtls_cipher_setup(&ctx, cipher_info);
    if (ret != 0) {
        return ret;
    }
    
    ret = mbedtls_cipher_setkey(&ctx, key, (int)(key_len * 8), MBEDTLS_ENCRYPT);
    if (ret != 0) {
        mbedtls_cipher_free(&ctx);
        return ret;
    }
    
    memcpy(counter, iv, 16);
    
    /* CTR mode encryption: C[i] = P[i] ^ AES(K, CTR[i]) */
    for (i = 0; i < data_len; i += 16) {
        /* Generate keystream */
        ret = mbedtls_cipher_update(&ctx, counter, 16, stream, NULL);
        if (ret != 0) {
            mbedtls_cipher_free(&ctx);
            return ret;
        }
        
        /* XOR with plaintext/ciphertext */
        size_t block_len = (data_len - i < 16) ? (data_len - i) : 16;
        for (j = 0; j < block_len; j++) {
            output[i + j] = input[i + j] ^ stream[j];
        }
        
        /* Increment counter */
        for (j = 16; j > 0; j--) {
            if (++counter[j - 1] != 0) {
                break;
            }
        }
    }
    
    mbedtls_cipher_free(&ctx);
    
    return 0;
}

int scp03_aes_ctr_decrypt(const uint8_t *key, size_t key_len,
                          const uint8_t *iv, size_t iv_len,
                          const uint8_t *input, uint8_t *output,
                          size_t data_len)
{
    /* In CTR mode, encryption and decryption are identical operations */
    return scp03_aes_ctr_encrypt(key, key_len, iv, iv_len, input, output, data_len);
}

/*==============================================================================
 * SECURITY LEVEL CHECKS
 *============================================================================*/

static int has_c_mac(scp03_security_level_t level)
{
    return (level == SCP03_I_04) || (level == SCP03_I_0C) || (level == SCP03_I_14);
}

static int has_c_enc(scp03_security_level_t level)
{
    return (level == SCP03_I_0C) || (level == SCP03_I_14);
}

static int has_r_mac(scp03_security_level_t level)
{
    return (level == SCP03_I_14);
}

static int has_r_enc(scp03_security_level_t level)
{
    return (level == SCP03_I_14);
}

/*==============================================================================
 * MAC CALCULATION
 *============================================================================*/

static int calculate_mac(scp03_session_t *session,
                         const uint8_t *data, size_t data_len,
                         uint8_t *mac_out, int is_response)
{
    uint8_t *mac_key;
    uint8_t full_mac[16];
    int ret;
    
    if (session == NULL || data == NULL || mac_out == NULL) {
        return -1;
    }
    
    /* Select appropriate key (MAC key) */
    mac_key = session->session_keys[SCP03_KEY_MAC];
    
    /* Calculate CMAC */
    ret = scp03_aes_cmac(mac_key, session->key_len, data, data_len, full_mac, 16);
    if (ret != 0) {
        return ret;
    }
    
    /* Truncate to 8 bytes for SCP03 */
    memcpy(mac_out, full_mac, SCP03_MAC_SIZE);
    
    return 0;
}

/*==============================================================================
 * APDU WRAP (SECURITY TRANSFORMATION)
 *============================================================================*/

int scp03_wrap_apdu(scp03_session_t *session,
                    const scp03_apdu_t *apdu_in,
                    uint8_t *apdu_out,
                    size_t *out_len)
{
    uint8_t *out_ptr;
    size_t total_len;
    size_t mac_data_len;
    uint8_t mac_data[SCP03_MAX_APDU_SIZE + 64];
    uint8_t mac[SCP03_MAC_SIZE];
    uint8_t encrypted_data[SCP03_MAX_APDU_SIZE];
    size_t encrypted_len;
    size_t header_len = 4; /* CLA + INS + P1 + P2 */
    int ret;
    
    if (session == NULL || apdu_in == NULL || apdu_out == NULL || out_len == NULL) {
        return -1;
    }
    
    if (!session->session_open) {
        return -2;
    }
    
    if (apdu_in->data_len > SCP03_MAX_APDU_SIZE) {
        return -1;
    }
    
    out_ptr = apdu_out;
    total_len = 0;
    
    /* Step 1: Build APDU header */
    out_ptr[0] = apdu_in->cla | 0x0C; /* Set secure messaging bit */
    out_ptr[1] = apdu_in->ins;
    out_ptr[2] = apdu_in->p1;
    out_ptr[3] = apdu_in->p2;
    total_len += header_len;
    
    /* Step 2: Encrypt command data if C-ENC enabled */
    encrypted_len = 0;
    if (has_c_enc(session->security_level) && apdu_in->data_len > 0) {
        uint8_t iv[16];
        
        /* Build IV from MAC chaining value */
        memcpy(iv, session->mac_chaining_value, 12);
        iv[12] = 0x00;
        iv[13] = 0x00;
        iv[14] = (session->mac_counter >> 8) & 0xFF;
        iv[15] = session->mac_counter & 0xFF;
        
        ret = scp03_aes_ctr_encrypt(session->session_keys[SCP03_KEY_ENC],
                                    session->key_len,
                                    iv, 16,
                                    apdu_in->data, encrypted_data,
                                    apdu_in->data_len);
        if (ret != 0) {
            return -3;
        }
        
        encrypted_len = apdu_in->data_len;
    }
    
    /* Step 3: Calculate C-MAC if enabled */
    if (has_c_mac(session->security_level)) {
        size_t mac_input_len = 0;
        
        /* Build MAC input data:
         * - MAC chaining value (16 bytes)
         * - APDU header (CLA' | INS | P1 | P2) (4 bytes)
         * - Lc' (if data present)
         * - Data (encrypted if C-ENC)
         * - Le (if present)
         */
        
        /* MAC chaining value */
        memcpy(mac_data + mac_input_len, session->mac_chaining_value, 16);
        mac_input_len += 16;
        
        /* APDU header */
        mac_data[mac_input_len++] = apdu_in->cla | 0x0C;
        mac_data[mac_input_len++] = apdu_in->ins;
        mac_data[mac_input_len++] = apdu_in->p1;
        mac_data[mac_input_len++] = apdu_in->p2;
        
        /* Lc' field (length of data + MAC if present) */
        size_t data_field_len = encrypted_len;
        if (has_c_mac(session->security_level)) {
            data_field_len += SCP03_MAC_SIZE;
        }
        
        if (encrypted_len > 0 || has_c_mac(session->security_level)) {
            if (data_field_len <= 255) {
                mac_data[mac_input_len++] = (uint8_t)data_field_len;
            } else {
                mac_data[mac_input_len++] = 0x00;
                mac_data[mac_input_len++] = (uint8_t)(data_field_len >> 8);
                mac_data[mac_input_len++] = (uint8_t)(data_field_len & 0xFF);
            }
        }
        
        /* Encrypted data (if C-ENC) */
        if (encrypted_len > 0) {
            memcpy(mac_data + mac_input_len, encrypted_data, encrypted_len);
            mac_input_len += encrypted_len;
        }
        
        /* Calculate MAC */
        ret = calculate_mac(session, mac_data, mac_input_len, mac, 0);
        if (ret != 0) {
            return -4;
        }
        
        /* Update chaining value */
        memcpy(session->mac_chaining_value, mac, SCP03_MAC_SIZE);
        memset(session->mac_chaining_value + SCP03_MAC_SIZE, 0, 16 - SCP03_MAC_SIZE);
    }
    
    /* Step 4: Build final APDU */
    total_len = header_len;
    
    /* Add encrypted data length */
    size_t total_data_len = encrypted_len;
    if (has_c_mac(session->security_level)) {
        total_data_len += SCP03_MAC_SIZE;
    }
    
    if (total_data_len > 0) {
        if (total_data_len <= 255) {
            apdu_out[total_len++] = (uint8_t)total_data_len;
        } else {
            apdu_out[total_len++] = 0x00;
            apdu_out[total_len++] = (uint8_t)(total_data_len >> 8);
            apdu_out[total_len++] = (uint8_t)(total_data_len & 0xFF);
        }
        
        /* Add encrypted data */
        if (encrypted_len > 0) {
            memcpy(apdu_out + total_len, encrypted_data, encrypted_len);
            total_len += encrypted_len;
        }
        
        /* Add MAC */
        if (has_c_mac(session->security_level)) {
            memcpy(apdu_out + total_len, mac, SCP03_MAC_SIZE);
            total_len += SCP03_MAC_SIZE;
        }
    }
    
    /* Add Le if specified */
    if (apdu_in->le != 0) {
        apdu_out[total_len++] = apdu_in->le;
    }
    
    *out_len = total_len;
    
    /* Update session state */
    session->mac_counter++;
    session->first_command = 0;
    
    return 0;
}

/*==============================================================================
 * APDU UNWRAP (SECURITY VERIFICATION)
 *============================================================================*/

int scp03_unwrap_apdu(scp03_session_t *session,
                      const uint8_t *resp_in,
                      size_t resp_len,
                      uint8_t *resp_out,
                      size_t *out_len)
{
    uint8_t rmac[SCP03_MAC_SIZE];
    uint8_t calculated_rmac[SCP03_MAC_SIZE];
    uint8_t *encrypted_data = NULL;
    size_t encrypted_len = 0;
    size_t sw_offset;
    int ret;
    
    if (session == NULL || resp_in == NULL || resp_out == NULL || out_len == NULL) {
        return -1;
    }
    
    if (!session->session_open) {
        return -2;
    }
    
    if (resp_len < 2) {
        return -1;
    }
    
    /* Response structure:
     * - Encrypted data (optional, only if R-ENC)
     * - R-MAC (8 bytes, if R-MAC enabled)
     * - SW1 SW2 (2 bytes)
     */
    
    sw_offset = resp_len - 2;
    
    /* Step 1: Extract components */
    if (has_r_mac(session->security_level)) {
        if (resp_len < 2 + SCP03_MAC_SIZE) {
            return -1;
        }
        memcpy(rmac, resp_in + resp_len - 2 - SCP03_MAC_SIZE, SCP03_MAC_SIZE);
        sw_offset = resp_len - 2 - SCP03_MAC_SIZE;
    }
    
    if (has_r_enc(session->security_level) && sw_offset > 0) {
        encrypted_data = (uint8_t *)resp_in;
        encrypted_len = sw_offset;
        if (has_r_mac(session->security_level)) {
            encrypted_len = sw_offset - SCP03_MAC_SIZE;
        }
    }
    
    /* Step 2: Verify R-MAC if enabled */
    if (has_r_mac(session->security_level)) {
        uint8_t mac_data[64];
        size_t mac_data_len = 0;
        
        /* Build MAC input:
         * - Response data (excluding R-MAC and SW)
         * - SW1 SW2
         */
        if (encrypted_len > 0) {
            memcpy(mac_data + mac_data_len, encrypted_data, encrypted_len);
            mac_data_len += encrypted_len;
        }
        
        /* Add SW */
        mac_data[mac_data_len++] = resp_in[resp_len - 2];
        mac_data[mac_data_len++] = resp_in[resp_len - 1];
        
        /* Calculate expected R-MAC */
        ret = calculate_mac(session, mac_data, mac_data_len, calculated_rmac, 1);
        if (ret != 0) {
            return -3;
        }
        
        /* Verify R-MAC */
        if (memcmp(rmac, calculated_rmac, SCP03_MAC_SIZE) != 0) {
            return -3;
        }
    }
    
    /* Step 3: Decrypt response data if R-ENC enabled */
    if (has_r_enc(session->security_level) && encrypted_len > 0) {
        uint8_t iv[16];
        
        /* Build IV for response decryption */
        memcpy(iv, session->mac_chaining_value, 12);
        iv[12] = 0x00;
        iv[13] = 0x00;
        iv[14] = (session->mac_counter >> 8) & 0xFF;
        iv[15] = session->mac_counter & 0xFF;
        
        ret = scp03_aes_ctr_decrypt(session->session_keys[SCP03_KEY_ENC],
                                    session->key_len,
                                    iv, 16,
                                    encrypted_data, resp_out,
                                    encrypted_len);
        if (ret != 0) {
            return -4;
        }
        
        *out_len = encrypted_len;
    } else {
        *out_len = 0;
    }
    
    /* Append SW1 SW2 */
    if (*out_len + 2 <= SCP03_MAX_APDU_SIZE) {
        resp_out[*out_len] = resp_in[resp_len - 2];
        resp_out[*out_len + 1] = resp_in[resp_len - 1];
        *out_len += 2;
    }
    
    return 0;
}

/*==============================================================================
 * SESSION INITIALIZATION AND CLEANUP
 *============================================================================*/

void scp03_session_init(scp03_session_t *session)
{
    if (session == NULL) {
        return;
    }
    
    memset(session, 0, sizeof(scp03_session_t));
    session->mac_counter = 1;
    session->first_command = 1;
}

void scp03_session_clear(scp03_session_t *session)
{
    if (session == NULL) {
        return;
    }
    
    /* Securely clear session keys */
    for (int i = 0; i < SCP03_KEY_COUNT; i++) {
        memset(session->session_keys[i], 0, SCP03_KEY_SIZE_32);
    }
    
    /* Clear all sensitive data */
    memset(session->mac_chaining_value, 0, SCP03_ICV_SIZE);
    memset(session->host_challenge, 0, SCP03_CHALLENGE_SIZE);
    memset(session->card_challenge, 0, SCP03_CHALLENGE_SIZE);
    memset(session->session_id, 0, SCP03_CHALLENGE_SIZE);
    memset(session->icv, 0, SCP03_ICV_SIZE);
    
    /* Reset flags */
    session->session_open = 0;
    session->first_command = 0;
}
