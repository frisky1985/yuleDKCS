/**
 * test_ccc_frame.c — CCC Digital Key v4.0 BLE 帧编解码单测
 *
 * 规范来源: CCC-TS-101 Table 19-19 / Table 19-21
 * 跨语言对齐: iOS CCCCommandFrame.swift / Android CccFrame.kt
 *
 * 线格式 (4 字节帧头 + 负载):
 *   [0]     message_header  Bit[5:0]=Message Type, Bit[7:6]=RFU(置 0)
 *   [1]     payload_header  Message ID (DK_APDU_RQ=0x0B / DK_APDU_RS=0x0C)
 *   [2-3]   length          负载长度 (大端 u16)
 *   [4...]  payload
 *
 * 规范示例: SELECT APDU →
 *   0x01 0x0B 0x00 0x13 00A404000DA000000809434343444B41763100
 *   = Message Type 0x01(SE) + DK_APDU_RQ 0x0B + BE Len 19 + 19B APDU
 *
 * iPhone / Android / 车端必须字节级一致.
 */

#include "unity.h"
#include "ccc_digital_key.h"
#include <string.h>
#include <stdio.h>

/* ─── 规范示例向量 (CCC-TS-101 §19.3) ─── */

/** SELECT APDU payload: 00 A4 04 00 0D A0 00 00 00 80 94 34 34 34 34 44 4B 41 76 31 00 */
static const uint8_t SELECT_APDU_PAYLOAD[] = {
    0x00, 0xA4, 0x04, 0x00, 0x0D, 0xA0, 0x00, 0x00, 0x00,
    0x80, 0x94, 0x34, 0x34, 0x34, 0x34, 0x44, 0x4B, 0x41,
    0x76, 0x31, 0x00
};
#define SELECT_APDU_LEN  21

/** 完整 SELECT APDU 帧 (规范示例, 4B 帧头 + 21B payload = 25B) */
static const uint8_t SELECT_APDU_FRAME[] = {
    0x01, 0x0B, 0x00, 0x15,                         /* 帧头: SE(1) + DK_APDU_RQ(11) + BE len 21 */
    0x00, 0xA4, 0x04, 0x00, 0x0D, 0xA0, 0x00, 0x00,
    0x00, 0x80, 0x94, 0x34, 0x34, 0x34, 0x34, 0x44,
    0x4B, 0x41, 0x76, 0x31, 0x00
};
#define SELECT_APDU_FRAME_LEN  25  /* 4 + 21 */

/* ─── msg_type_from_header 辅助 ─── */

void test_msg_type_from_header(void) {
    /* 正常 Message Type */
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_TYPE_FRAMEWORK, ccc_msg_type_from_header(0x00));
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_TYPE_SE, ccc_msg_type_from_header(0x01));
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_TYPE_DK_EVENT_NOTIFY, ccc_msg_type_from_header(0x03));
    /* RFU 位必须屏蔽 */
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_TYPE_SE, ccc_msg_type_from_header(0x41));  /* RFU bit6=1 */
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_TYPE_SE, ccc_msg_type_from_header(0x81));  /* RFU bit7=1 */
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_TYPE_SE, ccc_msg_type_from_header(0xC1));  /* RFU bits 7:6=11 */
}

/* ─── ccc_frame_build 成功路径 ─── */

void test_frame_build_select_apdu_spec_example(void) {
    uint8_t frame[64];
    uint16_t frame_len = 0;

    ccc_status_t ret = ccc_frame_build(
        CCC_MSG_TYPE_SE, CCC_MSG_ID_DK_APDU_RQ,
        SELECT_APDU_PAYLOAD, SELECT_APDU_LEN,
        frame, &frame_len
    );

    TEST_ASSERT_EQUAL_INT(CCC_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(SELECT_APDU_FRAME_LEN, frame_len);
    TEST_ASSERT_EQUAL_MEMORY(SELECT_APDU_FRAME, frame, SELECT_APDU_FRAME_LEN);
}

void test_frame_build_dk_event_notify(void) {
    /* 车辆状态上报: DK Event Notification (Message Type=3), 裸 3B 状态 */
    uint8_t status_payload[] = { 0x01, 0x00, 0x45 };  /* locked=1, engine_on=0, battery=69 */
    uint8_t frame[16];
    uint16_t frame_len = 0;

    ccc_status_t ret = ccc_frame_build(
        CCC_MSG_TYPE_DK_EVENT_NOTIFY, CCC_MSG_ID_DK_APDU_RS,
        status_payload, sizeof(status_payload),
        frame, &frame_len
    );

    TEST_ASSERT_EQUAL_INT(CCC_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(7, frame_len);  /* 4 + 3 */
    /* 帧头验证 */
    TEST_ASSERT_EQUAL_UINT8(0x03, frame[0]);  /* DK_EVENT_NOTIFY */
    TEST_ASSERT_EQUAL_UINT8(0x0C, frame[1]);  /* DK_APDU_RS */
    TEST_ASSERT_EQUAL_UINT8(0x00, frame[2]);  /* BE len high */
    TEST_ASSERT_EQUAL_UINT8(0x03, frame[3]);  /* BE len low */
    /* payload 验证 */
    TEST_ASSERT_EQUAL_UINT8(0x01, frame[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00, frame[5]);
    TEST_ASSERT_EQUAL_UINT8(0x45, frame[6]);
}

void test_frame_build_empty_payload(void) {
    uint8_t frame[4];
    uint16_t frame_len = 0;

    ccc_status_t ret = ccc_frame_build(
        CCC_MSG_TYPE_FRAMEWORK, CCC_MSG_ID_DK_APDU_RS,
        NULL, 0,
        frame, &frame_len
    );

    TEST_ASSERT_EQUAL_INT(CCC_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(4, frame_len);  /* 仅帧头 */
    TEST_ASSERT_EQUAL_UINT8(0x00, frame[0]);  /* FRAMEWORK */
    TEST_ASSERT_EQUAL_UINT8(0x0C, frame[1]);  /* DK_APDU_RS */
    TEST_ASSERT_EQUAL_UINT8(0x00, frame[2]);  /* BE len 0 */
    TEST_ASSERT_EQUAL_UINT8(0x00, frame[3]);
}

/* ─── ccc_frame_build 负向测试 ─── */

void test_frame_build_null_out_frame(void) {
    ccc_status_t ret = ccc_frame_build(CCC_MSG_TYPE_SE, CCC_MSG_ID_DK_APDU_RQ,
                                       NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_INVALID_PARAM, ret);
}

void test_frame_build_msg_type_out_of_range(void) {
    uint8_t frame[64];
    uint16_t frame_len;
    ccc_status_t ret = ccc_frame_build(0xFF, CCC_MSG_ID_DK_APDU_RQ,
                                       NULL, 0, frame, &frame_len);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_INVALID_PARAM, ret);
}

void test_frame_build_payload_too_large(void) {
    uint8_t frame[512];
    uint16_t frame_len;
    /* payload 超过 BLE_MAX_PAYLOAD - 4 = 240 */
    uint8_t big_payload[241];
    memset(big_payload, 0xAA, sizeof(big_payload));
    ccc_status_t ret = ccc_frame_build(CCC_MSG_TYPE_SE, CCC_MSG_ID_DK_APDU_RQ,
                                       big_payload, sizeof(big_payload),
                                       frame, &frame_len);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_INVALID_PARAM, ret);
}

/* ─── ccc_frame_parse 成功路径 ─── */

void test_frame_parse_select_apdu_spec_example(void) {
    ccc_frame_header_v4_t hdr;
    const uint8_t *payload = NULL;
    uint16_t payload_len = 0;

    ccc_status_t ret = ccc_frame_parse(
        SELECT_APDU_FRAME, SELECT_APDU_FRAME_LEN,
        &hdr, &payload, &payload_len
    );

    TEST_ASSERT_EQUAL_INT(CCC_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_TYPE_SE, ccc_msg_type_from_header(hdr.message_header));
    TEST_ASSERT_EQUAL_UINT8(CCC_MSG_ID_DK_APDU_RQ, hdr.payload_header);
    TEST_ASSERT_EQUAL_UINT16(SELECT_APDU_LEN, payload_len);
    TEST_ASSERT_EQUAL_MEMORY(SELECT_APDU_PAYLOAD, payload, SELECT_APDU_LEN);
}

void test_frame_parse_roundtrip_all_msg_types(void) {
    /* 所有规范 Message Type 都能正确 roundtrip */
    for (uint8_t mt = 0; mt <= CCC_MSG_TYPE_HEAD_UNIT_PAIR; mt++) {
        uint8_t frame[64];
        uint16_t frame_len = 0;
        uint8_t payload[10] = { 0 };
        payload[0] = mt;  /* 标记 payload */

        TEST_ASSERT_EQUAL_INT(CCC_OK,
            ccc_frame_build(mt, CCC_MSG_ID_DK_APDU_RS,
                            payload, sizeof(payload),
                            frame, &frame_len));

        ccc_frame_header_v4_t hdr;
        const uint8_t *parsed_payload;
        uint16_t parsed_len;
        TEST_ASSERT_EQUAL_INT(CCC_OK,
            ccc_frame_parse(frame, frame_len, &hdr, &parsed_payload, &parsed_len));

        TEST_ASSERT_EQUAL_UINT8(mt, ccc_msg_type_from_header(hdr.message_header));
        TEST_ASSERT_EQUAL_UINT16(sizeof(payload), parsed_len);
        TEST_ASSERT_EQUAL_MEMORY(payload, parsed_payload, sizeof(payload));
    }
}

/* ─── ccc_frame_parse 负向测试 (fail-closed) ─── */

void test_frame_parse_too_short(void) {
    uint8_t buf[3] = { 0x01, 0x0B, 0x00 };  /* 仅 3 字节, 帧头需要 4 */
    ccc_frame_header_v4_t hdr;
    ccc_status_t ret = ccc_frame_parse(buf, 3, &hdr, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ret);
}

void test_frame_parse_rfu_bits_not_zero(void) {
    uint8_t frame[8] = {
        0xC1,  /* Bit[7:6]=11 (RFU 非零), Bit[5:0]=SE */
        0x0B,
        0x00, 0x04,
        0xAA, 0xBB, 0xCC, 0xDD
    };
    ccc_frame_header_v4_t hdr;
    ccc_status_t ret = ccc_frame_parse(frame, 8, &hdr, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ret);
}

void test_frame_parse_length_truncated(void) {
    /* 声明 payload_len=10, 实际只有 5 — 截断 */
    uint8_t frame[9] = {
        0x01, 0x0B,
        0x00, 0x0A,  /* BE len = 10 */
        0x01, 0x02, 0x03, 0x04, 0x05
    };
    ccc_frame_header_v4_t hdr;
    ccc_status_t ret = ccc_frame_parse(frame, 9, &hdr, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ret);
}

void test_frame_parse_length_stuffed(void) {
    /* 声明 payload_len=2, 实际有 5 — 粘包 */
    uint8_t frame[11] = {
        0x01, 0x0B,
        0x00, 0x02,  /* BE len = 2 */
        0x01, 0x02,
        0xAA, 0xBB, 0xCC  /* 多余数据 */
    };
    ccc_frame_header_v4_t hdr;
    ccc_status_t ret = ccc_frame_parse(frame, 11, &hdr, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ret);
}

void test_frame_parse_null_hdr(void) {
    uint8_t frame[8] = { 0x01, 0x0B, 0x00, 0x04, 0xAA, 0xBB, 0xCC, 0xDD };
    ccc_status_t ret = ccc_frame_parse(frame, 8, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CCC_ERR_INVALID_PARAM, ret);
}

void test_frame_parse_be_length_zero(void) {
    /* 空 payload 帧是合法的 */
    uint8_t frame[4] = { 0x01, 0x0B, 0x00, 0x00 };
    ccc_frame_header_v4_t hdr;
    const uint8_t *payload;
    uint16_t payload_len;
    ccc_status_t ret = ccc_frame_parse(frame, 4, &hdr, &payload, &payload_len);
    TEST_ASSERT_EQUAL_INT(CCC_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(0, payload_len);
    TEST_ASSERT_NULL(payload);
}

/* ─── ccc_ble_send_v4 负向测试 ─── */

void test_ble_send_v4_payload_too_large(void) {
    uint8_t big[BLE_MAX_PAYLOAD];  /* 超过 244-4=240 */
    memset(big, 0xAA, sizeof(big));
    ccc_status_t ret = ccc_ble_send_v4(0x1234, CCC_MSG_TYPE_SE, CCC_MSG_ID_DK_APDU_RQ,
                                       big, sizeof(big));
    TEST_ASSERT_EQUAL_INT(CCC_ERR_PARAM, ret);
}

/* ─── 统一 runner ─── */

int run_ccc_frame_tests(void) {
    printf("\n--- CCC Digital Key v4.0 Frame (协议一致性锚点) ---\n");

    UNITY_BEGIN();

    /* msg_type_from_header */
    RUN_TEST(test_msg_type_from_header);

    /* ccc_frame_build — 规范示例 */
    RUN_TEST(test_frame_build_select_apdu_spec_example);
    RUN_TEST(test_frame_build_dk_event_notify);
    RUN_TEST(test_frame_build_empty_payload);

    /* ccc_frame_build — 负向 */
    RUN_TEST(test_frame_build_null_out_frame);
    RUN_TEST(test_frame_build_msg_type_out_of_range);
    RUN_TEST(test_frame_build_payload_too_large);

    /* ccc_frame_parse — 成功 */
    RUN_TEST(test_frame_parse_select_apdu_spec_example);
    RUN_TEST(test_frame_parse_roundtrip_all_msg_types);

    /* ccc_frame_parse — 负向 (fail-closed) */
    RUN_TEST(test_frame_parse_too_short);
    RUN_TEST(test_frame_parse_rfu_bits_not_zero);
    RUN_TEST(test_frame_parse_length_truncated);
    RUN_TEST(test_frame_parse_length_stuffed);
    RUN_TEST(test_frame_parse_null_hdr);
    RUN_TEST(test_frame_parse_be_length_zero);

    /* ccc_ble_send_v4 — 负向 */
    RUN_TEST(test_ble_send_v4_payload_too_large);

    UNITY_END();
}
