/**
 * @file iccoa_dk30.c
 * @brief ICCOA DK 3.0 Protocol Implementation
 */

#include "iccoa_digital_key.h"

static uint16_t g_seq_num = 0;
static iccoa_ble_recv_cb_t g_recv_cb = NULL;

uint8_t iccoa_dk30_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t cs = 0;
    for (uint16_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

int32_t iccoa_dk30_init(void)
{
    g_seq_num = 0;
    return ICCOA_OK;
}

static int32_t handle_bind_request(const uint8_t *payload, uint16_t len)
{
    (void)len;
    /* Parse phone public key from payload */
    /* Generate vehicle key pair via SE050 */
    /* Build bind response with vehicle public key */
    uint8_t rsp_payload[128] = {0};
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

static int32_t handle_ctrl_request(const uint8_t *payload, uint16_t len)
{
    if (!payload || len < 2) return ICCOA_ERR_PARAM;

    /* Check permission first */
    iccoa_ctrl_cmd_e cmd = (iccoa_ctrl_cmd_e)payload[0];
    uint8_t param = payload[1];

    int32_t ret = iccoa_ctrl_execute(cmd, param);
    uint8_t rsp[2] = { (ret == ICCOA_OK) ? 0x00 : 0x01, 0x00 };
    return iccoa_dk30_send_response(ICCOA_CMD_CTRL_RSP, rsp, 2);
}

int32_t iccoa_dk30_process(const uint8_t *raw, uint16_t len)
{
    if (!raw || len < 6) return ICCOA_ERR_PARAM;

    /* Validate SOP/EOP */
    if (raw[0] != DK30_SOP) return ICCOA_ERR_PARAM;

    const iccoa_dk30_frame_t *frame = (const iccoa_dk30_frame_t *)raw;
    uint16_t payload_len = frame->payload_len;

    /* Validate checksum */
    uint8_t cs = iccoa_dk30_checksum(raw + 1, 4 + payload_len);
    if (cs != frame->checksum) return ICCOA_ERR_SECURITY;

    /* Dispatch by command */
    switch (frame->cmd_id) {
    case ICCOA_CMD_BIND_REQ:
        return handle_bind_request(frame->payload, payload_len);
    case ICCOA_CMD_UNBIND_REQ:
        /* TODO: Handle unbind */
        break;
    case ICCOA_CMD_AUTH_REQ:
        return handle_auth_request(frame->payload, payload_len);
    case ICCOA_CMD_CTRL_REQ:
        return handle_ctrl_request(frame->payload, payload_len);
    case ICCOA_CMD_KEY_SHARE:
        /* TODO: Handle key share */
        break;
    default:
        break;
    }

    return ICCOA_OK;
}

int32_t iccoa_dk30_send_response(iccoa_cmd_e cmd, const uint8_t *payload, uint16_t len)
{
    iccoa_dk30_frame_t frame = {0};
    frame.sop = DK30_SOP;
    frame.cmd_id = (uint8_t)cmd;
    frame.seq_num = g_seq_num++;
    frame.payload_len = len;
    if (len > 0 && payload) {
        memcpy(frame.payload, payload, len);
    }
    frame.checksum = iccoa_dk30_checksum((const uint8_t *)&frame + 1, 4 + len);
    frame.eop = DK30_EOP;

    return iccoa_ble_send((const uint8_t *)&frame, 6 + len);
}
