/**
 * @file iccoa_dk_core.c
 * @brief ICCOA Digital Key Core - Main Loop & Dispatch
 */

#include "iccoa_digital_key.h"

static bool g_initialized = false;
static uint8_t g_protocol_version = 3;  /* Default: DK 3.0 */

static void on_ble_data(const uint8_t *data, uint16_t len)
{
    if (!data || len < 2) return;

    /* Detect protocol version */
    if (data[0] == DK30_SOP) {
        /* DK 3.0 frame */
        iccoa_dk30_process(data, len);
    } else if (data[0] == 0xC0 && data[1] == 0x1C) {
        /* DK 4.0 frame (magic 0xICC0, little-endian first byte 0xC0) */
        iccoa_dk40_process(data, len);
    }
}

int32_t iccoa_dk_init(void)
{
    int32_t ret;

    /* Init BLE */
    ret = iccoa_ble_init();
    if (ret != ICCOA_OK) return ret;

    /* Register BLE callback */
    iccoa_ble_register_cb(on_ble_data);

    /* Init DK 3.0 */
    ret = iccoa_dk30_init();
    if (ret != ICCOA_OK) return ret;

    /* Init DK 4.0 */
    ret = iccoa_dk40_init();
    if (ret != ICCOA_OK) return ret;

    /* Init Auth */
    ret = iccoa_auth_init();
    if (ret != ICCOA_OK) return ret;

    /* Init Service */
    ret = iccoa_service_init();
    if (ret != ICCOA_OK) return ret;

    /* Start BLE advertising */
    iccoa_ble_start_adv();

    g_initialized = true;
    return ICCOA_OK;
}

int32_t iccoa_dk_deinit(void)
{
    iccoa_ble_stop_adv();
    iccoa_ble_deinit();
    g_initialized = false;
    return ICCOA_OK;
}

int32_t iccoa_dk_run(void)
{
    /* Main loop handled by BLE callback */
    /* This can be used for periodic tasks (status sync, key expiry check) */
    return ICCOA_OK;
}
