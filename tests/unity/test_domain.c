/** @file test_domain.c
 * @brief 域参与者功能测试
 *
 * @copyright Copyright (c) 2024 YuleTech
 * @license MIT
 */

#include "unity.h"
#include "microdds/microdds.h"

/* ============================================================================
 * 测试前后置处理
 * ============================================================================ */

void setUp(void) {
    /* 每个测试前重新初始化Micro-DDS */
    MicroDDS_shutdown();
    MicroDDS_init();
}

void tearDown(void) {
    /* 每个测试后清理 */
    MicroDDS_shutdown();
}

/* ============================================================================
 * 域参与者创建测试
 * ============================================================================ */

void test_DomainParticipant_create_with_default_qos(void) {
    DDS_DomainParticipant participant;
    
    participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(participant);
    
    DDS_DomainParticipant_delete(participant);
}

void test_DomainParticipant_create_with_custom_qos(void) {
    DDS_DomainParticipant participant;
    DDS_DomainParticipantQos qos;
    
    DDS_DomainParticipantQos_init_default(&qos);
    qos.entity_factory.autoenable_created_entities = false;
    
    participant = DDS_DomainParticipant_create(1, &qos);
    TEST_ASSERT_NOT_NULL(participant);
    
    DDS_DomainParticipant_delete(participant);
}

void test_DomainParticipant_create_multiple(void) {
    DDS_DomainParticipant p1, p2, p3;
    
    p1 = DDS_DomainParticipant_create(0, NULL);
    p2 = DDS_DomainParticipant_create(1, NULL);
    p3 = DDS_DomainParticipant_create(2, NULL);
    
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p3);
    
    DDS_DomainParticipant_delete(p1);
    DDS_DomainParticipant_delete(p2);
    DDS_DomainParticipant_delete(p3);
}

void test_DomainParticipant_create_exceed_limit(void) {
    DDS_DomainParticipant participants[8];
    uint32_t i;
    
    /* 创建超过限制的域参与者 */
    for (i = 0; i < 8; i++) {
        participants[i] = DDS_DomainParticipant_create(i, NULL);
        if (participants[i] == NULL) {
            break;
        }
    }
    
    /* 最少应该能创建1个 */
    TEST_ASSERT_GREATER_THAN(0, i);
    
    /* 清理 */
    for (uint32_t j = 0; j < i; j++) {
        if (participants[j] != NULL) {
            DDS_DomainParticipant_delete(participants[j]);
        }
    }
}

/* ============================================================================
 * 域参与者删除测试
 * ============================================================================ */

void test_DomainParticipant_delete_valid(void) {
    DDS_DomainParticipant participant;
    DDS_ReturnCode_t ret;
    
    participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(participant);
    
    ret = DDS_DomainParticipant_delete(participant);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
}

void test_DomainParticipant_delete_null(void) {
    DDS_ReturnCode_t ret;
    
    ret = DDS_DomainParticipant_delete(NULL);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
}

void test_DomainParticipant_delete_already_deleted(void) {
    DDS_DomainParticipant participant;
    DDS_ReturnCode_t ret;
    
    participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(participant);
    
    DDS_DomainParticipant_delete(participant);
    ret = DDS_DomainParticipant_delete(participant);
    
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_ALREADY_DELETED, ret);
}

/* ============================================================================
 * 域参与者QoS测试
 * ============================================================================ */

void test_DomainParticipant_get_qos(void) {
    DDS_DomainParticipant participant;
    DDS_DomainParticipantQos qos;
    DDS_ReturnCode_t ret;
    
    participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(participant);
    
    ret = DDS_DomainParticipant_get_qos(participant, &qos);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
    
    DDS_DomainParticipant_delete(participant);
}

void test_DomainParticipant_get_qos_null_params(void) {
    DDS_DomainParticipant participant;
    DDS_DomainParticipantQos qos;
    DDS_ReturnCode_t ret;
    
    participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(participant);
    
    ret = DDS_DomainParticipant_get_qos(NULL, &qos);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
    
    ret = DDS_DomainParticipant_get_qos(participant, NULL);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
    
    DDS_DomainParticipant_delete(participant);
}

void test_DomainParticipant_set_qos(void) {
    DDS_DomainParticipant participant;
    DDS_DomainParticipantQos qos;
    DDS_ReturnCode_t ret;
    
    DDS_DomainParticipantQos_init_default(&qos);
    qos.entity_factory.autoenable_created_entities = false;
    
    participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(participant);
    
    ret = DDS_DomainParticipant_set_qos(participant, &qos);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
    
    /* 验证设置是否生效 */
    ret = DDS_DomainParticipant_get_qos(participant, &qos);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
    TEST_ASSERT_EQUAL_INT(false, qos.entity_factory.autoenable_created_entities);
    
    DDS_DomainParticipant_delete(participant);
}

void test_DomainParticipant_set_qos_null_params(void) {
    DDS_DomainParticipant participant;
    DDS_DomainParticipantQos qos;
    DDS_ReturnCode_t ret;
    
    participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(participant);
    
    DDS_DomainParticipantQos_init_default(&qos);
    
    ret = DDS_DomainParticipant_set_qos(NULL, &qos);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
    
    ret = DDS_DomainParticipant_set_qos(participant, NULL);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
    
    DDS_DomainParticipant_delete(participant);
}

/* ============================================================================
 * 域ID测试
 * ============================================================================ */

void test_DomainParticipant_different_domain_ids(void) {
    DDS_DomainParticipant p0, p1, p99;
    
    p0 = DDS_DomainParticipant_create(0, NULL);
    p1 = DDS_DomainParticipant_create(1, NULL);
    p99 = DDS_DomainParticipant_create(99, NULL);
    
    TEST_ASSERT_NOT_NULL(p0);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p99);
    
    TEST_ASSERT(p0 != p1);
    TEST_ASSERT(p1 != p99);
    
    DDS_DomainParticipant_delete(p0);
    DDS_DomainParticipant_delete(p1);
    DDS_DomainParticipant_delete(p99);
}

/* ============================================================================
 * 版本信息测试
 * ============================================================================ */

void test_MicroDDS_get_version_string(void) {
    const char* version;
    
    version = MicroDDS_get_version_string();
    TEST_ASSERT_NOT_NULL(version);
    TEST_ASSERT(strlen(version) > 0);
}

/* ============================================================================
 * 初始化/关闭测试
 * ============================================================================ */

void test_MicroDDS_init_shutdown_cycle(void) {
    DDS_ReturnCode_t ret;
    
    ret = MicroDDS_init();
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
    
    ret = MicroDDS_shutdown();
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
    
    /* 可以重复初始化 */
    ret = MicroDDS_init();
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
}

void test_MicroDDS_double_init(void) {
    DDS_ReturnCode_t ret;
    
    ret = MicroDDS_init();
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
    
    /* 再次初始化应该成功 */
    ret = MicroDDS_init();
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
}

void test_MicroDDS_shutdown_without_init(void) {
    DDS_ReturnCode_t ret;
    
    /* 确保未初始化 */
    MicroDDS_shutdown();
    
    ret = MicroDDS_shutdown();
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
}

/* ============================================================================
 * 测试主函数
 * ============================================================================ */

int main(void) {
    UnityBegin();
    
    /* 域参与者创建测试 */
    UnityRunTest(test_DomainParticipant_create_with_default_qos,
                 "test_DomainParticipant_create_with_default_qos", __LINE__);
    UnityRunTest(test_DomainParticipant_create_with_custom_qos,
                 "test_DomainParticipant_create_with_custom_qos", __LINE__);
    UnityRunTest(test_DomainParticipant_create_multiple,
                 "test_DomainParticipant_create_multiple", __LINE__);
    UnityRunTest(test_DomainParticipant_create_exceed_limit,
                 "test_DomainParticipant_create_exceed_limit", __LINE__);
    
    /* 域参与者删除测试 */
    UnityRunTest(test_DomainParticipant_delete_valid,
                 "test_DomainParticipant_delete_valid", __LINE__);
    UnityRunTest(test_DomainParticipant_delete_null,
                 "test_DomainParticipant_delete_null", __LINE__);
    UnityRunTest(test_DomainParticipant_delete_already_deleted,
                 "test_DomainParticipant_delete_already_deleted", __LINE__);
    
    /* 域参与者QoS测试 */
    UnityRunTest(test_DomainParticipant_get_qos,
                 "test_DomainParticipant_get_qos", __LINE__);
    UnityRunTest(test_DomainParticipant_get_qos_null_params,
                 "test_DomainParticipant_get_qos_null_params", __LINE__);
    UnityRunTest(test_DomainParticipant_set_qos,
                 "test_DomainParticipant_set_qos", __LINE__);
    UnityRunTest(test_DomainParticipant_set_qos_null_params,
                 "test_DomainParticipant_set_qos_null_params", __LINE__);
    
    /* 域ID测试 */
    UnityRunTest(test_DomainParticipant_different_domain_ids,
                 "test_DomainParticipant_different_domain_ids", __LINE__);
    
    /* 版本信息测试 */
    UnityRunTest(test_MicroDDS_get_version_string,
                 "test_MicroDDS_get_version_string", __LINE__);
    
    /* 初始化/关闭测试 */
    UnityRunTest(test_MicroDDS_init_shutdown_cycle,
                 "test_MicroDDS_init_shutdown_cycle", __LINE__);
    UnityRunTest(test_MicroDDS_double_init,
                 "test_MicroDDS_double_init", __LINE__);
    UnityRunTest(test_MicroDDS_shutdown_without_init,
                 "test_MicroDDS_shutdown_without_init", __LINE__);
    
    return UnityEnd();
}
