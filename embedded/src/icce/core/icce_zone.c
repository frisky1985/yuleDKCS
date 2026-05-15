/**
 * @file icce_zone.c
 * @brief ICCE Zone Management - Distance-based Zone Classification
 */

#include "icce_digital_key.h"

static const icce_zone_def_t g_zone_defs[ICCE_ZONE_MAX] = {
    /* zone,              inner_mm, outer_mm, actions_mask */
    { ICCE_ZONE_NONE,     0,        0,       0x00 },
    { ICCE_ZONE_FAR,      20000,    50000,   0x40 },  /* FIND only */
    { ICCE_ZONE_MID,      10000,    20000,   0x60 },  /* FIND + BLE conn */
    { ICCE_ZONE_NEAR,     3000,     10000,   0x70 },  /* + approach notify */
    { ICCE_ZONE_VICINITY, 1000,     3000,    0x73 },  /* + UNLOCK/LOCK */
    { ICCE_ZONE_INTERIOR, 0,        1000,    0xFF },  /* All actions */
    { ICCE_ZONE_NONE,     0,        0,       0x00 },
    { ICCE_ZONE_NONE,     0,        0,       0x00 },
};

int32_t icce_zone_init(void)
{
    return ICCE_OK;
}

icce_zone_e icce_zone_classify(int32_t distance_mm)
{
    if (distance_mm < 0) return ICCE_ZONE_NONE;
    if (distance_mm < 1000)   return ICCE_ZONE_INTERIOR;
    if (distance_mm < 3000)   return ICCE_ZONE_VICINITY;
    if (distance_mm < 10000)  return ICCE_ZONE_NEAR;
    if (distance_mm < 20000)  return ICCE_ZONE_MID;
    if (distance_mm < 50000)  return ICCE_ZONE_FAR;
    return ICCE_ZONE_NONE;
}

int32_t icce_zone_get_def(icce_zone_e zone, icce_zone_def_t *out)
{
    if (!out) return ICCE_ERR_PARAM;
    if (zone >= ICCE_ZONE_MAX) return ICCE_ERR_PARAM;

    *out = g_zone_defs[zone];
    return ICCE_OK;
}
