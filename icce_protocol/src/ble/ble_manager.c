/**
 * @file ble_manager.c
 * @brief BLE通信管理模块实现
 * @version 1.0
 * @date 2026-05-05
 * 
 * 基于NXP KW47A的BLE通信管理实现
 */

#include "ble_manager.h"
#include "ble_adapter.h"
#include "ble_gatt.h"
#include <string.h>
#include <stdlib.h>

/* 私有定义 */
#define MAX_CONNECTIONS         4
#define MAX_ADV_DATA_LEN        31
#define MAX_SCAN_DATA_LEN       31
#define CONN_TIMEOUT_MS         5000
#define MAX_PENDING_PACKETS     8

/* 连接上下文 */
typedef struct {
    ble_connection_t info;
    uint8_t rx_buffer[256];
    uint16_t rx_len;
    uint8_t tx_buffer[256];
    uint16_t tx_len;
    uint8_t pending_packets;
    uint32_t last_activity;
    bool in_use;
} conn_context_t;

/* BLE管理器实例 */
typedef struct {
    bool initialized;
    bool advertising;
    bool scanning;
    ble_adv_config_t adv_config;
    conn_context_t connections[MAX_CONNECTIONS];
    ble_event_callback_t event_callback;
    uint16_t conn_count;
    
    /* GATT服务 */
    uint16_t service_handle;
    uint16_t key_status_handle;
    uint16_t ranging_data_handle;
    uint16_t auth_challenge_handle;
    uint16_t control_cmd_handle;
    uint16_t session_key_handle;
} ble_manager_t;

/* 全局实例 */
static ble_manager_t g_ble_manager = {0};

/* 前向声明 */
static void ble_adapter_event_handler(uint8_t event, void *data);
static int16_t find_free_connection_slot(void);
static conn_context_t* find_connection(uint16_t conn_handle);

/*============================================================================
 * 公开API实现
 *===========================================================================*/

ble_result_t ble_manager_init(void)
{
    if (g_ble_manager.initialized) {
        return BLE_ERR_ALREADY_INITIALIZED;
    }
    
    /* 初始化适配器 */
    ble_adapter_config_t adapter_config = {
        .device_name = "ICCE-Vehicle",
        .appearance = 0x0000,  // Generic Unknown
        .max_connections = MAX_CONNECTIONS
    };
    
    if (ble_adapter_init(&adapter_config) != ADAPTER_SUCCESS) {
        return BLE_ERR_ADAPTER_NOT_FOUND;
    }
    
    /* 注册事件处理 */
    ble_adapter_register_event_handler(ble_adapter_event_handler);
    
    /* 初始化GATT服务 */
    if (ble_gatt_init() != GATT_SUCCESS) {
        return BLE_ERR_HARDWARE_FAULT;
    }
    
    /* 创建数字钥匙服务 */
    ble_gatt_service_t service = {
        .uuid_type = BLE_UUID_TYPE_16,
        .uuid = GATT_UUID_DIGITAL_KEY_SERVICE,
        .is_primary = true
    };
    
    if (ble_gatt_add_service(&service, &g_ble_manager.service_handle) != GATT_SUCCESS) {
        return BLE_ERR_HARDWARE_FAULT;
    }
    
    /* 添加特征 */
    /* 1. 钥匙状态特征 */
    ble_gatt_char_t key_status_char = {
        .uuid = GATT_UUID_KEY_STATUS,
        .properties = GATT_PROP_READ | GATT_PROP_NOTIFY,
        .permissions = GATT_PERM_READ | GATT_PERM_WRITE
    };
    ble_gatt_add_characteristic(g_ble_manager.service_handle, 
                                &key_status_char,
                                &g_ble_manager.key_status_handle);
    
    /* 2. 测距数据特征 */
    ble_gatt_char_t ranging_char = {
        .uuid = GATT_UUID_RANGING_DATA,
        .properties = GATT_PROP_READ | GATT_PROP_NOTIFY,
        .permissions = GATT_PERM_READ
    };
    ble_gatt_add_characteristic(g_ble_manager.service_handle,
                                &ranging_char,
                                &g_ble_manager.ranging_data_handle);
    
    /* 3. 认证挑战特征 */
    ble_gatt_char_t auth_char = {
        .uuid = GATT_UUID_AUTH_CHALLENGE,
        .properties = GATT_PROP_WRITE | GATT_PROP_NOTIFY,
        .permissions = GATT_PERM_READ | GATT_PERM_WRITE
    };
    ble_gatt_add_characteristic(g_ble_manager.service_handle,
                                &auth_char,
                                &g_ble_manager.auth_challenge_handle);
    
    /* 4. 控制命令特征 */
    ble_gatt_char_t cmd_char = {
        .uuid = GATT_UUID_CONTROL_COMMAND,
        .properties = GATT_PROP_WRITE,
        .permissions = GATT_PERM_WRITE
    };
    ble_gatt_add_characteristic(g_ble_manager.service_handle,
                                &cmd_char,
                                &g_ble_manager.control_cmd_handle);
    
    /* 5. 会话密钥特征 */
    ble_gatt_char_t session_char = {
        .uuid = GATT_UUID_SESSION_KEY,
        .properties = GATT_PROP_WRITE,
        .permissions = GATT_PERM_WRITE
    };
    ble_gatt_add_characteristic(g_ble_manager.service_handle,
                                &session_char,
                                &g_ble_manager.session_key_handle);
    
    /* 初始化连接槽 */
    memset(g_ble_manager.connections, 0, sizeof(g_ble_manager.connections));
    
    g_ble_manager.initialized = true;
    g_ble_manager.conn_count = 0;
    
    return BLE_SUCCESS;
}

ble_result_t ble_start_advertising(const ble_adv_config_t *config)
{
    if (!g_ble_manager.initialized) {
        return BLE_ERR_NOT_INITIALIZED;
    }
    
    if (g_ble_manager.advertising) {
        return BLE_SUCCESS;  // 已在广播
    }
    
    /* 构建广播数据 */
    uint8_t adv_data[MAX_ADV_DATA_LEN];
    uint8_t adv_len = 0;
    
    /* 标志 */
    adv_data[adv_len++] = 0x02;  // 长度
    adv_data[adv_len++] = 0x01;  // 类型: Flags
    adv_data[adv_len++] = config->flags;
    
    /* 服务UUID列表 */
    if (config->service_uuid_count > 0) {
        uint8_t uuid_len = 2;  // 16-bit UUID
        uint8_t total_len = 1 + uuid_len * config->service_uuid_count;
        adv_data[adv_len++] = total_len;
        adv_data[adv_len++] = 0x03;  // 类型: 16-bit Service UUIDs
        
        for (int i = 0; i < config->service_uuid_count && i < 8; i++) {
            adv_data[adv_len++] = config->service_uuids[i] & 0xFF;
            adv_data[adv_len++] = (config->service_uuids[i] >> 8) & 0xFF;
        }
    }
    
    /* 设备名称 */
    uint8_t name_len = strlen((char*)config->local_name);
    if (adv_len + 2 + name_len <= MAX_ADV_DATA_LEN) {
        adv_data[adv_len++] = 1 + name_len;
        adv_data[adv_len++] = 0x09;  // 类型: Complete Local Name
        memcpy(&adv_data[adv_len], config->local_name, name_len);
        adv_len += name_len;
    }
    
    /* 开始广播 */
    ble_adapter_adv_params_t adv_params = {
        .interval_min = config->min_interval_ms * 1000 / 625,  // 转换为0.625ms单位
        .interval_max = config->max_interval_ms * 1000 / 625,
        .type = BLE_ADV_TYPE_CONNECTABLE_UNDIRECTED,
        .duration = 0  // 永久
    };
    
    if (ble_adapter_start_advertising(adv_data, adv_len, &adv_params) != ADAPTER_SUCCESS) {
        return BLE_ERR_HARDWARE_FAULT;
    }
    
    g_ble_manager.advertising = true;
    memcpy(&g_ble_manager.adv_config, config, sizeof(ble_adv_config_t));
    
    return BLE_SUCCESS;
}

ble_result_t ble_stop_advertising(void)
{
    if (!g_ble_manager.initialized) {
        return BLE_ERR_NOT_INITIALIZED;
    }
    
    if (!g_ble_manager.advertising) {
        return BLE_SUCCESS;
    }
    
    if (ble_adapter_stop_advertising() != ADAPTER_SUCCESS) {
        return BLE_ERR_HARDWARE_FAULT;
    }
    
    g_ble_manager.advertising = false;
    return BLE_SUCCESS;
}

ble_result_t ble_connect(const ble_device_t *device, 
                         const ble_conn_params_t *params,
                         uint16_t *conn_handle)
{
    if (!g_ble_manager.initialized) {
        return BLE_ERR_NOT_INITIALIZED;
    }
    
    int16_t slot = find_free_connection_slot();
    if (slot < 0) {
        return BLE_ERR_CONNECTION_FAILED;
    }
    
    ble_adapter_conn_params_t adapter_params = {
        .addr_type = device->address.type,
        .address = device->address.addr,
        .conn_interval_min = params->conn_interval_min,
        .conn_interval_max = params->conn_interval_max,
        .slave_latency = params->slave_latency,
        .supervision_timeout = params->supervision_timeout
    };
    
    uint16_t handle;
    if (ble_adapter_connect(&adapter_params, &handle) != ADAPTER_SUCCESS) {
        return BLE_ERR_CONNECTION_FAILED;
    }
    
    /* 更新连接上下文 */
    conn_context_t *ctx = &g_ble_manager.connections[slot];
    ctx->in_use = true;
    ctx->info.conn_handle = handle;
    memcpy(&ctx->info.device, device, sizeof(ble_device_t));
    memcpy(&ctx->info.params, params, sizeof(ble_conn_params_t));
    ctx->info.state = BLE_CONN_STATE_CONNECTING;
    ctx->last_activity = 0;
    
    *conn_handle = handle;
    g_ble_manager.conn_count++;
    
    return BLE_SUCCESS;
}

ble_result_t ble_disconnect(uint16_t conn_handle)
{
    if (!g_ble_manager.initialized) {
        return BLE_ERR_NOT_INITIALIZED;
    }
    
    conn_context_t *ctx = find_connection(conn_handle);
    if (!ctx) {
        return BLE_ERR_CONNECTION_FAILED;
    }
    
    if (ble_adapter_disconnect(conn_handle) != ADAPTER_SUCCESS) {
        return BLE_ERR_HARDWARE_FAULT;
    }
    
    ctx->info.state = BLE_CONN_STATE_DISCONNECTING;
    return BLE_SUCCESS;
}

ble_result_t ble_send_data(uint16_t conn_handle, 
                           const uint8_t *data, 
                           uint16_t length)
{
    if (!g_ble_manager.initialized) {
        return BLE_ERR_NOT_INITIALIZED;
    }
    
    conn_context_t *ctx = find_connection(conn_handle);
    if (!ctx || ctx->info.state != BLE_CONN_STATE_ENCRYPTED) {
        return BLE_ERR_CONNECTION_FAILED;
    }
    
    if (length > sizeof(ctx->tx_buffer)) {
        return BLE_ERR_BUFFER_OVERFLOW;
    }
    
    if (ctx->pending_packets >= MAX_PENDING_PACKETS) {
        return BLE_ERR_BUFFER_OVERFLOW;
    }
    
    /* 通过GATT发送通知 */
    if (ble_gatt_send_notification(conn_handle, 
                                    g_ble_manager.key_status_handle,
                                    data, length) != GATT_SUCCESS) {
        return BLE_ERR_HARDWARE_FAULT;
    }
    
    ctx->pending_packets++;
    ctx->last_activity = 0;  // TODO: 获取实际时间
    
    return BLE_SUCCESS;
}

ble_result_t ble_register_callback(ble_event_callback_t callback)
{
    if (!g_ble_manager.initialized) {
        return BLE_ERR_NOT_INITIALIZED;
    }
    
    g_ble_manager.event_callback = callback;
    return BLE_SUCCESS;
}

ble_result_t ble_get_connection_info(uint16_t conn_handle, 
                                     ble_connection_t *conn_info)
{
    if (!g_ble_manager.initialized) {
        return BLE_ERR_NOT_INITIALIZED;
    }
    
    conn_context_t *ctx = find_connection(conn_handle);
    if (!ctx) {
        return BLE_ERR_CONNECTION_FAILED;
    }
    
    memcpy(conn_info, &ctx->info, sizeof(ble_connection_t));
    return BLE_SUCCESS;
}

/*============================================================================
 * 私有函数实现
 *===========================================================================*/

static int16_t find_free_connection_slot(void)
{
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_ble_manager.connections[i].in_use) {
            return i;
        }
    }
    return -1;
}

static conn_context_t* find_connection(uint16_t conn_handle)
{
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_ble_manager.connections[i].in_use &&
            g_ble_manager.connections[i].info.conn_handle == conn_handle) {
            return &g_ble_manager.connections[i];
        }
    }
    return NULL;
}

static void ble_adapter_event_handler(uint8_t event, void *data)
{
    switch (event) {
        case BLE_ADAPTER_EVENT_CONNECTED:
        {
            ble_adapter_conn_evt_t *evt = (ble_adapter_conn_evt_t*)data;
            conn_context_t *ctx = find_connection(evt->conn_handle);
            if (ctx) {
                ctx->info.state = BLE_CONN_STATE_CONNECTED;
                ctx->last_activity = 0;  // TODO
                
                /* 启动加密 */
                ble_adapter_start_encryption(evt->conn_handle);
            }
            break;
        }
        
        case BLE_ADAPTER_EVENT_DISCONNECTED:
        {
            ble_adapter_disconn_evt_t *evt = (ble_adapter_disconn_evt_t*)data;
            conn_context_t *ctx = find_connection(evt->conn_handle);
            if (ctx) {
                ctx->info.state = BLE_CONN_STATE_DISCONNECTED;
                ctx->in_use = false;
                ctx->pending_packets = 0;
                g_ble_manager.conn_count--;
            }
            break;
        }
        
        case BLE_ADAPTER_EVENT_ENCRYPTION_COMPLETE:
        {
            ble_adapter_enc_evt_t *evt = (ble_adapter_enc_evt_t*)data;
            conn_context_t *ctx = find_connection(evt->conn_handle);
            if (ctx && evt->success) {
                ctx->info.state = BLE_CONN_STATE_ENCRYPTED;
            }
            break;
        }
        
        case BLE_ADAPTER_EVENT_DATA_RECEIVED:
        {
            ble_adapter_data_evt_t *evt = (ble_adapter_data_evt_t*)data;
            conn_context_t *ctx = find_connection(evt->conn_handle);
            if (ctx) {
                /* 处理接收数据 */
                if (ctx->rx_len + evt->length <= sizeof(ctx->rx_buffer)) {
                    memcpy(&ctx->rx_buffer[ctx->rx_len], evt->data, evt->length);
                    ctx->rx_len += evt->length;
                }
                ctx->last_activity = 0;  // TODO
            }
            break;
        }
        
        case BLE_ADAPTER_EVENT_DATA_SENT:
        {
            ble_adapter_data_evt_t *evt = (ble_adapter_data_evt_t*)data;
            conn_context_t *ctx = find_connection(evt->conn_handle);
            if (ctx && ctx->pending_packets > 0) {
                ctx->pending_packets--;
            }
            break;
        }
    }
    
    /* 调用用户回调 */
    if (g_ble_manager.event_callback) {
        g_ble_manager.event_callback(event, data);
    }
}

/*============================================================================
 * End of file
 *===========================================================================*/
