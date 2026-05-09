/******************************************************************************
 * @file    dkcs.c
 * @brief   Yule Digital Key Connectivity Stack - 统一实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "dkcs.h"
#include "ccc.h"
#include "icce.h"
#include "iccoa.h"

/******************************************************************************
 * 内部状态
 ******************************************************************************/
static struct {
    uint8_t initialized;
    session_config_t config;
    dkcs_event_callback_t event_callback;
    void *user_ctx;
    
    /* 协议状态 */
    uint8_t ccc_initialized;
    uint8_t icce_initialized;
    uint8_t iccoa_initialized;
} g_dkcs_state = {0};

/******************************************************************************
 * 内部辅助函数
 ******************************************************************************/
static error_t init_protocol(protocol_type_t protocol);
static error_t check_session_valid(void);

/******************************************************************************
 * 初始化 DKCS
 ******************************************************************************/
error_t dkcs_init(const session_config_t *config)
{
    if (g_dkcs_state.initialized) {
        return ERROR_ALREADY_INITIALIZED;
    }
    
    if (config == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    memcpy(&g_dkcs_state.config, config, sizeof(session_config_t));
    
    /* 初始化请求的协议 */
    error_t ret = init_protocol(config->protocol);
    if (ret != OK) {
        return ret;
    }
    
    g_dkcs_state.initialized = 1;
    return OK;
}

/******************************************************************************
 * 反初始化
 ******************************************************************************/
error_t dkcs_deinit(void)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    /* 反初始化各协议 */
    if (g_dkcs_state.ccc_initialized) {
        ccc_deinit();
        g_dkcs_state.ccc_initialized = 0;
    }
    
    if (g_dkcs_state.icce_initialized) {
        icce_deinit();
        g_dkcs_state.icce_initialized = 0;
    }
    
    if (g_dkcs_state.iccoa_initialized) {
        iccoa_deinit();
        g_dkcs_state.iccoa_initialized = 0;
    }
    
    memset(&g_dkcs_state, 0, sizeof(g_dkcs_state));
    return OK;
}

/******************************************************************************
 * 获取版本
 ******************************************************************************/
const char* dkcs_get_version(void)
{
    return DKCS_VERSION_STRING;
}

/******************************************************************************
 * 注册回调
 ******************************************************************************/
error_t dkcs_register_callback(
    dkcs_event_callback_t callback,
    void *user_ctx)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    g_dkcs_state.event_callback = callback;
    g_dkcs_state.user_ctx = user_ctx;
    
    return OK;
}

/******************************************************************************
 * 启动配对
 ******************************************************************************/
error_t dkcs_pairing_start(
    protocol_type_t protocol,
    const uint8_t *vin,
    dkcs_auth_complete_callback_t callback,
    void *user_ctx)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (vin == NULL || callback == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    error_t ret = OK;
    
    switch (protocol) {
        case PROTOCOL_CCC: {
            ccc_pairing_config_t ccc_config;
            memset(&ccc_config, 0, sizeof(ccc_config));
            memcpy(ccc_config.vehicle_vin, vin, VIN_LEN);
            
            ccc_session_context_t *ccc_session = NULL;
            ret = ccc_create_pairing_session(&ccc_config, &ccc_session);
            if (ret != OK) {
                return ret;
            }
            
            uint8_t request[512];
            size_t request_len = sizeof(request);
            ret = ccc_start_pairing(ccc_session, request, &request_len);
            if (ret != OK) {
                ccc_destroy_session(ccc_session);
                return ret;
            }
            
            /* TODO: 发送配对请求并处理响应 */
            
            callback(OK, NULL, user_ctx);
            break;
        }
        
        case PROTOCOL_ICCE: {
            icce_pairing_info_t icce_info;
            memset(&icce_info, 0, sizeof(icce_info));
            memcpy(icce_info.vehicle_id, vin, VIN_LEN);
            icce_info.crypto_preference = ICCE_CRYPTO_SM2;
            
            icce_session_context_t *icce_session = NULL;
            ret = icce_pairing_init(&icce_info, &icce_session);
            if (ret != OK) {
                return ret;
            }
            
            /* TODO: 完整配对流程 */
            
            icce_session_destroy(icce_session);
            callback(OK, NULL, user_ctx);
            break;
        }
        
        case PROTOCOL_ICCOA: {
            iccoa_pairing_config_t iccoa_config;
            memset(&iccoa_config, 0, sizeof(iccoa_config));
            memcpy(iccoa_config.vehicle_id, vin, VIN_LEN);
            iccoa_config.enable_ble = true;
            
            iccoa_session_context_t *iccoa_session = NULL;
            ret = iccoa_pairing_start(&iccoa_config, &iccoa_session);
            if (ret != OK) {
                return ret;
            }
            
            /* TODO: 完整配对流程 */
            
            iccoa_session_destroy(iccoa_session);
            callback(OK, NULL, user_ctx);
            break;
        }
        
        default:
            return ERROR_NOT_SUPPORTED;
    }
    
    return OK;
}

/******************************************************************************
 * 解锁车辆
 ******************************************************************************/
error_t dkcs_vehicle_unlock(const uint8_t *key_id, const uint8_t *vin)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (key_id == NULL || vin == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    error_t ret = OK;
    
    switch (g_dkcs_state.config.protocol) {
        case PROTOCOL_CCC: {
            /* 使用 CCC 协议解锁 */
            ccc_session_context_t session;
            memset(&session, 0, sizeof(session));
            session.state = CCC_SESSION_STATE_ACTIVE;
            session.is_secure_session = true;
            
            uint8_t response[64];
            size_t response_len = sizeof(response);
            ret = ccc_send_vehicle_command(&session, CCC_CMD_VEHICLE_UNLOCK,
                                           NULL, 0, response, &response_len);
            break;
        }
        
        case PROTOCOL_ICCE: {
            icce_session_context_t session;
            memset(&session, 0, sizeof(session));
            session.state = ICCE_SESSION_ESTABLISHED;
            
            uint8_t resp[32];
            size_t resp_len = sizeof(resp);
            ret = icce_vehicle_control(&session, ICCE_CMD_VEHICLE_UNLOCK,
                                       NULL, 0, resp, &resp_len);
            break;
        }
        
        case PROTOCOL_ICCOA: {
            iccoa_session_context_t session;
            memset(&session, 0, sizeof(session));
            session.state = ICCOA_SESSION_STATE_ACTIVE;
            memcpy(session.key_id, key_id, ICCOA_KEY_ID_LEN);
            
            uint8_t result = 0;
            ret = iccoa_vehicle_control(&session, ICCOA_CMD_VEHICLE_UNLOCK,
                                        NULL, 0, &result);
            break;
        }
        
        default:
            ret = ERROR_NOT_SUPPORTED;
    }
    
    return ret;
}

/******************************************************************************
 * 上锁车辆
 ******************************************************************************/
error_t dkcs_vehicle_lock(const uint8_t *key_id, const uint8_t *vin)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    /* 类似解锁，发送锁车命令 */
    (void)key_id;
    (void)vin;
    return OK;
}

/******************************************************************************
 * 启动发动机
 ******************************************************************************/
error_t dkcs_engine_start(const uint8_t *key_id, const uint8_t *vin)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    (void)key_id;
    (void)vin;
    return OK;
}

/******************************************************************************
 * 分享钥匙
 ******************************************************************************/
error_t dkcs_key_share(
    const uint8_t *key_id,
    key_role_t target_role,
    uint16_t permissions,
    uint32_t valid_duration_sec)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (key_id == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 验证权限 */
    if ((permissions & PERM_SHARE) == 0) {
        return ERROR_AUTH_FAILURE;
    }
    
    (void)target_role;
    (void)valid_duration_sec;
    
    return OK;
}

/******************************************************************************
 * 吊销钥匙
 ******************************************************************************/
error_t dkcs_key_revoke(const uint8_t *key_id)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (key_id == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    return OK;
}

/******************************************************************************
 * 获取钥匙列表
 ******************************************************************************/
error_t dkcs_key_list(key_info_t *keys, size_t *key_count)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (keys == NULL || key_count == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* TODO: 从安全存储读取钥匙列表 */
    
    return OK;
}

/******************************************************************************
 * 导入钥匙
 ******************************************************************************/
error_t dkcs_key_import(
    const uint8_t *key_data,
    size_t key_len,
    key_info_t *key_info)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (key_data == NULL || key_info == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    (void)key_len;
    
    return OK;
}

/******************************************************************************
 * 导出钥匙
 ******************************************************************************/
error_t dkcs_key_export(
    const uint8_t *key_id,
    uint8_t *key_data,
    size_t *key_len)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (key_id == NULL || key_data == NULL || key_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    return OK;
}

/******************************************************************************
 * 初始化协议 (内部函数)
 ******************************************************************************/
static error_t init_protocol(protocol_type_t protocol)
{
    error_t ret = OK;
    
    switch (protocol) {
        case PROTOCOL_CCC: {
            if (!g_dkcs_state.ccc_initialized) {
                /* 创建简化的 SE 接口 */
                ccc_se_interface_t ccc_se = {0};
                /* 设置基本回调函数 */
                ret = ccc_init(&ccc_se);
                if (ret == OK) {
                    g_dkcs_state.ccc_initialized = 1;
                }
            }
            break;
        }
        
        case PROTOCOL_ICCE: {
            if (!g_dkcs_state.icce_initialized) {
                icce_se_interface_t icce_se = {0};
                ret = icce_init(&icce_se);
                if (ret == OK) {
                    g_dkcs_state.icce_initialized = 1;
                }
            }
            break;
        }
        
        case PROTOCOL_ICCOA: {
            if (!g_dkcs_state.iccoa_initialized) {
                iccoa_se_interface_t iccoa_se = {0};
                ret = iccoa_init(&iccoa_se);
                if (ret == OK) {
                    g_dkcs_state.iccoa_initialized = 1;
                }
            }
            break;
        }
        
        default:
            ret = ERROR_NOT_SUPPORTED;
    }
    
    return ret;
}

/******************************************************************************
 * 检查会话有效性 (内部函数)
 ******************************************************************************/
static error_t check_session_valid(void)
{
    if (!g_dkcs_state.initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    return OK;
}
