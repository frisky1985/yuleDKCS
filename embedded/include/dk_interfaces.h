/**
 * @file dk_interfaces.h
 * @brief yuleDKCS 统一接口契约 — 总览头 (ASPICE SWE.2.BP2 evidence)
 *
 * 汇总层接口契约：消息信封、消息类型码、KeyType/AccessLevel 枚举与范围。
 * 引用: docs/architecture.md §4.4, docs/design/HUB-DETAILED-DESIGN.md §6
 *
 * 本头文件仅定义接口契约（数据类型与范围），不参与产品构建；
 * 产品实现见 backend/cloud/hub/internal/unified/ 与 embedded/*/include/。
 */
#ifndef YULEDKCS_DK_INTERFACES_H
#define YULEDKCS_DK_INTERFACES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 消息信封 (BERTLV) ─────────────────────────────────────────────
 * 结构: Header (E1 01) + Body (BERTLV Tag-Length-Value) + Trailer (E1 FF)
 * 需求: REQ-024-S2, REQ-018
 */
#define DK_MSG_ENVELOPE_HEADER_MARK   0xE101u   /* Header 起始标记 */
#define DK_MSG_ENVELOPE_TRAILER_MARK  0xE1FFu   /* Trailer 签名段标记 */

/* 消息头字段 (E1 01..E1 09), 格式: BCD/N14/N4/N8/AN16/AN32 */
#define DK_HEADER_TAG_VERSION       0xE101u   /* BCD, "0100" = v1.0 */
#define DK_HEADER_TAG_TIMESTAMP     0xE102u   /* N14, YYYYMMDDhhmmss */
#define DK_HEADER_TAG_MSGTYPE       0xE103u   /* N4, 消息类型码 */
#define DK_HEADER_TAG_SEQUENCE      0xE104u   /* N8, 序列号 */
#define DK_HEADER_TAG_DEVICE_ID     0xE105u   /* AN16, 设备ID */
#define DK_HEADER_TAG_SESSION_ID    0xE106u   /* AN32, 会话ID(可选) */
#define DK_HEADER_TAG_PRIORITY      0xE107u   /* N1, 1低/2中/3高/4紧急 */
#define DK_HEADER_TAG_FLAGS         0xE108u   /* B1, 标志位 */
#define DK_HEADER_TAG_CORRELATION   0xE109u   /* AN32, 关联消息ID */

/* 消息尾字段 (E1 FF) */
#define DK_TRAILER_TAG_SIGNATURE    0xE1FF01u /* HMAC-SHA256, 32B */
#define DK_TRAILER_TAG_MAC_KEY_ID   0xE1FF02u /* AN16, MAC密钥ID */

/* ── 消息类型码 (N4) ────────────────────────────────────────────────
 * 需求: REQ-010 ~ REQ-017
 */
enum dk_message_type {
    DK_MSG_KEY_BIND_REQ      = 1000,  /* REQ-010 密钥绑定请求 */
    DK_MSG_KEY_BIND_RSP      = 1001,
    DK_MSG_KEY_UNBIND_REQ    = 1002,  /* REQ-011 密钥解绑 */
    DK_MSG_KEY_UNBIND_RSP    = 1003,
    DK_MSG_KEY_REVOKE_REQ    = 1004,  /* REQ-012 密钥撤销 (Emergency=1 紧急) */
    DK_MSG_KEY_REVOKE_RSP    = 1005,
    DK_MSG_KEY_LIST_REQ      = 1010,  /* REQ-013 密钥列表 (Page/PageSize) */
    DK_MSG_KEY_LIST_RSP      = 1011,
    DK_MSG_SHARE_CREATE_REQ  = 2000,  /* REQ-014 分享创建 */
    DK_MSG_SHARE_CREATE_RSP  = 2001,
    DK_MSG_VEHICLE_CMD_REQ   = 3000,  /* REQ-015 车辆控制指令 */
    DK_MSG_VEHICLE_CMD_RSP   = 3001,
    DK_MSG_VEHICLE_STATUS    = 3002,  /* REQ-016 车辆状态上报 */
    DK_MSG_HEARTBEAT_REQ     = 9000,  /* REQ-017 心跳 */
    DK_MSG_HEARTBEAT_RSP     = 9001
};

/* ── KeyType (N2) ──────────────────────────────────────────────────
 * 需求: REQ-010-S3
 */
enum dk_key_type {
    DK_KEY_TYPE_OWNER     = 0x01,  /* 车主钥匙 */
    DK_KEY_TYPE_FRIEND    = 0x02,  /* 亲友钥匙 */
    DK_KEY_TYPE_SERVICE   = 0x03,  /* 服务钥匙 */
    DK_KEY_TYPE_TEMPORARY = 0x04   /* 临时钥匙 */
};

/* ── AccessLevel (16-bit 位掩码, B4) ───────────────────────────────
 * 需求: REQ-010-S4; 定义位 0~15，未定义位保留
 */
enum dk_access_level_bits {
    DK_ACCESS_LOCK      = (1u << 0),  /* 上锁 */
    DK_ACCESS_UNLOCK    = (1u << 1),  /* 解锁 */
    DK_ACCESS_ENGINE    = (1u << 2),  /* 启动发动机 */
    DK_ACCESS_TRUNK     = (1u << 3),  /* 后备箱 */
    DK_ACCESS_WINDOW    = (1u << 4),  /* 车窗 */
    DK_ACCESS_CLIMATE   = (1u << 5),  /* 空调 */
    DK_ACCESS_FIND      = (1u << 6),  /* 寻车 */
    DK_ACCESS_SEAT      = (1u << 7)   /* 座椅 */
};
#define DK_ACCESS_LEVEL_MASK  0xFFFFu  /* 全部 16 位 */

/* ── 车辆控制 Action / Source ──────────────────────────────────────
 * 需求: REQ-015-S2/S3
 */
enum dk_vehicle_action {
    DK_ACTION_UNLOCK = 1, DK_ACTION_LOCK, DK_ACTION_ENGINE_START,
    DK_ACTION_ENGINE_STOP, DK_ACTION_TRUNK_OPEN, DK_ACTION_WINDOW_UP,
    DK_ACTION_WINDOW_DOWN, DK_ACTION_CLIMATE_ON, DK_ACTION_CLIMATE_OFF,
    DK_ACTION_FIND_VEHICLE, DK_ACTION_HORN
};  /* 共 11 种 */

enum dk_command_source {
    DK_SOURCE_NFC = 1, DK_SOURCE_BLE, DK_SOURCE_UWB,
    DK_SOURCE_REMOTE, DK_SOURCE_EDGE
};  /* 共 5 种 */

/* ── 消息头结构 (与 HUB-DETAILED-DESIGN §6.2 对齐) ────────────────── */
typedef struct dk_header {
    uint8_t  version[4];       /* BCD "0100" */
    char     timestamp[15];    /* N14 YYYYMMDDhhmmss + NUL */
    uint16_t message_type;     /* N4, 见 dk_message_type */
    uint32_t sequence_no;      /* N8, 递增序列号 */
    char     device_id[17];    /* AN16 + NUL */
    char     session_id[33];   /* AN32 + NUL (可选) */
    uint8_t  priority;         /* N1, 1~4 */
    uint8_t  flags;            /* B1 */
    char     correlation_id[33]; /* AN32 + NUL (可选) */
} dk_header_t;

/* ── 消息尾结构 ──────────────────────────────────────────────────── */
typedef struct dk_trailer {
    uint8_t signature[32];     /* HMAC-SHA256(Header+Body), 32B */
    char     mac_key_id[17];   /* AN16 + NUL */
} dk_trailer_t;

#ifdef __cplusplus
}
#endif

#endif /* YULEDKCS_DK_INTERFACES_H */
