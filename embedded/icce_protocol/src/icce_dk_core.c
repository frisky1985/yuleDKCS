/**
 * @file icce_dk_core.c
 * @brief ICCE Digital Key Core - Main Loop & Dispatch
 */

#include "icce_digital_key.h"

static bool g_initialized = false;
static icce_zone_e g_current_zone = ICCE_ZONE_NONE;

static void on_ble_data(const uint8_t *data, uint16_t len)
{
    /* Parse ICCE command frames */
    /* Dispatch to security/auth/ctrl modules */
    (void)data; (void)len;
}

static void on_uwb_ranging(const icce_uwb_session_t *session)
{
    if (!session) return;

    /* Classify zone from distance */
    icce_zone_e new_zone = icce_zone_classify(session->distance_mm);

    if (new_zone != g_current_zone) {
        /* Zone transition detected - trigger edge rules */
        icce_trigger_e trigger = (new_zone > g_current_zone)
            ? ICCE_TRIGGER_ZONE_ENTER
            : ICCE_TRIGGER_ZONE_EXIT;

        icce_edge_process_trigger(trigger, &new_zone, sizeof(new_zone));
        g_current_zone = new_zone;
    }

    /* Also evaluate edge rules with current distance */
    icce_edge_evaluate(session->distance_mm, 0, (uint8_t)new_zone);
}

int32_t icce_dk_init(void)
{
    int32_t ret;

    ret = icce_ble_init();
    if (ret != ICCE_OK) return ret;

    ret = icce_uwb_init();
    if (ret != ICCE_OK) return ret;

    ret = icce_edge_init();
    if (ret != ICCE_OK) return ret;

    ret = icce_zone_init();
    if (ret != ICCE_OK) return ret;

    ret = icce_security_init();
    if (ret != ICCE_OK) return ret;

    ret = icce_vehicle_init();
    if (ret != ICCE_OK) return ret;

    /* Register callbacks */
    icce_ble_register_cb(on_ble_data);
    icce_uwb_register_cb(on_uwb_ranging);

    /* Start BLE advertising */
    icce_ble_start_adv();

    g_initialized = true;
    g_current_zone = ICCE_ZONE_NONE;
    return ICCE_OK;
}

int32_t icce_dk_deinit(void)
{
    icce_ble_stop_adv();
    icce_uwb_deinit();
    icce_ble_deinit();
    icce_edge_deinit();
    g_initialized = false;
    return ICCE_OK;
}

int32_t icce_dk_run(void)
{
    /* Main loop: UWB ranging + edge evaluation runs via callbacks */
    /* Periodic: status sync, key expiry check, cloud heartbeat */
    return ICCE_OK;
}
