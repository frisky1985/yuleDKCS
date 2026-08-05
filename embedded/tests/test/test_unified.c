/**
 * test_unified.c — Digital Key Unified Protocol Interface Unit Tests
 *
 * Maps to integration test cases from test_suite/*.md:
 *   - Cross-protocol integration scenarios
 *   - Unified API dispatch across CCC/ICCOA/ICCE
 *   - Device type routing
 *
 * Tests the dk_unified.h API layer, including:
 *   - Device configuration and initialization
 *   - Protocol routing by device type
 *   - Key management (cross-protocol)
 *   - Zone/location management
 *   - Vehicle control commands
 *   - Callback registration
 */

#include "unity.h"
#include "dk_unified.h"

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* ========================================================================
 *  Unified Init — 设备初始化配置
 * ======================================================================== */
void test_unified_init_smartphone(void)
{
    /* 构建智能手机设备配置 */
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));

    device.device_type  = DK_DEVICE_SMARTPHONE;
    device.protocol     = DK_PROTOCOL_ICCOA;
    device.protocol_version = DK_VERSION_ICCOA_40;
    device.capabilities.capabilities = DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE;
    device.capabilities.max_keys = 8;
    device.capabilities.max_sessions = 4;
    device.capabilities.ble_mtu = 247;

    /* 初始化 */
    dk_status_t ret = dk_init(&device);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 获取状态 */
    dk_device_status_t status;
    (void)memset(&status, 0, sizeof(status));
    ret = dk_get_status(&status);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 验证设备类型 */
    TEST_ASSERT_EQUAL(DK_DEVICE_SMARTPHONE, status.device.device_type);
    TEST_ASSERT_EQUAL(DK_PROTOCOL_ICCOA, status.device.protocol);

    ret = dk_deinit();
    TEST_ASSERT_EQUAL(DK_OK, ret);
}

/* ========================================================================
 *  Unified Init — 车端 CE 模块 (CCC protocol)
 * ======================================================================== */
void test_unified_init_vehicle_ce(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));

    device.device_type  = DK_DEVICE_VEHICLE_CE;
    device.protocol     = DK_PROTOCOL_CCC;
    device.protocol_version = DK_VERSION_CCC_30;
    device.capabilities.capabilities = DK_CAP_BLE | DK_CAP_UWB | DK_CAP_NFC | DK_CAP_SE;

    dk_status_t ret = dk_init(&device);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    dk_device_status_t status;
    ret = dk_get_status(&status);
    TEST_ASSERT_EQUAL(DK_OK, ret);
    TEST_ASSERT_EQUAL(DK_DEVICE_VEHICLE_CE, status.device.device_type);

    dk_deinit();
}

/* ========================================================================
 *  Unified Init — ICCE 边缘计算设备
 * ======================================================================== */
void test_unified_init_icce(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));

    device.device_type  = DK_DEVICE_VEHICLE_TCU;
    device.protocol     = DK_PROTOCOL_ICCE;
    device.protocol_version = DK_VERSION_ICCE_10;
    device.capabilities.capabilities = DK_CAP_BLE | DK_CAP_UWB | DK_CAP_EDGE_COMPUTE;

    dk_status_t ret = dk_init(&device);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    dk_device_status_t status;
    ret = dk_get_status(&status);
    TEST_ASSERT_EQUAL(DK_OK, ret);
    TEST_ASSERT_EQUAL(DK_DEVICE_VEHICLE_TCU, status.device.device_type);

    dk_deinit();
}

/* ========================================================================
 *  Unified — 连接回调注册
 * ======================================================================== */
void test_unified_callback_registration(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_SMARTPHONE;
    device.protocol = DK_PROTOCOL_ICCOA;

    dk_init(&device);

    /* 注册所有回调 */
    dk_status_t ret;

    ret = dk_register_conn_cb(NULL, NULL);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    ret = dk_register_auth_cb(NULL, NULL);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    ret = dk_register_location_cb(NULL, NULL);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    ret = dk_register_zone_cb(NULL, NULL);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    ret = dk_register_vehicle_cb(NULL, NULL);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    dk_deinit();
}

/* ========================================================================
 *  Unified — 蓝牙广播控制
 * ======================================================================== */
void test_unified_ble_adv(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_SMARTPHONE;
    device.protocol = DK_PROTOCOL_ICCOA;
    device.capabilities.capabilities = DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE;

    dk_init(&device);

    dk_status_t ret = dk_ble_start_adv(DK_PROTOCOL_ICCOA);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    ret = dk_ble_stop_adv();
    TEST_ASSERT_EQUAL(DK_OK, ret);

    dk_deinit();
}

/* ========================================================================
 *  Unified — NFC 监听控制
 * ======================================================================== */
void test_unified_nfc_listen(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_VEHICLE_CE;
    device.protocol = DK_PROTOCOL_CCC;
    device.capabilities.capabilities = DK_CAP_NFC;

    dk_init(&device);
    TEST_ASSERT_EQUAL(DK_OK, dk_nfc_start_listen());
    TEST_ASSERT_EQUAL(DK_OK, dk_nfc_stop_listen());
    dk_deinit();
}

/* ========================================================================
 *  Unified — 钥匙管理
 * ======================================================================== */
void test_unified_key_lifecycle(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_VEHICLE_CE;
    device.protocol = DK_PROTOCOL_CCC;
    device.capabilities.capabilities = DK_CAP_NFC | DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE | DK_CAP_KEY_SHARE;

    dk_init(&device);

    /* 创建钥匙 (16-byte key_id — KEY_ID_LEN is 16 and slots match via memcmp) */
    dk_key_t key;
    (void)memset(&key, 0, sizeof(key));
    uint8_t key_id[16];
    (void)memset(key_id, 0, sizeof(key_id));
    (void)memcpy(key_id, "unified_key_01", 14);
    (void)memcpy(key.key_id, key_id, 16);
    (void)memcpy(key.vehicle_id, "VH_00001", 8);
    key.key_type = DK_KEY_OWNER;
    key.state = DK_KEY_STATE_ACTIVE;
    key.access_rights[0] = DK_ACCESS_LOCK | DK_ACCESS_UNLOCK | DK_ACCESS_ENGINE_START;

    dk_status_t ret = dk_key_create(&key);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 获取钥匙 */
    dk_key_t retrieved;
    (void)memset(&retrieved, 0, sizeof(retrieved));
    ret = dk_key_get(key_id, &retrieved);
    TEST_ASSERT_EQUAL(DK_OK, ret);
    TEST_ASSERT_EQUAL(DK_KEY_OWNER, retrieved.key_type);

    /* 列出钥匙 */
    dk_key_t keys[8];
    uint8_t count = 8;
    ret = dk_key_list(keys, &count);
    TEST_ASSERT_EQUAL(DK_OK, ret);
    TEST_ASSERT_TRUE(count >= 1);

    /* 分享钥匙 */
    ret = dk_key_share(key_id, DK_KEY_TEMPORARY, 7200);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 暂停 + 恢复 */
    ret = dk_key_suspend(key_id);
    TEST_ASSERT_EQUAL(DK_OK, ret);
    ret = dk_key_resume(key_id);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 撤销 */
    ret = dk_key_revoke(key_id);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 删除 */
    ret = dk_key_delete(key_id);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    dk_deinit();
}

/* ========================================================================
 *  Unified — 车辆控制
 * ======================================================================== */
void test_unified_vehicle_control(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_SMARTPHONE;
    device.protocol = DK_PROTOCOL_ICCOA;
    device.capabilities.capabilities = DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE;

    dk_init(&device);

    /* 建立认证状态: bind + provision key with rights + verify */
    TEST_ASSERT_EQUAL(DK_OK, dk_auth_bind(DK_PROTOCOL_ICCOA));
    {
        dk_key_t auth_key;
        (void)memset(&auth_key, 0, sizeof(auth_key));
        (void)memcpy(auth_key.key_id, "veh_ctrl_key", 12);
        auth_key.key_type = DK_KEY_OWNER;
        auth_key.state = DK_KEY_STATE_ACTIVE;
        auth_key.access_rights[0] = DK_ACCESS_LOCK | DK_ACCESS_UNLOCK | DK_ACCESS_ENGINE_START | DK_ACCESS_TRUNK;
        auth_key.access_rights[1] = (uint8_t)(DK_ACCESS_FIND >> 8);
        TEST_ASSERT_EQUAL(DK_OK, dk_key_create(&auth_key));
    }
    TEST_ASSERT_EQUAL(DK_OK, dk_auth_verify());

    /* 解锁 / 锁定 */
    TEST_ASSERT_EQUAL(DK_OK, dk_vehicle_ctrl(DK_CTRL_UNLOCK, 0));
    TEST_ASSERT_EQUAL(DK_OK, dk_vehicle_ctrl(DK_CTRL_LOCK, 0));
    TEST_ASSERT_EQUAL(DK_OK, dk_vehicle_ctrl(DK_CTRL_ENGINE_START, 0));
    TEST_ASSERT_EQUAL(DK_OK, dk_vehicle_ctrl(DK_CTRL_ENGINE_STOP, 0));
    TEST_ASSERT_EQUAL(DK_OK, dk_vehicle_ctrl(DK_CTRL_TRUNK_OPEN, 0));
    TEST_ASSERT_EQUAL(DK_OK, dk_vehicle_ctrl(DK_CTRL_FIND, 0));

    /* 获取车辆状态 */
    dk_vehicle_status_t vs;
    (void)memset(&vs, 0, sizeof(vs));
    TEST_ASSERT_EQUAL(DK_OK, dk_vehicle_get_status(&vs));

    dk_deinit();
}

/* ========================================================================
 *  Unified — 定位管理
 * ======================================================================== */
void test_unified_location(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_VEHICLE_TCU;
    device.protocol = DK_PROTOCOL_ICCE;
    device.capabilities.capabilities = DK_CAP_BLE | DK_CAP_UWB | DK_CAP_EDGE_COMPUTE;

    dk_init(&device);

    /* Start UWB ranging so dk_location_get has an active session */
    uint32_t uwb_sid = 0;
    TEST_ASSERT_EQUAL(DK_OK, dk_uwb_start_ranging(&uwb_sid));
    TEST_ASSERT_TRUE(uwb_sid != 0);

    /* 设置区域阈值 */
    TEST_ASSERT_EQUAL(DK_OK, dk_zone_set_threshold(1000, 500, 200, 50));

    /* 获取位置 */
    dk_location_t loc;
    (void)memset(&loc, 0, sizeof(loc));
    TEST_ASSERT_EQUAL(DK_OK, dk_location_get(&loc));
    TEST_ASSERT_TRUE(loc.zone >= DK_ZONE_LOCKED && loc.zone <= DK_ZONE_UNKNOWN);

    dk_deinit();
}

/* ========================================================================
 *  Unified — 协议扩展 (raw send)
 * ======================================================================== */
void test_unified_protocol_raw(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_SMARTPHONE;
    device.protocol = DK_PROTOCOL_ICCOA;
    device.capabilities.capabilities = DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE;

    dk_init(&device);

    /* Simulate an established BLE link so iccoa_ble_send() is allowed
     * (state must be CONNECTED/READY, not IDLE). */
    extern void hal_ble_on_connect(uint16_t conn_handle, const uint8_t *peer_addr);
    {
        uint8_t peer[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
        hal_ble_on_connect(1, peer);
    }

    uint8_t raw_data[] = { 0xAA, 0x10, 0x00, 0x01, 0x00, 0x03, 0x01, 0x02, 0x03, 0x55 };
    dk_status_t ret = dk_protocol_send_raw(DK_PROTOCOL_ICCOA, raw_data, sizeof(raw_data));
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 获取协议信息 */
    uint16_t version;
    const char *name;
    ret = dk_protocol_get_info(DK_PROTOCOL_CCC, &version, &name);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    ret = dk_protocol_get_info(DK_PROTOCOL_ICCOA, &version, &name);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    ret = dk_protocol_get_info(DK_PROTOCOL_ICCE, &version, &name);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    dk_deinit();
}

/* ========================================================================
 *  Unified — 认证流程
 * ======================================================================== */
void test_unified_auth_flow(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_SMARTPHONE;
    device.protocol = DK_PROTOCOL_ICCOA;

    dk_init(&device);

    /* 绑定 */
    dk_status_t ret = dk_auth_bind(DK_PROTOCOL_ICCOA);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    /* 创建钥匙 (with unlock rights) */
    dk_key_t key;
    (void)memset(&key, 0, sizeof(key));
    uint8_t auth_key_id[16];
    (void)memset(auth_key_id, 0, sizeof(auth_key_id));
    (void)memcpy(auth_key_id, "auth_test_key", 13);
    (void)memcpy(key.key_id, auth_key_id, 16);
    key.key_type = DK_KEY_OWNER;
    key.state = DK_KEY_STATE_ACTIVE;
    key.access_rights[0] = DK_ACCESS_LOCK | DK_ACCESS_UNLOCK;
    TEST_ASSERT_EQUAL(DK_OK, dk_key_create(&key));

    /* 完成认证 */
    TEST_ASSERT_EQUAL(DK_OK, dk_auth_verify());

    /* 解锁 */
    bool has_perm = dk_auth_check_permission(DK_ACCESS_UNLOCK);
    TEST_ASSERT_TRUE(has_perm);

    /* 解绑 */
    ret = dk_auth_unbind(auth_key_id);
    TEST_ASSERT_EQUAL(DK_OK, ret);

    dk_deinit();
}

/* ========================================================================
 *  Unified — 主循环 tick
 * ======================================================================== */
void test_unified_run_tick(void)
{
    dk_device_type_t device;
    (void)memset(&device, 0, sizeof(device));
    device.device_type = DK_DEVICE_SMARTPHONE;
    device.protocol = DK_PROTOCOL_ICCOA;

    dk_init(&device);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(DK_OK, dk_run());
    }

    dk_deinit();
}

/* ========================================================================
 *  Test Runner
 * ======================================================================== */
int run_unified_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_unified_init_smartphone);
    RUN_TEST(test_unified_init_vehicle_ce);
    RUN_TEST(test_unified_init_icce);
    RUN_TEST(test_unified_callback_registration);
    RUN_TEST(test_unified_ble_adv);
    RUN_TEST(test_unified_nfc_listen);
    RUN_TEST(test_unified_key_lifecycle);
    RUN_TEST(test_unified_vehicle_control);
    RUN_TEST(test_unified_location);
    RUN_TEST(test_unified_protocol_raw);
    RUN_TEST(test_unified_auth_flow);
    RUN_TEST(test_unified_run_tick);

    UNITY_END();
}

#ifndef TEST_UNIFIED_NO_MAIN
int main(void) { return run_unified_tests(); }
#endif /* TEST_UNIFIED_NO_MAIN */
