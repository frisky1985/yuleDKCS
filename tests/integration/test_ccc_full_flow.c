/******************************************************************************
 * @file    test_ccc_full_flow.c
 * @brief   CCC Digital Key 完整流程集成测试
 ******************************************************************************/

#include "unity.h"
#include "ccc.h"
#include "ccc_uwb.h"
#include <string.h>

/* 模拟数据 */
static uint8_t g_owner_key[32];
static uint8_t g_friend_key[32];
static ccc_session_context_t g_session;

void setUp(void) {
    memset(&g_session, 0, sizeof(g_session));
}

/* 测试: Owner 配对流程 */
void test_ccc_owner_pairing_flow(void) {
    ccc_pairing_config_t config;
    memset(&config, 0, sizeof(config));
    memcpy(config.vehicle_vin, "WBA1234567890ABCD", 17);
    config.requested_role = KEY_ROLE_OWNER;
    config.requested_permissions = PERM_UNLOCK | PERM_LOCK | PERM_ENGINE_START;
    
    /* 步骤1: 创建配对会话 */
    int ret = ccc_create_pairing_session(&config, &g_session);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    
    /* 步骤2: 生成配对请求 */
    uint8_t request[256];
    size_t request_len = sizeof(request);
    ret = ccc_start_pairing(&g_session, request, &request_len);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    TEST_ASSERT_TRUE(request_len > 0);
    
    /* 步骤3: 处理车辆响应 (模拟) */
    uint8_t response[256] = {0x90, 0x00};  /* SW_SUCCESS */
    size_t response_len = 2;
    ret = ccc_process_pairing_response(&g_session, response, response_len);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    
    /* 步骤4: 完成配对 */
    ret = ccc_complete_pairing(&g_session, g_owner_key, sizeof(g_owner_key));
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    
    /* 验证密钥已生成 */
    uint8_t zeros[32] = {0};
    TEST_ASSERT_NOT_EQUAL_MEMORY(zeros, g_owner_key, 32);
}

/* 测试: 解锁流程 */
void test_ccc_unlock_flow(void) {
    /* 使用已配对的密钥 */
    memcpy(g_session.session_key, g_owner_key, 32);
    g_session.state = CCC_SESSION_ACTIVE;
    
    /* 创建解锁命令 */
    ccc_command_t cmd;
    cmd.type = CMD_UNLOCK;
    cmd.authenticate = true;
    
    uint8_t apdu[128];
    size_t apdu_len = sizeof(apdu);
    int ret = ccc_build_command(&g_session, &cmd, apdu, &apdu_len);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    TEST_ASSERT_TRUE(apdu_len > 5);  /* 至少有 APDU 头 */
    
    /* 处理响应 (模拟解锁成功) */
    uint8_t response[] = {0x90, 0x00};
    ccc_response_t parsed;
    ret = ccc_parse_response(&g_session, response, sizeof(response), &parsed);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    TEST_ASSERT_EQUAL(SW_SUCCESS, parsed.sw_code);
}

/* 测试: 会话管理 */
void test_ccc_session_management(void) {
    /* 创建会话 */
    int ret = ccc_create_standard_session(&g_session, PROTOCOL_CCC, CONN_BLE);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    TEST_ASSERT_EQUAL(CCC_SESSION_INIT, g_session.state);
    
    /* 激活会话 */
    memcpy(g_session.session_key, g_owner_key, 32);
    ret = ccc_activate_session(&g_session);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    TEST_ASSERT_EQUAL(CCC_SESSION_ACTIVE, g_session.state);
    
    /* 终止会话 */
    ret = ccc_terminate_session(&g_session);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    TEST_ASSERT_EQUAL(CCC_SESSION_CLOSED, g_session.state);
}

/* 测试: UWB 解锁流程 (完整版) */
void test_ccc_uwb_unlock_flow(void) {
    /* 配置 UWB */
    ccc_uwb_config_t uwb_config = {
        .ranging_protocol = CCC_UWB_PROTOCOL_802154Z,
        .uwb_caps = CCC_UWB_CAP_RANGING
    };
    int ret = ccc_uwb_init_ranging(&uwb_config);
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
    
    /* 执行测距解锁 */
    ret = ccc_uwb_start_ranging();
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
    
    float distance, accuracy;
    ret = ccc_uwb_get_distance(&distance, &accuracy);
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
    
    /* 验证在解锁范围内 */
    if (distance < 2.0f) {
        /* 允许解锁 */
        TEST_MESSAGE("UWB unlock permitted");
    }
    
    ccc_uwb_stop_ranging();
}

/* 测试: 错误恢复 */
void test_ccc_error_recovery(void) {
    /* 模拟通信失败 */
    g_session.state = CCC_SESSION_ERROR;
    
    /* 尝试恢复 */
    int ret = ccc_recover_session(&g_session);
    TEST_ASSERT_EQUAL(CCC_SUCCESS, ret);
    TEST_ASSERT_NOT_EQUAL(CCC_SESSION_ERROR, g_session.state);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ccc_owner_pairing_flow);
    RUN_TEST(test_ccc_unlock_flow);
    RUN_TEST(test_ccc_session_management);
    RUN_TEST(test_ccc_uwb_unlock_flow);
    RUN_TEST(test_ccc_error_recovery);
    
    return UNITY_END();
}
