/** @file test_publisher.c
 * @brief 发布者功能测试
 *
 * @copyright Copyright (c) 2024 YuleTech
 * @license MIT
 */

#include "unity.h"
#include "microdds/microdds.h"

/* 全局测试资源 */
static DDS_DomainParticipant g_participant = NULL;

/* ============================================================================
 * 测试前后置处理
 * ============================================================================ */

void setUp(void) {
    MicroDDS_shutdown();
    MicroDDS_init();
    g_participant = DDS_DomainParticipant_create(0, NULL);
    TEST_ASSERT_NOT_NULL(g_participant);
}

void tearDown(void) {
    if (g_participant != NULL) {
        DDS_DomainParticipant_delete(g_participant);
        g_participant = NULL;
    }
    MicroDDS_shutdown();
}

/* ============================================================================
 * 发布者创建测试
 * ============================================================================ */

void test_Publisher_create_with_default_qos(void) {
    DDS_Publisher publisher;
    
    publisher = DDS_Publisher_create(g_participant, NULL);
    TEST_ASSERT_NOT_NULL(publisher);
    
    DDS_Publisher_delete(publisher);
}

void test_Publisher_create_with_custom_qos(void) {
    DDS_Publisher publisher;
    DDS_PublisherQos qos;
    
    DDS_PublisherQos_init_default(&qos);
    qos.entity_factory.autoenable_created_entities = false;
    
    publisher = DDS_Publisher_create(g_participant, &qos);
    TEST_ASSERT_NOT_NULL(publisher);
    
    DDS_Publisher_delete(publisher);
}

void test_Publisher_create_multiple(void) {
    DDS_Publisher p1, p2, p3;
    
    p1 = DDS_Publisher_create(g_participant, NULL);
    p2 = DDS_Publisher_create(g_participant, NULL);
    p3 = DDS_Publisher_create(g_participant, NULL);
    
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p3);
    
    DDS_Publisher_delete(p1);
    DDS_Publisher_delete(p2);
    DDS_Publisher_delete(p3);
}

void test_Publisher_create_null_participant(void) {
    DDS_Publisher publisher;
    
    publisher = DDS_Publisher_create(NULL, NULL);
    TEST_ASSERT_NULL(publisher);
}

void test_Publisher_create_exceed_limit(void) {
    DDS_Publisher publishers[16];
    uint32_t i;
    
    /* 创建超过限制的发布者 */
    for (i = 0; i < 16; i++) {
        publishers[i] = DDS_Publisher_create(g_participant, NULL);
        if (publishers[i] == NULL) {
            break;
        }
    }
    
    /* 至少应该能创建1个 */
    TEST_ASSERT_GREATER_THAN(0, i);
    
    /* 清理 */
    for (uint32_t j = 0; j < i; j++) {
        if (publishers[j] != NULL) {
            DDS_Publisher_delete(publishers[j]);
        }
    }
}

/* ============================================================================
 * 发布者删除测试
 * ============================================================================ */

void test_Publisher_delete_valid(void) {
    DDS_Publisher publisher;
    DDS_ReturnCode_t ret;
    
    publisher = DDS_Publisher_create(g_participant, NULL);
    TEST_ASSERT_NOT_NULL(publisher);
    
    ret = DDS_Publisher_delete(publisher);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
}

void test_Publisher_delete_null(void) {
    DDS_ReturnCode_t ret;
    
    ret = DDS_Publisher_delete(NULL);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
}

void test_Publisher_delete_already_deleted(void) {
    DDS_Publisher publisher;
    DDS_ReturnCode_t ret;
    
    publisher = DDS_Publisher_create(g_participant, NULL);
    TEST_ASSERT_NOT_NULL(publisher);
    
    DDS_Publisher_delete(publisher);
    ret = DDS_Publisher_delete(publisher);
    
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_ALREADY_DELETED, ret);
}

/* ============================================================================
 * 不同域参与者测试
 * ============================================================================ */

void test_Publisher_different_participants(void) {
    DDS_DomainParticipant p1, p2;
    DDS_Publisher pub1, pub2;
    
    p1 = DDS_DomainParticipant_create(0, NULL);
    p2 = DDS_DomainParticipant_create(1, NULL);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    
    pub1 = DDS_Publisher_create(p1, NULL);
    pub2 = DDS_Publisher_create(p2, NULL);
    
    TEST_ASSERT_NOT_NULL(pub1);
    TEST_ASSERT_NOT_NULL(pub2);
    TEST_ASSERT(pub1 != pub2);
    
    DDS_Publisher_delete(pub1);
    DDS_Publisher_delete(pub2);
    DDS_DomainParticipant_delete(p1);
    DDS_DomainParticipant_delete(p2);
}

/* ============================================================================
 * QoS配置测试
 * ============================================================================ */

void test_Publisher_qos_presentation(void) {
    DDS_Publisher publisher;
    DDS_PublisherQos qos;
    
    DDS_PublisherQos_init_default(&qos);
    qos.presentation.access_scope = DDS_TOPIC_PRESENTATION_QOS;
    qos.presentation.coherent_access = true;
    qos.presentation.ordered_access = true;
    
    publisher = DDS_Publisher_create(g_participant, &qos);
    TEST_ASSERT_NOT_NULL(publisher);
    
    DDS_Publisher_delete(publisher);
}

void test_Publisher_qos_entity_factory(void) {
    DDS_Publisher publisher;
    DDS_PublisherQos qos;
    
    DDS_PublisherQos_init_default(&qos);
    qos.entity_factory.autoenable_created_entities = false;
    
    publisher = DDS_Publisher_create(g_participant, &qos);
    TEST_ASSERT_NOT_NULL(publisher);
    
    DDS_Publisher_delete(publisher);
}

/* ============================================================================
 * 测试主函数
 * ============================================================================ */

int main(void) {
    UnityBegin();
    
    /* 发布者创建测试 */
    UnityRunTest(test_Publisher_create_with_default_qos,
                 "test_Publisher_create_with_default_qos", __LINE__);
    UnityRunTest(test_Publisher_create_with_custom_qos,
                 "test_Publisher_create_with_custom_qos", __LINE__);
    UnityRunTest(test_Publisher_create_multiple,
                 "test_Publisher_create_multiple", __LINE__);
    UnityRunTest(test_Publisher_create_null_participant,
                 "test_Publisher_create_null_participant", __LINE__);
    UnityRunTest(test_Publisher_create_exceed_limit,
                 "test_Publisher_create_exceed_limit", __LINE__);
    
    /* 发布者删除测试 */
    UnityRunTest(test_Publisher_delete_valid,
                 "test_Publisher_delete_valid", __LINE__);
    UnityRunTest(test_Publisher_delete_null,
                 "test_Publisher_delete_null", __LINE__);
    UnityRunTest(test_Publisher_delete_already_deleted,
                 "test_Publisher_delete_already_deleted", __LINE__);
    
    /* 不同域参与者测试 */
    UnityRunTest(test_Publisher_different_participants,
                 "test_Publisher_different_participants", __LINE__);
    
    /* QoS配置测试 */
    UnityRunTest(test_Publisher_qos_presentation,
                 "test_Publisher_qos_presentation", __LINE__);
    UnityRunTest(test_Publisher_qos_entity_factory,
                 "test_Publisher_qos_entity_factory", __LINE__);
    
    return UnityEnd();
}
