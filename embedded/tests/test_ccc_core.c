/******************************************************************************
 * @file    test_ccc_core.c
 * @brief   CCC Digital Key R3 核心协议测试
 *          测试: 初始化/反初始化, 配对流程, 安全会话,
 *                加密/解密, HKDF-SHA256, 重放攻击防护
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "ccc.h"

/* ========================================================================
 * 模拟 SE 接口
 * ======================================================================== */
static int mock_se_init(void) { return 0; }
static int mock_se_deinit(void) { return 0; }
static int mock_gen_key(uint8_t *pub, uint8_t *priv) {
    memset(pub, 0xAA, 65);
    memset(priv, 0xBB, 32);
    return 0;
}
static int mock_sign(const uint8_t *d, size_t l, const uint8_t *k, uint8_t *s) {
    (void)d; (void)l; (void)k;
    memset(s, 0xCC, 64);
    return 0;
}
static int mock_verify(const uint8_t *d, size_t l, const uint8_t *k, const uint8_t *s) {
    (void)d; (void)l; (void)k; (void)s;
    return 1; /* always valid */
}
static int mock_derive(const uint8_t *p, const uint8_t *k, uint8_t *s) {
    (void)p; (void)k;
    memset(s, 0xDD, 32);
    return 0;
}
static int mock_secure_store(uint32_t key_id, const uint8_t *data, size_t len) {
    (void)key_id; (void)data; (void)len;
    return 0;
}
static int mock_secure_read(uint32_t key_id, uint8_t *data, size_t *len) {
    (void)key_id; (void)data; (void)len;
    return 0;
}

static ccc_se_interface_t create_mock_se(void)
{
    ccc_se_interface_t se;
    memset(&se, 0, sizeof(se));
    se.init = mock_se_init;
    se.deinit = mock_se_deinit;
    se.generate_key_pair = mock_gen_key;
    se.sign = mock_sign;
    se.verify = mock_verify;
    se.derive_shared_secret = mock_derive;
    se.secure_store = mock_secure_store;
    se.secure_read = mock_secure_read;
    return se;
}

/* ========================================================================
 * 测试: 初始化与反初始化
 * ======================================================================== */
static int test_ccc_init_deinit(void)
{
    printf("  [Test] ccc_init / ccc_deinit ... ");
    ccc_se_interface_t se = create_mock_se();

    error_t ret = ccc_init(&se);
    assert(ret == OK);

    ret = ccc_deinit();
    assert(ret == OK);

    printf("PASSED\n");
    return 0;
}

static int test_ccc_double_init(void)
{
    printf("  [Test] ccc_init 双重初始化防护 ... ");
    ccc_se_interface_t se = create_mock_se();

    error_t ret = ccc_init(&se);
    assert(ret == OK);

    ret = ccc_init(&se);
    assert(ret == ERROR_ALREADY_INITIALIZED);

    ccc_deinit();
    printf("PASSED\n");
    return 0;
}

static int test_ccc_init_null(void)
{
    printf("  [Test] ccc_init(NULL) 参数校验 ... ");
    error_t ret = ccc_init(NULL);
    assert(ret == ERROR_INVALID_PARAM);
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 配对会话管理
 * ======================================================================== */
static int test_ccc_pairing_session(void)
{
    printf("  [Test] 配对会话创建与销毁 ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    ccc_pairing_config_t config;
    memset(&config, 0, sizeof(config));
    memcpy(config.vehicle_vin, "LSVNV2182E2100001", 17);

    ccc_session_context_t *session = NULL;
    error_t ret = ccc_create_pairing_session(&config, &session);
    assert(ret == OK);
    assert(session != NULL);
    assert(session->state == CCC_SESSION_STATE_IDLE);
    assert(session->pairing_state == CCC_PAIRING_STATE_IDLE);

    ccc_destroy_session(session);
    ccc_deinit();
    printf("PASSED\n");
    return 0;
}

static int test_ccc_pairing_flow(void)
{
    printf("  [Test] 完整配对流程 (创建 → 开始 → 响应 → 完成) ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    /* 创建会话 */
    ccc_pairing_config_t config;
    memset(&config, 0, sizeof(config));
    memcpy(config.vehicle_vin, "LSVNV2182E2100001", 17);

    ccc_session_context_t *session = NULL;
    error_t ret = ccc_create_pairing_session(&config, &session);
    assert(ret == OK);

    /* 开始配对 (发送请求) */
    uint8_t request[512];
    size_t request_len = sizeof(request);
    ret = ccc_start_pairing(session, request, &request_len);
    assert(ret == OK);
    assert(request_len > 0);
    assert(session->state == CCC_SESSION_STATE_PAIRING);

    /* 模拟车辆响应 */
    uint8_t response[256];
    size_t offset = 0;
    response[offset++] = (3 << 4) | 0;  /* Version 3.0 */
    memset(&response[offset], 0xAA, 65);  /* Vehicle public key */
    offset += 65;
    memset(&response[offset], 0xEE, 32);  /* Vehicle challenge */
    offset += 32;
    response[offset++] = 0x00;           /* No cert chain */
    response[offset++] = 0x00;
    memset(&response[offset], 0xCC, 64);  /* Signature */
    offset += 64;

    /* 处理配对响应 */
    ret = ccc_process_pairing_response(session, response, offset);
    assert(ret == OK);
    assert(session->pairing_state == CCC_PAIRING_STATE_DEVICE_VERIFIED);

    /* 完成配对确认 */
    uint8_t confirmation[512];
    size_t conf_len = sizeof(confirmation);
    ret = ccc_complete_pairing(session, confirmation, &conf_len);
    assert(ret == OK);
    assert(session->state == CCC_SESSION_STATE_ACTIVE);
    assert(session->is_secure_session == true);

    ccc_destroy_session(session);
    ccc_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 安全会话
 * ======================================================================== */
static int test_ccc_establish_session(void)
{
    printf("  [Test] 安全会话建立 ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    ccc_session_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = CCC_SESSION_STATE_IDLE;

    uint8_t vehicle_pub[65];
    memset(vehicle_pub, 0xAA, 65);
    uint8_t vehicle_challenge[32];
    memset(vehicle_challenge, 0xBB, 32);

    error_t ret = ccc_establish_session(&ctx, vehicle_pub, vehicle_challenge);
    assert(ret == OK);
    assert(ctx.state == CCC_SESSION_STATE_ACTIVE);
    assert(ctx.is_secure_session == true);

    ccc_destroy_session(&ctx);
    ccc_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 加密与解密消息
 * ======================================================================== */
static int test_ccc_encrypt_decrypt(void)
{
    printf("  [Test] AES-128-GCM 加密/解密 ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    ccc_session_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = CCC_SESSION_STATE_ACTIVE;
    ctx.is_secure_session = true;
    ctx.message_counter = 0;
    memset(ctx.session_key_enc, 0x42, 16);
    memset(ctx.session_key_mac, 0x42, 16);

    /* 测试载荷 */
    const uint8_t payload[] = "Hello, CCC R2.0 Digital Key!";
    size_t payload_len = strlen((const char *)payload);

    uint8_t encrypted[512];
    size_t encrypted_len = sizeof(encrypted);

    error_t ret = ccc_encrypt_message(&ctx, CCC_CMD_VEHICLE_UNLOCK,
                                       payload, payload_len,
                                       encrypted, &encrypted_len);
    assert(ret == OK);
    assert(encrypted_len > 0);
    assert(ctx.message_counter == 1);

    /* 解密 */
    ccc_command_t received_cmd;
    uint8_t decrypted[256];
    size_t decrypted_len = sizeof(decrypted);

    ret = ccc_decrypt_message(&ctx, encrypted, encrypted_len,
                               &received_cmd, decrypted, &decrypted_len);
    assert(ret == OK);
    assert(received_cmd == CCC_CMD_VEHICLE_UNLOCK);
    assert(decrypted_len == payload_len);
    assert(memcmp(decrypted, payload, payload_len) == 0);

    ccc_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 重放攻击防护
 * ======================================================================== */
static int test_ccc_replay_protection(void)
{
    printf("  [Test] 重放攻击防护 (计数器验证) ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    ccc_session_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = CCC_SESSION_STATE_ACTIVE;
    ctx.is_secure_session = true;
    ctx.message_counter = 5;  /* 当前计数为 5 */
    memset(ctx.session_key_enc, 0x42, 16);
    memset(ctx.session_key_mac, 0x42, 16);

    /* 使用计数器 3 (旧消息) 加密 */
    ctx.message_counter = 3;
    uint8_t old_encrypted[512];
    size_t old_enc_len = sizeof(old_encrypted);

    const uint8_t payload[] = "Test Replay";
    error_t ret = ccc_encrypt_message(&ctx, CCC_CMD_VEHICLE_LOCK,
                                       payload, strlen((const char *)payload),
                                       old_encrypted, &old_enc_len);
    assert(ret == OK);

    /* 重置计数器到 5 (模拟后续状态) */
    ctx.message_counter = 5;

    /* 尝试重放旧消息 */
    ccc_command_t cmd;
    uint8_t decrypted[256];
    size_t dec_len = sizeof(decrypted);

    ret = ccc_decrypt_message(&ctx, old_encrypted, old_enc_len,
                               &cmd, decrypted, &dec_len);
    /* 应该失败: 计数器 3 != 5 */
    assert(ret != OK);

    printf("PASSED (重放攻击被正确拒绝)\n");
    ccc_deinit();
    return 0;
}

/* ========================================================================
 * 测试: 挑战生成
 * ======================================================================== */
static int test_ccc_challenge(void)
{
    printf("  [Test] 挑战生成 (随机性) ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    uint8_t challenge1[CCC_CHALLENGE_LEN];
    uint8_t challenge2[CCC_CHALLENGE_LEN];

    error_t ret = ccc_generate_challenge(challenge1);
    assert(ret == OK);

    ret = ccc_generate_challenge(challenge2);
    assert(ret == OK);

    /* 两次生成的挑战应该不同 */
    assert(memcmp(challenge1, challenge2, CCC_CHALLENGE_LEN) != 0);

    ccc_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 会话超时
 * ======================================================================== */
static int test_ccc_session_timeout(void)
{
    printf("  [Test] 会话超时检测 ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    ccc_session_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = CCC_SESSION_STATE_ACTIVE;
    ctx.is_secure_session = true;
    ctx.session_timeout_ms = 100;  /* 100ms 超时 */
    ctx.last_activity = 0;         /* 最后活动很早 */

    uint8_t response[64];
    size_t response_len = sizeof(response);

    error_t ret = ccc_send_vehicle_command(&ctx, CCC_CMD_STATUS_REQUEST,
                                            NULL, 0,
                                            response, &response_len);
    assert(ret == ERROR_TIMEOUT);
    assert(ctx.state == CCC_SESSION_STATE_TERMINATING);

    printf("PASSED\n");
    ccc_deinit();
    return 0;
}

/* ========================================================================
 * 测试: HKDF 密钥派生
 * ======================================================================== */
static int test_ccc_hkdf_derive(void)
{
    printf("  [Test] HKDF-SHA256 密钥派生 ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    uint8_t shared_secret[32];
    memset(shared_secret, 0xDD, 32);

    uint8_t enc_key[16];
    uint8_t mac_key[16];

    error_t ret = ccc_derive_session_keys(shared_secret,
                                           (uint8_t *)"test-salt",
                                           enc_key, mac_key);
    assert(ret == OK);

    /* 两次派生应该得到相同结果 (确定性的) */
    uint8_t enc_key2[16], mac_key2[16];
    ret = ccc_derive_session_keys(shared_secret,
                                   (uint8_t *)"test-salt",
                                   enc_key2, mac_key2);
    assert(ret == OK);
    assert(memcmp(enc_key, enc_key2, 16) == 0);
    assert(memcmp(mac_key, mac_key2, 16) == 0);

    /* 不同 salt 应该得到不同的密钥 */
    uint8_t enc_key3[16], mac_key3[16];
    ret = ccc_derive_session_keys(shared_secret,
                                   (uint8_t *)"different-salt",
                                   enc_key3, mac_key3);
    assert(ret == OK);
    assert(memcmp(enc_key, enc_key3, 16) != 0);

    printf("PASSED\n");
    ccc_deinit();
    return 0;
}

/* ========================================================================
 * 测试: 空参数校验
 * ======================================================================== */
static int test_ccc_null_params(void)
{
    printf("  [Test] 所有 API 的空指针校验 ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    /* 测试所有需要 NULL 检查的 API */
    assert(ccc_create_pairing_session(NULL, NULL) == ERROR_INVALID_PARAM);

    ccc_session_context_t dummy_ctx;
    memset(&dummy_ctx, 0, sizeof(dummy_ctx));
    ccc_session_context_t *session = &dummy_ctx;

    uint8_t buf[64];
    size_t len = sizeof(buf);

    assert(ccc_start_pairing(NULL, buf, &len) == ERROR_INVALID_PARAM);
    assert(ccc_start_pairing(session, NULL, &len) == ERROR_INVALID_PARAM);
    assert(ccc_process_pairing_response(NULL, buf, len) == ERROR_INVALID_PARAM);
    assert(ccc_establish_session(NULL, buf, buf) == ERROR_INVALID_PARAM);
    assert(ccc_encrypt_message(NULL, CCC_CMD_VEHICLE_UNLOCK, buf, 1, buf, &len) == ERROR_INVALID_PARAM);

    ccc_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: MAC 计算与验证
 * ======================================================================== */
static int test_ccc_mac(void)
{
    printf("  [Test] MAC 计算与验证 ... ");
    ccc_se_interface_t se = create_mock_se();
    ccc_init(&se);

    uint8_t mac_key[16];
    memset(mac_key, 0xAA, 16);

    const uint8_t message[] = "This is a test message for MAC";
    uint8_t mac1[32];
    uint8_t mac2[32];

    error_t ret = ccc_compute_mac(mac_key, message, sizeof(message), mac1);
    assert(ret == OK);

    /* 相同输入应该产生相同 MAC */
    ret = ccc_compute_mac(mac_key, message, sizeof(message), mac2);
    assert(ret == OK);
    assert(memcmp(mac1, mac2, 32) == 0);

    /* 不同消息应该产生不同 MAC */
    const uint8_t other_msg[] = "Different message";
    ret = ccc_compute_mac(mac_key, other_msg, sizeof(other_msg), mac2);
    assert(ret == OK);
    assert(memcmp(mac1, mac2, 32) != 0);

    /* MAC 验证 */
    ret = ccc_verify_mac(mac_key, message, sizeof(message), mac1);
    assert(ret == OK);

    /* 错误 MAC 应该验证失败 */
    mac2[0] ^= 0xFF;
    ret = ccc_verify_mac(mac_key, message, sizeof(message), mac2);
    assert(ret != OK);

    printf("PASSED\n");
    ccc_deinit();
    return 0;
}

/* ========================================================================
 * 主测试入口
 * ======================================================================== */
int main(void)
{
    printf("========================================\n");
    printf("  CCC R2.0 核心协议测试套件\n");
    printf("========================================\n\n");

    int total = 0;
    int passed = 0;
    int failed = 0;

    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"初始化/反初始化",              test_ccc_init_deinit},
        {"双重初始化防护",              test_ccc_double_init},
        {"NULL 参数校验",               test_ccc_init_null},
        {"配对会话创建与销毁",           test_ccc_pairing_session},
        {"完整配对流程",                test_ccc_pairing_flow},
        {"安全会话建立",                test_ccc_establish_session},
        {"AES-128-GCM 加密/解密",       test_ccc_encrypt_decrypt},
        {"重放攻击防护",                test_ccc_replay_protection},
        {"挑战生成随机性",              test_ccc_challenge},
        {"会话超时检测",                test_ccc_session_timeout},
        {"HKDF-SHA256 密钥派生",        test_ccc_hkdf_derive},
        {"NULL 参数空指针校验",          test_ccc_null_params},
        {"MAC 计算与验证",              test_ccc_mac},
    };

    size_t num_tests = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < num_tests; i++) {
        total++;
        printf("[%zu/%zu] %s\n", i + 1, num_tests, tests[i].name);
        int result = tests[i].fn();
        if (result == 0) {
            passed++;
        } else {
            failed++;
            printf("  --> FAILED (code %d)\n", result);
        }
        printf("\n");
    }

    printf("========================================\n");
    printf("  测试总结\n");
    printf("========================================\n");
    printf("  总数:  %d\n", total);
    printf("  通过:  %d\n", passed);
    printf("  失败:  %d\n", failed);
    printf("========================================\n");

    return (failed == 0) ? 0 : 1;
}
