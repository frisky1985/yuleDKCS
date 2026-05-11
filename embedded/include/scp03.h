/**
 * @file scp03.h
 * @brief SCP03 (Secure Channel Protocol '03') Header File
 *
 * SCP03 is a GlobalPlatform standard secure channel protocol.
 * This file defines data structures, enums, constants, and API functions
 * for SCP03 secure channel operations.
 *
 * @copyright yuleDKCS
 * @version 1.0
 */

#ifndef SCP03_H
#define SCP03_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * CONSTANTS AND MACROS
 *============================================================================*/

/** Key size in bytes - supports AES-128/192/256 */
#define SCP03_KEY_SIZE_16   16  /**< AES-128 key size */
#define SCP03_KEY_SIZE_24   24  /**< AES-192 key size */
#define SCP03_KEY_SIZE_32   32  /**< AES-256 key size */

/** Default key size (AES-128) */
#define SCP03_KEY_SIZE      SCP03_KEY_SIZE_16

/** MAC size in bytes (CMAC output truncated to 8 bytes) */
#define SCP03_MAC_SIZE      8

/** ICV (Initial Chaining Value) size in bytes */
#define SCP03_ICV_SIZE      16

/** Challenge size in bytes */
#define SCP03_CHALLENGE_SIZE 8

/** Maximum APDU data size */
#define SCP03_MAX_APDU_SIZE 65536

/** Session key count */
#define SCP03_KEY_COUNT     3

/*==============================================================================
 * SECURITY LEVEL ENUMERATIONS (i-values)
 *============================================================================*/

/**
 * @brief SCP03 Security Levels
 *
 * The 'i' parameter defines the security level for the secure channel.
 */
typedef enum {
    SCP03_I_00 = 0x00,  /**< No security - plaintext communication */
    SCP03_I_04 = 0x04,  /**< C-MAC: Command MAC verification */
    SCP03_I_0C = 0x0C,  /**< C-MAC + C-ENC: Command MAC + Command encryption */
    SCP03_I_14 = 0x14,  /**< C-MAC + C-ENC + R-MAC + R-ENC: Full security */
} scp03_security_level_t;

/*==============================================================================
 * KEY TYPE ENUMERATIONS
 *============================================================================*/

/**
 * @brief SCP03 Key Set Types
 *
 * Defines the method used for key derivation and management.
 */
typedef enum {
    SCP03_KEY_STATIC       = 0x00,  /**< Static keys - fixed keys */
    SCP03_KEY_DERIVED      = 0x01,  /**< Password-based key derivation */
    SCP03_KEY_CALCULATED   = 0x02,  /**< Calculated/derived keys from master key */
} scp03_key_type_t;

/*==============================================================================
 * KEY IDENTIFIERS
 *============================================================================*/

/**
 * @brief SCP03 Session Key Identifiers
 *
 * SCP03 uses three session keys derived from static keys.
 */
typedef enum {
    SCP03_KEY_ENC = 0,  /**< Encryption key for C-ENC/R-ENC */
    SCP03_KEY_MAC = 1,  /**< MAC key for C-MAC/R-MAC */
    SCP03_KEY_DEK = 2,  /**< Data encryption key for sensitive data */
} scp03_key_id_t;

/*==============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * @brief SCP03 Static Key Set
 *
 * Contains the master/static keys used for session key derivation.
 */
typedef struct {
    uint8_t enc_key[SCP03_KEY_SIZE_32]; /**< Static ENC key */
    uint8_t mac_key[SCP03_KEY_SIZE_32]; /**< Static MAC key */
    uint8_t dek_key[SCP03_KEY_SIZE_32]; /**< Static DEK key */
    size_t  key_len;                    /**< Actual key length (16, 24, or 32) */
} scp03_static_keys_t;

/**
 * @brief SCP03 Session Configuration
 *
 * Configuration parameters for opening a secure session.
 */
typedef struct {
    scp03_static_keys_t static_keys;    /**< Static keys */
    uint8_t             host_challenge[SCP03_CHALLENGE_SIZE]; /**< Host challenge */
    scp03_key_type_t    key_type;       /**< Key derivation type */
    scp03_security_level_t security_level; /**< Requested security level */
} scp03_config_t;

/**
 * @brief SCP03 Session Context
 *
 * Maintains the state of an active SCP03 secure session.
 */
typedef struct {
    /* Session Keys (derived from static keys) */
    uint8_t session_keys[SCP03_KEY_COUNT][SCP03_KEY_SIZE_32];
    size_t  key_len;                    /**< Session key length */

    /* Sequence Counter */
    uint8_t mac_chaining_value[SCP03_ICV_SIZE]; /**< MAC chaining value */

    /* Security Level */
    scp03_security_level_t security_level; /**< Current security level */

    /* Session State */
    uint8_t host_challenge[SCP03_CHALLENGE_SIZE]; /**< Host challenge */
    uint8_t card_challenge[SCP03_CHALLENGE_SIZE]; /**< Card challenge */
    uint8_t session_id[SCP03_CHALLENGE_SIZE];     /**< Session identifier */

    /* Cryptographic State */
    uint8_t icv[SCP03_ICV_SIZE];        /**< Initial chaining value for encryption */
    uint16_t mac_counter;               /**< MAC counter for R-MAC */

    /* Flags */
    uint8_t session_open : 1;           /**< Session is open */
    uint8_t first_command : 1;          /**< First command flag */
    uint8_t reserved : 6;
} scp03_session_t;

/**
 * @brief SCP03 APDU Structure
 *
 * Represents a command or response APDU.
 */
typedef struct {
    uint8_t  cla;                       /**< Class byte */
    uint8_t  ins;                       /**< Instruction byte */
    uint8_t  p1;                        /**< Parameter 1 */
    uint8_t  p2;                        /**< Parameter 2 */
    uint8_t *data;                      /**< Data buffer */
    size_t   data_len;                  /**< Data length */
    uint8_t  le;                        /**< Expected response length */
} scp03_apdu_t;

/*==============================================================================
 * API FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Open an SCP03 secure session
 *
 * Performs the INITIALIZE UPDATE and EXTERNAL AUTHENTICATE commands
 * to establish a secure channel with the card.
 *
 * @param session       Pointer to session context (output)
 * @param config        Pointer to session configuration
 *
 * @return 0 on success, negative error code on failure
 * @retval  0   Success
 * @retval -1   Invalid parameter
 * @retval -2   Key derivation failed
 * @retval -3   Authentication failed
 * @retval -4   Communication error
 */
int scp03_open_session(scp03_session_t *session, const scp03_config_t *config);

/**
 * @brief Close an SCP03 secure session
 *
 * Clears session keys and resets the session state.
 *
 * @param session       Pointer to session context
 *
 * @return 0 on success, negative error code on failure
 * @retval  0   Success
 * @retval -1   Invalid parameter
 */
int scp03_close_session(scp03_session_t *session);

/**
 * @brief Wrap (encrypt and/or MAC) an APDU command
 *
 * Applies security transformations to the command APDU based on
 * the current security level.
 *
 * @param session       Pointer to active session context
 * @param apdu_in       Input APDU (plaintext)
 * @param apdu_out      Output buffer for wrapped APDU
 * @param out_len       Output buffer size (input), actual length (output)
 *
 * @return 0 on success, negative error code on failure
 * @retval  0   Success
 * @retval -1   Invalid parameter
 * @retval -2   Session not open
 * @retval -3   Encryption failed
 * @retval -4   MAC calculation failed
 * @retval -5   Buffer too small
 */
int scp03_wrap_apdu(scp03_session_t *session,
                    const scp03_apdu_t *apdu_in,
                    uint8_t *apdu_out,
                    size_t *out_len);

/**
 * @brief Unwrap (decrypt and/or verify MAC) an APDU response
 *
 * Verifies and decrypts the response APDU based on the current
 * security level.
 *
 * @param session       Pointer to active session context
 * @param resp_in       Input response (wrapped)
 * @param resp_len      Input response length
 * @param resp_out      Output buffer for unwrapped response
 * @param out_len       Output buffer size (input), actual length (output)
 *
 * @return 0 on success, negative error code on failure
 * @retval  0   Success
 * @retval -1   Invalid parameter
 * @retval -2   Session not open
 * @retval -3   MAC verification failed
 * @retval -4   Decryption failed
 * @retval -5   Buffer too small
 */
int scp03_unwrap_apdu(scp03_session_t *session,
                      const uint8_t *resp_in,
                      size_t resp_len,
                      uint8_t *resp_out,
                      size_t *out_len);

/**
 * @brief Initialize an SCP03 session context
 *
 * Sets default values and clears sensitive data.
 *
 * @param session       Pointer to session context
 */
void scp03_session_init(scp03_session_t *session);

/**
 * @brief Clear sensitive data from session context
 *
 * Securely wipes session keys and cryptographic state.
 *
 * @param session       Pointer to session context
 */
void scp03_session_clear(scp03_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* SCP03_H */
