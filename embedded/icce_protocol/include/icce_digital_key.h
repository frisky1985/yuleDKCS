/**
 * @file icce_digital_key.h
 * @brief ICCE Digital Key Protocol — Type Definitions & Public API
 *
 * Defines all ICCE types, enums, error codes, and public function declarations
 * for the vehicle-side digital key implementation.
 *
 * Architecture layers (top-down):
 *   icce_dk_core       — Main loop, dispatch, lifecycle
 *   icce_edge          — Edge computing engine (rules, triggers, state machine)
 *   icce_zone          — Distance-based zone classification
 *   icce_security      — Binding, auth, session verification
 *   icce_vehicle       — CAN vehicle control & status
 *   icce_uwb           — NXP NCJ29D6 UWB ranging (shared with CCC)
 *   ble_manager        — NXP KW47A BLE communication
 *   offline_decision   — Local authentication decision engine
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
 *  ICCE Status / Error Codes
 * ======================================================================== */
#define ICCE_OK                 0
#define ICCE_ERR_PARAM         -1
#define ICCE_ERR_NOT_FOUND     -2
#define ICCE_ERR_BUSY          -3
#define ICCE_ERR_NO_MEM        -4
#define ICCE_ERR_SECURITY      -5

/* ========================================================================
 *  Zone Enumeration
 * ======================================================================== */
typedef enum {
    ICCE_ZONE_NONE     = 0,   /**< Out of range / unknown */
    ICCE_ZONE_FAR      = 1,   /**< 20m–50m: FIND only */
    ICCE_ZONE_MID      = 2,   /**< 10m–20m: FIND + BLE conn */
    ICCE_ZONE_NEAR     = 3,   /**< 2m–10m: approach notify */
    ICCE_ZONE_VICINITY = 4,   /**< 1m–2m: UNLOCK/LOCK */
    ICCE_ZONE_INTERIOR = 5,   /**< 0–1m: all actions (START, etc.) */
    ICCE_ZONE_MAX      = 8    /**< Array sizing slot count (not a real zone) */
} icce_zone_e;

/** Zone definition (distance bounds and allowed action mask) */
typedef struct {
    icce_zone_e zone;
    uint32_t    inner_mm;        /**< Inner boundary in mm */
    uint32_t    outer_mm;        /**< Outer boundary in mm */
    uint8_t     actions_mask;    /**< Bitmask of allowed ICCE actions */
} icce_zone_def_t;

/* ========================================================================
 *  Trigger Enumeration
 * ======================================================================== */
typedef enum {
    ICCE_TRIGGER_ZONE_ENTER    = 0,  /**< Device entered a new zone */
    ICCE_TRIGGER_ZONE_EXIT     = 1,  /**< Device left a zone */
    ICCE_TRIGGER_DISTANCE      = 2,  /**< Absolute distance threshold (mm) */
    ICCE_TRIGGER_BLE_RSSI      = 3,  /**< BLE RSSI level (dBm) */
    ICCE_TRIGGER_UWB_RANGE     = 4,  /**< UWB ranging result (mm + quality) */
    ICCE_TRIGGER_VEHICLE_STATE = 5,  /**< Vehicle status change (engine/door/lock) */
    ICCE_TRIGGER_TIME_INTERVAL = 6,  /**< Periodic timer-based trigger */
    ICCE_TRIGGER_COMPOUND      = 7,  /**< Composite condition (AND/OR of sub-conditions) */
    ICCE_TRIGGER_MAX           = 8
} icce_trigger_e;

/* ========================================================================
 *  Action Enumeration
 * ======================================================================== */
typedef enum {
    ICCE_ACTION_UNLOCK   = 0,
    ICCE_ACTION_LOCK     = 1,
    ICCE_ACTION_START    = 2,
    ICCE_ACTION_STOP     = 3,
    ICCE_ACTION_CLIMATE  = 4,
    ICCE_ACTION_LIGHTS   = 5,
    ICCE_ACTION_HORN     = 6,
    ICCE_ACTION_CUSTOM   = 7,
    ICCE_ACTION_MAX      = 8
} icce_action_e;

/* ========================================================================
 *  Condition Types (for composite triggers)
 * ======================================================================== */

/** Condition operator for composite expressions */
typedef enum {
    COND_OP_NONE      = 0,
    COND_OP_AND       = 1,   /**< All sub-conditions must be true */
    COND_OP_OR        = 2,   /**< Any sub-condition must be true */
    COND_OP_NOT       = 3,   /**< Invert sub-condition result */
    COND_OP_RSSI_GT   = 4,   /**< RSSI > threshold (dBm, e.g. -70) */
    COND_OP_RSSI_LT   = 5,   /**< RSSI < threshold */
    COND_OP_DIST_GT   = 6,   /**< Distance > threshold (mm) */
    COND_OP_DIST_LT   = 7,   /**< Distance < threshold */
    COND_OP_ZONE_EQ   = 8,   /**< Zone == value */
    COND_OP_VEHICLE_STOPPED = 9,  /**< Vehicle engine == off && speed == 0 */
    COND_OP_VEHICLE_LOCKED  = 10, /**< Vehicle lock status == locked */
    COND_OP_VEHICLE_PARKED  = 11, /**< Gear == Park */
    COND_OP_TIME_IN_WINDOW  = 12, /**< Current time within time_mask window */
} icce_condition_op_e;

/** A single condition leaf or composite node */
typedef struct icce_condition {
    icce_condition_op_e op;          /**< Operator type */
    int32_t             threshold;   /**< Numeric threshold (RSSI dBm, distance mm, etc.) */
    uint8_t             zone_id;     /**< Zone ID for ZONE_EQ conditions */
    struct icce_condition *left;     /**< Left sub-condition (for AND/OR/NOT) */
    struct icce_condition *right;    /**< Right sub-condition (for AND/OR) */
} icce_condition_t;

/* ========================================================================
 *  Edge Computing Rule
 * ======================================================================== */

/** Maximum actions per rule */
#define ICCE_EDGE_RULE_MAX_ACTIONS   4
/** Maximum number of edge rules */
#define ICCE_EDGE_MAX_RULES         16
/** Maximum depth of nested compound conditions */
#define ICCE_EDGE_MAX_COND_DEPTH     3

/** Edge computing rule — maps a trigger + optional conditions to one or more actions */
typedef struct {
    icce_trigger_e     trigger;          /**< Trigger type */
    uint8_t            zone_id;          /**< Target zone (for zone triggers) */
    int32_t            threshold_mm;     /**< Distance threshold in mm (for DISTANCE triggers) */
    int32_t            threshold_rssi;   /**< RSSI threshold in dBm (for BLE_RSSI triggers) */
    uint32_t           time_mask;        /**< Bitmask of hours when rule is active (bit 0 = 00:00–01:00) */
    uint32_t           interval_ms;      /**< Polling interval for TIME_INTERVAL triggers */
    icce_action_e      actions[ICCE_EDGE_RULE_MAX_ACTIONS]; /**< Actions to execute */
    uint8_t            action_count;     /**< Number of valid actions */
    uint8_t            priority;         /**< Priority (higher = more important, 0–255) */
    bool               enabled;          /**< Rule enabled flag */

    /* Compound condition support */
    icce_condition_t   condition;        /**< Root of condition tree (for COMPOUND triggers) */

    /* Rule metadata */
    uint32_t           cooldown_ms;      /**< Minimum time between re-triggers */
    uint32_t           last_triggered;   /**< System tick of last trigger (internal) */
} icce_edge_rule_t;

/* ========================================================================
 *  Edge Engine State Machine
 * ======================================================================== */
typedef enum {
    ICCE_EDGE_STATE_IDLE        = 0,  /**< Engine initialized, not monitoring */
    ICCE_EDGE_STATE_MONITORING  = 1,  /**< Actively evaluating triggers */
    ICCE_EDGE_STATE_TRIGGERED   = 2,  /**< Trigger fired, executing actions */
    ICCE_EDGE_STATE_ACTIVE      = 3,  /**< Actions executed, monitoring hold */
    ICCE_EDGE_STATE_FALLBACK    = 4,  /**< Recovery / fallback state */
    ICCE_EDGE_STATE_MAX         = 5
} icce_edge_state_e;

/** State transition event type */
typedef enum {
    STATE_EVENT_INIT        = 0,
    STATE_EVENT_START       = 1,
    STATE_EVENT_TRIGGER     = 2,
    STATE_EVENT_SUCCESS     = 3,
    STATE_EVENT_FAILURE     = 4,
    STATE_EVENT_TIMEOUT     = 5,
    STATE_EVENT_STOP        = 6,
    STATE_EVENT_FALLBACK    = 7,
} icce_edge_state_event_e;

/* ========================================================================
 *  Vehicle Status & Callback
 * ======================================================================== */
typedef struct {
    uint8_t  lock_status;      /**< 0x00 = unlocked, 0x01 = locked */
    uint8_t  engine_status;    /**< 0x00 = off, 0x01 = on */
    uint8_t  door_status;      /**< Bitmask: bit 0 = driver, bit 1 = passenger, etc. */
    uint8_t  window_status;    /**< Bitmask */
    uint8_t  battery_pct;      /**< 0–100 */
    int8_t   interior_temp;    /**< Celsius */
    uint32_t odometer_km;      /**< Odometer in km */
    uint8_t  alarm_status;     /**< 0x00 = disarmed, 0x01 = armed */
    uint8_t  fuel_level;       /**< 0–100 */
    uint8_t  gear_position;    /**< 0 = Park, 1 = Reverse, 2 = Neutral, 3 = Drive */
    uint8_t  speed_kmh;        /**< Current speed in km/h */
    uint8_t  reserved[4];
} icce_vehicle_status_t;

/** Vehicle status change callback */
typedef void (*icce_vehicle_status_cb_t)(const icce_vehicle_status_t *status);

/* ========================================================================
 *  UWB / BLE Types
 * ======================================================================== */
typedef struct {
    uint16_t session_id;
    uint8_t  channel;
    uint8_t  role;             /**< 0 = initiator, 1 = responder */
    uint8_t  mac_mode;         /**< 0 = SP0, 1 = SP3 */
    int32_t  distance_mm;      /**< Ranging distance in mm */
    uint8_t  quality;          /**< Ranging quality 0–255 */
} icce_uwb_session_t;

#define ICCE_UWB_MAX_RANGING_SESSIONS  4

/** UWB ranging callback */
typedef void (*icce_uwb_ranging_cb_t)(const icce_uwb_session_t *session);

/* ========================================================================
 *  Edge Engine Public API
 * ======================================================================== */
int32_t icce_edge_init(void);
int32_t icce_edge_deinit(void);
int32_t icce_edge_add_rule(const icce_edge_rule_t *rule);
int32_t icce_edge_remove_rule(uint8_t rule_id);
int32_t icce_edge_enable_rule(uint8_t rule_id, bool enable);
int32_t icce_edge_process_trigger(icce_trigger_e trigger, const void *data, uint16_t len);
int32_t icce_edge_evaluate(int32_t distance_mm, int16_t rssi, uint8_t zone);
int32_t icce_edge_get_state(icce_edge_state_e *state);
int32_t icce_edge_timer_tick(uint32_t elapsed_ms);
int32_t icce_edge_update_rssi(int16_t rssi);
int32_t icce_edge_update_vehicle_state(const icce_vehicle_status_t *status);

/* [P1-2] Internal accessor for NVM save — returns pointer to internal rule array */
icce_edge_rule_t *icce_edge_get_rule_array(uint8_t *count_out);

/* ========================================================================
 *  Zone Management API
 * ======================================================================== */
int32_t     icce_zone_init(void);
icce_zone_e icce_zone_classify(int32_t distance_mm);
int32_t     icce_zone_get_def(icce_zone_e zone, icce_zone_def_t *out);

/* ========================================================================
 *  Vehicle Control API
 * ======================================================================== */
int32_t icce_vehicle_init(void);
int32_t icce_vehicle_ctrl(icce_action_e action, uint8_t param);
int32_t icce_vehicle_get_status(icce_vehicle_status_t *status);
int32_t icce_vehicle_register_cb(icce_vehicle_status_cb_t cb);

/* ========================================================================
 *  Security API
 * ======================================================================== */
int32_t icce_security_init(void);
int32_t icce_security_bind(const uint8_t *device_pubkey, uint16_t len);
int32_t icce_security_auth(const uint8_t *challenge, uint16_t chal_len,
                           const uint8_t *signature, uint16_t sig_len);
int32_t icce_security_verify_session(uint16_t session_id);
int32_t icce_security_check_engine_start_perm(const uint8_t *device_pubkey, uint16_t key_len);

/* ========================================================================
 *  UWB API
 * ======================================================================== */
int32_t icce_uwb_init(void);
int32_t icce_uwb_deinit(void);
int32_t icce_uwb_start_session(uint16_t session_id, uint8_t role, uint8_t channel);
int32_t icce_uwb_stop_session(uint16_t session_id);
int32_t icce_uwb_get_ranging(uint16_t session_id, icce_uwb_session_t *out);
int32_t icce_uwb_register_cb(icce_uwb_ranging_cb_t cb);

/* ========================================================================
 *  BLE API
 * ======================================================================== */
int32_t icce_ble_init(void);
int32_t icce_ble_deinit(void);
int32_t icce_ble_start_adv(void);
int32_t icce_ble_stop_adv(void);
int32_t icce_ble_register_cb(void (*cb)(const uint8_t *data, uint16_t len));

/* ========================================================================
 *  Digital Key Core API
 * ======================================================================== */
void    icce_dk_on_wakeup(uint32_t wakeup_source);
void    icce_dk_on_sleep(void);
void    icce_dk_early_init(void);
void    icce_dk_late_init(void);
int32_t icce_dk_init(void);
int32_t icce_dk_deinit(void);
int32_t icce_dk_run(void);

#ifdef __cplusplus
}
#endif

#endif /* ICCE_DIGITAL_KEY_H */
