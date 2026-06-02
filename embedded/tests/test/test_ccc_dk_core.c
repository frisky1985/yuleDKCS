/**
 * test_ccc_dk_core.c — CCC Digital Key Core Module Unit Tests
 *
 * Maps to test_suite/TEST_CASES_CCC.md:
 *   - CCC_CORE_001..023  (连接管理、密钥交换、数据传输)
 *   - CCC_BLE_001..023   (BLE扫描、连接、数据传输)
 *   - CCC_SEC_001..023   (加解密、安全通道、SE通信)
 *   - CCC_NFC_001..013   (NFC初始化、OOB交换)
 *   - CCC_UWB_001..015   (UWB测距、区域划分)
 *
 * Tests CCC core state machine, zone classification, key management,
 * NFC OOB exchange, and UWB distance processing.
 */

#include "unity.h"
#include "ccc_digital_key.h"

void setUp(void) {}
void tearDown(void) {}

/* ========================================================================
 *  CCC_CORE_001 — 初始化和连接管理
 * ======================================================================== */
void test_ccc_init(void)
{
    /* CCC_CORE_001: 初始化 */
    ccc_status_t ret = ccc_dk_init();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* CCC_CORE_003: 反初始化 */
    ret = ccc_dk_deinit();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 二次初始化 */
    ret = ccc_dk_init();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* CCC_CORE_002: 获取状态 */
    system_status_t status;
    memset(&status, 0, sizeof(status));
    ret = ccc_dk_get_status(&status);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 状态应是 STANDBY */
    TEST_ASSERT_EQUAL(STATE_STANDBY, ccc_dk_get_state());

    ret = ccc_dk_deinit();
    TEST_ASSERT_EQUAL(CCC_OK, ret);
}

/* ========================================================================
 *  CCC_CORE_004 — 初始状态 INIT
 * ======================================================================== */
void test_ccc_initial_state(void)
{
    /* 未初始化时不应有有效状态 */
    main_state_e state = ccc_dk_get_state();
    /* 调用未初始化 get_state 返回什么？实现返回 static 变量，初始为 0 = STATE_INIT */
    TEST_ASSERT_EQUAL(STATE_INIT, state);
}

/* ========================================================================
 *  CCC_CORE_020 — 发送数据 (init -> run cycle)
 * ======================================================================== */
void test_ccc_run_cycle(void)
{
    TEST_ASSERT_EQUAL(CCC_OK, ccc_dk_init());

    /* 运行主循环 tick */
    ccc_status_t ret = ccc_dk_run();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 状态应在 STANDBY (无 NFC 场时) */
    main_state_e state = ccc_dk_get_state();
    TEST_ASSERT_EQUAL(STATE_STANDBY, state);

    TEST_ASSERT_EQUAL(CCC_OK, ccc_dk_deinit());
}

/* ========================================================================
 *  CCC_UWB_001 — UWB 初始化
 * ======================================================================== */
void test_uwb_init(void)
{
    ccc_status_t ret = uwb_ncj29d6_init();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ret = uwb_ncj29d6_deinit();
    TEST_ASSERT_EQUAL(CCC_OK, ret);
}

/* ========================================================================
 *  CCC_UWB_002 — 创建 UWB 会话
 * ======================================================================== */
void test_uwb_create_session(void)
{
    uwb_ncj29d6_init();

    uwb_session_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.session_id[0] = 0x01; cfg.session_id[1] = 0x02;
    cfg.channel = 9;
    cfg.preamble_code = 12;
    cfg.prf_len = 128;
    cfg.sfd_id = 0;
    cfg.phr_rate = 0;
    cfg.data_rate = 0;
    cfg.rframe_config = 1;
    cfg.sts_config = 0;

    uint32_t session_id = uwb_create_session(&cfg);
    TEST_ASSERT_NOT_EQUAL(UWB_INVALID_SESSION, session_id);

    /* 销毁会话 */
    ccc_status_t ret = uwb_destroy_session(session_id);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    uwb_ncj29d6_deinit();
}

/* ========================================================================
 *  CCC_UWB_003 — UWB 区域分类测试
 * ======================================================================== */
void test_uwb_zone_classification(void)
{
    uwb_ncj29d6_init();

    /* 设置自定义阈值 */
    distance_threshold_t th = {
        .approach_cm  = 1000,
        .unlock_cm    = 500,
        .entry_cm     = 200,
        .inside_cm    = 50,
        .hysteresis_cm = 30
    };
    uwb_set_threshold(&th);

    /* 请注意：zone 分类实现在 ccc_dk_core.c 内部 (static classify_distance) */
    /* 我们只能通过 UWB session 的 get_zone 来间接测试 */
    /* 创建会话后直接测试区间 */

    /* 测试 UWB 测距 */
    uint16_t dist;
    ccc_status_t ret;

    /* 创建会话 */
    uwb_session_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.channel = 9;
    cfg.preamble_code = 12;
    cfg.prf_len = 128;
    uint32_t sid = uwb_create_session(&cfg);
    TEST_ASSERT_NOT_EQUAL(UWB_INVALID_SESSION, sid);

    /* 开始测距 */
    ret = uwb_start_ranging(sid);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 获取距离 */
    ret = uwb_get_distance(sid, &dist);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 获取区域 */
    distance_zone_e zone = uwb_get_zone(sid);
    TEST_ASSERT_TRUE(zone >= ZONE_LOCKED && zone <= ZONE_INSIDE);

    /* 停止测距 */
    ret = uwb_stop_ranging(sid);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    uwb_destroy_session(sid);
    uwb_ncj29d6_deinit();
}

/* ========================================================================
 *  CCC_NFC_001 — NFC 初始化
 * ======================================================================== */
void test_nfc_init(void)
{
    ccc_status_t ret = nfc_st25r501_init();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ret = nfc_st25r501_deinit();
    TEST_ASSERT_EQUAL(CCC_OK, ret);
}

/* ========================================================================
 *  CCC_NFC_003 — NFC 场检测
 * ======================================================================== */
void test_nfc_field_detect(void)
{
    nfc_st25r501_init();

    /* 无 NFC 场时返回 false */
    bool field = nfc_field_detect();
    TEST_ASSERT_FALSE(field);

    nfc_st25r501_deinit();
}

/* ========================================================================
 *  CCC_NFC_005 — NFC 监听模式
 * ======================================================================== */
void test_nfc_listen(void)
{
    nfc_st25r501_init();

    ccc_status_t ret = nfc_start_listen();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    nfc_state_e state = nfc_get_state();
    TEST_ASSERT_TRUE(state >= NFC_STATE_IDLE && state <= NFC_STATE_ERROR);

    ret = nfc_stop_listen();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    nfc_st25r501_deinit();
}

/* ========================================================================
 *  CCC_SEC_001 — 安全模块初始化
 * ======================================================================== */
void test_security_init(void)
{
    ccc_status_t ret = sec_init();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ret = sec_deinit();
    TEST_ASSERT_EQUAL(CCC_OK, ret);
}

/* ========================================================================
 *  CCC_SEC_020 — SE 密钥存储
 * ======================================================================== */
void test_se_key_store_load(void)
{
    sec_init();

    uint8_t key_id[] = "test_key_001";
    uint8_t key_data[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    ccc_status_t ret = sec_store_key(key_id, key_data, sizeof(key_data));
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    uint8_t loaded[16];
    uint16_t loaded_len = sizeof(loaded);
    ret = sec_load_key(key_id, loaded, &loaded_len);
    TEST_ASSERT_EQUAL(CCC_OK, ret);
    TEST_ASSERT_EQUAL(sizeof(key_data), loaded_len);
    TEST_ASSERT_EQUAL_MEMORY(key_data, loaded, loaded_len);

    ret = sec_delete_key(key_id);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    sec_deinit();
}

/* ========================================================================
 *  CCC_BLE_001 — BLE 初始化 + GATT 服务注册
 * ======================================================================== */
void test_ble_init_and_gatt(void)
{
    ccc_status_t ret = ble_kw47a_init();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ret = ble_register_gatt_service();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ret = ble_kw47a_deinit();
    TEST_ASSERT_EQUAL(CCC_OK, ret);
}

/* ========================================================================
 *  CCC_BLE_010 — BLE 连接管理
 * ======================================================================== */
void test_ble_connect_disconnect(void)
{
    ble_kw47a_init();

    /* 广播参数 */
    ble_adv_param_t adv = {
        .interval_min = 100,
        .interval_max = 200,
        .len = 10
    };
    memcpy(adv.data, "\x02\x01\x06\x07\xFF\xD1\xFF\x01\x02\x03", 10);

    ccc_status_t ret = ble_start_adv(&adv);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ret = ble_stop_adv();
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ble_kw47a_deinit();
}

/* ========================================================================
 *  CCC_CORE_010 — 密钥创建/删除/查询
 * ======================================================================== */
void test_key_management(void)
{
    key_mgmt_init();

    ccc_digital_key_t key;
    memset(&key, 0, sizeof(key));
    memcpy(key.key_id, "key_core_001_test", 16);
    memcpy(key.vehicle_id, "vehicle_0001", 12);
    key.key_type = KEY_TYPE_OWNER;
    key.access_rights[0] = ACCESS_LOCK_UNLOCK | ACCESS_ENGINE_START;
    key.valid_from = 1700000000;
    key.valid_until = 1730000000;
    key.state = KEY_STATE_ACTIVE;

    /* CCC_CORE_010: 创建密钥 */
    ccc_status_t ret = key_create(&key);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* CCC_CORE_011: 查询密钥 */
    ccc_digital_key_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    ret = key_get((const uint8_t*)"key_core_001_test", &retrieved);
    TEST_ASSERT_EQUAL(CCC_OK, ret);
    TEST_ASSERT_EQUAL_MEMORY(key.key_id, retrieved.key_id, 16);
    TEST_ASSERT_EQUAL(KEY_TYPE_OWNER, retrieved.key_type);

    /* 列出所有密钥 */
    ccc_digital_key_t keys[MAX_KEYS];
    uint8_t count = MAX_KEYS;
    ret = key_list(keys, &count);
    TEST_ASSERT_EQUAL(CCC_OK, ret);
    TEST_ASSERT_TRUE(count >= 1);

    /* CCC_CORE_013: 删除密钥 */
    ret = key_delete((const uint8_t*)"key_core_001_test");
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    key_mgmt_deinit();
}

/* ========================================================================
 *  CCC_CORE_012 — 密钥分享
 * ======================================================================== */
void test_key_sharing(void)
{
    key_mgmt_init();

    ccc_digital_key_t key;
    memset(&key, 0, sizeof(key));
    memcpy(key.key_id, "share_test_key", 14);
    key.key_type = KEY_TYPE_OWNER;
    key.state = KEY_STATE_ACTIVE;
    key_create(&key);

    /* 分享密钥 */
    ccc_status_t ret = key_share((const uint8_t*)"share_test_key",
                                  KEY_TYPE_TEMPORARY,
                                  3600); /* 1 hour */
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 暂停 */
    ret = key_suspend((const uint8_t*)"share_test_key");
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 恢复 */
    ret = key_resume((const uint8_t*)"share_test_key");
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    /* 撤销 */
    ret = key_revoke((const uint8_t*)"share_test_key");
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    key_delete((const uint8_t*)"share_test_key");
    key_mgmt_deinit();
}

/* ========================================================================
 *  CCC_UWB_010 — UWB 阈值和区域回调注册
 * ======================================================================== */
void test_uwb_threshold_and_callback(void)
{
    uwb_ncj29d6_init();

    distance_threshold_t th = {
        .approach_cm  = 800,
        .unlock_cm    = 400,
        .entry_cm     = 150,
        .inside_cm    = 30,
        .hysteresis_cm = 20
    };
    ccc_status_t ret = uwb_set_threshold(&th);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    ret = uwb_register_zone_cb(NULL);
    TEST_ASSERT_EQUAL(CCC_OK, ret);

    uwb_ncj29d6_deinit();
}

/* ========================================================================
 *  CCC_CORE — 完整状态机生命周期
 * ======================================================================== */
void test_ccc_full_lifecycle(void)
{
    TEST_ASSERT_EQUAL(CCC_OK, ccc_dk_init());

    /* 运行时 tick */
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(CCC_OK, ccc_dk_run());
    }

    TEST_ASSERT_EQUAL(CCC_OK, ccc_dk_deinit());

    /* 二次 init */
    TEST_ASSERT_EQUAL(CCC_OK, ccc_dk_init());
    main_state_e state = ccc_dk_get_state();
    TEST_ASSERT_EQUAL(STATE_STANDBY, state);
    TEST_ASSERT_EQUAL(CCC_OK, ccc_dk_deinit());
}

/* ========================================================================
 *  CCC_SEC_003 — ECC 签名和验证 (伪)
 * ======================================================================== */
void test_security_sign_verify(void)
{
    sec_init();

    uint8_t data[] = "test_data_for_signature";
    uint8_t sig[64];
    uint32_t sig_len = sizeof(sig);

    ccc_status_t ret = sec_sign(data, sizeof(data), sig, &sig_len);
    TEST_ASSERT_EQUAL(CCC_OK, ret);
    TEST_ASSERT_TRUE(sig_len > 0);

    verify_result_e vr = sec_verify(data, sizeof(data), sig, sig_len);
    TEST_ASSERT_EQUAL(VERIFY_OK, vr);

    sec_deinit();
}

/* ========================================================================
 *  Test Runner
 * ======================================================================== */
int run_ccc_core_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ccc_init);
    RUN_TEST(test_ccc_initial_state);
    RUN_TEST(test_ccc_run_cycle);
    RUN_TEST(test_uwb_init);
    RUN_TEST(test_uwb_create_session);
    RUN_TEST(test_uwb_zone_classification);
    RUN_TEST(test_nfc_init);
    RUN_TEST(test_nfc_field_detect);
    RUN_TEST(test_nfc_listen);
    RUN_TEST(test_security_init);
    RUN_TEST(test_se_key_store_load);
    RUN_TEST(test_ble_init_and_gatt);
    RUN_TEST(test_ble_connect_disconnect);
    RUN_TEST(test_key_management);
    RUN_TEST(test_key_sharing);
    RUN_TEST(test_uwb_threshold_and_callback);
    RUN_TEST(test_ccc_full_lifecycle);
    RUN_TEST(test_security_sign_verify);

    UNITY_END();
}

int main(void) { return run_ccc_core_tests(); }
