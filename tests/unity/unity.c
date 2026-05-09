/** @file unity.c
 * @brief Unity测试框架实现
 *
 * @copyright Copyright (c) 2024 YuleTech
 * @license MIT
 */

#include "unity.h"
#include <stdlib.h>

/* ============================================================================
 * 全局状态
 * ============================================================================ */

Unity_Struct Unity;

/* 默认的setUp和tearDown空函数 */
__attribute__((weak)) void setUp(void) {
    /* 用户可重写 */
}

__attribute__((weak)) void tearDown(void) {
    /* 用户可重写 */
}

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

static void UnityPrintHeader(void) {
    printf("\n");
    printf("========================================\n");
    printf("    Unity Test Framework v%d.%d.%d\n",
           UNITY_VERSION_MAJOR, UNITY_VERSION_MINOR, UNITY_VERSION_PATCH);
    printf("========================================\n");
    printf("\n");
}

static void UnityPrintTestStart(const char* test_name) {
    printf("TEST: %-40s ", test_name);
    fflush(stdout);
}

static void UnityPrintTestPass(void) {
    printf("[PASS]\n");
}

static void UnityPrintTestFail(void) {
    printf("[FAIL]\n");
    printf("      File: %s:%d\n", Unity.test_file ? Unity.test_file : "unknown", Unity.test_line);
    printf("      Msg:  %s\n", Unity.output_buffer);
}

static void UnityPrintTestIgnore(void) {
    printf("[IGNORE]\n");
    if (Unity.output_buffer[0] != '\0') {
        printf("      Reason: %s\n", Unity.output_buffer);
    }
}

/* ============================================================================
 * 公共API实现
 * ============================================================================ */

void UnityBegin(void) {
    /* 清零状态 */
    memset(&Unity, 0, sizeof(Unity_Struct));
    Unity.output_buffer[0] = '\0';
    
    /* 打印标题 */
    UnityPrintHeader();
}

int UnityEnd(void) {
    /* 打印测试摘要 */
    UnityPrintSummary();
    
    /* 返回退出码: 0=成功, 1=失败 */
    return (Unity.fail_count > 0) ? 1 : 0;
}

void UnityRunTest(void (*test_func)(void), const char* test_name, int line) {
    (void)line;
    
    /* 更新当前测试名称 */
    Unity.current_test_name = test_name;
    Unity.current_test_failed = false;
    Unity.current_test_ignored = false;
    Unity.test_count++;
    
    /* 打印测试开始 */
    UnityPrintTestStart(test_name);
    
    /* 设置跳转点以处理测试失败 */
    if (setjmp(Unity.env) == 0) {
        /* 调用setUp */
        setUp();
        
        /* 运行测试函数 */
        test_func();
        
        /* 调用tearDown */
        tearDown();
        
        /* 测试通过 */
        Unity.pass_count++;
        UnityPrintTestPass();
    } else {
        /* 从longjmp返回 - 测试失败或忽略 */
        tearDown();
        
        if (Unity.current_test_ignored) {
            Unity.ignore_count++;
            UnityPrintTestIgnore();
        } else {
            Unity.fail_count++;
            UnityPrintTestFail();
        }
    }
}

void UnityPrintSummary(void) {
    uint32_t total = Unity.test_count;
    uint32_t passed = Unity.pass_count;
    uint32_t failed = Unity.fail_count;
    uint32_t ignored = Unity.ignore_count;
    
    printf("\n");
    printf("----------------------------------------\n");
    printf("Test Summary:\n");
    printf("  Total:   %u\n", total);
    printf("  Passed:  %u\n", passed);
    printf("  Failed:  %u\n", failed);
    printf("  Ignored: %u\n", ignored);
    printf("----------------------------------------\n");
    
    if (failed == 0) {
        printf("\n*** ALL TESTS PASSED ***\n");
    } else {
        printf("\n*** %u TEST(S) FAILED ***\n", failed);
    }
    printf("\n");
}
