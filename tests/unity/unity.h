/** @file unity.h
 * @brief Unity测试框架头文件
 *
 * @copyright Copyright (c) 2024 YuleTech
 * @license MIT
 *
 * 轻量级单元测试框架 - 专为嵌入式MCU设计
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 版本定义
 * ============================================================================ */
#define UNITY_VERSION_MAJOR 2
#define UNITY_VERSION_MINOR 5
#define UNITY_VERSION_PATCH 2

/* ============================================================================
 * 配置常量
 * ============================================================================ */
#ifndef UNITY_MAX_TEST_CASES
#define UNITY_MAX_TEST_CASES 64
#endif

#ifndef UNITY_OUTPUT_BUFFER_SIZE
#define UNITY_OUTPUT_BUFFER_SIZE 256
#endif

/* ============================================================================
 * 测试框架内部状态
 * ============================================================================ */
typedef struct {
    jmp_buf env;
    const char* test_file;
    int test_line;
    const char* current_test_name;
    uint32_t test_count;
    uint32_t pass_count;
    uint32_t fail_count;
    uint32_t ignore_count;
    bool current_test_failed;
    bool current_test_ignored;
    char output_buffer[UNITY_OUTPUT_BUFFER_SIZE];
} Unity_Struct;

extern Unity_Struct Unity;

/* ============================================================================
 * 内部宏 (用户代码不应直接使用)
 * ============================================================================ */
#define UNITY_TEST_ASSERT(condition, message, line, file) \
    do { \
        if (!(condition)) { \
            Unity.current_test_failed = true; \
            Unity.test_line = (line); \
            Unity.test_file = (file); \
            snprintf(Unity.output_buffer, UNITY_OUTPUT_BUFFER_SIZE, \
                     "Assertion failed: %s", (message)); \
            longjmp(Unity.env, 1); \
        } \
    } while (0)

#define UNITY_TEST_FAIL(message, line, file) \
    do { \
        Unity.current_test_failed = true; \
        Unity.test_line = (line); \
        Unity.test_file = (file); \
        strncpy(Unity.output_buffer, (message), UNITY_OUTPUT_BUFFER_SIZE - 1); \
        Unity.output_buffer[UNITY_OUTPUT_BUFFER_SIZE - 1] = '\0'; \
        longjmp(Unity.env, 1); \
    } while (0)

#define UNITY_TEST_IGNORE(message, line, file) \
    do { \
        Unity.current_test_ignored = true; \
        Unity.test_line = (line); \
        Unity.test_file = (file); \
        strncpy(Unity.output_buffer, (message), UNITY_OUTPUT_BUFFER_SIZE - 1); \
        Unity.output_buffer[UNITY_OUTPUT_BUFFER_SIZE - 1] = '\0'; \
        longjmp(Unity.env, 1); \
    } while (0)

/* ============================================================================
 * 公共断言宏
 * ============================================================================ */

/* 布尔断言 */
#define TEST_ASSERT(condition) \
    UNITY_TEST_ASSERT((condition), #condition, __LINE__, __FILE__)

#define TEST_ASSERT_TRUE(condition) \
    TEST_ASSERT((condition) == true)

#define TEST_ASSERT_FALSE(condition) \
    TEST_ASSERT((condition) == false)

#define TEST_ASSERT_NULL(pointer) \
    TEST_ASSERT((pointer) == NULL)

#define TEST_ASSERT_NOT_NULL(pointer) \
    TEST_ASSERT((pointer) != NULL)

/* 整数断言 */
#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        const int _expected = (expected); \
        const int _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %d but was %d", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    TEST_ASSERT_EQUAL(expected, actual)

#define TEST_ASSERT_EQUAL_UINT(expected, actual) \
    do { \
        const unsigned int _expected = (expected); \
        const unsigned int _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %u but was %u", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_INT8(expected, actual) \
    do { \
        const int8_t _expected = (expected); \
        const int8_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %d but was %d", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual) \
    do { \
        const uint8_t _expected = (expected); \
        const uint8_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %u but was %u", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_INT16(expected, actual) \
    do { \
        const int16_t _expected = (expected); \
        const int16_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %d but was %d", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual) \
    do { \
        const uint16_t _expected = (expected); \
        const uint16_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %u but was %u", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_INT32(expected, actual) \
    do { \
        const int32_t _expected = (expected); \
        const int32_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %ld but was %ld", (long)_expected, (long)_actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) \
    do { \
        const uint32_t _expected = (expected); \
        const uint32_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %lu but was %lu", (unsigned long)_expected, (unsigned long)_actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX8(expected, actual) \
    do { \
        const uint8_t _expected = (expected); \
        const uint8_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected 0x%02X but was 0x%02X", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX16(expected, actual) \
    do { \
        const uint16_t _expected = (expected); \
        const uint16_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected 0x%04X but was 0x%04X", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX32(expected, actual) \
    do { \
        const uint32_t _expected = (expected); \
        const uint32_t _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected 0x%08X but was 0x%08X", _expected, _actual); \
        UNITY_TEST_ASSERT((_expected) == (_actual), _msg, __LINE__, __FILE__); \
    } while (0)

/* 字符串断言 */
#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { \
        const char* _expected = (expected); \
        const char* _actual = (actual); \
        char _msg[256]; \
        snprintf(_msg, sizeof(_msg), "Expected \"%s\" but was \"%s\"", \
                 (_expected) ? _expected : "(null)", \
                 (_actual) ? _actual : "(null)"); \
        UNITY_TEST_ASSERT(strcmp((_expected) ? _expected : "", (_actual) ? _actual : "") == 0, \
                          _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len) \
    do { \
        const char* _expected = (expected); \
        const char* _actual = (actual); \
        char _msg[256]; \
        snprintf(_msg, sizeof(_msg), "Expected \"%.32s\" but was \"%.32s\"", \
                 (_expected) ? _expected : "(null)", \
                 (_actual) ? _actual : "(null)"); \
        UNITY_TEST_ASSERT(strncmp((_expected) ? _expected : "", \
                                  (_actual) ? _actual : "", (len)) == 0, \
                          _msg, __LINE__, __FILE__); \
    } while (0)

/* 内存比较断言 */
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) \
    do { \
        const void* _expected = (expected); \
        const void* _actual = (actual); \
        size_t _len = (len); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Memory mismatch at offset %%zu"); \
        UNITY_TEST_ASSERT(memcmp(_expected, _actual, _len) == 0, _msg, __LINE__, __FILE__); \
    } while (0)

/* 范围断言 */
#define TEST_ASSERT_GREATER_THAN(threshold, actual) \
    do { \
        const int _threshold = (threshold); \
        const int _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected value > %d but was %d", _threshold, _actual); \
        UNITY_TEST_ASSERT((_actual) > (_threshold), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_LESS_THAN(threshold, actual) \
    do { \
        const int _threshold = (threshold); \
        const int _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected value < %d but was %d", _threshold, _actual); \
        UNITY_TEST_ASSERT((_actual) < (_threshold), _msg, __LINE__, __FILE__); \
    } while (0)

#define TEST_ASSERT_IN_RANGE(minimum, maximum, actual) \
    do { \
        const int _min = (minimum); \
        const int _max = (maximum); \
        const int _actual = (actual); \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected value in range [%d, %d] but was %d", _min, _max, _actual); \
        UNITY_TEST_ASSERT(((_actual) >= (_min)) && ((_actual) <= (_max)), _msg, __LINE__, __FILE__); \
    } while (0)

/* 失败和忽略宏 */
#define TEST_FAIL_MESSAGE(message) \
    UNITY_TEST_FAIL(message, __LINE__, __FILE__)

#define TEST_IGNORE_MESSAGE(message) \
    UNITY_TEST_IGNORE(message, __LINE__, __FILE__)

/* ============================================================================
 * 测试控制函数
 * ============================================================================ */

/** @brief 初始化Unity测试框架 */
void UnityBegin(void);

/** @brief 结束测试并打印统计信息 */
int UnityEnd(void);

/** @brief 运行单个测试函数
 * @param test_func 测试函数指针
 * @param test_name 测试名称
 */
void UnityRunTest(void (*test_func)(void), const char* test_name, int line);

/** @brief 打印测试摘要 */
void UnityPrintSummary(void);

/* ============================================================================
 * setUp/tearDown钩子 (可选)
 * ============================================================================ */

/** @brief 在每个测试前调用 (用户可重写) */
void setUp(void);

/** @brief 在每个测试后调用 (用户可重写) */
void tearDown(void);

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H */
