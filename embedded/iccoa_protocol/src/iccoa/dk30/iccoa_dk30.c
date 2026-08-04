/**
 * @file iccoa_dk30.c
 * @brief ICCOA DK 3.0 Protocol Implementation
 *
 * @fix [V-02] Stack buffer overflow, OOB read, unchecked enum cast
 *   - send_response: reject payload > ICCOA_MAX_PAYLOAD before memcpy
 *   - process: validate payload_len against real buffer size; use wire offsets
 *              instead of struct cast to avoid OOB checksum read
 *   - ctrl_request: validate cmd enum range before dispatch
 *   - checksum: guard against NULL data + len > 0
 */

#include "iccoa_digital_key.h"

static uint16_t g_seq_num = 0;
static uint16_t g_last_seq_num = 0;
static iccoa_ble_recv_cb_t g_recv_cb = NULL;

/* Header size before payload: SOP(1) + CMD_ID(1) + SEQ_NUM(2) + PAYLOAD_LEN(2) = 6 */
#define DK30_HEADER_SIZE  6

/* -----------------------------------------------------------------------
 *  Checksum — prevent OOB read on NULL + len > 0
 * ----------------------------------------------------------------------- */
uint8_t iccoa_dk30_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t cs = 0;
    if (!data || len == 0) {
        return cs;
    }
    for (uint16_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

/* -----------------------------------------------------------------------
 *  Init
 * ----------------------------------------------------------------------- */
int32_t iccoa_dk30_init(void)
{
    g_seq_num = 0;
    g_last_seq_num = 0;
    return ICCOA_OK;
}

/* -----------------------------------------------------------------------
 *  Handler stubs
 * ----------------------------------------------------------------------- */
static int32_t handle_bind_request(const uint8_t *payload, uint16_t len)
{
    (void)len;
    /* Parse phone public key from payload */
    /* Generate vehicle key pair via SE050 */
    /* Build bind response with vehicle public key */
    static uint8_t rsp_payload[128];
    (void)memset(rsp_payload, 0, sizeof(rsp_payload));
    /* TODO: Fill with vehicle public key + signature */
    return iccoa_dk30_send_response(ICCOA_CMD_BIND_RSP, rsp_payload, 64);
}

static int32_t handle_auth_request(const uint8_t *payload, uint16_t len)
{
    (void)len;
    /* Parse auth data */
    /* Verify signature */
    int32_t auth_ret = iccoa_auth_verify(payload, len);
    uint8_t rsp[2] = { (auth_ret == ICCOA_OK) ? 0x00 : 0x01, 0x00 };
    return iccoa_dk30_send_response(ICCOA_CMD_AUTH_RSP, rsp, 2);
}

/* -----------------------------------------------------------------------
 *  Control-request dispatch — [V-02] validate cmd enum before use
 * ----------------------------------------------------------------------- */
static int32_t handle_ctrl_request(const uint8_t *payload, uint16_t len)
{
    if (!payload || len < 2) return ICCOA_ERR_PARAM;

    /* [V-02 fix] validate that cmd is a known enum value */
    uint8_t raw_cmd = payload[0];
    if (raw_cmd < CTRL_LOCK || raw_cmd > CTRL_HORN) {
        return ICCOA_ERR_PARAM;
    }

    iccoa_ctrl_cmd_e cmd = (iccoa_ctrl_cmd_e)raw_cmd;
    uint8_t param = payload[1];

    int32_t ret = iccoa_ctrl_execute(cmd, param);
    uint8_t rsp[2] = { (ret == ICCOA_OK) ? 0x00 : 0x01, 0x00 };
    return iccoa_dk30_send_response(ICCOA_CMD_CTRL_RSP, rsp, 2);
}

/* -----------------------------------------------------------------------
 *  Process incoming DK3.0 frame — [V-02] validate payload_len against buffer
 * ----------------------------------------------------------------------- */
int32_t iccoa_dk30_process(const uint8_t *raw, uint16_t len)
{
    if (!raw || len < DK30_HEADER_SIZE + 1) return ICCOA_ERR_PARAM;
    /* Minimum viable: header(6) + 1-byte checksum + EOP(1) = 8 */

    if (raw[0] != DK30_SOP) return ICCOA_ERR_PARAM;

    /* [V-02 fix] Read header fields manually — do NOT cast into the
     * full-size struct (sizeof = 251), which would place checksum at
     * compile-time offset (250) instead of the real wire offset. */
    uint8_t  cmd_id      = raw[1];
    uint16_t seq_num     = (uint16_t)(raw[2]) | ((uint16_t)(raw[3]) << 8);
    uint16_t payload_len = (uint16_t)(raw[4]) | ((uint16_t)(raw[5]) << 8);

    /* [V-02 fix] Verify payload_len fits in remaining buffer:
     *   DK30_HEADER_SIZE + payload_len + checksum(1) + EOP(1) <= len
     * → payload_len <= len - DK30_HEADER_SIZE - 2 */
    if (payload_len > len - DK30_HEADER_SIZE - 2) {
        return ICCOA_ERR_PARAM;
    }

    /* Checksum covers: cmd_id + seq_num + payload_len + payload body */
    uint16_t cs_len = sizeof(cmd_id) + sizeof(seq_num) + sizeof(payload_len) + payload_len;
    uint8_t cs = iccoa_dk30_checksum(raw + 1, cs_len);

    /* [V-02 fix] Read checksum and EOP by wire offset */
    uint8_t wire_checksum = raw[DK30_HEADER_SIZE + payload_len];
    uint8_t wire_eop      = raw[DK30_HEADER_SIZE + payload_len + 1];

    if (cs != wire_checksum) return ICCOA_ERR_SECURITY;
    if (wire_eop != DK30_EOP) return ICCOA_ERR_PARAM;

    /* [V-20 fix] anti-replay: seq_num must monotonically increase */
    /* Wrap-around (seq rolls over from 0xFFFF → 0) is allowed */
    if (seq_num == 0 && g_last_seq_num == 0) {
        /* First frame ever — accept seq 0 */
    } else if (seq_num == 0 && g_last_seq_num == 0xFFFF) {
        /* [CR-1 fix] Valid wrap from 0xFFFF to 0 — accept */
    } else if (seq_num <= g_last_seq_num) {
        /* seq not greater than last */
        return ICCOA_ERR_SECURITY;
    }
    g_last_seq_num = seq_num;

    /* Safely point into the raw buffer for the payload portion */
    const uint8_t *payload = raw + DK30_HEADER_SIZE;

    /* Dispatch by command */
    switch (cmd_id) {
    case ICCOA_CMD_BIND_REQ:
        return handle_bind_request(payload, payload_len);
    case ICCOA_CMD_UNBIND_REQ:
        /* TODO: Handle unbind */
        break;
    case ICCOA_CMD_AUTH_REQ:
        return handle_auth_request(payload, payload_len);
    case ICCOA_CMD_CTRL_REQ:
        return handle_ctrl_request(payload, payload_len);
    case ICCOA_CMD_KEY_SHARE:
        /* TODO: Handle key share */
        break;
    default:
        break;
    }

    return ICCOA_OK;
}

/* -----------------------------------------------------------------------
 *  Send DK3.0 response — [V-02] guard memcpy size
 * ----------------------------------------------------------------------- */
int32_t iccoa_dk30_send_response(iccoa_cmd_e cmd, const uint8_t *payload, uint16_t len)
{
    /* [V-02 fix] reject oversized payload before memcpy */
    if (len > ICCOA_MAX_PAYLOAD) {
        return ICCOA_ERR_PARAM;
    }

    iccoa_dk30_frame_t frame = {0};
    frame.sop        = DK30_SOP;
    frame.cmd_id     = (uint8_t)cmd;
    frame.seq_num    = g_seq_num++;
    frame.payload_len = len;

    if (len > 0 && payload) {
        (void)memcpy(frame.payload, payload, len);
    }

    frame.checksum = iccoa_dk30_checksum((const uint8_t *)&frame + 1, 4 + len);
    frame.eop      = DK30_EOP;

    return iccoa_ble_send((const uint8_t *)&frame, 6 + len);
}
