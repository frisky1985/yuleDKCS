#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#define CAN_SUCCESS 0
#define CAN_MODE_NORMAL 0

/* CAN Message */
typedef struct can_message_s {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} can_message_t;

/* CAN Filter */
typedef struct {
    uint32_t id;
    uint32_t mask;
} can_filter_t;

/* CAN Driver Config */
typedef struct {
    uint32_t baudrate;
    uint8_t mode;
    uint32_t tx_timeout_ms;
    uint32_t rx_timeout_ms;
} can_driver_config_t;

/* CAN Driver API */
int32_t can_driver_init(const can_driver_config_t *config);
int32_t can_driver_set_filters(const can_filter_t *filters, uint8_t count);
int32_t can_driver_register_rx_handler(void (*handler)(const can_message_t *msg));
int32_t can_driver_start(void);
int32_t can_driver_stop(void);
int32_t can_driver_send(const can_message_t *msg, uint32_t timeout_ms);
int32_t can_driver_receive(can_message_t *msg, uint32_t timeout_ms);

#endif /* CAN_DRIVER_H */
