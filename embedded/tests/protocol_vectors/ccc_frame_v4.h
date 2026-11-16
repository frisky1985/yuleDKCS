/**
 * @file ccc_frame_v4.h
 * @brief CCC Digital Key v4.0 BLE 帧 — 跨语言固定字节向量 (单一来源)
 *
 * 规范: CCC-TS-101 Table 19-19 (frame) / Table 19-21 (Message Type)
 *
 * 线格式 (4 字节帧头 + 负载):
 *   [0]     message_header  Bit[5:0]=Message Type, Bit[7:6]=RFU(必须 0)
 *   [1]     payload_header  Message ID (DK_APDU_RQ=0x0B / DK_APDU_RS=0x0C)
 *   [2-3]   length          负载长度 (大端 u16)
 *   [4...]  payload
 *
 * 车端 (C, ble_kw47a.c) / iOS (CCCCommandFrame.swift) / Android (CccFrame.kt)
 * 的编解码必须逐字节与下列向量一致. 任何一端改动都需要同时更新本文件与
 * 各端测试, 否则 CI 的跨语言一致性断言失败.
 *
 * 用途: 仅供测试与联调, 不参与固件编译.
 */

#ifndef CCC_FRAME_V4_VECTORS_H
#define CCC_FRAME_V4_VECTORS_H

#include <stdint.h>

/* ========================================================================
 *  向量 1: SELECT APDU (规范 §19.3 示例, 合法请求)
 *
 *  0x01 0x0B 0x00 0x15 | 00 A4 04 00 0D A0 00 00 00 80 94 34 34 34 34 44 4B 41 76 31 00
 *  = SE(1) + DK_APDU_RQ(0x0B) + BE len 21 + 21B SELECT APDU
 * ======================================================================== */
static const uint8_t CCC_VEC_SELECT_APDU_PAYLOAD[] = {
    0x00, 0xA4, 0x04, 0x00, 0x0D, 0xA0, 0x00, 0x00,
    0x00, 0x80, 0x94, 0x34, 0x34, 0x34, 0x34, 0x44,
    0x4B, 0x41, 0x76, 0x31, 0x00
};
static const uint16_t CCC_VEC_SELECT_APDU_PAYLOAD_LEN = 21;

static const uint8_t CCC_VEC_SELECT_APDU_FRAME[] = {
    0x01, 0x0B, 0x00, 0x15,
    0x00, 0xA4, 0x04, 0x00, 0x0D, 0xA0, 0x00, 0x00,
    0x00, 0x80, 0x94, 0x34, 0x34, 0x34, 0x34, 0x44,
    0x4B, 0x41, 0x76, 0x31, 0x00
};
static const uint16_t CCC_VEC_SELECT_APDU_FRAME_LEN = 25;  /* 4 + 21 */

/* ========================================================================
 *  向量 2: DK Event Notification (车 → 手机 状态上报, 合法)
 *
 *  0x03 0x0C 0x00 0x03 | 01 00 45
 *  = DK_EVENT_NOTIFY(3) + DK_APDU_RS(0x0C) + BE len 3
 *    payload: locked=1, engine_on=0, battery=69
 * ======================================================================== */
static const uint8_t CCC_VEC_DK_EVENT_PAYLOAD[] = { 0x01, 0x00, 0x45 };

static const uint8_t CCC_VEC_DK_EVENT_FRAME[] = {
    0x03, 0x0C, 0x00, 0x03, 0x01, 0x00, 0x45
};
static const uint16_t CCC_VEC_DK_EVENT_FRAME_LEN = 7;  /* 4 + 3 */

/* ========================================================================
 *  向量 3: 空负载帧 (合法, 仅帧头)
 *
 *  0x00 0x0C 0x00 0x00
 *  = FRAMEWORK(0) + DK_APDU_RS(0x0C) + BE len 0
 * ======================================================================== */
static const uint8_t CCC_VEC_EMPTY_FRAME[] = { 0x00, 0x0C, 0x00, 0x00 };
static const uint16_t CCC_VEC_EMPTY_FRAME_LEN = 4;

/* ========================================================================
 *  向量 4 (负向): RFU 位非零 — 必须拒绝
 *
 *  0xC1 = Bit[7:6]=11 (RFU 非 0), Bit[5:0]=SE(1)
 * ======================================================================== */
static const uint8_t CCC_VEC_INVALID_RFU_FRAME[] = {
    0xC1, 0x0B, 0x00, 0x04, 0xAA, 0xBB, 0xCC, 0xDD
};
static const uint16_t CCC_VEC_INVALID_RFU_FRAME_LEN = 8;

/* ========================================================================
 *  向量 5 (负向): 长度截断 — 声明 10, 实际 5, 必须拒绝
 * ======================================================================== */
static const uint8_t CCC_VEC_TRUNCATED_FRAME[] = {
    0x01, 0x0B, 0x00, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05
};
static const uint16_t CCC_VEC_TRUNCATED_FRAME_LEN = 9;

/* ========================================================================
 *  向量 6 (负向): 长度粘包 — 声明 2, 实际 7 (含多余 5B), 必须拒绝
 * ======================================================================== */
static const uint8_t CCC_VEC_STUFFED_FRAME[] = {
    0x01, 0x0B, 0x00, 0x02, 0x01, 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE
};
static const uint16_t CCC_VEC_STUFFED_FRAME_LEN = 11;

#endif /* CCC_FRAME_V4_VECTORS_H */
