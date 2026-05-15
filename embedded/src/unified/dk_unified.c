/**
 * @file dk_unified.c
 * @brief Digital Key Unified Protocol Implementation
 * @version 1.0
 * @date 2026-05-08
 *
 * 统一接口实现 - 路由到 CCC / ICCOA / ICCE 具体协议
 */

#include "dk_unified.h"
#include "../ccc_protocol/include/ccc_digital_key.h"
#include "../iccoa_protocol/include/iccoa_digital_key.h"
#include "../icce_protocol/include/icce_digital_key.h"

/* ========================================================================
 *  内部状态
 * ======================================================================== */
static struct {
    bool                    initialized;
    dk_device_type_t        device;
    dk_device_status_t      status;
    
    /* 回调函数 */
    dk_conn_cb_t            conn_cb;
    void                   *conn_cb_data;
    dk_auth_cb_t            auth_cb;
    void                   *auth_cb_data;
    dk_location_cb_t        location_cb;
    void                   *location_cb_data;
    dk_zone_cb_t            zone_cb;
    void                   *zone_cb_data;
    dk_vehicle_cb_t         vehicle_cb;
    void                   *vehicle_cb_data;
    
    /* 协议特定状态 */
    uint32_t                uwb_session_id;
    uint16_t                ble_conn_handle;
} g_dk = {0};

/* ========================================================================
 *  协议路由辅助函数
 * ======================================================================== */

/**
 * @brief 检查协议是否支持某能力
 */
static bool protocol_supports_capability(dk_protocol_e proto, uint32_t cap_flag)
{
    switch (proto) {
        case DK_PROTOCOL_CCC:
            /* CCC 支持 NFC + BLE + UWB + SE */
            if (cap_flag & (DK_CAP_NFC | DK_CAP_BLE | DK_CAP_UWB | DK_CAP_SE | DK_CAP_LPCD))
                return true;
            break;
            
        case DK_PROTOCOL_ICCOA:
            /* ICCOA 主要基于 BLE，可选 UWB */
            if (cap_flag & (DK_CAP_BLE | DK_CAP_UWB | DK_CAP_KEY_SHARE))
                return true;
            break;
            
        case DK_PROTOCOL_ICCE:
            /* ICCE 支持 BLE + UWB + 边缘计算 */
            if (cap_flag & (DK_CAP_BLE | DK_CAP_UWB | DK_CAP_EDGE_COMPUTE | DK_CAP_UWB_ANGULAR))
                return true;
            break;
    }
    return false;
}

/**
 * @brief 映射 CCC 状态到统一状态
 */
static dk_zone_e map_ccc_zone(distance_zone_e ccc_zone)
{
    switch (ccc_zone) {
        case ZONE_LOCKED:   return DK_ZONE_LOCKED;
        case ZONE_APPROACH: return DK_ZONE_APPROACH;
        case ZONE_UNLOCK:   return DK_ZONE_UNLOCK;
        case ZONE_ENTRY:    return DK_ZONE_ENTRY;
        case ZONE_INSIDE:   return DK_ZONE_INSIDE;
        default:            return DK_ZONE_UNKNOWN;
    }
}

/**
 * @brief 映射 ICCE 区域到统一状态
 */
static dk_zone_e map_icce_zone(icce_zone_e icce_zone)
{
    switch (icce_zone) {
        case ICCE_ZONE_FAR:      return DK_ZONE_LOCKED;
        case ICCE_ZONE_MID:      return DK_ZONE_APPROACH;
        case ICCE_ZONE_NEAR:     return DK_ZONE_UNLOCK;
        case ICCE_ZONE_VICINITY: return DK_ZONE_ENTRY;
        case ICCE_ZONE_INTERIOR: return DK_ZONE_INSIDE;
        default:                 return DK_ZONE_UNKNOWN;
    }
}

/* ========================================================================
 *  初始化与配置
 * ======================================================================== */

dk_status_t dk_init(const dk_device_type_t *device)
{
    if (g_dk.initialized) {
        return DK_ERR_ALREADY_EXISTS;
    }
    
    if (!device) {
        return DK_ERR_INVALID_PARAM;
    }
    
    /* 保存设备配置 */
    memcpy(&g_dk.device, device, sizeof(dk_device_type_t));
    
    /* 根据协议类型初始化对应协议栈 */
    dk_status_t ret = DK_OK;
    
    switch (device->protocol) {
        case DK_PROTOCOL_CCC:
            if (ccc_dk_init() != CCC_OK) {
                ret = DK_ERR_PROTOCOL;
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            if (iccoa_dk_init() != ICCOA_OK) {
                ret = DK_ERR_PROTOCOL;
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            if (icce_dk_init() != ICCE_OK) {
                ret = DK_ERR_PROTOCOL;
            }
            break;
            
        default:
            ret = DK_ERR_UNSUPPORTED;
            break;
    }
    
    if (ret == DK_OK) {
        g_dk.initialized = true;
        memset(&g_dk.status, 0, sizeof(g_dk.status));
        memcpy(&g_dk.status.device, device, sizeof(dk_device_type_t));
    }
    
    return ret;
}

dk_status_t dk_deinit(void)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            ccc_dk_deinit();
            break;
        case DK_PROTOCOL_ICCOA:
            iccoa_dk_deinit();
            break;
        case DK_PROTOCOL_ICCE:
            icce_dk_deinit();
            break;
    }
    
    g_dk.initialized = false;
    return DK_OK;
}

dk_status_t dk_get_status(dk_device_status_t *status)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    if (!status) {
        return DK_ERR_INVALID_PARAM;
    }
    
    memcpy(status, &g_dk.status, sizeof(dk_device_status_t));
    return DK_OK;
}

dk_status_t dk_run(void)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            ccc_dk_run();
            break;
        case DK_PROTOCOL_ICCOA:
            iccoa_dk_run();
            break;
        case DK_PROTOCOL_ICCE:
            icce_dk_run();
            break;
    }
    
    return DK_OK;
}

/* ========================================================================
 *  连接管理
 * ======================================================================== */

dk_status_t dk_nfc_start_listen(void)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    /* 检查能力 */
    if (!(g_dk.device.capabilities.capabilities & DK_CAP_NFC)) {
        return DK_ERR_UNSUPPORTED;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            if (nfc_start_listen() == CCC_OK) {
                g_dk.status.conn.nfc_state = DK_CONN_NFC_FIELD;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
        case DK_PROTOCOL_ICCE:
            /* ICCOA/ICCE 不使用 NFC */
            return DK_ERR_UNSUPPORTED;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_nfc_stop_listen(void)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            if (nfc_stop_listen() == CCC_OK) {
                g_dk.status.conn.nfc_state = DK_CONN_DISCONNECTED;
                return DK_OK;
            }
            break;
            
        default:
            return DK_ERR_UNSUPPORTED;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_ble_start_adv(dk_protocol_e protocol)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    if (!(g_dk.device.capabilities.capabilities & DK_CAP_BLE)) {
        return DK_ERR_UNSUPPORTED;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            {
                ble_adv_param_t param = {
                    .interval_min = 160,  /* 100ms */
                    .interval_max = 320,  /* 200ms */
                    .len = 0
                };
                if (ble_start_adv(&param) == CCC_OK) {
                    g_dk.status.conn.ble_state = DK_CONN_BLE_ADV;
                    return DK_OK;
                }
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            if (iccoa_ble_start_adv() == ICCOA_OK) {
                g_dk.status.conn.ble_state = DK_CONN_BLE_ADV;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            if (icce_ble_start_adv() == ICCE_OK) {
                g_dk.status.conn.ble_state = DK_CONN_BLE_ADV;
                return DK_OK;
            }
            break;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_ble_stop_adv(void)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            if (ble_stop_adv() == CCC_OK) {
                g_dk.status.conn.ble_state = DK_CONN_DISCONNECTED;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            if (iccoa_ble_stop_adv() == ICCOA_OK) {
                g_dk.status.conn.ble_state = DK_CONN_DISCONNECTED;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            if (icce_ble_stop_adv() == ICCE_OK) {
                g_dk.status.conn.ble_state = DK_CONN_DISCONNECTED;
                return DK_OK;
            }
            break;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_ble_disconnect(void)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            if (ble_disconnect(g_dk.ble_conn_handle) == CCC_OK) {
                g_dk.status.conn.ble_state = DK_CONN_DISCONNECTED;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
        case DK_PROTOCOL_ICCE:
            /* ICCOA/ICCE BLE 断开由协议内部处理 */
            g_dk.status.conn.ble_state = DK_CONN_DISCONNECTED;
            return DK_OK;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_uwb_start_ranging(uint32_t *session_id)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    if (!(g_dk.device.capabilities.capabilities & DK_CAP_UWB)) {
        return DK_ERR_UNSUPPORTED;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            {
                uwb_session_config_t cfg = {
                    .channel = 9,
                    .preamble_code = 12,
                    .prf_len = 128,
                    /* 其他参数使用默认值 */
                };
                uint32_t sid = uwb_create_session(&cfg);
                if (sid != UWB_INVALID_SESSION) {
                    if (uwb_start_ranging(sid) == CCC_OK) {
                        g_dk.uwb_session_id = sid;
                        g_dk.status.conn.uwb_state = DK_CONN_UWB_RANGING;
                        g_dk.status.conn.uwb_session_id = sid;
                        if (session_id) *session_id = sid;
                        return DK_OK;
                    }
                    uwb_destroy_session(sid);
                }
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            /* ICCOA UWB 通过 BLE 配置 */
            return DK_ERR_UNSUPPORTED;
            
        case DK_PROTOCOL_ICCE:
            {
                uint16_t sid = (session_id && *session_id) ? *session_id : 1;
                if (icce_uwb_start_session(sid, ICCE_UWB_ROLE_RESPONDER, 9) == ICCE_OK) {
                    g_dk.uwb_session_id = sid;
                    g_dk.status.conn.uwb_state = DK_CONN_UWB_RANGING;
                    if (session_id) *session_id = sid;
                    return DK_OK;
                }
            }
            break;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_uwb_stop_ranging(uint32_t session_id)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            if (uwb_stop_ranging(session_id) == CCC_OK &&
                uwb_destroy_session(session_id) == CCC_OK) {
                g_dk.status.conn.uwb_state = DK_CONN_UWB_IDLE;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            if (icce_uwb_stop_session((uint16_t)session_id) == ICCE_OK) {
                g_dk.status.conn.uwb_state = DK_CONN_UWB_IDLE;
                return DK_OK;
            }
            break;
            
        default:
            return DK_ERR_UNSUPPORTED;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_register_conn_cb(dk_conn_cb_t cb, void *user_data)
{
    g_dk.conn_cb = cb;
    g_dk.conn_cb_data = user_data;
    return DK_OK;
}

/* ========================================================================
 *  认证管理
 * ======================================================================== */

dk_status_t dk_auth_bind(dk_protocol_e protocol)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    g_dk.status.auth.state = DK_AUTH_BINDING;
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            /* CCC 配对通过 NFC OOB */
            {
                ccc_nfc_oob_data_t oob = {0};
                if (nfc_oob_exchange(&oob, &oob) == CCC_OK &&
                    ble_oob_pair(&oob) == CCC_OK) {
                    g_dk.status.auth.state = DK_AUTH_BOUND;
                    return DK_OK;
                }
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            if (iccoa_auth_request(ICCOA_AUTH_BIND, NULL, 0) == ICCOA_OK) {
                g_dk.status.auth.state = DK_AUTH_BOUND;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            /* ICCE 绑定通过 BLE */
            return DK_OK;
    }
    
    g_dk.status.auth.state = DK_AUTH_FAILED;
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_auth_unbind(const uint8_t *key_id)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            if (key_delete(key_id) == CCC_OK) {
                g_dk.status.auth.state = DK_AUTH_NONE;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            /* ICCOA 解绑 */
            g_dk.status.auth.state = DK_AUTH_NONE;
            return DK_OK;
            
        case DK_PROTOCOL_ICCE:
            g_dk.status.auth.state = DK_AUTH_NONE;
            return DK_OK;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_auth_verify(void)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    g_dk.status.auth.state = DK_AUTH_CHALLENGING;
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            /* CCC 认证通过 SE */
            {
                ccc_attestation_t att = {0};
                if (sec_attestation(&att) == CCC_OK &&
                    sec_verify_attestation(&att) == VERIFY_OK) {
                    g_dk.status.auth.state = DK_AUTH_VERIFIED;
                    return DK_OK;
                }
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            if (iccoa_auth_verify(NULL, 0) == ICCOA_OK) {
                g_dk.status.auth.state = DK_AUTH_VERIFIED;
                return DK_OK;
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            if (icce_security_verify_session((uint16_t)g_dk.uwb_session_id) == ICCE_OK) {
                g_dk.status.auth.state = DK_AUTH_VERIFIED;
                return DK_OK;
            }
            break;
    }
    
    g_dk.status.auth.state = DK_AUTH_FAILED;
    return DK_ERR_PROTOCOL;
}

bool dk_auth_check_permission(uint32_t permission)
{
    if (!g_dk.initialized || g_dk.status.auth.state != DK_AUTH_VERIFIED) {
        return false;
    }
    
    uint32_t rights = 0;
    rights |= (g_dk.status.auth.access_rights[0]);
    rights |= (g_dk.status.auth.access_rights[1] << 8);
    rights |= (g_dk.status.auth.access_rights[2] << 16);
    rights |= (g_dk.status.auth.access_rights[3] << 24);
    
    return (rights & permission) != 0;
}

dk_status_t dk_register_auth_cb(dk_auth_cb_t cb, void *user_data)
{
    g_dk.auth_cb = cb;
    g_dk.auth_cb_data = user_data;
    return DK_OK;
}

/* ========================================================================
 *  钥匙管理
 * ======================================================================== */

dk_status_t dk_key_create(dk_key_t *key)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            {
                ccc_digital_key_t ccc_key = {0};
                memcpy(ccc_key.key_id, key->key_id, 16);
                memcpy(ccc_key.vehicle_id, key->vehicle_id, 16);
                memcpy(ccc_key.owner_id, key->owner_id, 16);
                ccc_key.key_type = (uint8_t)key->key_type;
                memcpy(ccc_key.access_rights, key->access_rights, 4);
                ccc_key.valid_from = key->valid_from;
                ccc_key.valid_until = key->valid_until;
                
                if (key_create(&ccc_key) == CCC_OK) {
                    key->se_key_ref = ccc_key.se_key_id;
                    return DK_OK;
                }
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
        case DK_PROTOCOL_ICCE:
            /* ICCOA/ICCE 钥匙管理通过云端 */
            return DK_OK;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_key_delete(const uint8_t *key_id)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            return (key_delete(key_id) == CCC_OK) ? DK_OK : DK_ERR_PROTOCOL;
            
        default:
            return DK_OK;
    }
}

dk_status_t dk_key_get(const uint8_t *key_id, dk_key_t *key)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            {
                ccc_digital_key_t ccc_key = {0};
                if (key_get(key_id, &ccc_key) == CCC_OK) {
                    memcpy(key->key_id, ccc_key.key_id, 16);
                    memcpy(key->vehicle_id, ccc_key.vehicle_id, 16);
                    memcpy(key->owner_id, ccc_key.owner_id, 16);
                    key->key_type = (dk_key_type_e)ccc_key.key_type;
                    key->state = (dk_key_state_e)ccc_key.state;
                    memcpy(key->access_rights, ccc_key.access_rights, 4);
                    key->valid_from = ccc_key.valid_from;
                    key->valid_until = ccc_key.valid_until;
                    return DK_OK;
                }
            }
            break;
    }
    
    return DK_ERR_NOT_FOUND;
}

dk_status_t dk_key_list(dk_key_t *keys, uint8_t *count)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            {
                ccc_digital_key_t ccc_keys[MAX_KEYS];
                if (key_list(ccc_keys, count) == CCC_OK) {
                    for (uint8_t i = 0; i < *count; i++) {
                        memcpy(keys[i].key_id, ccc_keys[i].key_id, 16);
                        keys[i].key_type = (dk_key_type_e)ccc_keys[i].key_type;
                        keys[i].state = (dk_key_state_e)ccc_keys[i].state;
                    }
                    return DK_OK;
                }
            }
            break;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_key_share(const uint8_t *key_id, dk_key_type_e type, uint32_t duration_s)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    if (!(g_dk.device.capabilities.capabilities & DK_CAP_KEY_SHARE)) {
        return DK_ERR_UNSUPPORTED;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            return (key_share(key_id, (key_type_e)type, duration_s) == CCC_OK) ? 
                   DK_OK : DK_ERR_PROTOCOL;
        default:
            return DK_OK;
    }
}

dk_status_t dk_key_revoke(const uint8_t *key_id)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            return (key_revoke(key_id) == CCC_OK) ? DK_OK : DK_ERR_PROTOCOL;
        default:
            return DK_OK;
    }
}

dk_status_t dk_key_suspend(const uint8_t *key_id)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            return (key_suspend(key_id) == CCC_OK) ? DK_OK : DK_ERR_PROTOCOL;
        default:
            return DK_OK;
    }
}

dk_status_t dk_key_resume(const uint8_t *key_id)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            return (key_resume(key_id) == CCC_OK) ? DK_OK : DK_ERR_PROTOCOL;
        default:
            return DK_OK;
    }
}

/* ========================================================================
 *  定位与区域
 * ======================================================================== */

dk_status_t dk_location_get(dk_location_t *loc)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            {
                uint16_t dist_cm = 0;
                if (uwb_get_distance(g_dk.uwb_session_id, &dist_cm) == CCC_OK) {
                    loc->distance_mm = dist_cm * 10;
                    loc->zone = map_ccc_zone(uwb_get_zone(g_dk.uwb_session_id));
                    loc->method = 0; /* UWB */
                    loc->quality = 80;
                    return DK_OK;
                }
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            {
                icce_uwb_session_t session = {0};
                if (icce_uwb_get_ranging((uint16_t)g_dk.uwb_session_id, &session) == ICCE_OK) {
                    loc->distance_mm = session.distance_mm;
                    loc->angle_azimuth = session.angle_azimuth;
                    loc->angle_elevation = session.angle_elevation;
                    loc->zone = map_icce_zone(icce_zone_classify(session.distance_mm));
                    loc->quality = session.quality;
                    loc->method = 0; /* UWB */
                    return DK_OK;
                }
            }
            break;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_zone_set_threshold(uint16_t approach_cm,
                                  uint16_t unlock_cm,
                                  uint16_t entry_cm,
                                  uint16_t inside_cm)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            {
                distance_threshold_t th = {
                    .approach_cm = approach_cm,
                    .unlock_cm = unlock_cm,
                    .entry_cm = entry_cm,
                    .inside_cm = inside_cm,
                    .hysteresis_cm = 30
                };
                return (uwb_set_threshold(&th) == CCC_OK) ? DK_OK : DK_ERR_PROTOCOL;
            }
            
        default:
            return DK_OK;
    }
}

dk_status_t dk_register_location_cb(dk_location_cb_t cb, void *user_data)
{
    g_dk.location_cb = cb;
    g_dk.location_cb_data = user_data;
    return DK_OK;
}

dk_status_t dk_register_zone_cb(dk_zone_cb_t cb, void *user_data)
{
    g_dk.zone_cb = cb;
    g_dk.zone_cb_data = user_data;
    return DK_OK;
}

/* ========================================================================
 *  车辆控制
 * ======================================================================== */

dk_status_t dk_vehicle_ctrl(dk_ctrl_cmd_e cmd, uint8_t param)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    /* 检查权限 */
    uint32_t perm = 0;
    switch (cmd) {
        case DK_CTRL_LOCK:
        case DK_CTRL_UNLOCK:
        case DK_CTRL_UNLOCK_ALL:
            perm = DK_ACCESS_LOCK | DK_ACCESS_UNLOCK;
            break;
        case DK_CTRL_ENGINE_START:
        case DK_CTRL_ENGINE_STOP:
            perm = DK_ACCESS_ENGINE_START;
            break;
        case DK_CTRL_TRUNK_OPEN:
        case DK_CTRL_TRUNK_CLOSE:
            perm = DK_ACCESS_TRUNK;
            break;
        case DK_CTRL_WINDOW_UP:
        case DK_CTRL_WINDOW_DOWN:
        case DK_CTRL_SUNROOF_OPEN:
        case DK_CTRL_SUNROOF_CLOSE:
            perm = DK_ACCESS_WINDOWS | DK_ACCESS_SUNROOF;
            break;
        case DK_CTRL_CLIMATE_ON:
        case DK_CTRL_CLIMATE_OFF:
            perm = DK_ACCESS_CLIMATE;
            break;
        case DK_CTRL_HORN:
        case DK_CTRL_LIGHTS:
        case DK_CTRL_FIND:
            perm = DK_ACCESS_FIND;
            break;
    }
    
    if (!dk_auth_check_permission(perm)) {
        return DK_ERR_DENIED;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_CCC:
            /* CCC 车控通过 BLE */
            return DK_OK;
            
        case DK_PROTOCOL_ICCOA:
            {
                iccoa_ctrl_cmd_e iccoa_cmd = (iccoa_ctrl_cmd_e)cmd;
                return (iccoa_ctrl_execute(iccoa_cmd, param) == ICCOA_OK) ? 
                       DK_OK : DK_ERR_PROTOCOL;
            }
            
        case DK_PROTOCOL_ICCE:
            {
                icce_action_e icce_cmd = (icce_action_e)cmd;
                return (icce_vehicle_ctrl(icce_cmd, param) == ICCE_OK) ? 
                       DK_OK : DK_ERR_PROTOCOL;
            }
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_vehicle_get_status(dk_vehicle_status_t *status)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    switch (g_dk.device.protocol) {
        case DK_PROTOCOL_ICCOA:
            {
                iccoa_vehicle_status_t iccoa_status = {0};
                if (iccoa_service_get_status(&iccoa_status) == ICCOA_OK) {
                    status->lock_status = iccoa_status.lock_status;
                    status->engine_status = iccoa_status.engine_status;
                    status->door_status = iccoa_status.door_status;
                    status->window_status = iccoa_status.window_status;
                    status->battery_pct = iccoa_status.battery_level;
                    status->interior_temp = iccoa_status.interior_temp;
                    status->alarm_status = iccoa_status.alarm_status;
                    return DK_OK;
                }
            }
            break;
            
        case DK_PROTOCOL_ICCE:
            {
                icce_vehicle_status_t icce_status = {0};
                if (icce_vehicle_get_status(&icce_status) == ICCE_OK) {
                    status->lock_status = icce_status.lock_status;
                    status->engine_status = icce_status.engine_status;
                    status->door_status = icce_status.door_status;
                    status->window_status = icce_status.window_status;
                    status->battery_pct = icce_status.battery_pct;
                    status->interior_temp = icce_status.interior_temp;
                    status->odometer_km = icce_status.odometer_km;
                    status->fuel_level = icce_status.fuel_level;
                    status->alarm_status = icce_status.alarm_status;
                    return DK_OK;
                }
            }
            break;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_register_vehicle_cb(dk_vehicle_cb_t cb, void *user_data)
{
    g_dk.vehicle_cb = cb;
    g_dk.vehicle_cb_data = user_data;
    return DK_OK;
}

/* ========================================================================
 *  协议扩展
 * ======================================================================== */

dk_status_t dk_protocol_send_raw(dk_protocol_e protocol, 
                                  const uint8_t *data, 
                                  uint16_t len)
{
    if (!g_dk.initialized) {
        return DK_ERR_NOT_INIT;
    }
    
    /* 只能发送当前协议或厂商扩展数据 */
    if (protocol != g_dk.device.protocol && protocol != 0xFF) {
        return DK_ERR_DENIED;
    }
    
    switch (protocol) {
        case DK_PROTOCOL_CCC:
            if (ble_is_connected(g_dk.ble_conn_handle)) {
                return (ble_send(g_dk.ble_conn_handle, data, len) == CCC_OK) ? 
                       DK_OK : DK_ERR_PROTOCOL;
            }
            break;
            
        case DK_PROTOCOL_ICCOA:
            return (iccoa_ble_send(data, len) == ICCOA_OK) ? DK_OK : DK_ERR_PROTOCOL;
            
        case DK_PROTOCOL_ICCE:
            return (icce_ble_send(data, len) == ICCE_OK) ? DK_OK : DK_ERR_PROTOCOL;
    }
    
    return DK_ERR_PROTOCOL;
}

dk_status_t dk_protocol_get_info(dk_protocol_e protocol,
                                  uint16_t *version,
                                  const char **name)
{
    if (!version || !name) {
        return DK_ERR_INVALID_PARAM;
    }
    
    switch (protocol) {
        case DK_PROTOCOL_CCC:
            *version = DK_VERSION_CCC_30;
            *name = "CCC Digital Key 3.0";
            break;
            
        case DK_PROTOCOL_ICCOA:
            *version = DK_VERSION_ICCOA_40;
            *name = "ICCOA DK 4.0";
            break;
            
        case DK_PROTOCOL_ICCE:
            *version = DK_VERSION_ICCE_10;
            *name = "ICCE Edge Computing";
            break;
            
        default:
            return DK_ERR_INVALID_PARAM;
    }
    
    return DK_OK;
}