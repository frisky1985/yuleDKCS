/**
 * @file iccoa_digital_key.h
 * @brief ICCOA Digital Key Protocol Stack - Main Header
 * @version 1.0
 * @date 2026-05-06
 */

#ifndef ICCOA_DIGITAL_KEY_H
#define ICCOA_DIGITAL_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================
 *  Return Codes
 * ======================================================================== */
#define ICCOA_OK                (0)
#define ICCOA_ERR_PARAM         (-1)
#define ICCOA_ERR_NO_MEM        (-2)
#define ICCOA_ERR_TIMEOUT       (-3)
#define ICCOA_ERR_BUSY          (-4)
#define ICCOA_ERR_NOT_INIT      (-5)
#define ICCOA_ERR_HARDWARE      (-6)
#define ICCOA_ERR_SECURITY      (-7)
#define ICCOA_ERR_NOT_FOUND     (-8)
#define ICCOA_ERR_EXISTS        (-9)
#define ICCOA_ERR_DENIED        (-10)

/* ========================================================================
 *  BLE Communication
 * ======================================================================== */
#define ICCOA_MAX_PAYLOAD   244
#define ICCOA_SERVICE_UUID  0xFEF5

typedef void (*iccoa_ble_recv_cb_t)(const uint8_t *data, uint16_t len);

int32_t iccoa_ble_init(void);
int32_t iccoa_ble_deinit(void);
int32_t iccoa_ble_start_adv(void);
int32_t iccoa_ble_stop_adv(void);
int32_t iccoa_ble_send(const uint8_t *data, uint16_t len);
int32_t iccoa_ble_register_cb(iccoa_ble_recv_cb_t cb);

/* ========================================================================
 *  ICCOA DK 3.0 Protocol
 * ======================================================================== */
#define DK30_SOP    0xAA
#define DK30_EOP    0x55

typedef enum {
    ICCOA_CMD_BIND_REQ     = 0x01,
    ICCOA_CMD_BIND_RSP     = 0x02,
    ICCOA_CMD_UNBIND_REQ   = 0x03,
    ICCOA_CMD_UNBIND_RSP   = 0x04,
    ICCOA_CMD_AUTH_REQ     = 0x10,
    ICCOA_CMD_AUTH_RSP     = 0x11,
    ICCOA_CMD_CTRL_REQ     = 0x20,
    ICCOA_CMD_CTRL_RSP     = 0x21,
    ICCOA_CMD_STATUS_NTF   = 0x30,
    ICCOA_CMD_KEY_SHARE    = 0x40,
    ICCOA_CMD_KEY_SHARE_ACK= 0x41,
    ICCOA_CMD_ERROR        = 0xFF
} iccoa_cmd_e;

typedef struct __attribute__((packed)) {
    uint8_t  sop;
    uint8_t  cmd_id;
    uint16_t seq_num;
    uint16_t payload_len;
    uint8_t  payload[ICCOA_MAX_PAYLOAD];
    uint8_t  checksum;
    uint8_t  eop;
} iccoa_dk30_frame_t;

int32_t iccoa_dk30_init(void);
int32_t iccoa_dk30_process(const uint8_t *raw, uint16_t len);
int32_t iccoa_dk30_send_response(iccoa_cmd_e cmd, const uint8_t *payload, uint16_t len);
uint8_t iccoa_dk30_checksum(const uint8_t *data, uint16_t len);

/* ========================================================================
 *  ICCOA DK 4.0 Protocol
 * ======================================================================== */
#define DK40_MAGIC  0xICC0

typedef enum {
    ICCOA_V4_SESSION_OPEN  = 0x01,
    ICCOA_V4_SESSION_CLOSE = 0x02,
    ICCOA_V4_BIND          = 0x10,
    ICCOA_V4_AUTH          = 0x20,
    ICCOA_V4_CTRL          = 0x30,
    ICCOA_V4_UWB_CONFIG    = 0x40,
    ICCOA_V4_SHARE         = 0x50,
    ICCOA_V4_NOTIFY        = 0x60,
    ICCOA_V4_ERROR         = 0xFF
} iccoa_v4_cmd_e;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t msg_id;
    uint16_t flags;
    uint16_t payload_len;
    uint8_t  session_token[4];
    uint8_t  payload[ICCOA_MAX_PAYLOAD];
    uint8_t  hmac[16];
} iccoa_dk40_frame_t;

int32_t iccoa_dk40_init(void);
int32_t iccoa_dk40_process(const uint8_t *raw, uint16_t len);
int32_t iccoa_dk40_send_response(iccoa_v4_cmd_e cmd, const uint8_t *payload, uint16_t len);

/* ========================================================================
 *  Authorization
 * ======================================================================== */
#define ICCOA_PERM_LOCK     (1 << 0)
#define ICCOA_PERM_UNLOCK   (1 << 1)
#define ICCOA_PERM_ENGINE   (1 << 2)
#define ICCOA_PERM_TRUNK    (1 << 3)
#define ICCOA_PERM_WINDOW   (1 << 4)
#define ICCOA_PERM_CLIMATE  (1 << 5)
#define ICCOA_PERM_FIND     (1 << 6)
#define ICCOA_PERM_SEAT     (1 << 7)

typedef struct {
    uint8_t  user_id[16];
    uint8_t  permissions[4];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t  max_uses;
    uint8_t  used_count;
} iccoa_permission_t;

typedef enum {
    ICCOA_AUTH_BIND   = 0x01,
    ICCOA_AUTH_DAILY  = 0x02,
    ICCOA_AUTH_REMOTE = 0x03,
    ICCOA_AUTH_SHARE  = 0x04
} iccoa_auth_type_e;

int32_t iccoa_auth_init(void);
int32_t iccoa_auth_request(iccoa_auth_type_e type, const uint8_t *challenge, uint16_t len);
int32_t iccoa_auth_verify(const uint8_t *response, uint16_t len);
int32_t iccoa_auth_check_permission(const uint8_t *user_id, uint8_t perm);

/* ========================================================================
 *  Vehicle Service
 * ======================================================================== */
typedef enum {
    CTRL_LOCK       = 0x01,
    CTRL_UNLOCK     = 0x02,
    CTRL_ENGINE_ON  = 0x03,
    CTRL_ENGINE_OFF = 0x04,
    CTRL_TRUNK_OPEN = 0x05,
    CTRL_WINDOW_UP  = 0x06,
    CTRL_WINDOW_DOWN= 0x07,
    CTRL_CLIMATE_ON = 0x08,
    CTRL_CLIMATE_OFF= 0x09,
    CTRL_FIND       = 0x0A,
    CTRL_HORN       = 0x0B
} iccoa_ctrl_cmd_e;

typedef struct __attribute__((packed)) {
    uint8_t  door_status;
    uint8_t  window_status;
    uint8_t  engine_status;
    uint8_t  lock_status;
    int8_t   battery_level;
    int16_t  interior_temp;
    uint8_t  alarm_status;
    uint8_t  reserved[3];
} iccoa_vehicle_status_t;

int32_t iccoa_service_init(void);
int32_t iccoa_ctrl_execute(iccoa_ctrl_cmd_e cmd, uint8_t param);
int32_t iccoa_service_get_status(iccoa_vehicle_status_t *status);

/* ========================================================================
 *  Main Entry
 * ======================================================================== */
int32_t iccoa_dk_init(void);
int32_t iccoa_dk_deinit(void);
int32_t iccoa_dk_run(void);

#ifdef __cplusplus
}
#endif

#endif /* ICCOA_DIGITAL_KEY_H */
