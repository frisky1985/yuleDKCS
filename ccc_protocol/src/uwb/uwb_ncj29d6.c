/**
 * @file uwb_ncj29d6.c
 * @brief UWB Driver for NXP NCJ29D6
 */

#include "ccc_digital_key.h"

/* NCJ29D6 SPI Commands */
#define NCJ29D6_CMD_INIT            0x01
#define NCJ29D6_CMD_CREATE_SESSION  0x10
#define NCJ29D6_CMD_DESTROY_SESSION 0x11
#define NCJ29D6_CMD_START_RANGING   0x20
#define NCJ29D6_CMD_STOP_RANGING    0x21
#define NCJ29D6_CMD_GET_DISTANCE    0x30
#define NCJ29D6_EVT_RANGING_RESULT  0xB0

/* Session table */
typedef struct {
    uint32_t id;
    bool     active;
    bool     ranging;
    uint16_t last_distance_cm;
    distance_zone_e last_zone;
} uwb_session_entry_t;

static uwb_session_entry_t g_sessions[UWB_MAX_SESSIONS];
static distance_threshold_t g_threshold;
static uwb_zone_cb_t g_zone_cb = NULL;

extern int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void    gpio_write(uint8_t port, uint8_t pin, uint8_t val);

static ccc_status_t ncj29d6_send_cmd(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint8_t header[4] = { cmd, 0x00, (uint8_t)(len >> 8), (uint8_t)(len & 0xFF) };

    gpio_write(NCJ29D6_CS_PORT, NCJ29D6_CS_PIN, 0);
    spi_transfer(3, header, NULL, 4);
    if (len > 0 && payload) {
        spi_transfer(3, payload, NULL, len);
    }
    gpio_write(NCJ29D6_CS_PORT, NCJ29D6_CS_PIN, 1);

    return CCC_OK;
}

ccc_status_t uwb_ncj29d6_init(void)
{
    gpio_write(NCJ29D6_RST_PORT, NCJ29D6_RST_PIN, 0);
    gpio_write(NCJ29D6_RST_PORT, NCJ29D6_RST_PIN, 1);

    ncj29d6_send_cmd(NCJ29D6_CMD_INIT, NULL, 0);

    memset(g_sessions, 0, sizeof(g_sessions));
    return CCC_OK;
}

ccc_status_t uwb_ncj29d6_deinit(void)
{
    for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
        if (g_sessions[i].active) {
            uwb_stop_ranging(g_sessions[i].id);
            uwb_destroy_session(g_sessions[i].id);
        }
    }
    return CCC_OK;
}

uint32_t uwb_create_session(const uwb_session_config_t *cfg)
{
    if (!cfg) return UWB_INVALID_SESSION;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
        if (!g_sessions[i].active) { slot = i; break; }
    }
    if (slot < 0) return UWB_INVALID_SESSION;

    /* Send create session command to NCJ29D6 */
    ccc_status_t ret = ncj29d6_send_cmd(NCJ29D6_CMD_CREATE_SESSION,
                                          (const uint8_t *)cfg, sizeof(*cfg));
    if (ret != CCC_OK) return UWB_INVALID_SESSION;

    /* Generate session ID from config */
    uint32_t sid = (uint32_t)cfg->channel << 24 |
                   (uint32_t)cfg->preamble_code << 16 |
                   (uint32_t)slot;

    g_sessions[slot].id = sid;
    g_sessions[slot].active = true;
    g_sessions[slot].ranging = false;
    g_sessions[slot].last_distance_cm = 0xFFFF;
    g_sessions[slot].last_zone = ZONE_LOCKED;

    return sid;
}

ccc_status_t uwb_destroy_session(uint32_t session_id)
{
    for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
        if (g_sessions[i].active && g_sessions[i].id == session_id) {
            ncj29d6_send_cmd(NCJ29D6_CMD_DESTROY_SESSION,
                             (const uint8_t *)&session_id, 4);
            g_sessions[i].active = false;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}

ccc_status_t uwb_start_ranging(uint32_t session_id)
{
    for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
        if (g_sessions[i].active && g_sessions[i].id == session_id) {
            ccc_status_t ret = ncj29d6_send_cmd(NCJ29D6_CMD_START_RANGING,
                                                  (const uint8_t *)&session_id, 4);
            if (ret == CCC_OK) g_sessions[i].ranging = true;
            return ret;
        }
    }
    return CCC_ERR_NOT_FOUND;
}

ccc_status_t uwb_stop_ranging(uint32_t session_id)
{
    for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
        if (g_sessions[i].active && g_sessions[i].id == session_id) {
            ncj29d6_send_cmd(NCJ29D6_CMD_STOP_RANGING,
                             (const uint8_t *)&session_id, 4);
            g_sessions[i].ranging = false;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}

ccc_status_t uwb_get_distance(uint32_t session_id, uint16_t *dist_cm)
{
    if (!dist_cm) return CCC_ERR_INVALID_PARAM;
    for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
        if (g_sessions[i].active && g_sessions[i].id == session_id) {
            *dist_cm = g_sessions[i].last_distance_cm;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}

distance_zone_e uwb_get_zone(uint32_t session_id)
{
    for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
        if (g_sessions[i].active && g_sessions[i].id == session_id) {
            return g_sessions[i].last_zone;
        }
    }
    return ZONE_LOCKED;
}

ccc_status_t uwb_set_threshold(const distance_threshold_t *th)
{
    if (!th) return CCC_ERR_INVALID_PARAM;
    memcpy(&g_threshold, th, sizeof(g_threshold));
    return CCC_OK;
}

ccc_status_t uwb_register_zone_cb(uwb_zone_cb_t cb)
{
    g_zone_cb = cb;
    return CCC_OK;
}

/* IRQ handler - ranging result from NCJ29D6 */
void ncj29d6_irq_handler(void)
{
    uint8_t result_buf[16] = {0};
    uint8_t header[4] = {0};

    gpio_write(NCJ29D6_CS_PORT, NCJ29D6_CS_PIN, 0);
    spi_transfer(3, NULL, header, 4);
    uint16_t len = ((uint16_t)header[2] << 8) | header[3];
    if (len > 0 && len <= 16) {
        spi_transfer(3, NULL, result_buf, len);
    }
    gpio_write(NCJ29D6_CS_PORT, NCJ29D6_CS_PIN, 1);

    if (header[0] == NCJ29D6_EVT_RANGING_RESULT && len >= 6) {
        uint32_t sid = ((uint32_t)result_buf[0] << 24) |
                       ((uint32_t)result_buf[1] << 16) |
                       ((uint32_t)result_buf[2] << 8)  |
                       result_buf[3];
        uint16_t dist = ((uint16_t)result_buf[4] << 8) | result_buf[5];

        for (int i = 0; i < UWB_MAX_SESSIONS; i++) {
            if (g_sessions[i].active && g_sessions[i].id == sid) {
                distance_zone_e old_zone = g_sessions[i].last_zone;
                g_sessions[i].last_distance_cm = dist;
                g_sessions[i].last_zone = classify_distance_impl(dist);

                if (g_sessions[i].last_zone != old_zone && g_zone_cb) {
                    g_zone_cb(sid, g_sessions[i].last_zone, dist);
                }
                break;
            }
        }
    }
}

/* Internal: classify distance into zone */
static distance_zone_e classify_distance_impl(uint16_t dist_cm)
{
    if (dist_cm <= g_threshold.inside_cm)    return ZONE_INSIDE;
    if (dist_cm <= g_threshold.entry_cm)     return ZONE_ENTRY;
    if (dist_cm <= g_threshold.unlock_cm)    return ZONE_UNLOCK;
    if (dist_cm <= g_threshold.approach_cm)  return ZONE_APPROACH;
    return ZONE_LOCKED;
}
