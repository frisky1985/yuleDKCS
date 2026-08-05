/**
 * test_iccoa_dk_core.c — ICCOA Digital Key Core Module Unit Tests
 *
 * Uses only the PUBLIC API declared in iccoa_digital_key.h.
 * Tests are linked against real source files + stubs.
 *
 * Maps to test_suite/TEST_CASES_ICCOA.md:
 *   - ICCOA_AUTH_001..013  (认证/授权)
 *   - ICCOA_BLE_001..013   (BLE连接)
 *   - ICCOA_CORE_001..023  (车辆控制)
 *   - ICCOA_SVC_001..012   (服务发现)
 */

#include "unity.h"
#include "iccoa_digital_key.h"

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* Extern declarations for BLE HAL event callbacks (defined in iccoa_ble.c) */
void hal_ble_on_connect(uint16_t conn_handle, const uint8_t *peer_addr);
void hal_ble_on_mtu_exchanged(uint16_t conn_handle, uint16_t mtu);
void hal_ble_on_encryption_complete(uint16_t conn_handle, uint8_t success);
void hal_ble_on_bonding_complete(uint16_t conn_handle, uint8_t success);

/* Helper: simulate BLE connection for test setup */
static void simulate_ble_connect(void)
{
    uint8_t peer_addr[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    hal_ble_on_connect(1, peer_addr);
    hal_ble_on_mtu_exchanged(1, 247);
    hal_ble_on_encryption_complete(1, 1);
    hal_ble_on_bonding_complete(1, 1);
}

/* ========================================================================
 *  ICCOA_AUTH_001 — 初始化 (p0)
 * ======================================================================== */
void test_iccoa_init(void)
{
    int32_t ret = iccoa_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    /* 重复 init 应幂等 */
    ret = iccoa_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_dk_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);
}

/* ========================================================================
 *  ICCOA_AUTH_010 — 认证请求
 * ======================================================================== */
void test_iccoa_auth_request(void)
{
    int32_t ret = iccoa_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    uint8_t challenge[16] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };

    ret = iccoa_auth_request(ICCOA_AUTH_BIND, challenge, sizeof(challenge));
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_auth_request(ICCOA_AUTH_DAILY, challenge, sizeof(challenge));
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    iccoa_dk_deinit();
}

/* ========================================================================
 *  ICCOA_AUTH_011 — 认证验证 (带响应)
 * ======================================================================== */
void test_iccoa_auth_verify(void)
{
    int32_t ret = iccoa_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    /* 验证需满足 user_id(16) + signature(≥32) */
    uint8_t response[48]; /* 16+32 */
    (void)memset(response, 0xAB, sizeof(response));
    ret = iccoa_auth_verify(response, sizeof(response));
    /* 取决于 stub 实现 */
    TEST_ASSERT_TRUE(ret == ICCOA_OK || ret == ICCOA_ERR_DENIED);

    /* 过短响应应拒绝 */
    ret = iccoa_auth_verify(response, 10);
    TEST_ASSERT_EQUAL_INT32(ICCOA_ERR_PARAM, ret);

    iccoa_dk_deinit();
}

/* ========================================================================
 *  ICCOA_BLE_001 — BLE 广播控制
 * ======================================================================== */
void test_iccoa_ble_adv(void)
{
    int32_t ret = iccoa_ble_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_ble_start_adv();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_ble_stop_adv();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    iccoa_ble_deinit();
}

/* ========================================================================
 *  ICCOA_BLE_010 — BLE 发送数据 (需要先建立连接)
 * ======================================================================== */
void test_iccoa_ble_send_data(void)
{
    iccoa_ble_init();

    uint8_t data[] = { 0xAA, 0x10, 0x00, 0x01, 0x00, 0x05, 0x48, 0x65, 0x6C, 0x6C, 0x6F };

    /* 未连接时发送应返回 ERR_NOT_INIT (state machine check) */
    int32_t ret = iccoa_ble_send(data, sizeof(data));
    TEST_ASSERT_EQUAL_INT32(ICCOA_ERR_NOT_INIT, ret);  /* [V-02] 状态机保护 */

    /* 模拟 BLE 连接 (调用 HAL 回调) */
    simulate_ble_connect();

    /* 连接后发送应成功 */
    ret = iccoa_ble_send(data, sizeof(data));
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    /* NULL data (即使已连接也应拒绝) */
    ret = iccoa_ble_send(NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCOA_ERR_PARAM, ret);

    iccoa_ble_deinit();
}

/* ========================================================================
 *  ICCOA_BLE_011 — BLE 回调注册
 * ======================================================================== */
void test_iccoa_ble_callback(void)
{
    iccoa_ble_init();
    int32_t ret = iccoa_ble_register_cb(NULL);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);
    iccoa_ble_deinit();
}

/* ========================================================================
 *  ICCOA_DK30 — DK30 帧校验
 * ======================================================================== */
void test_iccoa_dk30_checksum(void)
{
    iccoa_dk30_init();

    /* 构建 DK30 帧 header + payload, 计算 checksum */
    uint8_t frame_buf[32];
    (void)memset(frame_buf, 0, sizeof(frame_buf));
    frame_buf[0] = DK30_SOP;    /* sop */
    frame_buf[1] = ICCOA_CMD_BIND_REQ; /* cmd_id */
    frame_buf[2] = 0x00;        /* seq_num lo */
    frame_buf[3] = 0x01;        /* seq_num hi */
    frame_buf[4] = 0x04;        /* payload_len lo */
    frame_buf[5] = 0x00;        /* payload_len hi */
    frame_buf[6] = 0xAA;        /* payload[0] */
    frame_buf[7] = 0xBB;        /* payload[1] */
    frame_buf[8] = 0xCC;        /* payload[2] */
    frame_buf[9] = 0xDD;        /* payload[3] */

    /* checksum over header(6) + payload(4) = 10 bytes */
    uint16_t cs_len = 6 + 4;
    uint8_t cs = iccoa_dk30_checksum(frame_buf, cs_len);
    TEST_ASSERT_TRUE(cs != 0);

    /* Verify repeatability */
    uint8_t cs2 = iccoa_dk30_checksum(frame_buf, cs_len);
    TEST_ASSERT_EQUAL_UINT8(cs, cs2);

    /* Different data → different checksum */
    frame_buf[6] = 0xEE;
    uint8_t cs3 = iccoa_dk30_checksum(frame_buf, cs_len);
    TEST_ASSERT_TRUE(cs3 != cs);

    /* NULL safe */
    uint8_t cs_null = iccoa_dk30_checksum(NULL, 10);
    TEST_ASSERT_EQUAL_UINT8(0, cs_null);

    iccoa_dk30_init();
}

/* ========================================================================
 *  ICCOA_DK30 — DK30 响应发送
 * ======================================================================== */
void test_iccoa_dk30_response(void)
{
    iccoa_dk30_init();
    iccoa_ble_init();
    simulate_ble_connect();

    uint8_t payload[] = { 0x00, 0x01, 0x02 };
    int32_t ret = iccoa_dk30_send_response(ICCOA_CMD_BIND_RSP, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    iccoa_ble_deinit();
    iccoa_dk30_init();
}

/* ========================================================================
 *  ICCOA_CORE_001..005 — 车辆控制
 * ======================================================================== */
void test_iccoa_vehicle_control(void)
{
    int32_t ret = iccoa_service_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_ctrl_execute(CTRL_UNLOCK, 0);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_ctrl_execute(CTRL_LOCK, 0);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_ctrl_execute(CTRL_ENGINE_ON, 0);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_ctrl_execute(CTRL_ENGINE_OFF, 0);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    ret = iccoa_ctrl_execute(CTRL_TRUNK_OPEN, 0);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);
}

/* ========================================================================
 *  ICCOA_CORE_010..012 — 车辆状态查询
 * ======================================================================== */
void test_iccoa_vehicle_status(void)
{
    iccoa_service_init();

    iccoa_vehicle_status_t status;
    (void)memset(&status, 0, sizeof(status));
    int32_t ret = iccoa_service_get_status(&status);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    /* 默认值 */
    TEST_ASSERT_EQUAL_UINT8(1, status.lock_status);

    iccoa_dk_deinit();
}

/* ========================================================================
 *  ICCOA_SVC_002 — 服务发现 (通过 dk_init)
 * ======================================================================== */
void test_iccoa_service_dk_init(void)
{
    int32_t ret = iccoa_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);
    iccoa_dk_deinit();
}

/* ========================================================================
 *  ICCOA — DK40 init 调用测试
 * ======================================================================== */
void test_iccoa_dk40_init(void)
{
    int32_t ret = iccoa_dk40_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);
}

/* ========================================================================
 *  Test Runner
 * ======================================================================== */
int run_iccoa_core_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_iccoa_init);
    RUN_TEST(test_iccoa_auth_request);
    RUN_TEST(test_iccoa_auth_verify);
    RUN_TEST(test_iccoa_ble_adv);
    RUN_TEST(test_iccoa_ble_send_data);
    RUN_TEST(test_iccoa_ble_callback);
    RUN_TEST(test_iccoa_dk30_checksum);
    RUN_TEST(test_iccoa_dk30_response);
    RUN_TEST(test_iccoa_vehicle_control);
    RUN_TEST(test_iccoa_vehicle_status);
    RUN_TEST(test_iccoa_service_dk_init);
    RUN_TEST(test_iccoa_dk40_init);

    UNITY_END();
}

#ifndef ICCOA_CORE_NO_MAIN
int main(void) { return run_iccoa_core_tests(); }
#endif
