/**
 * @file test_kw47a_nv.c
 * @brief KW47A NV 绑定骨架测试（hal_nv_flash_* 用 RAM 假 Flash 替代）
 */

#include "unity.h"
#include "kw47a_nv.h"
#include "sec_nv.h"
#include "ccc_digital_key.h"

#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif

static uint8_t g_flash[KW47A_NV_SIZE];
static int     g_read_rc;
static int     g_write_rc;
static int     g_erase_rc;

/* ── 假 HAL：仅实现 NV 分区所需接口 ─────────────────────── */
int hal_nv_flash_read(uint32_t off, void *data, uint32_t len)
{
    if (g_read_rc != 0) {
        return g_read_rc;
    }
    if (data == NULL || off > KW47A_NV_SIZE || len > KW47A_NV_SIZE - off) {
        return -1;
    }
    (void)memcpy(data, &g_flash[off], len);
    return 0;
}

int hal_nv_flash_write(uint32_t off, const void *data, uint32_t len)
{
    if (g_write_rc != 0) {
        return g_write_rc;
    }
    if (data == NULL || off > KW47A_NV_SIZE || len > KW47A_NV_SIZE - off) {
        return -1;
    }
    (void)memcpy(&g_flash[off], data, len);
    return 0;
}

int hal_nv_flash_erase(uint32_t off, uint32_t len)
{
    if (g_erase_rc != 0) {
        return g_erase_rc;
    }
    if (off > KW47A_NV_SIZE || len > KW47A_NV_SIZE - off) {
        return -1;
    }
    (void)memset(&g_flash[off], 0xFF, len);
    return 0;
}

/* ── 辅助 ───────────────────────────────────────────────── */
static void env_reset(void)
{
    (void)memset(g_flash, 0xFF, sizeof(g_flash));
    g_read_rc = 0;
    g_write_rc = 0;
    g_erase_rc = 0;
    sec_nv_reset_for_test();
    sec_test_reset_security();
}

static bool bank_has_magic(uint32_t off)
{
    return memcmp(&g_flash[off], "YRNV", 4) == 0;
}

/* ── 用例 ───────────────────────────────────────────────── */
static void test_init_fails_when_hal_unavailable(void)
{
    bool locked = true;

    env_reset();
    g_read_rc = -1;

    TEST_ASSERT_EQUAL_INT(-1, kw47a_nv_init());
    TEST_ASSERT_FALSE(kw47a_nv_bound());
    TEST_ASSERT_EQUAL_INT(CCC_ERR_NOT_INIT, sec_nv_load_lock(&locked));
    TEST_ASSERT_EQUAL_INT(CCC_ERR_NOT_INIT, sec_nv_save_lock(NULL));
}

static void test_init_binds_and_reports_layout(void)
{
    env_reset();

    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());
    TEST_ASSERT_TRUE(kw47a_nv_bound());
    TEST_ASSERT_EQUAL_UINT(SEC_NV_FLASH_BASE, kw47a_nv_base());
    TEST_ASSERT_EQUAL_UINT(SEC_NV_FLASH_SIZE, KW47A_NV_SIZE);
}

static void test_lock_roundtrip_survives_reinit(void)
{
    bool locked = false;

    env_reset();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());

    TEST_ASSERT_EQUAL_INT(CCC_ERR_NOT_FOUND, sec_nv_load_lock(&locked));
    TEST_ASSERT_EQUAL_INT(CCC_OK, sec_nv_save_lock(NULL));
    TEST_ASSERT_TRUE(bank_has_magic(SEC_NV_LOCK_A_OFF) ||
                     bank_has_magic(SEC_NV_LOCK_B_OFF));

    TEST_ASSERT_EQUAL_INT(CCC_OK, sec_nv_load_lock(&locked));
    TEST_ASSERT_TRUE(locked);

    /* 模拟复位：清 RAM 状态，保留 Flash 内容，重新绑定 */
    sec_nv_reset_for_test();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());

    locked = false;
    TEST_ASSERT_EQUAL_INT(CCC_OK, sec_nv_load_lock(&locked));
    TEST_ASSERT_TRUE(locked);
}

static void test_crl_roundtrip_and_partition_isolation(void)
{
    uint8_t blob[72];
    uint8_t out[SEC_NV_MAX_CRL];
    uint16_t out_len = 0;
    uint16_t i;

    env_reset();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());

    (void)memset(blob, 0x5A, sizeof(blob));
    blob[8]  = 0x07; /* epoch = 7 (LE) */
    blob[9]  = 0x00;
    blob[10] = 0x00;
    blob[11] = 0x00;

    TEST_ASSERT_EQUAL_INT(CCC_OK,
                          sec_nv_save_crl(blob, (uint16_t)sizeof(blob)));
    TEST_ASSERT_TRUE(bank_has_magic(SEC_NV_CRL_A_OFF) ||
                     bank_has_magic(SEC_NV_CRL_B_OFF));

    TEST_ASSERT_EQUAL_INT(CCC_OK,
                          sec_nv_load_crl(out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT16(sizeof(blob), out_len);
    TEST_ASSERT_EQUAL_MEMORY(blob, out, sizeof(blob));

    /* Lock 区与 keystore 区不得被 CRL 写入污染 */
    for (i = 0; i < SEC_NV_LOCK_BANK_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0xFF, g_flash[SEC_NV_LOCK_A_OFF + i]);
    }

    /* 模拟复位后仍可读出 */
    sec_nv_reset_for_test();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());
    out_len = 0;
    TEST_ASSERT_EQUAL_INT(CCC_OK,
                          sec_nv_load_crl(out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT16(sizeof(blob), out_len);
    TEST_ASSERT_EQUAL_MEMORY(blob, out, sizeof(blob));
}

static void test_erase_failure_keeps_previous_lock(void)
{
    bool locked = false;

    env_reset();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());
    TEST_ASSERT_EQUAL_INT(CCC_OK, sec_nv_save_lock(NULL));

    /* 第二次保存需要擦除另一 Bank：注入失败 */
    g_erase_rc = -1;
    TEST_ASSERT_NOT_EQUAL(CCC_OK, sec_nv_save_lock(NULL));
    g_erase_rc = 0;

    sec_nv_reset_for_test();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());
    TEST_ASSERT_EQUAL_INT(CCC_OK, sec_nv_load_lock(&locked));
    TEST_ASSERT_TRUE(locked);
}

static void test_write_failure_keeps_previous_record(void)
{
    uint8_t blob[20];
    uint8_t out[SEC_NV_MAX_CRL];
    uint16_t out_len = 0;

    env_reset();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());

    (void)memset(blob, 0x11, sizeof(blob));
    blob[8] = 0x01; /* epoch = 1 */
    TEST_ASSERT_EQUAL_INT(CCC_OK,
                          sec_nv_save_crl(blob, (uint16_t)sizeof(blob)));

    g_write_rc = -1;
    blob[0] = 0x22;
    TEST_ASSERT_NOT_EQUAL(CCC_OK,
                          sec_nv_save_crl(blob, (uint16_t)sizeof(blob)));
    g_write_rc = 0;

    sec_nv_reset_for_test();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());
    TEST_ASSERT_EQUAL_INT(CCC_OK,
                          sec_nv_load_crl(out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_UINT16(sizeof(blob), out_len);
    TEST_ASSERT_EQUAL_UINT8(0x11, out[0]);
}

static void test_read_failure_never_reports_unlocked(void)
{
    bool locked = true;

    env_reset();
    TEST_ASSERT_EQUAL_INT(0, kw47a_nv_init());
    TEST_ASSERT_EQUAL_INT(CCC_OK, sec_nv_save_lock(NULL));

    g_read_rc = -1;
    locked = false;
    TEST_ASSERT_NOT_EQUAL(CCC_OK, sec_nv_load_lock(&locked));
    /* fail-closed：读失败绝不能返回“未锁定” */
    TEST_ASSERT_TRUE(locked);
}

int run_kw47a_nv_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_fails_when_hal_unavailable);
    RUN_TEST(test_init_binds_and_reports_layout);
    RUN_TEST(test_lock_roundtrip_survives_reinit);
    RUN_TEST(test_crl_roundtrip_and_partition_isolation);
    RUN_TEST(test_erase_failure_keeps_previous_lock);
    RUN_TEST(test_write_failure_keeps_previous_record);
    RUN_TEST(test_read_failure_never_reports_unlocked);
    UNITY_END();
}

#ifndef TEST_LIB_MODE
int main(void)
{
    return run_kw47a_nv_tests();
}
#endif
