/**
 * @file dk_protocol.h
 * @brief yuleDKCS 协议接口契约 (ASPICE SWE.2.BP2 evidence)
 *
 * BERTLV 编码规则、MQTT topic/QoS 映射、REST 统一响应、错误码段位。
 * 引用: docs/architecture.md §4.1, backend/cloud/protocol/*.md
 * 需求: REQ-024 ~ REQ-027
 */
#ifndef YULEDKCS_DK_PROTOCOL_H
#define YULEDKCS_DK_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── BERTLV Length 编码规则 ────────────────────────────────────────
 * 需求: REQ-024-S3
 *   00-7F        : 单字节长度 (值 = 字节数)
 *   80-FF 首字节 : 长度后续字节计数 (80=1, 81=2, ...)
 */
#define DK_TLV_LEN_SINGLE_MAX  0x7Fu   /* 单字节长度上限 127 */
#define DK_TLV_LEN_MULTI_BASE  0x80u   /* 多字节长度首字节基数 */

/* ── 消息完整性 ────────────────────────────────────────────────────
 * 需求: REQ-018-S2/S3, REQ-024-S4
 * 签名: HMAC-SHA256(Header + Body, sessionKey), 输出 32B
 */
#define DK_HMAC_SHA256_LEN  32u

/* ── MQTT 协议 (DKCS↔TCU) ─────────────────────────────────────────
 * 需求: REQ-025
 * Topic 格式: digitalkey/{tsp_id}/{vehicle_id}/{resource}/{action}
 * 资源: control | keysync | status | heartbeat | ota
 */
#define DK_MQTT_TOPIC_TEMPLATE  "digitalkey/%s/%s/%s/%s"

enum dk_mqtt_qos {
    DK_MQTT_QOS_AT_MOST_ONCE  = 0,  /* 心跳、状态上报 */
    DK_MQTT_QOS_AT_LEAST_ONCE = 1,  /* 密钥同步 */
    DK_MQTT_QOS_EXACTLY_ONCE  = 2   /* 控制指令、密钥绑定 */
};

/* ── REST 统一响应结构 (App↔HUB) ───────────────────────────────────
 * 需求: REQ-026, REQ-038-S3
 * 字段: code, message, data, requestId, timestamp
 */
enum dk_rest_http_status {
    DK_HTTP_200_OK          = 200,
    DK_HTTP_401_UNAUTH      = 401,   /* token_expired / invalid_signature / token_revoked */
    DK_HTTP_403_FORBIDDEN   = 403,   /* 权限不足 (非车主分享/吊销) */
    DK_HTTP_404_NOT_FOUND   = 404,
    DK_HTTP_409_CONFLICT    = 409,   /* 钥匙已达上限 (单车辆 ≤5) */
    DK_HTTP_503_UNAVAILABLE = 503    /* 车辆离线 */
};

/* ── 统一错误码段位 ────────────────────────────────────────────────
 * 需求: REQ-027-S2
 *   1xxx 请求错误 | 2xxx 密钥错误 | 3xxx 车辆错误 | 4xxx 厂商错误
 */
#define DK_ERR_SEGMENT_REQUEST  0x1000u
#define DK_ERR_SEGMENT_KEY      0x2000u
#define DK_ERR_SEGMENT_VEHICLE  0x3000u
#define DK_ERR_SEGMENT_VENDOR   0x4000u

/* 常用业务错误码 (与 HUB-DETAILED-DESIGN §4.4 对齐) */
enum dk_biz_error_code {
    DK_ERR_INVALID_REQUEST       = 0x1001,  /* 协议不匹配/请求非法 */
    DK_ERR_KEY_LIMIT_EXCEEDED    = 0x2006,  /* 单车辆钥匙数量超限 */
    DK_ERR_PERMISSION_DENIED     = 0x2007,  /* 权限不足 */
    DK_ERR_VEHICLE_OFFLINE       = 0x3002,  /* 车辆离线 */
    DK_ERR_VENDOR_ADAPTER_MISS   = 0x4001,  /* 适配器未注册 */
    DK_ERR_VENDOR_API_ERROR      = 0x4002,  /* 厂商 API 错误 */
    DK_ERR_VENDOR_TIMEOUT        = 0x4003   /* 厂商 API 超时 */
};

#ifdef __cplusplus
}
#endif

#endif /* YULEDKCS_DK_PROTOCOL_H */
