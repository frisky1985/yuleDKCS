/******************************************************************************
 * @file    iccoa_core.c
 * @brief   ICCOA (智慧车联开放联盟) 核心协议实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "iccoa.h"

/******************************************************************************
 * 内部状态和宏定义
 ******************************************************************************/
#define ICCOA_STATE_UNINITIALIZED   0
#define ICCOA_STATE_INITIALIZED     1

static uint8_t g_iccoa_state = ICCOA_STATE_UNINITIALIZED;
static iccoa_se_interface_t g_se_interface;

/******************************************************************************
 * 内部辅助函数
 ******************************************************************************/
static uint32_t iccoa_get_timestamp(void);
static error_t iccoa_validate_message_header(const iccoa_message_header_t *header);
static void iccoa_update_activity(iccoa_session_context_t *session);

/******************************************************************************
 * 初始化 ICCOA 协议栈
 ******************************************************************************/
error_t iccoa_init(const iccoa_se_interface_t *se_if)
{
    if (g_iccoa_state != ICCOA_STATE_UNINITIALIZED) {
        return ERROR_ALREADY_INITIALIZED;
    }
    
    if (se_if == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    memcpy(&g_se_interface, se_if, sizeof(iccoa_se_interface_t));
    
    error_t ret = g_se_interface.init();
    if (ret != OK) {
        return ret;
    }
    
    g_iccoa_state = ICCOA_STATE_INITIALIZED;
    return OK;
}

/******************************************************************************
 * 反初始化
 ******************************************************************************/
error_t iccoa_deinit(void)
{
    if (g_iccoa_state == ICCOA_STATE_UNINITIALIZED) {
        return ERROR_NOT_INITIALIZED;
    }
    
    g_se_interface.deinit();
    g_iccoa_state = ICCOA_STATE_UNINITIALIZED;
    
    return OK;
}

/******************************************************************************
 * 构建 TLV 元素
 ******************************************************************************/
error_t iccoa_build_tlv(
    iccoa_tlv_t *tlv,
    iccoa_tlv_type_t type,
    uint16_t length,
    const uint8_t *value)
{
    if (tlv == NULL || (length > 0 && value == NULL)) {
        return ERROR_INVALID_PARAM;
    }
    
    tlv->type = type;
    tlv->length = length;
    
    if (length > 0) {
        tlv->value = (uint8_t *)malloc(length);
        if (tlv->value == NULL) {
            return ERROR_NO_MEMORY;
        }
        memcpy(tlv->value, value, length);
    } else {
        tlv->value = NULL;
    }
    
    return OK;
}

/******************************************************************************
 * 解析 TLV 数据
 ******************************************************************************/
error_t iccoa_parse_tlvs(
    const uint8_t *data,
    size_t len,
    iccoa_tlv_t *tlvs,
    uint8_t *tlv_count)
{
    if (data == NULL || tlvs == NULL || tlv_count == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    size_t offset = 0;
    uint8_t count = 0;
    const uint8_t max_tlvs = *tlv_count;
    
    while (offset < len && count < max_tlvs) {
        if (offset + 3 > len) {
            break;
        }
        
        iccoa_tlv_type_t type = (iccoa_tlv_type_t)data[offset++];
        uint16_t length = ((uint16_t)data[offset] << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + length > len) {
            return ERROR_INVALID_PARAM;
        }
        
        error_t ret = iccoa_build_tlv(&tlvs[count], type, length, &data[offset]);
        if (ret != OK) {
            return ret;
        }
        
        offset += length;
        count++;
    }
    
    *tlv_count = count;
    return OK;
}

/******************************************************************************
 * 构建消息
 ******************************************************************************/
error_t iccoa_build_message(
    iccoa_message_t *msg,
    iccoa_command_t cmd,
    const iccoa_tlv_t *tlvs,
    uint8_t tlv_count,
    uint16_t sequence)
{
    if (msg == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 初始化消息头 */
    msg->header.magic = ICCOA_MAGIC;
    msg->header.version = (ICCOA_VERSION_MAJOR << 4) | ICCOA_VERSION_MINOR;
    msg->header.cmd = cmd;
    msg->header.flags = 0;
    msg->header.sequence = sequence;
    
    /* 复制 TLV */
    if (tlv_count > 0 && tlvs != NULL) {
        msg->tlvs = (iccoa_tlv_t *)malloc(tlv_count * sizeof(iccoa_tlv_t));
        if (msg->tlvs == NULL) {
            return ERROR_NO_MEMORY;
        }
        memcpy(msg->tlvs, tlvs, tlv_count * sizeof(iccoa_tlv_t));
        msg->tlv_count = tlv_count;
    } else {
        msg->tlvs = NULL;
        msg->tlv_count = 0;
    }
    
    msg->raw_data = NULL;
    msg->raw_len = 0;
    
    return OK;
}

/******************************************************************************
 * 序列化消息
 ******************************************************************************/
error_t iccoa_serialize_message(
    const iccoa_message_t *msg,
    uint8_t *buffer,
    size_t *len)
{
    if (msg == NULL || buffer == NULL || len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    size_t offset = 0;
    size_t total_len = 8; /* 头部长度 */
    
    /* 计算 TLV 总长度 */
    for (uint8_t i = 0; i < msg->tlv_count; i++) {
        total_len += 3 + msg->tlvs[i].length;
    }
    
    if (*len < total_len) {
        return ERROR_BUFFER_TOO_SMALL;
    }
    
    /* 写入头部 */
    buffer[offset++] = msg->header.magic;
    buffer[offset++] = msg->header.version;
    buffer[offset++] = (total_len >> 8) & 0xFF;
    buffer[offset++] = total_len & 0xFF;
    buffer[offset++] = msg->header.cmd;
    buffer[offset++] = msg->header.flags;
    buffer[offset++] = (msg->header.sequence >> 8) & 0xFF;
    buffer[offset++] = msg->header.sequence & 0xFF;
    
    /* 写入 TLV */
    for (uint8_t i = 0; i < msg->tlv_count; i++) {
        buffer[offset++] = msg->tlvs[i].type;
        buffer[offset++] = (msg->tlvs[i].length >> 8) & 0xFF;
        buffer[offset++] = msg->tlvs[i].length & 0xFF;
        if (msg->tlvs[i].length > 0 && msg->tlvs[i].value != NULL) {
            memcpy(&buffer[offset], msg->tlvs[i].value, msg->tlvs[i].length);
            offset += msg->tlvs[i].length;
        }
    }
    
    *len = offset;
    return OK;
}

/******************************************************************************
 * 反序列化消息
 ******************************************************************************/
error_t iccoa_deserialize_message(
    const uint8_t *data,
    size_t len,
    iccoa_message_t *msg)
{
    if (data == NULL || msg == NULL || len < 8) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 解析头部 */
    msg->header.magic = data[0];
    msg->header.version = data[1];
    msg->header.length = ((uint16_t)data[2] << 8) | data[3];
    msg->header.cmd = data[4];
    msg->header.flags = data[5];
    msg->header.sequence = ((uint16_t)data[6] << 8) | data[7];
    
    /* 验证魔数 */
    if (msg->header.magic != ICCOA_MAGIC) {
        return ICCOA_ERR_INVALID_PARAM;
    }
    
    /* 解析 TLV */
    if (len > 8) {
        const uint8_t max_tlvs = 16;
        iccoa_tlv_t tlvs[16];
        uint8_t tlv_count = max_tlvs;
        
        error_t ret = iccoa_parse_tlvs(&data[8], len - 8, tlvs, &tlv_count);
        if (ret != OK) {
            return ret;
        }
        
        msg->tlvs = (iccoa_tlv_t *)malloc(tlv_count * sizeof(iccoa_tlv_t));
        if (msg->tlvs == NULL) {
            return ERROR_NO_MEMORY;
        }
        memcpy(msg->tlvs, tlvs, tlv_count * sizeof(iccoa_tlv_t));
        msg->tlv_count = tlv_count;
    } else {
        msg->tlvs = NULL;
        msg->tlv_count = 0;
    }
    
    msg->raw_data = (uint8_t *)malloc(len);
    if (msg->raw_data != NULL) {
        memcpy(msg->raw_data, data, len);
        msg->raw_len = len;
    }
    
    return OK;
}

/******************************************************************************
 * 配对开始
 ******************************************************************************/
error_t iccoa_pairing_start(
    const iccoa_pairing_config_t *config,
    iccoa_session_context_t **session)
{
    if (config == NULL || session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (g_iccoa_state != ICCOA_STATE_INITIALIZED) {
        return ERROR_NOT_INITIALIZED;
    }
    
    iccoa_session_context_t *ctx = (iccoa_session_context_t *)malloc(sizeof(iccoa_session_context_t));
    if (ctx == NULL) {
        return ERROR_NO_MEMORY;
    }
    
    memset(ctx, 0, sizeof(iccoa_session_context_t));
    
    /* 初始化状态 */
    ctx->state = ICCOA_SESSION_STATE_IDLE;
    ctx->pairing_state = ICCOA_PAIRING_STATE_IDLE;
    ctx->conn_type = config->enable_ble ? ICCOA_CONN_BLE : ICCOA_CONN_NFC;
    ctx->session_timeout_ms = config->valid_duration > 0 ? config->valid_duration : 300000;
    
    /* 复制设备信息 */
    memcpy(ctx->device_id, config->device_id, ICCOA_DEVICE_ID_LEN);
    memcpy(ctx->bt_address, config->bt_address, ICCOA_MAX_BT_ADDR_LEN);
    
    /* 生成临时密钥对 */
    error_t ret = g_se_interface.generate_keypair(ctx->device_public_key,
                                                        ctx->device_private_key);
    if (ret != OK) {
        free(ctx);
        return ret;
    }
    
    /* 生成随机 nonce */
    for (int i = 0; i < ICCOA_NONCE_LEN; i++) {
        ctx->nonce[i] = (uint8_t)(iccoa_get_timestamp() + i);
    }
    
    *session = ctx;
    return OK;
}

/******************************************************************************
 * 密钥交换
 ******************************************************************************/
error_t iccoa_pairing_exchange_key(
    iccoa_session_context_t *session,
    const uint8_t *vehicle_pub_key,
    uint8_t *device_pub_key)
{
    if (session == NULL || vehicle_pub_key == NULL || device_pub_key == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 复制车辆公钥 */
    memcpy(session->vehicle_public_key, vehicle_pub_key, ICCOA_ECC_P256_PUB_KEY_LEN);
    
    /* 返回设备公钥 */
    memcpy(device_pub_key, session->device_public_key, ICCOA_ECC_P256_PUB_KEY_LEN);
    
    /* 计算共享秘钥 */
    uint8_t shared_secret[32];
    error_t ret = g_se_interface.derive_key(session->device_private_key,
                                                  session->vehicle_public_key,
                                                  shared_secret);
    if (ret != OK) {
        return ret;
    }
    
    /* 派生会话密钥 */
    ret = g_se_interface.derive_key(shared_secret, (uint8_t *)"ICCOA-SESSION", 
                                    session->session_key_enc);
    if (ret != OK) {
        return ret;
    }
    
    ret = g_se_interface.derive_key(shared_secret, (uint8_t *)"ICCOA-MAC",
                                    session->session_key_mac);
    if (ret != OK) {
        return ret;
    }
    
    session->pairing_state = ICCOA_PAIRING_STATE_KEY_EXCHANGED;
    return OK;
}

/******************************************************************************
 * 配对认证
 ******************************************************************************/
error_t iccoa_pairing_authenticate(
    iccoa_session_context_t *session,
    const uint8_t *challenge,
    uint8_t *response)
{
    if (session == NULL || challenge == NULL || response == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 使用设备私钥签名挑战 */
    error_t ret = g_se_interface.sign(challenge, ICCOA_CHALLENGE_LEN, response);
    if (ret != OK) {
        return ICCOA_ERR_AUTH_FAIL;
    }
    
    session->pairing_state = ICCOA_PAIRING_STATE_AUTHENTICATED;
    return OK;
}

/******************************************************************************
 * 确认配对
 ******************************************************************************/
error_t iccoa_pairing_confirm(
    iccoa_session_context_t *session,
    const uint8_t *confirmation)
{
    if (session == NULL || confirmation == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 验证确认消息 */
    (void)confirmation;
    
    session->pairing_state = ICCOA_PAIRING_STATE_CONFIRMED;
    return OK;
}

/******************************************************************************
 * 完成配对
 ******************************************************************************/
error_t iccoa_pairing_complete(
    iccoa_session_context_t *session,
    key_info_t *key_info)
{
    if (session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 生成密钥ID */
    for (int i = 0; i < ICCOA_KEY_ID_LEN; i++) {
        session->key_id[i] = (uint8_t)(iccoa_get_timestamp() + i);
    }
    
    if (key_info != NULL) {
        memcpy(key_info->key_id, session->key_id, ICCOA_KEY_ID_LEN);
        key_info->protocol = PROTOCOL_ICCOA;
        key_info->status = KEY_STATUS_ACTIVE;
        key_info->preferred_conn = session->conn_type == ICCOA_CONN_BLE ? 
                                   CONN_BLE : CONN_NFC;
    }
    
    session->pairing_state = ICCOA_PAIRING_STATE_COMPLETED;
    session->is_secure = true;
    session->is_authenticated = true;
    
    return OK;
}

/******************************************************************************
 * 创建会话
 ******************************************************************************/
error_t iccoa_session_create(
    iccoa_session_context_t *session,
    const uint8_t *key_id)
{
    if (session == NULL || key_id == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    memcpy(session->key_id, key_id, ICCOA_KEY_ID_LEN);
    session->session_id = iccoa_get_timestamp();
    session->tx_sequence = 0;
    session->rx_sequence = 0;
    session->state = ICCOA_SESSION_STATE_ACTIVE;
    
    iccoa_update_activity(session);
    
    return OK;
}

/******************************************************************************
 * 发送命令
 ******************************************************************************/
error_t iccoa_send_command(
    iccoa_session_context_t *session,
    iccoa_command_t cmd,
    const iccoa_tlv_t *tlvs,
    uint8_t tlv_count,
    iccoa_message_t *response)
{
    if (session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (session->state != ICCOA_SESSION_STATE_ACTIVE) {
        return ICCOA_ERR_SESSION_INVALID;
    }
    
    /* 检查超时 */
    uint32_t elapsed = iccoa_get_timestamp() - session->last_activity;
    if (elapsed > session->session_timeout_ms) {
        session->state = ICCOA_SESSION_STATE_IDLE;
        return ICCOA_ERR_TIMEOUT;
    }
    
    /* 构建请求消息 */
    iccoa_message_t request;
    error_t ret = iccoa_build_message(&request, cmd, tlvs, tlv_count, 
                                           session->tx_sequence++);
    if (ret != OK) {
        return ret;
    }
    
    /* TODO: 通过传输层发送 */
    
    iccoa_update_activity(session);
    
    (void)response;
    return OK;
}

/******************************************************************************
 * 车辆控制
 ******************************************************************************/
error_t iccoa_vehicle_control(
    iccoa_session_context_t *session,
    iccoa_command_t cmd,
    const uint8_t *params,
    size_t params_len,
    uint8_t *result)
{
    if (session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    iccoa_tlv_t tlvs[2];
    uint8_t tlv_count = 0;
    
    /* 添加密钥ID */
    iccoa_build_tlv(&tlvs[tlv_count++], ICCOA_TLV_KEY_ID, 
                    ICCOA_KEY_ID_LEN, session->key_id);
    
    /* 添加参数 */
    if (params != NULL && params_len > 0) {
        iccoa_build_tlv(&tlvs[tlv_count++], ICCOA_TLV_PAYLOAD, params_len, params);
    }
    
    iccoa_message_t response;
    error_t ret = iccoa_send_command(session, cmd, tlvs, tlv_count, &response);
    
    /* 清理 TLV */
    for (uint8_t i = 0; i < tlv_count; i++) {
        if (tlvs[i].value != NULL) {
            free(tlvs[i].value);
        }
    }
    
    if (ret != OK) {
        return ret;
    }
    
    /* 解析响应 */
    if (result != NULL && response.tlv_count > 0) {
        for (uint8_t i = 0; i < response.tlv_count; i++) {
            if (response.tlvs[i].type == ICCOA_TLV_CONTROL_RESULT &&
                response.tlvs[i].length > 0) {
                *result = response.tlvs[i].value[0];
                break;
            }
        }
    }
    
    iccoa_free_message(&response);
    return OK;
}

/******************************************************************************
 * 获取车辆状态
 ******************************************************************************/
error_t iccoa_get_vehicle_status(
    iccoa_session_context_t *session,
    iccoa_vehicle_status_t *status)
{
    if (session == NULL || status == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    iccoa_tlv_t tlv;
    iccoa_build_tlv(&tlv, ICCOA_TLV_KEY_ID, ICCOA_KEY_ID_LEN, session->key_id);
    
    iccoa_message_t response;
    error_t ret = iccoa_send_command(session, ICCOA_CMD_QUERY_STATUS, &tlv, 1, &response);
    
    if (tlv.value != NULL) {
        free(tlv.value);
    }
    
    if (ret != OK) {
        return ret;
    }
    
    /* 解析状态 TLV */
    for (uint8_t i = 0; i < response.tlv_count; i++) {
        if (response.tlvs[i].type == ICCOA_TLV_VEHICLE_STATUS &&
            response.tlvs[i].length >= sizeof(iccoa_vehicle_status_t)) {
            memcpy(status, response.tlvs[i].value, sizeof(iccoa_vehicle_status_t));
            break;
        }
    }
    
    iccoa_free_message(&response);
    return OK;
}

/******************************************************************************
 * 错误码转字符串
 ******************************************************************************/
const char* iccoa_error_to_string(iccoa_error_code_t err)
{
    switch (err) {
        case ICCOA_OK: return "Success";
        case ICCOA_ERR_UNSUPPORTED_CMD: return "Unsupported command";
        case ICCOA_ERR_INVALID_PARAM: return "Invalid parameter";
        case ICCOA_ERR_INVALID_LENGTH: return "Invalid length";
        case ICCOA_ERR_AUTH_FAIL: return "Authentication failed";
        case ICCOA_ERR_KEY_NOT_FOUND: return "Key not found";
        case ICCOA_ERR_KEY_REVOKED: return "Key revoked";
        case ICCOA_ERR_KEY_EXPIRED: return "Key expired";
        case ICCOA_ERR_SESSION_INVALID: return "Invalid session";
        case ICCOA_ERR_BUSY: return "Device busy";
        case ICCOA_ERR_TIMEOUT: return "Operation timeout";
        case ICCOA_ERR_MEMORY: return "Memory error";
        case ICCOA_ERR_CRYPTO: return "Crypto error";
        case ICCOA_ERR_TRANSPORT: return "Transport error";
        case ICCOA_ERR_INTERNAL: return "Internal error";
        case ICCOA_ERR_PAIRING_CANCELLED: return "Pairing cancelled";
        case ICCOA_ERR_VERSION_MISMATCH: return "Version mismatch";
        case ICCOA_ERR_SE_UNAVAILABLE: return "SE unavailable";
        case ICCOA_ERR_PERMISSION_DENIED: return "Permission denied";
        default: return "Unknown error";
    }
}

/******************************************************************************
 * 释放消息
 ******************************************************************************/
void iccoa_free_message(iccoa_message_t *msg)
{
    if (msg == NULL) {
        return;
    }
    
    if (msg->tlvs != NULL) {
        for (uint8_t i = 0; i < msg->tlv_count; i++) {
            if (msg->tlvs[i].value != NULL) {
                free(msg->tlvs[i].value);
            }
        }
        free(msg->tlvs);
        msg->tlvs = NULL;
    }
    
    if (msg->raw_data != NULL) {
        free(msg->raw_data);
        msg->raw_data = NULL;
    }
}

/******************************************************************************
 * 释放会话
 ******************************************************************************/
void iccoa_session_destroy(iccoa_session_context_t *session)
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

/******************************************************************************
 * 内部辅助函数实现
 ******************************************************************************/
static uint32_t iccoa_get_timestamp(void)
{
    static uint32_t tick = 0;
    return ++tick * 10;
}

static void iccoa_update_activity(iccoa_session_context_t *session)
{
    session->last_activity = iccoa_get_timestamp();
}

static error_t iccoa_validate_message_header(const iccoa_message_header_t *header)
{
    if (header->magic != ICCOA_MAGIC) {
        return ICCOA_ERR_INVALID_PARAM;
    }
    return OK;
}
