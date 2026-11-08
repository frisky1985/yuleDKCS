#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <stdint.h>
#include <stdbool.h>

#define GATT_SUCCESS 0

/* ── GATT Characteristic Properties (BLE Core Spec, Vol 3, Part G, 3.3.1.1) ── */
#define GATT_PROP_BROADCAST          0x01
#define GATT_PROP_READ               0x02
#define GATT_PROP_WRITE_NO_RESPONSE  0x04
#define GATT_PROP_WRITE              0x08
#define GATT_PROP_NOTIFY             0x10
#define GATT_PROP_INDICATE           0x20
#define GATT_PROP_WRITE_SIGNED       0x40
#define GATT_PROP_EXTENDED           0x80

/* ── GATT Attribute Permissions ── */
#define GATT_PERM_NONE               0x00
#define GATT_PERM_READ               0x01
#define GATT_PERM_WRITE              0x02
#define GATT_PERM_READ_ENCRYPTED     0x04
#define GATT_PERM_WRITE_ENCRYPTED    0x08
#define GATT_PERM_AUTHENTICATION     0x10

/* ── GATT Service / Characteristic UUIDs (16-bit, ICCE profile) ── */
#define GATT_UUID_DIGITAL_KEY_SERVICE  0xFFE0
#define GATT_UUID_KEY_STATUS           0xFFE1
#define GATT_UUID_RANGING_DATA         0xFFE2
#define GATT_UUID_AUTH_CHALLENGE       0xFFE3
#define GATT_UUID_CONTROL_COMMAND      0xFFE4
#define GATT_UUID_SESSION_KEY          0xFFE5

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
