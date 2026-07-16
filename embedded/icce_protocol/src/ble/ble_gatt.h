#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <stdint.h>
#include <stdbool.h>

#define GATT_SUCCESS 0

/* GATT Service */
typedef struct {
    uint8_t uuid_type;
    uint16_t uuid;
    uint8_t is_primary;
} ble_gatt_service_t;

/* GATT Characteristic */
typedef struct {
    uint16_t uuid;
    uint8_t properties;
    uint8_t permissions;
} ble_gatt_char_t;

/* GATT API Prototypes */
int32_t ble_gatt_init(void);
int32_t ble_gatt_add_service(const ble_gatt_service_t *service, uint16_t *service_handle);
int32_t ble_gatt_add_characteristic(uint16_t service_handle, const ble_gatt_char_t *chr, uint16_t *char_handle);
int32_t ble_gatt_send_notification(uint16_t conn_handle, uint16_t char_handle, const uint8_t *data, uint16_t len);

#endif /* BLE_GATT_H */
