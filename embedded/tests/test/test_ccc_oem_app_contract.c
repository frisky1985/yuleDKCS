/**
 * test_ccc_oem_app_contract.c — OEM App (Message Type 4) 契约常量固化
 *
 * 只锁定契约本身, 不断言行为 (实现尚未落地).
 * 任何对布局/命令字/结果码的意外改动都会在这里编译失败或测试失败.
 */

#include "unity.h"
#include "ccc_oem_app.h"

#include <stdio.h>

/* ─── 编译期: 封装与布局 ─── */

_Static_assert(CCC_OEM_MSG_TYPE == 4, "OEM App 必须绑定 CCC Message Type 4");
_Static_assert(CCC_OEM_MSG_TYPE == CCC_MSG_TYPE_VEHICLE_OEM_APP,
               "OEM App msg_type 必须引用统一枚举");

_Static_assert(CCC_OEM_OFF_VERSION      == 0,  "version offset");
_Static_assert(CCC_OEM_OFF_COMMAND      == 1,  "command offset");
_Static_assert(CCC_OEM_OFF_FLAGS_RESULT == 2,  "flags/result offset");
_Static_assert(CCC_OEM_OFF_RESERVED     == 3,  "reserved offset");
_Static_assert(CCC_OEM_OFF_SESSION_ID   == 4,  "session_id offset");
_Static_assert(CCC_OEM_OFF_SEQUENCE     == 8,  "sequence offset");
_Static_assert(CCC_OEM_OFF_BODY_LEN     == 10, "body_len offset");
_Static_assert(CCC_OEM_OFF_BODY         == 12, "body offset");
_Static_assert(CCC_OEM_APP_HDR_LEN      == 12, "header length");

_Static_assert(CCC_OEM_APP_HDR_LEN + CCC_OEM_APP_MAX_BODY == 235,
               "OEM 载荷必须与其它通道共享 235B 单帧上限");
_Static_assert(CCC_OEM_APP_MAX_BODY == 223, "max body");

/* ─── 编译期: 命令字与结果码 ─── */

_Static_assert(CCC_OEM_CMD_CAPABILITY      == 0x01, "capability cmd");
_Static_assert(CCC_OEM_CMD_STATUS          == 0x02, "status cmd");
_Static_assert(CCC_OEM_CMD_VEHICLE_CONTROL == 0x03, "vehicle control cmd");

_Static_assert(CCC_OEM_CAP_CAPABILITY      == (1u << 0), "cap bit");
_Static_assert(CCC_OEM_CAP_STATUS          == (1u << 1), "cap bit");
_Static_assert(CCC_OEM_CAP_VEHICLE_CONTROL == (1u << 2), "cap bit");

_Static_assert(CCC_OEM_ACT_LOCK         == 0x01, "action lock");
_Static_assert(CCC_OEM_ACT_UNLOCK       == 0x02, "action unlock");
_Static_assert(CCC_OEM_ACT_TRUNK        == 0x03, "action trunk");
_Static_assert(CCC_OEM_ACT_ENGINE_START == 0x04, "action engine start");

_Static_assert(CCC_OEM_OK == 0x00, "success must be zero");
_Static_assert(CCC_OEM_ERR_AUTH_REQUIRED     != CCC_OEM_ERR_PERMISSION_DENIED,
               "认证缺失与权限不足必须可区分");
_Static_assert(CCC_OEM_ERR_SESSION           != CCC_OEM_ERR_REPLAY,
               "会话不符与重放必须可区分");
_Static_assert(CCC_OEM_ERR_UNSUPPORTED_CMD   != CCC_OEM_ERR_UNKNOWN_COMMAND,
               "未注册执行器不得等同于未知命令");

_Static_assert(CCC_OEM_CAPABILITY_BODY_LEN == 8, "capability body");
_Static_assert(CCC_OEM_STATUS_BODY_LEN     == 8, "status body");
_Static_assert(CCC_OEM_SESSION_NONE == 0, "无会话标识必须为 0");

/* ─── 运行期: 跨模块一致性 ─── */

void test_oem_does_not_reuse_se_apdu_namespace(void) {
    /* Type 4 载荷不走 APDU: message_id 不得与 DK_APDU_RQ/RS 相同 */
    TEST_ASSERT_NOT_EQUAL(CCC_MSG_ID_DK_APDU_RQ, CCC_OEM_MSG_ID_RQ);
    TEST_ASSERT_NOT_EQUAL(CCC_MSG_ID_DK_APDU_RS, CCC_OEM_MSG_ID_RS);
}

void test_oem_action_rights_mapping_is_defined(void) {
    /* 每个动作都必须能映射到已定义的 ACCESS_* 位, 否则无法做权限判定 */
    TEST_ASSERT_EQUAL_HEX8(1u << 0, ACCESS_LOCK_UNLOCK);
    TEST_ASSERT_EQUAL_HEX8(1u << 1, ACCESS_ENGINE_START);
    TEST_ASSERT_EQUAL_HEX8(1u << 2, ACCESS_TRUNK);
}

int run_ccc_oem_app_contract_tests(void) {
    printf("\n--- CCC OEM App (Type 4) Contract ---\n");
    UNITY_BEGIN();
    RUN_TEST(test_oem_does_not_reuse_se_apdu_namespace);
    RUN_TEST(test_oem_action_rights_mapping_is_defined);
    UNITY_END();
}
