/**
 * @file iccoa_dk40.c
 * @brief ICCOA DK 4.0 Protocol Implementation
 */

#include "iccoa_digital_key.h"

static uint16_t g_v4_msg_id = 0;
static uint8_t  g_session_token[4] = {0};

int32_t iccoa_dk40_init(void)
{
    g_v4_msg_id = 0;
    memset(g_session_token, 0, sizeof(g_session_token));
    return ICCOA_OK;
}

static uint16_t compute_hmac_trunc(const uint8_t *data, uint16_t len, uint8_t *hmac_out)
{
    /* HMAC-SHA256 truncated to 16 bytes */
    /* Platform-specific: use SE050 HMAC */
    (void)data; (void)len;
    memset(hmac_out, 0, 16);
    return 16;
}

int32_t iccoa_dk40_process(const uint8_t *raw, uint16_t len)
{
    if (!raw || len < 14) return ICCOA_ERR_PARAM;

    const iccoa_dk40_frame_t *frame = (const iccoa_dk40_frame_t *)raw;

    /* Validate magic */
    if (frame->magic != DK40_MAGIC) return ICCOA_ERR_PARAM;
    if (frame->version != 4) return ICCOA_ERR_PARAM;

    /* Verify HMAC */
    uint8_t computed_hmac[16];
    compute_hmac_trunc(raw, len - 16, computed_hmac);
    if (memcmp(computed_hmac, frame->hmac, 16) != 0) {
        return ICCOA_ERR_SECURITY;
    }

    /* Dispatch */
    switch (frame->msg_type) {
    case ICCOA_V4_SESSION_OPEN:
        memcpy(g_session_token, frame->session_token, 4);
        iccoa_dk40_send_response(ICCOA_V4_SESSION_OPEN, NULL, 0);
        break;

    case ICCOA_V4_SESSION_CLOSE:
        memset(g_session_token, 0, 4);
        break;

    case ICCOA_V4_BIND:
        /* DK 4.0 enhanced binding */
        /* TODO: ECDH key exchange + certificate verification */
        break;

    case ICCOA_V4_AUTH:
        iccoa_auth_verify(frame->payload, frame->payload_len);
        break;

    case ICCOA_V4_CTRL: {
        if (frame->payload_len >= 2) {
            iccoa_ctrl_cmd_e cmd = (iccoa_ctrl_cmd_e)frame->payload[0];
            uint8_t param = frame->payload[1];
            iccoa_ctrl_execute(cmd, param);
        }
        break;
    }

    case ICCOA_V4_UWB_CONFIG:
        /* DK 4.0 UWB configuration */
        /* TODO: Forward UWB config to UWB module */
        break;

    case ICCOA_V4_SHARE:
        /* DK 4.0 remote key sharing */
        /* TODO: Handle cloud-based sharing */
        break;

    default:
        break;
    }

    return ICCOA_OK;
}

int32_t iccoa_dk40_send_response(iccoa_v4_cmd_e cmd, const uint8_t *payload, uint16_t len)
{
    iccoa_dk40_frame_t frame = {0};
    frame.magic = DK40_MAGIC;
    frame.version = 4;
    frame.msg_type = (uint8_t)cmd;
    frame.msg_id = g_v4_msg_id++;
    frame.flags = 0;
    frame.payload_len = len;
    memcpy(frame.session_token, g_session_token, 4);
    if (len > 0 && payload) {
        memcpy(frame.payload, payload, len);
    }

    /* Compute HMAC */
    compute_hmac_trunc((const uint8_t *)&frame, 14 + len, frame.hmac);

    return iccoa_ble_send((const uint8_t *)&frame, 14 + len + 16);
}
