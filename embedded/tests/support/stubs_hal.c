/**
 * stubs.c — Hardware Abstraction Layer + CCC/Unified API Stubs
 *
 * Provides mock implementations for:
 *   1. extern/HAL functions referenced by real protocol source files
 *   2. CCC public API functions (when real CCC sources can't compile)
 *   3. Unified protocol API functions (when real unified source can't link)
 *
 * Real ICCOA protocol source files are compiled directly; only their
 * extern/HAL dependencies are stubbed here.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/* ========================================================================
 *  System Time
 * ======================================================================== */
static uint32_t g_sys_tick = 0;
uint32_t sys_tick_get_ms(void) { return g_sys_tick++; }

/* ========================================================================
 *  ICCOA BLE HAL — iccoa_ble.c
 * ======================================================================== */
int hal_ble_init(void) { return 0; }
int hal_ble_deinit(void) { return 0; }
int hal_ble_start_advertising(const uint8_t *a, uint8_t al, const uint8_t *s, uint8_t sl,
                              uint16_t imin, uint16_t imax)
{ (void)a;(void)al;(void)s;(void)sl;(void)imin;(void)imax; return 0; }
int hal_ble_stop_advertising(void) { return 0; }
int hal_ble_send_notification(uint16_t ch, uint16_t cchar, const uint8_t *d, uint16_t l)
{ (void)ch;(void)cchar;(void)d;(void)l; return 0; }
int hal_ble_disconnect(uint16_t ch) { (void)ch; return 0; }
int hal_ble_request_encryption(uint16_t ch) { (void)ch; return 0; }
int hal_ble_update_connection(uint16_t ch, uint16_t iv, uint16_t la, uint16_t to)
{ (void)ch;(void)iv;(void)la;(void)to; return 0; }
int hal_ble_add_service(uint16_t uuid, uint16_t *sh) { (void)uuid; *sh=1; return 0; }
int hal_ble_add_characteristic(uint16_t sh, uint16_t uuid, uint8_t prop, uint8_t perm, uint16_t *ch)
{ (void)sh;(void)uuid;(void)prop;(void)perm; *ch=1; return 0; }

/* ========================================================================
 *  ICCOA Vehicle HAL — iccoa_service.c
 * ======================================================================== */
int hal_vehicle_lock_doors(uint8_t l) { (void)l; return 0; }
int hal_vehicle_start_engine(uint8_t s) { (void)s; return 0; }
int hal_vehicle_control_window(uint8_t w, uint8_t d) { (void)w;(void)d; return 0; }
int hal_vehicle_open_trunk(void) { return 0; }
int hal_vehicle_set_climate(uint8_t on, int8_t t) { (void)on;(void)t; return 0; }
int hal_vehicle_horn(uint8_t t) { (void)t; return 0; }
int hal_vehicle_flash_lights(uint8_t p) { (void)p; return 0; }
int hal_vehicle_get_door_status(uint8_t *s) { *s=0; return 0; }
int hal_vehicle_get_window_status(uint8_t *s) { *s=0; return 0; }
int hal_vehicle_get_engine_status(uint8_t *s) { *s=0; return 0; }
int hal_vehicle_get_lock_status(uint8_t *s) { *s=1; return 0; }
int hal_vehicle_get_battery_level(int8_t *l) { *l=80; return 0; }
int hal_vehicle_get_interior_temp(int16_t *t) { *t=220; return 0; }
int hal_vehicle_get_alarm_status(uint8_t *s) { *s=0; return 0; }

/* ========================================================================
 *  Platform
 * ======================================================================== */
void platform_main_loop_step(void) {}

/* ========================================================================
 *  CCC SPI/GPIO/Delay — ble_kw47a.c, nfc_st25r501.c, uwb_ncj29d6.c
 * ======================================================================== */
int32_t spi_transfer(uint8_t dev, const uint8_t *tx, uint8_t *rx, uint16_t len)
{ (void)dev;(void)tx;(void)rx;(void)len; return 0; }
void gpio_write(uint8_t p, uint8_t pin, uint8_t v) { (void)p;(void)pin;(void)v; }
void gpio_write_wake(uint8_t pin, uint8_t v) { (void)pin;(void)v; }
uint8_t gpio_read(uint8_t p, uint8_t pin) { (void)p;(void)pin; return 0; }
void delay_ms(uint32_t ms) { (void)ms; }

/* ========================================================================
 *  CCC SE050 I2C — security.c
 * ======================================================================== */
/* Mock SE050 transparent storage for testing */
#define MOCK_SE050_OBJ_MAX 32
static struct {
    uint32_t obj_id;
    uint8_t  data[576];
    uint16_t len;
} g_mock_se050_objs[MOCK_SE050_OBJ_MAX];
static int g_mock_se050_obj_count = 0;

int32_t i2c_transfer(uint8_t d, uint8_t a, const uint8_t *tx, uint16_t txl, uint8_t *rx, uint16_t rxl)
{ (void)d;(void)a;(void)tx;(void)txl;(void)rx;(void)rxl; return 0; }
int se05x_open_session(void) { return 0; }
void se05x_close_session(void) {}
int se05x_write_transparent(uint32_t oid, const uint8_t *d, uint16_t l)
{
    /* Find existing or free slot */
    for (int i = 0; i < MOCK_SE050_OBJ_MAX; i++) {
        if (i < g_mock_se050_obj_count && g_mock_se050_objs[i].obj_id == oid) {
            memcpy(g_mock_se050_objs[i].data, d, l);
            g_mock_se050_objs[i].len = l;
            return 0;
        }
    }
    if (g_mock_se050_obj_count < MOCK_SE050_OBJ_MAX) {
        int idx = g_mock_se050_obj_count++;
        g_mock_se050_objs[idx].obj_id = oid;
        memcpy(g_mock_se050_objs[idx].data, d, l);
        g_mock_se050_objs[idx].len = l;
        return 0;
    }
    return -1;
}
int se05x_read_transparent(uint32_t oid, uint8_t *d, uint16_t *l)
{
    for (int i = 0; i < g_mock_se050_obj_count; i++) {
        if (g_mock_se050_objs[i].obj_id == oid) {
            uint16_t copy_len = (*l < g_mock_se050_objs[i].len) ? *l : g_mock_se050_objs[i].len;
            memcpy(d, g_mock_se050_objs[i].data, copy_len);
            *l = g_mock_se050_objs[i].len;
            return 0;
        }
    }
    *l = 0;
    return -1; /* Not found */
}
int se05x_delete_object(uint32_t oid) {
    for (int i = 0; i < g_mock_se050_obj_count; i++) {
        if (g_mock_se050_objs[i].obj_id == oid) {
            /* Shift remaining objects */
            for (int j = i; j < g_mock_se050_obj_count - 1; j++) {
                g_mock_se050_objs[j] = g_mock_se050_objs[j + 1];
            }
            g_mock_se050_obj_count--;
            return 0;
        }
    }
    return -1;
}
int se05x_get_free_memory(uint32_t *f) { *f=4096; return 0; }

/* ========================================================================
 *  Logger — dk_logger.h 
 * ======================================================================== */
void dk_logger_log(uint8_t level, const char *tag, const char *file, int line,
                    const char *fmt, ...)
{ (void)level;(void)tag;(void)file;(void)line;(void)fmt; }

/* ========================================================================
 *  Virtual flash — key_mgmt.c
 * ======================================================================== */
__attribute__((weak)) int virt_flash_write(uint32_t a, const uint8_t *d, uint16_t l)
{ (void)a;(void)d;(void)l; return -1; }
__attribute__((weak)) int virt_flash_read(uint32_t a, uint8_t *d, uint16_t l)
{ (void)a;(void)d;(void)l; return -1; }
__attribute__((weak)) int virt_flash_erase(uint32_t a, uint16_t l)
{ (void)a;(void)l; return -1; }

/* ========================================================================
 *  DK40 SE050/UWB externs — matching iccoa_dk40.c signatures
 * ======================================================================== */
int se050_rng(uint8_t *out, uint16_t len) { (void)out;(void)len; return 0; }
int se050_key_get_master_key(uint8_t *ko) { if(ko)memset(ko,0x42,16); return 0; }
int se050_hmac_sha256(const uint8_t *k, uint16_t kl, const uint8_t *d, uint16_t dl, uint8_t *mo)
{ (void)k;(void)kl;(void)d;(void)dl;(void)mo; return 0; }
int se050_verify_ecdsa(const uint8_t *pk, const uint8_t *h, const uint8_t *s, uint16_t sl)
{ (void)pk;(void)h;(void)s;(void)sl; return 0; }
int uwb_ncj29d6_start_ranging(uint8_t ch, uint8_t md) { (void)ch;(void)md; return 0; }
int uwb_ncj29d6_set_threshold(int16_t mn, int16_t mx) { (void)mn;(void)mx; return 0; }
int uwb_ncj29d6_stop_ranging(uint32_t sid) { (void)sid; return 0; }
int uwb_ncj29d6_get_distance(uint32_t sid, uint16_t *d) { (void)sid; *d=300; return 0; }
int se050_sha256(const uint8_t *in, uint16_t ilen, uint8_t *out) { (void)in;(void)ilen; if(out)memset(out,0xAB,32); return 0; }
int se050_verify_share_token(const uint8_t *token, uint16_t tlen, const uint8_t *pubkey, uint16_t pklen) { (void)token;(void)tlen;(void)pubkey;(void)pklen; return 0; }
int se050_ecdsa_sign(const uint8_t *key, const uint8_t *hash, uint8_t *sig) { (void)key;(void)hash;(void)sig; return 0; }
int se050_key_derive_shared(const uint8_t *priv, uint16_t privlen, const uint8_t *pub, uint16_t publen, uint8_t *out, uint16_t *outlen) { (void)priv;(void)privlen;(void)pub;(void)publen; if(outlen)*outlen=32; if(out)memset(out,0x42,32); return 0; }
int se050_key_get_public_key(const uint8_t *key, uint8_t *pub, uint16_t *publen) { (void)key; if(publen)*publen=64; if(pub)memset(pub,0xAA,64); return 0; }

/* ========================================================================
 *  ICCE BLE stubs — icce_digital_key.h API
 *  icce_ble_* is declared in icce_digital_key.h, normally in ble/ble_manager.c.
 *  These stubs let high-level ICCE modules compile without the real BLE HAL.
 * ======================================================================== */
#include "icce_digital_key.h"

int32_t icce_ble_init(void) { return ICCE_OK; }
int32_t icce_ble_deinit(void) { return ICCE_OK; }
int32_t icce_ble_start_adv(void) { return ICCE_OK; }
int32_t icce_ble_stop_adv(void) { return ICCE_OK; }
int32_t icce_ble_send(const uint8_t *data, uint16_t len) { (void)data;(void)len; return ICCE_OK; }
int32_t icce_ble_register_cb(icce_ble_recv_cb_t cb) { (void)cb; return ICCE_OK; }

/* se05x_rng — referenced by icce crypto_utils.c (if compiled) */
int se05x_rng(uint8_t *buf, size_t len) { (void)buf;(void)len; return 0; }
