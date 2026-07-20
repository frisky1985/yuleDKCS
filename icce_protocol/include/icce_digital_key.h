/**
 * @file icce_digital_key.h
 * @brief ICCE Digital Key Protocol Stack - Main Header
 * @version 1.0
 * @date 2026-05-06
 *
 * ICCE (International Connected Car Edge Computing) Digital Key
 * Focus: Edge computing + BLE 5.0 + UWB FiRa + Vehicle TCU integration
 */

#ifndef ICCE_DIGITAL_KEY_H
#define ICCE_DIGITAL_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================
 *  Return Codes
 * ======================================================================== */
#define ICCE_OK                (0)
#define ICCE_ERR_PARAM         (-1)
#define ICCE_ERR_NO_MEM        (-2)
#define ICCE_ERR_TIMEOUT       (-3)
#define ICCE_ERR_BUSY          (-4)
#define ICCE_ERR_NOT_INIT      (-5)
#define ICCE_ERR_HARDWARE      (-6)
#define ICCE_ERR_SECURITY      (-7)
#define ICCE_ERR_NOT_FOUND     (-8)
#define ICCE_ERR_EDGE          (-9)
#define ICCE_ERR_CLOUD         (-10)

/* ========================================================================
 *  BLE 5.0 Communication
 * ======================================================================== */
#define ICCE_BLE_MAX_PAYLOAD   244
#define ICCE_BLE_SERVICE_UUID  0xFEFA

typedef void (*icce_ble_recv_cb_t)(const uint8_t *data, uint16_t len);

int32_t icce_ble_init(void);
int32_t icce_ble_deinit(void);
int32_t icce_ble_start_adv(void);
int32_t icce_ble_stop_adv(void);
int32_t icce_ble_send(const uint8_t *data, uint16_t len);
int32_t icce_ble_register_cb(icce_ble_recv_cb_t cb);

/* ========================================================================
 *  UWB FiRa Module
 * ======================================================================== */
#define ICCE_UWB_MAX_RANGING_SESSIONS  4
#define ICCE_UWB_MAX_ANCHORS           8

typedef enum {
    ICCE_UWB_ROLE_CONTROLLER = 0,
    ICCE_UWB_ROLE_RESPONDER  = 1
} icce_uwb_role_e;

typedef struct {
    uint16_t session_id;
    uint8_t  channel;         /* 5, 6, 8, 9 */
    uint8_t  preamble_code;
    uint8_t  role;            /* icce_uwb_role_e */
    uint8_t  mac_mode;        /* 0=SP1, 1=SP3 */
    uint8_t  ranging_round;   /* 0-255 */
    int32_t  distance_mm;
    uint16_t angle_azimuth;
    uint16_t angle_elevation;
    uint8_t  quality;         /* 0-100 */
} icce_uwb_session_t;

typedef void (*icce_uwb_ranging_cb_t)(const icce_uwb_session_t *session);

int32_t icce_uwb_init(void);
int32_t icce_uwb_deinit(void);
int32_t icce_uwb_start_session(uint16_t session_id, icce_uwb_role_e role, uint8_t channel);
int32_t icce_uwb_stop_session(uint16_t session_id);
int32_t icce_uwb_get_ranging(uint16_t session_id, icce_uwb_session_t *out);
int32_t icce_uwb_register_cb(icce_uwb_ranging_cb_t cb);

/* ========================================================================
 *  Edge Computing Module
 * ======================================================================== */
#define ICCE_EDGE_MAX_RULES     32
#define ICCE_EDGE_MAX_ACTIONS   8

typedef enum {
    ICCE_ACTION_UNLOCK   = 0x01,
    ICCE_ACTION_LOCK     = 0x02,
    ICCE_ACTION_START    = 0x03,
    ICCE_ACTION_STOP     = 0x04,
    ICCE_ACTION_CLIMATE  = 0x05,
    ICCE_ACTION_LIGHTS   = 0x06,
    ICCE_ACTION_HORN     = 0x07,
    ICCE_ACTION_CUSTOM   = 0xFF
} icce_action_e;

typedef enum {
    ICCE_TRIGGER_ZONE_ENTER  = 0x01,
    ICCE_TRIGGER_ZONE_EXIT   = 0x02,
    ICCE_TRIGGER_DISTANCE    = 0x03,
    ICCE_TRIGGER_TIME        = 0x04,
    ICCE_TRIGGER_GESTURE     = 0x05,
    ICCE_TRIGGER_BLE_RSSI    = 0x06
} icce_trigger_e;

typedef struct {
    icce_trigger_e trigger;
    uint8_t        zone_id;
    int32_t        threshold_mm;   /* distance threshold */
    uint32_t       time_mask;      /* bitmask: which hours valid */
    icce_action_e  actions[ICCE_EDGE_MAX_ACTIONS];
    uint8_t        action_count;
    uint8_t        priority;       /* 0=low, 255=critical */
    bool           enabled;
} icce_edge_rule_t;

int32_t icce_edge_init(void);
int32_t icce_edge_deinit(void);
int32_t icce_edge_add_rule(const icce_edge_rule_t *rule);
int32_t icce_edge_remove_rule(uint8_t rule_id);
int32_t icce_edge_enable_rule(uint8_t rule_id, bool enable);
int32_t icce_edge_process_trigger(icce_trigger_e trigger, const void *data, uint16_t len);
int32_t icce_edge_evaluate(int32_t distance_mm, int16_t rssi, uint8_t zone);

/* ========================================================================
 *  Zone Management
 * ======================================================================== */
#define ICCE_ZONE_MAX  8

typedef enum {
    ICCE_ZONE_NONE     = 0,
    ICCE_ZONE_FAR      = 1,    /* >20m: wake BLE */
    ICCE_ZONE_MID      = 2,    /* 10-20m: start UWB */
    ICCE_ZONE_NEAR     = 3,    /* 3-10m: approach */
    ICCE_ZONE_VICINITY = 4,    /* 1-3m: unlock ready */
    ICCE_ZONE_INTERIOR = 5     /* <1m: inside vehicle */
} icce_zone_e;

typedef struct {
    icce_zone_e zone;
    int32_t     inner_mm;
    int32_t     outer_mm;
    uint8_t     actions_mask;   /* bitmask of allowed actions */
} icce_zone_def_t;

int32_t icce_zone_init(void);
icce_zone_e icce_zone_classify(int32_t distance_mm);
int32_t icce_zone_get_def(icce_zone_e zone, icce_zone_def_t *out);

/* ========================================================================
 *  Vehicle TCU Integration
 * ======================================================================== */
typedef struct __attribute__((packed)) {
    uint8_t  lock_status;
    uint8_t  engine_status;
    uint8_t  door_status;
    uint8_t  window_status;
    int8_t   battery_pct;
    int16_t  interior_temp;
    uint16_t odometer_km;
    uint8_t  alarm_status;
    uint8_t  fuel_level;
    uint8_t  reserved[5];
} icce_vehicle_status_t;

typedef void (*icce_vehicle_status_cb_t)(const icce_vehicle_status_t *status);

int32_t icce_vehicle_init(void);
int32_t icce_vehicle_ctrl(icce_action_e action, uint8_t param);
int32_t icce_vehicle_get_status(icce_vehicle_status_t *status);
int32_t icce_vehicle_register_cb(icce_vehicle_status_cb_t cb);

/* ========================================================================
 *  Security & Key Management
 * ======================================================================== */
int32_t icce_security_init(void);
int32_t icce_security_bind(const uint8_t *device_pubkey, uint16_t len);
int32_t icce_security_auth(const uint8_t *challenge, uint16_t chal_len,
                           const uint8_t *signature, uint16_t sig_len);
int32_t icce_security_verify_session(uint16_t session_id);

/* ========================================================================
 *  Main Entry
 * ======================================================================== */
int32_t icce_dk_init(void);
int32_t icce_dk_deinit(void);
int32_t icce_dk_run(void);

#ifdef __cplusplus
}
#endif

#endif /* ICCE_DIGITAL_KEY_H */
