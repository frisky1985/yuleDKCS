/**
 * @file sec_x509.h
 * @brief Minimal X.509 (DER) parser for certificate-chain validation.
 *
 * Scope: ECDSA P-256 / SHA-256 end-entity and CA certificates. Extracts the
 * TBSCertificate bytes, issuer/subject names, validity, SubjectPublicKeyInfo
 * and ECDSA signature. Extensions are skipped.
 *
 * This is deliberately minimal: no extension parsing (BasicConstraints, KeyUsage,
 * CRL/OCSP). Chain validation in security.c enforces issuer linkage, signature,
 * curve and validity.
 */

#ifndef SEC_X509_H
#define SEC_X509_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEC_X509_PUBKEY_LEN 64
#define SEC_X509_SIG_LEN    64

typedef struct {
    const uint8_t *tbs;        /**< DER-encoded TBSCertificate (tag+len+value) */
    uint16_t       tbs_len;
    const uint8_t *issuer;     /**< DER-encoded Name */
    uint16_t       issuer_len;
    const uint8_t *subject;    /**< DER-encoded Name */
    uint16_t       subject_len;
    uint8_t        pub[SEC_X509_PUBKEY_LEN]; /**< X || Y */
    uint8_t        sig[SEC_X509_SIG_LEN];    /**< r || s */
    uint32_t       not_before; /**< Unix seconds */
    uint32_t       not_after;
    uint8_t        has_validity;
} sec_x509_cert_t;

/**
 * @brief Parse one DER-encoded certificate.
 * @return 0 on success, -1 on any structural/algorithm error.
 */
int sec_x509_parse(const uint8_t *der, uint16_t len, sec_x509_cert_t *out);

/**
 * @brief Return the total encoded length of the certificate at @p der.
 * @return Total length (>0) or -1 if malformed/truncated.
 */
int sec_x509_total_len(const uint8_t *der, uint16_t avail);

#ifdef __cplusplus
}
#endif

#endif /* SEC_X509_H */
