/**
 * @file ccc_digital_key.h
 * @module EMB-SWC-CCC (ASPICE SWE.6)
 * @brief CCC Digital Key Protocol Stack - Main Header
 * @version 1.0
 * @date 2026-05-06
 * Layer: Application Layer (SWC)
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
#define CCC_ERR_PARAM           (-11)  /* Malformed input (bad blob/length) */
#define CCC_ERR_STATE           (-12)  /* State conflict (replay / locked) */

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
#include "hardware_abstraction.h"   /* CCC_SERVICE_UUID / ICCOA_SERVICE_UUID / ICCE_SERVICE_UUID 单一来源 */

/* 便捷别名 — CCC BLE Service UUID (主用 v4.0, 见 hardware_abstraction.h) */
#define CCC_BLE_SERVICE_UUID   CCC_SERVICE_UUID
#define BLE_MAX_PAYLOAD        244     /* ATT MTU - 3 */
#define BLE_MAX_CONN           1
#define BLE_ADV_DATA_MAX       31

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

/* ========================================================================
 *  CCC BLE v4.0 帧格式 (CCC-TS-101 Table 19-19) — 单一来源
 *
 * 线格式 (4 字节帧头 + 负载):
 *    [0]     message_header  Bit[5:0]=Message Type, Bit[7:6]=RFU(置 0)
 *    [1]     payload_header  Message ID (DK_APDU_RQ=0x0B / DK_APDU_RS=0x0C)
 *    [2-3]   length          负载长度 (大端 u16)
 *    [4...]  payload
 *
 * 规范示例: SELECT APDU → 0x01 0x0B 0x00 0x13 00A404000DA000000809434343444B41763100
 *
 * 注: 旧 R3.0 参考实现定义过 5B 帧头 (msg_type+msg_id+len_LE+reserved) 与
 *     非规范 Message Type 值域 (0x01/0x20/0x30/0x40...)，但从未在 ble_send()
 *     中实际封装，纯死代码。两端协议不一致导致根本无法互通。
 *     本定义为 v4.0 规范收敛，车端与手机端 (iOS/Android) 必须严格一致。
 * ======================================================================== */

/** CCC Message Type (CCC-TS-101 Table 19-21, Bit[5:0]) */
typedef enum {
    CCC_MSG_TYPE_FRAMEWORK        = 0,  /**< Framework message */
    CCC_MSG_TYPE_SE               = 1,  /**< Secure Element APDU */
    CCC_MSG_TYPE_UWB_RANGING      = 2,  /**< UWB Ranging Service */
    CCC_MSG_TYPE_DK_EVENT_NOTIFY  = 3,  /**< DK Event Notification */
    CCC_MSG_TYPE_VEHICLE_OEM_APP  = 4,  /**< Vehicle OEM App */
    CCC_MSG_TYPE_SUPPLEMENTARY    = 5,  /**< Supplementary Service */
    CCC_MSG_TYPE_HEAD_UNIT_PAIR   = 6   /**< Head Unit Pairing */
} ccc_msg_type_e;

/** CCC Payload Header (Message ID, §19.3.2) */
typedef enum {
    CCC_MSG_ID_DK_APDU_RQ  = 0x0B,   /**< DK APDU Request  (手机 → 车) */
    CCC_MSG_ID_DK_APDU_RS  = 0x0C    /**< DK APDU Response (车 → 手机) */
} ccc_msg_id_e;

/** CCC v4.0 帧头 — 4B packed, 大端 length */
typedef struct __attribute__((packed)) {
    uint8_t  message_header;   /**< Bit[5:0]=ccc_msg_type_e, Bit[7:6]=RFU(0) */
    uint8_t  payload_header;   /**< ccc_msg_id_e */
    uint16_t payload_len_be;   /**< 负载长度 (大端 u16, 无线传输序) */
} ccc_frame_header_v4_t;

/** 帧头长度常量 — 供手机端/测试脚本引用 */
#define CCC_FRAME_HEADER_LEN   4

/** RFU 掩码: message_header 高 2 位必须置 0 */
#define CCC_MSG_TYPE_MASK      0x3F
#define CCC_MSG_TYPE_RFU_MASK  0xC0

/* ========================================================================
 *  R3.0 兼容 (旧参考实现, 不建议新代码使用)
 *  仅在车端与存量 R3.0 设备互操作时启用, v4.0 主路径禁止使用
 * ======================================================================== */
#if defined(CCC_R30_COMPAT)
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
#endif /* CCC_R30_COMPAT */

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
 *  CCC v4.0 帧编解码 + 高层发送 (单一来源, 手机端严格对齐)
 *
 * 帧格式: ccc_frame_header_v4_t (4B) + payload, 详见上方定义.
 *
 * 用法:
 *   // 方式 1: 高层一键发送 (内部自动分配栈上 buffer)
 *   ccc_ble_send_v4(h, CCC_MSG_TYPE_SE, CCC_MSG_ID_DK_APDU_RS, payload, len);
 *
 *   // 方式 2: 手动构建帧 (大 payload 需外部 buffer)
 *   uint8_t frame[CCC_FRAME_HEADER_LEN + 512];
 *   uint16_t frame_len;
 *   ccc_frame_build(CCC_MSG_TYPE_SE, CCC_MSG_ID_DK_APDU_RS,
 *                   payload, len, frame, &frame_len);
 *   ble_send(h, frame, frame_len);
 * ======================================================================== */

/** 构建 v4.0 帧: 帧头 + payload, 输出到 out_frame.
 *  out_frame 必须 ≥ 4 + payload_len 字节.
 *  RFU 位自动置 0, payload_len_be 大端序自动编码. */
ccc_status_t ccc_frame_build(uint8_t msg_type, uint8_t msg_id,
                              const uint8_t *payload, uint16_t payload_len,
                              uint8_t *out_frame, uint16_t *out_len);

/** 解析 v4.0 帧: 长度精确匹配, 非法帧头/截断/粘包均返回 CCC_ERR_PARAM.
 *  out_payload 指向 out_frame+4 (不复制), 调用者在 out_frame 存活期间有效. */
ccc_status_t ccc_frame_parse(const uint8_t *frame, uint16_t frame_len,
                              ccc_frame_header_v4_t *out_hdr,
                              const uint8_t **out_payload, uint16_t *out_payload_len);

/** CCC v4.0 高层发送: 自动封装帧头 + ble_send.
 *  payload_len > (BLE_MAX_PAYLOAD - 4) 时返回 CCC_ERR_PARAM (单帧限制). */
ccc_status_t ccc_ble_send_v4(uint16_t conn_handle,
                              uint8_t msg_type, uint8_t msg_id,
                              const uint8_t *payload, uint16_t payload_len);

/** 协议辅助: 从 message_header 提取 Message Type (屏蔽 RFU 位). */
static inline uint8_t ccc_msg_type_from_header(uint8_t header_byte) {
    return header_byte & CCC_MSG_TYPE_MASK;
}

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

/* 轻量钥匙摘要 — 避免在 MCU 栈上复制完整 ccc_digital_key_t (含证书).
 *
 * 语义: 仅在"恰好一把 ACTIVE 钥匙"时返回 CCC_OK; 0 把返回 CCC_ERR_NOT_FOUND;
 *       多把返回 CCC_ERR_STATE (歧义即拒绝, fail-closed).
 * 用途: 认证成功后绑定会话权限; 多钥匙场景须由 APDU 身份交换明确 key_id. */
typedef struct {
    uint8_t key_id[KEY_ID_LEN];
    uint8_t access_rights[4];
    uint8_t key_type;
    uint8_t state;
} key_summary_t;

ccc_status_t key_get_sole_active_summary(key_summary_t *out);

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
    uint8_t  attestation_cert[512];
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

/* Public-key / device-key algorithm identifiers (also used in attestation.key_type). */
#define SEC_KEY_TYPE_P256  0x01U
#define SEC_KEY_TYPE_SM2   0x02U

ccc_status_t  sec_init(void);
ccc_status_t  sec_deinit(void);
ccc_status_t  sec_scp03_open(scp03_channel_t *ch);
ccc_status_t  sec_scp03_close(scp03_channel_t *ch);
ccc_status_t  sec_encrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len);
ccc_status_t  sec_decrypt(const uint8_t *in, uint32_t len, uint8_t *out, uint32_t *out_len);
ccc_status_t  sec_sign(const uint8_t *data, uint32_t len, uint8_t *sig, uint32_t *sig_len);
verify_result_e sec_verify(const uint8_t *data, uint32_t len, const uint8_t *sig, uint32_t sig_len);
ccc_status_t  sec_store_key(const uint8_t *key_id, const uint8_t *key_data, uint16_t key_len);
ccc_status_t  sec_load_key(const uint8_t *key_id, uint8_t *key_data, uint16_t *key_len);
ccc_status_t  sec_delete_key(const uint8_t *key_id);
ccc_status_t  sec_attestation(ccc_attestation_t *att);
ccc_status_t  sec_attestation_with_nonce(ccc_attestation_t *att, const uint8_t nonce[16]);
verify_result_e sec_verify_attestation(const ccc_attestation_t *att);
verify_result_e sec_verify_attestation_peer(const ccc_attestation_t *att,
                                            const uint8_t expected_nonce[16]);
verify_result_e sec_verify_cert_chain(const uint8_t *cert_chain, uint16_t chain_len,
                                       const uint8_t *root_pub_key, uint16_t root_key_len);

/* ========================================================================
 *  Peer / device key provisioning (P0-04 T4)
 * ========================================================================
 * sec_verify() requires a registered peer key; there is no built-in
 * development key fallback. A successful sec_verify_cert_chain() registers
 * the leaf public key automatically.
 */
ccc_status_t sec_set_peer_public_key(uint8_t key_type, const uint8_t *pub, uint16_t len);
void         sec_clear_peer_public_key(void);

/* Device-provisioned certificate-chain trust anchor.
 *
 * The chain trust anchor MUST come from device provisioning (e.g. factory
 * key store), never from the BLE peer. sec_verify_cert_chain() anchors only
 * on this key; the root_pub_key argument it still accepts for ABI
 * compatibility is treated as untrusted input and ignored. */
ccc_status_t sec_set_peer_trust_root(uint8_t key_type, const uint8_t *pub, uint16_t len);
void         sec_clear_peer_trust_root(void);

/** Non-zero when a peer public key is available (set explicitly or registered
 *  by a successfully verified certificate chain). */
int          sec_peer_public_key_provisioned(void);

/** Trusted wall-clock time for certificate validity checks (0 = disabled). */
void         sec_set_trusted_time(uint32_t unix_secs);

/** Provision device identity: SE050 OTP / production data (16 + 16 bytes). */
ccc_status_t sec_set_device_identity(const uint8_t device_id[16], const uint8_t key_id[16]);

/** Provision the expected firmware measurement (SHA-256). */
ccc_status_t sec_set_firmware_measurement(const uint8_t hash[32]);

/** Provision the DER attestation certificate (max 256 bytes). */
ccc_status_t sec_set_attestation_cert(const uint8_t *der, uint16_t len);

/** Provision the P-256 root public key (X||Y) that signs attestation certs. */
ccc_status_t sec_set_attestation_root(const uint8_t *pub, uint16_t len);

/** Provision the device public key, used when no attestation cert is present. */
ccc_status_t sec_set_device_public_key(uint8_t key_type, const uint8_t *pub, uint16_t len);

/* ========================================================================
 *  Production provisioning lock (量产一次性锁定)
 * ========================================================================
 * Called ONCE by the production line after all provisioning data is written
 * (typically right after boot on the production jig). There is deliberately
 * no unlock API: once locked, every provisioning write below returns
 * CCC_ERR_STATE and the clear APIs become no-ops (existing state is kept):
 *   sec_set_peer_public_key / sec_clear_peer_public_key
 *   sec_set_peer_trust_root / sec_clear_peer_trust_root
 *   sec_set_device_identity / sec_set_firmware_measurement
 *   sec_set_attestation_cert / sec_set_attestation_root
 *   sec_set_device_public_key
 *
 * sec_set_trusted_time() is NOT affected: field time calibration must remain
 * available for certificate validity checks.
 */

/** Lock provisioning writes; persists the lock state to NV when bound.
 *  Returns CCC_OK when the RAM lock is set (NV unavailable degrades to RAM
 *  only), or the persistence error. The RAM lock stays set on failure. */
ccc_status_t sec_lock_provisioning(void);

/** true once sec_lock_provisioning() has been called or NV reports locked. */
bool sec_provisioning_locked(void);

/** Power-on restore: load provisioning lock + CRL from NV. No record is not
 *  an error; returns the first real error. */
ccc_status_t sec_security_init(void);

/** Provision the CRL verification root (device trust anchor, never peer data). */
ccc_status_t sec_set_revocation_root(uint8_t key_type, const uint8_t *pub, uint16_t len);

/** Install a signed CRL blob using the provisioned revocation root.
 *  Works after provisioning lock (revocation must remain updateable).
 *  Returns CCC_ERR_STATE when no root was provisioned, CCC_ERR_SECURITY on
 *  bad signature, CCC_ERR_STATE on a replayed/older epoch. */
ccc_status_t sec_install_revocation(const uint8_t *blob, uint16_t len);

#ifdef UNIT_TEST
/** Test-only: clear every provisioning static (including the lock) and CRL RAM. */
void         sec_test_reset_security(void);
#endif

/* GATT Service Registration ([P0-2]) */
ccc_status_t  ble_register_gatt_service(void);
ccc_status_t  ble_register_gatt_value_change_cb(void (*cb)(uint16_t char_uuid,
                                                            const uint8_t *data,
                                                            uint16_t len));
ccc_status_t  ble_gatt_notify(uint16_t char_uuid, const uint8_t *data, uint16_t len);

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

/* 认证会话信息 — 车辆指令权限判定的唯一权威 (由 core 维护).
 *
 * authenticated: 证书链 + attestation + 挑战签名全部通过后才为 true;
 *                任何失败、重新认证、断开连接都会清零.
 * key_bound:     已绑定唯一 ACTIVE 钥匙; 为 false 时 access_rights 全 0,
 *                调用方必须拒绝一切车控指令 (fail-closed).
 * session_id:    认证通过时由 TRNG 生成的随机会话标识, 供 OEM App
 *                (Message Type 4) 做会话绑定与防重放; 未认证/生成失败为 0. */
typedef struct {
    bool    authenticated;
    bool    key_bound;
    uint8_t key_id[KEY_ID_LEN];
    uint8_t access_rights[4];
    uint32_t session_id;
} ccc_auth_info_t;

ccc_status_t ccc_dk_init(void);
ccc_status_t ccc_dk_deinit(void);
ccc_status_t ccc_dk_run(void);    /* main loop tick */
main_state_e ccc_dk_get_state(void);
ccc_status_t ccc_dk_get_status(system_status_t *status);
ccc_status_t ccc_dk_get_auth_info(ccc_auth_info_t *info);

/* 提交对端认证数据 (由 BLE 接收回调调用), 触发 AUTH_PROCESS 闭环 */
void ccc_dk_set_auth_data(const uint8_t *cert_chain, uint16_t cert_chain_len,
                          const uint8_t *root_pub_key, uint16_t root_key_len,
                          const ccc_attestation_t *attestation,
                          const uint8_t *auth_data, uint32_t auth_data_len,
                          const uint8_t *auth_sig, uint32_t auth_sig_len);

#ifdef __cplusplus
}
#endif

#endif /* CCC_DIGITAL_KEY_H */
