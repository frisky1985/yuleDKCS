/*
 * Host tests for the real KW47A MCUX flash adapter, HAL NV path and default
 * eFuse binding. The SDK flash API is backed by an mmap'd RAM region.
 */
#include "unity.h"
#include "fsl_flash.h"
#include "hal_interface.h"
#include "kw47a_flash_ll.h"
#include "kw47a_nv.h"
#include "kw47a_otp_ll.h"
#include "sec_nv.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int kw47a_flash_mcux_bind(void);

static void bind_parent_flash(void)
{
    TEST_ASSERT_EQUAL_INT(0,
        mcux_mock_flash_reset(SEC_NV_FLASH_BASE, SEC_NV_FLASH_SIZE, 0x2000u));
    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_mcux_bind());
    TEST_ASSERT_EQUAL_UINT(0x2000u, kw47a_flash_ll_page_size());
}

static int geometry_child(uint32_t sector_size, int expected_rc)
{
    int rc;

    if (mcux_mock_flash_reset(SEC_NV_FLASH_BASE, SEC_NV_FLASH_SIZE,
                              sector_size) != 0) {
        return 10;
    }
    rc = kw47a_flash_mcux_bind();
    if (rc != expected_rc) {
        return 11;
    }
    if (expected_rc == 0 && kw47a_flash_ll_page_size() != sector_size) {
        return 12;
    }
    if (expected_rc != 0 && kw47a_flash_ll_page_size() != 0u) {
        return 13;
    }
    return 0;
}

static void check_geometry(uint32_t sector_size, int expected_rc)
{
    pid_t pid;
    int status = 0;

    (void)fflush(NULL);
    pid = fork();
    if (pid < 0) {
        TEST_ASSERT_TRUE(pid >= 0);
    }
    if (pid == 0) {
        _exit(geometry_child(sector_size, expected_rc));
    }
    TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("geometry child failed: sector=%u expected=%d status=%d\n",
               (unsigned int)sector_size, expected_rc, status);
    }
    TEST_ASSERT_TRUE(WIFEXITED(status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(status));
}

static void test_flash_init_maps_geometry(void)
{
    flash_config_t config;

    TEST_ASSERT_EQUAL_INT(0,
        mcux_mock_flash_reset(SEC_NV_FLASH_BASE, SEC_NV_FLASH_SIZE, 0x1000u));
    (void)memset(&config, 0, sizeof(config));
    TEST_ASSERT_EQUAL_INT32(kStatus_Success, FLASH_Init(&config));
    TEST_ASSERT_EQUAL_UINT(SEC_NV_FLASH_BASE, config.pflashBlockBase);
    TEST_ASSERT_EQUAL_UINT(SEC_NV_FLASH_SIZE, config.pflashTotalSize);
    TEST_ASSERT_EQUAL_UINT(1u, config.pflashBlockCount);
    TEST_ASSERT_EQUAL_UINT(0x1000u, config.pflashSectorSize);
}

static void test_mcux_geometry_binding_gates(void)
{
    check_geometry(0x4000u, -1); /* 16KB sector cannot contain one 8KB Bank. */
    check_geometry(0x2000u, 0);
    check_geometry(0x1000u, 0);
    check_geometry(0x0400u, 0);
}

static void test_mcux_hal_nv_full_chain(void)
{
    static const uint8_t src[8] = { 0x00, 0x11, 0x22, 0x33,
                                    0x44, 0x55, 0x66, 0x77 };
    uint8_t out[8] = { 0 };
    const uint8_t *bytes;
    uint32_t i;

    bind_parent_flash();
    TEST_ASSERT_EQUAL_UINT(0x74000u, kw47a_nv_base());
    TEST_ASSERT_EQUAL_UINT(0xC000u, KW47A_NV_SIZE);

    TEST_ASSERT_EQUAL_INT(0,
        hal_nv_flash_write(SEC_NV_CRL_A_OFF + 16u, src, sizeof(src)));
    TEST_ASSERT_EQUAL_INT(0,
        hal_nv_flash_read(SEC_NV_CRL_A_OFF + 16u, out, sizeof(out)));
    TEST_ASSERT_EQUAL_MEMORY(src, out, sizeof(src));

    TEST_ASSERT_EQUAL_INT(0,
        hal_nv_flash_erase(SEC_NV_CRL_A_OFF, SEC_NV_CRL_BANK_SIZE));
    bytes = mcux_mock_flash_bytes();
    TEST_ASSERT_NOT_NULL(bytes);
    for (i = 0u; i < SEC_NV_CRL_BANK_SIZE; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFFu, bytes[SEC_NV_CRL_A_OFF + i]);
    }
}

static void test_mcux_alignment_and_cross_bank_rejection(void)
{
    static const uint8_t src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    bind_parent_flash();
    TEST_ASSERT_EQUAL_INT(-1, hal_nv_flash_write(2u, src, 4u));
    TEST_ASSERT_EQUAL_INT(-1,
        hal_nv_flash_erase(4u, SEC_NV_KEYSTORE_BANK_SIZE));
    TEST_ASSERT_EQUAL_INT(-1, hal_nv_flash_erase(0u, 0x1000u));
    TEST_ASSERT_EQUAL_INT(-1,
        kw47a_flash_ll_program(SEC_NV_FLASH_BASE + SEC_NV_KEYSTORE_B_OFF - 4u,
                               src, sizeof(src)));
    TEST_ASSERT_EQUAL_INT(-1,
        hal_nv_flash_write(SEC_NV_KEYSTORE_B_OFF - 4u, src, sizeof(src)));
    TEST_ASSERT_EQUAL_INT(-1,
        hal_nv_flash_erase(SEC_NV_KEYSTORE_A_OFF,
                           2u * SEC_NV_KEYSTORE_BANK_SIZE));
}

static void test_mcux_erase_program_and_partial_failures(void)
{
    static const uint8_t src[4] = { 0xA5, 0x5A, 0x12, 0x34 };
    uint8_t *bytes;

    bind_parent_flash();
    bytes = mcux_mock_flash_bytes();
    (void)memset(bytes, 0, SEC_NV_KEYSTORE_BANK_SIZE);
    mcux_mock_flash_set_erase_status(kStatus_FLASH_EraseFailure);
    TEST_ASSERT_EQUAL_INT(-1,
        hal_nv_flash_erase(SEC_NV_KEYSTORE_A_OFF, SEC_NV_KEYSTORE_BANK_SIZE));
    TEST_ASSERT_EQUAL_HEX8(0x00u, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, bytes[SEC_NV_KEYSTORE_BANK_SIZE - 1u]);

    mcux_mock_flash_set_erase_status(kStatus_Success);
    TEST_ASSERT_EQUAL_INT(0,
        hal_nv_flash_erase(SEC_NV_KEYSTORE_A_OFF, SEC_NV_KEYSTORE_BANK_SIZE));
    mcux_mock_flash_set_program_status(kStatus_FLASH_ProgramFailure);
    TEST_ASSERT_EQUAL_INT(-1, hal_nv_flash_write(0u, src, sizeof(src)));
    TEST_ASSERT_EQUAL_HEX8(0xFFu, bytes[0]);

    mcux_mock_flash_set_program_status(kStatus_Success);
    mcux_mock_flash_set_program_partial(2u);
    TEST_ASSERT_EQUAL_INT(-1, hal_nv_flash_write(4u, src, sizeof(src)));
    TEST_ASSERT_EQUAL_HEX8(src[0], bytes[4]);
    TEST_ASSERT_EQUAL_HEX8(src[1], bytes[5]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, bytes[6]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, bytes[7]);
}

/* Strong definitions override kw47a_otp_ll.c's weak SDK defaults. */
#define SDK_OTP_WORDS 4u
static uint32_t g_sdk_otp[SDK_OTP_WORDS];
static int g_sdk_otp_fail;
static uint32_t g_sdk_otp_program_calls;

uint32_t kw47a_sdk_otp_read(uint32_t idx)
{
    return (idx < SDK_OTP_WORDS) ? g_sdk_otp[idx] : 0u;
}

int kw47a_sdk_otp_program(uint32_t idx, uint32_t value)
{
    uint32_t old;

    g_sdk_otp_program_calls++;
    if (g_sdk_otp_fail || idx >= SDK_OTP_WORDS) {
        return -1;
    }
    old = g_sdk_otp[idx];
    if ((old & ~value) != 0u) {
        return -1;
    }
    g_sdk_otp[idx] = value;
    return 0;
}

static void sdk_otp_reset(void)
{
    (void)memset(g_sdk_otp, 0, sizeof(g_sdk_otp));
    g_sdk_otp_fail = 0;
    g_sdk_otp_program_calls = 0u;
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_bind_default());
}

static void test_default_otp_lock_and_crl_epoch(void)
{
    uint32_t calls;

    sdk_otp_reset();
    TEST_ASSERT_FALSE(kw47a_otp_lock_read());
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_lock_set());
    TEST_ASSERT_TRUE(kw47a_otp_lock_read());
    TEST_ASSERT_EQUAL_UINT(1u, g_sdk_otp[KW47A_OTP_WORD_LOCK]);
    calls = g_sdk_otp_program_calls;
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_lock_set());
    TEST_ASSERT_EQUAL_UINT(calls, g_sdk_otp_program_calls);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_debug_lock_set());
    TEST_ASSERT_TRUE(kw47a_otp_debug_lock_read());
    TEST_ASSERT_EQUAL_UINT(3u, g_sdk_otp[KW47A_OTP_WORD_LOCK]);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(5u));
    TEST_ASSERT_EQUAL_UINT(5u, kw47a_otp_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0x1Fu, g_sdk_otp[KW47A_OTP_WORD_EPOCH]);
    calls = g_sdk_otp_program_calls;
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(3u));
    TEST_ASSERT_EQUAL_UINT(calls, g_sdk_otp_program_calls);
}

static void test_default_otp_keystore_crosses_word_boundary(void)
{
    uint32_t calls;

    sdk_otp_reset();
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(31u));
    TEST_ASSERT_EQUAL_UINT(31u, kw47a_otp_keystore_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0x7FFFFFFFu,
                            g_sdk_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH]);
    TEST_ASSERT_EQUAL_UINT(0u,
                            g_sdk_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH + 1u]);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(32u));
    TEST_ASSERT_EQUAL_UINT(32u, kw47a_otp_keystore_epoch_read());
    TEST_ASSERT_EQUAL_UINT(1u,
                            g_sdk_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH + 1u]);
    calls = g_sdk_otp_program_calls;
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(32u));
    TEST_ASSERT_EQUAL_UINT(calls, g_sdk_otp_program_calls);
}

static void test_default_otp_program_failure_preserves_old_bits(void)
{
    sdk_otp_reset();
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_lock_set());
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(2u));
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(2u));

    g_sdk_otp_fail = 1;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_debug_lock_set());
    TEST_ASSERT_EQUAL_UINT(1u, g_sdk_otp[KW47A_OTP_WORD_LOCK]);
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_ll_epoch_raise(8u));
    TEST_ASSERT_EQUAL_UINT(3u, g_sdk_otp[KW47A_OTP_WORD_EPOCH]);
    TEST_ASSERT_EQUAL_UINT(2u, kw47a_otp_epoch_read());
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_keystore_epoch_raise(4u));
    TEST_ASSERT_EQUAL_UINT(3u, g_sdk_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH]);
    TEST_ASSERT_EQUAL_UINT(2u, kw47a_otp_keystore_epoch_read());
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flash_init_maps_geometry);
    RUN_TEST(test_mcux_geometry_binding_gates);
    RUN_TEST(test_mcux_hal_nv_full_chain);
    RUN_TEST(test_mcux_alignment_and_cross_bank_rejection);
    RUN_TEST(test_mcux_erase_program_and_partial_failures);
    RUN_TEST(test_default_otp_lock_and_crl_epoch);
    RUN_TEST(test_default_otp_keystore_crosses_word_boundary);
    RUN_TEST(test_default_otp_program_failure_preserves_old_bits);
    UNITY_END();
}
