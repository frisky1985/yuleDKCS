/* ble_adapter.h — Test stub for ICCE BLE adapter HAL */
#ifndef BLE_ADAPTER_H
#define BLE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>

#define ADAPTER_SUCCESS     0
#define ADAPTER_ERR_FAIL    -1

#define BLE_ADV_TYPE_CONNECTABLE_UNDIRECTED 0
#define BLE_UUID_TYPE_16 0

/* Event types */
#define BLE_ADAPTER_EVENT_CONNECTED             1
#define BLE_ADAPTER_EVENT_DISCONNECTED          2
#define BLE_ADAPTER_EVENT_ENCRYPTION_COMPLETE   3
#define BLE_ADAPTER_EVENT_DATA_RECEIVED         4
#define BLE_ADAPTER_EVENT_DATA_SENT             5

typedef struct {
    uint8_t device_name[32];
    uint16_t appearance;
    uint8_t max_connections;
} ble_adapter_config_t;

typedef struct {
    uint16_t interval_min;
    uint16_t interval_max;
    uint8_t  type;
    uint32_t duration;
} ble_adapter_adv_params_t;

typedef struct {
    uint8_t     addr_type;
    uint8_t     address[6];
    uint16_t    conn_interval_min;
    uint16_t    conn_interval_max;
    uint16_t    slave_latency;
    uint16_t    supervision_timeout;
} ble_adapter_conn_params_t;

typedef struct {
    uint16_t conn_handle;
} ble_adapter_conn_evt_t;

typedef struct {
    uint16_t conn_handle;
} ble_adapter_disconn_evt_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t  success;
} ble_adapter_enc_evt_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t *data;
    uint16_t length;
} ble_adapter_data_evt_t;

typedef void (*ble_adapter_event_handler_t)(uint8_t event, void *data);

int32_t ble_adapter_init(const ble_adapter_config_t *config);
void    ble_adapter_register_event_handler(ble_adapter_event_handler_t handler);
int32_t ble_adapter_start_advertising(const uint8_t *data, uint16_t len,
                                      const ble_adapter_adv_params_t *params);
int32_t ble_adapter_stop_advertising(void);
int32_t ble_adapter_connect(const ble_adapter_conn_params_t *params, uint16_t *handle);
int32_t ble_adapter_disconnect(uint16_t conn_handle);
int32_t ble_adapter_start_encryption(uint16_t conn_handle);

#endif /* BLE_ADAPTER_H */
