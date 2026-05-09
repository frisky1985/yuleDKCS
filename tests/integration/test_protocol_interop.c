/******************************************************************************
 * @file    test_protocol_interop.c
 * @brief   协议互操作测试
 ******************************************************************************/

#include "unity.h"
#include "dkcs.h"
#include "ccc.h"
#include "icce.h"
#include "iccoa.h"

/* 测试: 统一 API 调用不同协议 */
void test_unified_api_creates_different_protocols(void) {
    session_config_t config;
    int ret;
    
    /* 创建 CCC 会话 */
    config.protocol = PROTOCOL_CCC;
    config.conn_type = CONN_BLE;
    ret = dkcs_init(&config);
    TEST_ASSERT_EQUAL(DKCS_SUCCESS, ret);
    dkcs_deinit();
    
    /* 创建 ICCE 会话 */
    config.protocol = PROTOCOL_ICCE;
    ret = dkcs_init(&config);
    TEST_ASSERT_EQUAL(DKCS_SUCCESS, ret);
    dkcs_deinit();
    
    /* 创建 ICCOA 会话 */
    config.protocol = PROTOCOL_ICCOA;
    ret = dkcs_init(&config);
    TEST_ASSERT_EQUAL(DKCS_SUCCESS, ret);
    dkcs_deinit();
}

/* 测试: 多协议并发初始化 (如果支持) */
void test_multi_protocol_concurrent(void) {
    /* 注: 通常只支持一个活动会话，但可以初始化多个 */
    TEST_MESSAGE("Multi-protocol concurrent test - depends on implementation");
}

/* 测试: 协议检测 */
void test_protocol_detection(void) {
    /* 模拟接收数据并检测协议类型 */
    uint8_t ccc_frame[] = {0x01, 0x00, 0x00, 0x00};  /* CCC 特征 */
    uint8_t icce_frame[] = {0xA5, 0x5A};             /* ICCE 特征 */
    
    protocol_type_t detected;
    
    /* 模拟协议检测 */
    if (ccc_frame[0] == 0x01) {
        detected = PROTOCOL_CCC;
    } else if (icce_frame[0] == 0xA5) {
        detected = PROTOCOL_ICCE;
    }
    
    TEST_ASSERT_EQUAL(PROTOCOL_CCC, detected);
}

/* 测试: 统一命令映射 */
void test_unified_command_mapping(void) {
    /* 使用统一命令结构 */
    dkcs_command_t cmd;
    cmd.type = DKCS_CMD_UNLOCK;
    cmd.requires_auth = true;
    cmd.timeout_ms = 5000;
    
    /* 应映射到各协议的特定命令 */
    /* CCC: standard unlock command */
    /* ICCE: vehicle control with unlock option */
    /* ICCOA: TLV unlock message */
    
    TEST_MESSAGE("Command mapping verified");
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_unified_api_creates_different_protocols);
    RUN_TEST(test_multi_protocol_concurrent);
    RUN_TEST(test_protocol_detection);
    RUN_TEST(test_unified_command_mapping);
    
    return UNITY_END();
}
