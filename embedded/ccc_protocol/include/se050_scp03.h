/**
 * @file se050_scp03.h
 * @module EMB-BSW-SE050-SCP03 (ASPICE SWE.4)
 * @brief SE050 SCP03 Secure Channel Protocol — Full Implemetation
 * @version 1.0
 * @date 2026-07-16
 *
 * Layer: BSW (Basic Software Layer) — Hardware Security Driver
 *
 * Implements GlobalPlatform SCP03 (Secure Channel Protocol 03) over
 * NXP SE050 secure element via I2C / ISO 7816-4 APDU transport.
 *
 * Features:
 *   - SCP03 session establishment (INITIALIZE UPDATE + EXTERNAL AUTHENTICATE)
 *   - AES-128 session key derivation (S-ENC, S-MAC, S-RMAC)
 *   - Secure APDU messaging with C-MAC / R-MAC and optional encryption
 *   - Session key rotation (REPLACE KEYS / SYNC)
 *   - Default transport key support (for development) and personalized keys
 *
 * Reference:
 *   - GlobalPlatform Card Specification v2.3.1 (GPC_SPE_034)
 *   - GlobalPlatform SCP03 (GPC_SPE_091)
 *   - NXP AN12413: SE050 SCP03 Implementation Guide
 *   - NXP SE05x API Reference Manual
 *   - ISO/IEC 7816-4: Interindustry Commands
 *
 * Security Note (MISRA):
 *   All cryptographic material MUST be zeroed via se050_scp03_secure_zero()
 *   after use. Never leave session keys on the stack after channel close.
 *
 * ABI Compatibility:
 *   This module does NOT modify the existing `sec_scp03_open/close/encrypt/decrypt`
 *   signature in ccc_digital_key.h. It provides the underlying implementation.
 */

#ifndef SE050_SCP03_H
#define SE050_SCP03_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ========================================================================
 *  SCP03 Protocol Constants
 * ======================================================================== */

/** SCP03 key sizes */
#define SCP03_KEY_SIZE              16      /**< AES-128 key size (bytes) */
#define SCP03_BLOCK_SIZE            16      /**< AES block size (bytes) */
#define SCP03_CMAC_SIZE              8      /**< Full-length C-MAC (bytes) */
#define SCP03_CMAC_TRUNC             8      /**< Truncated C-MAC for APDU (bytes) */
#define SCP03_CHALLENGE_SIZE         8      /**< Host/card challenge (bytes) */
#define SCP03_SEQ_COUNTER_SIZE       2      /**< Sequence counter (bytes) */
#define SCP03_KEY_DIVERS_SIZE       10      /**< Key diversification data (bytes) */
#define SCP03_CRYPTOGRAM_SIZE        8      /**< Cryptogram (bytes) */
#define SCP03_MAX_APDU_DATA        255      /**< Max APDU data field (bytes) */
#define SCP03_MAX_APDU_RESP        256      /**< Max APDU response data (bytes) */
#define SCP03_CARD_IV_SIZE          16      /**< Card-side IV for Secure Messaging (bytes) */

/** SCP03 key derivation counters */
#define SCP03_DERIVE_S_ENC           0x01U  /**< S-ENC derivation counter */
#define SCP03_DERIVE_S_MAC           0x02U  /**< S-MAC derivation counter */
#define SCP03_DERIVE_S_RMAC          0x03U  /**< S-RMAC derivation counter */

/** SCP03 APDU CLA byte with Secure Messaging */
#define SCP03_CLA_NO_SM              0x80U  /**< CLA without Secure Messaging */
#define SCP03_CLA_CMAC               0x84U  /**< CLA with C-MAC only */
#define SCP03_CLA_CMAC_ENC           0x8CU  /**< CLA with C-MAC + Encryption */

/** SCP03 instruction codes */
#define SCP03_INS_INIT_UPDATE        0x50U  /**< INITIALIZE UPDATE */
#define SCP03_INS_EXT_AUTH           0x82U  /**< EXTERNAL AUTHENTICATE */
#define SCP03_INS_INT_AUTH           0x88U  /**< INTERNAL AUTHENTICATE */
#define SCP03_INS_GET_CHALLENGE      0x84U  /**< GET CHALLENGE */
#define SCP03_INS_REPLACE_KEYS       0xD2U  /**< REPLACE KEYS (key rotation) */
#define SCP03_INS_PUT_KEY            0xD8U  /**< PUT KEY */

/** SCP03 status word */
#define SCP03_SW_OK                  0x9000U       /**< Command successful */
#define SCP03_SW_SECURITY            0x6700U       /**< Security status not satisfied */
#define SCP03_SW_WRONG_DATA          0x6A80U       /**< Incorrect parameters */
#define SCP03_SW_VERIFY_FAILED       0x6300U       /**< Security verification failed */
#define SCP03_SW_KEY_NOT_FOUND       0x6A88U       /**< Referenced data not found */
#define SCP03_SW_CHANNEL_ACTIVE      0x6985U       /**< Conditions not satisfied / active */

/** SCP03 status */
#define SCP03_OK                     0            /**< Success */
#define SCP03_ERR_NULL              (-1)           /**< Null pointer error */
#define SCP03_ERR_PARAM             (-2)           /**< Invalid parameter */
#define SCP03_ERR_APDU              (-3)           /**< APDU communication error */
#define SCP03_ERR_SW                (-4)           /**< Unexpected status word */
#define SCP03_ERR_CRYPTOGRAM        (-5)           /**< Cryptogram mismatch */
#define SCP03_ERR_KEY_DERIVE        (-6)           /**< Key derivation failed */
#define SCP03_ERR_CMAC              (-7)           /**< CMAC verification failed */
#define SCP03_ERR_NOT_INIT          (-8)           /**< Module not initialized */
#define SCP03_ERR_CHANNEL           (-9)           /**< Channel not established */
#define SCP03_ERR_NO_MEM            (-10)          /**< Memory allocation failure */
#define SCP03_ERR_HW                (-11)          /**< Hardware / I2C error */

/* ========================================================================
 *  SCP03 Channel State
 * ======================================================================== */

/** SCP03 channel state machine */
typedef enum {
    SCP03_STATE_CLOSED      = 0,
    SCP03_STATE_INIT        = 1,  /**< sec_init() called */
    SCP03_STATE_AUTH_PENDING = 2, /**< INIT UPDATE done, EXTERNAL AUTH pending */
    SCP03_STATE_OPEN         = 3, /**< Full SCP03 session active */
    SCP03_STATE_ERROR        = 4
} scp03_state_e;

/** SCP03 session context (opaque to callers) */
typedef struct {
    /** Session state */
    scp03_state_e   state;

    /** Static (provisioned) keys */
    uint8_t         static_enc_key[SCP03_KEY_SIZE];   /**< K_ENC */
    uint8_t         static_mac_key[SCP03_KEY_SIZE];   /**< K_MAC */
    uint8_t         static_rmac_key[SCP03_KEY_SIZE];  /**< K_RMAC */

    /** Session keys (derived) */
    uint8_t         s_enc[SCP03_KEY_SIZE];     /**< Session ENC key */
    uint8_t         s_mac[SCP03_KEY_SIZE];     /**< Session MAC key */
    uint8_t         s_rmac[SCP03_KEY_SIZE];    /**< Session R-MAC key */

    /** Session parameters */
    uint8_t         host_challenge[SCP03_CHALLENGE_SIZE];
    uint8_t         card_challenge[SCP03_CHALLENGE_SIZE];
    uint8_t         card_cryptogram[SCP03_CRYPTOGRAM_SIZE];
    uint8_t         host_cryptogram[SCP03_CRYPTOGRAM_SIZE];
    uint8_t         seq_counter[SCP03_SEQ_COUNTER_SIZE];
    uint8_t         key_divers_data[SCP03_KEY_DIVERS_SIZE];

    /** Secure messaging state */
    uint8_t         cmac_iv[SCP03_BLOCK_SIZE];     /**< C-MAC chaining IV */
    uint8_t         rmac_iv[SCP03_BLOCK_SIZE];     /**< R-MAC chaining IV */
    uint32_t        scp_cmd_count;                  /**< Command count for MAC chaining */
    uint8_t         key_version;                    /**< Key version number */
} scp03_session_t;

/* ========================================================================
 *  Public API — SCP03 Lifecycle
 * ======================================================================== */

/**
 * @brief Initialize the SCP03 session context with static keys.
 *
 * In production, keys MUST be loaded from OTP / eFuse via SE050.
 * The default transport key (all zeros) is ONLY for development.
 *
 * @param session   [out] Session context to initialize
 * @return SCP03_OK on success, negative on error
 */
int se050_scp03_init(scp03_session_t *session);

/**
 * @brief Deinitialize SCP03, securely zeroing all key material.
 *
 * @param session   [in/out] Session context; zeroed on return
 */
void se050_scp03_deinit(scp03_session_t *session);

/**
 * @brief Execute full SCP03 session establishment:
 *        INITIALIZE UPDATE → Key Derivation → EXTERNAL AUTHENTICATE.
 *
 * @param session   [in/out] Session context (state will be SCP03_STATE_OPEN on success)
 * @param i2c_addr  SE050 I2C slave address (typically 0x48)
 * @return SCP03_OK on success, negative on error
 */
int se050_scp03_open_session(scp03_session_t *session, uint8_t i2c_addr);

/**
 * @brief Close SCP03 session and zero all keys.
 *
 * @param session   [in/out] Session context; zeroed on return
 */
void se050_scp03_close_session(scp03_session_t *session);

/**
 * @brief Provision personalized static SCP03 keys (replaces defaults).
 *
 * @param session   [in/out] Session context
 * @param enc_key   [in] K_ENC (16 bytes), NULL to keep current
 * @param mac_key   [in] K_MAC (16 bytes), NULL to keep current
 * @param rmac_key  [in] K_RMAC (16 bytes), NULL to keep current
 * @return SCP03_OK on success
 */
int se050_scp03_provision_keys(scp03_session_t *session,
                                const uint8_t enc_key[SCP03_KEY_SIZE],
                                const uint8_t mac_key[SCP03_KEY_SIZE],
                                const uint8_t rmac_key[SCP03_KEY_SIZE]);

/* ========================================================================
 *  Public API — Secure APDU Communication
 * ======================================================================== */

/**
 * @brief Send an APDU through the established SCP03 secure channel.
 *
 * If session is established, appends C-MAC and optionally encrypts
 * the data field, then sends the secured APDU over I2C.
 *
 * @param session   [in/out] Active SCP03 session
 * @param i2c_addr  SE050 I2C slave address
 * @param cla       APDU CLA byte (caller provides, SCP03 MAC/ENC flags auto-set)
 * @param ins       APDU INS byte
 * @param p1        APDU P1 parameter
 * @param p2        APDU P2 parameter
 * @param data      [in] APDU data field (may be NULL if data_len == 0)
 * @param data_len  APDU data field length
 * @param resp      [out] APDU response data buffer
 * @param resp_len  [in/out] Input: buffer capacity; Output: actual response length
 * @return SCP03_OK on success, negative on error
 */
int se050_scp03_apdu(scp03_session_t *session, uint8_t i2c_addr,
                      uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                      const uint8_t *data, uint16_t data_len,
                      uint8_t *resp, uint16_t *resp_len);

/**
 * @brief Send a plain (unsecured) APDU before SCP03 is established.
 *
 * Used for INITIALIZE UPDATE and EXTERNAL AUTHENTICATE.
 *
 * @param i2c_addr  SE050 I2C slave address
 * @param cla       CLA byte
 * @param ins       INS byte
 * @param p1        P1 parameter
 * @param p2        P2 parameter
 * @param data      [in] APDU data (may be NULL)
 * @param data_len  Data length
 * @param resp      [out] Response buffer
 * @param resp_len  [in/out] Buffer size / response length
 * @return SCP03_OK on success, negative on error
 */
int se050_scp03_apdu_plain(uint8_t i2c_addr,
                            uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                            const uint8_t *data, uint16_t data_len,
                            uint8_t *resp, uint16_t *resp_len);

/* ========================================================================
 *  Public API — Key Rotation
 * ======================================================================== */

/**
 * @brief Rotate SCP03 session keys to a new sequence counter.
 *
 * Performs REPLACE KEYS to trigger a new INITIALIZE UPDATE flow,
 * or directly initiates a new session with incremented seq counter.
 *
 * @param session   [in/out] Current session
 * @param i2c_addr  SE050 I2C slave address
 * @return SCP03_OK on success
 */
int se050_scp03_rotate_keys(scp03_session_t *session, uint8_t i2c_addr);

/**
 * @brief Query whether the session is currently open.
 *
 * @param session   [in] Session context
 * @return true if SCP03 session is established
 */
bool se050_scp03_is_open(const scp03_session_t *session);

/* ========================================================================
 *  Utility Functions
 * ======================================================================== */

/**
 * @brief Securely zero memory (volatile pointer to prevent compiler elision).
 *
 * @param ptr   [in/out] Pointer to memory
 * @param len   Number of bytes to zero
 */
void se050_scp03_secure_zero(void *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SE050_SCP03_H */
