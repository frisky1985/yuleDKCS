/**
 * test_ccc_oem_app.c — OEM App (Message Type 4) 分发器行为测试
 *
 * 认证会话通过 ccc_auth_info_t 注入, 无需真实密码学即可覆盖全部负向路径.
 */

#include "unity.h"
#include "ccc_oem_app.h"

#include <stdio.h>
#include <string.h>

static uint8_t rq[256];
static uint8_t rs[256];
static uint16_t rq_len;
static ccc_auth_info_t auth;

static int exec_calls;
static uint8_t exec_action;
static ccc_status_t exec_ret;

static ccc_status_t fake_exec(uint8_t action, const uint8_t *params, uint16_t len)
{
    (void)params; (void)len;
    exec_calls++;
    exec_action = action;
    return exec_ret;
}

static void reset_all(void)
{
    (void)memset(rq, 0, sizeof(rq));
    (void)memset(rs, 0, sizeof(rs));
    (void)memset(&auth, 0, sizeof(auth));
    rq_len = 0;
    exec_calls = 0;
    exec_action = 0;
    exec_ret = CCC_OK;
    ccc_oem_app_reset_session();
    ccc_oem_app_register_exec(NULL);
}

static void make_authenticated(void)
{
    auth.authenticated = true;
    auth.key_bound = true;
    auth.session_id = 0x11223344u;
    auth.access_rights[0] = ACCESS_LOCK_UNLOCK | ACCESS_TRUNK | ACCESS_ENGINE_START;
}

static void build(uint8_t cmd, uint32_t sid, uint16_t seq,
                  const uint8_t *body, uint16_t blen)
{
    rq[0] = CCC_OEM_APP_VERSION;
    rq[1] = cmd;
    rq[2] = 0;
    rq[3] = 0;
    rq[4] = (uint8_t)(sid >> 24); rq[5] = (uint8_t)(sid >> 16);
    rq[6] = (uint8_t)(sid >> 8);  rq[7] = (uint8_t)sid;
    rq[8] = (uint8_t)(seq >> 8);  rq[9] = (uint8_t)seq;
    rq[10] = (uint8_t)(blen >> 8); rq[11] = (uint8_t)blen;
    if (blen > 0 && body != NULL) (void)memcpy(rq + 12, body, blen);
    rq_len = (uint16_t)(CCC_OEM_APP_HDR_LEN + blen);
}

static uint16_t call(void)
{
    return ccc_oem_app_handle(&auth, rq, rq_len, rs, (uint16_t)sizeof(rs));
}

static uint8_t result(void) { return rs[CCC_OEM_OFF_FLAGS_RESULT]; }
static uint16_t rs_blen(void) { return (uint16_t)((rs[10] << 8) | rs[11]); }
static uint32_t rs_sid(void)
{
    return ((uint32_t)rs[4] << 24) | ((uint32_t)rs[5] << 16)
         | ((uint32_t)rs[6] << 8) | (uint32_t)rs[7];
}

/* ─── CAPABILITY ─── */

void test_capability_ok(void) {
    reset_all();
    build(CCC_OEM_CMD_CAPABILITY, 0, 0, NULL, 0);
    TEST_ASSERT_EQUAL_UINT16(CCC_OEM_APP_HDR_LEN + CCC_OEM_CAPABILITY_BODY_LEN, call());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_OK, result());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_APP_VERSION, rs[12]);
    TEST_ASSERT_EQUAL_HEX16(CCC_OEM_APP_MAX_BODY, (rs[13] << 8) | rs[14]);
    TEST_ASSERT_EQUAL_HEX8(0x00, rs[15]);                       /* bitmap 高字节 */
    TEST_ASSERT_EQUAL_HEX8(0x07, rs[18]);                       /* 三条命令 */
    TEST_ASSERT_EQUAL_HEX16(CCC_OEM_CAPABILITY_BODY_LEN, rs_blen());
}

void test_capability_rejects_nonzero_session_fields(void) {
    reset_all();
    build(CCC_OEM_CMD_CAPABILITY, 0x1234, 0, NULL, 0);
    TEST_ASSERT_EQUAL_UINT16(CCC_OEM_APP_HDR_LEN, call());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_MALFORMED, result());

    build(CCC_OEM_CMD_CAPABILITY, 0, 7, NULL, 0);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_MALFORMED, result());
}

/* ─── 通用格式校验 ─── */

void test_bad_version_rejected(void) {
    reset_all();
    build(CCC_OEM_CMD_CAPABILITY, 0, 0, NULL, 0);
    rq[0] = 0x02;
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_UNSUPPORTED_VERSION, result());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_APP_VERSION, rs[0]);   /* 响应始终用本端版本 */
}

void test_nonzero_flags_and_reserved_rejected(void) {
    reset_all();
    build(CCC_OEM_CMD_CAPABILITY, 0, 0, NULL, 0);
    rq[2] = 0x01;
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_MALFORMED, result());

    build(CCC_OEM_CMD_CAPABILITY, 0, 0, NULL, 0);
    rq[3] = 0x01;
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_MALFORMED, result());
}

void test_short_frame_rejected(void) {
    reset_all();
    build(CCC_OEM_CMD_CAPABILITY, 0, 0, NULL, 0);
    rq_len = 5;
    TEST_ASSERT_EQUAL_UINT16(CCC_OEM_APP_HDR_LEN, call());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_MALFORMED, result());
}

void test_body_len_mismatch_rejected(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, body, 1);
    rq[11] = 2;   /* 声明 2 字节, 实际 1 字节 */
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_MALFORMED, result());
}

void test_payload_too_large_rejected(void) {
    reset_all();
    build(CCC_OEM_CMD_CAPABILITY, 0, 0, NULL, 0);
    rq_len = CCC_OEM_APP_HDR_LEN + CCC_OEM_APP_MAX_BODY + 1;
    rq[10] = (uint8_t)((CCC_OEM_APP_MAX_BODY + 1) >> 8);
    rq[11] = (uint8_t)(CCC_OEM_APP_MAX_BODY + 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_PAYLOAD_TOO_LARGE, result());
}

void test_unknown_command_rejected(void) {
    reset_all();
    build(0x7F, 0, 0, NULL, 0);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_UNKNOWN_COMMAND, result());
}

/* ─── STATUS ─── */

void test_status_requires_auth(void) {
    reset_all();
    build(CCC_OEM_CMD_STATUS, 0, 0, NULL, 0);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_AUTH_REQUIRED, result());
}

void test_status_returns_session_params(void) {
    reset_all();
    make_authenticated();
    build(CCC_OEM_CMD_STATUS, 0, 0, NULL, 0);
    TEST_ASSERT_EQUAL_UINT16(CCC_OEM_APP_HDR_LEN + CCC_OEM_STATUS_BODY_LEN, call());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_OK, result());
    TEST_ASSERT_EQUAL_UINT(0x11223344u,
        ((uint32_t)rs[12] << 24) | ((uint32_t)rs[13] << 16)
        | ((uint32_t)rs[14] << 8) | rs[15]);
    TEST_ASSERT_EQUAL_HEX16(1, (rs[16] << 8) | rs[17]);   /* next_seq 从 1 开始 */
}

/* ─── VEHICLE_CONTROL: 会话与重放 ─── */

void test_vehicle_requires_auth(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    build(CCC_OEM_CMD_VEHICLE_CONTROL, 0x11223344u, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_AUTH_REQUIRED, result());
    TEST_ASSERT_EQUAL_INT(0, exec_calls);
}

void test_vehicle_rejects_wrong_session(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    ccc_oem_app_register_exec(fake_exec);
    build(CCC_OEM_CMD_VEHICLE_CONTROL, 0xDEADBEEFu, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_SESSION, result());
    TEST_ASSERT_EQUAL_INT(0, exec_calls);
}

void test_vehicle_rejects_zero_session_even_when_authenticated(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    auth.session_id = 0;          /* 随机源失效: 会话不可绑定 */
    build(CCC_OEM_CMD_VEHICLE_CONTROL, 0, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_SESSION, result());
}

void test_vehicle_replay_and_out_of_order_rejected(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    ccc_oem_app_register_exec(fake_exec);

    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 5, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_OK, result());
    TEST_ASSERT_EQUAL_INT(1, exec_calls);

    call();                        /* 同序号重放 */
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_REPLAY, result());

    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 3, body, 1);
    call();                        /* 序号回退 */
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_REPLAY, result());

    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 0, body, 1);
    call();                        /* 0 非法 */
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_REPLAY, result());
    TEST_ASSERT_EQUAL_INT(1, exec_calls);
}

void test_new_session_reopens_sequence_window(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    ccc_oem_app_register_exec(fake_exec);

    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 9, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_OK, result());

    auth.session_id = 0x55667788u;   /* 重新认证, 新会话 */
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_OK, result());
    TEST_ASSERT_EQUAL_INT(2, exec_calls);
}

/* ─── VEHICLE_CONTROL: 权限与执行 ─── */

void test_vehicle_denied_when_key_unbound(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    auth.key_bound = false;       /* 多钥匙歧义 */
    ccc_oem_app_register_exec(fake_exec);
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_PERMISSION_DENIED, result());
    TEST_ASSERT_EQUAL_INT(0, exec_calls);
}

void test_vehicle_denied_without_required_right(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_ENGINE_START };
    reset_all();
    make_authenticated();
    auth.access_rights[0] = ACCESS_LOCK_UNLOCK;   /* 无启动权限 */
    ccc_oem_app_register_exec(fake_exec);
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_PERMISSION_DENIED, result());
    TEST_ASSERT_EQUAL_INT(0, exec_calls);
}

void test_vehicle_unsupported_without_exec(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_UNSUPPORTED_CMD, result());
}

void test_vehicle_success_echoes_session_and_seq(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_TRUNK };
    reset_all();
    make_authenticated();
    ccc_oem_app_register_exec(fake_exec);
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 2, body, 1);
    TEST_ASSERT_EQUAL_UINT16(CCC_OEM_APP_HDR_LEN, call());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_OK, result());
    TEST_ASSERT_EQUAL_UINT(auth.session_id, rs_sid());
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_CMD_VEHICLE_CONTROL, rs[1]);
    TEST_ASSERT_EQUAL_HEX8(2, rs[9]);
    TEST_ASSERT_EQUAL_INT(1, exec_calls);
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ACT_TRUNK, exec_action);
}

void test_vehicle_exec_failure_reported_and_not_retryable(void) {
    static const uint8_t body[1] = { CCC_OEM_ACT_UNLOCK };
    reset_all();
    make_authenticated();
    exec_ret = CCC_ERR_HARDWARE;
    ccc_oem_app_register_exec(fake_exec);

    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_EXEC_FAILED, result());

    call();   /* 同序号不允许借"执行失败"重试 */
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_REPLAY, result());
    TEST_ASSERT_EQUAL_INT(1, exec_calls);
}

void test_vehicle_unknown_action_rejected(void) {
    static const uint8_t body[1] = { 0x55 };
    reset_all();
    make_authenticated();
    ccc_oem_app_register_exec(fake_exec);
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, body, 1);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_UNKNOWN_COMMAND, result());
    TEST_ASSERT_EQUAL_INT(0, exec_calls);
}

void test_vehicle_bad_body_length_rejected(void) {
    reset_all();
    make_authenticated();
    ccc_oem_app_register_exec(fake_exec);
    build(CCC_OEM_CMD_VEHICLE_CONTROL, auth.session_id, 1, NULL, 0);
    call();
    TEST_ASSERT_EQUAL_HEX8(CCC_OEM_ERR_MALFORMED, result());
    TEST_ASSERT_EQUAL_INT(0, exec_calls);
}

int run_ccc_oem_app_tests(void) {
    printf("\n--- CCC OEM App Dispatcher ---\n");
    UNITY_BEGIN();

    RUN_TEST(test_capability_ok);
    RUN_TEST(test_capability_rejects_nonzero_session_fields);
    RUN_TEST(test_bad_version_rejected);
    RUN_TEST(test_nonzero_flags_and_reserved_rejected);
    RUN_TEST(test_short_frame_rejected);
    RUN_TEST(test_body_len_mismatch_rejected);
    RUN_TEST(test_payload_too_large_rejected);
    RUN_TEST(test_unknown_command_rejected);

    RUN_TEST(test_status_requires_auth);
    RUN_TEST(test_status_returns_session_params);

    RUN_TEST(test_vehicle_requires_auth);
    RUN_TEST(test_vehicle_rejects_wrong_session);
    RUN_TEST(test_vehicle_rejects_zero_session_even_when_authenticated);
    RUN_TEST(test_vehicle_replay_and_out_of_order_rejected);
    RUN_TEST(test_new_session_reopens_sequence_window);
    RUN_TEST(test_vehicle_denied_when_key_unbound);
    RUN_TEST(test_vehicle_denied_without_required_right);
    RUN_TEST(test_vehicle_unsupported_without_exec);
    RUN_TEST(test_vehicle_success_echoes_session_and_seq);
    RUN_TEST(test_vehicle_exec_failure_reported_and_not_retryable);
    RUN_TEST(test_vehicle_unknown_action_rejected);
    RUN_TEST(test_vehicle_bad_body_length_rejected);

    UNITY_END();
}
