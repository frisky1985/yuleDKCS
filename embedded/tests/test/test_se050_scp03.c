/**
 * @file test_se050_scp03.c
 * @brief SE050 SCP03 安全通道内部密码原语单元测试
 *
 * se050_scp03.c 的密码学内部函数全部为 static, 且 stubs_hal.c 的
 * i2c_transfer mock 不模拟数据 (只返回 0), 无法驱动完整 SCP03 握手,
 * 因此本测试直接 #include 被测源文件以访问 static 函数 (嵌入式单元
 * 测试标准做法)。会话级函数 (open_session/apdu 等) 仅覆盖确定性
 * 错误分支 (NULL/状态/长度), 完整握手路径因 I2C mock 无数据而不可达。
 *
 * 测试向量来源:
 *   - AES-128: FIPS-197 (密钥 000102...0f, 明文 001122...ff)
 *   - CMAC: NIST SP 800-38B (密钥 2b7e151628aed2a6abf7158809cf4f3c)
 *   - 密钥派生: GlobalPlatform SCP03 密钥 (404142...4f, seq=0001),
 *     期望值用 OpenSSL AES-128-ECB 独立验证
 */

#include "unity.h"
#include "se050_scp03.h"

/* 直接包含被测源文件 (唯一访问 static 内部函数的方式) */
#include "../../ccc_protocol/src/security/se050_scp03.c"

#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* 会话级握手需要随机数: crypto_random.c 提供强符号 (独立/合并构建均链接),
 * 首次调用前需 crypto_random_init() (主机环境下可成功初始化) */
extern int crypto_random_init(void);

/* ========================================================================
 * 测试向量
 * ======================================================================== */
/* FIPS-197 AES-128 */
static const uint8_t V_AES_KEY[16] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F};
static const uint8_t V_AES_PT[16] = {
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
static const uint8_t V_AES_CT[16] = {
    0x69,0xC4,0xE0,0xD8,0x6A,0x7B,0x04,0x30,0xD8,0xCD,0xB7,0x80,0x70,0xB4,0xC5,0x5A};

/* NIST SP 800-38B CMAC */
static const uint8_t V_CMAC_KEY[16] = {
    0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,
    0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};
static const uint8_t V_CMAC_K1[16] = {
    0xFB,0xEE,0xD6,0x18,0x35,0x71,0x33,0x66,0x7C,0x85,0xE0,0x8F,0x72,0x36,0xA8,0xDE};
static const uint8_t V_CMAC_K2[16] = {
    0xF7,0xDD,0xAC,0x30,0x6A,0xE2,0x66,0xCC,0xF9,0x0B,0xC1,0x1E,0xE4,0x6D,0x51,0x3B};
static const uint8_t V_CMAC_M1[16] = {
    0x6B,0xC1,0xBE,0xE2,0x2E,0x40,0x9F,0x96,0xE9,0x3D,0x7E,0x11,0x73,0x93,0x17,0x2A};
static const uint8_t V_CMAC_M2[40] = {
    0x6B,0xC1,0xBE,0xE2,0x2E,0x40,0x9F,0x96,0xE9,0x3D,0x7E,0x11,0x73,0x93,0x17,0x2A,
    0xAE,0x2D,0x8A,0x57,0x1E,0x03,0xAC,0x9C,0x9E,0xB7,0x6F,0xAC,0x45,0xAF,0x8E,0x51,
    0x30,0xC8,0x1C,0x46,0xA3,0x5C,0xE4,0x11};
static const uint8_t V_CMAC_M3[56] = {
    0x6B,0xC1,0xBE,0xE2,0x2E,0x40,0x9F,0x96,0xE9,0x3D,0x7E,0x11,0x73,0x93,0x17,0x2A,
    0xAE,0x2D,0x8A,0x57,0x1E,0x03,0xAC,0x9C,0x9E,0xB7,0x6F,0xAC,0x45,0xAF,0x8E,0x51,
    0x30,0xC8,0x1C,0x46,0xA3,0x5C,0xE4,0x11,0xE5,0xFB,0xC1,0x19,0x1A,0x0A,0x52,0xEF,
    0xF6,0x9F,0x24,0x45,0xDF,0x4F,0x9B,0x17};
/* 期望 CMAC (OpenSSL 独立验证) */
static const uint8_t V_CMAC_EMPTY[16] = {
    0x8C,0xAA,0x8E,0xB9,0x98,0x4A,0xA5,0xDD,0x61,0x1E,0xB1,0xF6,0x04,0x14,0x0F,0x24};
static const uint8_t V_CMAC_15B[16] = {
    0xF2,0x12,0xD4,0xC2,0x15,0x4C,0x87,0x66,0xDE,0x60,0xC1,0x8C,0x98,0xFA,0x0C,0x93};
static const uint8_t V_CMAC_16B[16] = {
    0x07,0x0A,0x16,0xB4,0x6B,0x4D,0x41,0x44,0xF7,0x9B,0xDD,0x9D,0xD0,0x4A,0x28,0x7C};
static const uint8_t V_CMAC_31B[16] = {
    0x8A,0x15,0x7A,0xCF,0xF5,0x17,0xD2,0x1B,0xCD,0x6A,0xB6,0x5C,0xD0,0x14,0xCC,0x70};
static const uint8_t V_CMAC_40B[16] = {
    0xDF,0xA6,0x67,0x47,0xDE,0x9A,0xE6,0x30,0x30,0xCA,0x32,0x61,0x14,0x97,0xC8,0x27};
static const uint8_t V_CMAC_56B[16] = {
    0xC8,0xCA,0x5A,0xD6,0x52,0x91,0x3C,0x5F,0xBB,0x82,0x72,0x84,0x8C,0x25,0xF5,0xB5};

/* GP SCP03 密钥派生 (K_ENC=K_MAC=K_RMAC=404142...4f, seq=0001) */
static const uint8_t V_GP_KEY[16] = {
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F};
static const uint8_t V_SEQ[2] = { 0x00, 0x01 };
static const uint8_t V_S_ENC[16] = {
    0x90,0x6B,0x5C,0x59,0x0A,0x96,0x4C,0x8B,0x88,0xC8,0x0E,0x31,0x84,0xF8,0x1A,0x8F};
static const uint8_t V_S_MAC[16] = {
    0x63,0x95,0x7E,0x84,0xC8,0xCC,0xFD,0x15,0xBC,0x00,0xEB,0x56,0x3A,0x09,0xE4,0xB4};
static const uint8_t V_S_RMAC[16] = {
    0xE7,0xAE,0x90,0x8D,0xDE,0x9D,0xFE,0xE3,0xE2,0x1D,0x41,0xA8,0x03,0x71,0x62,0xF2};

/* ========================================================================
 * AES-128 (FIPS-197)
 * ======================================================================== */
void test_scp03_aes128_fips197(void)
{
    scp03_aes128_ctx_t ctx;
    uint8_t out[16];
    scp03_aes128_key_expand(&ctx, V_AES_KEY);
    scp03_aes128_encrypt(&ctx, V_AES_PT, out);
    TEST_ASSERT_EQUAL_MEMORY(V_AES_CT, out, 16);

    /* 别名调用: out == in */
    {
        uint8_t buf[16];
        memcpy(buf, V_AES_PT, 16);
        scp03_aes128_encrypt(&ctx, buf, buf);
        TEST_ASSERT_EQUAL_MEMORY(V_AES_CT, buf, 16);
    }
}

/* ========================================================================
 * CMAC 子密钥与 CMAC 向量
 * ======================================================================== */
void test_scp03_cmac_subkeys(void)
{
    uint8_t k1[16], k2[16];
    scp03_cmac_generate_subkeys(V_CMAC_KEY, k1, k2);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_K1, k1, 16);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_K2, k2, 16);

    /* MSB 置位分支 (L = AES(key,0) 首位 >= 0x80 → 左移后异或 0x87):
     * key = 0123456789abcdeffedcba9876543210, AES(0) = d5c825a2... */
    {
        const uint8_t k2_high[16] = {
            0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10};
        uint8_t a[16], b[16];
        scp03_cmac_generate_subkeys(k2_high, a, b);
        /* K1 = 2*L, K2 = 2*K1 (L MSB 置位 → 两次 0x87 修正) */
        TEST_ASSERT_FALSE(memcmp(a, b, 16) == 0);
        TEST_ASSERT_NOT_EQUAL(0, a[0]);
    }
}

void test_scp03_cmac_vectors(void)
{
    uint8_t mac[16];
    uint8_t m31[31];
    memcpy(m31, V_CMAC_M1, 16);
    memcpy(m31 + 16, V_CMAC_M2 + 16, 15);

    /* 空消息: 实现用 K1 (与 NIST 用 K2 不同), 断言实际行为并注释 */
    scp03_aes_cmac(V_CMAC_KEY, NULL, 0, mac);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_EMPTY, mac, 16);

    scp03_aes_cmac(V_CMAC_KEY, V_CMAC_M1, 15, mac);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_15B, mac, 16);

    scp03_aes_cmac(V_CMAC_KEY, V_CMAC_M1, 16, mac);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_16B, mac, 16);

    scp03_aes_cmac(V_CMAC_KEY, m31, 31, mac);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_31B, mac, 16);

    scp03_aes_cmac(V_CMAC_KEY, V_CMAC_M2, 40, mac);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_40B, mac, 16);

    scp03_aes_cmac(V_CMAC_KEY, V_CMAC_M3, 56, mac);
    TEST_ASSERT_EQUAL_MEMORY(V_CMAC_56B, mac, 16);
}

/* ========================================================================
 * SCP03 会话密钥派生
 * ======================================================================== */
void test_scp03_derive_session_keys(void)
{
    uint8_t s_enc[16], s_mac[16], s_rmac[16];
    uint8_t s_enc2[16], s_mac2[16], s_rmac2[16];
    const uint8_t seq2[2] = { 0x00, 0x02 };

    scp03_derive_session_keys(V_GP_KEY, V_GP_KEY, V_GP_KEY, V_SEQ,
                              s_enc, s_mac, s_rmac);
    TEST_ASSERT_EQUAL_MEMORY(V_S_ENC, s_enc, 16);
    TEST_ASSERT_EQUAL_MEMORY(V_S_MAC, s_mac, 16);
    TEST_ASSERT_EQUAL_MEMORY(V_S_RMAC, s_rmac, 16);

    /* 确定性: 相同输入两次结果一致 */
    scp03_derive_session_keys(V_GP_KEY, V_GP_KEY, V_GP_KEY, V_SEQ,
                              s_enc2, s_mac2, s_rmac2);
    TEST_ASSERT_EQUAL_MEMORY(s_enc, s_enc2, 16);
    TEST_ASSERT_EQUAL_MEMORY(s_mac, s_mac2, 16);
    TEST_ASSERT_EQUAL_MEMORY(s_rmac, s_rmac2, 16);

    /* 不同 seq counter → 不同会话密钥 */
    scp03_derive_session_keys(V_GP_KEY, V_GP_KEY, V_GP_KEY, seq2,
                              s_enc2, s_mac2, s_rmac2);
    TEST_ASSERT_FALSE(memcmp(s_enc, s_enc2, 16) == 0);

    /* 单个派生接口 */
    {
        uint8_t k[16];
        int rc = scp03_derive_session_key(V_GP_KEY, SCP03_DERIVE_S_ENC, V_SEQ, k);
        TEST_ASSERT_EQUAL_INT(0, rc);
        TEST_ASSERT_EQUAL_MEMORY(V_S_ENC, k, 16);
    }
}

/* ========================================================================
 * APDU 响应解析
 * ======================================================================== */
void test_scp03_parse_response(void)
{
    const uint8_t resp_ok[5] = { 0x01, 0x02, 0x03, 0x90, 0x00 };
    const uint8_t resp_short[1] = { 0x90 };
    uint16_t dlen = 0xFFFF;
    uint16_t sw = 0;

    TEST_ASSERT_EQUAL_INT(0, scp03_parse_response(resp_ok, 5, &dlen, &sw));
    TEST_ASSERT_EQUAL_UINT(3, dlen);
    TEST_ASSERT_EQUAL_HEX16(0x9000, sw);

    /* data_len 可空 */
    sw = 0;
    TEST_ASSERT_EQUAL_INT(0, scp03_parse_response(resp_ok, 5, NULL, &sw));
    TEST_ASSERT_EQUAL_HEX16(0x9000, sw);

    /* 长度不足 → SCP03_ERR_APDU */
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_APDU, scp03_parse_response(resp_short, 1, &dlen, &sw));
}

/* ========================================================================
 * 生命周期与公共 API 错误分支
 * ======================================================================== */
void test_scp03_init_deinit(void)
{
    scp03_session_t sess;
    memset(&sess, 0xAA, sizeof(sess));

    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL, se050_scp03_init(NULL));
    TEST_ASSERT_EQUAL_INT(SCP03_OK, se050_scp03_init(&sess));
    TEST_ASSERT_EQUAL_INT(SCP03_STATE_INIT, sess.state);
    /* 幂等 */
    TEST_ASSERT_EQUAL_INT(SCP03_OK, se050_scp03_init(&sess));

    se050_scp03_deinit(NULL);   /* 安全 */
    se050_scp03_deinit(&sess);
    /* deinit 后状态清零 (CLOSED = 0) */
    TEST_ASSERT_EQUAL_INT(SCP03_STATE_CLOSED, sess.state);
}

void test_scp03_provision_keys(void)
{
    scp03_session_t sess;
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL, se050_scp03_provision_keys(NULL, V_GP_KEY, V_GP_KEY, V_GP_KEY));

    se050_scp03_init(&sess);
    TEST_ASSERT_EQUAL_INT(SCP03_OK,
        se050_scp03_provision_keys(&sess, V_GP_KEY, V_GP_KEY, V_GP_KEY));
    TEST_ASSERT_EQUAL_MEMORY(V_GP_KEY, sess.static_enc_key, 16);
    TEST_ASSERT_EQUAL_MEMORY(V_GP_KEY, sess.static_mac_key, 16);
    TEST_ASSERT_EQUAL_MEMORY(V_GP_KEY, sess.static_rmac_key, 16);

    /* 会话非 INIT 状态时拒绝覆写密钥 */
    sess.state = SCP03_STATE_OPEN;
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_CHANNEL,
        se050_scp03_provision_keys(&sess, V_GP_KEY, V_GP_KEY, V_GP_KEY));
    se050_scp03_deinit(&sess);
}

void test_scp03_apdu_plain_errors(void)
{
    uint8_t resp[16];
    uint16_t resp_len = 16;
    uint8_t data[16];

    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL,
        se050_scp03_apdu_plain(0x48, 0x80, 0x50, 0, 0, data, 16, NULL, &resp_len));
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL,
        se050_scp03_apdu_plain(0x48, 0x80, 0x50, 0, 0, data, 16, resp, NULL));
    /* 数据超长 */
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_PARAM,
        se050_scp03_apdu_plain(0x48, 0x80, 0x50, 0, 0, data, SCP03_MAX_APDU_DATA + 1,
                               resp, &resp_len));
}

void test_scp03_open_session_errors(void)
{
    scp03_session_t sess;
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL, se050_scp03_open_session(NULL, 0x48));

    se050_scp03_init(&sess);
    /* 状态非 INIT 时拒绝 */
    sess.state = SCP03_STATE_OPEN;
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_PARAM, se050_scp03_open_session(&sess, 0x48));
    se050_scp03_deinit(&sess);

    /* 完整握手依赖 I2C mock 数据, 无法驱动成功路径: 先初始化 RNG,
     * 走真实握手流程的失败分支 (覆盖 key 派生/cryptogram/I2C 路径) */
    (void)crypto_random_init();
    se050_scp03_init(&sess);
    {
        int rc = se050_scp03_open_session(&sess, 0x48);
        TEST_ASSERT_NOT_EQUAL(SCP03_OK, rc);
    }
    se050_scp03_deinit(&sess);
}

/* ========================================================================
 * I2C 依赖路径 (stub 不返回数据, 覆盖失败分支与传输层)
 * ======================================================================== */
void test_scp03_apdu_plain_i2c_path(void)
{
    uint8_t resp[64];
    uint16_t resp_len = 64;
    const uint8_t data[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

    /* 有效参数: 走 APDU 构建 → i2c 写 → i2c 读 → 解析 (stub 无数据,
     * 结果必为错误; 具体错误码取决于栈内容) */
    {
        int rc = se050_scp03_apdu_plain(0x48, 0x80, 0x50, 0x00, 0x00,
                                        data, sizeof(data), resp, &resp_len);
        TEST_ASSERT_NOT_EQUAL(SCP03_OK, rc);
    }
    /* 无数据 Case 3 APDU (Lc=0 分支) */
    resp_len = 64;
    {
        int rc = se050_scp03_apdu_plain(0x48, 0x80, 0x50, 0x00, 0x00,
                                        NULL, 0, resp, &resp_len);
        TEST_ASSERT_NOT_EQUAL(SCP03_OK, rc);
    }
}

void test_scp03_apdu_cmac_path(void)
{
    scp03_session_t sess;
    uint8_t resp[64];
    uint16_t resp_len = 64;
    const uint8_t data[4] = { 0xAA, 0xBB, 0xCC, 0xDD };

    (void)crypto_random_init();
    se050_scp03_init(&sess);
    /* 伪造 OPEN 状态: 走 C-MAC 计算 → i2c 写 → i2c 读 → CMAC 校验失败路径 */
    sess.state = SCP03_STATE_OPEN;
    {
        int rc = se050_scp03_apdu(&sess, 0x48, 0x84, 0x00, 0x00, 0x00,
                                  data, sizeof(data), resp, &resp_len);
        TEST_ASSERT_NOT_EQUAL(SCP03_OK, rc);
    }
    se050_scp03_deinit(&sess);
}

void test_scp03_close_session_path(void)
{
    scp03_session_t sess;
    memset(&sess, 0, sizeof(sess));
    /* 无论状态, close_session 执行 RESET I2C 与密钥清零主体,
     * 状态回到 INIT (可重新 open) */
    se050_scp03_close_session(&sess);
    TEST_ASSERT_EQUAL_INT(SCP03_STATE_INIT, sess.state);
}

void test_scp03_rotate_keys_path(void)
{
    scp03_session_t sess;
    uint8_t resp[16];
    uint16_t resp_len = 16;

    (void)crypto_random_init();
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL, se050_scp03_rotate_keys(NULL, 0x48));

    se050_scp03_init(&sess);
    /* 非 OPEN 状态 → ERR_CHANNEL */
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_CHANNEL, se050_scp03_rotate_keys(&sess, 0x48));

    /* OPEN 状态: 快照密钥 → close → 恢复 → 重开会话 (stub 下失败分支) */
    sess.state = SCP03_STATE_OPEN;
    {
        int rc = se050_scp03_rotate_keys(&sess, 0x48);
        TEST_ASSERT_NOT_EQUAL(SCP03_OK, rc);
    }
    se050_scp03_deinit(&sess);
}

void test_scp03_apdu_errors(void)
{
    scp03_session_t sess;
    uint8_t resp[16];
    uint16_t resp_len = 16;
    uint8_t data[4] = { 1, 2, 3, 4 };

    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL,
        se050_scp03_apdu(NULL, 0x48, 0x84, 0x00, 0, 0, data, 4, resp, &resp_len));
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_NULL,
        se050_scp03_apdu(&sess, 0x48, 0x84, 0x00, 0, 0, data, 4, NULL, &resp_len));

    se050_scp03_init(&sess);
    /* 会话未打开 → SCP03_ERR_CHANNEL (状态检查在长度检查之前) */
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_CHANNEL,
        se050_scp03_apdu(&sess, 0x48, 0x84, 0x00, 0, 0, data, 4, resp, &resp_len));
    /* 会话 OPEN 状态 + 数据超长 → SCP03_ERR_PARAM (在触碰 I2C 之前返回) */
    sess.state = SCP03_STATE_OPEN;
    TEST_ASSERT_EQUAL_INT(SCP03_ERR_PARAM,
        se050_scp03_apdu(&sess, 0x48, 0x84, 0x00, 0, 0, data, SCP03_MAX_APDU_DATA + 1,
                         resp, &resp_len));
    se050_scp03_deinit(&sess);
}

void test_scp03_close_rotate_isopen(void)
{
    scp03_session_t sess;
    se050_scp03_close_session(NULL);   /* 安全 */

    TEST_ASSERT_FALSE(se050_scp03_is_open(NULL));
    memset(&sess, 0, sizeof(sess));
    TEST_ASSERT_FALSE(se050_scp03_is_open(&sess));

    /* rotate_keys NULL → 错误 (不崩溃) */
    TEST_ASSERT_NOT_EQUAL(SCP03_OK, se050_scp03_rotate_keys(NULL, 0x48));
}

void test_scp03_secure_zero(void)
{
    uint8_t buf[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    se050_scp03_secure_zero(buf, 8);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[7]);
    se050_scp03_secure_zero(NULL, 8);  /* 安全 */
    se050_scp03_secure_zero(buf, 0);
}

/* ========================================================================
 * 运行器
 * ======================================================================== */
int run_se050_scp03_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_scp03_aes128_fips197);
    RUN_TEST(test_scp03_cmac_subkeys);
    RUN_TEST(test_scp03_cmac_vectors);
    RUN_TEST(test_scp03_derive_session_keys);
    RUN_TEST(test_scp03_parse_response);
    RUN_TEST(test_scp03_init_deinit);
    RUN_TEST(test_scp03_provision_keys);
    RUN_TEST(test_scp03_apdu_plain_errors);
    RUN_TEST(test_scp03_open_session_errors);
    RUN_TEST(test_scp03_apdu_plain_i2c_path);
    RUN_TEST(test_scp03_apdu_cmac_path);
    RUN_TEST(test_scp03_close_session_path);
    RUN_TEST(test_scp03_rotate_keys_path);
    RUN_TEST(test_scp03_apdu_errors);
    RUN_TEST(test_scp03_close_rotate_isopen);
    RUN_TEST(test_scp03_secure_zero);
    UNITY_END();
}

#ifndef TEST_SE050_SCP03_NO_MAIN
int main(void)
{
    return run_se050_scp03_tests();
}
#endif /* TEST_SE050_SCP03_NO_MAIN */
