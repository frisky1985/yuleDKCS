/**
 * @file vehicle_integration.c
 * @brief 车辆集成模块实现
 * @version 1.0
 * @date 2026-05-05
 * 
 * 提供CAN总线通信和车辆控制功能
 */

#include "vehicle_integration.h"
#include "can_driver.h"
#include <string.h>

/* 私有定义 */
#define CAN_TX_TIMEOUT_MS       100
#define CAN_RX_TIMEOUT_MS       1000
#define MAX_CAN_FILTERS         16
#define STATE_UPDATE_INTERVAL   100

/* CAN消息ID定义 */
#define CAN_ID_DIGITAL_KEY_CMD  0x700
#define CAN_ID_DIGITAL_KEY_RSP  0x701
#define CAN_ID_VEHICLE_STATE    0x800
#define CAN_ID_DOOR_STATUS      0x801
#define CAN_ID_ENGINE_STATUS    0x802
#define CAN_ID_BATTERY_STATUS   0x803

/* 车辆集成实例 */
typedef struct {
    bool initialized;
    bool monitoring;
    vehicle_state_t current_state;
    void (*state_callback)(const vehicle_state_t*);
    uint32_t last_update;
    uint8_t pending_command;
    command_result_t last_result;
} vehicle_integration_t;

/* 全局实例 */
static vehicle_integration_t g_vehicle = {0};

/* CAN接收缓冲区 */
static can_message_t g_can_rx_buffer[32];
static uint32_t g_can_rx_count = 0;

/* 前向声明 */
static void can_rx_handler(const can_message_t *msg);
static int32_t process_vehicle_state_msg(const can_message_t *msg);
static int32_t process_door_status_msg(const can_message_t *msg);
static int32_t process_engine_status_msg(const can_message_t *msg);
static int32_t process_command_response(const can_message_t *msg);
static uint8_t calculate_checksum(const uint8_t *data, uint8_t len);

/*============================================================================
 * 公开API实现
 *===========================================================================*/

vehicle_result_t vehicle_init(void)
{
    if (g_vehicle.initialized) {
        return VEHICLE_SUCCESS;
    }
    
    /* 初始化CAN驱动 */
    can_driver_config_t can_config = {
        .baudrate = 500000,  // 500kbps
        .mode = CAN_MODE_NORMAL,
        .tx_timeout_ms = CAN_TX_TIMEOUT_MS,
        .rx_timeout_ms = CAN_RX_TIMEOUT_MS
    };
    
    if (can_driver_init(&can_config) != CAN_SUCCESS) {
        return VEHICLE_ERR_CAN_INIT_FAILED;
    }
    
    /* 设置CAN过滤器 */
    can_filter_t filters[] = {
        {.id = CAN_ID_DIGITAL_KEY_RSP, .mask = 0x7FF},
        {.id = CAN_ID_VEHICLE_STATE, .mask = 0x7FF},
        {.id = CAN_ID_DOOR_STATUS, .mask = 0x7FF},
        {.id = CAN_ID_ENGINE_STATUS, .mask = 0x7FF},
        {.id = CAN_ID_BATTERY_STATUS, .mask = 0x7FF}
    };
    
    if (can_driver_set_filters(filters, sizeof(filters)/sizeof(can_filter_t)) != CAN_SUCCESS) {
        return VEHICLE_ERR_CAN_INIT_FAILED;
    }
    
    /* 注册接收回调 */
    can_driver_register_rx_handler(can_rx_handler);
    
    /* 启动CAN驱动 */
    if (can_driver_start() != CAN_SUCCESS) {
        return VEHICLE_ERR_CAN_INIT_FAILED;
    }
    
    memset(&g_vehicle.current_state, 0, sizeof(vehicle_state_t));
    g_vehicle.state_callback = NULL;
    g_vehicle.monitoring = false;
    g_vehicle.initialized = true;
    
    return VEHICLE_SUCCESS;
}

vehicle_result_t vehicle_execute_command(const vehicle_command_t *cmd,
                                         command_result_t *result)
{
    if (!g_vehicle.initialized || !cmd || !result) {
        return VEHICLE_ERR_INVALID_COMMAND;
    }
    
    memset(result, 0, sizeof(command_result_t));
    
    /* 构建CAN消息 */
    can_message_t can_msg;
    can_msg.id = CAN_ID_DIGITAL_KEY_CMD;
    can_msg.dlc = 8;
    
    /* 填充数据 */
    can_msg.data[0] = cmd->command_type;
    can_msg.data[1] = cmd->target;
    can_msg.data[2] = (cmd->user_id >> 0) & 0xFF;
    can_msg.data[3] = (cmd->user_id >> 8) & 0xFF;
    can_msg.data[4] = (cmd->user_id >> 16) & 0xFF;
    can_msg.data[5] = (cmd->user_id >> 24) & 0xFF;
    can_msg.data[6] = (cmd->timestamp >> 0) & 0xFF;
    can_msg.data[7] = calculate_checksum(can_msg.data, 7);
    
    /* 发送命令 */
    if (can_driver_send(&can_msg, CAN_TX_TIMEOUT_MS) != CAN_SUCCESS) {
        result->command_type = cmd->command_type;
        result->result = 0xFF;
        result->error_code = 0x01;
        return VEHICLE_ERR_CAN_SEND_FAILED;
    }
    
    g_vehicle.pending_command = cmd->command_type;
    
    /* 等待响应 */
    uint32_t start_time = 0;  // TODO: 获取实际时间
    uint32_t timeout = 500;   // 500ms超时
    
    while ((0 - start_time) < timeout) {  // TODO: 时间比较
        if (g_vehicle.last_result.command_type == cmd->command_type) {
            memcpy(result, &g_vehicle.last_result, sizeof(command_result_t));
            g_vehicle.pending_command = 0;
            return (result->result == 0) ? VEHICLE_SUCCESS : VEHICLE_ERR_EXECUTION_FAILED;
        }
    }
    
    /* 超时 */
    result->command_type = cmd->command_type;
    result->result = 0xFF;
    result->error_code = 0x02;
    g_vehicle.pending_command = 0;
    
    return VEHICLE_ERR_TIMEOUT;
}

vehicle_result_t vehicle_get_state(vehicle_state_t *state)
{
    if (!g_vehicle.initialized || !state) {
        return VEHICLE_ERR_INVALID_PARAM;
    }
    
    memcpy(state, &g_vehicle.current_state, sizeof(vehicle_state_t));
    return VEHICLE_SUCCESS;
}

vehicle_result_t vehicle_register_state_callback(
    void (*callback)(const vehicle_state_t *state))
{
    if (!g_vehicle.initialized) {
        return VEHICLE_ERR_INVALID_PARAM;
    }
    
    g_vehicle.state_callback = callback;
    return VEHICLE_SUCCESS;
}

vehicle_result_t vehicle_send_can(const can_message_t *msg)
{
    if (!g_vehicle.initialized || !msg) {
        return VEHICLE_ERR_INVALID_PARAM;
    }
    
    if (can_driver_send(msg, CAN_TX_TIMEOUT_MS) != CAN_SUCCESS) {
        return VEHICLE_ERR_CAN_SEND_FAILED;
    }
    
    return VEHICLE_SUCCESS;
}

vehicle_result_t vehicle_receive_can(can_message_t *msg, uint32_t timeout_ms)
{
    if (!g_vehicle.initialized || !msg) {
        return VEHICLE_ERR_INVALID_PARAM;
    }
    
    if (can_driver_receive(msg, timeout_ms) != CAN_SUCCESS) {
        return VEHICLE_ERR_CAN_RECEIVE_FAILED;
    }
    
    return VEHICLE_SUCCESS;
}

vehicle_result_t vehicle_start_monitoring(void)
{
    if (!g_vehicle.initialized) {
        return VEHICLE_ERR_INVALID_PARAM;
    }
    
    g_vehicle.monitoring = true;
    return VEHICLE_SUCCESS;
}

vehicle_result_t vehicle_stop_monitoring(void)
{
    if (!g_vehicle.initialized) {
        return VEHICLE_ERR_INVALID_PARAM;
    }
    
    g_vehicle.monitoring = false;
    return VEHICLE_SUCCESS;
}

/*============================================================================
 * 私有函数实现
 *===========================================================================*/

static void can_rx_handler(const can_message_t *msg)
{
    if (!g_vehicle.initialized) {
        return;
    }
    
    /* 根据CAN ID分发消息 */
    switch (msg->id) {
        case CAN_ID_DIGITAL_KEY_RSP:
            process_command_response(msg);
            break;
            
        case CAN_ID_VEHICLE_STATE:
            process_vehicle_state_msg(msg);
            break;
            
        case CAN_ID_DOOR_STATUS:
            process_door_status_msg(msg);
            break;
            
        case CAN_ID_ENGINE_STATUS:
            process_engine_status_msg(msg);
            break;
            
        default:
            /* 保存到缓冲区 */
            if (g_can_rx_count < sizeof(g_can_rx_buffer)/sizeof(can_message_t)) {
                memcpy(&g_can_rx_buffer[g_can_rx_count++], msg, sizeof(can_message_t));
            }
            break;
    }
}

static int32_t process_vehicle_state_msg(const can_message_t *msg)
{
    if (msg->dlc < 8) {
        return -1;
    }
    
    /* 解析车辆状态 */
    /* 格式: [status_flags][battery_volt_L][battery_volt_H][...] */
    
    g_vehicle.current_state.lock_status = msg->data[0] & 0x0F;
    g_vehicle.current_state.alarm_status = (msg->data[0] >> 4) & 0x0F;
    g_vehicle.current_state.battery_voltage = msg->data[1] | (msg->data[2] << 8);
    
    g_vehicle.last_update = 0;  // TODO
    
    /* 触发回调 */
    if (g_vehicle.state_callback && g_vehicle.monitoring) {
        g_vehicle.state_callback(&g_vehicle.current_state);
    }
    
    return 0;
}

static int32_t process_door_status_msg(const can_message_t *msg)
{
    if (msg->dlc < 2) {
        return -1;
    }
    
    g_vehicle.current_state.door_status = msg->data[0];
    
    if (msg->dlc >= 5) {
        for (int i = 0; i < 4 && (i + 1) < msg->dlc; i++) {
            g_vehicle.current_state.window_status[i] = msg->data[i + 1];
        }
    }
    
    g_vehicle.last_update = 0;  // TODO
    
    if (g_vehicle.state_callback && g_vehicle.monitoring) {
        g_vehicle.state_callback(&g_vehicle.current_state);
    }
    
    return 0;
}

static int32_t process_engine_status_msg(const can_message_t *msg)
{
    if (msg->dlc < 2) {
        return -1;
    }
    
    g_vehicle.current_state.engine_status = msg->data[0];
    g_vehicle.current_state.gear_position = msg->data[1];
    
    if (msg->dlc >= 6) {
        g_vehicle.current_state.odometer = msg->data[2] |
                                           (msg->data[3] << 8) |
                                           (msg->data[4] << 16) |
                                           (msg->data[5] << 24);
    }
    
    g_vehicle.last_update = 0;  // TODO
    
    if (g_vehicle.state_callback && g_vehicle.monitoring) {
        g_vehicle.state_callback(&g_vehicle.current_state);
    }
    
    return 0;
}

static int32_t process_command_response(const can_message_t *msg)
{
    if (msg->dlc < 3) {
        return -1;
    }
    
    g_vehicle.last_result.command_type = msg->data[0];
    g_vehicle.last_result.result = msg->data[1];
    g_vehicle.last_result.error_code = msg->data[2];
    g_vehicle.last_result.execution_time = 0;  // TODO
    
    if (msg->dlc > 3) {
        uint8_t response_len = msg->dlc - 3;
        if (response_len > sizeof(g_vehicle.last_result.response_data)) {
            response_len = sizeof(g_vehicle.last_result.response_data);
        }
        memcpy(g_vehicle.last_result.response_data, &msg->data[3], response_len);
    }
    
    return 0;
}

static uint8_t calculate_checksum(const uint8_t *data, uint8_t len)
{
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(~sum + 1);
}

/*============================================================================
 * End of file
 *===========================================================================*/
