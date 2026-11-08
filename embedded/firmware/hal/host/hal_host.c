/*
 * hal_host.c — Host-simulation HAL backend
 *
 * Used when FIRMWARE_HOST_SIM=ON (no NXP KW47A / MCUXpresso SDK available).
 * Every hardware-touching function is a deterministic stub so the firmware
 * and all three protocol stacks compile and link on a standard host toolchain.
 * This enables CI build-integrity and protocol-stack link verification.
 *
 * It is NOT functionally equivalent to the real KW47A HAL and must not be
 * used as production evidence.
 */

#include "hal_interface.h"
#include "kw47a_board.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* ── Time ────────────────────────────────────────────────── */
uint64_t hal_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

uint32_t hal_time_ms(void)
{
    return (uint32_t)(hal_time_us() / 1000ULL);
}

void hal_delay_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ── BLE (hal_interface.h) ───────────────────────────────── */
int hal_ble_init(void)                              { return 0; }
int hal_ble_advertise_start(const uint8_t *adv_data, uint16_t len)
{ (void)adv_data; (void)len; return 0; }
int hal_ble_advertise_stop(void)                   { return 0; }
int hal_ble_send_notify(uint16_t conn_handle, uint16_t char_handle,
                        const uint8_t *data, uint16_t len)
{ (void)conn_handle; (void)char_handle; (void)data; (void)len; return 0; }
int hal_ble_read_rssi(uint16_t conn_handle, int8_t *rssi)
{ (void)conn_handle; if (rssi) *rssi = -50; return 0; }

/* ── NFC ─────────────────────────────────────────────────── */
int hal_nfc_init(void)                              { return 0; }
int hal_nfc_poll(uint8_t *buffer, uint16_t *len, uint32_t timeout_ms)
{ (void)buffer; (void)len; (void)timeout_ms; return -1; }
int hal_nfc_transceive(const uint8_t *tx, uint16_t tx_len,
                       uint8_t *rx, uint16_t *rx_len)
{ (void)tx; (void)tx_len; (void)rx; if (rx_len) *rx_len = 0; return -1; }

/* ── UWB ─────────────────────────────────────────────────── */
int hal_uwb_init(void)                              { return 0; }
int hal_uwb_start_ranging(uint16_t session_id)      { (void)session_id; return 0; }
int hal_uwb_stop_ranging(void)                      { return 0; }
int hal_uwb_get_distance(float *distance_mm, int8_t *rssi)
{ if (distance_mm) *distance_mm = 0.0f; if (rssi) *rssi = 0; return -1; }

/* ── SE050 Secure Element ────────────────────────────────── */
int hal_se050_init(void)                            { return 0; }
int hal_se050_ecdsa_sign(const uint8_t *key_id, const uint8_t *hash,
                         uint8_t *signature)
{ (void)key_id; (void)hash; (void)signature; return -1; }
int hal_se050_ecdsa_verify(const uint8_t *pub_key, const uint8_t *hash,
                           const uint8_t *sig)
{ (void)pub_key; (void)hash; (void)sig; return -1; }
int hal_se050_ecdh_compute(const uint8_t *key_id, const uint8_t *peer_pub,
                           uint8_t *shared_secret)
{ (void)key_id; (void)peer_pub; (void)shared_secret; return -1; }
int hal_se050_generate_random(uint8_t *buffer, uint16_t len)
{ (void)buffer; (void)len; return -1; }

/* ── Storage ─────────────────────────────────────────────── */
int hal_storage_init(void)                          { return 0; }
int hal_storage_write(const char *key, const uint8_t *data, uint16_t len)
{ (void)key; (void)data; (void)len; return 0; }
int hal_storage_read(const char *key, uint8_t *data, uint16_t *len)
{ (void)key; (void)data; if (len) *len = 0; return -1; }
int hal_storage_delete(const char *key)             { (void)key; return 0; }

/* ── Debug ───────────────────────────────────────────────── */
void hal_debug_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* ══════════════════════════════════════════════════════════
 *  KW47A Board BSP stubs (kw47a_board.h)
 *  Real implementation configures clocks/pinmux/UART/SPI/I2C.
 *  On host they are no-ops.
 * ══════════════════════════════════════════════════════════ */
void kw47a_board_init(void)                         { }
void kw47a_clock_init(void)                         { }
void kw47a_pinmux_init(void)                        { }
void kw47a_uart_init(uint32_t baud)                 { (void)baud; }
void kw47a_spi_init(int instance, uint32_t baud)    { (void)instance; (void)baud; }
void kw47a_i2c_init(int instance, uint32_t baud)    { (void)instance; (void)baud; }

/* ══════════════════════════════════════════════════════════
 *  hardware_abstraction.h externs (used by ccc_protocol)
 * ══════════════════════════════════════════════════════════ */
int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len)
{ (void)dev; (void)tx; (void)rx; (void)len; return -1; }

void gpio_write(uint8_t port, uint8_t pin, uint8_t val)
{ (void)port; (void)pin; (void)val; }

uint8_t gpio_read(uint8_t port, uint8_t pin)
{ (void)port; (void)pin; return 0; }

void gpio_write_wake(uint8_t pin, uint8_t val)
{ (void)pin; (void)val; }

void delay_ms(uint32_t ms)                          { hal_delay_ms(ms); }

/* ══════════════════════════════════════════════════════════
 *  TRNG (mcal_stubs/include/Trng.h) — used by ccc_protocol crypto_random.c
 * ══════════════════════════════════════════════════════════ */
#define HAL_TRNG_OK              0
#define HAL_TRNG_ERR_NOT_INIT   (-1)
#define HAL_TRNG_ERR_PARAM      (-2)

static int g_trng_inited = 0;

int hal_trng_init(void)
{
    g_trng_inited = 1;
    return HAL_TRNG_OK;
}

void hal_trng_deinit(void)
{
    g_trng_inited = 0;
}

int hal_trng_read(uint8_t *buf, size_t len)
{
    if (!g_trng_inited) return HAL_TRNG_ERR_NOT_INIT;
    if (!buf || len == 0) return HAL_TRNG_ERR_PARAM;
    /* Host fallback: fill with pseudo-random bytes. Not cryptographically
     * equivalent to the hardware RNGA — for build/link verification only. */
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
    return HAL_TRNG_OK;
}

int hal_trng_self_test(void)
{
    return HAL_TRNG_OK;
}

/* ══════════════════════════════════════════════════════════
 *  ICCOA BLE HAL (declared extern in iccoa_protocol/src/ble/iccoa_ble.c)
 *  Note: iccoa uses different BLE function names than hal_interface.h.
 * ══════════════════════════════════════════════════════════ */
int hal_ble_deinit(void)                            { return 0; }

int hal_ble_start_advertising(const uint8_t *adv_data, uint8_t adv_len,
                              const uint8_t *scan_rsp, uint8_t scan_len,
                              uint16_t interval_min_ms, uint16_t interval_max_ms)
{
    (void)adv_data; (void)adv_len; (void)scan_rsp; (void)scan_len;
    (void)interval_min_ms; (void)interval_max_ms;
    return 0;
}

int hal_ble_stop_advertising(void)                  { return 0; }

int hal_ble_send_notification(uint16_t conn_handle, uint16_t char_handle,
                              const uint8_t *data, uint16_t len)
{ (void)conn_handle; (void)char_handle; (void)data; (void)len; return 0; }

int hal_ble_disconnect(uint16_t conn_handle)        { (void)conn_handle; return 0; }
int hal_ble_request_encryption(uint16_t conn_handle){ (void)conn_handle; return 0; }

int hal_ble_update_connection(uint16_t conn_handle, uint16_t min_interval,
                              uint16_t max_interval, uint16_t latency,
                              uint16_t timeout)
{ (void)conn_handle; (void)min_interval; (void)max_interval; (void)latency; (void)timeout; return 0; }

int hal_ble_add_service(uint16_t uuid, uint16_t *svc_handle)
{ (void)uuid; if (svc_handle) *svc_handle = 1; return 0; }

int hal_ble_add_characteristic(uint16_t svc_handle, uint16_t uuid,
                               uint8_t props, uint8_t perms,
                               uint16_t *char_handle, uint16_t *ccc_handle)
{
    (void)svc_handle; (void)uuid; (void)props; (void)perms;
    if (char_handle) *char_handle = 1;
    if (ccc_handle)  *ccc_handle  = 2;
    return 0;
}

/* ══════════════════════════════════════════════════════════
 *  ICCOA Vehicle HAL (declared extern in iccoa_service.c)
 * ══════════════════════════════════════════════════════════ */
int hal_vehicle_lock_doors(uint8_t lock)            { (void)lock; return 0; }
int hal_vehicle_start_engine(uint8_t start)         { (void)start; return 0; }
int hal_vehicle_control_window(uint8_t window_id, uint8_t direction)
{ (void)window_id; (void)direction; return 0; }
int hal_vehicle_open_trunk(void)                    { return 0; }
int hal_vehicle_set_climate(uint8_t on, int8_t temp){ (void)on; (void)temp; return 0; }
int hal_vehicle_horn(uint8_t times)                 { (void)times; return 0; }
int hal_vehicle_flash_lights(uint8_t pattern)       { (void)pattern; return 0; }
int hal_vehicle_get_door_status(uint8_t *status)    { if (status) *status = 0; return 0; }
int hal_vehicle_get_window_status(uint8_t *status)  { if (status) *status = 0; return 0; }
int hal_vehicle_get_engine_status(uint8_t *status)  { if (status) *status = 0; return 0; }
int hal_vehicle_get_lock_status(uint8_t *status)    { if (status) *status = 0; return 0; }
int hal_vehicle_get_battery_level(int8_t *level)    { if (level) *level = 100; return 0; }
int hal_vehicle_get_interior_temp(int16_t *temp)    { if (temp) *temp = 22; return 0; }
int hal_vehicle_get_alarm_status(uint8_t *status)   { if (status) *status = 0; return 0; }

/* ══════════════════════════════════════════════════════════
 *  Platform I2C (used by ccc_protocol se050_scp03.c / security.c)
 * ══════════════════════════════════════════════════════════ */
int32_t i2c_transfer(uint8_t dev, uint8_t addr,
                     const uint8_t *tx, uint16_t tx_len,
                     uint8_t *rx, uint16_t rx_len)
{
    (void)dev; (void)addr; (void)tx; (void)tx_len; (void)rx; (void)rx_len;
    return -1;
}

/* ══════════════════════════════════════════════════════════
 *  ICCE BLE (declared in icce_digital_key.h, used by icce_dk_core.c)
 * ══════════════════════════════════════════════════════════ */
int32_t icce_ble_init(void)                          { return 0; }
int32_t icce_ble_deinit(void)                        { return 0; }
int32_t icce_ble_start_adv(void)                     { return 0; }
int32_t icce_ble_stop_adv(void)                      { return 0; }
int32_t icce_ble_register_cb(void (*cb)(const uint8_t *data, uint16_t len))
{ (void)cb; return 0; }

/* ══════════════════════════════════════════════════════════
 *  ICCE Storage driver (icce_protocol/src/cache/storage_driver.h)
 * ══════════════════════════════════════════════════════════ */
int32_t storage_write(uint8_t handle, const uint8_t *key, uint16_t key_len,
                      const uint8_t *data, uint32_t data_len)
{ (void)handle; (void)key; (void)key_len; (void)data; (void)data_len; return 0; }

int32_t storage_read(uint8_t handle, const uint8_t *key, uint16_t key_len,
                     uint8_t *data, uint32_t *data_len)
{
    (void)handle; (void)key; (void)key_len; (void)data;
    if (data_len) *data_len = 0;
    return -1;
}

int32_t storage_delete(uint8_t handle, const uint8_t *key, uint16_t key_len)
{ (void)handle; (void)key; (void)key_len; return 0; }

/* ══════════════════════════════════════════════════════════
 *  ICCOA platform / logger / time
 * ══════════════════════════════════════════════════════════ */
void platform_main_loop_step(void)                   { }

void dk_logger_log(uint8_t level, const char *tag,
                   const char *file, int line,
                   const char *format, ...)
{
    (void)level; (void)tag; (void)file; (void)line; (void)format;
    /* Host stub: swallow logs to keep test output clean. */
}

uint32_t sys_tick_get_ms(void)                       { return hal_time_ms(); }

/* ══════════════════════════════════════════════════════════
 *  ICCOA SE050 crypto (declared extern in iccoa_dk40.c)
 *  Host build: return unsupported (-1); real target delegates to SE050.
 * ══════════════════════════════════════════════════════════ */
int se050_key_get_master_key(uint8_t *key_out)
{ (void)key_out; return -1; }

int se050_hmac_sha256(const uint8_t *key, uint16_t key_len,
                      const uint8_t *data, uint16_t data_len,
                      uint8_t *mac_out)
{ (void)key; (void)key_len; (void)data; (void)data_len; (void)mac_out; return -1; }

int se050_rng(uint8_t *out, uint16_t len)
{ (void)out; (void)len; return -1; }

int se050_sha256(const uint8_t *data, uint16_t len, uint8_t *hash_out)
{ (void)data; (void)len; (void)hash_out; return -1; }

int se050_key_get_public_key(uint8_t *pub_key_out)
{ (void)pub_key_out; return -1; }

int se050_key_derive_shared(const uint8_t *seed, uint16_t seed_len, uint8_t *key_out)
{ (void)seed; (void)seed_len; (void)key_out; return -1; }

int se050_verify_ecdsa(const uint8_t *pub_key, const uint8_t *hash,
                       const uint8_t *sig, uint16_t sig_len)
{ (void)pub_key; (void)hash; (void)sig; (void)sig_len; return -1; }

/* ══════════════════════════════════════════════════════════
 *  ICCOA UWB (declared extern in iccoa_dk40.c)
 * ══════════════════════════════════════════════════════════ */
int uwb_ncj29d6_start_ranging(uint8_t channel, uint8_t mode)
{ (void)channel; (void)mode; return -1; }

int uwb_ncj29d6_set_threshold(int16_t min, int16_t max)
{ (void)min; (void)max; return 0; }

/* ══════════════════════════════════════════════════════════
 *  FreeRTOS heap (freertos_port/include/portable.h)
 *  Host build: forward to libc malloc/free.
 * ══════════════════════════════════════════════════════════ */
void *pvPortMalloc(size_t xSize)
{
    return malloc(xSize);
}

void vPortFree(void *pv)
{
    free(pv);
}

/* ══════════════════════════════════════════════════════════
 *  SE05x middleware (declared extern in ccc_protocol security.c)
 *  Host build: return unsupported.
 * ══════════════════════════════════════════════════════════ */
int se05x_open_session(void)
{
    /* Host simulation: no real SE050. Report success so the software
     * crypto fallback path (crypto_engine) can be exercised and tested.
     * The USE_SE050 hardware path is not compiled in host mode. */
    return 0;
}
void se05x_close_session(void)                        { }
int se05x_write_transparent(uint32_t obj_id, const uint8_t *data, uint16_t len)
{ (void)obj_id; (void)data; (void)len; return -1; }
int se05x_read_transparent(uint32_t obj_id, uint8_t *data, uint16_t *len)
{ (void)obj_id; (void)data; if (len) *len = 0; return -1; }
int se05x_delete_object(uint32_t obj_id)              { (void)obj_id; return -1; }
int se05x_get_free_memory(uint32_t *free_bytes)       { if (free_bytes) *free_bytes = 0; return -1; }
