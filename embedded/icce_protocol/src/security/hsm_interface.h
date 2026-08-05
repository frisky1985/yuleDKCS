/**
 * @file hsm_interface.h
 * @module EMB-BSW-HSM (ASPICE SWE.4)
 * @brief Hardware Security Module Interface — Secure key storage & crypto ops
 * Layer: BSW (Basic Software Layer) - Hardware Security
 */

#ifndef HSM_INTERFACE_H
#define HSM_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

/* HSM Result Codes */
#define HSM_SUCCESS 0

/* HSM Key Handle */
typedef uint32_t hsm_key_handle_t;

/* HSM API */
int32_t hsm_init(void);
int32_t hsm_generate_random(uint8_t *buf, uint16_t len);
int32_t hsm_store_key(uint32_t key_id, const uint8_t *key_data, uint16_t key_len, hsm_key_handle_t *handle);
int32_t hsm_load_key(uint32_t key_id, uint8_t *key_data, uint16_t *key_len);
int32_t hsm_generate_ecdh_keypair(uint8_t *private_key, uint8_t *public_key);
int32_t hsm_ecdh_compute_shared(const uint8_t *private_key, const uint8_t *peer_public, uint8_t *shared_secret);
int32_t hsm_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash, uint8_t *signature, uint16_t *sig_len);
int32_t hsm_ecdsa_verify(const uint8_t *public_key, const uint8_t *hash, const uint8_t *signature, uint16_t sig_len);

#endif /* HSM_INTERFACE_H */
