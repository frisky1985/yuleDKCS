/**
 * @file ccc_dk_core.c
 * @brief CCC Digital Key - Core State Machine & Main Loop
 */

#include "ccc_digital_key.h"

/* -------------------------------------------------------------------
 *  Module Instances
 * ------------------------------------------------------------------- */
static system_status_t g_status;
static distance_threshold_t g_threshold = {
    .approach_cm  = 1000,
    .unlock_cm    = 500,
    .entry_cm     = 200,
    .inside_cm    = 50,
    .hysteresis_cm = 30
};

/* -------------------------------------------------------------------
 *  Authentication context — holds data received from the peer (phone)
 *  via BLE, consumed by STATE_AUTH_PROCESS.
 * ------------------------------------------------------------------- */
typedef struct {
    bool            pending;
    const uint8_t  *cert_chain;
    uint16_t        cert_chain_len;
    const uint8_t  *root_pub_key;
    uint16_t        root_key_len;
    const ccc_attestation_t *attestation;
    const uint8_t  *auth_data;
    uint32_t        auth_data_len;
    const uint8_t  *auth_sig;
    uint32_t        auth_sig_len;
} auth_ctx_t;

static auth_ctx_t g_auth = {0};

/* -------------------------------------------------------------------
 *  Internal Helpers
 * ------------------------------------------------------------------- */
static void transition_state(main_state_e next)
{
    if (g_status.state == next) return;
    g_status.state = next;
}

static distance_zone_e classify_distance(uint16_t dist_cm)
{
    if (dist_cm <= g_threshold.inside_cm)    return ZONE_INSIDE;
    if (dist_cm <= g_threshold.entry_cm)     return ZONE_ENTRY;
    if (dist_cm <= g_threshold.unlock_cm)    return ZONE_UNLOCK;
    if (dist_cm <= g_threshold.approach_cm)  return ZONE_APPROACH;
    return ZONE_LOCKED;
}

/* -------------------------------------------------------------------
 *  UWB Zone Callback
 * ------------------------------------------------------------------- */
static void on_uwb_zone_changed(uint32_t session_id, distance_zone_e zone, uint16_t distance_cm)
{
    (void)session_id;
    g_status.uwb_distance_cm = distance_cm;

    switch (zone) {
    case ZONE_UNLOCK:
    case ZONE_ENTRY:
        transition_state(STATE_UWB_RANGING);
        break;
    case ZONE_INSIDE:
        transition_state(STATE_UNLOCKED);
        break;
    default:
        transition_state(STATE_STANDBY);
        break;
    }
}

/* -------------------------------------------------------------------
 *  BLE Callbacks
 * ------------------------------------------------------------------- */
static void on_ble_connected(uint16_t conn_handle, uint8_t status)
{
    (void)conn_handle;
    if (status == 0) {
        g_status.ble_connected = true;
        transition_state(STATE_BLE_CONNECTED);
    }
}

static void on_ble_disconnected(uint16_t conn_handle, uint8_t reason)
{
    (void)conn_handle; (void)reason;
    g_status.ble_connected = false;
    transition_state(STATE_STANDBY);
}

/* -------------------------------------------------------------------
 *  Authentication processing — P0-03 AUTH_PROCESS 闭环
 *
 *  流程: 验证证书链 → 验证远程证明 → 验证认证签名 → 启动 UWB 测距
 *        → 根据测距区间解锁/闭锁
 * ------------------------------------------------------------------- */
static ccc_status_t process_authentication(void)
{
    if (!g_auth.pending) return CCC_ERR_BUSY;

    /* Step 1: 验证对端数字钥匙证书链 */
    verify_result_e vr = sec_verify_cert_chain(g_auth.cert_chain, g_auth.cert_chain_len,
                                               g_auth.root_pub_key, g_auth.root_key_len);
    if (vr != VERIFY_OK) {
        g_auth.pending = false;
        transition_state(STATE_LOCKED);
        return CCC_ERR_SECURITY;
    }

    /* Step 2: 验证设备远程证明 (固件完整性 + 签名) */
    if (g_auth.attestation) {
        vr = sec_verify_attestation(g_auth.attestation);
        if (vr != VERIFY_OK) {
            g_auth.pending = false;
            transition_state(STATE_LOCKED);
            return CCC_ERR_SECURITY;
        }
    }

    /* Step 3: 验证认证挑战签名 (对端私钥对 auth_data 的签名) */
    vr = sec_verify(g_auth.auth_data, g_auth.auth_data_len,
                    g_auth.auth_sig, g_auth.auth_sig_len);
    if (vr != VERIFY_OK) {
        g_auth.pending = false;
        transition_state(STATE_LOCKED);
        return CCC_ERR_SECURITY;
    }

    g_auth.pending = false;

    /* Step 4: 认证通过, 启动 UWB 测距 */
    ccc_status_t ret = uwb_start_ranging(1);  /* session_id = 1 */
    if (ret == CCC_OK) {
        /* 测距结果由 on_uwb_zone_changed() 回调处理解锁/闭锁 */
        transition_state(STATE_UWB_RANGING);
    } else {
        /* UWB 不可用时回退: 认证通过即解锁 (开发/降级路径) */
        transition_state(STATE_UNLOCKED);
    }

    return CCC_OK;
}

/* 供 BLE 接收回调调用: 提交对端认证数据 */
void ccc_dk_set_auth_data(const uint8_t *cert_chain, uint16_t cert_chain_len,
                          const uint8_t *root_pub_key, uint16_t root_key_len,
                          const ccc_attestation_t *attestation,
                          const uint8_t *auth_data, uint32_t auth_data_len,
                          const uint8_t *auth_sig, uint32_t auth_sig_len)
{
    g_auth.pending        = true;
    g_auth.cert_chain     = cert_chain;
    g_auth.cert_chain_len = cert_chain_len;
    g_auth.root_pub_key   = root_pub_key;
    g_auth.root_key_len   = root_key_len;
    g_auth.attestation    = attestation;
    g_auth.auth_data      = auth_data;
    g_auth.auth_data_len  = auth_data_len;
    g_auth.auth_sig       = auth_sig;
    g_auth.auth_sig_len   = auth_sig_len;

    /* 若处于待机或已连接状态, 触发认证流程 */
    if (g_status.state == STATE_STANDBY ||
        g_status.state == STATE_BLE_CONNECTED) {
        transition_state(STATE_AUTH_PROCESS);
    }
}

/* -------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------- */
ccc_status_t ccc_dk_init(void)
{
    (void)memset(&g_status, 0, sizeof(g_status));
    g_status.state = STATE_INIT;

    /* Init NFC */
    ccc_status_t ret = nfc_st25r501_init();
    if (ret != CCC_OK) return ret;

    /* Init BLE */
    ret = ble_kw47a_init();
    if (ret != CCC_OK) return ret;

    /* Register BLE callbacks */
    ble_register_conn_cb(on_ble_connected);
    ble_register_disconn_cb(on_ble_disconnected);

    /* Init UWB */
    ret = uwb_ncj29d6_init();
    if (ret != CCC_OK) return ret;

    /* Set distance thresholds */
    uwb_set_threshold(&g_threshold);
    uwb_register_zone_cb(on_uwb_zone_changed);

    /* Init Security */
    ret = sec_init();
    if (ret != CCC_OK) return ret;

    /* Init Key Management */
    ret = key_mgmt_init();
    if (ret != CCC_OK) return ret;

    transition_state(STATE_STANDBY);
    return CCC_OK;
}

ccc_status_t ccc_dk_deinit(void)
{
    key_mgmt_deinit();
    sec_deinit();
    uwb_ncj29d6_deinit();
    ble_kw47a_deinit();
    nfc_st25r501_deinit();
    transition_state(STATE_INIT);
    return CCC_OK;
}

ccc_status_t ccc_dk_run(void)
{
    switch (g_status.state) {
    case STATE_STANDBY:
        /* Check NFC field */
        if (nfc_field_detect()) {
            g_status.nfc_field = true;
            transition_state(STATE_NFC_DETECT);
        }
        break;

    case STATE_NFC_DETECT:
        nfc_start_listen();
        transition_state(STATE_NFC_READ);
        break;

    case STATE_NFC_READ: {
        ccc_nfc_oob_data_t oob_out = {0};
        ccc_nfc_oob_data_t oob_in  = {0};
        if (nfc_oob_exchange(&oob_out, &oob_in) == CCC_OK) {
            /* Got OOB data, start BLE OOB pairing */
            ble_oob_pair(&oob_in);
            transition_state(STATE_BLE_CONNECTED);
        }
        g_status.nfc_field = false;
        break;
    }

    case STATE_BLE_CONNECTED:
        /* Wait for auth from phone */
        transition_state(STATE_AUTH_PROCESS);
        break;

    case STATE_AUTH_PROCESS: {
        /* 认证闭环: 验钥 → 证明 → UWB → 解锁/闭锁 */
        (void)process_authentication();
        break;
    }

    case STATE_UNLOCKED:
        /* Monitor UWB for zone changes */
        break;

    case STATE_LOCKED:
        transition_state(STATE_STANDBY);
        break;

    case STATE_UWB_RANGING:
        /* UWB active ranging, zone changes handled by callback */
        break;

    default:
        break;
    }

    return CCC_OK;
}

main_state_e ccc_dk_get_state(void)
{
    return g_status.state;
}

ccc_status_t ccc_dk_get_status(system_status_t *status)
{
    if (!status) return CCC_ERR_INVALID_PARAM;
    (void)memcpy(status, &g_status, sizeof(*status));
    return CCC_OK;
}
