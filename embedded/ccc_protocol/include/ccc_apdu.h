/**
 * @file ccc_apdu.h
 * @module EMB-SWC-CCC (ASPICE SWE.2)
 * @brief CCC Digital Key v4.0 — ISO 7816-4 APDU 分发契约
 *
 * 分层 (单一职责):
 *   BLE/NFC 传输  →  4B v4.0 帧 (ccc_frame_parse)  →  本模块 (APDU 路由)
 *                                                        ↓
 *                                                security.c / SE050 (密码学服务)
 *
 * 本模块只负责:
 *   - ISO 7816-4 命令结构解析 (CLA/INS/P1/P2/Lc/Data/Le)
 *   - AID 选择与会话状态 (selected / challenged / authenticated)
 *   - 状态字 (SW1SW2) 返回, 未知命令一律拒绝
 * 不负责: 帧头、连接句柄、密钥存储、验签算法.
 *
 * 规范: CCC-TS-101 v4.0.0 §19.3.2 / ISO/IEC 7816-4
 * 参考: docs/certification/ccc-ts101-ble-secure-channel.md §5
 */

#ifndef CCC_APDU_H
#define CCC_APDU_H

#include <stdint.h>
#include <stdbool.h>

#include "ccc_digital_key.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 *  状态字 (ISO 7816-4)
 * ======================================================================== */
#define CCC_SW_OK                     0x9000u  /**< 命令成功 */
#define CCC_SW_WRONG_LENGTH           0x6700u  /**< Lc/Le 或结构长度错误 */
#define CCC_SW_SECURITY_NOT_SAT       0x6982u  /**< 安全状态不满足 (验签失败) */
#define CCC_SW_CONDITIONS_NOT_SAT     0x6985u  /**< 前置条件不满足 (未选 AID/无挑战) */
#define CCC_SW_WRONG_DATA             0x6A80u  /**< 数据域内容错误 */
#define CCC_SW_FILE_NOT_FOUND         0x6A82u  /**< AID 不匹配 */
#define CCC_SW_WRONG_P1P2             0x6A86u  /**< P1/P2 错误 */
#define CCC_SW_INS_NOT_SUPPORTED      0x6D00u  /**< INS 不支持 */
#define CCC_SW_CLA_NOT_SUPPORTED      0x6E00u  /**< CLA 不支持 */

/* ========================================================================
 *  AID / 类别 / 指令
 * ======================================================================== */

/** CCC Digital Key AID: A0 00 00 08 09 43 43 43 44 4B 41 76 31 ("CCCDKAv1") */
#define CCC_APDU_AID_LEN   13u
extern const uint8_t CCC_APDU_DK_AID[CCC_APDU_AID_LEN];

/** 允许的 CLA (规范 §5: 00=SELECT, 80=非安全, 84=安全消息) */
#define CCC_APDU_CLA_ISO          0x00u
#define CCC_APDU_CLA_PROPRIETARY  0x80u
#define CCC_APDU_CLA_SECURE       0x84u

/** INS: 标准 ISO 7816-4 */
#define CCC_APDU_INS_SELECT         0xA4u  /**< SELECT (P1=0x04 按 AID) */
#define CCC_APDU_INS_GET_CHALLENGE  0x84u  /**< GET CHALLENGE */
#define CCC_APDU_INS_INTERNAL_AUTH  0x88u  /**< INTERNAL AUTHENTICATE */
/** INS: 本项目车辆指令 (CLA=0x80, 载荷见 docs/sdk/NFC-INTEGRATION.md) */
#define CCC_APDU_INS_VEHICLE_CMD    0xD2u

/** 单帧 APDU 数据域上限 (BLE_MAX_PAYLOAD - 4B 帧头 - 5B APDU 开销) */
#define CCC_APDU_MAX_DATA  235u

/** GET CHALLENGE 返回的挑战长度 */
#define CCC_APDU_CHALLENGE_LEN  8u

/* ========================================================================
 *  APDU 结构
 * ======================================================================== */
typedef struct {
    uint8_t        cla;
    uint8_t        ins;
    uint8_t        p1;
    uint8_t        p2;
    uint16_t       lc;        /**< 数据域长度 (0 表示无数据) */
    const uint8_t *data;      /**< 指向输入缓冲区, 不拷贝; lc==0 时为 NULL */
    uint16_t       le;        /**< 期望响应长度; has_le==false 时无意义 */
    bool           has_le;
} ccc_apdu_t;

typedef void (*ccc_apdu_auth_cb_t)(const uint8_t *challenge, uint16_t challenge_len,
                                   const uint8_t *sig, uint16_t sig_len);

typedef ccc_status_t (*ccc_apdu_cmd_cb_t)(uint8_t subcmd,
                                          const uint8_t *data, uint16_t len);

/* ========================================================================
 *  API
 * ======================================================================== */

/**
 * 解析短 APDU (ISO 7816-4 case 1/2/3/4).
 * 拒绝: 不足 4 字节、扩展长度 (0x87/0x88)、Lc 超限、截断/粘包.
 * @return CCC_OK 或 CCC_ERR_INVALID_PARAM
 */
ccc_status_t ccc_apdu_parse(const uint8_t *buf, uint16_t len, ccc_apdu_t *out);

/**
 * 处理一条 APDU 命令.
 *
 * 响应写入 resp: 数据域后紧跟 SW1 SW2; *resp_len 为总长度 (含 2B SW).
 * 返回值同时以 uint16_t 形式给出状态字, 便于调用方判断.
 *
 * fail-closed: 任何解析/权限/验签失败都必须返回非 0x9000 且不改动认证状态.
 */
uint16_t ccc_apdu_process(const uint8_t *req, uint16_t req_len,
                          uint8_t *resp, uint16_t resp_cap,
                          uint16_t *resp_len);

/** 复位会话状态 (断开连接 / 重新 SELECT 时调用) */
void ccc_apdu_reset(void);

bool ccc_apdu_is_selected(void);
bool ccc_apdu_is_authenticated(void);

/** 注册认证成功回调 (由 core 注入, 用于触发车辆认证流程) */
ccc_status_t ccc_apdu_register_auth_cb(ccc_apdu_auth_cb_t cb);

/** 注册车辆指令回调 (由 core 注入; 未注册时指令返回 0x6D00) */
ccc_status_t ccc_apdu_register_cmd_cb(ccc_apdu_cmd_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* CCC_APDU_H */
