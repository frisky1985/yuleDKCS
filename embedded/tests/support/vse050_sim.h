/**
 * @file vse050_sim.h
 * @brief Virtual SE050 (vSE050) — asynchronous T=1' + SCP03 device model.
 *
 * The model implements the card side of the NXP T=1' over I2C block protocol
 * (AN12413) and the SCP03 handshake (INITIALIZE UPDATE / EXTERNAL
 * AUTHENTICATE), so the real host-side stack (se050_t1oi2c.c +
 * se050_scp03.c) can be exercised without a physical secure element.
 *
 * Asynchronous behaviour:
 *   - Every APDU is "submitted" and only becomes readable after the virtual
 *     clock has advanced past the configured processing latency. Reads made
 *     before that return -1 (NACK / clock-stretch), which the transport
 *     retries. This reproduces BUSY semantics.
 *   - The card may emit S(WTX request) before the actual response and waits
 *     for the host's S(WTX response) — exercising the WTX path.
 *
 * Boundary: this model validates protocol/state-machine behaviour only. It is
 * NOT evidence of secure key storage, side-channel resistance, or production
 * readiness.
 */

#ifndef VSE050_SIM_H
#define VSE050_SIM_H

#include <stdint.h>

#include "se050_t1oi2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Card-side SCP03 state */
typedef enum {
    VSE050_STATE_RESET        = 0, /**< No INITIALIZE UPDATE yet */
    VSE050_STATE_AUTH_PENDING = 1, /**< INITIALIZE UPDATE done */
    VSE050_STATE_OPEN         = 2, /**< EXTERNAL AUTHENTICATE accepted */
    VSE050_STATE_LOCKED       = 3  /**< Too many failed authentications */
} vse050_state_e;

/** Device configuration */
typedef struct {
    uint8_t  enc_key[16];          /**< Static K-ENC (default: all zero) */
    uint8_t  mac_key[16];          /**< Static K-MAC (default: all zero) */
    uint8_t  rmac_key[16];         /**< Static K-RMAC (default: all zero) */
    uint8_t  key_version;          /**< Key version returned to the host */
    uint8_t  seq_counter[2];       /**< SCP03 sequence counter (big-endian) */
    uint8_t  key_divers_data[10];  /**< Key diversification data */
    uint8_t  card_challenge[8];    /**< Base card challenge */

    uint32_t latency_us;           /**< APDU processing latency (virtual us) */
    uint32_t poll_step_us;         /**< Virtual time per failed poll (us) */

    uint8_t  wtx_on_init_update;   /**< 1: emit S(WTX) before INIT UPDATE reply */
    uint8_t  reject_ext_auth;      /**< 1: EXTERNAL AUTH always fails */
    uint8_t  sign_disabled;        /**< 1: ECDSA sign returns SW=6982 */
    uint8_t  corrupt_rmac;         /**< One-shot: corrupt the response R-MAC */
    uint8_t  corrupt_ciphertext;   /**< One-shot: flip a command ciphertext byte */
    uint8_t  corrupt_next_response;/**< One-shot: corrupt the next block CRC */
    uint8_t  drop_next_response;   /**< One-shot: answer nothing (timeout) */
} vse050_config_t;

/** Populate @p cfg with defaults (zero keys, 3 ms latency). */
void vse050_sim_default_config(vse050_config_t *cfg);

/**
 * @brief Reset the singleton device and install its I/O hooks.
 *
 * A NULL @p cfg applies the defaults.
 */
void vse050_sim_init(const vse050_config_t *cfg);

/** Install the virtual device as the T=1' transport backend. */
void vse050_sim_bind(void);

/** I/O hooks (write/read/delay_us) backed by the virtual device. */
const se050_t1_io_t *vse050_sim_io(void);

/** Advance the virtual clock by @p us (used as the delay_us hook). */
void vse050_sim_delay_us(uint32_t us);

/* ── Observability ────────────────────────────────────────── */
vse050_state_e vse050_sim_state(void);
uint32_t vse050_sim_now_us(void);
uint32_t vse050_sim_busy_polls(void);   /**< Polls rejected while busy */
uint32_t vse050_sim_nack_polls(void);   /**< Polls with no block at all */
uint32_t vse050_sim_wtx_count(void);
uint32_t vse050_sim_crc_errors(void);   /**< Injected CRC faults */
uint32_t vse050_sim_auth_failures(void);
uint32_t vse050_sim_sign_count(void);    /**< Accepted ECDSA sign commands */
uint32_t vse050_sim_encrypted_cmds(void);/**< Sign commands received encrypted */
uint16_t vse050_sim_last_cipher_len(void);/**< Ciphertext length of the last encrypted command */
uint16_t vse050_sim_apdu_count(void);

/** AES-128 block primitives, exposed for known-answer tests. */
void vse050_sim_aes128_decrypt_block(const uint8_t key[16],
                                     const uint8_t in[16], uint8_t out[16]);

/* ── Crypto helpers (exposed for known-answer tests) ──────── */
void vse050_sim_aes128_encrypt_block(const uint8_t key[16],
                                     const uint8_t in[16], uint8_t out[16]);
void vse050_sim_aes_cmac(const uint8_t key[16],
                         const uint8_t *msg, uint16_t msg_len,
                         uint8_t mac[16]);

#ifdef __cplusplus
}
#endif

#endif /* VSE050_SIM_H */
