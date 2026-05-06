/**
 * @file ccc_digital_key.h
 * @brief CCC Digital Key Protocol Stack - Main Header
 * @version 1.0
 * @date 2026-05-06
 *
 * Hardware: NXP KW47A (BLE) + NXP NCJ29D6 (UWB) + ST ST25R501 (NFC) + NXP SE050 (Security)
 */

#ifndef CCC_DIGITAL_KEY_H
#define CCC_DIGITAL_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================
 *  Return Codes
 * ======================================================================== */
#define CCC_OK                  (0)
#define CCC_ERR_INVALID_PARAM   (-1)
#define CCC_ERR_NO_MEM          (-2)
#define CCC_ERR_TIMEOUT         (-3)
#define CCC_ERR_BUSY            (-4)
#define CCC_ERR_NOT_INIT        (-5)
#define CCC_ERR_HARDWARE        (-6)
#define CCC_ERR_SECURITY        (-7)
#define CCC_ERR_NOT_FOUND       (-8)
#define CCC_ERR_ALREADY_EXISTS  (-9)
#define CCC_ERR_DENIED          (-10)

/* ========================================================================
 *  Common Types
 * ======================================================================== */
typedef int32_t ccc_status_t;

/* ========================================================================
 *  NFC Module (ST ST25R501)
 * ======================================================================== */
#define NFC_MAX_PAYLOAD     512

typedef struct {
    uint8_t  ble_mac[6];
    uint8_t  uwb_session_id[8];
    uint8_t  uwb_channel;
    uint8_t  uwb_preamble_code;
    uint8_t  capability[4];
    uint8_t  oob_data[32];
} ccc_nfc_oob_data_t;

typedef enum {
    NFC_STATE_IDLE = 0,
    NFC_STATE_FIELD_DETECT,
    NFC_STATE_ANTI_COLLISION,
    NFC_STATE_SELECTED,
    NFC_STATE_DATA_EXCHANGE,
    NFC_STATE_ERROR
} nfc_state_e;

ccc_status_t nfc_st25r501_init(void);
ccc_status_t nfc_st25r501_deinit(void);
bool         nfc_field_detect(void);
nfc_state_e  nfc_get_state(void);
ccc_status_t nfc_start_listen(void);
ccc_status_t nfc_stop_listen(void);
ccc_status_t nfc_send(const uint8_t *data, uint16_t len);
ccc_status_t nfc_recv(uint8_t *buf, uint16_t *len, uint32_t timeout_ms);
ccc_status_t nfc_oob_exchange(ccc_nfc_oob_data_t *oob_out, ccc_nfc_oob_data_t *oob_in);

/* ========================================================================
 *  BLE Module (NXP KW47A)
 * ======================================================================== */
#define BLE_MAX_PAYLOAD     244    /* ATT MTU - 3 */
#define BLE_MAX_CONN        1
#define BLE_ADV_DATA_MAX    31

typedef struct {
    uint8_t  addr[6];
    uint8_t  addr_type;
} ble_addr_t;

typedef struct {
    uint16_t conn_interval;
    uint16_t conn_latency;
    uint16_t supervision_timeout;
    uint16_t mtu;
} ble_conn_param_t;

typedef struct {
    uint8_t  data[BLE_ADV_DATA_MAX];
    uint8_t  len;
    uint16_t interval_min;   /* units of 0.625ms */
    uint16_t interval_max;
} ble_adv_param_t;

typedef void (*ble_recv_cb_t)(uint16_t conn_handle, const uint8_t *data, uint16_t len);
typedef void (*ble_conn_cb_t)(uint16_t conn_handle, uint8_t status);
typedef void (*ble_disconn_cb_t)(uint16_t conn_handle, uint8_t reason);

/* BLE message types */
typedef enum {
    BLE_MSG_PAIR_REQUEST     = 0x01,
    BLE_MSG_PAIR_RESPONSE    = 0x02,
    BLE_MSG_KEY_CREATE       = 0x10,
    BLE_MSG_KEY_DELETE       = 0x11,
    BLE_MSG_KEY_SHARE        = 0x12,
    BLE_MSG_AUTH_REQUEST     = 0x20,
    BLE_MSG_AUTH_RESPONSE    = 0x21,
    BLE_MSG_UWB_CONFIG       = 0x30,
    BLE_MSG_STATE_NOTIFY     = 0x40,
    BLE_MSG_ERROR            = 0xFF
} ble_msg_type_e;

typedef struct __attribute__((packed)) {
    uint8_t  msg_type;
    uint8_t  msg_id;
    uint16_t payload_len;
    uint8_t  reserved;
} ble_frame_header_t;

ccc_status_t ble_kw47a_init(void);
ccc_status_t ble_kw47a_deinit(void);
ccc_status_t ble_start_adv(const ble_adv_param_t *param);
ccc_status_t ble_stop_adv(void);
ccc_status_t ble_connect(const ble_addr_t *addr, const ble_conn_param_t *param);
ccc_status_t ble_disconnect(uint16_t conn_handle);
ccc_status_t ble_send(uint16_t conn_handle, const uint8_t *data, uint16_t len);
ccc_status_t ble_register_recv_cb(ble_recv_cb_t cb);
ccc_status_t ble_register_conn_cb(ble_conn_cb_t cb);
ccc_status_t ble_register_disconn_cb(ble_disconn_cb_t cb);
ccc_status_t ble_oob_pair(const ccc_nfc_oob_data_t *oob);
bool         ble_is_connected(uint16_t conn_handle);

/* ========================================================================
 *  UWB Module (NXP NCJ29D6)
 * ======================================================================== */
#define UWB_MAX_SESSIONS   4
#define UWB_INVALID_SESSION 0xFFFFFFFF

typedef struct {
    uint8_t  session_id[8];
    uint8_t  sts_key[16];
    uint8_t  sts_iv[16];
    uint8_t  channel;          /* 5 or 9 */
    uint8_t  preamble_code;    /* 9 (ch5) or 12 (ch9) */
    uint8_t  prf_len;          /* 128 */
    uint8_t  sfd_id;
    uint8_t  phr_rate;
    uint8_t  data_rate;
    uint8_t  tx_power;
    uint8_t  rframe_config;    /* SP0/SP1/SP3 */
    uint8_t  sts_config;
} uwb_session_config_t;

typedef enum {
    ZONE_LOCKED   = 0,   /* >10m */
    ZONE_APPROACH = 1,   /* 5-10m */
    ZONE_UNLOCK   = 2,   /* 2-5m */
    ZONE_ENTRY    = 3,   /* 0-2m */
    ZONE_INSIDE   = 4    /* <0.5m */
} distance_zone_e;

typedef struct {
    uint16_t approach_cm;   /* default: 1000 */
    uint16_t unlock_cm;     /* default: 500 */
    uint16_t entry_cm;      /* default: 200 */
    uint16_t inside_cm;     /* default: 50 */
    uint16_t hysteresis_cm; /* default: 30 */
} distance_threshold_t;

typedef void (*uwb_zone_cb_t)(uint32_t session_id, distance_zone_e zone, uint16_t distance_cm);

ccc_status_t uwb_ncj29d6_init(void);
ccc_status_t uwb_ncj29d6_deinit(void);
uint32_t     uwb_create_session(const uwb_session_config_t *cfg);
ccc_status_t uwb_destroy_session(uint32_t session_id);
ccc_status_t uwb_start_ranging(uint32_t session_id);
ccc_status_t uwb_stop_ranging(uint32_t session_id);
ccc_status_t uwb_get_distance(uint32_t session_id, uint16_t *dist_cm);
distance_zone_e uwb_get_zone(uint32_t session_id);
ccc_status_t uwb_set_threshold(const distance_threshold_t *th);
ccc_status_t uwb_register_zone_cb(uwb_zone_cb_t cb);

/* ========================================================================
 *  Key Management Module
 * ======================================================================== */
#define KEY_ID_LEN          16
#define VEHICLE_ID_LEN      16
#define OWNER_ID_LEN        16
#define CERT_MAX_LEN        256
#define ATTESTATION_LEN     128
#define MAX_KEYS            8

typedef enum {
    KEY_TYPE_OWNER     = 0x01,
    KEY_TYPE_FRIEND    = 0x02,
    KEY_TYPE_SERVICE   = 0x03,
    KEY_TYPE_TEMPORARY = 0x04
} key_type_e;

typedef enum {
    KEY_STATE_INACTIVE  = 0x00,
    KEY_STATE_ACTIVE    = 0x01,
    KEY_STATE_SUSPENDED = 0x02,
    KEY_STATE_EXPIRED   = 0x03,
    KEY_STATE_REVOKED   = 0x04
} key_state_e;

#define ACCESS_LOCK_UNLOCK  (1 << 0)
#define ACCESS_ENGINE_START (1 << 1)
#define ACCESS_TRUNK        (1 << 2)
#define ACCESS_WINDOWS      (1 << 3)
#define ACCESS_SUNROOF      (1 << 4)
#define ACCESS_CLIMATE      (1 << 5)
#define ACCESS_SEAT         (1 << 6)
#define ACCESS_FUEL_DOOR    (1 << 7)

typedef struct __attribute__((packed)) {
    uint8_t  key_id[KEY_ID_LEN];
    uint8_t  vehicle_id[VEHICLE_ID_LEN];
    uint8_t  owner_id[OWNER_ID_LEN];
    uint8_t  key_type;
    uint8_t  access_rights[4];
    uint8_t  restrictions[4];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t  state;
    uint8_t  version;
    uint8_t  ca_cert[CERT_MAX_LEN];
    uint16_t ca_cert_len;
    uint8_t  device_cert[CERT_MAX_LEN];
    uint16_t device_cert_len;
    uint8_t  attestation[ATTESTATION_LEN];
    uint16_t attestation_len;
    uint8_t  se_key_id;
    uint8_t  reserved[3];
} ccc_digital_key_t;

ccc_status_t key_mgmt_init(void);
ccc_status_t key_mgmt_deinit(void);
ccc_status_t key_create(ccc_digital_key_t *key);
ccc_status_t key_delete(const uint8_t *key_id);
ccc_status_t key_get(const uint8_t *key_id, ccc_digital_key_t *key);
ccc_status_t key_list(ccc_digital_key_t *keys, uint8_t *count);
ccc_status_t key_share(const uint8_t *key_id, key_type_e type, uint32_t duration_s);
ccc_status_t key_revoke(const uint8_t *key_id);
ccc_status_t key_suspend(const uint8_t *key_id);
ccc_status_t key_resume(const uint8_t *key_id);
ccc_status_t key_validate(const uint8_t *key_id);

/* ========================================================================
 *  Security Module (SE050)
 * ======================================================================== */
typedef struct {
    uint8_t  enc_key[16];
    uint8_t  mac_key[16];
    uint8_t  dek_key[16];
    uint8_t  host_challenge[8];
    uint8_t  card_challenge[8];
    uint8_t  seq_counter[2];
    uint8_t  chain_mode;
} scp03_channel_t;

typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  nonce[16];
    uint8_t  device_id[16];
    uint8_t  key_id[16];
    uint8_t  key_type;
    uint8_t  access_rights[4];
    uint8_t  firmware_hash[32];
    uint8_t  security_state;
    uint8_t  attestation_cert[256];
    uint16_t attestation_cert_len;
    uint8_t  signature[64];
} ccc_attestation_t;

typedef enum {
    VERIFY_OK                = 0x00,
    VERIFY_CERT_INVALID      = 0x01,
    VERIFY_SIGN_INVALID      = 0x02,
    VERIFY_KEY_EXPIRED       = 0x03,
    VERIFY_KEY_REVOKED       = 0x04,
    VERIFY_PERMISSION_DENIED = 0x05,
    VERIFY_FW_TAMPERED       = 0x06
} verify_result_e;

ccc_status_t  sec_init(void);
ccc_status_t  sec_deinit(void);
ccc_status_t  sec_scp03_open(scp03_channel_t *ch);
ccc_status_t  sec_scp03_close(scp03_channel_t *ch);
ccc_status_t  sec_encrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len);
ccc_status_t  sec_decrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len);
ccc_status_t  sec_sign(const uint8_t *data, uint32_t len, uint8_t *sig, uint32_t *sig_len);
verify_result_e sec_verify(const uint8_t *data, uint32_t len, const uint8_t *sig, uint32_t sig_len);
ccc_status_t  sec_attestation(ccc_attestation_t *att);
verify_result_e sec_verify_attestation(const ccc_attestation_t *att);

/* ========================================================================
 *  Main State Machine
 * ======================================================================== */
typedef enum {
    STATE_INIT         = 0,
    STATE_STANDBY,
    STATE_NFC_DETECT,
    STATE_NFC_READ,
    STATE_UWB_RANGING,
    STATE_BLE_CONNECTED,
    STATE_AUTH_PROCESS,
    STATE_UNLOCKED,
    STATE_LOCKED,
    STATE_ERROR
} main_state_e;

typedef struct {
    main_state_e state;
    uint16_t     uwb_distance_cm;
    int8_t       ble_rssi;
    bool         nfc_field;
    bool         ble_connected;
    uint8_t      active_key_count;
} system_status_t;

ccc_status_t ccc_dk_init(void);
ccc_status_t ccc_dk_deinit(void);
ccc_status_t ccc_dk_run(void);    /* main loop tick */
main_state_e ccc_dk_get_state(void);
ccc_status_t ccc_dk_get_status(system_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* CCC_DIGITAL_KEY_H */
