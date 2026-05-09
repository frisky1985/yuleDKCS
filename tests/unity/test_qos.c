/** @file test_qos.c
 * @brief QoS策略测试
 *
 * @copyright Copyright (c) 2024 YuleTech
 * @license MIT
 */

#include "unity.h"
#include "microdds/qos.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_topic_qos_init_default(void) {
    DDS_TopicQos qos;
    DDS_ReturnCode_t rc = DDS_TopicQos_init_default(&qos);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    TEST_ASSERT_EQUAL(DDS_VOLATILE_DURABILITY_QOS, qos.durability.kind);
    TEST_ASSERT_EQUAL(DDS_BEST_EFFORT_RELIABILITY_QOS, qos.reliability.kind);
    TEST_ASSERT_EQUAL(DDS_KEEP_LAST_HISTORY_QOS, qos.history.kind);
    TEST_ASSERT_EQUAL(1, qos.history.depth);
}

void test_writer_qos_init_default(void) {
    DDS_DataWriterQos qos;
    DDS_ReturnCode_t rc = DDS_DataWriterQos_init_default(&qos);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    TEST_ASSERT_EQUAL(DDS_VOLATILE_DURABILITY_QOS, qos.durability.kind);
    TEST_ASSERT_TRUE(qos.writer_data_lifecycle.autodispose_unregistered_instances);
}

void test_reader_qos_init_default(void) {
    DDS_DataReaderQos qos;
    DDS_ReturnCode_t rc = DDS_DataReaderQos_init_default(&qos);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    TEST_ASSERT_EQUAL(DDS_VOLATILE_DURABILITY_QOS, qos.durability.kind);
}

void test_qos_copy_topic(void) {
    DDS_TopicQos qos1, qos2;
    DDS_TopicQos_init_default(&qos1);
    qos1.history.depth = 10;
    
    DDS_ReturnCode_t rc = DDS_TopicQos_copy(&qos2, &qos1);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    TEST_ASSERT_EQUAL(10, qos2.history.depth);
}

void test_qos_is_compatible_matching(void) {
    DDS_TopicQos offered, requested;
    DDS_TopicQos_init_default(&offered);
    DDS_TopicQos_init_default(&requested);
    
    bool compatible = DDS_TopicQos_is_compatible(&offered, &requested);
    TEST_ASSERT_TRUE(compatible);
}

void test_qos_is_compatible_not_matching(void) {
    DDS_TopicQos offered, requested;
    DDS_TopicQos_init_default(&offered);
    DDS_TopicQos_init_default(&requested);
    
    offered.reliability.kind = DDS_BEST_EFFORT_RELIABILITY_QOS;
    requested.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    
    bool compatible = DDS_TopicQos_is_compatible(&offered, &requested);
    TEST_ASSERT_FALSE(compatible);
}

void test_qos_null_pointer(void) {
    DDS_ReturnCode_t rc = DDS_TopicQos_init_default(NULL);
    TEST_ASSERT_EQUAL(DDS_RETCODE_BAD_PARAMETER, rc);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_topic_qos_init_default);
    RUN_TEST(test_writer_qos_init_default);
    RUN_TEST(test_reader_qos_init_default);
    RUN_TEST(test_qos_copy_topic);
    RUN_TEST(test_qos_is_compatible_matching);
    RUN_TEST(test_qos_is_compatible_not_matching);
    RUN_TEST(test_qos_null_pointer);
    
    return UNITY_END();
}
