/**
 * test_ccc_auth_info.c — core 认证状态查询面测试
 *
 * 验证:
 *   1. 查询接口参数校验与初值 (未认证)
 *   2. 认证失败后 authenticated 必须为 false (fail-closed)
 *   3. 钥匙权限绑定: 0 把 / 1 把 / 多把 的确定语义
 */

#include "unity.h"
#include "ccc_digital_key.h"

#include <stdio.h>
#include <string.h>

static ccc_digital_key_t g_all_keys[MAX_KEYS];
static const uint8_t    g_zero_rights[4] = { 0, 0, 0, 0 };

static void purge_all_keys(void)
{
    uint8_t n = MAX_KEYS;
    if (key_list(g_all_keys, &n) != CCC_OK) return;
    for (uint8_t i = 0; i < n; i++) {
        (void)key_delete(g_all_keys[i].key_id);
    }
}

static void make_key(ccc_digital_key_t *k, uint8_t seed, uint8_t rights)
{
    (void)memset(k, 0, sizeof(*k));
    (void)memset(k->key_id, seed, KEY_ID_LEN);
    k->key_type = 0x01;
    k->access_rights[0] = rights;
    k->state = KEY_STATE_ACTIVE;
}

/* ─── 查询接口 ─── */

void test_get_auth_info_rejects_null(void) {
    TEST_ASSERT_EQUAL_INT(CCC_ERR_INVALID_PARAM, ccc_dk_get_auth_info(NULL));
}

void test_auth_info_initial_state_unauthenticated(void) {
    ccc_auth_info_t info;

    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_dk_init());
    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_dk_get_auth_info(&info));

    TEST_ASSERT_FALSE(info.authenticated);
    TEST_ASSERT_FALSE(info.key_bound);
    TEST_ASSERT_EQUAL_MEMORY(g_zero_rights, info.access_rights, 4);
    TEST_ASSERT_EQUAL_UINT(0u, info.session_id);

    (void)ccc_dk_deinit();
}

void test_auth_failure_keeps_unauthenticated(void) {
    ccc_auth_info_t info;
    /* 空证书链 + 假签名: 必须认证失败 */
    static const uint8_t bogus_sig[8] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22 };

    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_dk_init());
    ccc_dk_set_auth_data(NULL, 0, NULL, 0, NULL,
                         (const uint8_t *)"challenge", 9,
                         bogus_sig, sizeof(bogus_sig));
    (void)ccc_dk_run();   /* AUTH_PROCESS 一 tick 内失败并闭锁 */

    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_dk_get_auth_info(&info));
    TEST_ASSERT_FALSE(info.authenticated);
    TEST_ASSERT_FALSE(info.key_bound);
    TEST_ASSERT_EQUAL_MEMORY(g_zero_rights, info.access_rights, 4);
    TEST_ASSERT_EQUAL_UINT(0u, info.session_id);

    (void)ccc_dk_deinit();
}

/* ─── 钥匙权限绑定 ─── */

void test_sole_key_summary_none(void) {
    key_summary_t ks;
    (void)key_mgmt_init();
    purge_all_keys();
    TEST_ASSERT_EQUAL_INT(CCC_ERR_NOT_FOUND, key_get_sole_active_summary(&ks));
}

void test_sole_key_summary_binds_single_key(void) {
    key_summary_t ks;
    ccc_digital_key_t k;

    (void)key_mgmt_init();
    purge_all_keys();

    make_key(&k, 0x11, ACCESS_LOCK_UNLOCK | ACCESS_TRUNK);
    TEST_ASSERT_EQUAL_INT(CCC_OK, key_create(&k));

    TEST_ASSERT_EQUAL_INT(CCC_OK, key_get_sole_active_summary(&ks));
    TEST_ASSERT_EQUAL_MEMORY(k.key_id, ks.key_id, KEY_ID_LEN);
    TEST_ASSERT_EQUAL_HEX8(ACCESS_LOCK_UNLOCK | ACCESS_TRUNK, ks.access_rights[0]);
    TEST_ASSERT_EQUAL_UINT8(KEY_STATE_ACTIVE, ks.state);

    (void)key_delete(k.key_id);
}

/** 多把 ACTIVE 钥匙时不得猜测, 必须拒绝绑定 */
void test_sole_key_summary_ambiguous_multi_key(void) {
    key_summary_t ks;
    ccc_digital_key_t k1, k2;

    (void)key_mgmt_init();
    purge_all_keys();

    make_key(&k1, 0x21, ACCESS_LOCK_UNLOCK);
    make_key(&k2, 0x22, ACCESS_LOCK_UNLOCK | ACCESS_ENGINE_START);
    TEST_ASSERT_EQUAL_INT(CCC_OK, key_create(&k1));
    TEST_ASSERT_EQUAL_INT(CCC_OK, key_create(&k2));

    TEST_ASSERT_EQUAL_INT(CCC_ERR_STATE, key_get_sole_active_summary(&ks));

    (void)key_delete(k1.key_id);
    (void)key_delete(k2.key_id);
}

void test_sole_key_summary_ignores_suspended(void) {
    key_summary_t ks;
    ccc_digital_key_t k1, k2;

    (void)key_mgmt_init();
    purge_all_keys();

    make_key(&k1, 0x31, ACCESS_LOCK_UNLOCK);
    make_key(&k2, 0x32, ACCESS_LOCK_UNLOCK);
    TEST_ASSERT_EQUAL_INT(CCC_OK, key_create(&k1));
    TEST_ASSERT_EQUAL_INT(CCC_OK, key_create(&k2));
    TEST_ASSERT_EQUAL_INT(CCC_OK, key_suspend(k2.key_id));

    /* 挂起的不算 ACTIVE, 因此唯一 ACTIVE 是 k1 */
    TEST_ASSERT_EQUAL_INT(CCC_OK, key_get_sole_active_summary(&ks));
    TEST_ASSERT_EQUAL_MEMORY(k1.key_id, ks.key_id, KEY_ID_LEN);

    (void)key_delete(k1.key_id);
    (void)key_delete(k2.key_id);
}

int run_ccc_auth_info_tests(void) {
    printf("\n--- CCC Auth Info / Key Binding ---\n");

    (void)key_mgmt_init();
    purge_all_keys();

    UNITY_BEGIN();

    RUN_TEST(test_get_auth_info_rejects_null);
    RUN_TEST(test_auth_info_initial_state_unauthenticated);
    RUN_TEST(test_auth_failure_keeps_unauthenticated);

    RUN_TEST(test_sole_key_summary_none);
    RUN_TEST(test_sole_key_summary_binds_single_key);
    RUN_TEST(test_sole_key_summary_ambiguous_multi_key);
    RUN_TEST(test_sole_key_summary_ignores_suspended);

    UNITY_END();
}
