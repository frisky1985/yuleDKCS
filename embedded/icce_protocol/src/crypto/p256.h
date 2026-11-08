/**
 * @file p256.h
 * @brief ECDSA P-256 (secp256r1) — software verify/sign.
 *
 * Reuses the 256-bit field arithmetic in crypto_utils.c (SM2 and P-256 share
 * the same prime p and coefficient a = p-3), adding P-256's b, G and n.
 *
 * Boundary: the field arithmetic is not constant-time. Use SE050 for
 * production signing; this module exists for host-side verification and for
 * development fallback.
 */

#ifndef CRYPTO_P256_H
#define CRYPTO_P256_H

#include "crypto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Verify an ECDSA P-256 / SHA-256 signature.
 *
 * @param pub  Uncompressed public key X||Y (64 bytes).
 * @param hash SHA-256 digest of the message (32 bytes).
 * @param sig  Signature r||s (64 bytes).
 * @return CRYPTO_SUCCESS or CRYPTO_ERR_*.
 */
int crypto_p256_verify(const uint8_t pub[64], const uint8_t hash[32],
                       const uint8_t sig[64]);

/**
 * @brief Generate an ECDSA P-256 / SHA-256 signature.
 *
 * Development/test only: k is drawn from crypto_random_bytes(); the
 * implementation is not constant-time and must not replace the SE050 path.
 *
 * @return CRYPTO_SUCCESS or CRYPTO_ERR_*.
 */
int crypto_p256_sign(const uint8_t priv[32], const uint8_t hash[32],
                     uint8_t sig[64]);

/**
 * @brief Check that a 64-byte public key is a valid point on P-256.
 */
int crypto_p256_pubkey_valid(const uint8_t pub[64]);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_P256_H */
