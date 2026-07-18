/* ble_gatt.h — Test stub for ICCE BLE GATT layer */
#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <stdint.h>
#include <stdbool.h>

#define GATT_SUCCESS     0
#define GATT_ERR_FAIL    -1

typedef struct {
    uint8_t  uuid_type;
    uint16_t uuid;
    bool     is_primary;
} ble_gatt_service_t;

typedef struct {
    uint16_t uuid;
    uint8_t  properties;
    uint8_t  permissions;
} ble_gatt_char_t;

int32_t ble_gatt_init(void);
int32_t ble_gatt_add_service(const ble_gatt_service_t *svc, uint16_t *handle);
int32_t ble_gatt_add_characteristic(uint16_t svc_handle,
                                    const ble_gatt_char_t *chr,
                                    uint16_t *char_handle);
int32_t ble_gatt_send_notification(uint16_t conn_handle, uint16_t char_handle,
                                   const uint8_t *data, uint16_t len);

#endif /* BLE_GATT_H */
