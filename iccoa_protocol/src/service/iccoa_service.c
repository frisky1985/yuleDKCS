/**
 * @file iccoa_service.c
 * @brief ICCOA Vehicle Service - Control Commands & Status Reporting
 */

#include "iccoa_digital_key.h"

static iccoa_vehicle_status_t g_status = {
    .door_status    = 0xFF,  /* All closed */
    .window_status  = 0xFF,  /* All closed */
    .engine_status  = 0x00,  /* Off */
    .lock_status    = 0x01,  /* Locked */
    .battery_level  = 100,
    .interior_temp  = 25,
    .alarm_status   = 0x00,  /* No alarm */
    .reserved       = {0}
};

int32_t iccoa_service_init(void)
{
    /* Register with vehicle CAN bus */
    /* Subscribe to vehicle status updates */
    return ICCOA_OK;
}

int32_t iccoa_ctrl_execute(iccoa_ctrl_cmd_e cmd, uint8_t param)
{
    switch (cmd) {
    case CTRL_LOCK:
        /* CAN: send lock command */
        g_status.lock_status = 0x01;
        break;
    case CTRL_UNLOCK:
        g_status.lock_status = 0x00;
        break;
    case CTRL_ENGINE_ON:
        g_status.engine_status = 0x01;
        break;
    case CTRL_ENGINE_OFF:
        g_status.engine_status = 0x00;
        break;
    case CTRL_TRUNK_OPEN:
        g_status.door_status &= ~(1 << 4);  /* Clear trunk bit */
        break;
    case CTRL_WINDOW_UP:
        g_status.window_status |= (1 << param);  /* Set window bit */
        break;
    case CTRL_WINDOW_DOWN:
        g_status.window_status &= ~(1 << param);
        break;
    case CTRL_CLIMATE_ON:
        /* CAN: climate control on, set temp = param */
        break;
    case CTRL_CLIMATE_OFF:
        break;
    case CTRL_FIND:
        /* CAN: flash lights + horn for 5s */
        break;
    case CTRL_HORN:
        /* CAN: horn beep */
        break;
    default:
        return ICCOA_ERR_PARAM;
    }

    /* Notify status change via BLE notification */
    uint8_t ntf[2] = { (uint8_t)cmd, 0x00 };
    iccoa_dk30_send_response(ICCOA_CMD_STATUS_NTF, ntf, 2);

    return ICCOA_OK;
}

int32_t iccoa_service_get_status(iccoa_vehicle_status_t *status)
{
    if (!status) return ICCOA_ERR_PARAM;
    memcpy(status, &g_status, sizeof(iccoa_vehicle_status_t));
    return ICCOA_OK;
}
