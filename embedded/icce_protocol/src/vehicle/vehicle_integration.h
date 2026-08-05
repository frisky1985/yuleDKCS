#ifndef VEHICLE_INTEGRATION_H
#define VEHICLE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

/* Vehicle Result Codes */
typedef enum {
    VEHICLE_SUCCESS = 0,
    VEHICLE_ERR_CAN_INIT_FAILED = -1,
    VEHICLE_ERR_CAN_SEND_FAILED = -2,
    VEHICLE_ERR_CAN_RECEIVE_FAILED = -3,
    VEHICLE_ERR_INVALID_COMMAND = -4,
    VEHICLE_ERR_INVALID_PARAM = -5,
    VEHICLE_ERR_EXECUTION_FAILED = -6,
    VEHICLE_ERR_TIMEOUT = -7,
} vehicle_result_t;

/* Vehicle State */
typedef struct {
    uint8_t lock_status;
    uint8_t alarm_status;
    uint8_t engine_status;
    uint8_t gear_position;
    uint8_t door_status;
    uint8_t window_status[4];
    uint16_t battery_voltage;
    uint32_t odometer;
} vehicle_state_t;

/* Vehicle Command */
typedef struct {
    uint8_t command_type;
    uint8_t target;
    uint32_t user_id;
    uint32_t timestamp;
} vehicle_command_t;

/* Command Result */
typedef struct {
    uint8_t command_type;
    uint8_t result;
    uint8_t error_code;
    uint32_t execution_time;
    uint8_t response_data[64];
} command_result_t;

/* Can Message (forward declaration - full in can_driver.h) */
typedef struct can_message_s can_message_t;

/* API */
vehicle_result_t vehicle_init(void);
vehicle_result_t vehicle_execute_command(const vehicle_command_t *cmd, command_result_t *result);
vehicle_result_t vehicle_get_state(vehicle_state_t *state);
vehicle_result_t vehicle_register_state_callback(void (*callback)(const vehicle_state_t *state));
vehicle_result_t vehicle_send_can(const can_message_t *msg);
vehicle_result_t vehicle_receive_can(can_message_t *msg, uint32_t timeout_ms);
vehicle_result_t vehicle_start_monitoring(void);
vehicle_result_t vehicle_stop_monitoring(void);

#endif /* VEHICLE_INTEGRATION_H */
