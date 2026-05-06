/**
 * @file icce_uwb.c
 * @brief ICCE UWB Module - NXP NCJ29D6 FiRa Ranging (shared with CCC)
 */

#include "icce_digital_key.h"

#define NCJ29D6_SPI_CMD_READ    0x01
#define NCJ29D6_SPI_CMD_WRITE   0x02
#define NCJ29D6_IRQ_PIN         GPIO_PIN_4

static icce_uwb_session_t g_sessions[ICCE_UWB_MAX_RANGING_SESSIONS];
static icce_uwb_ranging_cb_t g_ranging_cb = NULL;
static uint8_t g_session_count = 0;

int32_t icce_uwb_init(void)
{
    /* NCJ29D6 SPI3 init @ 16MHz */
    /* Load FiRa MAC config */
    /* Set default channel 9, preamble code 11 */
    memset(g_sessions, 0, sizeof(g_sessions));
    g_ranging_cb = NULL;
    g_session_count = 0;
    return ICCE_OK;
}

int32_t icce_uwb_deinit(void)
{
    /* Stop all active sessions */
    for (uint8_t i = 0; i < g_session_count; i++) {
        if (g_sessions[i].session_id != 0) {
            icce_uwb_stop_session(g_sessions[i].session_id);
        }
    }
    g_session_count = 0;
    return ICCE_OK;
}

static int32_t find_session(uint16_t session_id)
{
    for (uint8_t i = 0; i < g_session_count; i++) {
        if (g_sessions[i].session_id == session_id) return i;
    }
    return ICCE_ERR_NOT_FOUND;
}

int32_t icce_uwb_start_session(uint16_t session_id, icce_uwb_role_e role, uint8_t channel)
{
    if (g_session_count >= ICCE_UWB_MAX_RANGING_SESSIONS) return ICCE_ERR_BUSY;
    if (channel != 5 && channel != 6 && channel != 8 && channel != 9) return ICCE_ERR_PARAM;

    int32_t idx = g_session_count;
    g_sessions[idx].session_id = session_id;
    g_sessions[idx].channel = channel;
    g_sessions[idx].role = (uint8_t)role;
    g_sessions[idx].mac_mode = 1;  /* SP3 by default */
    g_sessions[idx].distance_mm = 0;
    g_sessions[idx].quality = 0;

    /* Configure NCJ29D6 for this session */
    /* Send FiRa session config via SPI */

    g_session_count++;
    return ICCE_OK;
}

int32_t icce_uwb_stop_session(uint16_t session_id)
{
    int32_t idx = find_session(session_id);
    if (idx < 0) return ICCE_ERR_NOT_FOUND;

    /* De-session NCJ29D6 */
    memset(&g_sessions[idx], 0, sizeof(icce_uwb_session_t));

    /* Compact array */
    for (uint8_t i = idx; i < g_session_count - 1; i++) {
        g_sessions[i] = g_sessions[i + 1];
    }
    g_session_count--;
    return ICCE_OK;
}

int32_t icce_uwb_get_ranging(uint16_t session_id, icce_uwb_session_t *out)
{
    int32_t idx = find_session(session_id);
    if (idx < 0) return ICCE_ERR_NOT_FOUND;
    if (!out) return ICCE_ERR_PARAM;

    memcpy(out, &g_sessions[idx], sizeof(icce_uwb_session_t));
    return ICCE_OK;
}

int32_t icce_uwb_register_cb(icce_uwb_ranging_cb_t cb)
{
    if (!cb) return ICCE_ERR_PARAM;
    g_ranging_cb = cb;
    return ICCE_OK;
}

/* Called from SPI IRQ when ranging data is ready */
void icce_uwb_irq_handler(void)
{
    /* Read ranging result from NCJ29D6 */
    /* Update g_sessions[] with new distance/angle */
    /* Call g_ranging_cb if registered */
    if (g_ranging_cb && g_session_count > 0) {
        /* TODO: parse ranging data, update session */
        /* g_ranging_cb(&g_sessions[0]); */
    }
}
