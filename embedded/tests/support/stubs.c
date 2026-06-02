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
int32_t i2c_transfer(uint8_t d, uint8_t a, const uint8_t *tx, uint16_t txl, uint8_t *rx, uint16_t rxl)
{ (void)d;(void)a;(void)tx;(void)txl;(void)rx;(void)rxl; return 0; }
int se05x_open_session(void) { return 0; }
void se05x_close_session(void) {}
int se05x_write_transparent(uint32_t oid, const uint8_t *d, uint16_t l)
{ (void)oid;(void)d;(void)l; return 0; }
int se05x_read_transparent(uint32_t oid, uint8_t *d, uint16_t *l)
{ (void)oid;(void)d;*l=0; return 0; }
int se05x_delete_object(uint32_t oid) { (void)oid; return 0; }
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

/* ========================================================================
 *  CCC Public API — for tests that don't compile real CCC sources
 * ======================================================================== */
#include "ccc_digital_key.h"

static bool g_ccc_initd = false;
ccc_status_t ccc_dk_init(void) { g_ccc_initd = true; return CCC_OK; }
ccc_status_t ccc_dk_deinit(void) { g_ccc_initd = false; return CCC_OK; }
ccc_status_t ccc_dk_run(void) { return CCC_OK; }
main_state_e ccc_dk_get_state(void) { return g_ccc_initd ? STATE_STANDBY : STATE_INIT; }
ccc_status_t ccc_dk_get_status(system_status_t *s) { memset(s,0,sizeof(*s)); return CCC_OK; }

ccc_status_t nfc_st25r501_init(void) { return CCC_OK; }
ccc_status_t nfc_st25r501_deinit(void) { return CCC_OK; }
bool nfc_field_detect(void) { return false; }
nfc_state_e nfc_get_state(void) { return NFC_STATE_IDLE; }
ccc_status_t nfc_start_listen(void) { return CCC_OK; }
ccc_status_t nfc_stop_listen(void) { return CCC_OK; }
ccc_status_t nfc_send(const uint8_t *d, uint16_t l) { (void)d;(void)l; return CCC_OK; }
ccc_status_t nfc_recv(uint8_t *b, uint16_t *l, uint32_t t) { (void)b;(void)l;(void)t; return CCC_OK; }
ccc_status_t nfc_oob_exchange(ccc_nfc_oob_data_t *o, ccc_nfc_oob_data_t *i)
{ (void)o;(void)i; return CCC_OK; }

ccc_status_t ble_kw47a_init(void) { return CCC_OK; }
ccc_status_t ble_kw47a_deinit(void) { return CCC_OK; }
ccc_status_t ble_start_adv(const ble_adv_param_t *p) { (void)p; return CCC_OK; }
ccc_status_t ble_stop_adv(void) { return CCC_OK; }
ccc_status_t ble_connect(const ble_addr_t *a, const ble_conn_param_t *p) { (void)a;(void)p; return CCC_OK; }
ccc_status_t ble_disconnect(uint16_t h) { (void)h; return CCC_OK; }
ccc_status_t ble_send(uint16_t h, const uint8_t *d, uint16_t l)
{ (void)h;(void)d;(void)l; return CCC_OK; }
ccc_status_t ble_register_recv_cb(ble_recv_cb_t c) { (void)c; return CCC_OK; }
ccc_status_t ble_register_conn_cb(ble_conn_cb_t c) { (void)c; return CCC_OK; }
ccc_status_t ble_register_disconn_cb(ble_disconn_cb_t c) { (void)c; return CCC_OK; }
ccc_status_t ble_oob_pair(const ccc_nfc_oob_data_t *o) { (void)o; return CCC_OK; }
bool ble_is_connected(uint16_t h) { (void)h; return false; }
ccc_status_t ble_register_gatt_service(void) { return CCC_OK; }
ccc_status_t ble_register_gatt_value_change_cb(void (*c)(uint16_t, const uint8_t*, uint16_t))
{ (void)c; return CCC_OK; }
ccc_status_t ble_gatt_notify(uint16_t u, const uint8_t *d, uint16_t l)
{ (void)u;(void)d;(void)l; return CCC_OK; }

ccc_status_t uwb_ncj29d6_init(void) { return CCC_OK; }
ccc_status_t uwb_ncj29d6_deinit(void) { return CCC_OK; }
uint32_t uwb_create_session(const uwb_session_config_t *c)
{ (void)c; static uint32_t n=100; return n++; }
ccc_status_t uwb_destroy_session(uint32_t s) { (void)s; return CCC_OK; }
ccc_status_t uwb_start_ranging(uint32_t s) { (void)s; return CCC_OK; }
ccc_status_t uwb_stop_ranging(uint32_t s) { (void)s; return CCC_OK; }
ccc_status_t uwb_get_distance(uint32_t s, uint16_t *d) { (void)s; *d=300; return CCC_OK; }
distance_zone_e uwb_get_zone(uint32_t s) { (void)s; return ZONE_UNLOCK; }
ccc_status_t uwb_set_threshold(const distance_threshold_t *t) { (void)t; return CCC_OK; }
ccc_status_t uwb_register_zone_cb(uwb_zone_cb_t c) { (void)c; return CCC_OK; }

ccc_status_t sec_init(void) { return CCC_OK; }
ccc_status_t sec_deinit(void) { return CCC_OK; }

/* In-memory key store for sec_store/load */
#define SEC_STORE_MAX 8
static struct { uint8_t id[16]; uint8_t data[256]; uint16_t len; } g_sec[SEC_STORE_MAX];
static int g_sec_n = 0;

static bool id_match16(const uint8_t *a, const uint8_t *b)
{
    for (int i=0; i<16; i++) {
        if (a[i]=='\0' && b[i]=='\0') return true;
        if (a[i]=='\0' || b[i]=='\0') return false;
        if (a[i]!=b[i]) return false;
    }
    return true;
}

ccc_status_t sec_store_key(const uint8_t *id, const uint8_t *d, uint16_t l)
{
    if (l>256) return CCC_ERR_INVALID_PARAM;
    if (g_sec_n>=SEC_STORE_MAX) return CCC_ERR_NO_MEM;
    memcpy(g_sec[g_sec_n].id, id, 16);
    memcpy(g_sec[g_sec_n].data, d, l);
    g_sec[g_sec_n].len = l;
    g_sec_n++;
    return CCC_OK;
}
ccc_status_t sec_load_key(const uint8_t *id, uint8_t *d, uint16_t *l)
{
    for (int i=0; i<g_sec_n; i++) {
        if (id_match16(g_sec[i].id, id)) {
            uint16_t cl = g_sec[i].len;
            if (*l < cl) cl = *l;
            memcpy(d, g_sec[i].data, cl);
            *l = g_sec[i].len;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}
ccc_status_t sec_delete_key(const uint8_t *id)
{
    for (int i=0; i<g_sec_n; i++) {
        if (id_match16(g_sec[i].id, id)) {
            memmove(&g_sec[i], &g_sec[i+1], (g_sec_n-i-1)*sizeof(g_sec[0]));
            g_sec_n--;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}
ccc_status_t sec_sign(const uint8_t *d, uint32_t l, uint8_t *s, uint32_t *sl)
{ (void)d;(void)l;(void)s; if(sl)*sl=64; return CCC_OK; }
verify_result_e sec_verify(const uint8_t *d, uint32_t l, const uint8_t *s, uint32_t sl)
{ (void)d;(void)l;(void)s;(void)sl; return VERIFY_OK; }

/* In-memory CCC digital key store */
#define KEY_STORE_MAX 8
static struct { ccc_digital_key_t key; } g_kstore[KEY_STORE_MAX];
static int g_kcount = 0;

static bool key_id_match(const uint8_t *a, const uint8_t *b)
{
    for (int i=0; i<KEY_ID_LEN; i++) {
        if (a[i]=='\0' && b[i]=='\0') return true;
        if (a[i]=='\0' || b[i]=='\0') return false;
        if (a[i]!=b[i]) return false;
    }
    return true;
}

ccc_status_t key_mgmt_init(void) { g_kcount=0; return CCC_OK; }
ccc_status_t key_mgmt_deinit(void) { g_kcount=0; return CCC_OK; }
ccc_status_t key_create(ccc_digital_key_t *k)
{
    if (g_kcount>=KEY_STORE_MAX) return CCC_ERR_NO_MEM;
    memcpy(&g_kstore[g_kcount].key, k, sizeof(ccc_digital_key_t));
    g_kcount++;
    return CCC_OK;
}
ccc_status_t key_delete(const uint8_t *id)
{
    for (int i=0; i<g_kcount; i++) {
        if (key_id_match(g_kstore[i].key.key_id, id)) {
            memmove(&g_kstore[i], &g_kstore[i+1], (g_kcount-i-1)*sizeof(g_kstore[0]));
            g_kcount--;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}
ccc_status_t key_get(const uint8_t *id, ccc_digital_key_t *k)
{
    for (int i=0; i<g_kcount; i++) {
        if (key_id_match(g_kstore[i].key.key_id, id)) {
            memcpy(k, &g_kstore[i].key, sizeof(ccc_digital_key_t));
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}
ccc_status_t key_list(ccc_digital_key_t *k, uint8_t *c)
{
    uint8_t max = (*c < (uint8_t)g_kcount) ? *c : (uint8_t)g_kcount;
    for (uint8_t i=0; i<max; i++) memcpy(&k[i], &g_kstore[i].key, sizeof(ccc_digital_key_t));
    *c = (uint8_t)g_kcount;
    return CCC_OK;
}
ccc_status_t key_share(const uint8_t *id, key_type_e t, uint32_t d)
{ (void)id;(void)t;(void)d; return CCC_OK; }
ccc_status_t key_revoke(const uint8_t *id)
{
    for (int i=0; i<g_kcount; i++) {
        if (key_id_match(g_kstore[i].key.key_id, id)) {
            g_kstore[i].key.state = KEY_STATE_REVOKED;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}
ccc_status_t key_suspend(const uint8_t *id)
{
    for (int i=0; i<g_kcount; i++) {
        if (key_id_match(g_kstore[i].key.key_id, id)) {
            g_kstore[i].key.state = KEY_STATE_SUSPENDED;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}
ccc_status_t key_resume(const uint8_t *id)
{
    for (int i=0; i<g_kcount; i++) {
        if (key_id_match(g_kstore[i].key.key_id, id)) {
            g_kstore[i].key.state = KEY_STATE_ACTIVE;
            return CCC_OK;
        }
    }
    return CCC_ERR_NOT_FOUND;
}
ccc_status_t key_validate(const uint8_t *id) { (void)id; return CCC_OK; }

/* ========================================================================
 *  Unified protocol API stubs (with minimal state for tests)
 * ======================================================================== */
#include "dk_unified.h"

static dk_device_type_t g_dk_dev;
static bool g_dk_initd = false;

dk_status_t dk_init(const dk_device_type_t *d) {
    if (!d) return DK_ERR_INVALID_PARAM;
    memcpy(&g_dk_dev, d, sizeof(g_dk_dev));
    g_dk_initd = true;
    return DK_OK;
}
dk_status_t dk_deinit(void) { g_dk_initd = false; return DK_OK; }
dk_status_t dk_get_status(dk_device_status_t *s) {
    memset(s,0,sizeof(*s));
    memcpy(&s->device, &g_dk_dev, sizeof(g_dk_dev));
    return DK_OK;
}
dk_status_t dk_run(void) { return DK_OK; }
dk_status_t dk_nfc_start_listen(void) { return DK_OK; }
dk_status_t dk_nfc_stop_listen(void) { return DK_OK; }
dk_status_t dk_ble_start_adv(dk_protocol_e p) { (void)p; return DK_OK; }
dk_status_t dk_ble_stop_adv(void) { return DK_OK; }
dk_status_t dk_ble_disconnect(void) { return DK_OK; }
dk_status_t dk_uwb_start_ranging(uint32_t *s) { (void)s; return DK_OK; }
dk_status_t dk_uwb_stop_ranging(uint32_t s) { (void)s; return DK_OK; }
dk_status_t dk_register_conn_cb(dk_conn_cb_t c, void *u) { (void)c;(void)u; return DK_OK; }
dk_status_t dk_auth_bind(dk_protocol_e p) { (void)p; return DK_OK; }
dk_status_t dk_auth_unbind(const uint8_t *k) { (void)k; return DK_OK; }
dk_status_t dk_auth_verify(void) { return DK_OK; }
bool dk_auth_check_permission(uint32_t p) { (void)p; return true; }
dk_status_t dk_register_auth_cb(dk_auth_cb_t c, void *u) { (void)c;(void)u; return DK_OK; }
/* In-memory unified key store */
#define DK_KEY_MAX 8
static struct { dk_key_t key; } g_dk_keys[DK_KEY_MAX];
static int g_dk_key_n = 0;

static bool dk_key_id_match(const uint8_t *a, const uint8_t *b)
{
    for (int i=0; i<16; i++) {
        if (a[i]=='\0' && b[i]=='\0') return true;
        if (a[i]=='\0' || b[i]=='\0') return false;
        if (a[i]!=b[i]) return false;
    }
    return true;
}

dk_status_t dk_key_create(dk_key_t *k) {
    if (g_dk_key_n >= DK_KEY_MAX) return DK_ERR_NO_MEM;
    memcpy(&g_dk_keys[g_dk_key_n].key, k, sizeof(dk_key_t));
    g_dk_key_n++;
    return DK_OK;
}
dk_status_t dk_key_delete(const uint8_t *id) {
    for (int i=0; i<g_dk_key_n; i++) {
        if (dk_key_id_match(g_dk_keys[i].key.key_id, id)) {
            memmove(&g_dk_keys[i], &g_dk_keys[i+1], (g_dk_key_n-i-1)*sizeof(g_dk_keys[0]));
            g_dk_key_n--;
            return DK_OK;
        }
    }
    return DK_ERR_NOT_FOUND;
}
dk_status_t dk_key_get(const uint8_t *id, dk_key_t *o) {
    for (int i=0; i<g_dk_key_n; i++) {
        if (dk_key_id_match(g_dk_keys[i].key.key_id, id)) {
            memcpy(o, &g_dk_keys[i].key, sizeof(dk_key_t));
            return DK_OK;
        }
    }
    return DK_ERR_NOT_FOUND;
}
dk_status_t dk_key_list(dk_key_t *k, uint8_t *c) {
    uint8_t max = (*c < (uint8_t)g_dk_key_n) ? *c : (uint8_t)g_dk_key_n;
    for (uint8_t i=0; i<max; i++) memcpy(&k[i], &g_dk_keys[i].key, sizeof(dk_key_t));
    *c = (uint8_t)g_dk_key_n;
    return DK_OK;
}
static dk_status_t dk_key_set_state(const uint8_t *id, dk_key_state_e st)
{
    for (int i=0; i<g_dk_key_n; i++) {
        if (dk_key_id_match(g_dk_keys[i].key.key_id, id)) {
            g_dk_keys[i].key.state = st;
            return DK_OK;
        }
    }
    return DK_ERR_NOT_FOUND;
}

dk_status_t dk_key_share(const uint8_t *id, dk_key_type_e t, uint32_t d)
{ (void)id;(void)t;(void)d; return DK_OK; }
dk_status_t dk_key_revoke(const uint8_t *id) { return dk_key_set_state(id, DK_KEY_STATE_REVOKED); }
dk_status_t dk_key_suspend(const uint8_t *id) { return dk_key_set_state(id, DK_KEY_STATE_SUSPENDED); }
dk_status_t dk_key_resume(const uint8_t *id) { return dk_key_set_state(id, DK_KEY_STATE_ACTIVE); }
dk_status_t dk_location_get(dk_location_t *l)
{ memset(l,0,sizeof(*l)); l->zone=DK_ZONE_LOCKED; return DK_OK; }
dk_status_t dk_zone_set_threshold(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{ (void)a;(void)b;(void)c;(void)d; return DK_OK; }
dk_status_t dk_register_location_cb(dk_location_cb_t c, void *u) { (void)c;(void)u; return DK_OK; }
dk_status_t dk_register_zone_cb(dk_zone_cb_t c, void *u) { (void)c;(void)u; return DK_OK; }
dk_status_t dk_vehicle_ctrl(dk_ctrl_cmd_e c, uint8_t p) { (void)c;(void)p; return DK_OK; }
dk_status_t dk_vehicle_get_status(dk_vehicle_status_t *s)
{ memset(s,0,sizeof(*s)); return DK_OK; }
dk_status_t dk_register_vehicle_cb(dk_vehicle_cb_t c, void *u) { (void)c;(void)u; return DK_OK; }
dk_status_t dk_protocol_send_raw(dk_protocol_e p, const uint8_t *d, uint16_t l)
{ (void)p;(void)d;(void)l; return DK_OK; }
dk_status_t dk_protocol_get_info(dk_protocol_e p, uint16_t *v, const char **n)
{ (void)p; *v=0x0300; *n="test"; return DK_OK; }
