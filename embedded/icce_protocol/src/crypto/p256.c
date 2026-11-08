/**
 * @file p256.c
 * @brief ECDSA P-256 (secp256r1) verify/sign.
 *
 * NOTE: secp256r1 does NOT share its field prime with SM2.  The SM2
 * helpers in crypto_utils.c (fp_*, ec_point_*) are hard-wired to the
 * sm2p256v1 modulus/a, so P-256 keeps a private set of field and
 * Jacobian point operations here.  Only bn256_* and the generic
 * bn256_mod_mul()/bn256_mod_inv() are reused.
 */

#include "p256.h"
#include "crypto_utils.h"

#include <string.h>

/* p = FFFFFFFF 00000001 00000000 00000000 00000000 FFFFFFFF FFFFFFFF FFFFFFFF */
static const bn256_t P256_P = {
    { 0xFFFFFFFFU, 0x00000001U, 0x00000000U, 0x00000000U,
      0x00000000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU }
};

/* a = p - 3 = FFFFFFFF 00000001 00000000 00000000 00000000 FFFFFFFF FFFFFFFF FFFFFFFC */
static const bn256_t P256_A = {
    { 0xFFFFFFFFU, 0x00000001U, 0x00000000U, 0x00000000U,
      0x00000000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFCU }
};

/* b = 5AC635D8 AA3A93E7 B3EBBD55 769886BC 651D06B0 CC53B0F6 3BCE3C3E 27D2604B */
static const bn256_t P256_B = {
    { 0x5AC635D8U, 0xAA3A93E7U, 0xB3EBBD55U, 0x769886BCU,
      0x651D06B0U, 0xCC53B0F6U, 0x3BCE3C3EU, 0x27D2604BU }
};

/* Gx = 6B17D1F2 E12C4247 F8BCE6E5 63A440F2 77037D81 2DEB33A0 F4A13945 D898C296 */
static const bn256_t P256_GX = {
    { 0x6B17D1F2U, 0xE12C4247U, 0xF8BCE6E5U, 0x63A440F2U,
      0x77037D81U, 0x2DEB33A0U, 0xF4A13945U, 0xD898C296U }
};

/* Gy = 4FE342E2 FE1A7F9B 8EE7EB4A 7C0F9E16 2BCE3357 6B315ECE CBB64068 37BF51F5 */
static const bn256_t P256_GY = {
    { 0x4FE342E2U, 0xFE1A7F9BU, 0x8EE7EB4AU, 0x7C0F9E16U,
      0x2BCE3357U, 0x6B315ECEU, 0xCBB64068U, 0x37BF51F5U }
};

/* n = FFFFFFFF 00000000 FFFFFFFF FFFFFFFF BCE6FAAD A7179E84 F3B9CAC2 FC632551 */
static const bn256_t P256_N = {
    { 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0xFFFFFFFFU,
      0xBCE6FAADU, 0xA7179E84U, 0xF3B9CAC2U, 0xFC632551U }
};

/* ── F_p arithmetic ───────────────────────────────────────────────────── */

static void p256_fp_add(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    bn256_t s;
    uint32_t carry = bn256_add(&s, a, b);

    if (carry || bn256_cmp(&s, &P256_P) >= 0) {
        (void)bn256_sub(&s, &s, &P256_P);
    }
    bn256_copy(r, &s);
}

static void p256_fp_sub(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    bn256_t d;

    if (bn256_sub(&d, a, b)) {
        (void)bn256_add(&d, &d, &P256_P);
    }
    bn256_copy(r, &d);
}

static void p256_fp_mul(bn256_t *r, const bn256_t *a, const bn256_t *b)
{
    bn256_mod_mul(r, a, b, &P256_P);
}

static void p256_fp_sqr(bn256_t *r, const bn256_t *a)
{
    bn256_mod_mul(r, a, a, &P256_P);
}

static void p256_fp_inv(bn256_t *r, const bn256_t *a)
{
    bn256_mod_inv(r, a, &P256_P);
}

/* ── Jacobian point arithmetic (P-256 curve) ─────────────────────────── */

static int p256_point_is_inf(const ec_point_jac_t *p)
{
    return bn256_is_zero(&p->Z);
}

static void p256_point_dbl(ec_point_jac_t *r, const ec_point_jac_t *a)
{
    bn256_t X1, Y1, Z1, XX, YY, YYYY, S, M, t0, t1;

    if (p256_point_is_inf(a)) {
        bn256_set_word(&r->X, 0);
        bn256_set_word(&r->Y, 0);
        bn256_set_word(&r->Z, 0);
        return;
    }

    /* Snapshot: r may alias a. */
    bn256_copy(&X1, &a->X);
    bn256_copy(&Y1, &a->Y);
    bn256_copy(&Z1, &a->Z);

    p256_fp_sqr(&XX, &X1);
    p256_fp_sqr(&YY, &Y1);
    p256_fp_sqr(&YYYY, &YY);

    p256_fp_add(&S, &X1, &YY);
    p256_fp_sqr(&S, &S);
    p256_fp_sub(&S, &S, &XX);
    p256_fp_sub(&S, &S, &YYYY);
    p256_fp_add(&S, &S, &S);        /* S = 2*((X+Y²)² - X² - Y⁴) */

    p256_fp_sqr(&t0, &Z1);
    p256_fp_sqr(&t0, &t0);
    p256_fp_mul(&t0, &t0, &P256_A); /* a*Z⁴ = -3*Z⁴ */
    p256_fp_add(&M, &XX, &XX);
    p256_fp_add(&M, &M, &XX);
    p256_fp_add(&M, &M, &t0);       /* M = 3X² + aZ⁴ */

    p256_fp_sqr(&r->X, &M);
    p256_fp_sub(&r->X, &r->X, &S);
    p256_fp_sub(&r->X, &r->X, &S);

    p256_fp_sub(&t0, &S, &r->X);
    p256_fp_mul(&t1, &M, &t0);
    p256_fp_add(&YYYY, &YYYY, &YYYY);
    p256_fp_add(&YYYY, &YYYY, &YYYY);
    p256_fp_add(&YYYY, &YYYY, &YYYY);
    p256_fp_sub(&r->Y, &t1, &YYYY);

    p256_fp_mul(&t0, &Y1, &Z1);
    p256_fp_add(&r->Z, &t0, &t0);
}

static void p256_point_add(ec_point_jac_t *r, const ec_point_jac_t *a,
                           const ec_point_jac_t *b)
{
    bn256_t Z1Z1, Z2Z2, U1, U2, S1, S2, H, rr, H2, H3, U1H2, t0, t1;

    if (p256_point_is_inf(a)) {
        bn256_copy(&r->X, &b->X);
        bn256_copy(&r->Y, &b->Y);
        bn256_copy(&r->Z, &b->Z);
        return;
    }
    if (p256_point_is_inf(b)) {
        bn256_copy(&r->X, &a->X);
        bn256_copy(&r->Y, &a->Y);
        bn256_copy(&r->Z, &a->Z);
        return;
    }

    p256_fp_sqr(&Z1Z1, &a->Z);
    p256_fp_sqr(&Z2Z2, &b->Z);
    p256_fp_mul(&U1, &a->X, &Z2Z2);
    p256_fp_mul(&U2, &b->X, &Z1Z1);
    p256_fp_mul(&t0, &b->Z, &Z2Z2);
    p256_fp_mul(&S1, &a->Y, &t0);
    p256_fp_mul(&t0, &a->Z, &Z1Z1);
    p256_fp_mul(&S2, &b->Y, &t0);

    p256_fp_sub(&H, &U2, &U1);
    p256_fp_sub(&rr, &S2, &S1);

    if (bn256_is_zero(&H)) {
        if (bn256_is_zero(&rr)) {
            p256_point_dbl(r, a);
        } else {
            /* a == -b → point at infinity */
            bn256_set_word(&r->X, 0);
            bn256_set_word(&r->Y, 0);
            bn256_set_word(&r->Z, 0);
        }
        return;
    }

    p256_fp_sqr(&H2, &H);
    p256_fp_mul(&H3, &H2, &H);
    p256_fp_mul(&U1H2, &U1, &H2);

    p256_fp_sqr(&t0, &rr);
    p256_fp_sub(&t0, &t0, &H3);
    p256_fp_sub(&t0, &t0, &U1H2);
    p256_fp_sub(&t0, &t0, &U1H2);
    bn256_copy(&r->X, &t0);         /* X3 = r² - H³ - 2U1H² */

    p256_fp_sub(&t0, &U1H2, &r->X);
    p256_fp_mul(&t1, &rr, &t0);
    p256_fp_mul(&t0, &S1, &H3);
    p256_fp_sub(&r->Y, &t1, &t0);   /* Y3 = r(U1H² - X3) - S1H³ */

    p256_fp_mul(&t0, &a->Z, &b->Z);
    p256_fp_mul(&r->Z, &t0, &H);    /* Z3 = Z1Z2H */
}

/* Scalar multiplication r = k * p (MSB-first double-and-add). */
static void p256_point_mul(ec_point_jac_t *r, const bn256_t *k,
                           const ec_point_jac_t *p)
{
    ec_point_jac_t acc;

    bn256_set_word(&acc.X, 0);
    bn256_set_word(&acc.Y, 0);
    bn256_set_word(&acc.Z, 0);

    for (int i = 0; i < 256; i++) {
        int word_idx = i / 32;
        int bit_idx = 31 - (i % 32);
        uint32_t bit = (k->w[word_idx] >> bit_idx) & 1U;

        p256_point_dbl(&acc, &acc);
        if (bit) {
            p256_point_add(&acc, &acc, p);
        }
    }

    bn256_copy(&r->X, &acc.X);
    bn256_copy(&r->Y, &acc.Y);
    bn256_copy(&r->Z, &acc.Z);
}

static int p256_point_on_curve(const bn256_t *x, const bn256_t *y)
{
    bn256_t lhs, rhs, t, x2;

    if (bn256_cmp(x, &P256_P) >= 0 || bn256_cmp(y, &P256_P) >= 0) {
        return 0;
    }

    p256_fp_sqr(&lhs, y);
    p256_fp_sqr(&x2, x);
    p256_fp_mul(&rhs, &x2, x);
    p256_fp_add(&t, x, x);
    p256_fp_add(&t, &t, x);
    p256_fp_sub(&rhs, &rhs, &t);
    p256_fp_add(&rhs, &rhs, &P256_B);

    return bn256_cmp(&lhs, &rhs) == 0;
}

int crypto_p256_pubkey_valid(const uint8_t pub[64])
{
    bn256_t x, y;

    if (pub == NULL) return CRYPTO_ERR_NULL_PTR;

    bn256_from_bytes(&x, pub);
    bn256_from_bytes(&y, pub + 32);
    return p256_point_on_curve(&x, &y) ? CRYPTO_SUCCESS
                                       : CRYPTO_ERR_INVALID_INPUT;
}

/* R = u1*G + u2*Q (mod n), affine X returned in rx. */
static int p256_ecdsa_point(const bn256_t *u1, const bn256_t *u2,
                            const bn256_t *qx, const bn256_t *qy,
                            bn256_t *rx)
{
    ec_point_jac_t G, Q, A, B, R;
    bn256_t x, y;

    if (!p256_point_on_curve(qx, qy)) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    G.X = P256_GX; G.Y = P256_GY; bn256_set_word(&G.Z, 1);
    Q.X = *qx; Q.Y = *qy; bn256_set_word(&Q.Z, 1);

    p256_point_mul(&A, u1, &G);
    p256_point_mul(&B, u2, &Q);
    p256_point_add(&R, &A, &B);

    if (p256_point_is_inf(&R)) return CRYPTO_ERR_VERIFY_FAILED;

    {
        bn256_t zinv, z2, z3;
        p256_fp_inv(&zinv, &R.Z);
        p256_fp_sqr(&z2, &zinv);
        p256_fp_mul(&z3, &z2, &zinv);
        p256_fp_mul(&x, &R.X, &z2);
        p256_fp_mul(&y, &R.Y, &z3);
    }
    (void)y;
    bn256_copy(rx, &x);
    return CRYPTO_SUCCESS;
}

int crypto_p256_verify(const uint8_t pub[64], const uint8_t hash[32],
                       const uint8_t sig[64])
{
    bn256_t qx, qy, r, s, e, w, u1, u2, xr;

    if (pub == NULL || hash == NULL || sig == NULL) {
        return CRYPTO_ERR_NULL_PTR;
    }

    bn256_from_bytes(&qx, pub);
    bn256_from_bytes(&qy, pub + 32);
    if (!p256_point_on_curve(&qx, &qy)) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    bn256_from_bytes(&r, sig);
    bn256_from_bytes(&s, sig + 32);

    /* r, s ∈ [1, n-1] */
    if (bn256_is_zero(&r) || bn256_is_zero(&s) ||
        bn256_cmp(&r, &P256_N) >= 0 || bn256_cmp(&s, &P256_N) >= 0) {
        return CRYPTO_ERR_VERIFY_FAILED;
    }

    bn256_from_bytes(&e, hash);

    /* w = s⁻¹, u1 = e·w, u2 = r·w (mod n) */
    bn256_mod_inv(&w, &s, &P256_N);
    bn256_mod_mul(&u1, &e, &w, &P256_N);
    bn256_mod_mul(&u2, &r, &w, &P256_N);

    if (p256_ecdsa_point(&u1, &u2, &qx, &qy, &xr) != CRYPTO_SUCCESS) {
        return CRYPTO_ERR_VERIFY_FAILED;
    }

    /* xr mod n (xr < p; p - n < n, so at most one subtraction) */
    if (bn256_cmp(&xr, &P256_N) >= 0) {
        (void)bn256_sub(&xr, &xr, &P256_N);
    }

    return (bn256_cmp(&xr, &r) == 0) ? CRYPTO_SUCCESS
                                     : CRYPTO_ERR_VERIFY_FAILED;
}

int crypto_p256_sign(const uint8_t priv[32], const uint8_t hash[32],
                     uint8_t sig[64])
{
    bn256_t d, e, k, r, s, rx, ry, kinv, t;
    ec_point_jac_t G, K;
    int attempt;

    if (priv == NULL || hash == NULL || sig == NULL) {
        return CRYPTO_ERR_NULL_PTR;
    }

    bn256_from_bytes(&d, priv);
    if (bn256_is_zero(&d) || bn256_cmp(&d, &P256_N) >= 0) {
        return CRYPTO_ERR_INVALID_INPUT;
    }
    bn256_from_bytes(&e, hash);

    G.X = P256_GX; G.Y = P256_GY; bn256_set_word(&G.Z, 1);

    for (attempt = 0; attempt < 16; attempt++) {
        uint8_t kbytes[32];

        if (crypto_random_bytes(kbytes, sizeof(kbytes)) != 0) {
            return CRYPTO_ERR_KEY_GEN_FAILED;
        }
        bn256_from_bytes(&k, kbytes);
        if (bn256_is_zero(&k) || bn256_cmp(&k, &P256_N) >= 0) {
            continue;
        }

        p256_point_mul(&K, &k, &G);
        if (p256_point_is_inf(&K)) {
            continue;
        }
        {
            bn256_t zinv, z2, z3;
            p256_fp_inv(&zinv, &K.Z);
            p256_fp_sqr(&z2, &zinv);
            p256_fp_mul(&z3, &z2, &zinv);
            p256_fp_mul(&rx, &K.X, &z2);
            p256_fp_mul(&ry, &K.Y, &z3);
        }
        (void)ry;

        /* r = rx mod n */
        bn256_copy(&r, &rx);
        if (bn256_cmp(&r, &P256_N) >= 0) {
            (void)bn256_sub(&r, &r, &P256_N);
        }
        if (bn256_is_zero(&r)) {
            continue;
        }

        /* s = k⁻¹ (e + r·d) mod n */
        bn256_mod_mul(&t, &r, &d, &P256_N);
        {
            bn256_t sum;
            uint32_t carry = bn256_add(&sum, &e, &t);
            if (carry || bn256_cmp(&sum, &P256_N) >= 0) {
                (void)bn256_sub(&sum, &sum, &P256_N);
            }
            bn256_copy(&t, &sum);
        }
        bn256_mod_inv(&kinv, &k, &P256_N);
        bn256_mod_mul(&s, &kinv, &t, &P256_N);
        if (bn256_is_zero(&s)) {
            continue;
        }

        /* Low-s normalisation is not applied: CCC/ICCE accept both. */
        bn256_to_bytes(&r, sig);
        bn256_to_bytes(&s, sig + 32);
        return CRYPTO_SUCCESS;
    }

    return CRYPTO_ERR_KEY_GEN_FAILED;
}
