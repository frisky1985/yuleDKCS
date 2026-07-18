/* vehicle_integration.h — Test stub for ICCE vehicle integration */
#ifndef VEHICLE_INTEGRATION_H
#define VEHICLE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

#define VEH_OK          0
#define VEH_ERR         -1
#define VEH_ERR_BUSY    -2

typedef struct {
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
} veh_status_t;

int32_t veh_init(void);
int32_t veh_ctrl(uint8_t action, uint8_t param);
int32_t veh_get_status(veh_status_t *status);
int32_t veh_register_cb(void (*cb)(const veh_status_t *status));

#endif /* VEHICLE_INTEGRATION_H */
