/**
 * @file sec_x509.c
 * @brief Minimal DER/X.509 parser (P-256 / SHA-256).
 */

#include "sec_x509.h"

#include <string.h>

#define DER_TAG_SEQUENCE 0x30U
#define DER_TAG_INTEGER  0x02U
#define DER_TAG_BITSTR   0x03U
#define DER_TAG_OID      0x06U
#define DER_TAG_UTCTIME  0x17U
#define DER_TAG_GENTIME  0x18U
#define DER_TAG_CTX0     0xA0U

/* OID values (content octets, without tag/len) */
static const uint8_t OID_EC_PUBKEY[] = {
    0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01
};
static const uint8_t OID_PRIME256V1[] = {
    0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
};
static const uint8_t OID_ECDSA_SHA256[] = {
    0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02
};

/* Read one TLV. Returns 0 on success, -1 on malformed/truncated input. */
static int der_read(const uint8_t *p, uint16_t avail, uint8_t *tag,
                    const uint8_t **val, uint16_t *vlen, uint16_t *total)
{
    uint16_t hdr = 2;
    uint32_t len;

    if (p == NULL || avail < 2U) return -1;

    *tag = p[0];
    if ((p[1] & 0x80U) == 0U) {
        len = p[1];
    } else {
        uint8_t n = (uint8_t)(p[1] & 0x7FU);
        if (n == 0U || n > 2U) return -1;      /* indefinite / unsupported */
        if ((uint16_t)(2U + n) > avail) return -1;
        len = 0;
        for (uint8_t i = 0; i < n; i++) {
            len = (len << 8) | p[2U + i];
        }
        hdr = (uint16_t)(2U + n);
    }

    if (len > (uint32_t)(avail - hdr)) return -1;
    if (val != NULL)   *val = p + hdr;
    if (vlen != NULL)  *vlen = (uint16_t)len;
    if (total != NULL) *total = (uint16_t)(hdr + len);
    return 0;
}

static int oid_equals(const uint8_t *val, uint16_t len,
                      const uint8_t *expect, uint16_t expect_len)
{
    return (len == expect_len) && (memcmp(val, expect, len) == 0);
}

static uint32_t days_from_civil(int y, unsigned m, unsigned d)
{
    int era;
    unsigned yoe, doy, doe;
    int64_t days;

    y -= (m <= 2U) ? 1 : 0;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153U * ((m > 2U) ? (m - 3U) : (m + 9U)) + 2U) / 5U + d - 1U;
    doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    days = (int64_t)era * 146097 + (int64_t)doe - 719468;
    return (uint32_t)days;
}

static int digits(const uint8_t *s, uint8_t n, unsigned *out)
{
    unsigned v = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10U + (unsigned)(s[i] - '0');
    }
    *out = v;
    return 0;
}

/* UTCTime (YYMMDDHHMMSSZ) or GeneralizedTime (YYYYMMDDHHMMSSZ). */
static int parse_time(const uint8_t *tag, const uint8_t *val, uint16_t len,
                      uint32_t *out)
{
    unsigned year, mon, day, hour, min, sec;
    const uint8_t *p = val;
    uint8_t ylen;

    if (*tag == DER_TAG_UTCTIME) {
        if (len != 13U || val[12] != 'Z') return -1;
        if (digits(p, 2, &year) != 0) return -1;
        year += (year < 50U) ? 2000U : 1900U;
        ylen = 2;
    } else if (*tag == DER_TAG_GENTIME) {
        if (len != 15U || val[14] != 'Z') return -1;
        if (digits(p, 4, &year) != 0) return -1;
        ylen = 4;
    } else {
        return -1;
    }

    p += ylen;
    if (digits(p + 0, 2, &mon) != 0) return -1;
    if (digits(p + 2, 2, &day) != 0) return -1;
    if (digits(p + 4, 2, &hour) != 0) return -1;
    if (digits(p + 6, 2, &min) != 0) return -1;
    if (digits(p + 8, 2, &sec) != 0) return -1;

    if (mon < 1U || mon > 12U || day < 1U || day > 31U ||
        hour > 23U || min > 59U || sec > 60U) {
        return -1;
    }

    *out = days_from_civil((int)year, mon, day) * 86400U +
           hour * 3600U + min * 60U + sec;
    return 0;
}

/* Convert DER INTEGER (possibly with leading zero) to a fixed 32-byte value. */
static int int_to_fixed(const uint8_t *val, uint16_t len, uint8_t out[32])
{
    while (len > 0U && val[0] == 0x00U) {
        val++;
        len--;
    }
    if (len > 32U) return -1;
    (void)memset(out, 0, 32);
    (void)memcpy(out + (32U - len), val, len);
    return 0;
}

static int parse_validity(const uint8_t *val, uint16_t len, sec_x509_cert_t *out)
{
    const uint8_t *p, *v;
    uint16_t vlen, total, rem;
    uint8_t tag;

    p = val;
    rem = len;
    if (der_read(p, rem, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_UTCTIME && tag != DER_TAG_GENTIME) return -1;
    if (parse_time(&tag, v, vlen, &out->not_before) != 0) return -1;

    p += total;
    rem = (uint16_t)(rem - total);
    if (der_read(p, rem, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_UTCTIME && tag != DER_TAG_GENTIME) return -1;
    if (parse_time(&tag, v, vlen, &out->not_after) != 0) return -1;

    out->has_validity = 1;
    return 0;
}

static int parse_spki(const uint8_t *val, uint16_t len, uint8_t pub[64])
{
    const uint8_t *p, *v, *alg_val;
    uint16_t vlen, total, rem, alg_len, alg_total;
    uint8_t tag;

    p = val;
    rem = len;

    /* AlgorithmIdentifier */
    if (der_read(p, rem, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_SEQUENCE) return -1;
    alg_val = v;
    alg_len = vlen;
    alg_total = total;

    if (der_read(alg_val, alg_len, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_OID || !oid_equals(v, vlen, OID_EC_PUBKEY,
                                          (uint16_t)sizeof(OID_EC_PUBKEY))) {
        return -1;
    }
    if (der_read(alg_val + total, (uint16_t)(alg_len - total), &tag, &v,
                 &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_OID || !oid_equals(v, vlen, OID_PRIME256V1,
                                          (uint16_t)sizeof(OID_PRIME256V1))) {
        return -1;
    }

    /* subjectPublicKey BIT STRING */
    p += alg_total;
    rem = (uint16_t)(rem - alg_total);
    if (der_read(p, rem, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_BITSTR || vlen != 66U || v[0] != 0x00U || v[1] != 0x04U) {
        return -1;
    }
    (void)memcpy(pub, v + 2, 64);
    return 0;
}

static int parse_sig_value(const uint8_t *val, uint16_t len, uint8_t sig[64])
{
    const uint8_t *seq, *v;
    uint16_t seq_len, vlen, total, rem;
    uint8_t tag;

    if (der_read(val, len, &tag, &seq, &seq_len, &total) != 0) return -1;
    if (tag != DER_TAG_SEQUENCE || total != len) return -1;

    if (der_read(seq, seq_len, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_INTEGER) return -1;
    if (int_to_fixed(v, vlen, sig) != 0) return -1;

    rem = (uint16_t)(seq_len - total);
    if (der_read(seq + total, rem, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_INTEGER) return -1;
    return int_to_fixed(v, vlen, sig + 32);
}

int sec_x509_total_len(const uint8_t *der, uint16_t avail)
{
    const uint8_t *v;
    uint16_t vlen, total;
    uint8_t tag;

    if (der_read(der, avail, &tag, &v, &vlen, &total) != 0) return -1;
    (void)v;
    (void)vlen;
    return (int)total;
}

int sec_x509_parse(const uint8_t *der, uint16_t len, sec_x509_cert_t *out)
{
    const uint8_t *p, *v, *tbs_val, *tbs_start;
    uint16_t vlen, total, rem, tbs_len, tbs_total;
    uint8_t tag;

    if (der == NULL || out == NULL) return -1;
    (void)memset(out, 0, sizeof(*out));

    /* Certificate ::= SEQUENCE { tbsCertificate, signatureAlgorithm, signatureValue } */
    if (der_read(der, len, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_SEQUENCE || total != len) return -1;
    p = v;
    rem = vlen;

    /* tbsCertificate */
    tbs_start = p;
    if (der_read(p, rem, &tag, &tbs_val, &tbs_len, &tbs_total) != 0) return -1;
    if (tag != DER_TAG_SEQUENCE) return -1;
    out->tbs = tbs_start;
    out->tbs_len = tbs_total;

    /* Walk TBSCertificate fields */
    {
        const uint8_t *q = tbs_val;
        uint16_t qrem = tbs_len;

        /* optional [0] version */
        if (qrem >= 1U && q[0] == DER_TAG_CTX0) {
            if (der_read(q, qrem, &tag, &v, &vlen, &total) != 0) return -1;
            q += total;
            qrem = (uint16_t)(qrem - total);
        }
        /* serialNumber INTEGER */
        if (der_read(q, qrem, &tag, &v, &vlen, &total) != 0) return -1;
        if (tag != DER_TAG_INTEGER) return -1;
        q += total;
        qrem = (uint16_t)(qrem - total);

        /* signature AlgorithmIdentifier */
        if (der_read(q, qrem, &tag, &v, &vlen, &total) != 0) return -1;
        if (tag != DER_TAG_SEQUENCE) return -1;
        q += total;
        qrem = (uint16_t)(qrem - total);

        /* issuer Name */
        out->issuer = q;
        if (der_read(q, qrem, &tag, &v, &vlen, &total) != 0) return -1;
        if (tag != DER_TAG_SEQUENCE) return -1;
        out->issuer_len = total;
        q += total;
        qrem = (uint16_t)(qrem - total);

        /* validity */
        if (der_read(q, qrem, &tag, &v, &vlen, &total) != 0) return -1;
        if (tag != DER_TAG_SEQUENCE) return -1;
        if (parse_validity(v, vlen, out) != 0) return -1;
        q += total;
        qrem = (uint16_t)(qrem - total);

        /* subject Name */
        out->subject = q;
        if (der_read(q, qrem, &tag, &v, &vlen, &total) != 0) return -1;
        if (tag != DER_TAG_SEQUENCE) return -1;
        out->subject_len = total;
        q += total;
        qrem = (uint16_t)(qrem - total);

        /* subjectPublicKeyInfo */
        if (der_read(q, qrem, &tag, &v, &vlen, &total) != 0) return -1;
        if (tag != DER_TAG_SEQUENCE) return -1;
        if (parse_spki(v, vlen, out->pub) != 0) return -1;
    }

    /* signatureAlgorithm */
    p += tbs_total;
    rem = (uint16_t)(rem - tbs_total);
    if (der_read(p, rem, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_SEQUENCE) return -1;
    {
        const uint8_t *oid;
        uint16_t oid_len, oid_total;
        if (der_read(v, vlen, &tag, &oid, &oid_len, &oid_total) != 0) return -1;
        if (tag != DER_TAG_OID ||
            !oid_equals(oid, oid_len, OID_ECDSA_SHA256,
                        (uint16_t)sizeof(OID_ECDSA_SHA256))) {
            return -1;
        }
    }

    /* signatureValue */
    p += total;
    rem = (uint16_t)(rem - total);
    if (der_read(p, rem, &tag, &v, &vlen, &total) != 0) return -1;
    if (tag != DER_TAG_BITSTR || vlen < 3U || v[0] != 0x00U) return -1;
    return parse_sig_value(v + 1, (uint16_t)(vlen - 1U), out->sig);
}
