/**
 * @file sec_x509.h
 * @brief Minimal X.509 (DER) parser for certificate-chain validation.
 *
 * Scope: ECDSA P-256 / SHA-256 end-entity and CA certificates. Extracts the
 * TBSCertificate bytes, issuer/subject names, validity, SubjectPublicKeyInfo,
 * ECDSA signature, serial number and the security-relevant extensions
 * (basicConstraints / keyUsage). Unknown *critical* extensions are flagged so
 * chain validation can reject the certificate (RFC 5280 §6.1.4).
 *
 * Deliberately minimal: no pathLenConstraint, nameConstraints, policy or
 * CRL/OCSP parsing.
 */

#ifndef SEC_X509_H
#define SEC_X509_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEC_X509_PUBKEY_LEN 64
#define SEC_X509_SIG_LEN    64
#define SEC_X509_SERIAL_MAX 20

/* KeyUsage bit masks (DER BIT STRING, MSB-first within the first byte). */
#define SEC_X509_KU_DIGITAL_SIGNATURE 0x80U
#define SEC_X509_KU_KEY_CERT_SIGN     0x04U
#define SEC_X509_KU_CRL_SIGN          0x02U

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
    uint8_t        serial[SEC_X509_SERIAL_MAX]; /**< big-endian, leading zeros stripped */
    uint8_t        serial_len;
    uint8_t        is_ca;              /**< basicConstraints cA == TRUE */
    uint8_t        has_basic_constraints;
    uint8_t        key_usage;          /**< KeyUsage bit mask */
    uint8_t        has_key_usage;
    uint8_t        unknown_critical;   /**< 1 = contains unsupported critical extension */
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
