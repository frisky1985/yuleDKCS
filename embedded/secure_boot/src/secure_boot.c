/**
 * @file secure_boot.c
 * @brief Minimal software secure-boot implementation (manifest sign/verify).
 */

#include "secure_boot.h"
#include "crypto_engine.h"
#include "p256.h"
#include "sm2.h"

#include <string.h>

/* ── Little-endian field helpers ──────────────────────────── */
static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t get_u32le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void manifest_serialize(const sb_manifest_t *m,
                               uint8_t out[SB_MANIFEST_SIZE])
{
    (void)memcpy(out, m->magic, 4);
    out[4] = m->format_version;
    out[5] = m->key_type;
    put_u32le(out + 6,  m->image_version);
    put_u32le(out + 10, m->anti_rollback);
    put_u32le(out + 14, m->image_len);
    (void)memcpy(out + 18, m->image_sha256, SB_SHA256_SIZE);
    (void)memcpy(out + 50, m->key_id, SB_KEY_ID_SIZE);
    (void)memcpy(out + 66, m->signature, SB_SIGNATURE_SIZE);
}

/* ── Manifest parse ───────────────────────────────────────── */
int sb_manifest_parse(const uint8_t *buf, size_t len, sb_manifest_t *out)
{
    static const uint8_t magic[4] = { 'Y', 'S', 'B', 'M' };

    if ((buf == NULL) || (out == NULL)) {
        return SB_ERR_BAD_PARAM;
    }
    /* Fixed-size record: trailing bytes would not be authenticated. */
    if (len != SB_MANIFEST_SIZE) {
        return SB_ERR_BAD_FORMAT;
    }

    (void)memset(out, 0, sizeof(*out));
    (void)memcpy(out->magic, buf, 4);
    if (memcmp(out->magic, magic, 4) != 0) {
        return SB_ERR_BAD_MAGIC;
    }

    out->format_version = buf[4];
    out->key_type       = buf[5];
    if ((out->format_version != SB_FORMAT_VERSION) ||
        ((out->key_type != SB_KEY_TYPE_P256) &&
         (out->key_type != SB_KEY_TYPE_SM2))) {
        return SB_ERR_BAD_FORMAT;
    }

    out->image_version = get_u32le(buf + 6);
    out->anti_rollback = get_u32le(buf + 10);
    out->image_len     = get_u32le(buf + 14);
    (void)memcpy(out->image_sha256, buf + 18, SB_SHA256_SIZE);
    (void)memcpy(out->key_id,       buf + 50, SB_KEY_ID_SIZE);
    (void)memcpy(out->signature,    buf + 66, SB_SIGNATURE_SIZE);

    return SB_OK;
}

/* ── Manifest sign ────────────────────────────────────────── */
int sb_manifest_sign(const uint8_t priv[32], uint8_t key_type,
                     uint32_t image_version, uint32_t anti_rollback,
                     const uint8_t *image, size_t image_len,
                     const uint8_t key_id[16],
                     uint8_t *out, size_t out_cap, size_t *out_len)
{
    sb_manifest_t m;
    uint8_t buf[SB_MANIFEST_SIZE];
    uint8_t digest[SB_SHA256_SIZE];
    int rc;

    if ((priv == NULL) || (image == NULL) || (key_id == NULL) ||
        (out == NULL) || (out_len == NULL)) {
        return SB_ERR_BAD_PARAM;
    }
    if (out_cap < SB_MANIFEST_SIZE) {
        return SB_ERR_BAD_PARAM;
    }
    if ((key_type != SB_KEY_TYPE_P256) && (key_type != SB_KEY_TYPE_SM2)) {
        return SB_ERR_BAD_PARAM;
    }
#if SIZE_MAX > UINT32_MAX
    if (image_len > (size_t)UINT32_MAX) {
        return SB_ERR_BAD_PARAM;
    }
#endif

    (void)memset(&m, 0, sizeof(m));
    (void)memcpy(m.magic, "YSBM", 4);
    m.format_version = (uint8_t)SB_FORMAT_VERSION;
    m.key_type       = key_type;
    m.image_version  = image_version;
    m.anti_rollback  = anti_rollback;
    m.image_len      = (uint32_t)image_len;
    (void)memcpy(m.key_id, key_id, SB_KEY_ID_SIZE);

    if (crypto_sha256(image, image_len, m.image_sha256) != CRYPTO_SUCCESS) {
        return SB_ERR_CRYPTO;
    }

    /* Signature covers every manifest byte except the signature itself. */
    manifest_serialize(&m, buf);
    if (crypto_sha256(buf, SB_MANIFEST_SIGNED_SIZE, digest) != CRYPTO_SUCCESS) {
        return SB_ERR_CRYPTO;
    }

    if (key_type == SB_KEY_TYPE_P256) {
        rc = crypto_p256_sign(priv, digest, m.signature);
    } else {
        rc = sm2_sign_hash(priv, digest, m.signature);
    }
    if (rc != CRYPTO_SUCCESS) {
        return SB_ERR_CRYPTO;
    }

    manifest_serialize(&m, buf);
    (void)memcpy(out, buf, SB_MANIFEST_SIZE);
    *out_len = SB_MANIFEST_SIZE;

    return SB_OK;
}

/* ── Image verification ───────────────────────────────────── */
int sb_verify_image(const uint8_t *image, size_t image_len,
                    const uint8_t *manifest, size_t manifest_len,
                    const uint8_t *trusted_pub, uint8_t trusted_key_type,
                    uint32_t min_counter, sb_verify_result_t *result)
{
    sb_manifest_t m;
    uint8_t digest[SB_SHA256_SIZE];
    uint8_t image_hash[SB_SHA256_SIZE];
    int rc;

    if ((image == NULL) || (manifest == NULL) || (trusted_pub == NULL)) {
        return SB_ERR_BAD_PARAM;
    }
    if ((trusted_key_type != SB_KEY_TYPE_P256) &&
        (trusted_key_type != SB_KEY_TYPE_SM2)) {
        return SB_ERR_BAD_PARAM;
    }

    rc = sb_manifest_parse(manifest, manifest_len, &m);
    if (rc != SB_OK) {
        return rc;
    }

    if (m.key_type != trusted_key_type) {
        return SB_ERR_KEY_MISMATCH;
    }

    /* Signature over the signed manifest prefix. */
    if (crypto_sha256(manifest, SB_MANIFEST_SIGNED_SIZE, digest) !=
        CRYPTO_SUCCESS) {
        return SB_ERR_CRYPTO;
    }
    if (trusted_key_type == SB_KEY_TYPE_P256) {
        rc = crypto_p256_verify(trusted_pub, digest, m.signature);
    } else {
        rc = sm2_verify_hash(trusted_pub, digest, m.signature);
    }
    if (rc != CRYPTO_SUCCESS) {
        return SB_ERR_BAD_SIGNATURE;
    }

    /* The signed image_len must describe exactly the supplied image. */
    if ((size_t)m.image_len != image_len) {
        return SB_ERR_BAD_FORMAT;
    }
    if (crypto_sha256(image, image_len, image_hash) != CRYPTO_SUCCESS) {
        return SB_ERR_CRYPTO;
    }
    if (memcmp(image_hash, m.image_sha256, SB_SHA256_SIZE) != 0) {
        return SB_ERR_BAD_HASH;
    }

    if (m.anti_rollback < min_counter) {
        return SB_ERR_ROLLBACK;
    }

    if (result != NULL) {
        result->key_type       = m.key_type;
        result->image_version  = m.image_version;
        result->anti_rollback  = m.anti_rollback;
        result->image_len      = m.image_len;
        (void)memcpy(result->key_id, m.key_id, SB_KEY_ID_SIZE);
    }

    return SB_OK;
}

/* ── Anti-rollback counter ────────────────────────────────── */
int sb_counter_commit(const sb_counter_store_t *store, uint32_t new_counter)
{
    uint32_t current;

    if ((store == NULL) || (store->read == NULL) || (store->write == NULL)) {
        return SB_ERR_BAD_PARAM;
    }
    if (store->read(&current) != 0) {
        return SB_ERR_IO;
    }
    if (new_counter < current) {
        return SB_ERR_ROLLBACK;
    }
    if (store->write(new_counter) != 0) {
        return SB_ERR_IO;
    }

    return SB_OK;
}

/* ── Debug-port policy ────────────────────────────────────── */
int sb_debug_port_allowed(uint8_t lifecycle)
{
    return (lifecycle == SB_LIFECYCLE_DEV) ? 1 : 0;
}
