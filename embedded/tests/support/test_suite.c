/**
 * test_suite.c — 车端 C 代码测试套件 — 全量运行入口
 *
 * Calls each test runner function in sequence via extern declarations.
 * No #include of test source files — those are compiled separately.
 */

#include <stdio.h>
#include <stdlib.h>

/* Test runners from each test module */
extern int run_iccoa_core_tests(void);
extern int run_iccoa_ble_tests(void);
extern int run_ccc_core_tests(void);
extern int run_unified_tests(void);

int main(void)
{
    int ret = 0;

    printf("\n============================================\n");
    printf("  车端 C 代码测试套件 — 全量运行\n");
    printf("============================================\n");

    printf("\n--- ICCOA Digital Key Core Tests ---\n");
    ret += run_iccoa_core_tests();

    printf("\n--- ICCOA BLE Tests ---\n");
    ret += run_iccoa_ble_tests();

    printf("\n--- CCC Digital Key Core Tests ---\n");
    ret += run_ccc_core_tests();

    printf("\n--- Unified Protocol Tests ---\n");
    ret += run_unified_tests();

    printf("\n============================================\n");
    if (ret == 0) {
        printf("  ✅ 全部测试通过!\n");
    } else {
        printf("  ❌ 存在 %d 个测试失败\n", ret);
    }
    printf("============================================\n\n");

    return ret;
}
