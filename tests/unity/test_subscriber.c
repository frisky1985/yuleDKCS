/** @file test_subscriber.c
 * @brief 订阅者功能测试
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
 * 订阅者创建测试
 * ============================================================================ */

void test_Subscriber_create_with_default_qos(void) {
    DDS_Subscriber subscriber;
    
    subscriber = DDS_Subscriber_create(g_participant, NULL);
    TEST_ASSERT_NOT_NULL(subscriber);
    
    DDS_Subscriber_delete(subscriber);
}

void test_Subscriber_create_with_custom_qos(void) {
    DDS_Subscriber subscriber;
    DDS_SubscriberQos qos;
    
    DDS_SubscriberQos_init_default(&qos);
    qos.entity_factory.autoenable_created_entities = false;
    
    subscriber = DDS_Subscriber_create(g_participant, &qos);
    TEST_ASSERT_NOT_NULL(subscriber);
    
    DDS_Subscriber_delete(subscriber);
}

void test_Subscriber_create_multiple(void) {
    DDS_Subscriber s1, s2, s3;
    
    s1 = DDS_Subscriber_create(g_participant, NULL);
    s2 = DDS_Subscriber_create(g_participant, NULL);
    s3 = DDS_Subscriber_create(g_participant, NULL);
    
    TEST_ASSERT_NOT_NULL(s1);
    TEST_ASSERT_NOT_NULL(s2);
    TEST_ASSERT_NOT_NULL(s3);
    
    DDS_Subscriber_delete(s1);
    DDS_Subscriber_delete(s2);
    DDS_Subscriber_delete(s3);
}

void test_Subscriber_create_null_participant(void) {
    DDS_Subscriber subscriber;
    
    subscriber = DDS_Subscriber_create(NULL, NULL);
    TEST_ASSERT_NULL(subscriber);
}

void test_Subscriber_create_exceed_limit(void) {
    DDS_Subscriber subscribers[16];
    uint32_t i;
    
    /* 创建超过限制的订阅者 */
    for (i = 0; i < 16; i++) {
        subscribers[i] = DDS_Subscriber_create(g_participant, NULL);
        if (subscribers[i] == NULL) {
            break;
        }
    }
    
    /* 至少应该能创建1个 */
    TEST_ASSERT_GREATER_THAN(0, i);
    
    /* 清理 */
    for (uint32_t j = 0; j < i; j++) {
        if (subscribers[j] != NULL) {
            DDS_Subscriber_delete(subscribers[j]);
        }
    }
}

/* ============================================================================
 * 订阅者删除测试
 * ============================================================================ */

void test_Subscriber_delete_valid(void) {
    DDS_Subscriber subscriber;
    DDS_ReturnCode_t ret;
    
    subscriber = DDS_Subscriber_create(g_participant, NULL);
    TEST_ASSERT_NOT_NULL(subscriber);
    
    ret = DDS_Subscriber_delete(subscriber);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
}

void test_Subscriber_delete_null(void) {
    DDS_ReturnCode_t ret;
    
    ret = DDS_Subscriber_delete(NULL);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
}

void test_Subscriber_delete_already_deleted(void) {
    DDS_Subscriber subscriber;
    DDS_ReturnCode_t ret;
    
    subscriber = DDS_Subscriber_create(g_participant, NULL);
    TEST_ASSERT_NOT_NULL(subscriber);
    
    DDS_Subscriber_delete(subscriber);
    ret = DDS_Subscriber_delete(subscriber);
    
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_ALREADY_DELETED, ret);
}

/* ============================================================================
 * 不同域参与者测试
 * ============================================================================ */

void test_Subscriber_different_participants(void) {
    DDS_DomainParticipant p1, p2;
    DDS_Subscriber sub1, sub2;
    
    p1 = DDS_DomainParticipant_create(0, NULL);
    p2 = DDS_DomainParticipant_create(1, NULL);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    
    sub1 = DDS_Subscriber_create(p1, NULL);
    sub2 = DDS_Subscriber_create(p2, NULL);
    
    TEST_ASSERT_NOT_NULL(sub1);
    TEST_ASSERT_NOT_NULL(sub2);
    TEST_ASSERT(sub1 != sub2);
    
    DDS_Subscriber_delete(sub1);
    DDS_Subscriber_delete(sub2);
    DDS_DomainParticipant_delete(p1);
    DDS_DomainParticipant_delete(p2);
}

/* ============================================================================
 * 发布者和订阅者组合测试
 * ============================================================================ */

void test_Publisher_and_Subscriber_same_participant(void) {
    DDS_Publisher publisher;
    DDS_Subscriber subscriber;
    
    publisher = DDS_Publisher_create(g_participant, NULL);
    subscriber = DDS_Subscriber_create(g_participant, NULL);
    
    TEST_ASSERT_NOT_NULL(publisher);
    TEST_ASSERT_NOT_NULL(subscriber);
    TEST_ASSERT(publisher != subscriber);
    
    DDS_Publisher_delete(publisher);
    DDS_Subscriber_delete(subscriber);
}

void test_Multiple_Publishers_Subscribers_same_participant(void) {
    DDS_Publisher p1, p2;
    DDS_Subscriber s1, s2;
    
    p1 = DDS_Publisher_create(g_participant, NULL);
    p2 = DDS_Publisher_create(g_participant, NULL);
    s1 = DDS_Subscriber_create(g_participant, NULL);
    s2 = DDS_Subscriber_create(g_participant, NULL);
    
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(s1);
    TEST_ASSERT_NOT_NULL(s2);
    
    TEST_ASSERT(p1 != p2);
    TEST_ASSERT(s1 != s2);
    TEST_ASSERT(p1 != s1);
    
    DDS_Publisher_delete(p1);
    DDS_Publisher_delete(p2);
    DDS_Subscriber_delete(s1);
    DDS_Subscriber_delete(s2);
}

/* ============================================================================
 * QoS配置测试
 * ============================================================================ */

void test_Subscriber_qos_presentation(void) {
    DDS_Subscriber subscriber;
    DDS_SubscriberQos qos;
    
    DDS_SubscriberQos_init_default(&qos);
    qos.presentation.access_scope = DDS_TOPIC_PRESENTATION_QOS;
    qos.presentation.coherent_access = true;
    qos.presentation.ordered_access = true;
    
    subscriber = DDS_Subscriber_create(g_participant, &qos);
    TEST_ASSERT_NOT_NULL(subscriber);
    
    DDS_Subscriber_delete(subscriber);
}

void test_Subscriber_qos_entity_factory(void) {
    DDS_Subscriber subscriber;
    DDS_SubscriberQos qos;
    
    DDS_SubscriberQos_init_default(&qos);
    qos.entity_factory.autoenable_created_entities = false;
    
    subscriber = DDS_Subscriber_create(g_participant, &qos);
    TEST_ASSERT_NOT_NULL(subscriber);
    
    DDS_Subscriber_delete(subscriber);
}

/* ============================================================================
 * 测试主函数
 * ============================================================================ */

int main(void) {
    UnityBegin();
    
    /* 订阅者创建测试 */
    UnityRunTest(test_Subscriber_create_with_default_qos,
                 "test_Subscriber_create_with_default_qos", __LINE__);
    UnityRunTest(test_Subscriber_create_with_custom_qos,
                 "test_Subscriber_create_with_custom_qos", __LINE__);
    UnityRunTest(test_Subscriber_create_multiple,
                 "test_Subscriber_create_multiple", __LINE__);
    UnityRunTest(test_Subscriber_create_null_participant,
                 "test_Subscriber_create_null_participant", __LINE__);
    UnityRunTest(test_Subscriber_create_exceed_limit,
                 "test_Subscriber_create_exceed_limit", __LINE__);
    
    /* 订阅者删除测试 */
    UnityRunTest(test_Subscriber_delete_valid,
                 "test_Subscriber_delete_valid", __LINE__);
    UnityRunTest(test_Subscriber_delete_null,
                 "test_Subscriber_delete_null", __LINE__);
    UnityRunTest(test_Subscriber_delete_already_deleted,
                 "test_Subscriber_delete_already_deleted", __LINE__);
    
    /* 不同域参与者测试 */
    UnityRunTest(test_Subscriber_different_participants,
                 "test_Subscriber_different_participants", __LINE__);
    
    /* 发布者和订阅者组合测试 */
    UnityRunTest(test_Publisher_and_Subscriber_same_participant,
                 "test_Publisher_and_Subscriber_same_participant", __LINE__);
    UnityRunTest(test_Multiple_Publishers_Subscribers_same_participant,
                 "test_Multiple_Publishers_Subscribers_same_participant", __LINE__);
    
    /* QoS配置测试 */
    UnityRunTest(test_Subscriber_qos_presentation,
                 "test_Subscriber_qos_presentation", __LINE__);
    UnityRunTest(test_Subscriber_qos_entity_factory,
                 "test_Subscriber_qos_entity_factory", __LINE__);
    
    return UnityEnd();
}
