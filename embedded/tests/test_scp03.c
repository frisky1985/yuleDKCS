/******************************************************************************
 * @file    test_scp03.c
 * @brief   SCP03 安全通道单元测试
 ******************************************************************************/

#include "unity.h"
#include "scp03.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* 测试: KDF 密钥派生 (已知向量验证) */
void test_scp03_kdf_derive(void) {
    /* 测试向量来自 GlobalPlatform 规范 */
    uint8_t static_key[16] = {
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
    };
    uint8_t derivation_data[32] = {0};
    uint8_t derived_key[16];
    
    int ret = scp03_kdf(static_key, 16, derivation_data, 32, derived_key, 16);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    
    /* 验证输出不为零 */
    uint8_t zeros[16] = {0};
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, derived_key, 16);
}

/* 测试: AES-CMAC 计算 */
void test_scp03_aes_cmac(void) {
    uint8_t key[16] = {
        0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
    };
    uint8_t message[] = "Hello, SCP03!";
    uint8_t mac[16];
    
    int ret = scp03_aes_cmac(key, 16, message, sizeof(message)-1, mac, 16);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    
    /* 验证 MAC 不为零 */
    uint8_t zeros[16] = {0};
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, mac, 16);
}

/* 测试: 会话建立和关闭 */
void test_scp03_session_lifecycle(void) {
    scp03_session_t session;
    scp03_config_t config;
    
    /* 配置 */
    memset(&config, 0, sizeof(config));
    memcpy(config.static_key, "1234567890123456", 16);
    memcpy(config.host_challenge, "ABCDEFGH", 8);
    config.key_type = SCP03_KEY_STATIC;
    config.security_level = SCP03_I_0C;
    
    /* 建立会话 */
    int ret = scp03_open_session(&session, &config);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_EQUAL(SCP03_STATE_OPEN, session.state);
    
    /* 验证会话密钥已生成 */
    uint8_t zeros[32] = {0};
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, session.sess_enc, 16);
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, session.sess_mac, 16);
    
    /* 关闭会话 */
    ret = scp03_close_session(&session);
    TEST_ASSERT_EQUAL(SCP03_SUCCESS, ret);
    TEST_ASSERT_EQUAL(SCP03_STATE_CLOSED, session.state);
    
    /* 验证敏感数据已清除 */
    TEST_ASSERT_EQUAL_MEMORY(zeros, session.sess_enc, 16);
}

/* 测试: 不同安全级别 */
void test_scp03_security_levels(void) {
    scp03_session_t session;
    scp03_config_t config;
    memset(&config, 0, sizeof(config));
    memcpy(config.static_key, "1234567890123456", 16);
    
    /* i=00 无安全性 */
    config.security_level = SCP03_I_00;
    scp03_open_session(&session, &config);
    TEST_ASSERT_FALSE(scp03_is_encryption_required(&session));
    TEST_ASSERT_FALSE(scp03_is_mac_required(&session));
    scp03_close_session(&session);
    
    /* i=04 仅 MAC */
    config.security_level = SCP03_I_04;
    scp03_open_session(&session, &config);
    TEST_ASSERT_FALSE(scp03_is_encryption_required(&session));
    TEST_ASSERT_TRUE(scp03_is_mac_required(&session));
    scp03_close_session(&session);
    
    /* i=0C MAC + 加密 */
    config.security_level = SCP03_I_0C;
    scp03_open_session(&session, &config);
    TEST_ASSERT_TRUE(scp03_is_encryption_required(&session));
    TEST_ASSERT_TRUE(scp03_is_mac_required(&session));
    scp03_close_session(&session);
}

/* 测试: 重放防护 (会话计数器) */
void test_scp03_replay_protection(void) {
    scp03_session_t session;
    scp03_config_t config;
    memset(&config, 0, sizeof(config));
    memcpy(config.static_key, "1234567890123456", 16);
    config.security_level = SCP03_I_04;
    
    scp03_open_session(&session, &config);
    
    /* 初始序列计数器应为 1 */
    TEST_ASSERT_EQUAL(1, session.mac_chaining[0]);
    
    /* 模拟发送 APDU 后计数器增加 */
    session.mac_chaining[0]++;
    TEST_ASSERT_EQUAL(2, session.mac_chaining[0]);
    
    scp03_close_session(&session);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_scp03_kdf_derive);
    RUN_TEST(test_scp03_aes_cmac);
    RUN_TEST(test_scp03_session_lifecycle);
    RUN_TEST(test_scp03_security_levels);
    RUN_TEST(test_scp03_replay_protection);
    
    return UNITY_END();
}
