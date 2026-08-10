/**
 * test_suite.c — 车端 C 代码测试套件 — 全量运行入口
 *
 * Calls each test runner function in sequence via extern declarations.
 * No #include of test source files — those are compiled separately.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unity hooks — defined once for the merged suite. Individual test files
 * skip their own copies when compiled with TEST_LIB_MODE. */
void setUp(void) {}
void tearDown(void) {}

/* Test runners from each test module */
extern int run_iccoa_core_tests(void);
extern int run_iccoa_ble_tests(void);
extern int run_ccc_core_tests(void);
extern int run_icce_tests(void);
extern int run_unified_tests(void);
extern int run_edge_condition_tests(void);
extern int run_icce_edge_tests(void);
extern int run_sm_crypto_tests(void);
extern int run_se050_scp03_tests(void);

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

    printf("\n--- ICCE Protocol Tests ---\n");
    ret += run_icce_tests();

    printf("\n--- Unified Protocol Tests ---\n");
    ret += run_unified_tests();

    printf("\n--- Edge Condition Tree Tests ---\n");
    ret += run_edge_condition_tests();

    printf("\n--- ICCE Edge Rule Engine Tests ---\n");
    ret += run_icce_edge_tests();

    printf("\n--- SM Crypto (SM2/SM3/SM4) Tests ---\n");
    ret += run_sm_crypto_tests();

    printf("\n--- SE050 SCP03 Tests ---\n");
    ret += run_se050_scp03_tests();

    printf("\n============================================\n");
    if (ret == 0) {
        printf("  ✅ 全部测试通过!\n");
    } else {
        printf("  ❌ 存在 %d 个测试失败\n", ret);
    }
    printf("============================================\n\n");

    return ret;
}
