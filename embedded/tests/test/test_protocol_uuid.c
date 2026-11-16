/**
 * test_protocol_uuid.c — 跨协议 BLE Service UUID 单一来源一致性测试
 *
 * 背景: 三端曾各自硬编码且相互漂移 —
 *   CCC  车端 0xFFD1 (R3.0) vs 手机 0xFFF5 (v4.0)
 *   CCC  车端曾误用 ICCOA 的 0xFEF5 作为 GATT 注册值
 *   ICCE 车端 0xFFE0 范围 vs 手机 0xFEFA (规范)
 * 任何回退都会让两端"发现不到彼此", 且现场很难定位. 本测试把
 * hardware_abstraction.h 作为唯一来源并锁定取值.
 */

#include "unity.h"
#include "ccc_digital_key.h"        /* 间接引入 hardware_abstraction.h */
#include "iccoa_digital_key.h"
/* 真实 ICCE GATT 头 (不走 tests/support/icce 桩) */
#include "../../icce_protocol/src/ble/ble_gatt.h"

#include <stdio.h>

/* ─── 编译期断言: 单一来源取值被钉死 ─── */
_Static_assert(CCC_SERVICE_UUID == 0xFFF5,
               "CCC v4.0 Service UUID 必须是 0xFFF5 (CCC-TS-101 Table 19-6)");
_Static_assert(CCC_SERVICE_UUID_R30 == 0xFFD1,
               "CCC R3.0 兼容 UUID 必须是 0xFFD1");
_Static_assert(ICCOA_SERVICE_UUID == 0xFEF5,
               "ICCOA Service UUID 必须是 0xFEF5");
_Static_assert(ICCE_SERVICE_UUID == 0xFEFA,
               "ICCE Service UUID 必须是 0xFEFA (T/CA 110-2020)");
_Static_assert(CCC_BLE_SERVICE_UUID == CCC_SERVICE_UUID,
               "CCC_BLE_SERVICE_UUID 必须别名到单一来源 CCC_SERVICE_UUID");
_Static_assert(GATT_UUID_DIGITAL_KEY_SERVICE == ICCE_SERVICE_UUID,
               "ICCE GATT service 必须引用单一来源 ICCE_SERVICE_UUID");

/* ─── 运行期测试 ─── */

void test_ccc_service_uuid_is_v40(void) {
    TEST_ASSERT_EQUAL_HEX16(0xFFF5, CCC_SERVICE_UUID);
    TEST_ASSERT_EQUAL_HEX16(CCC_SERVICE_UUID, CCC_BLE_SERVICE_UUID);
}

/** 负向: R3.0 旧值不得作为主用 UUID */
void test_ccc_legacy_uuid_not_primary(void) {
    TEST_ASSERT_NOT_EQUAL(CCC_SERVICE_UUID, CCC_SERVICE_UUID_R30);
    TEST_ASSERT_EQUAL_HEX16(0xFFD1, CCC_SERVICE_UUID_R30);
}

void test_iccoa_service_uuid(void) {
    TEST_ASSERT_EQUAL_HEX16(0xFEF5, ICCOA_SERVICE_UUID);
}

void test_icce_service_uuid(void) {
    TEST_ASSERT_EQUAL_HEX16(0xFEFA, ICCE_SERVICE_UUID);
    TEST_ASSERT_EQUAL_HEX16(ICCE_SERVICE_UUID, GATT_UUID_DIGITAL_KEY_SERVICE);
}

/** 负向: ICCE 不得回退到旧 0xFFE0 范围 */
void test_icce_legacy_range_not_used(void) {
    TEST_ASSERT_TRUE(ICCE_SERVICE_UUID < 0xFFE0 || ICCE_SERVICE_UUID > 0xFFE5);
    TEST_ASSERT_NOT_EQUAL(0xFFE0, ICCE_SERVICE_UUID);
}

/** 负向: CCC 不得再被写成 ICCOA 的 UUID (历史事故) */
void test_ccc_not_equal_iccoa(void) {
    TEST_ASSERT_NOT_EQUAL(ICCOA_SERVICE_UUID, CCC_SERVICE_UUID);
    TEST_ASSERT_NOT_EQUAL(ICCOA_SERVICE_UUID, CCC_SERVICE_UUID_R30);
}

/** 三种协议 UUID 必须互不相同, 否则广播无法区分协议 */
void test_all_protocol_uuids_distinct(void) {
    TEST_ASSERT_NOT_EQUAL(CCC_SERVICE_UUID, ICCOA_SERVICE_UUID);
    TEST_ASSERT_NOT_EQUAL(CCC_SERVICE_UUID, ICCE_SERVICE_UUID);
    TEST_ASSERT_NOT_EQUAL(ICCOA_SERVICE_UUID, ICCE_SERVICE_UUID);
}

/** ICCE 特征 UUID 必须与 Service 同段 (0xFEFA..0xFEFF) */
void test_icce_characteristic_uuids_in_range(void) {
    TEST_ASSERT_TRUE(GATT_UUID_KEY_STATUS      > ICCE_SERVICE_UUID);
    TEST_ASSERT_TRUE(GATT_UUID_RANGING_DATA    > ICCE_SERVICE_UUID);
    TEST_ASSERT_TRUE(GATT_UUID_AUTH_CHALLENGE  > ICCE_SERVICE_UUID);
    TEST_ASSERT_TRUE(GATT_UUID_CONTROL_COMMAND > ICCE_SERVICE_UUID);
    TEST_ASSERT_TRUE(GATT_UUID_SESSION_KEY     > ICCE_SERVICE_UUID);
    TEST_ASSERT_EQUAL_HEX16(0xFEFF, GATT_UUID_SESSION_KEY);
}

int run_protocol_uuid_tests(void) {
    printf("\n--- Cross-Protocol Service UUID Consistency ---\n");

    UNITY_BEGIN();

    RUN_TEST(test_ccc_service_uuid_is_v40);
    RUN_TEST(test_ccc_legacy_uuid_not_primary);
    RUN_TEST(test_iccoa_service_uuid);
    RUN_TEST(test_icce_service_uuid);
    RUN_TEST(test_icce_legacy_range_not_used);
    RUN_TEST(test_ccc_not_equal_iccoa);
    RUN_TEST(test_all_protocol_uuids_distinct);
    RUN_TEST(test_icce_characteristic_uuids_in_range);

    UNITY_END();
}
