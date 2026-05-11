/******************************************************************************
 * @file    icce_core.c
 * @brief   ICCE (智慧车联产业生态联盟) 核心协议实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "icce.h"

/******************************************************************************
 * 内部状态定义
 ******************************************************************************/
#define ICCE_STATE_UNINITIALIZED    0
#define ICCE_STATE_INITIALIZED      1

static uint8_t g_icce_state = ICCE_STATE_UNINITIALIZED;
static icce_se_interface_t g_se_interface;

/******************************************************************************
 * 内部辅助函数
 ******************************************************************************/
static uint16_t icce_sw_to_error(uint8_t sw1, uint8_t sw2);
static void icce_update_timestamp(icce_session_context_t *session);
static uint32_t icce_get_time_ms(void);

/******************************************************************************
 * 初始化 ICCE 协议栈
 ******************************************************************************/
error_t icce_init(const icce_se_interface_t *se_if)
{
    if (g_icce_state != ICCE_STATE_UNINITIALIZED) {
        return ERROR_ALREADY_INITIALIZED;
    }
    
    if (se_if == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    memcpy(&g_se_interface, se_if, sizeof(icce_se_interface_t));
    
    error_t ret = g_se_interface.open_channel(NULL, NULL);
    if (ret != OK) {
        return ret;
    }
    
    g_icce_state = ICCE_STATE_INITIALIZED;
    return OK;
}

/******************************************************************************
 * 反初始化
 ******************************************************************************/
error_t icce_deinit(void)
{
    if (g_icce_state == ICCE_STATE_UNINITIALIZED) {
        return ERROR_NOT_INITIALIZED;
    }
    
    g_se_interface.close_channel();
    g_icce_state = ICCE_STATE_UNINITIALIZED;
    
    return OK;
}

/******************************************************************************
 * 构建 APDU 命令
 ******************************************************************************/
error_t icce_build_apdu(
    icce_apdu_t *apdu,
    uint8_t cla,
    icce_command_t ins,
    uint8_t p1,
    uint8_t p2,
    const uint8_t *data,
    uint8_t lc,
    uint8_t le)
{
    if (apdu == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    apdu->cla = cla;
    apdu->ins = ins;
    apdu->p1 = p1;
    apdu->p2 = p2;
    apdu->lc = lc;
    apdu->le = le;
    
    if (lc > 0 && data != NULL) {
        memcpy(apdu->data, data, lc);
    }
    
    return OK;
}

/******************************************************************************
 * 发送 APDU 命令
 ******************************************************************************/
error_t icce_send_apdu(
    icce_session_context_t *session,
    const icce_apdu_t *apdu,
    icce_apdu_response_t *response)
{
    if (apdu == NULL || response == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (g_icce_state != ICCE_STATE_INITIALIZED) {
        return ERROR_NOT_INITIALIZED;
    }
    
    /* 检查会话超时 */
    if (session != NULL) {
        uint32_t elapsed = icce_get_time_ms() - session->last_activity;
        if (elapsed > session->security.session_timeout_ms) {
            session->state = ICCE_SESSION_CLOSED;
            return ICCE_ERR_TIMEOUT;
        }
    }
    
    /* 序列化 APDU */
    uint8_t apdu_data[ICCE_APDU_MAX_LEN + 6];
    size_t apdu_len = 0;
    
    apdu_data[apdu_len++] = apdu->cla;
    apdu_data[apdu_len++] = apdu->ins;
    apdu_data[apdu_len++] = apdu->p1;
    apdu_data[apdu_len++] = apdu->p2;
    
    if (apdu->lc > 0) {
        apdu_data[apdu_len++] = apdu->lc;
        memcpy(&apdu_data[apdu_len], apdu->data, apdu->lc);
        apdu_len += apdu->lc;
    }
    
    if (apdu->le > 0) {
        apdu_data[apdu_len++] = apdu->le;
    }
    
    /* 发送到 SE */
    uint8_t resp[ICCE_APDU_MAX_LEN + 2];
    size_t resp_len = sizeof(resp);
    
    error_t ret = g_se_interface.transmit(apdu_data, apdu_len, resp, &resp_len);
    if (ret != OK) {
        return ICCE_ERR_SE_COMM;
    }
    
    /* 解析响应 */
    ret = icce_parse_response(resp, resp_len, response);
    if (ret != OK) {
        return ret;
    }
    
    /* 检查状态字 */
    if (response->sw1 != 0x90 || response->sw2 != 0x00) {
        return icce_sw_to_error(response->sw1, response->sw2);
    }
    
    if (session != NULL) {
        session->cmd_counter++;
        icce_update_timestamp(session);
    }
    
    return OK;
}

/******************************************************************************
 * 解析 APDU 响应
 ******************************************************************************/
error_t icce_parse_response(
    const uint8_t *raw_data,
    size_t raw_len,
    icce_apdu_response_t *response)
{
    if (raw_data == NULL || response == NULL || raw_len < 2) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 响应数据长度 */
    response->len = raw_len - 2;
    
    if (response->len > 0) {
        memcpy(response->data, raw_data, response->len);
    }
    
    /* 状态字 */
    response->sw1 = raw_data[raw_len - 2];
    response->sw2 = raw_data[raw_len - 1];
    
    return OK;
}

/******************************************************************************
 * 创建配对会话
 ******************************************************************************/
error_t icce_pairing_init(
    const icce_pairing_info_t *info,
    icce_session_context_t **session)
{
    if (info == NULL || session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (g_icce_state != ICCE_STATE_INITIALIZED) {
        return ERROR_NOT_INITIALIZED;
    }
    
    icce_session_context_t *ctx = (icce_session_context_t *)malloc(sizeof(icce_session_context_t));
    if (ctx == NULL) {
        return ERROR_NO_MEMORY;
    }
    
    memset(ctx, 0, sizeof(icce_session_context_t));
    
    /* 初始化状态 */
    ctx->state = ICCE_SESSION_CLOSED;
    memcpy(ctx->device_id, info->device_id, ICCE_DEVICE_ID_LEN);
    memcpy(ctx->vehicle_id, info->vehicle_id, ICCE_VEHICLE_ID_LEN);
    ctx->security.crypto_type = info->crypto_preference;
    ctx->security.se_type = info->se_type;
    ctx->security.session_timeout_ms = 300000;  /* 5分钟 */
    ctx->security.enable_mutual_auth = true;
    ctx->security.enable_channel_binding = true;
    
    *session = ctx;
    return OK;
}

/******************************************************************************
 * 配对认证
 ******************************************************************************/
error_t icce_pairing_authenticate(
    icce_session_context_t *session,
    const icce_certificate_t *device_cert,
    const icce_certificate_t *vehicle_cert,
    uint8_t *auth_data,
    size_t *auth_len)
{
    if (session == NULL || device_cert == NULL || auth_data == NULL || auth_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 选择 ICCE Applet */
    const uint8_t icce_aid[] = {0xA0, 0x00, 0x00, 0x05, 0x59, 0x10, 0x10, 0x01};
    error_t ret = g_se_interface.select_applet(icce_aid, sizeof(icce_aid));
    if (ret != OK) {
        return ICCE_ERR_SE_COMM;
    }
    
    /* 发送配对初始化命令 */
    icce_apdu_t apdu;
    uint8_t p1 = session->security.crypto_type;  /* 加密算法选择 */
    uint8_t p2 = session->security.se_type;      /* SE 类型 */
    
    ret = icce_build_apdu(&apdu, 0x00, ICCE_CMD_PAIRING_INIT, p1, p2,
                          device_cert->public_key, ICCE_ECC_SM2_PUB_KEY_LEN, 0);
    if (ret != OK) {
        return ret;
    }
    
    icce_apdu_response_t response;
    ret = icce_send_apdu(session, &apdu, &response);
    if (ret != OK) {
        return ret;
    }
    
    /* 提取车辆挑战 */
    if (response.len < ICCE_CHALLENGE_LEN) {
        return ICCE_ERR_INVALID_LENGTH;
    }
    
    uint8_t challenge[ICCE_CHALLENGE_LEN];
    memcpy(challenge, response.data, ICCE_CHALLENGE_LEN);
    
    /* 使用 SM2 对挑战进行签名 */
    uint8_t signature[ICCE_SIGNATURE_LEN];
    ret = g_se_interface.sm2_sign(challenge, ICCE_CHALLENGE_LEN, signature);
    if (ret != OK) {
        return ICCE_ERR_CRYPTO_FAIL;
    }
    
    /* 构建认证响应 */
    memcpy(auth_data, signature, ICCE_SIGNATURE_LEN);
    *auth_len = ICCE_SIGNATURE_LEN;
    
    session->is_authenticated = true;
    return OK;
}

/******************************************************************************
 * 完成配对
 ******************************************************************************/
error_t icce_pairing_complete(
    icce_session_context_t *session,
    const uint8_t *confirmation,
    size_t confirm_len)
{
    if (session == NULL || confirmation == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    icce_apdu_t apdu;
    error_t ret = icce_build_apdu(&apdu, 0x00, ICCE_CMD_PAIRING_CONFIRM,
                                       0x00, 0x00, confirmation, confirm_len, 0);
    if (ret != OK) {
        return ret;
    }
    
    icce_apdu_response_t response;
    ret = icce_send_apdu(session, &apdu, &response);
    if (ret != OK) {
        return ret;
    }
    
    /* 提取密钥信息 */
    if (response.len >= ICCE_KEY_ID_LEN) {
        memcpy(session->session_key_enc, response.data, ICCE_SM4_KEY_LEN);
    }
    
    return OK;
}

/******************************************************************************
 * 建立安全会话
 ******************************************************************************/
error_t icce_session_start(
    icce_session_context_t *session,
    const icce_security_config_t *config)
{
    if (session == NULL || config == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    memcpy(&session->security, config, sizeof(icce_security_config_t));
    
    icce_apdu_t apdu;
    uint8_t params[4];
    params[0] = (config->session_timeout_ms >> 24) & 0xFF;
    params[1] = (config->session_timeout_ms >> 16) & 0xFF;
    params[2] = (config->session_timeout_ms >> 8) & 0xFF;
    params[3] = config->session_timeout_ms & 0xFF;
    
    error_t ret = icce_build_apdu(&apdu, 0x00, ICCE_CMD_SESSION_START,
                                       config->crypto_type, config->se_type,
                                       params, 4, 0);
    if (ret != OK) {
        return ret;
    }
    
    icce_apdu_response_t response;
    ret = icce_send_apdu(session, &apdu, &response);
    if (ret != OK) {
        return ret;
    }
    
    if (response.len >= 4) {
        session->session_id = ((uint32_t)response.data[0] << 24) |
                              ((uint32_t)response.data[1] << 16) |
                              ((uint32_t)response.data[2] << 8) |
                              response.data[3];
    }
    
    session->state = ICCE_SESSION_ESTABLISHED;
    session->resp_counter = 0;
    icce_update_timestamp(session);
    
    return OK;
}

/******************************************************************************
 * 关闭会话
 ******************************************************************************/
error_t icce_session_close(icce_session_context_t *session)
{
    if (session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    icce_apdu_t apdu;
    error_t ret = icce_build_apdu(&apdu, 0x00, ICCE_CMD_SESSION_CLOSE,
                                       0x00, 0x00, NULL, 0, 0);
    if (ret != OK) {
        return ret;
    }
    
    icce_apdu_response_t response;
    icce_send_apdu(session, &apdu, &response);
    
    session->state = ICCE_SESSION_CLOSED;
    
    return OK;
}

/******************************************************************************
 * 加密 APDU 数据
 ******************************************************************************/
error_t icce_encrypt_apdu(
    icce_session_context_t *session,
    const uint8_t *plaintext,
    size_t plain_len,
    uint8_t *ciphertext,
    size_t *cipher_len)
{
    if (session == NULL || plaintext == NULL || ciphertext == NULL || cipher_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (session->security.crypto_type == ICCE_CRYPTO_SM4) {
        /* SM4 CBC 加密 */
        return icce_sm4_encrypt_cbc(
            session->session_key_enc,
            session->iv,
            plaintext,
            plain_len,
            ciphertext,
            cipher_len
        );
    } else if (session->security.crypto_type == ICCE_CRYPTO_AES128) {
        /* AES-128 CBC 加密 */
        /* 调用 SE 接口 */
        (void)session;
        (void)plaintext;
        (void)plain_len;
        (void)ciphertext;
        *cipher_len = plain_len;
        return OK;
    }
    
    return ERROR_NOT_SUPPORTED;
}

/******************************************************************************
 * 车辆控制命令
 ******************************************************************************/
error_t icce_vehicle_control(
    icce_session_context_t *session,
    icce_command_t cmd,
    const uint8_t *params,
    size_t params_len,
    uint8_t *response_data,
    size_t *response_len)
{
    if (session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (session->state != ICCE_SESSION_ESTABLISHED) {
        return ICCE_ERR_SESSION_INVALID;
    }
    
    icce_apdu_t apdu;
    error_t ret = icce_build_apdu(&apdu, 0x00, cmd, 0x00, 0x00,
                                       params, (uint8_t)params_len, 0);
    if (ret != OK) {
        return ret;
    }
    
    icce_apdu_response_t response;
    ret = icce_send_apdu(session, &apdu, &response);
    if (ret != OK) {
        return ret;
    }
    
    if (response_data != NULL && response_len != NULL) {
        size_t copy_len = response.len < *response_len ? response.len : *response_len;
        memcpy(response_data, response.data, copy_len);
        *response_len = copy_len;
    }
    
    return OK;
}

/******************************************************************************
 * 获取车辆状态
 ******************************************************************************/
error_t icce_get_vehicle_status(
    icce_session_context_t *session,
    icce_vehicle_status_t *status)
{
    if (session == NULL || status == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    uint8_t resp[32];
    size_t resp_len = sizeof(resp);
    
    error_t ret = icce_vehicle_control(session, ICCE_CMD_STATUS_GET,
                                            NULL, 0, resp, &resp_len);
    if (ret != OK) {
        return ret;
    }
    
    /* 解析状态数据 */
    if (resp_len >= 16) {
        status->lock_status = resp[0];
        status->engine_status = resp[1];
        memcpy(status->door_status, &resp[2], 4);
        status->trunk_status = resp[6];
        status->fuel_level = resp[7];
        status->temperature = (int8_t)resp[8];
    }
    
    return OK;
}

/******************************************************************************
 * SM4 CBC 加密 (含 PKCS7 填充)
 ******************************************************************************/
error_t icce_sm4_encrypt_cbc(
    const uint8_t *key,
    const uint8_t *iv,
    const uint8_t *plaintext,
    size_t plain_len,
    uint8_t *ciphertext,
    size_t *cipher_len)
{
    if (key == NULL || plaintext == NULL || ciphertext == NULL || cipher_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* PKCS7 填充 */
    size_t pad_len = 16 - (plain_len % 16);
    size_t total_len = plain_len + pad_len;
    
    uint8_t padded[ICCE_APDU_MAX_LEN];
    memcpy(padded, plaintext, plain_len);
    memset(&padded[plain_len], pad_len, pad_len);
    
    /* 调用 SE 进行 SM4 加密 */
    error_t ret = g_se_interface.sm4_encrypt(key, iv, padded, total_len, ciphertext);
    if (ret != OK) {
        return ret;
    }
    
    *cipher_len = total_len;
    return OK;
}

/******************************************************************************
 * 错误码转换
 ******************************************************************************/
static uint16_t icce_sw_to_error(uint8_t sw1, uint8_t sw2)
{
    uint16_t sw = ((uint16_t)sw1 << 8) | sw2;
    
    switch (sw) {
        case 0x6A82: return ICCE_ERR_KEY_NOT_FOUND;
        case 0x6982: return ICCE_ERR_AUTH_FAIL;
        case 0x6A84: return ICCE_ERR_MEMORY;
        case 0x6D00: return ICCE_ERR_INVALID_CMD;
        case 0x6E00: return ICCE_ERR_INVALID_CMD;
        case 0x6A86: return ICCE_ERR_INVALID_PARAM;
        default: return ICCE_ERR_INTERNAL;
    }
}

/******************************************************************************
 * 错误码到字符串
 ******************************************************************************/
const char* icce_error_to_string(icce_error_code_t err)
{
    switch (err) {
        case ICCE_OK: return "Success";
        case ICCE_ERR_INVALID_CMD: return "Invalid command";
        case ICCE_ERR_INVALID_PARAM: return "Invalid parameter";
        case ICCE_ERR_INVALID_LENGTH: return "Invalid length";
        case ICCE_ERR_AUTH_FAIL: return "Authentication failed";
        case ICCE_ERR_SE_COMM: return "SE communication error";
        case ICCE_ERR_CRYPTO_FAIL: return "Crypto operation failed";
        case ICCE_ERR_KEY_NOT_FOUND: return "Key not found";
        case ICCE_ERR_KEY_REVOKED: return "Key revoked";
        case ICCE_ERR_KEY_EXPIRED: return "Key expired";
        case ICCE_ERR_SESSION_INVALID: return "Invalid session";
        case ICCE_ERR_TIMEOUT: return "Operation timeout";
        case ICCE_ERR_BUSY: return "Device busy";
        case ICCE_ERR_MEMORY: return "Memory error";
        default: return "Unknown error";
    }
}

/******************************************************************************
 * 更新时间戳
 ******************************************************************************/
static void icce_update_timestamp(icce_session_context_t *session)
{
    session->last_activity = icce_get_time_ms();
}

/******************************************************************************
 * 获取毫秒时间
 ******************************************************************************/
static uint32_t icce_get_time_ms(void)
{
    static uint32_t tick = 0;
    return ++tick * 10;
}

/******************************************************************************
 * 释放会话
 ******************************************************************************/
void icce_session_destroy(icce_session_context_t *session)
{
    if (session == NULL) {
        return;
    }
    
    /* 清除敏感数据 */
    memset(session->session_key_enc, 0, sizeof(session->session_key_enc));
    memset(session->session_key_mac, 0, sizeof(session->session_key_mac));
    memset(session->device_private_key, 0, sizeof(session->device_private_key));
    
    free(session);
}
