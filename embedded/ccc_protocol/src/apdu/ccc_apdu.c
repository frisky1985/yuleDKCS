/**
 * @file ccc_apdu.c
 * @module EMB-SWC-CCC
 * @brief CCC Digital Key v4.0 APDU 分发器 (ISO 7816-4 子集)
 *
 * 见 ccc_apdu.h 的分层说明. 本文件不感知 BLE 帧/连接句柄, 只处理
 * APDU 字节与状态字; 密码学操作委托给 security.c.
 */

#include "ccc_apdu.h"
#include "crypto_random.h"

#include <string.h>

/* CCC Digital Key AID: "CCCDKAv1" */
const uint8_t CCC_APDU_DK_AID[CCC_APDU_AID_LEN] = {
    0xA0, 0x00, 0x00, 0x08, 0x09, 0x43, 0x43,
    0x43, 0x44, 0x4B, 0x41, 0x76, 0x31
};

/* ------------------------------------------------------------------------
 *  会话状态
 * ------------------------------------------------------------------------ */
static bool     s_selected;
static bool     s_challenge_valid;
static bool     s_authenticated;
static uint8_t  s_challenge[CCC_APDU_CHALLENGE_LEN];

/* 签名拷贝: 回调消费者 (core) 会延迟处理, 不能引用 BLE 接收缓冲区 */
static uint8_t  s_sig_copy[CCC_APDU_MAX_DATA];
static uint16_t s_sig_copy_len;

static ccc_apdu_auth_cb_t s_auth_cb;
static ccc_apdu_cmd_cb_t  s_cmd_cb;

/* ------------------------------------------------------------------------
 *  内部辅助
 * ------------------------------------------------------------------------ */
static uint16_t sw_only(uint8_t *resp, uint16_t cap, uint16_t *resp_len, uint16_t sw)
{
    if (resp_len) *resp_len = 0;
    if (!resp || !resp_len || cap < 2U) return sw;
    resp[0] = (uint8_t)(sw >> 8);
    resp[1] = (uint8_t)(sw & 0xFFU);
    *resp_len = 2U;
    return sw;
}

static void clear_challenge(void)
{
    (void)memset(s_challenge, 0, sizeof(s_challenge));
    s_challenge_valid = false;
}

static bool cla_supported(uint8_t cla)
{
    return cla == CCC_APDU_CLA_ISO ||
           cla == CCC_APDU_CLA_PROPRIETARY ||
           cla == CCC_APDU_CLA_SECURE;
}

/* ------------------------------------------------------------------------
 *  解析
 * ------------------------------------------------------------------------ */
ccc_status_t ccc_apdu_parse(const uint8_t *buf, uint16_t len, ccc_apdu_t *out)
{
    if (buf == NULL || out == NULL || len < 4U) return CCC_ERR_PARAM;

    out->cla    = buf[0];
    out->ins    = buf[1];
    out->p1     = buf[2];
    out->p2     = buf[3];
    out->lc     = 0U;
    out->data   = NULL;
    out->le     = 0U;
    out->has_le = false;

    if (len == 4U) return CCC_OK;   /* case 1 */

    uint8_t b4 = buf[4];

    if (len == 5U) {                /* case 2: 第 5 字节为 Le */
        out->has_le = true;
        out->le     = (b4 == 0x00U) ? 256U : (uint16_t)b4;
        return CCC_OK;
    }

    /* case 3/4: b4 = Lc (短 APDU, 扩展长度 0x87/0x88 不支持) */
    if (b4 == 0x00U) return CCC_ERR_PARAM;
    uint16_t lc = (uint16_t)b4;
    if (lc > CCC_APDU_MAX_DATA) return CCC_ERR_PARAM;
    if ((uint16_t)(len - 5U) < lc) return CCC_ERR_PARAM;

    out->lc   = lc;
    out->data = buf + 5;

    uint16_t rest = (uint16_t)(len - 5U - lc);
    if (rest > 1U) return CCC_ERR_PARAM;   /* 粘包 */
    if (rest == 1U) {
        out->has_le = true;
        out->le     = (buf[5U + lc] == 0x00U) ? 256U : (uint16_t)buf[5U + lc];
    }
    return CCC_OK;
}

/* ------------------------------------------------------------------------
 *  命令处理
 * ------------------------------------------------------------------------ */
static uint16_t do_select(const ccc_apdu_t *a,
                          uint8_t *resp, uint16_t cap, uint16_t *resp_len)
{
    if (a->cla != CCC_APDU_CLA_ISO) {
        return sw_only(resp, cap, resp_len, CCC_SW_CLA_NOT_SUPPORTED);
    }
    if (a->p1 != 0x04U || a->p2 != 0x00U) {
        return sw_only(resp, cap, resp_len, CCC_SW_WRONG_P1P2);
    }
    if (a->lc == 0U || a->data == NULL) {
        return sw_only(resp, cap, resp_len, CCC_SW_WRONG_LENGTH);
    }
    if (a->lc != CCC_APDU_AID_LEN ||
        memcmp(a->data, CCC_APDU_DK_AID, CCC_APDU_AID_LEN) != 0) {
        return sw_only(resp, cap, resp_len, CCC_SW_FILE_NOT_FOUND);
    }

    /* 重新 SELECT 即开启新会话 */
    s_selected      = true;
    s_authenticated = false;
    clear_challenge();
    return sw_only(resp, cap, resp_len, CCC_SW_OK);
}

static uint16_t do_get_challenge(const ccc_apdu_t *a,
                                 uint8_t *resp, uint16_t cap, uint16_t *resp_len)
{
    if (a->cla != CCC_APDU_CLA_ISO) {
        return sw_only(resp, cap, resp_len, CCC_SW_CLA_NOT_SUPPORTED);
    }
    if (!s_selected) {
        return sw_only(resp, cap, resp_len, CCC_SW_CONDITIONS_NOT_SAT);
    }
    if (a->lc != 0U) {
        return sw_only(resp, cap, resp_len, CCC_SW_WRONG_LENGTH);
    }
    if (cap < (uint16_t)(CCC_APDU_CHALLENGE_LEN + 2U)) {
        return sw_only(resp, cap, resp_len, CCC_SW_WRONG_LENGTH);
    }
    if (crypto_random_bytes(s_challenge, CCC_APDU_CHALLENGE_LEN) != CRYPTO_RANDOM_OK) {
        clear_challenge();
        return sw_only(resp, cap, resp_len, CCC_SW_SECURITY_NOT_SAT);
    }

    s_challenge_valid = true;
    (void)memcpy(resp, s_challenge, CCC_APDU_CHALLENGE_LEN);
    resp[CCC_APDU_CHALLENGE_LEN]     = (uint8_t)(CCC_SW_OK >> 8);
    resp[CCC_APDU_CHALLENGE_LEN + 1] = (uint8_t)(CCC_SW_OK & 0xFFU);
    *resp_len = (uint16_t)(CCC_APDU_CHALLENGE_LEN + 2U);
    return CCC_SW_OK;
}

static uint16_t do_internal_auth(const ccc_apdu_t *a,
                                 uint8_t *resp, uint16_t cap, uint16_t *resp_len)
{
    if (a->cla != CCC_APDU_CLA_ISO) {
        return sw_only(resp, cap, resp_len, CCC_SW_CLA_NOT_SUPPORTED);
    }
    if (!s_selected || !s_challenge_valid) {
        return sw_only(resp, cap, resp_len, CCC_SW_CONDITIONS_NOT_SAT);
    }
    if (a->lc == 0U || a->data == NULL) {
        return sw_only(resp, cap, resp_len, CCC_SW_WRONG_LENGTH);
    }
    if (!sec_peer_public_key_provisioned()) {
        clear_challenge();
        return sw_only(resp, cap, resp_len, CCC_SW_CONDITIONS_NOT_SAT);
    }

    /* 先拷贝签名: 回调消费者在下一个 tick 才处理, 原缓冲区可能已失效 */
    (void)memcpy(s_sig_copy, a->data, a->lc);
    s_sig_copy_len = a->lc;

    verify_result_e vr = sec_verify(s_challenge, CCC_APDU_CHALLENGE_LEN,
                                    s_sig_copy, s_sig_copy_len);

    /* 一次性挑战: 无论成败都不可复用 */
    s_challenge_valid = false;

    if (vr != VERIFY_OK) {
        s_authenticated = false;
        return sw_only(resp, cap, resp_len, CCC_SW_SECURITY_NOT_SAT);
    }

    s_authenticated = true;
    if (s_auth_cb != NULL) {
        s_auth_cb(s_challenge, CCC_APDU_CHALLENGE_LEN, s_sig_copy, s_sig_copy_len);
    }
    /* 数据域留空: 认证结果由后续车辆指令体现 */
    return sw_only(resp, cap, resp_len, CCC_SW_OK);
}

static uint16_t do_vehicle_cmd(const ccc_apdu_t *a,
                               uint8_t *resp, uint16_t cap, uint16_t *resp_len)
{
    if (a->cla != CCC_APDU_CLA_PROPRIETARY && a->cla != CCC_APDU_CLA_SECURE) {
        return sw_only(resp, cap, resp_len, CCC_SW_CLA_NOT_SUPPORTED);
    }
    /* 未注册执行器 = 本实现不支持该 INS.
     * 先于认证判断返回 6D00: 避免因会话状态不同而产生行为差异, 也不泄露认证态. */
    if (s_cmd_cb == NULL) {
        return sw_only(resp, cap, resp_len, CCC_SW_INS_NOT_SUPPORTED);
    }
    if (!s_authenticated) {
        return sw_only(resp, cap, resp_len, CCC_SW_CONDITIONS_NOT_SAT);
    }

    ccc_status_t st = s_cmd_cb(a->p1, a->data, a->lc);
    if (st == CCC_OK) {
        return sw_only(resp, cap, resp_len, CCC_SW_OK);
    }
    if (st == CCC_ERR_SECURITY) {
        return sw_only(resp, cap, resp_len, CCC_SW_SECURITY_NOT_SAT);
    }
    return sw_only(resp, cap, resp_len, CCC_SW_CONDITIONS_NOT_SAT);
}

/* ------------------------------------------------------------------------
 *  公共 API
 * ------------------------------------------------------------------------ */
uint16_t ccc_apdu_process(const uint8_t *req, uint16_t req_len,
                          uint8_t *resp, uint16_t resp_cap,
                          uint16_t *resp_len)
{
    ccc_apdu_t a;

    if (resp_len != NULL) *resp_len = 0U;
    if (req == NULL || resp == NULL || resp_len == NULL) {
        return CCC_SW_WRONG_LENGTH;
    }

    if (ccc_apdu_parse(req, req_len, &a) != CCC_OK) {
        return sw_only(resp, resp_cap, resp_len, CCC_SW_WRONG_LENGTH);
    }
    if (!cla_supported(a.cla)) {
        return sw_only(resp, resp_cap, resp_len, CCC_SW_CLA_NOT_SUPPORTED);
    }

    switch (a.ins) {
    case CCC_APDU_INS_SELECT:
        return do_select(&a, resp, resp_cap, resp_len);
    case CCC_APDU_INS_GET_CHALLENGE:
        return do_get_challenge(&a, resp, resp_cap, resp_len);
    case CCC_APDU_INS_INTERNAL_AUTH:
        return do_internal_auth(&a, resp, resp_cap, resp_len);
    case CCC_APDU_INS_VEHICLE_CMD_NFC_PRIVATE:
        return do_vehicle_cmd(&a, resp, resp_cap, resp_len);
    default:
        return sw_only(resp, resp_cap, resp_len, CCC_SW_INS_NOT_SUPPORTED);
    }
}

void ccc_apdu_reset(void)
{
    s_selected      = false;
    s_authenticated = false;
    clear_challenge();
    (void)memset(s_sig_copy, 0, sizeof(s_sig_copy));
    s_sig_copy_len = 0U;
}

bool ccc_apdu_is_selected(void)      { return s_selected; }
bool ccc_apdu_is_authenticated(void) { return s_authenticated; }

ccc_status_t ccc_apdu_register_auth_cb(ccc_apdu_auth_cb_t cb)
{
    s_auth_cb = cb;
    return CCC_OK;
}

ccc_status_t ccc_apdu_register_cmd_cb(ccc_apdu_cmd_cb_t cb)
{
    s_cmd_cb = cb;
    return CCC_OK;
}
