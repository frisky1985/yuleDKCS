/** @file test_topic.c
 * @brief 主题功能测试
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
 * 主题创建测试
 * ============================================================================ */

void test_Topic_create_with_default_qos(void) {
    DDS_Topic topic;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", "TestType", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    DDS_Topic_delete(topic);
}

void test_Topic_create_with_custom_qos(void) {
    DDS_Topic topic;
    DDS_TopicQos qos;
    
    DDS_TopicQos_init_default(&qos);
    qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", "TestType", &qos);
    TEST_ASSERT_NOT_NULL(topic);
    
    DDS_Topic_delete(topic);
}

void test_Topic_create_multiple(void) {
    DDS_Topic t1, t2, t3;
    
    t1 = DDS_Topic_create(g_participant, "Topic1", "Type1", NULL);
    t2 = DDS_Topic_create(g_participant, "Topic2", "Type2", NULL);
    t3 = DDS_Topic_create(g_participant, "Topic3", "Type3", NULL);
    
    TEST_ASSERT_NOT_NULL(t1);
    TEST_ASSERT_NOT_NULL(t2);
    TEST_ASSERT_NOT_NULL(t3);
    
    DDS_Topic_delete(t1);
    DDS_Topic_delete(t2);
    DDS_Topic_delete(t3);
}

void test_Topic_create_null_participant(void) {
    DDS_Topic topic;
    
    topic = DDS_Topic_create(NULL, "TestTopic", "TestType", NULL);
    TEST_ASSERT_NULL(topic);
}

void test_Topic_create_null_name(void) {
    DDS_Topic topic;
    
    topic = DDS_Topic_create(g_participant, NULL, "TestType", NULL);
    TEST_ASSERT_NULL(topic);
}

void test_Topic_create_null_type_name(void) {
    DDS_Topic topic;
    const char* type_name;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", NULL, NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    /* 检查类型名称应该是空字符串 */
    type_name = DDS_Topic_get_type_name(topic);
    TEST_ASSERT_NOT_NULL(type_name);
    TEST_ASSERT_EQUAL_STRING("", type_name);
    
    DDS_Topic_delete(topic);
}

void test_Topic_create_exceed_limit(void) {
    DDS_Topic topics[16];
    uint32_t i;
    
    /* 创建超过限制的主题 */
    for (i = 0; i < 16; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Topic%u", i);
        topics[i] = DDS_Topic_create(g_participant, name, "Type", NULL);
        if (topics[i] == NULL) {
            break;
        }
    }
    
    /* 最少应该能创建1个 */
    TEST_ASSERT_GREATER_THAN(0, i);
    
    /* 清理 */
    for (uint32_t j = 0; j < i; j++) {
        if (topics[j] != NULL) {
            DDS_Topic_delete(topics[j]);
        }
    }
}

/* ============================================================================
 * 主题删除测试
 * ============================================================================ */

void test_Topic_delete_valid(void) {
    DDS_Topic topic;
    DDS_ReturnCode_t ret;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", "TestType", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    ret = DDS_Topic_delete(topic);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_OK, ret);
}

void test_Topic_delete_null(void) {
    DDS_ReturnCode_t ret;
    
    ret = DDS_Topic_delete(NULL);
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_BAD_PARAMETER, ret);
}

void test_Topic_delete_already_deleted(void) {
    DDS_Topic topic;
    DDS_ReturnCode_t ret;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", "TestType", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    DDS_Topic_delete(topic);
    ret = DDS_Topic_delete(topic);
    
    TEST_ASSERT_EQUAL_INT(DDS_RETCODE_ALREADY_DELETED, ret);
}

/* ============================================================================
 * 主题名称测试
 * ============================================================================ */

void test_Topic_get_name(void) {
    DDS_Topic topic;
    const char* name;
    
    topic = DDS_Topic_create(g_participant, "MyTestTopic", "TestType", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    name = DDS_Topic_get_name(topic);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("MyTestTopic", name);
    
    DDS_Topic_delete(topic);
}

void test_Topic_get_name_null(void) {
    const char* name;
    
    name = DDS_Topic_get_name(NULL);
    TEST_ASSERT_NULL(name);
}

void test_Topic_get_name_deleted(void) {
    DDS_Topic topic;
    const char* name;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", "TestType", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    DDS_Topic_delete(topic);
    name = DDS_Topic_get_name(topic);
    
    TEST_ASSERT_NULL(name);
}

void test_Topic_get_type_name(void) {
    DDS_Topic topic;
    const char* type_name;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", "MyTestType", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    type_name = DDS_Topic_get_type_name(topic);
    TEST_ASSERT_NOT_NULL(type_name);
    TEST_ASSERT_EQUAL_STRING("MyTestType", type_name);
    
    DDS_Topic_delete(topic);
}

void test_Topic_get_type_name_null(void) {
    const char* type_name;
    
    type_name = DDS_Topic_get_type_name(NULL);
    TEST_ASSERT_NULL(type_name);
}

void test_Topic_get_type_name_deleted(void) {
    DDS_Topic topic;
    const char* type_name;
    
    topic = DDS_Topic_create(g_participant, "TestTopic", "TestType", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    DDS_Topic_delete(topic);
    type_name = DDS_Topic_get_type_name(topic);
    
    TEST_ASSERT_NULL(type_name);
}

/* ============================================================================
 * 长名称测试
 * ============================================================================ */

void test_Topic_long_name(void) {
    DDS_Topic topic;
    const char* name;
    char long_name[256];
    
    /* 创建超长名称 */
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    
    topic = DDS_Topic_create(g_participant, long_name, "Type", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    name = DDS_Topic_get_name(topic);
    TEST_ASSERT_NOT_NULL(name);
    /* 名称应该被截断 */
    TEST_ASSERT(strlen(name) < sizeof(long_name));
    
    DDS_Topic_delete(topic);
}

void test_Topic_long_type_name(void) {
    DDS_Topic topic;
    const char* type_name;
    char long_type[256];
    
    /* 创建超长类型名 */
    memset(long_type, 'T', sizeof(long_type) - 1);
    long_type[sizeof(long_type) - 1] = '\0';
    
    topic = DDS_Topic_create(g_participant, "Topic", long_type, NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    type_name = DDS_Topic_get_type_name(topic);
    TEST_ASSERT_NOT_NULL(type_name);
    /* 类型名应该被截断 */
    TEST_ASSERT(strlen(type_name) < sizeof(long_type));
    
    DDS_Topic_delete(topic);
}

/* ============================================================================
 * 特殊字符测试
 * ============================================================================ */

void test_Topic_name_with_special_chars(void) {
    DDS_Topic topic;
    const char* name;
    
    topic = DDS_Topic_create(g_participant, "Topic_123-Test.Type", "Type", NULL);
    TEST_ASSERT_NOT_NULL(topic);
    
    name = DDS_Topic_get_name(topic);
    TEST_ASSERT_EQUAL_STRING("Topic_123-Test.Type", name);
    
    DDS_Topic_delete(topic);
}

/* ============================================================================
 * 测试主函数
 * ============================================================================ */

int main(void) {
    UnityBegin();
    
    /* 主题创建测试 */
    UnityRunTest(test_Topic_create_with_default_qos,
                 "test_Topic_create_with_default_qos", __LINE__);
    UnityRunTest(test_Topic_create_with_custom_qos,
                 "test_Topic_create_with_custom_qos", __LINE__);
    UnityRunTest(test_Topic_create_multiple,
                 "test_Topic_create_multiple", __LINE__);
    UnityRunTest(test_Topic_create_null_participant,
                 "test_Topic_create_null_participant", __LINE__);
    UnityRunTest(test_Topic_create_null_name,
                 "test_Topic_create_null_name", __LINE__);
    UnityRunTest(test_Topic_create_null_type_name,
                 "test_Topic_create_null_type_name", __LINE__);
    UnityRunTest(test_Topic_create_exceed_limit,
                 "test_Topic_create_exceed_limit", __LINE__);
    
    /* 主题删除测试 */
    UnityRunTest(test_Topic_delete_valid,
                 "test_Topic_delete_valid", __LINE__);
    UnityRunTest(test_Topic_delete_null,
                 "test_Topic_delete_null", __LINE__);
    UnityRunTest(test_Topic_delete_already_deleted,
                 "test_Topic_delete_already_deleted", __LINE__);
    
    /* 主题名称测试 */
    UnityRunTest(test_Topic_get_name,
                 "test_Topic_get_name", __LINE__);
    UnityRunTest(test_Topic_get_name_null,
                 "test_Topic_get_name_null", __LINE__);
    UnityRunTest(test_Topic_get_name_deleted,
                 "test_Topic_get_name_deleted", __LINE__);
    UnityRunTest(test_Topic_get_type_name,
                 "test_Topic_get_type_name", __LINE__);
    UnityRunTest(test_Topic_get_type_name_null,
                 "test_Topic_get_type_name_null", __LINE__);
    UnityRunTest(test_Topic_get_type_name_deleted,
                 "test_Topic_get_type_name_deleted", __LINE__);
    
    /* 长名称测试 */
    UnityRunTest(test_Topic_long_name,
                 "test_Topic_long_name", __LINE__);
    UnityRunTest(test_Topic_long_type_name,
                 "test_Topic_long_type_name", __LINE__);
    
    /* 特殊字符测试 */
    UnityRunTest(test_Topic_name_with_special_chars,
                 "test_Topic_name_with_special_chars", __LINE__);
    
    return UnityEnd();
}
