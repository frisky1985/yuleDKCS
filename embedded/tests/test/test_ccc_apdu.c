/**
 * test_ccc_apdu.c — CCC v4.0 APDU 分发器测试
 *
 * 覆盖: ISO 7816-4 结构解析、AID 选择、挑战下发、认证前置条件、
 *       未知命令拒绝、以及全部 fail-closed 路径.
 * 固定字节向量与 embedded/tests/protocol_vectors/ccc_frame_v4.h 中
 * SELECT APDU 示例同源.
 */

#include "unity.h"
#include "ccc_apdu.h"
#include "crypto_random.h"

#include <stdio.h>
#include <string.h>

/* 规范 SELECT APDU: 00 A4 04 00 0D <AID> 00 */
static const uint8_t SELECT_APDU[] = {
    0x00, 0xA4, 0x04, 0x00, 0x0D,
    0xA0, 0x00, 0x00, 0x08, 0x09, 0x43, 0x43, 0x43, 0x44, 0x4B, 0x41, 0x76, 0x31,
    0x00
};

/* GET CHALLENGE: 00 84 00 00 08 */
static const uint8_t GET_CHALLENGE[] = { 0x00, 0x84, 0x00, 0x00, 0x08 };

static uint8_t resp[300];
static uint16_t resp_len;

static void reset_session(void)
{
    ccc_apdu_reset();
    resp_len = 0;
    (void)memset(resp, 0, sizeof(resp));
}

static uint16_t send(const uint8_t *req, uint16_t len)
{
    return ccc_apdu_process(req, len, resp, (uint16_t)sizeof(resp), &resp_len);
}

/* ─── 解析 ─── */

void test_parse_case1_no_data(void) {
    ccc_apdu_t a;
    uint8_t buf[4] = { 0x00, 0x84, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_apdu_parse(buf, 4, &a));
    TEST_ASSERT_EQUAL_UINT8(0x84, a.ins);
    TEST_ASSERT_EQUAL_UINT16(0, a.lc);
    TEST_ASSERT_NULL(a.data);
    TEST_ASSERT_FALSE(a.has_le);
}

void test_parse_case2_le(void) {
    ccc_apdu_t a;
    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_apdu_parse(GET_CHALLENGE, 5, &a));
    TEST_ASSERT_TRUE(a.has_le);
    TEST_ASSERT_EQUAL_UINT16(8, a.le);
    TEST_ASSERT_EQUAL_UINT16(0, a.lc);
}

void test_parse_case3_no_le(void) {
    ccc_apdu_t a;
    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_apdu_parse(SELECT_APDU, 18, &a));
    TEST_ASSERT_EQUAL_UINT16(13, a.lc);
    TEST_ASSERT_FALSE(a.has_le);
    TEST_ASSERT_EQUAL_MEMORY(CCC_APDU_DK_AID, a.data, CCC_APDU_AID_LEN);
}

void test_parse_case4_with_le(void) {
    ccc_apdu_t a;
    TEST_ASSERT_EQUAL_INT(CCC_OK, ccc_apdu_parse(SELECT_APDU, 19, &a));
    TEST_ASSERT_EQUAL_UINT16(13, a.lc);
    TEST_ASSERT_TRUE(a.has_le);
}

void test_parse_rejects_short(void) {
    ccc_apdu_t a;
    uint8_t buf[3] = { 0x00, 0xA4, 0x04 };
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ccc_apdu_parse(buf, 3, &a));
}

void test_parse_rejects_lc_overflow(void) {
    ccc_apdu_t a;
    uint8_t buf[6] = { 0x00, 0xA4, 0x04, 0x00, 0xEC, 0x00 };  /* Lc=236 > 235 */
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ccc_apdu_parse(buf, 6, &a));
}

void test_parse_rejects_truncated_data(void) {
    ccc_apdu_t a;
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ccc_apdu_parse(SELECT_APDU, 10, &a));
}

void test_parse_rejects_stuffed(void) {
    ccc_apdu_t a;
    uint8_t buf[8] = { 0x00, 0xA4, 0x04, 0x00, 0x01, 0xAA, 0xBB, 0xCC };
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ccc_apdu_parse(buf, 8, &a));
}

/* ─── SELECT ─── */

void test_select_valid_aid(void) {
    reset_session();
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(SELECT_APDU, 19));
    TEST_ASSERT_EQUAL_UINT16(2, resp_len);
    TEST_ASSERT_EQUAL_HEX8(0x90, resp[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[1]);
    TEST_ASSERT_TRUE(ccc_apdu_is_selected());
    TEST_ASSERT_FALSE(ccc_apdu_is_authenticated());
}

void test_select_wrong_aid_rejected(void) {
    reset_session();
    uint8_t bad[19];
    (void)memcpy(bad, SELECT_APDU, sizeof(bad));
    bad[5 + CCC_APDU_AID_LEN - 1] ^= 0xFF;   /* 篡改 AID 最后一字节 */

    TEST_ASSERT_EQUAL_HEX16(CCC_SW_FILE_NOT_FOUND, send(bad, 19));
    TEST_ASSERT_FALSE(ccc_apdu_is_selected());
}

void test_select_wrong_p1p2_rejected(void) {
    reset_session();
    uint8_t bad[19];
    (void)memcpy(bad, SELECT_APDU, sizeof(bad));
    bad[2] = 0x00;   /* P1 必须 0x04 */

    TEST_ASSERT_EQUAL_HEX16(CCC_SW_WRONG_P1P2, send(bad, 19));
    TEST_ASSERT_FALSE(ccc_apdu_is_selected());
}

/* ─── GET CHALLENGE ─── */

void test_get_challenge_requires_select(void) {
    reset_session();
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_CONDITIONS_NOT_SAT, send(GET_CHALLENGE, 5));
}

void test_get_challenge_returns_8_bytes(void) {
    reset_session();
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(SELECT_APDU, 19));
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(GET_CHALLENGE, 5));

    TEST_ASSERT_EQUAL_UINT16(CCC_APDU_CHALLENGE_LEN + 2, resp_len);
    TEST_ASSERT_EQUAL_HEX8(0x90, resp[CCC_APDU_CHALLENGE_LEN]);
    TEST_ASSERT_EQUAL_HEX8(0x00, resp[CCC_APDU_CHALLENGE_LEN + 1]);

    /* 挑战不应全零 (随机源健康) */
    uint8_t acc = 0;
    for (uint16_t i = 0; i < CCC_APDU_CHALLENGE_LEN; i++) acc |= resp[i];
    TEST_ASSERT_NOT_EQUAL(0, acc);
}

/* ─── INTERNAL AUTHENTICATE ─── */

void test_internal_auth_requires_challenge(void) {
    reset_session();
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(SELECT_APDU, 19));

    uint8_t auth[5] = { 0x00, 0x88, 0x00, 0x00, 0x01 };
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_CONDITIONS_NOT_SAT, send(auth, 5));
}

void test_internal_auth_without_peer_key_fails_closed(void) {
    reset_session();
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(SELECT_APDU, 19));
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(GET_CHALLENGE, 5));

    /* 1 字节假签名 + Le */
    uint8_t auth[7] = { 0x00, 0x88, 0x00, 0x00, 0x01, 0xAA, 0x00 };
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_CONDITIONS_NOT_SAT, send(auth, 7));

    /* 挑战一次性: 失败后再次认证仍然拒绝 */
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_CONDITIONS_NOT_SAT, send(auth, 7));
    TEST_ASSERT_FALSE(ccc_apdu_is_authenticated());
}

/* ─── 车辆指令 ─── */

void test_vehicle_cmd_requires_auth(void) {
    reset_session();
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(SELECT_APDU, 19));

    uint8_t cmd[7] = { 0x80, 0xD2, 0x01, 0x00, 0x01, 0xAA, 0x00 };
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_CONDITIONS_NOT_SAT, send(cmd, 7));
}

/* ─── 未知命令 / 类别 ─── */

void test_unknown_ins_rejected(void) {
    reset_session();
    uint8_t buf[4] = { 0x00, 0x55, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_INS_NOT_SUPPORTED, send(buf, 4));
}

void test_unsupported_cla_rejected(void) {
    reset_session();
    uint8_t buf[4] = { 0x40, 0x84, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_CLA_NOT_SUPPORTED, send(buf, 4));
}

void test_malformed_apdu_returns_6700(void) {
    reset_session();
    uint8_t buf[2] = { 0x00, 0xA4 };
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_WRONG_LENGTH, send(buf, 2));
    TEST_ASSERT_EQUAL_UINT16(2, resp_len);   /* 仍返回 SW, 便于对端定位 */
}

/** 重新 SELECT 必须作废旧会话认证态 */
void test_reselect_invalidates_challenge(void) {
    reset_session();
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(SELECT_APDU, 19));
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(GET_CHALLENGE, 5));
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_OK, send(SELECT_APDU, 19));

    uint8_t auth[7] = { 0x00, 0x88, 0x00, 0x00, 0x01, 0xAA, 0x00 };
    TEST_ASSERT_EQUAL_HEX16(CCC_SW_CONDITIONS_NOT_SAT, send(auth, 7));
}

int run_ccc_apdu_tests(void) {
    printf("\n--- CCC v4.0 APDU Dispatcher ---\n");

    (void)crypto_random_init();
    ccc_apdu_reset();

    UNITY_BEGIN();

    RUN_TEST(test_parse_case1_no_data);
    RUN_TEST(test_parse_case2_le);
    RUN_TEST(test_parse_case3_no_le);
    RUN_TEST(test_parse_case4_with_le);
    RUN_TEST(test_parse_rejects_short);
    RUN_TEST(test_parse_rejects_lc_overflow);
    RUN_TEST(test_parse_rejects_truncated_data);
    RUN_TEST(test_parse_rejects_stuffed);

    RUN_TEST(test_select_valid_aid);
    RUN_TEST(test_select_wrong_aid_rejected);
    RUN_TEST(test_select_wrong_p1p2_rejected);

    RUN_TEST(test_get_challenge_requires_select);
    RUN_TEST(test_get_challenge_returns_8_bytes);

    RUN_TEST(test_internal_auth_requires_challenge);
    RUN_TEST(test_internal_auth_without_peer_key_fails_closed);

    RUN_TEST(test_vehicle_cmd_requires_auth);

    RUN_TEST(test_unknown_ins_rejected);
    RUN_TEST(test_unsupported_cla_rejected);
    RUN_TEST(test_malformed_apdu_returns_6700);
    RUN_TEST(test_reselect_invalidates_challenge);

    UNITY_END();
}
