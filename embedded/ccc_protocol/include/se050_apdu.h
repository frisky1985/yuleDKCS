/**
 * @file se050_apdu.h
 * @module EMB-BSW-SE050-APDU (ASPICE SWE.4)
 * @brief SE050 command layer — ECDSA signature generation over SCP03.
 *
 * Layer: BSW (Basic Software Layer) — above se050_scp03.c, below security.c
 *
 * Transport stack:
 *   security.c / sec_sign
 *     └─ se050_apdu_sign()            (this module: SE050 command semantics)
 *          └─ se050_scp03_apdu_enc()  (C-MAC + AES-CBC encryption + R-MAC)
 *               └─ se050_t1oi2c.c     (T=1' over I2C)
 *
 * APDU layout (EC-DSA signature generation, AN12413 command set):
 *   CLA  = 0x8C (0x80 + C-MAC 0x04 + encryption 0x08), applied by SCP03
 *   INS  = 0x62
 *   P1   = 0x00
 *   P2   = 0x00
 *   Data = KeyObjectID (4, big-endian) || Hash (32)
 *          — padded to 48 bytes and AES-128-CBC encrypted under S-ENC
 *   Resp = r (32) || s (32) — encrypted, then authenticated by the R-MAC
 *          before decryption
 *
 * The SE050 keeps the private key inside the secure element; only the hash
 * crosses the I2C bus (encrypted) and only the signature comes back.
 */

#ifndef SE050_APDU_H
#define SE050_APDU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "se050_scp03.h"

/** APDU class without secure messaging bits */
#define SE050_APDU_CLA              0x80U

/** EC-DSA signature generation instruction */
#define SE050_APDU_INS_ECDSA_SIGN   0x62U

/** Hash input size (SHA-256) */
#define SE050_APDU_HASH_SIZE        32U

/** Signature size (r || s, 32 + 32) */
#define SE050_APDU_SIGNATURE_SIZE   64U

/** Sign command data field: key id (4) + hash (32) */
#define SE050_APDU_SIGN_INPUT_SIZE  (4U + SE050_APDU_HASH_SIZE)

/** Status codes */
#define SE050_APDU_OK                0
#define SE050_APDU_ERR_PARAM        (-1)  /**< NULL / invalid argument */
#define SE050_APDU_ERR_CHANNEL      (-2)  /**< SCP03 channel not open */
#define SE050_APDU_ERR_TRANSPORT    (-3)  /**< I2C / T=1' / SCP03 failure */
#define SE050_APDU_ERR_SW           (-4)  /**< Non-0x9000 status word */
#define SE050_APDU_ERR_INTEGRITY    (-5)  /**< R-MAC verification failed */
#define SE050_APDU_ERR_RESPONSE     (-6)  /**< Malformed response length */

/**
 * @brief Request an ECDSA signature for @p hash from the SE050.
 *
 * The command is sent over the established SCP03 secure channel; the
 * response R-MAC is verified and stripped by se050_scp03_apdu().
 * On any integrity failure the secure channel is closed.
 *
 * @param session   [in/out] Open SCP03 session
 * @param i2c_addr  SE050 I2C slave address (0x48)
 * @param key_id    SE050 key object id holding the private key
 * @param hash      [in] 32-byte message digest
 * @param signature [out] 64-byte r || s
 * @return SE050_APDU_OK on success, negative SE050_APDU_ERR_* on failure
 */
int se050_apdu_sign(scp03_session_t *session, uint8_t i2c_addr, uint32_t key_id,
                    const uint8_t hash[SE050_APDU_HASH_SIZE],
                    uint8_t signature[SE050_APDU_SIGNATURE_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SE050_APDU_H */
