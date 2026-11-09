/**
 * @file secure_boot.h
 * @brief Software secure-boot chain: signed image manifest + anti-rollback.
 *
 * The manifest is a fixed-size, explicitly serialised little-endian record so
 * there is no struct-padding ambiguity between the signing tool, the tests and
 * the firmware:
 *
 *   offset  size  field
 *   0       4     magic "YSBM"
 *   4       1     format_version (1)
 *   5       1     key_type (1 = P-256, 2 = SM2)
 *   6       4     image_version      (u32 LE)
 *   10      4     anti_rollback      (u32 LE)
 *   14      4     image_len          (u32 LE)
 *   18      32    image_sha256       (SHA-256 of the image bytes)
 *   50      16    key_id
 *   66      64    signature (r || s)
 *   --------------------------------------------------------------
 *   total   130 bytes; signature covers bytes [0, 66).
 *
 * Signature digest is always SHA-256 over the 66 signed bytes:
 *   - P-256: crypto_p256_sign()/crypto_p256_verify().
 *   - SM2  : sm2_sign_hash()/sm2_verify_hash() over the same 32-byte digest
 *            (no ZA/user-id pre-hash). This keeps the on-wire format simple;
 *            a full GB/T 32918.2 ZA-based profile is a future extension.
 *
 * All verification paths are fail-closed and return one of the sb_status_t
 * codes below.
 */

#ifndef SECURE_BOOT_H
#define SECURE_BOOT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Manifest constants ───────────────────────────────────── */
#define SB_KEY_TYPE_P256          1U
#define SB_KEY_TYPE_SM2           2U
#define SB_FORMAT_VERSION         1U

#define SB_SHA256_SIZE            32U
#define SB_KEY_ID_SIZE            16U
#define SB_SIGNATURE_SIZE         64U

#define SB_MANIFEST_SIGNED_SIZE   66U
#define SB_MANIFEST_SIZE          130U

/* ── Device lifecycle (debug-port policy) ─────────────────── */
#define SB_LIFECYCLE_DEV          1U
#define SB_LIFECYCLE_PROD         2U
#define SB_LIFECYCLE_RMA          3U

/* ── Status codes ─────────────────────────────────────────── */
typedef enum {
    SB_OK                = 0,
    SB_ERR_BAD_PARAM     = -1,
    SB_ERR_BAD_MAGIC     = -2,
    SB_ERR_BAD_FORMAT    = -3,
    SB_ERR_BAD_SIGNATURE = -4,
    SB_ERR_BAD_HASH      = -5,
    SB_ERR_ROLLBACK      = -6,
    SB_ERR_KEY_MISMATCH  = -7,
    SB_ERR_IO            = -8,
    SB_ERR_CRYPTO        = -9
} sb_status_t;

/* ── Parsed manifest ──────────────────────────────────────── */
typedef struct {
    uint8_t  magic[4];
    uint8_t  format_version;
    uint8_t  key_type;
    uint32_t image_version;
    uint32_t anti_rollback;
    uint32_t image_len;
    uint8_t  image_sha256[SB_SHA256_SIZE];
    uint8_t  key_id[SB_KEY_ID_SIZE];
    uint8_t  signature[SB_SIGNATURE_SIZE];
} sb_manifest_t;

/* ── Verification output ──────────────────────────────────── */
typedef struct {
    uint8_t  key_type;
    uint8_t  key_id[SB_KEY_ID_SIZE];
    uint32_t image_version;
    uint32_t anti_rollback;
    uint32_t image_len;
} sb_verify_result_t;

/**
 * @brief Parse and structurally validate a manifest.
 *
 * @param buf  Serialised manifest.
 * @param len  Must be exactly SB_MANIFEST_SIZE.
 * @param out  Parsed fields.
 * @return SB_OK, SB_ERR_BAD_PARAM, SB_ERR_BAD_FORMAT or SB_ERR_BAD_MAGIC.
 */
int sb_manifest_parse(const uint8_t *buf, size_t len, sb_manifest_t *out);

/**
 * @brief Build and sign a manifest for an image.
 *
 * @param priv           32-byte private key (P-256 or SM2 scalar).
 * @param key_type       SB_KEY_TYPE_P256 / SB_KEY_TYPE_SM2.
 * @param image_version  Monotonic image version.
 * @param anti_rollback  Anti-rollback counter value.
 * @param image          Image bytes (must be non-NULL).
 * @param image_len      Image length, must fit in u32.
 * @param key_id         16-byte key identifier (may be all zero).
 * @param out            Output buffer.
 * @param out_cap        Must be >= SB_MANIFEST_SIZE.
 * @param out_len        Receives SB_MANIFEST_SIZE.
 * @return SB_OK or an sb_status_t error.
 */
int sb_manifest_sign(const uint8_t priv[32], uint8_t key_type,
                     uint32_t image_version, uint32_t anti_rollback,
                     const uint8_t *image, size_t image_len,
                     const uint8_t key_id[16],
                     uint8_t *out, size_t out_cap, size_t *out_len);

/**
 * @brief Verify an image against a signed manifest.
 *
 * Checks, in order: manifest format, trusted-key type match, manifest
 * signature, image length/hash, and anti-rollback policy.
 *
 * @param image            Image bytes.
 * @param image_len        Image length.
 * @param manifest         Serialised manifest.
 * @param manifest_len     Must be SB_MANIFEST_SIZE.
 * @param trusted_pub      64-byte uncompressed public key (X || Y).
 * @param trusted_key_type SB_KEY_TYPE_P256 / SB_KEY_TYPE_SM2.
 * @param min_counter      Minimum acceptable anti_rollback value.
 * @param result           Optional verified metadata.
 * @return SB_OK or an sb_status_t error.
 */
int sb_verify_image(const uint8_t *image, size_t image_len,
                    const uint8_t *manifest, size_t manifest_len,
                    const uint8_t *trusted_pub, uint8_t trusted_key_type,
                    uint32_t min_counter, sb_verify_result_t *result);

/* ── Anti-rollback counter storage abstraction ────────────── */
typedef struct {
    int (*read)(uint32_t *out);
    int (*write)(uint32_t v);
} sb_counter_store_t;

/**
 * @brief Commit a new monotonic counter value.
 *
 * Reads the current value, refuses to move backwards (SB_ERR_ROLLBACK) and
 * only calls write() when new_counter >= current. Read/write failures return
 * SB_ERR_IO and never write.
 */
int sb_counter_commit(const sb_counter_store_t *store, uint32_t new_counter);

/**
 * @brief Debug-port policy: only DEV lifecycle unlocks the debug port.
 *
 * @return 1 if allowed, 0 for PROD/RMA/any unknown value.
 */
int sb_debug_port_allowed(uint8_t lifecycle);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_BOOT_H */
