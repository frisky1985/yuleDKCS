/******************************************************************************
 * @file    test_uwb.c
 * @brief   UWB 测距单元测试
 ******************************************************************************/

#include "unity.h"
#include "uwb_hal.h"
#include "ccc_uwb.h"
#include <string.h>
#include <math.h>

void setUp(void) {
    uwb_driver_init();
}

void tearDown(void) {
    uwb_driver_deinit();
}

/* 测试: UWB 驱动初始化 */
void test_uwb_driver_init(void) {
    int ret = uwb_driver_init();
    TEST_ASSERT_EQUAL(UWB_SUCCESS, ret);
    
    uwb_driver_info_t info;
    ret = uwb_driver_get_info(&info);
    TEST_ASSERT_EQUAL(UWB_SUCCESS, ret);
    TEST_ASSERT_TRUE(info.version_major > 0 || info.version_minor > 0);
}

/* 测试: CCC UWB 会话 */
void test_ccc_uwb_session(void) {
    ccc_uwb_config_t config = {
        .uwb_session_id = {0x01, 0x02, 0x03, 0x04},
        .ranging_protocol = CCC_UWB_PROTOCOL_802154Z,
        .uwb_config_id = 0x01,
        .uwb_caps = CCC_UWB_CAP_RANGING | CCC_UWB_CAP_AOA
    };
    
    int ret = ccc_uwb_init_ranging(&config);
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
    
    ret = ccc_uwb_start_ranging();
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
    
    ret = ccc_uwb_stop_ranging();
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
}

/* 测试: 测距读取 */
void test_uwb_get_distance(void) {
    ccc_uwb_config_t config = {0};
    ccc_uwb_init_ranging(&config);
    ccc_uwb_start_ranging();
    
    float distance, accuracy;
    int ret = ccc_uwb_get_distance(&distance, &accuracy);
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
    
    /* 测距范围应在合理范围内 (0-100米) */
    TEST_ASSERT_FLOAT_WITHIN(100.0, 0.0, distance);
    TEST_ASSERT_FLOAT_WITHIN(1.0, 0.0, accuracy);
    TEST_ASSERT_TRUE(accuracy >= 0);
    
    ccc_uwb_stop_ranging();
}

/* 测试: STS 密钥配置 */
void test_ccc_uwb_sts_config(void) {
    uint8_t sts_key[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    
    int ret = ccc_uwb_configure_sts(sts_key, sizeof(sts_key), 0x01);
    TEST_ASSERT_EQUAL(CCC_UWB_SUCCESS, ret);
}

/* 测试: 无效参数处理 */
void test_uwb_invalid_params(void) {
    /* NULL 配置 */
    int ret = ccc_uwb_init_ranging(NULL);
    TEST_ASSERT_EQUAL(CCC_UWB_ERROR_INVALID_PARAM, ret);
    
    /* 未初始化尝试测距 */
    float dist, acc;
    ret = ccc_uwb_get_distance(&dist, &acc);
    TEST_ASSERT_NOT_EQUAL(CCC_UWB_SUCCESS, ret);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_uwb_driver_init);
    RUN_TEST(test_ccc_uwb_session);
    RUN_TEST(test_uwb_get_distance);
    RUN_TEST(test_ccc_uwb_sts_config);
    RUN_TEST(test_uwb_invalid_params);
    
    return UNITY_END();
}
