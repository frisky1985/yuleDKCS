/**
 * @file icce_vehicle.c
 * @brief ICCE Vehicle TCU Integration - CAN Bus Control & Status
 */

#include "icce_digital_key.h"

static icce_vehicle_status_t g_status = {
    .lock_status    = 0x01,  /* Locked */
    .engine_status  = 0x00,
    .door_status    = 0xFF,  /* All closed */
    .window_status  = 0xFF,
    .battery_pct    = 100,
    .interior_temp  = 25,
    .odometer_km    = 0,
    .alarm_status   = 0x00,
    .fuel_level     = 80,
    .reserved       = {0}
};

static icce_vehicle_status_cb_t g_status_cb = NULL;

int32_t icce_vehicle_init(void)
{
    /* CAN bus init */
    /* Subscribe to vehicle status broadcast messages */
    g_status_cb = NULL;
    return ICCE_OK;
}

int32_t icce_vehicle_ctrl(icce_action_e action, uint8_t param)
{
    switch (action) {
    case ICCE_ACTION_UNLOCK:
        /* CAN: unlock all doors */
        g_status.lock_status = 0x00;
        break;
    case ICCE_ACTION_LOCK:
        g_status.lock_status = 0x01;
        break;
    case ICCE_ACTION_START:
        /* CAN: engine start request (requires PND validation) */
        g_status.engine_status = 0x01;
        break;
    case ICCE_ACTION_STOP:
        g_status.engine_status = 0x00;
        break;
    case ICCE_ACTION_CLIMATE:
        /* CAN: set climate target temp = param */
        break;
    case ICCE_ACTION_LIGHTS:
        /* CAN: flash lights */
        break;
    case ICCE_ACTION_HORN:
        /* CAN: horn beep */
        break;
    case ICCE_ACTION_CUSTOM:
        /* CAN: custom command via param */
        break;
    default:
        return ICCE_ERR_PARAM;
    }

    /* Notify status change */
    if (g_status_cb) {
        g_status_cb(&g_status);
    }
    return ICCE_OK;
}

int32_t icce_vehicle_get_status(icce_vehicle_status_t *status)
{
    if (!status) return ICCE_ERR_PARAM;
    memcpy(status, &g_status, sizeof(icce_vehicle_status_t));
    return ICCE_OK;
}

int32_t icce_vehicle_register_cb(icce_vehicle_status_cb_t cb)
{
    if (!cb) return ICCE_ERR_PARAM;
    g_status_cb = cb;
    return ICCE_OK;
}
