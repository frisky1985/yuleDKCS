/* ble_manager.h — Test stub for ICCE BLE manager */
#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define BLE_SUCCESS                 0
#define BLE_ERR_NOT_INITIALIZED    -1
#define BLE_ERR_ALREADY_INITIALIZED -2
#define BLE_ERR_CONNECTION_FAILED  -3
#define BLE_ERR_HARDWARE_FAULT     -4
#define BLE_ERR_ADAPTER_NOT_FOUND  -5
#define BLE_ERR_BUFFER_OVERFLOW    -6

#define BLE_CONN_STATE_DISCONNECTED     0
#define BLE_CONN_STATE_CONNECTING       1
#define BLE_CONN_STATE_CONNECTED        2
#define BLE_CONN_STATE_ENCRYPTED        3
#define BLE_CONN_STATE_DISCONNECTING    4

typedef struct {
    uint8_t addr[6];
    uint8_t type;
} ble_address_t;

typedef struct {
    ble_address_t address;
    uint8_t       name[32];
} ble_device_t;

typedef struct {
    uint16_t conn_interval_min;
    uint16_t conn_interval_max;
    uint16_t slave_latency;
    uint16_t supervision_timeout;
} ble_conn_params_t;

typedef struct {
    ble_device_t    device;
    ble_conn_params_t params;
    uint16_t        conn_handle;
    uint8_t         state;
} ble_connection_t;

typedef struct {
    uint8_t  flags;
    uint8_t  service_uuids[8];
    uint8_t  service_uuid_count;
    uint16_t min_interval_ms;
    uint16_t max_interval_ms;
    uint8_t  local_name[32];
} ble_adv_config_t;

typedef int32_t ble_result_t;
typedef void (*ble_event_callback_t)(uint8_t event, void *data);

ble_result_t ble_manager_init(void);
ble_result_t ble_start_advertising(const ble_adv_config_t *config);
ble_result_t ble_stop_advertising(void);
ble_result_t ble_connect(const ble_device_t *device,
                         const ble_conn_params_t *params,
                         uint16_t *conn_handle);
ble_result_t ble_disconnect(uint16_t conn_handle);
ble_result_t ble_send_data(uint16_t conn_handle,
                           const uint8_t *data, uint16_t length);
ble_result_t ble_register_callback(ble_event_callback_t callback);
ble_result_t ble_get_connection_info(uint16_t conn_handle,
                                     ble_connection_t *conn_info);

#endif /* BLE_MANAGER_H */
