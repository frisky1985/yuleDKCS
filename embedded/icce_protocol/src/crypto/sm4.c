/**
 * @file sm4.c
 * @brief SM4 分组密码算法实现
 * @version 1.0
 * @date 2026-05-28
 *
 * GB/T 32907-2016 SM4 分组密码算法。
 * 128 位密钥, 128 位分组, 32 轮 Feistel 结构。
 *
 * 测试向量:
 *  密钥:  0123456789ABCDEFFEDCBA9876543210
 *  明文:  0123456789ABCDEFFEDCBA9876543210
 *  密文:  681EDF34D206965E86B3E94F536E4246
 *
 * GCM 模式参考 NIST SP 800-38D。
 */

#include "sm4.h"

/* ========================================================================
 *  SM4 S 盒 (16×16)
 * ======================================================================== */
static const uint8_t SM4_SBOX[256] = {
    0xD6, 0x90, 0xE9, 0xFE, 0xCC, 0xE1, 0x3D, 0xB7, 0x16, 0xB6, 0x14, 0xC2, 0x28, 0xFB, 0x2C, 0x05,
    0x2B, 0x67, 0x9A, 0x76, 0x2A, 0xBE, 0x04, 0xC3, 0xAA, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
    0x9C, 0x42, 0x50, 0xF4, 0x91, 0xEF, 0x98, 0x7A, 0x33, 0x54, 0x0B, 0x43, 0xED, 0xCF, 0xAC, 0x62,
    0xE4, 0xB3, 0x1C, 0xA9, 0xC9, 0x08, 0xE8, 0x95, 0x80, 0xDF, 0x94, 0xFA, 0x75, 0x8F, 0x3F, 0xA6,
    0x47, 0x07, 0xA7, 0xFC, 0xF3, 0x73, 0x17, 0xBA, 0x83, 0x59, 0x3C, 0x19, 0xE6, 0x85, 0x4F, 0xA8,
    0x68, 0x6B, 0x81, 0xB2, 0x71, 0x64, 0xDA, 0x8B, 0xF8, 0xEB, 0x0F, 0x4B, 0x70, 0x56, 0x9D, 0x35,
    0x1E, 0x24, 0x0E, 0x5E, 0x63, 0x58, 0xD1, 0xA2, 0x25, 0x22, 0x7C, 0x3B, 0x01, 0x21, 0x78, 0x87,
    0xD4, 0x00, 0x46, 0x57, 0x9F, 0xD3, 0x27, 0x52, 0x4C, 0x36, 0x02, 0xE7, 0xA0, 0xC4, 0xC8, 0x9E,
    0xEA, 0xBF, 0x8A, 0xD2, 0x40, 0xC7, 0x38, 0xB5, 0xA3, 0xF7, 0xF2, 0xCE, 0xF9, 0x61, 0x15, 0xA1,
    0xE0, 0xAE, 0x5D, 0xA4, 0x9B, 0x34, 0x1A, 0x55, 0xAD, 0x93, 0x32, 0x30, 0xF5, 0x8C, 0xB1, 0xE3,
    0x1D, 0xF6, 0xE2, 0x2E, 0x82, 0x66, 0xCA, 0x60, 0xC0, 0x29, 0x23, 0xAB, 0x0D, 0x53, 0x4E, 0x6F,
    0xD5, 0xDB, 0x37, 0x45, 0xDE, 0xFD, 0x8E, 0x2F, 0x03, 0xFF, 0x6A, 0x72, 0x6D, 0x6C, 0x5B, 0x51,
    0x8D, 0x1B, 0xAF, 0x92, 0xBB, 0xDD, 0xBC, 0x7F, 0x11, 0xD9, 0x5C, 0x41, 0x1F, 0x10, 0x5A, 0xD8,
    0x0A, 0xC1, 0x31, 0x88, 0xA5, 0xCD, 0x7B, 0xBD, 0x2D, 0x74, 0xD0, 0x12, 0xB8, 0xE5, 0xB4, 0xB0,
    0x89, 0x69, 0x97, 0x4A, 0x0C, 0x96, 0x77, 0x7E, 0x65, 0xB9, 0xF1, 0x09, 0xC5, 0x6E, 0xC6, 0x84,
    0x18, 0xF0, 0x7D, 0xEC, 0x3A, 0xDC, 0x4D, 0x20, 0x79, 0xEE, 0x5F, 0x3E, 0xD7, 0xCB, 0x39, 0x48
};

/* ========================================================================
 *  SM4 固定参数 FK (系统参数)
 * ======================================================================== */
static const uint32_t SM4_FK[4] = {
    0xA3B1BAC6, 0x56AA3350, 0x677D9197, 0xB27022DC
};

/* ========================================================================
 *  SM4 固定参数 CK (轮常数, 32 个)
 * ======================================================================== */
static const uint32_t SM4_CK[32] = {
    0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269,
    0x70777E85, 0x8C939AA1, 0xA8AFB6BD, 0xC4CBD2D9,
    0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249,
    0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9,
    0xC0C7CED5, 0xDCE3EAF1, 0xF8FF060D, 0x141B2229,
    0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299,
    0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209,
    0x10171E25, 0x2C333A41, 0x484F565D, 0x646B7279
};

/* ========================================================================
 *  内部辅助函数
 * ======================================================================== */

/* S 盒替换: 输入 32 位, 每个字节替换后输出 32 位 */
static inline uint32_t sm4_sbox_u32(uint32_t x)
{
    uint8_t b[4];
    b[0] = SM4_SBOX[(x >> 24) & 0xFF];
    b[1] = SM4_SBOX[(x >> 16) & 0xFF];
    b[2] = SM4_SBOX[(x >>  8) & 0xFF];
    b[3] = SM4_SBOX[(x      ) & 0xFF];
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
}

/* 线性变换 L (用于轮函数): L(B) = B ⊕ (B ≪ 2) ⊕ (B ≪ 10) ⊕ (B ≪ 18) ⊕ (B ≪ 24) */
static inline uint32_t sm4_l(uint32_t b)
{
    return b ^ rotl32(b, 2) ^ rotl32(b, 10) ^ rotl32(b, 18) ^ rotl32(b, 24);
}

/* 线性变换 L' (用于密钥扩展): L'(B) = B ⊕ (B ≪ 13) ⊕ (B ≪ 23) */
static inline uint32_t sm4_lp(uint32_t b)
{
    return b ^ rotl32(b, 13) ^ rotl32(b, 23);
}

/* T 变换 = L(S_box(B)) */
static inline uint32_t sm4_t(uint32_t x)
{
    return sm4_l(sm4_sbox_u32(x));
}

/* T' 变换 = L'(S_box(B)) */
static inline uint32_t sm4_tp(uint32_t x)
{
    return sm4_lp(sm4_sbox_u32(x));
}

/* ========================================================================
 *  密钥扩展: rk[i] = K[i+4] = K[i] ⊕ T'(K[i+1] ⊕ K[i+2] ⊕ K[i+3] ⊕ CK[i])
 * ======================================================================== */
int sm4_set_key(const uint8_t key[SM4_KEY_SIZE], sm4_key_t *skey)
{
    if (!key || !skey) return CRYPTO_ERR_NULL_PTR;

    uint32_t K[36];

    /* MK = key; K[0..3] = MK ⊕ FK */
    K[0] = load_be32(key)     ^ SM4_FK[0];
    K[1] = load_be32(key + 4) ^ SM4_FK[1];
    K[2] = load_be32(key + 8) ^ SM4_FK[2];
    K[3] = load_be32(key +12) ^ SM4_FK[3];

    /* 生成 32 轮密钥 */
    for (int i = 0; i < 32; i++) {
        K[i+4] = K[i] ^ sm4_tp(K[i+1] ^ K[i+2] ^ K[i+3] ^ SM4_CK[i]);
        skey->rk[i] = K[i+4];
    }

    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  单块加密 (32 轮 Feistel)
 * ========================================================================
 * (X0, X1, X2, X3) = 输入
 * for i = 0..31:
 *     X[i+4] = X[i] ⊕ T(X[i+1] ⊕ X[i+2] ⊕ X[i+3] ⊕ rk[i])
 * 输出 = (X[35], X[34], X[33], X[32])  — 反序
 */
static void sm4_encrypt_block(const sm4_key_t *skey,
                              const uint8_t plain[SM4_BLOCK_SIZE],
                              uint8_t cipher[SM4_BLOCK_SIZE])
{
    uint32_t X[36];

    X[0] = load_be32(plain);
    X[1] = load_be32(plain + 4);
    X[2] = load_be32(plain + 8);
    X[3] = load_be32(plain + 12);

    for (int i = 0; i < 32; i++) {
        X[i+4] = X[i] ^ sm4_t(X[i+1] ^ X[i+2] ^ X[i+3] ^ skey->rk[i]);
    }

    /* 反序输出: (X35, X34, X33, X32) */
    store_be32(cipher,      X[35]);
    store_be32(cipher + 4,  X[34]);
    store_be32(cipher + 8,  X[33]);
    store_be32(cipher + 12, X[32]);
}

/* 单块解密: 轮密钥反序使用 */
static void sm4_decrypt_block(const sm4_key_t *skey,
                              const uint8_t cipher[SM4_BLOCK_SIZE],
                              uint8_t plain[SM4_BLOCK_SIZE])
{
    uint32_t X[36];

    X[0] = load_be32(cipher);
    X[1] = load_be32(cipher + 4);
    X[2] = load_be32(cipher + 8);
    X[3] = load_be32(cipher + 12);

    for (int i = 0; i < 32; i++) {
        X[i+4] = X[i] ^ sm4_t(X[i+1] ^ X[i+2] ^ X[i+3] ^ skey->rk[31 - i]);
    }

    store_be32(plain,      X[35]);
    store_be32(plain + 4,  X[34]);
    store_be32(plain + 8,  X[33]);
    store_be32(plain + 12, X[32]);
}

/* ========================================================================
 *  ECB 模式
 * ======================================================================== */

int sm4_ecb_encrypt(const sm4_key_t *skey,
                    const uint8_t *plain, size_t pt_len,
                    uint8_t *cipher)
{
    if (!skey || !plain || !cipher) return CRYPTO_ERR_NULL_PTR;
    if (pt_len == 0 || (pt_len % SM4_BLOCK_SIZE) != 0) return CRYPTO_ERR_BAD_LENGTH;

    for (size_t i = 0; i < pt_len; i += SM4_BLOCK_SIZE) {
        sm4_encrypt_block(skey, plain + i, cipher + i);
    }
    return CRYPTO_SUCCESS;
}

int sm4_ecb_decrypt(const sm4_key_t *skey,
                    const uint8_t *cipher, size_t ct_len,
                    uint8_t *plain)
{
    if (!skey || !cipher || !plain) return CRYPTO_ERR_NULL_PTR;
    if (ct_len == 0 || (ct_len % SM4_BLOCK_SIZE) != 0) return CRYPTO_ERR_BAD_LENGTH;

    for (size_t i = 0; i < ct_len; i += SM4_BLOCK_SIZE) {
        sm4_decrypt_block(skey, cipher + i, plain + i);
    }
    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  CBC 模式
 * ======================================================================== */

int sm4_cbc_encrypt(const sm4_key_t *skey,
                    const uint8_t iv[SM4_BLOCK_SIZE],
                    const uint8_t *plain, size_t pt_len,
                    uint8_t *cipher)
{
    if (!skey || !iv || !plain || !cipher) return CRYPTO_ERR_NULL_PTR;
    if (pt_len == 0 || (pt_len % SM4_BLOCK_SIZE) != 0) return CRYPTO_ERR_BAD_LENGTH;

    uint8_t block[SM4_BLOCK_SIZE];
    memcpy(block, iv, SM4_BLOCK_SIZE);

    for (size_t i = 0; i < pt_len; i += SM4_BLOCK_SIZE) {
        for (int j = 0; j < SM4_BLOCK_SIZE; j++) {
            block[j] ^= plain[i + j];
        }
        sm4_encrypt_block(skey, block, cipher + i);
        memcpy(block, cipher + i, SM4_BLOCK_SIZE);
    }
    return CRYPTO_SUCCESS;
}

int sm4_cbc_decrypt(const sm4_key_t *skey,
                    const uint8_t iv[SM4_BLOCK_SIZE],
                    const uint8_t *cipher, size_t ct_len,
                    uint8_t *plain)
{
    if (!skey || !iv || !cipher || !plain) return CRYPTO_ERR_NULL_PTR;
    if (ct_len == 0 || (ct_len % SM4_BLOCK_SIZE) != 0) return CRYPTO_ERR_BAD_LENGTH;

    uint8_t block[SM4_BLOCK_SIZE];
    uint8_t prev[SM4_BLOCK_SIZE];

    memcpy(prev, iv, SM4_BLOCK_SIZE);

    for (size_t i = 0; i < ct_len; i += SM4_BLOCK_SIZE) {
        sm4_decrypt_block(skey, cipher + i, block);
        for (int j = 0; j < SM4_BLOCK_SIZE; j++) {
            plain[i + j] = block[j] ^ prev[j];
        }
        memcpy(prev, cipher + i, SM4_BLOCK_SIZE);
    }
    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  GCM 模式 (GF(2^128) 乘法 + CTR 模式)
 * ========================================================================
 * GCM 使用 GHASH 作为认证机制, 底层调用 SM4 ECB 加密。
 * 参考 NIST SP 800-38D.
 */

/* GF(2^128) 乘法 - 用于 GHASH */
static void gf128_mul(uint8_t r[16], const uint8_t a[16], const uint8_t b[16])
{
    uint8_t Z[16] = {0};
    uint8_t V[16];
    memcpy(V, b, 16);

    for (int i = 0; i < 128; i++) {
        int byte_idx = i / 8;
        int bit_idx  = 7 - (i % 8);

        if ((a[byte_idx] >> bit_idx) & 1) {
            for (int j = 0; j < 16; j++) Z[j] ^= V[j];
        }

        /* V = V >> 1 (right shift), with polynomial feedback */
        uint8_t lsb = V[15] & 1;
        for (int j = 15; j > 0; j--) V[j] = (V[j] >> 1) | (V[j-1] << 7);
        V[0] >>= 1;

        if (lsb) {
            V[0] ^= 0xE1;  /* R = 0xE1 << 120 */
        }
    }
    memcpy(r, Z, 16);
}

/* GHASH: 对输入块链计算哈希 */
static void ghash(uint8_t result[16], const uint8_t H[16],
                  const uint8_t *aad, size_t aad_len,
                  const uint8_t *cipher, size_t ct_len)
{
    uint8_t Y[16] = {0};
    uint8_t block[16];

    /* 处理 AAD */
    size_t pos = 0;
    while (pos + 16 <= aad_len) {
        for (int j = 0; j < 16; j++) Y[j] ^= aad[pos + j];
        gf128_mul(Y, Y, H);
        pos += 16;
    }
    /* 处理 AAD 尾部 (如有) */
    if (pos < aad_len) {
        memset(block, 0, 16);
        memcpy(block, aad + pos, aad_len - pos);
        for (int j = 0; j < 16; j++) Y[j] ^= block[j];
        gf128_mul(Y, Y, H);
    }

    /* 处理密文 */
    pos = 0;
    while (pos + 16 <= ct_len) {
        for (int j = 0; j < 16; j++) Y[j] ^= cipher[pos + j];
        gf128_mul(Y, Y, H);
        pos += 16;
    }
    if (pos < ct_len) {
        memset(block, 0, 16);
        memcpy(block, cipher + pos, ct_len - pos);
        for (int j = 0; j < 16; j++) Y[j] ^= block[j];
        gf128_mul(Y, Y, H);
    }

    /* 处理长度块: [len(AAD)*8 || len(CT)*8] (big-endian, 各 64 位) */
    memset(block, 0, 16);
    uint64_t aad_bits = (uint64_t)aad_len * 8;
    uint64_t ct_bits  = (uint64_t)ct_len  * 8;
    store_be64(block,      aad_bits);
    store_be64(block + 8,  ct_bits);
    for (int j = 0; j < 16; j++) Y[j] ^= block[j];
    gf128_mul(Y, Y, H);

    memcpy(result, Y, 16);
}

/* GCM 增量计数器 */
static void gcm_incr(uint8_t *counter, size_t len)
{
    for (size_t i = len; i > 0; i--) {
        if (++counter[i-1] != 0) break;
    }
}

/* ========================================================================
 *  SM4-GCM 加密
 * ======================================================================== */
int sm4_gcm_encrypt(const uint8_t key[SM4_KEY_SIZE],
                    const uint8_t *iv, size_t iv_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *plaintext, size_t pt_len,
                    uint8_t *ciphertext,
                    uint8_t *tag, size_t tag_len)
{
    if (!key || !iv || !plaintext || !ciphertext || !tag) return CRYPTO_ERR_NULL_PTR;
    if (iv_len == 0) return CRYPTO_ERR_BAD_LENGTH;

    sm4_key_t skey;
    int ret = sm4_set_key(key, &skey);
    if (ret != CRYPTO_SUCCESS) return ret;

    /* H = SM4(0^128) */
    uint8_t H[16] = {0};
    sm4_encrypt_block(&skey, H, H);

    /* J0 构造 */
    uint8_t J0[16];
    if (iv_len == 12) {
        memcpy(J0, iv, 12);
        J0[12] = J0[13] = J0[14] = 0;
        J0[15] = 1;
    } else {
        memset(J0, 0, 16);
        /* GHASH(H, {}, IV)  — 但使用空 AAD 和 IV 作为数据 */
        /* 简化: 对于非标准 IV 长度, 使用内部 GHASH */
        ghash(J0, H, NULL, 0, iv, iv_len);
    }

    /* CTR 模式加密 */
    uint8_t counter[16];
    memcpy(counter, J0, 16);
    uint8_t keystream[16];
    size_t pos = 0;

    while (pos < pt_len) {
        gcm_incr(counter + 12, 4);
        sm4_encrypt_block(&skey, counter, keystream);
        size_t todo = (pt_len - pos < 16) ? (pt_len - pos) : 16;
        for (size_t j = 0; j < todo; j++) {
            ciphertext[pos + j] = plaintext[pos + j] ^ keystream[j];
        }
        pos += todo;
    }

    /* GHASH 计算认证标签: GHASH(H, AAD, Ciphertext) */
    uint8_t S[16];
    ghash(S, H, aad, aad_len, ciphertext, pt_len);

    /* T = GHASH ⊕ E(K, J0) */
    uint8_t EK_J0[16];
    sm4_encrypt_block(&skey, J0, EK_J0);
    for (size_t j = 0; j < tag_len; j++) {
        tag[j] = S[j] ^ EK_J0[j];
    }

    crypto_secure_zero(&skey, sizeof(skey));
    crypto_secure_zero(H, sizeof(H));
    crypto_secure_zero(J0, sizeof(J0));
    crypto_secure_zero(counter, sizeof(counter));
    crypto_secure_zero(keystream, sizeof(keystream));

    return CRYPTO_SUCCESS;
}

/* ========================================================================
 *  SM4-GCM 解密 (验证 + 解密)
 * ======================================================================== */
int sm4_gcm_decrypt(const uint8_t key[SM4_KEY_SIZE],
                    const uint8_t *iv, size_t iv_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *tag, size_t tag_len,
                    uint8_t *plaintext)
{
    if (!key || !iv || !ciphertext || !tag || !plaintext) return CRYPTO_ERR_NULL_PTR;
    if (iv_len == 0 || tag_len == 0) return CRYPTO_ERR_BAD_LENGTH;

    sm4_key_t skey;
    int ret = sm4_set_key(key, &skey);
    if (ret != CRYPTO_SUCCESS) return ret;

    /* H = SM4(0^128) */
    uint8_t H[16] = {0};
    sm4_encrypt_block(&skey, H, H);

    /* J0 */
    uint8_t J0[16];
    if (iv_len == 12) {
        memcpy(J0, iv, 12);
        J0[12] = J0[13] = J0[14] = 0;
        J0[15] = 1;
    } else {
        ghash(J0, H, NULL, 0, iv, iv_len);
    }

    /* 验证标签 */
    uint8_t S[16];
    ghash(S, H, aad, aad_len, ciphertext, ct_len);

    uint8_t EK_J0[16];
    sm4_encrypt_block(&skey, J0, EK_J0);
    for (size_t j = 0; j < tag_len; j++) {
        S[j] ^= EK_J0[j];
    }

    /* 常数时间标签比较 */
    uint8_t diff = 0;
    for (size_t j = 0; j < tag_len; j++) {
        diff |= (S[j] ^ tag[j]);
    }
    if (diff != 0) {
        crypto_secure_zero(&skey, sizeof(skey));
        return CRYPTO_ERR_VERIFY_FAILED;
    }

    /* CTR 解密 */
    uint8_t counter[16];
    memcpy(counter, J0, 16);
    uint8_t keystream[16];
    size_t pos = 0;

    while (pos < ct_len) {
        gcm_incr(counter + 12, 4);
        sm4_encrypt_block(&skey, counter, keystream);
        size_t todo = (ct_len - pos < 16) ? (ct_len - pos) : 16;
        for (size_t j = 0; j < todo; j++) {
            plaintext[pos + j] = ciphertext[pos + j] ^ keystream[j];
        }
        pos += todo;
    }

    crypto_secure_zero(&skey, sizeof(skey));
    crypto_secure_zero(H, sizeof(H));
    crypto_secure_zero(J0, sizeof(J0));
    crypto_secure_zero(counter, sizeof(counter));
    crypto_secure_zero(keystream, sizeof(keystream));

    return CRYPTO_SUCCESS;
}
