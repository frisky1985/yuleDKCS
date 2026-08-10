/*
 * hal_interface.h — Hardware Abstraction Layer API
 *
 * Platform-agnostic interface that protocol stacks use.
 * Implemented per-platform in hal/<mcu>/.
 */

#ifndef HAL_INTERFACE_H
#define HAL_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Time ────────────────────────────────────────────────── */
uint64_t hal_time_us(void);
uint32_t hal_time_ms(void);
void     hal_delay_ms(uint32_t ms);

/* ── BLE ─────────────────────────────────────────────────── */
int  hal_ble_init(void);
int  hal_ble_advertise_start(const uint8_t *adv_data, uint16_t len);
int  hal_ble_advertise_stop(void);
int  hal_ble_send_notify(uint16_t conn_handle, uint16_t char_handle,
                         const uint8_t *data, uint16_t len);
int  hal_ble_read_rssi(uint16_t conn_handle, int8_t *rssi);

/* ── NFC ─────────────────────────────────────────────────── */
int  hal_nfc_init(void);
int  hal_nfc_poll(uint8_t *buffer, uint16_t *len, uint32_t timeout_ms);
int  hal_nfc_transceive(const uint8_t *tx, uint16_t tx_len,
                         uint8_t *rx, uint16_t *rx_len);

/* ── UWB ─────────────────────────────────────────────────── */
int  hal_uwb_init(void);
int  hal_uwb_start_ranging(uint16_t session_id);
int  hal_uwb_stop_ranging(void);
int  hal_uwb_get_distance(float *distance_mm, int8_t *rssi);

/* ── SE050 Secure Element ────────────────────────────────── */
int  hal_se050_init(void);
int  hal_se050_ecdsa_sign(const uint8_t *key_id, const uint8_t *hash,
                           uint8_t *signature);
int  hal_se050_ecdsa_verify(const uint8_t *pub_key, const uint8_t *hash,
                             const uint8_t *sig);
int  hal_se050_ecdh_compute(const uint8_t *key_id, const uint8_t *peer_pub,
                             uint8_t *shared_secret);
int  hal_se050_generate_random(uint8_t *buffer, uint16_t len);

/* ── Storage ─────────────────────────────────────────────── */
int  hal_storage_init(void);
int  hal_storage_write(const char *key, const uint8_t *data, uint16_t len);
int  hal_storage_read(const char *key, uint8_t *data, uint16_t *len);
int  hal_storage_delete(const char *key);

/* ── Debug ───────────────────────────────────────────────── */
void hal_debug_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* HAL_INTERFACE_H */
