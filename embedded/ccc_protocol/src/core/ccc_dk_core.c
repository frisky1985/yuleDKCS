/**
 * @file ccc_dk_core.c
 * @brief CCC Digital Key - Core State Machine & Main Loop
 */

#include "ccc_digital_key.h"

/* -------------------------------------------------------------------
 *  Module Instances
 * ------------------------------------------------------------------- */
static system_status_t g_status;
static distance_threshold_t g_threshold = {
    .approach_cm  = 1000,
    .unlock_cm    = 500,
    .entry_cm     = 200,
    .inside_cm    = 50,
    .hysteresis_cm = 30
};

/* -------------------------------------------------------------------
 *  Internal Helpers
 * ------------------------------------------------------------------- */
static void transition_state(main_state_e next)
{
    if (g_status.state == next) return;
    g_status.state = next;
}

static distance_zone_e classify_distance(uint16_t dist_cm)
{
    if (dist_cm <= g_threshold.inside_cm)    return ZONE_INSIDE;
    if (dist_cm <= g_threshold.entry_cm)     return ZONE_ENTRY;
    if (dist_cm <= g_threshold.unlock_cm)    return ZONE_UNLOCK;
    if (dist_cm <= g_threshold.approach_cm)  return ZONE_APPROACH;
    return ZONE_LOCKED;
}

/* -------------------------------------------------------------------
 *  UWB Zone Callback
 * ------------------------------------------------------------------- */
static void on_uwb_zone_changed(uint32_t session_id, distance_zone_e zone, uint16_t distance_cm)
{
    (void)session_id;
    g_status.uwb_distance_cm = distance_cm;

    switch (zone) {
    case ZONE_UNLOCK:
    case ZONE_ENTRY:
        transition_state(STATE_UWB_RANGING);
        break;
    case ZONE_INSIDE:
        transition_state(STATE_UNLOCKED);
        break;
    default:
        transition_state(STATE_STANDBY);
        break;
    }
}

/* -------------------------------------------------------------------
 *  BLE Callbacks
 * ------------------------------------------------------------------- */
static void on_ble_connected(uint16_t conn_handle, uint8_t status)
{
    (void)conn_handle;
    if (status == 0) {
        g_status.ble_connected = true;
        transition_state(STATE_BLE_CONNECTED);
    }
}

static void on_ble_disconnected(uint16_t conn_handle, uint8_t reason)
{
    (void)conn_handle; (void)reason;
    g_status.ble_connected = false;
    transition_state(STATE_STANDBY);
}

/* -------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------- */
ccc_status_t ccc_dk_init(void)
{
    memset(&g_status, 0, sizeof(g_status));
    g_status.state = STATE_INIT;

    /* Init NFC */
    ccc_status_t ret = nfc_st25r501_init();
    if (ret != CCC_OK) return ret;

    /* Init BLE */
    ret = ble_kw47a_init();
    if (ret != CCC_OK) return ret;

    /* Register BLE callbacks */
    ble_register_conn_cb(on_ble_connected);
    ble_register_disconn_cb(on_ble_disconnected);

    /* Init UWB */
    ret = uwb_ncj29d6_init();
    if (ret != CCC_OK) return ret;

    /* Set distance thresholds */
    uwb_set_threshold(&g_threshold);
    uwb_register_zone_cb(on_uwb_zone_changed);

    /* Init Security */
    ret = sec_init();
    if (ret != CCC_OK) return ret;

    /* Init Key Management */
    ret = key_mgmt_init();
    if (ret != CCC_OK) return ret;

    transition_state(STATE_STANDBY);
    return CCC_OK;
}

ccc_status_t ccc_dk_deinit(void)
{
    key_mgmt_deinit();
    sec_deinit();
    uwb_ncj29d6_deinit();
    ble_kw47a_deinit();
    nfc_st25r501_deinit();
    transition_state(STATE_INIT);
    return CCC_OK;
}

ccc_status_t ccc_dk_run(void)
{
    switch (g_status.state) {
    case STATE_STANDBY:
        /* Check NFC field */
        if (nfc_field_detect()) {
            g_status.nfc_field = true;
            transition_state(STATE_NFC_DETECT);
        }
        break;

    case STATE_NFC_DETECT:
        nfc_start_listen();
        transition_state(STATE_NFC_READ);
        break;

    case STATE_NFC_READ: {
        ccc_nfc_oob_data_t oob_out = {0};
        ccc_nfc_oob_data_t oob_in  = {0};
        if (nfc_oob_exchange(&oob_out, &oob_in) == CCC_OK) {
            /* Got OOB data, start BLE OOB pairing */
            ble_oob_pair(&oob_in);
            transition_state(STATE_BLE_CONNECTED);
        }
        g_status.nfc_field = false;
        break;
    }

    case STATE_BLE_CONNECTED:
        /* Wait for auth from phone */
        transition_state(STATE_AUTH_PROCESS);
        break;

    case STATE_AUTH_PROCESS: {
        /* Verify key & attestation - handled via BLE recv callback */
        /* On success → UNLOCKED, on fail → LOCKED */
        break;
    }

    case STATE_UNLOCKED:
        /* Monitor UWB for zone changes */
        break;

    case STATE_LOCKED:
        transition_state(STATE_STANDBY);
        break;

    case STATE_UWB_RANGING:
        /* UWB active ranging, zone changes handled by callback */
        break;

    default:
        break;
    }

    return CCC_OK;
}

main_state_e ccc_dk_get_state(void)
{
    return g_status.state;
}

ccc_status_t ccc_dk_get_status(system_status_t *status)
{
    if (!status) return CCC_ERR_INVALID_PARAM;
    memcpy(status, &g_status, sizeof(*status));
    return CCC_OK;
}
