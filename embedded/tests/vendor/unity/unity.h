/**
 * Unity - A Minimalist Unit Testing Framework for C
 * This is a self-contained single-header-style implementation.
 */

#ifndef UNITY_H_
#define UNITY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 *  Unity Global State
 * ======================================================================== */
typedef struct {
    unsigned int num_tests;
    unsigned int num_failures;
    unsigned int num_ignored;
    const char *current_test;
    int          current_line;
    jmp_buf     abort_jmp;
    int          abort_active;
} Unity;

extern Unity UnityFixture;

/* ========================================================================
 *  Setup/Teardown (user must implement)
 * ======================================================================== */
void setUp(void);
void tearDown(void);

/* ========================================================================
 *  Test Runner
 * ======================================================================== */
#define RUN_TEST(func) do { \
    UnityFixture.current_test = #func; \
    setUp(); \
    if (setjmp(UnityFixture.abort_jmp) == 0) { \
        UnityFixture.abort_active = 1; \
        func(); \
    } \
    UnityFixture.abort_active = 0; \
    tearDown(); \
    UnityFixture.num_tests++; \
    printf("  ✓ %s\n", #func); \
} while(0)

/* ========================================================================
 *  Assertions
 * ======================================================================== */

/* Boolean */
#define TEST_ASSERT_TRUE(condition) \
    do { if (!(condition)) { _unity_fail("Expected TRUE but was FALSE", __LINE__); return; } } while(0)

#define TEST_ASSERT_FALSE(condition) \
    do { if ((condition)) { _unity_fail("Expected FALSE but was TRUE", __LINE__); return; } } while(0)

/* Integer equality — generic and typed */
#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        int _e = (int)(expected); \
        int _a = (int)(actual); \
        if (_e != _a) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected %d but was %d", _e, _a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    do { \
        int _e = (int)(expected); \
        int _a = (int)(actual); \
        if (_e != _a) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected %d but was %d", _e, _a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_UINT(expected, actual) \
    do { \
        unsigned int _e = (unsigned int)(expected); \
        unsigned int _a = (unsigned int)(actual); \
        if (_e != _a) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected %u but was %u", _e, _a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_INT32(expected, actual) \
    do { \
        int32_t _e = (int32_t)(expected); \
        int32_t _a = (int32_t)(actual); \
        if (_e != _a) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected %d but was %d", (int)_e, (int)_a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual) \
    do { \
        uint8_t _e = (uint8_t)(expected); \
        uint8_t _a = (uint8_t)(actual); \
        if (_e != _a) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected 0x%02X but was 0x%02X", _e, _a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual) \
    do { \
        uint16_t _e = (uint16_t)(expected); \
        uint16_t _a = (uint16_t)(actual); \
        if (_e != _a) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected %u but was %u", (unsigned)_e, (unsigned)_a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_HEX8(expected, actual) \
    TEST_ASSERT_EQUAL_UINT8(expected, actual)

#define TEST_ASSERT_EQUAL_HEX16(expected, actual) \
    TEST_ASSERT_EQUAL_UINT16(expected, actual)

/* Pointer */
#define TEST_ASSERT_NULL(pointer) \
    do { if ((pointer) != NULL) { _unity_fail("Expected NULL", __LINE__); return; } } while(0)

#define TEST_ASSERT_NOT_NULL(pointer) \
    do { if ((pointer) == NULL) { _unity_fail("Expected non-NULL", __LINE__); return; } } while(0)

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    do { \
        int _e = (int)(expected); \
        int _a = (int)(actual); \
        if (_e == _a) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected not equal to %d", _e); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

/* String */
#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { \
        const char *_e = (expected); \
        const char *_a = (actual); \
        if (strcmp(_e, _a) != 0) { \
            char _buf[256]; \
            snprintf(_buf, sizeof(_buf), "Expected \"%s\" but was \"%s\"", _e, _a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

/* Memory */
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) \
    do { \
        if (memcmp((expected), (actual), (len)) != 0) { \
            _unity_fail("Memory mismatch", __LINE__); \
            return; \
        } \
    } while(0)

/* Range */
#define TEST_ASSERT_INT_WITHIN(delta, expected, actual) \
    do { \
        int _d = (int)(delta); \
        int _e = (int)(expected); \
        int _a = (int)(actual); \
        if (_a < _e - _d || _a > _e + _d) { \
            char _buf[128]; \
            snprintf(_buf, sizeof(_buf), "Expected %d±%d but was %d", _e, _d, _a); \
            _unity_fail(_buf, __LINE__); \
            return; \
        } \
    } while(0)

/* Ignore */
#define TEST_IGNORE() \
    do { \
        UnityFixture.num_ignored++; \
        printf("  ⚠ IGNORED: %s\n", UnityFixture.current_test); \
        longjmp(UnityFixture.abort_jmp, 1); \
    } while(0)

#define TEST_IGNORE_MESSAGE(msg) \
    do { \
        UnityFixture.num_ignored++; \
        printf("  ⚠ IGNORED (%s): %s\n", msg, UnityFixture.current_test); \
        longjmp(UnityFixture.abort_jmp, 1); \
    } while(0)

/* ========================================================================
 *  Internal
 * ======================================================================== */
void _unity_fail(const char *msg, int line);

/* ========================================================================
 *  Main macro
 * ======================================================================== */
#define UNITY_BEGIN() do { \
    memset(&UnityFixture, 0, sizeof(UnityFixture)); \
    printf("\n=== Unity Test Run ===\n\n"); \
} while(0)

#define UNITY_END() do { \
    printf("\n=== Results: %d tests, %d failures, %d ignored ===\n\n", \
           UnityFixture.num_tests, \
           UnityFixture.num_failures, \
           UnityFixture.num_ignored); \
    return UnityFixture.num_failures; \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H_ */
