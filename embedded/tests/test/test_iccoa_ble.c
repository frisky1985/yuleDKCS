/**
 * test_iccoa_ble.c — ICCOA BLE Module Unit Tests
 *
 * Tests BLE initialization, advertisement, data transmission, and lifecycle.
 * Uses only the PUBLIC API declared in iccoa_digital_key.h.
 */

#include "unity.h"
#include "iccoa_digital_key.h"

void setUp(void) {}
void tearDown(void) {}

/* Extern: BLE HAL event callbacks from iccoa_ble.c */
void hal_ble_on_connect(uint16_t conn_handle, const uint8_t *peer_addr);
void hal_ble_on_mtu_exchanged(uint16_t conn_handle, uint16_t mtu);
void hal_ble_on_encryption_complete(uint16_t conn_handle, uint8_t success);
void hal_ble_on_bonding_complete(uint16_t conn_handle, uint8_t success);

static void simulate_ble_connect(void)
{
    uint8_t peer_addr[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    hal_ble_on_connect(1, peer_addr);
    hal_ble_on_mtu_exchanged(1, 247);
    hal_ble_on_encryption_complete(1, 1);
    hal_ble_on_bonding_complete(1, 1);
}

/* ========================================================================
 *  ICCOA_BLE_001 — BLE init + start/stop adv
 * ======================================================================== */
void test_ble_init_adv(void)
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
 *  ICCOA_BLE_003 — dk_init 包含 BLE init
 * ======================================================================== */
void test_dk_init_ble(void)
{
    int32_t ret = iccoa_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);
    iccoa_dk_deinit();
}

/* ========================================================================
 *  ICCOA_BLE_010 — BLE send
 * ======================================================================== */
void test_ble_send(void)
{
    int32_t ret = iccoa_ble_init();
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    /* 先模拟 BLE 连接, 否则 send 返回 ERR_NOT_INIT */
    simulate_ble_connect();

    uint8_t data[] = { 0xAA, 0x10, 0x00, 0x01, 0x00, 0x05, 0x48, 0x65, 0x6C, 0x6C, 0x6F };
    ret = iccoa_ble_send(data, sizeof(data));
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    /* Large payload within MTU */
    uint8_t big[240];
    memset(big, 0xAB, sizeof(big));
    ret = iccoa_ble_send(big, sizeof(big));
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);

    iccoa_ble_deinit();
}

/* ========================================================================
 *  ICCOA_BLE_013 — Callback registration
 * ======================================================================== */
void test_ble_callback(void)
{
    iccoa_ble_init();
    int32_t ret = iccoa_ble_register_cb(NULL);
    TEST_ASSERT_EQUAL_INT32(ICCOA_OK, ret);
    iccoa_ble_deinit();
}

/* ========================================================================
 *  ICCOA_BLE — Full lifecycle
 * ======================================================================== */
void test_ble_lifecycle(void)
{
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT32(ICCOA_OK, iccoa_ble_init());
        TEST_ASSERT_EQUAL_INT32(ICCOA_OK, iccoa_ble_start_adv());
        TEST_ASSERT_EQUAL_INT32(ICCOA_OK, iccoa_ble_stop_adv());
        TEST_ASSERT_EQUAL_INT32(ICCOA_OK, iccoa_ble_deinit());
    }
}

/* ========================================================================
 *  Test Runner
 * ======================================================================== */
int run_iccoa_ble_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ble_init_adv);
    RUN_TEST(test_dk_init_ble);
    RUN_TEST(test_ble_send);
    RUN_TEST(test_ble_callback);
    RUN_TEST(test_ble_lifecycle);

    UNITY_END();
}

int main(void) { return run_iccoa_ble_tests(); }
