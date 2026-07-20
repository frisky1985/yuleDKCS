/**
 * @file icce_edge.c
 * @brief ICCE Edge Computing Module - Rule Engine & Local Decision Making
 */

#include "icce_digital_key.h"

static icce_edge_rule_t g_rules[ICCE_EDGE_MAX_RULES];
static uint8_t g_rule_count = 0;

int32_t icce_edge_init(void)
{
    memset(g_rules, 0, sizeof(g_rules));
    g_rule_count = 0;

    /* Default rules: approach-based unlock/lock */
    g_rules[0] = (icce_edge_rule_t){
        .trigger = ICCE_TRIGGER_ZONE_ENTER,
        .zone_id = ICCE_ZONE_VICINITY,
        .threshold_mm = 3000,
        .time_mask = 0xFFFFFF,  /* 24h */
        .actions = { ICCE_ACTION_UNLOCK },
        .action_count = 1,
        .priority = 128,
        .enabled = true
    };

    g_rules[1] = (icce_edge_rule_t){
        .trigger = ICCE_TRIGGER_ZONE_EXIT,
        .zone_id = ICCE_ZONE_NEAR,
        .threshold_mm = 10000,
        .time_mask = 0xFFFFFF,
        .actions = { ICCE_ACTION_LOCK },
        .action_count = 1,
        .priority = 128,
        .enabled = true
    };

    g_rules[2] = (icce_edge_rule_t){
        .trigger = ICCE_TRIGGER_ZONE_ENTER,
        .zone_id = ICCE_ZONE_INTERIOR,
        .threshold_mm = 1000,
        .time_mask = 0xFFFFFF,
        .actions = { ICCE_ACTION_START },
        .action_count = 1,
        .priority = 200,
        .enabled = true
    };

    g_rule_count = 3;
    return ICCE_OK;
}

int32_t icce_edge_deinit(void)
{
    g_rule_count = 0;
    return ICCE_OK;
}

int32_t icce_edge_add_rule(const icce_edge_rule_t *rule)
{
    if (!rule || g_rule_count >= ICCE_EDGE_MAX_RULES) return ICCE_ERR_PARAM;
    g_rules[g_rule_count++] = *rule;
    return ICCE_OK;
}

int32_t icce_edge_remove_rule(uint8_t rule_id)
{
    if (rule_id >= g_rule_count) return ICCE_ERR_NOT_FOUND;
    for (uint8_t i = rule_id; i < g_rule_count - 1; i++) {
        g_rules[i] = g_rules[i + 1];
    }
    g_rule_count--;
    return ICCE_OK;
}

int32_t icce_edge_enable_rule(uint8_t rule_id, bool enable)
{
    if (rule_id >= g_rule_count) return ICCE_ERR_NOT_FOUND;
    g_rules[rule_id].enabled = enable;
    return ICCE_OK;
}

int32_t icce_edge_process_trigger(icce_trigger_e trigger, const void *data, uint16_t len)
{
    (void)data; (void)len;

    /* Find matching rules and execute highest priority */
    int32_t best_idx = -1;
    uint8_t best_pri = 0;

    for (uint8_t i = 0; i < g_rule_count; i++) {
        if (!g_rules[i].enabled) continue;
        if (g_rules[i].trigger != trigger) continue;
        if (g_rules[i].priority > best_pri) {
            best_pri = g_rules[i].priority;
            best_idx = i;
        }
    }

    if (best_idx < 0) return ICCE_OK;  /* No matching rule */

    /* Execute actions */
    const icce_edge_rule_t *rule = &g_rules[best_idx];
    for (uint8_t a = 0; a < rule->action_count; a++) {
        icce_vehicle_ctrl(rule->actions[a], 0);
    }

    return ICCE_OK;
}

int32_t icce_edge_evaluate(int32_t distance_mm, int16_t rssi, uint8_t zone)
{
    (void)rssi;

    /* Evaluate distance-based triggers */
    for (uint8_t i = 0; i < g_rule_count; i++) {
        if (!g_rules[i].enabled) continue;
        if (g_rules[i].trigger != ICCE_TRIGGER_DISTANCE) continue;
        if (distance_mm <= g_rules[i].threshold_mm) {
            for (uint8_t a = 0; a < g_rules[i].action_count; a++) {
                icce_vehicle_ctrl(g_rules[i].actions[a], 0);
            }
            g_rules[i].enabled = false;  /* One-shot: disable after execution */
        }
    }

    /* Evaluate BLE RSSI triggers */
    for (uint8_t i = 0; i < g_rule_count; i++) {
        if (!g_rules[i].enabled) continue;
        if (g_rules[i].trigger == ICCE_TRIGGER_BLE_RSSI && rssi > -50) {
            /* Strong BLE signal detected - potential proximity */
            (void)zone;
        }
    }

    return ICCE_OK;
}
