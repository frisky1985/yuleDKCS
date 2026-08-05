/**
 * @file icce_edge.c
 * @brief ICCE Edge Computing Engine — Rule Engine, Trigger Processing & State Machine
 *
 * == Overview ==
 * The edge engine is ICCE's core differentiator: it makes local decisions without
 * cloud round-trip latency.  This implementation provides three trigger families
 * and a 5-state machine.
 *
 * == Trigger Families ==
 *
 * 1. Event Triggers (immediate sensor data):
 *    - ICCE_TRIGGER_BLE_RSSI      — BLE RSSI crosses configurable thresholds
 *    - ICCE_TRIGGER_UWB_RANGE     — UWB ranging result (distance + quality)
 *    - ICCE_TRIGGER_VEHICLE_STATE — Vehicle engine/door/lock/gear changes
 *    - ICCE_TRIGGER_ZONE_ENTER    — Device enters a distance zone (existing)
 *    - ICCE_TRIGGER_ZONE_EXIT     — Device exits a distance zone (existing)
 *    - ICCE_TRIGGER_DISTANCE      — Absolute distance threshold (existing)
 *
 * 2. Time Triggers (periodic):
 *    - ICCE_TRIGGER_TIME_INTERVAL — Configurable polling interval
 *    - Engine calls icce_edge_timer_tick() from main loop
 *
 * 3. Condition Triggers (composite):
 *    - ICCE_TRIGGER_COMPOUND      — AND/OR/NOT tree of sub-conditions
 *    - Example: (RSSI > -70dBm && vehicle_stopped) → UNLOCK
 *
 * == State Machine ==
 *   IDLE → MONITORING → TRIGGERED → ACTIVE → (timeout/exit) → MONITORING
 *              ↓                                              ↑
 *          FALLBACK ← (failure) ← TRIGGERED ← (failure) ──────┘
 *
 * States:
 *   IDLE        — Engine initialized, no monitoring active
 *   MONITORING  — Normal runtime: evaluating rules against incoming data
 *   TRIGGERED   — A matching rule fired, executing action sequence
 *   ACTIVE      — Actions completed; waiting for exit condition or timeout
 *   FALLBACK    — Execution failed; recovery/retry logic
 *
 * == Version History ==
 * [ICCE-EDGE-v2] Complete rewrite with real trigger logic, state machine,
 *                composite conditions, and time-based evaluation.
 */

#include "icce_digital_key.h"
#include "edge_condition.h"
#include "sys_time.h"

/* ========================================================================
 *  Internal Constants
 * ======================================================================== */

/** Default cooldown between re-triggers (3 seconds) */
#define DEFAULT_COOLDOWN_MS        3000U

/** Timeout from ACTIVE back to MONITORING (30 seconds) */
#define ACTIVE_TIMEOUT_MS          30000U

/** Timeout from FALLBACK retry (5 seconds) */
#define FALLBACK_RETRY_MS          5000U

/** Maximum action execution attempts */
#define MAX_ACTION_RETRIES         3

/** Number of RSSI samples for moving average */
#define RSSI_MA_SAMPLES            5

/** Number of UWB distance samples for smoothing */
#define UWB_MA_SAMPLES             3

/* ========================================================================
 *  Internal State
 * ======================================================================== */

/** Edge engine instance */
typedef struct {
    /* Rule storage */
    icce_edge_rule_t    rules[ICCE_EDGE_MAX_RULES];
    uint8_t             rule_count;

    /* State machine */
    icce_edge_state_e   state;
    uint32_t            state_enter_tick;   /**< sys_tick when current state was entered */
    uint32_t            last_tick;          /**< Last timer_tick call time */

    /* Sensor state (latest values) */
    int32_t             current_distance_mm;
    int16_t             current_rssi;
    uint8_t             current_zone;
    icce_vehicle_status_t vehicle_status;

    /* RSSI moving average buffer */
    int16_t             rssi_samples[RSSI_MA_SAMPLES];
    uint8_t             rssi_sample_idx;
    int16_t             rssi_ma;            /**< Moving average RSSI */

    /* UWB distance moving average buffer */
    int32_t             uwb_samples[UWB_MA_SAMPLES];
    uint8_t             uwb_sample_idx;
    int32_t             uwb_ma;             /**< Moving average distance */

    /* Action execution state */
    uint8_t             active_rule_idx;    /**< Which rule is currently active */
    uint8_t             action_retries;

    /* Flags */
    bool                initialized;
} edge_engine_t;

/** Global singleton */
static edge_engine_t g_engine = {0};

/* ========================================================================
 *  Forward Declarations
 * ======================================================================== */
static bool evaluate_single_rule(const icce_edge_rule_t *rule);
static bool evaluate_condition_tree(const icce_condition_t *cond);
static int32_t execute_rule_actions(const icce_edge_rule_t *rule);
static void transition_state(icce_edge_state_e new_state);
static bool is_rule_in_time_window(const icce_edge_rule_t *rule);
static bool is_rule_in_cooldown(const icce_edge_rule_t *rule, uint32_t now);

/* ========================================================================
 *  Internal forward-declared accessor for NVM save
 * ======================================================================== */

/**
 * icce_edge_get_rule_info — retrieve a pointer to the internal rule array.
 * Internal use only (used by edge_condition_save_rules_to_nvm).
 */
icce_edge_rule_t *icce_edge_get_rule_array(uint8_t *count_out)
{
    if (!g_engine.initialized) {
        if (count_out) *count_out = 0;
        return NULL;
    }
    if (count_out) *count_out = g_engine.rule_count;
    return g_engine.rules;
}

/* ========================================================================
 *  Public API
 * ======================================================================== */

int32_t icce_edge_init(void)
{
    if (g_engine.initialized) {
        return ICCE_OK;
    }

    (void)memset(&g_engine, 0, sizeof(g_engine));

    /* Initialize the dynamic condition pool */
    edge_condition_pool_init();

    /* --- Attempt NVM loading first --- */
    /* Try storage handle 0 (default). If NVM has a config, it takes priority. */
    int32_t nvm_ret = edge_condition_load_rules_from_nvm(0);
    if (nvm_ret == ICCE_OK) {
        /* NVM rules loaded successfully — engine setup below (no static rules) */
    } else {
        /* --- Fallback: Default static rules --- */
        /* Rule 0: Enter VICINITY → UNLOCK (cooldown 3s, priority 128) */
        g_engine.rules[0] = (icce_edge_rule_t){
            .trigger        = ICCE_TRIGGER_ZONE_ENTER,
            .zone_id        = ICCE_ZONE_VICINITY,
            .threshold_mm   = 2000,
            .time_mask      = 0xFFFFFF,
            .actions        = { ICCE_ACTION_UNLOCK },
            .action_count   = 1,
            .priority       = 128,
            .enabled        = true,
            .cooldown_ms    = DEFAULT_COOLDOWN_MS,
            .condition.op   = COND_OP_NONE
        };

        /* Rule 1: Exit NEAR → LOCK (cooldown 5s, priority 128) */
        g_engine.rules[1] = (icce_edge_rule_t){
            .trigger        = ICCE_TRIGGER_ZONE_EXIT,
            .zone_id        = ICCE_ZONE_NEAR,
            .threshold_mm   = 10000,
            .time_mask      = 0xFFFFFF,
            .actions        = { ICCE_ACTION_LOCK },
            .action_count   = 1,
            .priority       = 128,
            .enabled        = true,
            .cooldown_ms    = 5000,
            .condition.op   = COND_OP_NONE
        };

        /* Rule 2: Enter INTERIOR → START (priority 200, requires vehicle stopped) */
        /* Compound: zone_enter_interior AND vehicle_is_parked.
         * Note: Uses edge_condition_create_* to avoid dangling pointers from
         * compound literal addresses. This is the P1-2 fix. */
        {
            icce_condition_t *zone_eq = edge_condition_create_leaf(
                COND_OP_ZONE_EQ, 0, ICCE_ZONE_INTERIOR);
            icce_condition_t *parked = edge_condition_create_leaf(
                COND_OP_VEHICLE_PARKED, 0, 0);
            icce_condition_t *composite = edge_condition_create_composite(
                COND_OP_AND, zone_eq, parked);

            if (composite) {
                g_engine.rules[2] = (icce_edge_rule_t){
                    .trigger        = ICCE_TRIGGER_ZONE_ENTER,
                    .zone_id        = ICCE_ZONE_INTERIOR,
                    .threshold_mm   = 1000,
                    .time_mask      = 0xFFFFFF,
                    .actions        = { ICCE_ACTION_START },
                    .action_count   = 1,
                    .priority       = 200,
                    .enabled        = true,
                    .cooldown_ms    = DEFAULT_COOLDOWN_MS,
                };
                /* Copy the dynamically allocated condition into the rule */
                memcpy(&g_engine.rules[2].condition, composite,
                       sizeof(icce_condition_t));
            } else {
                /* Pool exhausted — fallback to simple rule without condition */
                g_engine.rules[2] = (icce_edge_rule_t){
                    .trigger        = ICCE_TRIGGER_ZONE_ENTER,
                    .zone_id        = ICCE_ZONE_INTERIOR,
                    .threshold_mm   = 1000,
                    .time_mask      = 0xFFFFFF,
                    .actions        = { ICCE_ACTION_START },
                    .action_count   = 1,
                    .priority       = 200,
                    .enabled        = true,
                    .cooldown_ms    = DEFAULT_COOLDOWN_MS,
                    .condition.op   = COND_OP_NONE
                };
            }
        }

        /* Rule 3: BLE RSSI > -70dBm → approach notification */
        g_engine.rules[3] = (icce_edge_rule_t){
            .trigger        = ICCE_TRIGGER_BLE_RSSI,
            .threshold_rssi = -70,
            .time_mask      = 0xFFFFFF,
            .actions        = { ICCE_ACTION_LIGHTS },
            .action_count   = 1,
            .priority       = 100,
            .enabled        = true,
            .cooldown_ms    = DEFAULT_COOLDOWN_MS,
            .condition.op   = COND_OP_NONE
        };

        /* Rule 4: Time-based status sync every 60 seconds */
        g_engine.rules[4] = (icce_edge_rule_t){
            .trigger        = ICCE_TRIGGER_TIME_INTERVAL,
            .interval_ms    = 60000,
            .time_mask      = 0xFFFFFF,
            .actions        = { ICCE_ACTION_CUSTOM },
            .action_count   = 1,
            .priority       = 50,
            .enabled        = true,
            .cooldown_ms    = 0,
            .condition.op   = COND_OP_NONE
        };

        g_engine.rule_count = 5;
    }

    /* Initialize sensor state */
    g_engine.current_distance_mm = -1;
    g_engine.current_rssi        = -127;
    g_engine.current_zone        = ICCE_ZONE_NONE;
    (void)memset(&g_engine.vehicle_status, 0, sizeof(g_engine.vehicle_status));

    /* Initialize moving averages */
    for (uint8_t i = 0; i < RSSI_MA_SAMPLES; i++) {
        g_engine.rssi_samples[i] = -127;
    }
    g_engine.rssi_ma = -127;

    for (uint8_t i = 0; i < UWB_MA_SAMPLES; i++) {
        g_engine.uwb_samples[i] = -1;
    }
    g_engine.uwb_ma = -1;

    /* Start in IDLE; caller transitions to MONITORING */
    g_engine.state = ICCE_EDGE_STATE_IDLE;
    g_engine.state_enter_tick = 0;
    g_engine.last_tick = 0;
    g_engine.initialized = true;

    return ICCE_OK;
}

int32_t icce_edge_deinit(void)
{
    /* Free all condition trees in rules before clearing */
    for (uint8_t i = 0; i < g_engine.rule_count; i++) {
        if (g_engine.rules[i].condition.op != COND_OP_NONE) {
            /* Free the condition tree. Since the condition is embedded in the rule
             * struct (not a pointer), we check if its children are pool-allocated.
             * The edge_condition_free_tree handles both pool and static nodes safely. */
            edge_condition_free_tree(g_engine.rules[i].condition.left);
            edge_condition_free_tree(g_engine.rules[i].condition.right);
        }
    }

    g_engine.initialized = false;
    g_engine.rule_count = 0;
    g_engine.state = ICCE_EDGE_STATE_IDLE;
    return ICCE_OK;
}

int32_t icce_edge_add_rule(const icce_edge_rule_t *rule)
{
    if (!rule || !g_engine.initialized) return ICCE_ERR_PARAM;
    if (g_engine.rule_count >= ICCE_EDGE_MAX_RULES) return ICCE_ERR_NO_MEM;

    g_engine.rules[g_engine.rule_count++] = *rule;
    return ICCE_OK;
}

int32_t icce_edge_remove_rule(uint8_t rule_id)
{
    if (!g_engine.initialized) return ICCE_ERR_PARAM;
    if (rule_id >= g_engine.rule_count) return ICCE_ERR_NOT_FOUND;

    for (uint8_t i = rule_id; i < g_engine.rule_count - 1; i++) {
        g_engine.rules[i] = g_engine.rules[i + 1];
    }
    g_engine.rule_count--;
    return ICCE_OK;
}

int32_t icce_edge_enable_rule(uint8_t rule_id, bool enable)
{
    if (!g_engine.initialized) return ICCE_ERR_PARAM;
    if (rule_id >= g_engine.rule_count) return ICCE_ERR_NOT_FOUND;

    g_engine.rules[rule_id].enabled = enable;
    return ICCE_OK;
}

int32_t icce_edge_get_state(icce_edge_state_e *state)
{
    if (!state || !g_engine.initialized) return ICCE_ERR_PARAM;
    *state = g_engine.state;
    return ICCE_OK;
}

/* ========================================================================
 *  icce_edge_process_trigger — Handle event-based triggers
 *
 *  Called by the core dispatcher when a discrete trigger event occurs
 *  (zone enter/exit, BLE data, UWB ranging, vehicle state, etc.).
 *
 *  For ICCE_TRIGGER_COMPOUND, the data pointer may point to a trigger_data_t
 *  containing the sensor snapshot for the compound condition evaluation.
 * ======================================================================== */

/* Compound trigger data passed in via data pointer */
typedef struct {
    int32_t  distance_mm;
    int16_t  rssi;
    uint8_t  zone;
    icce_vehicle_status_t vehicle;
} trigger_data_t;

int32_t icce_edge_process_trigger(icce_trigger_e trigger, const void *data, uint16_t len)
{
    if (!g_engine.initialized) return ICCE_ERR_PARAM;

    /* If not monitoring, silently ignore triggers (edge engine not active) */
    if (g_engine.state != ICCE_EDGE_STATE_MONITORING &&
        g_engine.state != ICCE_EDGE_STATE_ACTIVE) {
        return ICCE_OK;
    }

    /* Populate trigger data from caller */
    trigger_data_t tdata;
    (void)memset(&tdata, 0, sizeof(tdata));
    tdata.distance_mm = g_engine.current_distance_mm;
    tdata.rssi        = g_engine.current_rssi;
    tdata.zone        = g_engine.current_zone;
    (void)memcpy(&tdata.vehicle, &g_engine.vehicle_status, sizeof(icce_vehicle_status_t));
    tdata.distance_mm = g_engine.current_distance_mm;
    tdata.rssi        = g_engine.current_rssi;
    tdata.zone        = g_engine.current_zone;
    memcpy(&tdata.vehicle, &g_engine.vehicle_status, sizeof(icce_vehicle_status_t));
>>>>>>> origin/master

    /* If caller provided data, overlay it */
    if (data && len > 0) {
        if (trigger == ICCE_TRIGGER_VEHICLE_STATE && len >= sizeof(icce_vehicle_status_t)) {
            (void)memcpy(&tdata.vehicle, data, sizeof(icce_vehicle_status_t));
        }
    }

    uint32_t now = g_engine.last_tick;


    /* Find best matching rule */
    int32_t  best_idx = -1;
    uint8_t  best_pri = 0;
    uint32_t best_tick = 0;  /* To disambiguate same-priority: earliest wins */

    for (uint8_t i = 0; i < g_engine.rule_count; i++) {
        const icce_edge_rule_t *rule = &g_engine.rules[i];
        if (!rule->enabled) continue;
        if (rule->trigger != trigger) continue;

        /* Check time window */
        if (!is_rule_in_time_window(rule)) continue;

        /* Check cooldown */
        if (is_rule_in_cooldown(rule, now)) continue;

        /* For compound triggers, evaluate the condition tree */
        if (trigger == ICCE_TRIGGER_COMPOUND) {
            if (!evaluate_condition_tree(&rule->condition)) continue;
        }

        /* For rule with inline condition (AND with a non-NONE sub-condition),
         * evaluate the condition tree */
        if (rule->condition.op != COND_OP_NONE) {
            if (!evaluate_condition_tree(&rule->condition)) continue;
        }

        if (rule->priority > best_pri) {
            best_pri = rule->priority;
            best_idx = i;
        }
    }

    if (best_idx < 0) {
        return ICCE_OK;  /* No matching rule */
    }

    /* Check cooldown one more time with the actual rule's last_triggered */
    if (now > 0) {
        uint32_t elapsed = now - g_engine.rules[best_idx].last_triggered;
        if (elapsed < g_engine.rules[best_idx].cooldown_ms) {
            return ICCE_OK;
        }
    }

    /* Found a match → transition to TRIGGERED and execute */
    g_engine.rules[best_idx].last_triggered = now;
    g_engine.active_rule_idx = (uint8_t)best_idx;
    g_engine.action_retries = 0;

    transition_state(ICCE_EDGE_STATE_TRIGGERED);

    /* Execute the action sequence */
    int32_t exec_result = execute_rule_actions(&g_engine.rules[best_idx]);
    if (exec_result == ICCE_OK) {
        transition_state(ICCE_EDGE_STATE_ACTIVE);
    } else {
        transition_state(ICCE_EDGE_STATE_FALLBACK);
    }

    return exec_result;
}

/* ========================================================================
 *  icce_edge_evaluate — Continuous sensor evaluation
 *
 *  Called from the main loop (typically from on_uwb_ranging callback).
 *  Evaluates all enabled rules against current distance and RSSI values.
 *
 *  This is the primary entry point for continuous (non-event) triggers.
 * ======================================================================== */

int32_t icce_edge_evaluate(int32_t distance_mm, int16_t rssi, uint8_t zone)
{
    if (!g_engine.initialized) return ICCE_ERR_PARAM;

    /* Update sensor state */
    g_engine.current_distance_mm = distance_mm;
    g_engine.current_zone = zone;

    /* Update RSSI moving average */
    g_engine.rssi_samples[g_engine.rssi_sample_idx] = rssi;
    g_engine.rssi_sample_idx = (g_engine.rssi_sample_idx + 1) % RSSI_MA_SAMPLES;

    int32_t sum = 0;
    for (uint8_t i = 0; i < RSSI_MA_SAMPLES; i++) {
        sum += g_engine.rssi_samples[i];
    }
    g_engine.rssi_ma = (int16_t)(sum / RSSI_MA_SAMPLES);

    /* Update UWB distance moving average */
    if (distance_mm >= 0) {
        g_engine.uwb_samples[g_engine.uwb_sample_idx] = distance_mm;
        g_engine.uwb_sample_idx = (g_engine.uwb_sample_idx + 1) % UWB_MA_SAMPLES;

        int64_t dist_sum = 0;
        uint8_t valid_count = 0;
        for (uint8_t i = 0; i < UWB_MA_SAMPLES; i++) {
            if (g_engine.uwb_samples[i] >= 0) {
                dist_sum += g_engine.uwb_samples[i];
                valid_count++;
            }
        }
        g_engine.uwb_ma = (valid_count > 0)
            ? (int32_t)(dist_sum / valid_count)
            : distance_mm;
    }

    /* Only evaluate when in MONITORING state */
    if (g_engine.state != ICCE_EDGE_STATE_MONITORING) {
        /* If ACTIVE, check for timeout → return to MONITORING */
        if (g_engine.state == ICCE_EDGE_STATE_ACTIVE) {
            uint32_t now = g_engine.last_tick;
            if (now > 0 && (now - g_engine.state_enter_tick) > ACTIVE_TIMEOUT_MS) {
                transition_state(ICCE_EDGE_STATE_MONITORING);
            }
        }
        /* If FALLBACK, check for retry timeout */
        if (g_engine.state == ICCE_EDGE_STATE_FALLBACK) {
            uint32_t now = g_engine.last_tick;
            if (now > 0 && (now - g_engine.state_enter_tick) > FALLBACK_RETRY_MS) {
                /* Retry the action */
                if (g_engine.action_retries < MAX_ACTION_RETRIES) {
                    uint8_t idx = g_engine.active_rule_idx;
                    if (idx < g_engine.rule_count && g_engine.rules[idx].enabled) {
                        g_engine.action_retries++;
                        int32_t ret = execute_rule_actions(&g_engine.rules[idx]);
                        if (ret == ICCE_OK) {
                            transition_state(ICCE_EDGE_STATE_ACTIVE);
                        } else {
                            /* Stay in FALLBACK with new tick */
                            g_engine.state_enter_tick = now;
                        }
                    } else {
                        /* Rule no longer valid → back to MONITORING */
                        transition_state(ICCE_EDGE_STATE_MONITORING);
                    }
                } else {
                    /* Exhausted retries → back to MONITORING */
                    transition_state(ICCE_EDGE_STATE_MONITORING);
                }
            }
        }
        return ICCE_OK;
    }

    /* In MONITORING: evaluate all enabled rules */
    uint32_t now = g_engine.last_tick;

    for (uint8_t i = 0; i < g_engine.rule_count; i++) {
        const icce_edge_rule_t *rule = &g_engine.rules[i];
        if (!rule->enabled) continue;

        /* Check time window */
        if (!is_rule_in_time_window(rule)) continue;

        /* Check cooldown */
        if (is_rule_in_cooldown(rule, now)) continue;

        bool match = false;

        switch (rule->trigger) {

        case ICCE_TRIGGER_DISTANCE:
            /* Absolute distance threshold match */
            if (distance_mm >= 0 && distance_mm <= rule->threshold_mm) {
                match = true;
            }
            break;

        case ICCE_TRIGGER_BLE_RSSI:
            /* RSSI threshold match using moving average */
            if (rssi > rule->threshold_rssi && g_engine.rssi_ma > rule->threshold_rssi) {
                match = true;
            }
            break;

        case ICCE_TRIGGER_UWB_RANGE:
            /* UWB distance + quality threshold */
            if (distance_mm >= 0 && distance_mm <= rule->threshold_mm) {
                /* If rule has a condition tree, evaluate it */
                if (rule->condition.op != COND_OP_NONE) {
                    match = evaluate_condition_tree(&rule->condition);
                } else {
                    match = true;
                }
            }
            break;

        case ICCE_TRIGGER_VEHICLE_STATE:
            /* Vehicle state-based trigger; already handled by process_trigger */
            /* In evaluate(), check if vehicle state has changed meaningfully */
            if (rule->condition.op != COND_OP_NONE) {
                match = evaluate_condition_tree(&rule->condition);
            }
            /* Also check zone-based defaults */
            if (!match && rule->trigger == ICCE_TRIGGER_VEHICLE_STATE) {
                if (rule->zone_id == ICCE_ZONE_INTERIOR &&
                    g_engine.vehicle_status.engine_status == 0) {
                    match = true;
                }
            }
            break;

        case ICCE_TRIGGER_ZONE_ENTER:
        case ICCE_TRIGGER_ZONE_EXIT:
        case ICCE_TRIGGER_TIME_INTERVAL:
        case ICCE_TRIGGER_COMPOUND:
            /* These are handled via process_trigger or timer_tick */
            break;

        default:
            break;
        }

        if (match) {
            /* Execute immediately (one-shot) */
            g_engine.active_rule_idx = i;
            g_engine.rules[i].last_triggered = now;
            g_engine.action_retries = 0;

            transition_state(ICCE_EDGE_STATE_TRIGGERED);

            int32_t exec_result = execute_rule_actions(rule);
            if (exec_result == ICCE_OK) {
                transition_state(ICCE_EDGE_STATE_ACTIVE);
            } else {
                transition_state(ICCE_EDGE_STATE_FALLBACK);
            }

            /* Only execute one rule per evaluate call (highest priority) */
            break;
        }
    }

    return ICCE_OK;
}

/* ========================================================================
 *  icce_edge_timer_tick — Time-based trigger evaluation
 *
 *  Must be called periodically from the main loop.
 *  Handles ICCE_TRIGGER_TIME_INTERVAL rules and state machine timeouts.
 * ======================================================================== */

int32_t icce_edge_timer_tick(uint32_t elapsed_ms)
{
    if (!g_engine.initialized) return ICCE_ERR_PARAM;

    g_engine.last_tick += elapsed_ms;
    uint32_t now = g_engine.last_tick;

    /* Handle ACTIVE timeout → MONITORING */
    if (g_engine.state == ICCE_EDGE_STATE_ACTIVE) {
        if ((now - g_engine.state_enter_tick) > ACTIVE_TIMEOUT_MS) {
            transition_state(ICCE_EDGE_STATE_MONITORING);
        }
    }

    /* Handle FALLBACK retry */
    if (g_engine.state == ICCE_EDGE_STATE_FALLBACK) {
        if ((now - g_engine.state_enter_tick) > FALLBACK_RETRY_MS) {
            if (g_engine.action_retries < MAX_ACTION_RETRIES) {
                uint8_t idx = g_engine.active_rule_idx;
                if (idx < g_engine.rule_count && g_engine.rules[idx].enabled) {
                    g_engine.action_retries++;
                    int32_t ret = execute_rule_actions(&g_engine.rules[idx]);
                    if (ret == ICCE_OK) {
                        transition_state(ICCE_EDGE_STATE_ACTIVE);
                    } else {
                        g_engine.state_enter_tick = now;
                    }
                } else {
                    transition_state(ICCE_EDGE_STATE_MONITORING);
                }
            } else {
                transition_state(ICCE_EDGE_STATE_MONITORING);
            }
        }
    }

    /* Evaluate time-interval rules (only in MONITORING) */
    if (g_engine.state != ICCE_EDGE_STATE_MONITORING) {
        return ICCE_OK;
    }

    for (uint8_t i = 0; i < g_engine.rule_count; i++) {
        icce_edge_rule_t *rule = &g_engine.rules[i];
        if (!rule->enabled) continue;
        if (rule->trigger != ICCE_TRIGGER_TIME_INTERVAL) continue;
        if (!is_rule_in_time_window(rule)) continue;

        /* Check if interval has elapsed since last trigger */
        if (rule->interval_ms == 0) continue;
        if ((now - rule->last_triggered) < rule->interval_ms) continue;

        /* Check cooldown */
        if (is_rule_in_cooldown(rule, now)) continue;

        /* Execute the time-based action */
        rule->last_triggered = now;
        g_engine.active_rule_idx = i;
        g_engine.action_retries = 0;

        transition_state(ICCE_EDGE_STATE_TRIGGERED);
        int32_t exec_result = execute_rule_actions(rule);
        if (exec_result == ICCE_OK) {
            transition_state(ICCE_EDGE_STATE_ACTIVE);
        } else {
            transition_state(ICCE_EDGE_STATE_FALLBACK);
        }

        /* Only one time trigger per tick */
        break;
    }

    return ICCE_OK;
}

/* ========================================================================
 *  icce_edge_update_rssi — Update RSSI and evaluate relevant rules
 *
 *  Called when BLE stack provides a new RSSI reading.
 * ======================================================================== */

int32_t icce_edge_update_rssi(int16_t rssi)
{
    if (!g_engine.initialized) return ICCE_ERR_PARAM;

    g_engine.current_rssi = rssi;

    /* Update moving average */
    g_engine.rssi_samples[g_engine.rssi_sample_idx] = rssi;
    g_engine.rssi_sample_idx = (g_engine.rssi_sample_idx + 1) % RSSI_MA_SAMPLES;

    int32_t sum = 0;
    for (uint8_t i = 0; i < RSSI_MA_SAMPLES; i++) {
        sum += g_engine.rssi_samples[i];
    }
    g_engine.rssi_ma = (int16_t)(sum / RSSI_MA_SAMPLES);

    /* Only evaluate in MONITORING state */
    if (g_engine.state != ICCE_EDGE_STATE_MONITORING) {
        return ICCE_OK;
    }

    uint32_t now = g_engine.last_tick;

    for (uint8_t i = 0; i < g_engine.rule_count; i++) {
        const icce_edge_rule_t *rule = &g_engine.rules[i];
        if (!rule->enabled) continue;
        if (rule->trigger != ICCE_TRIGGER_BLE_RSSI) continue;
        if (!is_rule_in_time_window(rule)) continue;
        if (is_rule_in_cooldown(rule, now)) continue;

        /* Evaluate condition tree if present */
        if (rule->condition.op != COND_OP_NONE) {
            if (!evaluate_condition_tree(&rule->condition)) continue;
        }

        /* Check RSSI threshold (use moving average for stability) */
        if (rssi > rule->threshold_rssi || g_engine.rssi_ma > rule->threshold_rssi) {
            g_engine.active_rule_idx = i;
            g_engine.rules[i].last_triggered = now;
            g_engine.action_retries = 0;

            transition_state(ICCE_EDGE_STATE_TRIGGERED);
            int32_t exec_result = execute_rule_actions(rule);
            if (exec_result == ICCE_OK) {
                transition_state(ICCE_EDGE_STATE_ACTIVE);
            } else {
                transition_state(ICCE_EDGE_STATE_FALLBACK);
            }
            break;  /* One rule per call */
        }
    }

    return ICCE_OK;
}

/* ========================================================================
 *  icce_edge_update_vehicle_state — Handle vehicle state changes
 *
 *  Called by the vehicle status callback when CAN bus reports a change.
 * ======================================================================== */

int32_t icce_edge_update_vehicle_state(const icce_vehicle_status_t *status)
{
    if (!status || !g_engine.initialized) return ICCE_ERR_PARAM;

    /* Detect state changes */
    bool engine_changed = (status->engine_status != g_engine.vehicle_status.engine_status);
    bool lock_changed   = (status->lock_status != g_engine.vehicle_status.lock_status);
    bool door_changed   = (status->door_status != g_engine.vehicle_status.door_status);
    bool gear_changed   = (status->gear_position != g_engine.vehicle_status.gear_position);

    /* Update stored state */
    (void)memcpy(&g_engine.vehicle_status, status, sizeof(icce_vehicle_status_t));

    /* Only evaluate if something changed and we're in MONITORING */
    if (!engine_changed && !lock_changed && !door_changed && !gear_changed) {
        return ICCE_OK;
    }
    if (g_engine.state != ICCE_EDGE_STATE_MONITORING) {
        return ICCE_OK;
    }

    uint32_t now = g_engine.last_tick;

    for (uint8_t i = 0; i < g_engine.rule_count; i++) {
        const icce_edge_rule_t *rule = &g_engine.rules[i];
        if (!rule->enabled) continue;
        if (rule->trigger != ICCE_TRIGGER_VEHICLE_STATE) continue;
        if (!is_rule_in_time_window(rule)) continue;
        if (is_rule_in_cooldown(rule, now)) continue;

        /* Evaluate condition tree */
        bool match = false;
        if (rule->condition.op != COND_OP_NONE) {
            match = evaluate_condition_tree(&rule->condition);
        } else {
            /* Default: match on any state change */
            match = true;
        }

        if (match) {
            g_engine.active_rule_idx = i;
            g_engine.rules[i].last_triggered = now;
            g_engine.action_retries = 0;

            transition_state(ICCE_EDGE_STATE_TRIGGERED);
            int32_t exec_result = execute_rule_actions(rule);
            if (exec_result == ICCE_OK) {
                transition_state(ICCE_EDGE_STATE_ACTIVE);
            } else {
                transition_state(ICCE_EDGE_STATE_FALLBACK);
            }
            break;
        }
    }

    return ICCE_OK;
}

/* ========================================================================
 *  Private: State Machine
 * ======================================================================== */

static void transition_state(icce_edge_state_e new_state)
{
    icce_edge_state_e old_state = g_engine.state;
    g_engine.state = new_state;
    g_engine.state_enter_tick = g_engine.last_tick;

    (void)old_state; /* For future trace/logging */

    /* On re-entering MONITORING from ACTIVE/FALLBACK, reset action context */
    if (new_state == ICCE_EDGE_STATE_MONITORING) {
        g_engine.active_rule_idx = 0;
        g_engine.action_retries = 0;
    }
}

/* ========================================================================
 *  Private: Condition Tree Evaluation
 *
 *  Recursively evaluates a condition tree against the current sensor state.
 *  Supports AND, OR, NOT, and leaf conditions (RSSI_GT, DIST_LT, ZONE_EQ,
 *  VEHICLE_STOPPED, VEHICLE_LOCKED, VEHICLE_PARKED, TIME_IN_WINDOW).
 * ======================================================================== */

static bool evaluate_condition_tree(const icce_condition_t *cond)
{
    if (!cond) return true;  /* No condition = always true */

    switch (cond->op) {

    /* --- Logical operators --- */
    case COND_OP_AND:
        return evaluate_condition_tree(cond->left) &&
               evaluate_condition_tree(cond->right);

    case COND_OP_OR:
        return evaluate_condition_tree(cond->left) ||
               evaluate_condition_tree(cond->right);

    case COND_OP_NOT:
        return !evaluate_condition_tree(cond->left);

    /* --- Leaf: RSSI comparisons --- */
    case COND_OP_RSSI_GT:
        return g_engine.current_rssi > cond->threshold;

    case COND_OP_RSSI_LT:
        return g_engine.current_rssi < cond->threshold;

    /* --- Leaf: Distance comparisons --- */
    case COND_OP_DIST_GT:
        return g_engine.current_distance_mm > cond->threshold;

    case COND_OP_DIST_LT:
        return g_engine.current_distance_mm >= 0 &&
               g_engine.current_distance_mm < cond->threshold;

    /* --- Leaf: Zone equality --- */
    case COND_OP_ZONE_EQ:
        return g_engine.current_zone == cond->zone_id;

    /* --- Leaf: Vehicle state --- */
    case COND_OP_VEHICLE_STOPPED:
        return g_engine.vehicle_status.engine_status == 0 &&
               g_engine.vehicle_status.speed_kmh == 0;

    case COND_OP_VEHICLE_LOCKED:
        return g_engine.vehicle_status.lock_status == 0x01;

    case COND_OP_VEHICLE_PARKED:
        return g_engine.vehicle_status.gear_position == 0; /* Park */

    /* --- Leaf: Time window --- */
    case COND_OP_TIME_IN_WINDOW:
        /* Uses the last_tick to determine approximate hour of day */
        /* Simplified: assumes last_tick is monotonic and wraps ~49.7 days */
        {
            uint32_t hour_ms = 3600000U;
            uint32_t day_ms  = 86400000U;
            uint32_t time_of_day = g_engine.last_tick % day_ms;
            uint8_t current_hour = (uint8_t)(time_of_day / hour_ms);
            uint32_t mask_bit = (uint32_t)1 << current_hour;
            return (cond->threshold != 0) && ((mask_bit & (uint32_t)cond->threshold) != 0);
        }

    case COND_OP_NONE:
    default:
        return true;  /* No-op condition always passes */
    }
}

/* ========================================================================
 *  Private: Rule Evaluation Helpers
 * ======================================================================== */

/**
 * Check if a rule is within its allowed time window.
 * time_mask is a 24-bit bitmap of hours (bit 0 = 00:00-01:00, etc.).
 * If time_mask is 0xFFFFFF (all hours), always in window.
 */
static bool is_rule_in_time_window(const icce_edge_rule_t *rule)
{
    if (rule->time_mask == 0xFFFFFF) {
        return true;  /* All hours allowed */
    }

    /* Simplified hour calculation from last_tick */
    uint32_t hour_ms = 3600000U;
    uint32_t day_ms  = 86400000U;
    uint32_t time_of_day = g_engine.last_tick % day_ms;
    uint8_t current_hour = (uint8_t)(time_of_day / hour_ms);

    uint32_t mask_bit = (uint32_t)1 << current_hour;
    return (rule->time_mask & mask_bit) != 0;
}

/**
 * Check if a rule is in cooldown (prevent rapid re-triggering).
 */
static bool is_rule_in_cooldown(const icce_edge_rule_t *rule, uint32_t now)
{
    if (rule->cooldown_ms == 0) return false;
    if (now == 0) return false;  /* Time not yet initialized */
    if (rule->last_triggered == 0) return false;  /* Never triggered */

    uint32_t elapsed = now - rule->last_triggered;
    return elapsed < rule->cooldown_ms;
}

/* ========================================================================
 *  Private: Action Execution
 * ======================================================================== */

/**
 * Execute all actions in a rule's action list.
 * Returns ICCE_OK if all actions succeeded, error code on first failure.
 */
static int32_t execute_rule_actions(const icce_edge_rule_t *rule)
{
    int32_t result = ICCE_OK;

    for (uint8_t a = 0; a < rule->action_count; a++) {
        int32_t ret = icce_vehicle_ctrl(rule->actions[a], 0);
        if (ret != ICCE_OK) {
            result = ret;
            /* Continue executing remaining actions (best-effort) */
        }
    }

    return result;
}

/* ========================================================================
 *  Private: Single Rule Condition Evaluation
 *  (for use within the main evaluation loop)
 * ======================================================================== */

static bool evaluate_single_rule(const icce_edge_rule_t *rule)
{
    if (!rule->enabled) return false;
    if (!is_rule_in_time_window(rule)) return false;

    /* If rule has a compound condition, check that too */
    if (rule->condition.op != COND_OP_NONE) {
        return evaluate_condition_tree(&rule->condition);
    }

    return true;
}
