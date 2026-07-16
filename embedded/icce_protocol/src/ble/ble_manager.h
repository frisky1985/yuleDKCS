#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "icce_digital_key.h"

/* BLE Manager Result */
typedef enum {
    BLE_SUCCESS = 0,
    BLE_ERR_NOT_INITIALIZED = -1,
    BLE_ERR_ADAPTER_NOT_FOUND = -2,
    BLE_ERR_HARDWARE_FAULT = -3,
    BLE_ERR_CONNECTION_FAILED = -4,
    BLE_ERR_BUFFER_OVERFLOW = -5,
    BLE_ERR_ALREADY_INITIALIZED = -6,
} ble_result_t;

/* BLE Connection State */
typedef enum {
    BLE_CONN_STATE_DISCONNECTED = 0,
    BLE_CONN_STATE_CONNECTING,
    BLE_CONN_STATE_CONNECTED,
    BLE_CONN_STATE_ENCRYPTING,
    BLE_CONN_STATE_ENCRYPTED,
    BLE_CONN_STATE_DISCONNECTING,
} ble_conn_state_t;

/* BLE Address */
typedef struct {
    uint8_t type;
    uint8_t addr[6];
} ble_address_t;

/* BLE Device */
typedef struct {
    ble_address_t address;
    uint8_t name[32];
    int8_t rssi;
} ble_device_t;

/* BLE Connection Parameters */
typedef struct {
    uint16_t conn_interval_min;
    uint16_t conn_interval_max;
    uint16_t slave_latency;
    uint16_t supervision_timeout;
} ble_conn_params_t;

/* BLE Connection */
typedef struct {
    uint16_t conn_handle;
    ble_device_t device;
    ble_conn_params_t params;
    ble_conn_state_t state;
} ble_connection_t;

/* BLE Advertising Config */
typedef struct {
    uint8_t flags;
    uint16_t service_uuids[8];
    uint8_t service_uuid_count;
    uint8_t local_name[32];
    uint16_t min_interval_ms;
    uint16_t max_interval_ms;
} ble_adv_config_t;

/* BLE Event Callback */
typedef void (*ble_event_callback_t)(uint8_t event, void *data);

/* BLE Manager API */
ble_result_t ble_manager_init(void);
ble_result_t ble_start_advertising(const ble_adv_config_t *config);
ble_result_t ble_stop_advertising(void);
ble_result_t ble_connect(const ble_device_t *device, const ble_conn_params_t *params, uint16_t *conn_handle);
ble_result_t ble_disconnect(uint16_t conn_handle);
ble_result_t ble_send_data(uint16_t conn_handle, const uint8_t *data, uint16_t length);
ble_result_t ble_register_callback(ble_event_callback_t callback);
ble_result_t ble_get_connection_info(uint16_t conn_handle, ble_connection_t *conn_info);

#endif /* BLE_MANAGER_H */
