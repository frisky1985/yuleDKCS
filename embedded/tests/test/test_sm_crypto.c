/**
 * @file test_sm_crypto.c
 * @brief 国密算法 (SM2/SM3/SM4) 与密码引擎标准测试向量单元测试
 *
 * 覆盖:
 *   - sm3.c: GB/T 32905 标准向量 ('abc'/空串/64B/65B 跨块), 增量 update
 *   - sm4.c: GB/T 32907 标准向量 (ECB/CBC), GCM 往返, 错误分支
 *   - sm2.c: GB/T 32918.2 A.2 标准验签向量, 签名-验签往返, 篡改检测,
 *            密钥交换函数级测试
 *   - crypto_engine.c: 算法选择/哈希/HMAC/KDF/AES-GCM/SM4-GCM/签名分发
 *   - crypto_utils.c: bn256/fp/fn/ec_point 已知值
 *
 * 说明: crypto_utils.c 的 crypto_random_bytes 为 weak 符号, 本测试在
 * 独立构建 (非 TEST_LIB_MODE) 下提供确定性强符号覆盖; 合并构建链接
 * ccc_protocol/src/security/crypto_random.c 的强符号, 需先调用
 * crypto_random_init() 使其可用。
 */

#include "unity.h"
#include "sm3.h"
#include "sm4.h"
#include "sm2.h"
#include "crypto_engine.h"
#include "crypto_utils.h"

#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* ========================================================================
 * 确定性随机数 (独立构建覆盖 weak crypto_random_bytes)
 * ======================================================================== */
#ifndef TEST_LIB_MODE
int crypto_random_bytes(uint8_t *buf, size_t len)
{
    static uint64_t seed = 0x123456789ABCDEFULL;
    size_t i;
    for (i = 0; i < len; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = (uint8_t)(seed >> 33);
    }
    return 0;
}
#else
/* 合并构建: crypto_random.c 提供强符号, 需先初始化 */
extern int crypto_random_init(void);
#endif /* TEST_LIB_MODE */

/* ========================================================================
 * 标准测试向量
 * ======================================================================== */
static const uint8_t V_SM3_ABC[32] = {
    0x66,0xC7,0xF0,0xF4,0x62,0xEE,0xED,0xD9,0xD1,0xF2,0xD4,0x6B,0xDC,0x10,0xE4,0xE2,
    0x41,0x67,0xC4,0x87,0x5C,0xF2,0xF7,0xA2,0x29,0x7D,0xA0,0x2B,0x8F,0x4B,0xA8,0xE0};
static const uint8_t V_SM3_EMPTY[32] = {
    0x1A,0xB2,0x1D,0x83,0x55,0xCF,0xA1,0x7F,0x8E,0x61,0x19,0x48,0x31,0xE8,0x1A,0x8F,
    0x22,0xBE,0xC8,0xC7,0x28,0xFE,0xFB,0x74,0x7E,0xD0,0x35,0xEB,0x50,0x82,0xAA,0x2B};
static const uint8_t V_SM3_64A[32] = {
    0x61,0x6E,0xC4,0x33,0xC3,0x59,0xE7,0xC2,0xB1,0x9F,0x36,0x0E,0x2B,0x8F,0x2A,0x1B,
    0x6E,0x9E,0xD7,0x6B,0x8D,0xC1,0xA7,0xD2,0x07,0xB3,0x1A,0x53,0x41,0xC6,0x11,0xE9};
static const uint8_t V_SM3_65A[32] = {
    0x3D,0x1D,0x94,0xAF,0xA2,0x38,0xEC,0x3E,0x2B,0xBC,0x20,0xAD,0x50,0x47,0x02,0xB2,
    0x4C,0x16,0xF2,0x88,0x9C,0x94,0x97,0x3F,0x2F,0x8D,0xA3,0x52,0x6C,0x44,0xE4,0xBC};

/* SM4: 密钥 = 明文 = 0123456789abcdeffedcba9876543210 */
static const uint8_t V_SM4_KEY[16] = {
    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10};
static const uint8_t V_SM4_PT[16] = {
    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10};
static const uint8_t V_SM4_CT[16] = {
    0x68,0x1E,0xDF,0x34,0xD2,0x06,0x96,0x5E,0x86,0xB3,0xE9,0x4F,0x53,0x6E,0x42,0x46};
static const uint8_t V_SM4_CBC_IV[16] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F};
/* CBC 1 块: 明文 V_SM4_PT + IV V_SM4_CBC_IV */
static const uint8_t V_SM4_CBC_CT1[16] = {
    0xA9,0xA2,0x68,0x88,0x3A,0x33,0x63,0x15,0xBA,0xC0,0xC9,0xC9,0xFF,0x35,0x0A,0xB1};
/* CBC 2 块: 明文 = V_SM4_PT || V_SM4_PT */
static const uint8_t V_SM4_CBC_CT2[32] = {
    0xA9,0xA2,0x68,0x88,0x3A,0x33,0x63,0x15,0xBA,0xC0,0xC9,0xC9,0xFF,0x35,0x0A,0xB1,
    0xB2,0x36,0xA4,0xA8,0x56,0x16,0xD4,0xAA,0xBF,0x0A,0x83,0x55,0x5C,0x7D,0x41,0x15};

/* SHA-256('abc') / HMAC-SHA256 参考 */
static const uint8_t V_SHA256_ABC[32] = {
    0xBA,0x78,0x16,0xBF,0x8F,0x01,0xCF,0xEA,0x41,0x41,0x40,0xDE,0x5D,0xAE,0x22,0x23,
    0xB0,0x03,0x61,0xA3,0x96,0x17,0x7A,0x9C,0xB4,0x10,0xFF,0x61,0xF2,0x00,0x15,0xAD};
/* HMAC-SHA256(key="key", "The quick brown fox jumps over the lazy dog") */
static const uint8_t V_HMAC_FOX[32] = {
    0xF7,0xBC,0x83,0xF4,0x30,0x53,0x84,0x24,0xB1,0x32,0x98,0xE6,0xAA,0x6F,0xB1,0x43,
    0xEF,0x4D,0x59,0xA1,0x49,0x46,0x17,0x59,0x97,0x47,0x9D,0xBC,0x2D,0x1A,0x3C,0xD8};

/* SM2 GB/T 32918.2 A.2 标准向量:
 * d = 3945208F7B2144B13F36E38AC6D39F95889393692860B51A42FB81EF4DF7C5B8
 * P = 04 09F9DF31...5020 CCEA490C...AD13, M = "message digest"
 * r = F5A03B0648D2C4630EEAC513E1BB81A15944DA3827D5B74143AC7EACEEE720B3
 * s = B1B6AA29DF212FD8763182BC0D421CA1BB9038FD1F7F42D4840B69C485BBC1AA */
static const uint8_t V_SM2_D[32] = {
    0x39,0x45,0x20,0x8F,0x7B,0x21,0x44,0xB1,0x3F,0x36,0xE3,0x8A,
    0xC6,0xD3,0x9F,0x95,0x88,0x93,0x93,0x69,0x28,0x60,0xB5,0x1A,
    0x42,0xFB,0x81,0xEF,0x4D,0xF7,0xC5,0xB8};
static const uint8_t V_SM2_P[64] = {
    0x09,0xF9,0xDF,0x31,0x1E,0x54,0x21,0xA1,0x50,0xDD,0x7D,0x16,0x1E,0x4B,0xC5,0xC6,
    0x72,0x17,0x9F,0xAD,0x18,0x33,0xFC,0x07,0x6B,0xB0,0x8F,0xF3,0x56,0xF3,0x50,0x20,
    0xCC,0xEA,0x49,0x0C,0xE2,0x67,0x75,0xA5,0x2D,0xC6,0xEA,0x71,0x8C,0xC1,0xAA,0x60,
    0x0A,0xED,0x05,0xFB,0xF3,0x5E,0x08,0x4A,0x66,0x32,0xF6,0x07,0x2D,0xA9,0xAD,0x13};
static const uint8_t V_SM2_SIG[64] = {
    0xF5,0xA0,0x3B,0x06,0x48,0xD2,0xC4,0x63,0x0E,0xEA,0xC5,0x13,0xE1,0xBB,0x81,0xA1,
    0x59,0x44,0xDA,0x38,0x27,0xD5,0xB7,0x41,0x43,0xAC,0x7E,0xAC,0xEE,0xE7,0x20,0xB3,
    0xB1,0xB6,0xAA,0x29,0xDF,0x21,0x2F,0xD8,0x76,0x31,0x82,0xBC,0x0D,0x42,0x1C,0xA1,
    0xBB,0x90,0x38,0xFD,0x1F,0x7F,0x42,0xD4,0x84,0x0B,0x69,0xC4,0x85,0xBB,0xC1,0xAA};

/* 2G (Python/OpenSSL 独立验证) */
static const uint8_t V_2G_X[32] = {
    0x56,0xCE,0xFD,0x60,0xD7,0xC8,0x7C,0x00,0x0D,0x58,0xEF,0x57,0xFA,0x73,0xBA,0x4D,
    0x9C,0x0D,0xFA,0x08,0xC0,0x8A,0x73,0x31,0x49,0x5C,0x2E,0x1D,0xA3,0xF2,0xBD,0x52};
static const uint8_t V_2G_Y[32] = {
    0x31,0xB7,0xE7,0xE6,0xCC,0x81,0x89,0xF6,0x68,0x53,0x5C,0xE0,0xF8,0xEA,0xF1,0xBD,
    0x6D,0xE8,0x4C,0x18,0x2F,0x6C,0x8E,0x71,0x6F,0x78,0x0D,0x3A,0x97,0x0A,0x23,0xC3};

/* ========================================================================
 * SM3 测试
 * ======================================================================== */
void test_sm3_standard_vectors(void)
{
    uint8_t h[32];
    const uint8_t data64[64] = { [0 ... 63] = 'a' };
    const uint8_t data65[65] = { [0 ... 64] = 'a' };

    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_hash((const uint8_t *)"abc", 3, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SM3_ABC, h, 32);

    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_hash((const uint8_t *)"", 0, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SM3_EMPTY, h, 32);

    /* 64 字节 (恰好一块, 需填充跨块) */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_hash(data64, 64, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SM3_64A, h, 32);

    /* 65 字节 (跨块) */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_hash(data65, 65, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SM3_65A, h, 32);
}

void test_sm3_incremental(void)
{
    sm3_ctx_t ctx;
    uint8_t h1[32], h2[32];
    const uint8_t data[100] = { [0 ... 99] = 0x5A };

    /* 一次性 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_hash(data, 100, h1));
    /* 分片: 1 + 7 + 64 + 28 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_init(&ctx));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_update(&ctx, data, 1));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_update(&ctx, data + 1, 7));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_update(&ctx, data + 8, 64));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_update(&ctx, data + 72, 28));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_final(&ctx, h2));
    TEST_ASSERT_EQUAL_MEMORY(h1, h2, 32);

    /* 空输入 incremental */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_init(&ctx));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm3_final(&ctx, h2));
    TEST_ASSERT_EQUAL_MEMORY(V_SM3_EMPTY, h2, 32);
}

void test_sm3_error_branches(void)
{
    sm3_ctx_t ctx;
    uint8_t h[32];
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm3_init(NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm3_update(NULL, h, 1));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm3_update(&ctx, NULL, 1));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm3_final(NULL, h));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm3_final(&ctx, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm3_hash(NULL, 1, h));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm3_hash((const uint8_t *)"a", 1, NULL));
}

/* ========================================================================
 * SM4 测试
 * ======================================================================== */
void test_sm4_ecb_standard_vector(void)
{
    sm4_key_t sk;
    uint8_t out[16];
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_set_key(V_SM4_KEY, &sk));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_ecb_encrypt(&sk, V_SM4_PT, 16, out));
    TEST_ASSERT_EQUAL_MEMORY(V_SM4_CT, out, 16);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_ecb_decrypt(&sk, out, 16, out));
    TEST_ASSERT_EQUAL_MEMORY(V_SM4_PT, out, 16);
}

void test_sm4_ecb_multiblock_roundtrip(void)
{
    sm4_key_t sk;
    uint8_t pt[48];
    uint8_t ct[48];
    uint8_t back[48];
    int i;
    for (i = 0; i < 48; i++) pt[i] = 0x5A;   /* 3 个相同明文块 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_set_key(V_SM4_KEY, &sk));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_ecb_encrypt(&sk, pt, 48, ct));
    /* 相同明文块 → 相同密文块 (ECB 特性) */
    TEST_ASSERT_EQUAL_MEMORY(ct, ct + 16, 16);
    TEST_ASSERT_EQUAL_MEMORY(ct, ct + 32, 16);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_ecb_decrypt(&sk, ct, 48, back));
    TEST_ASSERT_EQUAL_MEMORY(pt, back, 48);
}

void test_sm4_ecb_error_branches(void)
{
    sm4_key_t sk;
    uint8_t out[16];
    uint8_t bad_len[15];
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_set_key(NULL, &sk));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_set_key(V_SM4_KEY, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_ecb_encrypt(NULL, V_SM4_PT, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_ecb_encrypt(&sk, NULL, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_ecb_encrypt(&sk, V_SM4_PT, 16, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_ecb_decrypt(&sk, NULL, 16, out));
    /* 非 16 倍数长度 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_BAD_LENGTH, sm4_ecb_encrypt(&sk, bad_len, 15, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_BAD_LENGTH, sm4_ecb_decrypt(&sk, bad_len, 15, out));
}

void test_sm4_cbc_standard_vector(void)
{
    sm4_key_t sk;
    uint8_t pt2[32];
    uint8_t ct[32];
    uint8_t back[32];
    memcpy(pt2, V_SM4_PT, 16);
    memcpy(pt2 + 16, V_SM4_PT, 16);

    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_set_key(V_SM4_KEY, &sk));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_cbc_encrypt(&sk, V_SM4_CBC_IV, V_SM4_PT, 16, ct));
    TEST_ASSERT_EQUAL_MEMORY(V_SM4_CBC_CT1, ct, 16);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_cbc_encrypt(&sk, V_SM4_CBC_IV, pt2, 32, ct));
    TEST_ASSERT_EQUAL_MEMORY(V_SM4_CBC_CT2, ct, 32);

    /* 往返 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm4_cbc_decrypt(&sk, V_SM4_CBC_IV, ct, 32, back));
    TEST_ASSERT_EQUAL_MEMORY(pt2, back, 32);

    /* 相同明文块 → CBC 下密文块不同 (链接生效) */
    TEST_ASSERT_FALSE(memcmp(ct, ct + 16, 16) == 0);
}

void test_sm4_cbc_error_branches(void)
{
    sm4_key_t sk;
    uint8_t out[16];
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_cbc_encrypt(NULL, V_SM4_CBC_IV, V_SM4_PT, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_cbc_encrypt(&sk, NULL, V_SM4_PT, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_cbc_encrypt(&sk, V_SM4_CBC_IV, NULL, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_cbc_encrypt(&sk, V_SM4_CBC_IV, V_SM4_PT, 16, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_BAD_LENGTH, sm4_cbc_encrypt(&sk, V_SM4_CBC_IV, V_SM4_PT, 15, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm4_cbc_decrypt(&sk, V_SM4_CBC_IV, NULL, 16, out));
}

void test_sm4_gcm_roundtrip(void)
{
    uint8_t ct[48];
    uint8_t back[48];
    uint8_t tag[16];
    uint8_t tag_bad[16];
    uint8_t pt[48];
    const uint8_t iv[12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
    const uint8_t aad[5] = { 1,2,3,4,5 };
    int i;
    for (i = 0; i < 48; i++) pt[i] = (uint8_t)(0x80 + i);

    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        sm4_gcm_encrypt(V_SM4_KEY, iv, 12, aad, 5, pt, 48, ct, tag, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        sm4_gcm_decrypt(V_SM4_KEY, iv, 12, aad, 5, ct, 48, tag, 16, back));
    TEST_ASSERT_EQUAL_MEMORY(pt, back, 48);

    /* 错误 tag 必须失败 */
    memcpy(tag_bad, tag, 16);
    tag_bad[0] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        sm4_gcm_decrypt(V_SM4_KEY, iv, 12, aad, 5, ct, 48, tag_bad, 16, back));
    /* 错误 AAD 必须失败 */
    tag_bad[0] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        sm4_gcm_decrypt(V_SM4_KEY, iv, 12, aad, 4, ct, 48, tag, 16, back));
}

void test_sm4_gcm_error_branches(void)
{
    uint8_t out[16];
    uint8_t tag[16];
    uint8_t pt[16];
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_encrypt(NULL, V_SM4_CBC_IV, 12, NULL, 0, pt, 16, out, tag, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_encrypt(V_SM4_KEY, V_SM4_CBC_IV, 12, NULL, 0, NULL, 16, out, tag, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_encrypt(V_SM4_KEY, V_SM4_CBC_IV, 12, NULL, 0, pt, 16, NULL, tag, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_encrypt(V_SM4_KEY, V_SM4_CBC_IV, 12, NULL, 0, pt, 16, out, NULL, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_decrypt(NULL, V_SM4_CBC_IV, 12, NULL, 0, pt, 16, tag, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_decrypt(V_SM4_KEY, V_SM4_CBC_IV, 12, NULL, 0, NULL, 16, tag, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_decrypt(V_SM4_KEY, V_SM4_CBC_IV, 12, NULL, 0, pt, 16, NULL, 16, out));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm4_gcm_decrypt(V_SM4_KEY, V_SM4_CBC_IV, 12, NULL, 0, pt, 16, tag, 16, NULL));
}

/* ========================================================================
 * crypto_engine 测试
 * ======================================================================== */
void test_engine_algo_selection(void)
{
    crypto_algo_e ecc;
    hash_algo_e hash;
    sym_algo_e sym;

    crypto_engine_deinit();
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, crypto_engine_init());

    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_INVALID_INPUT,
        crypto_engine_set_algo((crypto_algo_e)99, HASH_ALGO_SHA256, SYM_ALGO_AES256_GCM));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_INVALID_INPUT,
        crypto_engine_set_algo(CRYPTO_ALGO_SM2, (hash_algo_e)99, SYM_ALGO_AES256_GCM));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_INVALID_INPUT,
        crypto_engine_set_algo(CRYPTO_ALGO_SM2, HASH_ALGO_SM3, (sym_algo_e)99));

    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_engine_set_algo(CRYPTO_ALGO_SM2, HASH_ALGO_SM3, SYM_ALGO_SM4_GCM));
    crypto_engine_get_algo(&ecc, &hash, &sym);
    TEST_ASSERT_EQUAL_INT(CRYPTO_ALGO_SM2, ecc);
    TEST_ASSERT_EQUAL_INT(HASH_ALGO_SM3, hash);
    TEST_ASSERT_EQUAL_INT(SYM_ALGO_SM4_GCM, sym);

    crypto_engine_deinit();
}

void test_engine_hash(void)
{
    uint8_t h[32];
    crypto_engine_deinit();
    crypto_engine_init();
    /* 默认 SHA-256 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, crypto_hash((const uint8_t *)"abc", 3, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SHA256_ABC, h, 32);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, crypto_sha256((const uint8_t *)"abc", 3, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SHA256_ABC, h, 32);
    /* SM3 直调 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, crypto_sm3((const uint8_t *)"abc", 3, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SM3_ABC, h, 32);
    /* 切到 SM3 后 crypto_hash 走 SM3 */
    crypto_engine_set_algo(CRYPTO_ALGO_SM2, HASH_ALGO_SM3, SYM_ALGO_SM4_GCM);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, crypto_hash((const uint8_t *)"abc", 3, h));
    TEST_ASSERT_EQUAL_MEMORY(V_SM3_ABC, h, 32);
    /* NULL 分支 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_hash(NULL, 3, h));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_hash((const uint8_t *)"abc", 3, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_sha256(NULL, 3, h));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_sm3(NULL, 3, h));
    crypto_engine_deinit();
}

void test_engine_hmac_kdf(void)
{
    uint8_t mac[32];
    uint8_t out1[32], out2[32];
    const uint8_t salt1[8] = { 1,2,3,4,5,6,7,8 };
    const uint8_t salt2[8] = { 9,9,9,9,9,9,9,9 };
    const uint8_t key[3] = { 'k','e','y' };
    const char *fox = "The quick brown fox jumps over the lazy dog";

    crypto_engine_deinit();
    crypto_engine_init();
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_hmac_sha256(key, 3, (const uint8_t *)fox, strlen(fox), mac));
    TEST_ASSERT_EQUAL_MEMORY(V_HMAC_FOX, mac, 32);

    /* KDF 确定性 + 盐敏感性 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_kdf(key, 3, salt1, 8, (const uint8_t *)"info", 4, out1, 32));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_kdf(key, 3, salt1, 8, (const uint8_t *)"info", 4, out2, 32));
    TEST_ASSERT_EQUAL_MEMORY(out1, out2, 32);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_kdf(key, 3, salt2, 8, (const uint8_t *)"info", 4, out2, 32));
    TEST_ASSERT_FALSE(memcmp(out1, out2, 32) == 0);

    /* 错误分支 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_hmac_sha256(NULL, 3, (const uint8_t *)fox, 3, mac));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_hmac_sha256(key, 3, NULL, 3, mac));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_hmac_sha256(key, 3, (const uint8_t *)fox, 3, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_kdf(NULL, 3, salt1, 8, NULL, 0, out1, 32));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_kdf(key, 3, salt1, 8, NULL, 0, NULL, 32));
    crypto_engine_deinit();
}

void test_engine_aes_gcm(void)
{
    uint8_t key32[32];
    uint8_t ct[32];
    uint8_t back[32];
    uint8_t tag[16];
    uint8_t tag_bad[16];
    const uint8_t iv[12] = { 0,0,0,0,0,0,0,0,0,0,0,1 };
    uint8_t pt[32];
    int i;
    for (i = 0; i < 32; i++) {
        pt[i] = (uint8_t)i;
        key32[i] = (uint8_t)(0x10 + i);
    }

    crypto_engine_deinit();
    crypto_engine_init();
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_aes_gcm_encrypt(key32, 32, iv, 12, pt, 32, ct, tag, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_aes_gcm_decrypt(key32, 32, iv, 12, ct, 32, back, tag, 16));
    TEST_ASSERT_EQUAL_MEMORY(pt, back, 32);
    memcpy(tag_bad, tag, 16);
    tag_bad[1] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        crypto_aes_gcm_decrypt(key32, 32, iv, 12, ct, 32, back, tag_bad, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_aes_gcm_encrypt(NULL, 32, iv, 12, pt, 32, ct, tag, 16));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_aes_gcm_decrypt(key32, 32, iv, 12, ct, 32, NULL, tag, 16));
    crypto_engine_deinit();
}

void test_engine_sym_encrypt_dispatch(void)
{
    uint8_t key16[16];
    uint8_t key32[32];
    uint8_t ct[16];
    uint8_t back[16];
    uint8_t tag[16];
    const uint8_t iv[12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
    int i;
    for (i = 0; i < 16; i++) {
        key16[i] = V_SM4_KEY[i];
        key32[i] = (uint8_t)(0x20 + i);
        key32[16 + i] = (uint8_t)(0x30 + i);
    }

    crypto_engine_deinit();
    crypto_engine_init();

    /* SM4-GCM 模式 (经 crypto_engine 分发) */
    crypto_engine_set_algo(CRYPTO_ALGO_SM2, HASH_ALGO_SM3, SYM_ALGO_SM4_GCM);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_encrypt(key16, 16, iv, 12, NULL, 0, V_SM4_PT, 16, ct, tag));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_decrypt(key16, 16, iv, 12, NULL, 0, ct, 16, tag, back));
    TEST_ASSERT_EQUAL_MEMORY(V_SM4_PT, back, 16);
    tag[0] ^= 0x80;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        crypto_decrypt(key16, 16, iv, 12, NULL, 0, ct, 16, tag, back));
    tag[0] ^= 0x80;

    /* AES-256-GCM 模式 (默认) */
    crypto_engine_set_algo(CRYPTO_ALGO_ECC_P256, HASH_ALGO_SHA256, SYM_ALGO_AES256_GCM);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_encrypt(key32, 32, iv, 12, NULL, 0, V_SM4_PT, 16, ct, tag));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_decrypt(key32, 32, iv, 12, NULL, 0, ct, 16, tag, back));
    TEST_ASSERT_EQUAL_MEMORY(V_SM4_PT, back, 16);

    /* 错误分支 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_encrypt(NULL, 16, iv, 12, NULL, 0, V_SM4_PT, 16, ct, tag));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_decrypt(key16, 16, iv, 12, NULL, 0, NULL, 16, tag, back));
    crypto_engine_deinit();
}

/* ========================================================================
 * SM2 测试
 * ======================================================================== */
void test_sm2_standard_vector_verify(void)
{
    const char *msg = "message digest";
    int ret = sm2_verify(V_SM2_P, (const uint8_t *)msg, strlen(msg), NULL, 0, V_SM2_SIG);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, ret);
}

void test_sm2_sign_verify_roundtrip(void)
{
    const char *msg = "message digest";
    uint8_t sig[64];
    uint8_t sig_bad[64];

#ifndef TEST_LIB_MODE
    /* 独立构建: LCG 确定性随机 */
#else
    (void)crypto_random_init();
#endif
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        sm2_sign(V_SM2_D, (const uint8_t *)msg, strlen(msg), NULL, 0, V_SM2_P, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        sm2_verify(V_SM2_P, (const uint8_t *)msg, strlen(msg), NULL, 0, sig));

    /* 篡改签名 → 拒绝 */
    memcpy(sig_bad, sig, 64);
    sig_bad[5] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        sm2_verify(V_SM2_P, (const uint8_t *)msg, strlen(msg), NULL, 0, sig_bad));

    /* 篡改消息 → 拒绝 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        sm2_verify(V_SM2_P, (const uint8_t *)"message digesT", 14, NULL, 0, sig));
}

void test_sm2_sign_hash_roundtrip(void)
{
    uint8_t hash[32];
    uint8_t sig[64];
    uint8_t sig_bad[64];
    int i;
    for (i = 0; i < 32; i++) hash[i] = (uint8_t)(i * 7);

    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm2_sign_hash(V_SM2_D, hash, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS, sm2_verify_hash(V_SM2_P, hash, sig));

    memcpy(sig_bad, sig, 64);
    sig_bad[30] ^= 0xFF;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED, sm2_verify_hash(V_SM2_P, hash, sig_bad));

    /* r/s 越界或为 0 拒绝 */
    memset(sig_bad, 0, 64);
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED, sm2_verify_hash(V_SM2_P, hash, sig_bad));
    memset(sig_bad, 0xFF, 64);
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED, sm2_verify_hash(V_SM2_P, hash, sig_bad));
}

void test_sm2_error_branches(void)
{
    uint8_t sig[64];
    const char *msg = "message digest";
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_sign(NULL, (const uint8_t *)msg, strlen(msg), NULL, 0, V_SM2_P, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_sign(V_SM2_D, NULL, strlen(msg), NULL, 0, V_SM2_P, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_sign(V_SM2_D, (const uint8_t *)msg, strlen(msg), NULL, 0, NULL, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_sign(V_SM2_D, (const uint8_t *)msg, strlen(msg), NULL, 0, V_SM2_P, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_verify(NULL, (const uint8_t *)msg, strlen(msg), NULL, 0, V_SM2_SIG));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_verify(V_SM2_P, NULL, strlen(msg), NULL, 0, V_SM2_SIG));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_verify(V_SM2_P, (const uint8_t *)msg, strlen(msg), NULL, 0, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm2_sign_hash(NULL, V_SM3_ABC, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm2_sign_hash(V_SM2_D, NULL, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm2_verify_hash(NULL, V_SM3_ABC, V_SM2_SIG));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, sm2_verify_hash(V_SM2_P, NULL, V_SM2_SIG));
}

void test_sm2_key_exchange_apis(void)
{
    uint8_t eph_priv[32];
    uint8_t eph_pub[64];
    uint8_t secret1[32];
    uint8_t secret2[32];

#ifndef TEST_LIB_MODE
    /* 独立构建: LCG 确定性随机 */
#else
    (void)crypto_random_init();
#endif
    /* 发起方与响应方函数级: 均可成功产出 32 字节共享密钥 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        sm2_key_exchange_initiator(V_SM2_D, V_SM2_P, V_SM2_P,
                                   NULL, 0, NULL, 0,
                                   eph_priv, eph_pub, secret1));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        sm2_key_exchange_responder(V_SM2_D, V_SM2_P, V_SM2_P,
                                   NULL, 0, NULL, 0,
                                   eph_pub, secret2));

    /* NULL 分支 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_key_exchange_initiator(NULL, V_SM2_P, V_SM2_P, NULL, 0, NULL, 0,
                                   eph_priv, eph_pub, secret1));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_key_exchange_initiator(V_SM2_D, V_SM2_P, V_SM2_P, NULL, 0, NULL, 0,
                                   NULL, eph_pub, secret1));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_key_exchange_initiator(V_SM2_D, V_SM2_P, V_SM2_P, NULL, 0, NULL, 0,
                                   eph_priv, eph_pub, NULL));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_key_exchange_responder(NULL, V_SM2_P, V_SM2_P, NULL, 0, NULL, 0,
                                   eph_pub, secret2));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        sm2_key_exchange_responder(V_SM2_D, V_SM2_P, V_SM2_P, NULL, 0, NULL, 0,
                                   NULL, secret2));
}

/* ========================================================================
 * crypto_engine SM2 签名分发
 * ======================================================================== */
void test_engine_sm2_sign_dispatch(void)
{
    const char *msg = "message digest";
    uint8_t sig[64];
    size_t sig_len = 64;
    uint8_t sig_bad[64];

    crypto_engine_deinit();
    crypto_engine_init();
#ifndef TEST_LIB_MODE
    /* LCG */
#else
    (void)crypto_random_init();
#endif
    crypto_engine_set_algo(CRYPTO_ALGO_SM2, HASH_ALGO_SM3, SYM_ALGO_SM4_GCM);

    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_sign(V_SM2_D, 32, (const uint8_t *)msg, strlen(msg), sig, &sig_len));
    TEST_ASSERT_EQUAL_INT(64, sig_len);
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_verify(V_SM2_P, 64, (const uint8_t *)msg, strlen(msg), sig, sig_len));

    memcpy(sig_bad, sig, 64);
    sig_bad[0] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        crypto_verify(V_SM2_P, 64, (const uint8_t *)msg, strlen(msg), sig_bad, sig_len));

    /* 长度校验分支 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_BAD_LENGTH,
        crypto_sign(V_SM2_D, 16, (const uint8_t *)msg, strlen(msg), sig, &sig_len));
    sig_len = 16;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_BUF_OVERFLOW,
        crypto_sign(V_SM2_D, 32, (const uint8_t *)msg, strlen(msg), sig, &sig_len));
    sig_len = 64;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_BAD_LENGTH,
        crypto_verify(V_SM2_P, 32, (const uint8_t *)msg, strlen(msg), sig, sig_len));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_BAD_LENGTH,
        crypto_verify(V_SM2_P, 64, (const uint8_t *)msg, strlen(msg), sig, 32));

    /* NULL 分支 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_sign(NULL, 32, (const uint8_t *)msg, strlen(msg), sig, &sig_len));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_sign(V_SM2_D, 32, NULL, strlen(msg), sig, &sig_len));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_sign(V_SM2_D, 32, (const uint8_t *)msg, strlen(msg), NULL, &sig_len));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_verify(NULL, 64, (const uint8_t *)msg, strlen(msg), sig, 64));

    /* SM2 直调接口 */
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_sm2_sign(V_SM2_D, V_SM3_ABC, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_sm2_verify(V_SM2_P, V_SM3_ABC, sig));
    sig[3] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_VERIFY_FAILED,
        crypto_sm2_verify(V_SM2_P, V_SM3_ABC, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_sm2_sign(NULL, V_SM3_ABC, sig));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR, crypto_sm2_verify(NULL, V_SM3_ABC, sig));

    /* P-256 占位分支 */
    crypto_engine_set_algo(CRYPTO_ALGO_ECC_P256, HASH_ALGO_SHA256, SYM_ALGO_AES256_GCM);
    sig_len = 64;
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_UNSUPPORTED,
        crypto_sign(V_SM2_D, 32, (const uint8_t *)msg, strlen(msg), sig, &sig_len));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_UNSUPPORTED,
        crypto_verify(V_SM2_P, 64, (const uint8_t *)msg, strlen(msg), sig, 64));

    crypto_engine_deinit();
}

void test_engine_sm2_key_exchange_dispatch(void)
{
    uint8_t secret[32];
    crypto_engine_deinit();
    crypto_engine_init();
    TEST_ASSERT_EQUAL_INT(CRYPTO_SUCCESS,
        crypto_sm2_key_exchange(V_SM2_D, V_SM2_P, secret));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_sm2_key_exchange(NULL, V_SM2_P, secret));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_sm2_key_exchange(V_SM2_D, NULL, secret));
    TEST_ASSERT_EQUAL_INT(CRYPTO_ERR_NULL_PTR,
        crypto_sm2_key_exchange(V_SM2_D, V_SM2_P, NULL));
    crypto_engine_deinit();
}

/* ========================================================================
 * bn256 / 椭圆曲线 (crypto_utils.c) 已知值测试
 * ======================================================================== */
void test_bn256_known_values(void)
{
    bn256_t a, b, r;
    uint8_t buf[32];
    const uint8_t a_bytes[32] = {
        0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
        0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};
    const uint8_t b_bytes[32] = {
        0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
        0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0xF0,0xE0,0xD0,0xC0,0xB0,0xA0,0x90,0x80};
    /* fp_mul / fn_mul 已知值 (Python 独立验证) */
    const uint8_t fp_expected[32] = {
        0x67,0xC3,0xCE,0x54,0x03,0x1D,0x3A,0x83,0xE2,0x02,0xB1,0x9E,0x3D,0x6E,0x40,0xB7,
        0x29,0x66,0x90,0x93,0xBB,0x5C,0x20,0xF8,0xD9,0xFD,0xB9,0x9B,0x0A,0x06,0xA1,0x9A};
    const uint8_t fn_expected[32] = {
        0x6C,0x2F,0x71,0x48,0x4B,0x60,0x8B,0x39,0x46,0x48,0x49,0x9B,0x17,0x50,0x01,0xDE,
        0x6F,0x79,0xB5,0x22,0xCF,0x07,0x99,0x48,0x93,0x8C,0x0B,0x7A,0xA6,0x3B,0xD4,0xFD};

    bn256_from_bytes(&a, a_bytes);
    bn256_from_bytes(&b, b_bytes);

    /* from/to bytes 往返 */
    bn256_to_bytes(&a, buf);
    TEST_ASSERT_EQUAL_MEMORY(a_bytes, buf, 32);

    TEST_ASSERT_EQUAL_INT(-1, bn256_cmp(&a, &b));  /* a < b (MSB 0x12 < 0xFE) */
    TEST_ASSERT_EQUAL_INT(0, bn256_cmp(&a, &a));
    TEST_ASSERT_FALSE(bn256_is_zero(&a));
    TEST_ASSERT_FALSE(bn256_is_zero(&b));
    /* bn256_is_zero 直接验证 */
    bn256_t zero;
    bn256_set_word(&zero, 0);
    TEST_ASSERT_TRUE(bn256_is_zero(&zero));

    fp_mul(&r, &a, &b);
    bn256_to_bytes(&r, buf);
    TEST_ASSERT_EQUAL_MEMORY(fp_expected, buf, 32);

    fn_mul(&r, &a, &b);
    bn256_to_bytes(&r, buf);
    TEST_ASSERT_EQUAL_MEMORY(fn_expected, buf, 32);

    /* fp_inv: 5 * 5^-1 == 1 */
    bn256_t five, inv, one, chk;
    bn256_set_word(&five, 5);
    fp_inv(&inv, &five);
    fp_mul(&chk, &five, &inv);
    bn256_set_word(&one, 1);
    TEST_ASSERT_EQUAL_INT(0, bn256_cmp(&chk, &one));

    /* fp_add/fp_sub 往返 */
    fp_add(&r, &a, &b);
    fp_sub(&r, &r, &b);
    TEST_ASSERT_EQUAL_INT(0, bn256_cmp(&r, &a));
}

void test_ec_point_known_values(void)
{
    bn256_t k, x, y;
    uint8_t buf[32];

    /* 1*G == G */
    bn256_set_word(&k, 1);
    ec_point_mul_base(&x, &y, &k);
    bn256_t gx, gy;
    bn256_from_bytes(&gx, (const uint8_t[]){
        0x32,0xC4,0xAE,0x2C,0x1F,0x19,0x81,0x19,0x5F,0x99,0x04,0x46,0x6A,0x39,0xC9,0x94,
        0x8F,0xE3,0x0B,0xBF,0xF2,0x66,0x0B,0xE1,0x71,0x5A,0x45,0x89,0x33,0x4C,0x74,0xC7});
    TEST_ASSERT_EQUAL_INT(0, bn256_cmp(&x, &gx));

    /* 2*G == 已知 2G */
    bn256_set_word(&k, 2);
    ec_point_mul_base(&x, &y, &k);
    bn256_to_bytes(&x, buf);
    TEST_ASSERT_EQUAL_MEMORY(V_2G_X, buf, 32);
    bn256_to_bytes(&y, buf);
    TEST_ASSERT_EQUAL_MEMORY(V_2G_Y, buf, 32);

    /* 3*G = 2G + G (ec_point_add 路径), 与 Python 独立计算对比 */
    bn256_t k3, x3, y3;
    bn256_set_word(&k3, 3);
    ec_point_mul_base(&x3, &y3, &k3);
    bn256_to_bytes(&x3, buf);
    const uint8_t g3_x[32] = {
        0xA9,0x7F,0x7C,0xD4,0xB3,0xC9,0x93,0xB4,0xBE,0x2D,0xAA,0x8C,0xDB,0x41,0xE2,0x4C,
        0xA1,0x3F,0x6B,0xD9,0x45,0x30,0x22,0x44,0xE2,0x69,0x18,0xF1,0xD0,0x50,0x9E,0xBF};
    TEST_ASSERT_EQUAL_MEMORY(g3_x, buf, 32);
    bn256_to_bytes(&y3, buf);
    const uint8_t g3_y[32] = {
        0x53,0x0B,0x5D,0xD8,0x8C,0x68,0x8E,0xF5,0xCC,0xC5,0xCE,0xC0,0x8A,0x72,0x15,0x0F,
        0x7C,0x40,0x0E,0xE5,0xCD,0x04,0x52,0x92,0xAA,0xAC,0xDD,0x03,0x74,0x58,0xF6,0xE6};
    TEST_ASSERT_EQUAL_MEMORY(g3_y, buf, 32);
}

/* ========================================================================
 * 运行器
 * ======================================================================== */
int run_sm_crypto_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sm3_standard_vectors);
    RUN_TEST(test_sm3_incremental);
    RUN_TEST(test_sm3_error_branches);
    RUN_TEST(test_sm4_ecb_standard_vector);
    RUN_TEST(test_sm4_ecb_multiblock_roundtrip);
    RUN_TEST(test_sm4_ecb_error_branches);
    RUN_TEST(test_sm4_cbc_standard_vector);
    RUN_TEST(test_sm4_cbc_error_branches);
    RUN_TEST(test_sm4_gcm_roundtrip);
    RUN_TEST(test_sm4_gcm_error_branches);
    RUN_TEST(test_engine_algo_selection);
    RUN_TEST(test_engine_hash);
    RUN_TEST(test_engine_hmac_kdf);
    RUN_TEST(test_engine_aes_gcm);
    RUN_TEST(test_engine_sym_encrypt_dispatch);
    RUN_TEST(test_sm2_standard_vector_verify);
    RUN_TEST(test_sm2_sign_verify_roundtrip);
    RUN_TEST(test_sm2_sign_hash_roundtrip);
    RUN_TEST(test_sm2_error_branches);
    RUN_TEST(test_sm2_key_exchange_apis);
    RUN_TEST(test_engine_sm2_sign_dispatch);
    RUN_TEST(test_engine_sm2_key_exchange_dispatch);
    RUN_TEST(test_bn256_known_values);
    RUN_TEST(test_ec_point_known_values);
    UNITY_END();
}

#ifndef TEST_SM_CRYPTO_NO_MAIN
int main(void)
{
    return run_sm_crypto_tests();
}
#endif /* TEST_SM_CRYPTO_NO_MAIN */
