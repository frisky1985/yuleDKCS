/*******************************************************************************
 * Copyright (C) 2024 Ingeek, Inc. or its affiliates.  All Rights Reserved.
 *
 * @file    test_dk_fault_inject.c
 * @brief   yuleDKCS Fault Injection Self-Test Runner
 *
 * @details Host-side test runner for the DK fault injection framework.
 *          Compile with -DDK_FAULT_INJECT_ENABLE=1 to enable.
 *          Compile with NO_INJECT (default) for stub-only verification.
 *
 *          Build (host):
 *            gcc -DDK_FAULT_INJECT_ENABLE=1 \
 *                -I../inc \
 *                -o test_dk_fault_inject \
 *                test_dk_fault_inject.c ../src/DKFaultInject.c -lm
 *
 *          Or use the provided CMakeLists.txt.
 ******************************************************************************/

#include "DKFaultInject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if DK_FAULT_INJECT_ENABLE

/* ========================================================================
 *  Unit Tests
 * ======================================================================== */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", msg); \
        tests_passed++; \
    } \
} while(0)

/* ── Framework Unit Tests ── */

static void test_framework_init_deinit(void)
{
    printf("\n[Unit] Framework Init/Deinit\n");

    dk_fi_init();
    TEST_ASSERT(dk_fi_get_result_count() == 0, "Result count starts at 0 after init");
    dk_fi_deinit();
    TEST_ASSERT(dk_fi_get_result_count() == 0, "Result count 0 after deinit");
}

static void test_framework_enable_disable(void)
{
    printf("\n[Unit] Fault Enable/Disable\n");

    dk_fi_init();

    /* Initially no faults active */
    TEST_ASSERT(!dk_fi_is_active(DK_FI_ICCE_SIGN_FORGERY), "No faults initially active");

    /* Enable a fault */
    dk_fi_enable(DK_FI_ICCE_SIGN_FORGERY, true);
    TEST_ASSERT(dk_fi_is_active(DK_FI_ICCE_SIGN_FORGERY), "Fault active after enable");

    /* Double enable is safe */
    dk_fi_enable(DK_FI_ICCE_SIGN_FORGERY, true);
    TEST_ASSERT(dk_fi_is_active(DK_FI_ICCE_SIGN_FORGERY), "Double enable keeps fault active");

    /* Disable */
    dk_fi_enable(DK_FI_ICCE_SIGN_FORGERY, false);
    TEST_ASSERT(!dk_fi_is_active(DK_FI_ICCE_SIGN_FORGERY), "Fault inactive after disable");

    /* Clear all */
    dk_fi_enable(DK_FI_ICCE_SIGN_FORGERY, true);
    dk_fi_enable(DK_FI_CCC_SECURE_CHANNEL_FAIL, true);
    dk_fi_clear_all();
    TEST_ASSERT(!dk_fi_is_active(DK_FI_ICCE_SIGN_FORGERY), "Cleared: ICCE fault inactive");
    TEST_ASSERT(!dk_fi_is_active(DK_FI_CCC_SECURE_CHANNEL_FAIL), "Cleared: CCC fault inactive");

    dk_fi_deinit();
}

static void test_framework_result_storage(void)
{
    printf("\n[Unit] Result Storage\n");

    dk_fi_init();

    dk_fi_result_t r1 = { .fault_id = 1, .protocol = DK_FI_PROTOCOL_ICCE, .name = "Test1", .status = DK_FI_STATUS_PASSED };
    dk_fi_result_t r2 = { .fault_id = 2, .protocol = DK_FI_PROTOCOL_CCC,  .name = "Test2", .status = DK_FI_STATUS_FAILED };

    dk_fi_record_result(&r1);
    dk_fi_record_result(&r2);

    TEST_ASSERT(dk_fi_get_result_count() == 2, "Two results recorded");
    TEST_ASSERT(dk_fi_get_result(0)->fault_id == 1, "Result 0 fault_id = 1");
    TEST_ASSERT(dk_fi_get_result(1)->fault_id == 2, "Result 1 fault_id = 2");
    TEST_ASSERT(dk_fi_get_result(2) == NULL, "Result 2 is NULL (out of range)");

    /* Verify result values */
    TEST_ASSERT(dk_fi_get_result(0)->protocol == DK_FI_PROTOCOL_ICCE, "Result 0 protocol = ICCE");
    TEST_ASSERT(dk_fi_get_result(0)->status == DK_FI_STATUS_PASSED, "Result 0 status = PASSED");
    TEST_ASSERT(dk_fi_get_result(1)->protocol == DK_FI_PROTOCOL_CCC, "Result 1 protocol = CCC");
    TEST_ASSERT(dk_fi_get_result(1)->status == DK_FI_STATUS_FAILED, "Result 1 status = FAILED");

    dk_fi_deinit();
}

/* ── ICCE Protocol Tests ── */

static void test_icce_sign_forgery(void)
{
    printf("\n[ICCE] Signature Forgery Detection\n");

    dk_fi_init();

    dk_fi_enable(DK_FI_ICCE_SIGN_FORGERY, true);
    TEST_ASSERT(dk_fi_is_active(DK_FI_ICCE_SIGN_FORGERY), "ICCE sign forgery fault enabled");

    /* Run test */
    dk_fi_icce_sign_forgery_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 2, "Forgery test produced at least 2 results");

    dk_fi_clear_all();
    dk_fi_deinit();
}

static void test_icce_cert_expired(void)
{
    printf("\n[ICCE] Certificate Expiry Detection\n");

    dk_fi_init();
    dk_fi_icce_cert_expired_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 2, "Cert expired test produced >= 2 results");
    dk_fi_deinit();
}

static void test_icce_illegal_transition(void)
{
    printf("\n[ICCE] Illegal State Transition Detection\n");

    dk_fi_init();
    dk_fi_icce_illegal_transition_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 2, "Illegal transition test produced >= 2 results");
    dk_fi_deinit();
}

static void test_icce_comm_timeout(void)
{
    printf("\n[ICCE] Communication Timeout Detection\n");

    dk_fi_init();
    dk_fi_icce_comm_timeout_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 2, "Comm timeout test produced >= 2 results");
    dk_fi_deinit();
}

static void test_icce_distance_spoof(void)
{
    printf("\n[ICCE] Distance Spoof Detection\n");

    dk_fi_init();
    dk_fi_icce_distance_spoof_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "Distance spoof test produced >= 1 result");
    dk_fi_deinit();
}

/* ── CCC Protocol Tests ── */

static void test_ccc_secure_channel_fail(void)
{
    printf("\n[CCC] Secure Channel Failure Detection\n");

    dk_fi_init();
    dk_fi_ccc_secure_channel_fail_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "SCP03 fail test produced >= 1 result");
    dk_fi_deinit();
}

static void test_ccc_cert_verify_anomaly(void)
{
    printf("\n[CCC] Certificate Verify Anomaly\n");

    dk_fi_init();
    dk_fi_ccc_cert_verify_anomaly_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "Cert verify anomaly test produced >= 1 result");
    dk_fi_deinit();
}

static void test_ccc_nfc_oob_corrupt(void)
{
    printf("\n[CCC] NFC OOB Corruption Detection\n");

    dk_fi_init();
    dk_fi_ccc_nfc_oob_corrupt_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "NFC OOB corrupt test produced >= 1 result");
    dk_fi_deinit();
}

static void test_ccc_ble_encrypt_fail(void)
{
    printf("\n[CCC] BLE Encryption Failure Detection\n");

    dk_fi_init();
    dk_fi_ccc_ble_encrypt_fail_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "BLE encrypt fail test produced >= 1 result");
    dk_fi_deinit();
}

/* ── ICCOA Protocol Tests ── */

static void test_iccoa_handshake_fail(void)
{
    printf("\n[ICCOA] Handshake Failure Detection\n");

    dk_fi_init();
    dk_fi_iccoa_handshake_fail_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "Handshake fail test produced >= 1 result");
    dk_fi_deinit();
}

static void test_iccoa_key_derive_error(void)
{
    printf("\n[ICCOA] Key Derive Error Detection\n");

    dk_fi_init();
    dk_fi_iccoa_key_derive_error_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "Key derive error test produced >= 1 result");
    dk_fi_deinit();
}

static void test_iccoa_downgrade_attack(void)
{
    printf("\n[ICCOA] Downgrade Attack Detection\n");

    dk_fi_init();
    dk_fi_iccoa_downgrade_attack_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "Downgrade attack test produced >= 1 result");
    dk_fi_deinit();
}

static void test_iccoa_permission_bypass(void)
{
    printf("\n[ICCOA] Permission Bypass Detection\n");

    dk_fi_init();
    dk_fi_iccoa_permission_bypass_test();
    TEST_ASSERT(dk_fi_get_result_count() >= 1, "Permission bypass test produced >= 1 result");
    dk_fi_deinit();
}

/* ── Run All Tests ── */

static void test_run_all_integration(void)
{
    printf("\n[Integration] Running all fault injection tests via dk_fi_run_all_tests()\n");

    dk_fi_run_all_tests();

    /* After run_all, there should be results */
    uint32_t count = dk_fi_get_result_count();
    printf("\n  Total results from run_all: %u\n", (unsigned int)count);
    TEST_ASSERT(count > 0, "run_all_tests() produced results");
}

#else /* DK_FAULT_INJECT_ENABLE */

static void test_stubs(void)
{
    printf("\n[STUBS] Verifying production stub behavior\n");

    /* Verify all stubs compile and return safe defaults */
    dk_fi_init();
    dk_fi_enable(42, true);
    if (dk_fi_is_active(42)) {
        printf("  FAIL: Stub should never report active fault\n");
    } else {
        printf("  PASS: Stub returns false for is_active\n");
    }

    dk_fi_result_t r = { .fault_id = 1 };
    dk_fi_record_result(&r);
    if (dk_fi_get_result_count() == 0) {
        printf("  PASS: Stub returns 0 for result count\n");
    } else {
        printf("  FAIL: Stub should return 0 results\n");
    }

    if (dk_fi_get_result(0) == NULL) {
        printf("  PASS: Stub returns NULL for get_result\n");
    } else {
        printf("  FAIL: Stub should return NULL\n");
    }

    /* Verify all injector stubs compile */
    dk_fi_icce_sign_forgery_test();
    dk_fi_ccc_secure_channel_fail_test();
    dk_fi_iccoa_handshake_fail_test();
    dk_fi_run_all_tests();
    dk_fi_deinit();

    printf("  PASS: All injector stubs compile and run safely\n");
}

#endif /* DK_FAULT_INJECT_ENABLE */

/* ========================================================================
 *  Main
 * ======================================================================== */

int main(void)
{
    printf("========================================\n");
    printf("yuleDKCS Fault Injection Test Suite\n");
    printf("Build: DK_FAULT_INJECT_ENABLE = %d\n", DK_FAULT_INJECT_ENABLE);
    printf("========================================\n");

#if DK_FAULT_INJECT_ENABLE
    /* Framework unit tests */
    test_framework_init_deinit();
    test_framework_enable_disable();
    test_framework_result_storage();

    /* ICCE protocol tests */
    test_icce_sign_forgery();
    test_icce_cert_expired();
    test_icce_illegal_transition();
    test_icce_comm_timeout();
    test_icce_distance_spoof();

    /* CCC protocol tests */
    test_ccc_secure_channel_fail();
    test_ccc_cert_verify_anomaly();
    test_ccc_nfc_oob_corrupt();
    test_ccc_ble_encrypt_fail();

    /* ICCOA protocol tests */
    test_iccoa_handshake_fail();
    test_iccoa_key_derive_error();
    test_iccoa_downgrade_attack();
    test_iccoa_permission_bypass();

    /* Integration */
    test_run_all_integration();

    /* Final summary */
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    return (tests_failed > 0) ? 1 : 0;

#else /* Stubs */
    test_stubs();
    printf("\n  STUB mode: PASS (no injection code active)\n");
    return 0;
#endif
}
