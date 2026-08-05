/**
 * @file ble_adapter.h
 * @module EMB-BSW-BLE (ASPICE SWE.4)
 * @brief ICCE BLE Adapter — Bluetooth Low Energy hardware abstraction
 * Layer: BSW (Basic Software Layer) - Communication
 */

#ifndef BLE_ADAPTER_H
#define BLE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>

/* Adapter Result */
#define ADAPTER_SUCCESS 0

/* BLE Adapter Events */
#define BLE_ADAPTER_EVENT_CONNECTED            0x01
#define BLE_ADAPTER_EVENT_DISCONNECTED          0x02
#define BLE_ADAPTER_EVENT_ENCRYPTION_COMPLETE   0x03
#define BLE_ADAPTER_EVENT_DATA_RECEIVED         0x04
#define BLE_ADAPTER_EVENT_DATA_SENT             0x05

/* BLE Advertising Type */
#define BLE_ADV_TYPE_CONNECTABLE_UNDIRECTED     0x00

/* BLE UUID Type */
#define BLE_UUID_TYPE_16                        0x01
#define BLE_UUID_TYPE_128                       0x02

/* Event data structures */
typedef struct {
    uint16_t conn_handle;
} ble_adapter_conn_evt_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t reason;
} ble_adapter_disconn_evt_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t success;
} ble_adapter_enc_evt_t;

typedef struct {
    uint16_t conn_handle;
    const uint8_t *data;
    uint16_t length;
} ble_adapter_data_evt_t;

typedef struct {
    uint8_t device_name[32];
    uint16_t appearance;
    uint8_t max_connections;
} ble_adapter_config_t;

typedef struct {
    uint16_t interval_min;
    uint16_t interval_max;
    uint8_t type;
    uint16_t duration;
} ble_adapter_adv_params_t;

typedef struct {
    uint8_t addr_type;
    uint8_t address[6];
    uint16_t conn_interval_min;
    uint16_t conn_interval_max;
    uint16_t slave_latency;
    uint16_t supervision_timeout;
} ble_adapter_conn_params_t;

/* Adapter API Prototypes */
int32_t ble_adapter_init(const ble_adapter_config_t *config);
int32_t ble_adapter_register_event_handler(void (*handler)(uint8_t event, void *data));
int32_t ble_adapter_start_advertising(const uint8_t *adv_data, uint8_t adv_len, const ble_adapter_adv_params_t *params);
int32_t ble_adapter_stop_advertising(void);
int32_t ble_adapter_connect(const ble_adapter_conn_params_t *params, uint16_t *conn_handle);
int32_t ble_adapter_disconnect(uint16_t conn_handle);
int32_t ble_adapter_start_encryption(uint16_t conn_handle);

#endif /* BLE_ADAPTER_H */
