/**
 * @file ccc_dk_core.c
 * @brief CCC Digital Key - Core State Machine & Main Loop
 */

#include "ccc_digital_key.h"
#include "ccc_apdu.h"

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

/* 认证会话 — 车辆指令权限的唯一权威. 只在完整认证通过后置位. */
static ccc_auth_info_t g_auth_info = {0};

static void clear_auth_session(void)
{
    (void)memset(&g_auth_info, 0, sizeof(g_auth_info));
}

static void mark_authenticated(void)
{
    key_summary_t ks;

    g_auth_info.authenticated = true;
    g_auth_info.key_bound     = false;
    (void)memset(g_auth_info.key_id, 0, KEY_ID_LEN);
    (void)memset(g_auth_info.access_rights, 0, 4);

    /* 只有"恰好一把 ACTIVE 钥匙"才能绑定权限; 否则保持 key_bound=false.
     * 多钥匙歧义场景需由 APDU 身份交换声明 key_id (待规范确认). */
    if (key_get_sole_active_summary(&ks) == CCC_OK) {
        (void)memcpy(g_auth_info.key_id, ks.key_id, KEY_ID_LEN);
        (void)memcpy(g_auth_info.access_rights, ks.access_rights, 4);
        g_auth_info.key_bound = true;
    }
}

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
    ccc_apdu_reset();          /* 会话隔离: 断开即作废 AID/挑战/认证态 */
    clear_auth_session();
    g_status.ble_connected = false;
    transition_state(STATE_STANDBY);
}

/* -------------------------------------------------------------------
 *  APDU 路由 — 传输层之上、密码学服务之下的唯一入口
 *
 *  BLE RX -> ccc_frame_parse (v4.0 帧) -> 本函数按 msg_type 分流
 *    SE + DK_APDU_RQ -> ccc_apdu_process -> SE + DK_APDU_RS 回包
 *  帧畸形/非预期消息一律丢弃, 不回包 (fail-closed).
 * ------------------------------------------------------------------- */
static void on_apdu_authenticated(const uint8_t *challenge, uint16_t challenge_len,
                                  const uint8_t *sig, uint16_t sig_len)
{
    /* 签名与挑战已由 ccc_apdu 拷贝到静态存储, 延迟到 ccc_dk_run 处理安全 */
    (void)ccc_dk_set_auth_data(NULL, 0, NULL, 0, NULL,
                               challenge, challenge_len, sig, sig_len);
}

static void ccc_ble_rx_handler(uint16_t conn_handle,
                               const uint8_t *data, uint16_t len)
{
    ccc_frame_header_v4_t hdr;
    const uint8_t *payload = NULL;
    uint16_t payload_len = 0;

    if (ccc_frame_parse(data, len, &hdr, &payload, &payload_len) != CCC_OK) {
        return;   /* 非法帧头/长度不匹配: 静默丢弃 */
    }

    uint8_t msg_type = ccc_msg_type_from_header(hdr.message_header);

    if (msg_type == CCC_MSG_TYPE_SE &&
        hdr.payload_header == CCC_MSG_ID_DK_APDU_RQ) {
        uint8_t  resp[BLE_MAX_PAYLOAD];
        uint16_t resp_len = 0;

        (void)ccc_apdu_process(payload, payload_len,
                               resp, (uint16_t)sizeof(resp), &resp_len);
        if (resp_len > 0) {
            (void)ccc_ble_send_v4(conn_handle,
                                  CCC_MSG_TYPE_SE, CCC_MSG_ID_DK_APDU_RS,
                                  resp, resp_len);
        }
    }
    /* 其他 msg_type (UWB_RANGING / DK_EVENT_NOTIFY / FRAMEWORK)
     * 不属于 APDU 路由, 由各自模块处理.
     * 注意: CCC_MSG_TYPE_VEHICLE_OEM_APP(4) 才是 OEM 自定义车控的规范入口
     * (CCC-TS-101 Table 19-21), 不应把车控塞进 SE 的私有 INS. */
}

/* -------------------------------------------------------------------
 *  Authentication processing — P0-03 AUTH_PROCESS 闭环
 *
 *  流程: 验证证书链 → 验证远程证明 → 验证认证签名 → 启动 UWB 测距
 *        → 根据测距区间解锁/闭锁
 * ------------------------------------------------------------------- */
static ccc_status_t process_authentication(void)
{
    verify_result_e vr;

    if (!g_auth.pending) return CCC_ERR_BUSY;

    /* Step 1: 验证对端数字钥匙证书链 (信任锚仅来自设备预置).
     *
     * SECURITY: 对端可自带 root_pub_key, 该参数不可信并被忽略;
     * sec_verify_cert_chain() 只使用 sec_set_peer_trust_root() 预置的锚.
     * 无证书链时, 必须已有预置的对端公钥 (直连公钥路径), 否则安全失败. */
    if (g_auth.cert_chain_len == 0U) {
        if (!sec_peer_public_key_provisioned()) {
            g_auth.pending = false;
            transition_state(STATE_LOCKED);
            return CCC_ERR_SECURITY;
        }
    } else {
        vr = sec_verify_cert_chain(g_auth.cert_chain, g_auth.cert_chain_len,
                                   g_auth.root_pub_key, g_auth.root_key_len);
        if (vr != VERIFY_OK) {
            g_auth.pending = false;
            transition_state(STATE_LOCKED);
            return CCC_ERR_SECURITY;
        }
    }

    /* Step 2: 验证对端远程证明 (对端身份 + 证明根锚 + 可选 nonce) */
    if (g_auth.attestation) {
        vr = sec_verify_attestation_peer(g_auth.attestation, NULL);
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
        clear_auth_session();
        transition_state(STATE_LOCKED);
        return CCC_ERR_SECURITY;
    }

    g_auth.pending = false;

    /* 完整认证通过: 置权威会话标志并绑定钥匙权限 */
    mark_authenticated();

    /* Step 4: 认证通过, 启动 UWB 测距 */
    ccc_status_t ret = uwb_start_ranging(1);  /* session_id = 1 */
    if (ret == CCC_OK) {
        /* 测距结果由 on_uwb_zone_changed() 回调处理解锁/闭锁 */
        transition_state(STATE_UWB_RANGING);
        return CCC_OK;
    }

    /* fail-closed: UWB 不可用时不得解锁 (无法做距离约束)。 */
    transition_state(STATE_LOCKED);
    return CCC_ERR_HARDWARE;
}

/* 供 BLE 接收回调调用: 提交对端认证数据 */
void ccc_dk_set_auth_data(const uint8_t *cert_chain, uint16_t cert_chain_len,
                          const uint8_t *root_pub_key, uint16_t root_key_len,
                          const ccc_attestation_t *attestation,
                          const uint8_t *auth_data, uint32_t auth_data_len,
                          const uint8_t *auth_sig, uint32_t auth_sig_len)
{
    /* 新的认证尝试立即作废上一会话的权限 */
    clear_auth_session();

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
    ble_register_recv_cb(ccc_ble_rx_handler);

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

    /* APDU 会话层: 认证成功事件回调 core 的认证流程 */
    ccc_apdu_reset();
    (void)ccc_apdu_register_auth_cb(on_apdu_authenticated);

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
    clear_auth_session();
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

ccc_status_t ccc_dk_get_auth_info(ccc_auth_info_t *info)
{
    if (!info) return CCC_ERR_INVALID_PARAM;
    (void)memcpy(info, &g_auth_info, sizeof(*info));
    return CCC_OK;
}
