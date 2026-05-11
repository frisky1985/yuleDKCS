/******************************************************************************
 * @file    ccc_core.c
 * @brief   CCC Digital Key R3 核心协议实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "ccc.h"
#include "dkcs.h"

/******************************************************************************
 * 内部状态和宏定义
 ******************************************************************************/
#define CCC_STATE_UNINITIALIZED     0
#define CCC_STATE_INITIALIZED       1
#define CCC_STATE_SESSION_ACTIVE    2

static uint8_t g_ccc_state = CCC_STATE_UNINITIALIZED;
static ccc_se_interface_t g_se_interface;

/* CCC 固定定义 */
static const uint8_t CCC_MASTER_SECRET_LABEL[] = "CCC-R3-MASTER-SECRET";
static const uint8_t CCC_SESSION_KEY_LABEL[] = "CCC-R3-SESSION-KEYS";
static const uint8_t CCC_CHALLENGE_LABEL[] = "CCC-R3-CHALLENGE";

/******************************************************************************
 * 内部辅助函数宣告
 ******************************************************************************/
static error_t ccc_hkdf_extract(
    const uint8_t *ikm, size_t ikm_len,
    const uint8_t *salt, size_t salt_len,
    uint8_t *prk
);

static error_t ccc_hkdf_expand(
    const uint8_t *prk, size_t prk_len,
    const uint8_t *info, size_t info_len,
    uint8_t *okm, size_t okm_len
);

static error_t ccc_generate_nonce(uint8_t *nonce, size_t len);
static uint32_t ccc_get_timestamp(void);

/******************************************************************************
 * CCC 初始化
 ******************************************************************************/
error_t ccc_init(const ccc_se_interface_t *se_interface)
{
    if (g_ccc_state != CCC_STATE_UNINITIALIZED) {
        return ERROR_ALREADY_INITIALIZED;
    }
    
    if (se_interface == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 复制 SE 接口 */
    memcpy(&g_se_interface, se_interface, sizeof(ccc_se_interface_t));
    
    /* 初始化 SE */
    error_t ret = g_se_interface.init();
    if (ret != OK) {
        return ret;
    }
    
    g_ccc_state = CCC_STATE_INITIALIZED;
    return OK;
}

/******************************************************************************
 * CCC 反初始化
 ******************************************************************************/
error_t ccc_deinit(void)
{
    if (g_ccc_state == CCC_STATE_UNINITIALIZED) {
        return ERROR_NOT_INITIALIZED;
    }
    
    g_se_interface.deinit();
    g_ccc_state = CCC_STATE_UNINITIALIZED;
    
    memset(&g_se_interface, 0, sizeof(ccc_se_interface_t));
    return OK;
}

/******************************************************************************
 * 创建配对会话
 ******************************************************************************/
error_t ccc_create_pairing_session(
    const ccc_pairing_config_t *config,
    ccc_session_context_t **session)
{
    if (g_ccc_state != CCC_STATE_INITIALIZED) {
        return ERROR_NOT_INITIALIZED;
    }
    
    if (config == NULL || session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 分配会话上下文 */
    ccc_session_context_t *ctx = (ccc_session_context_t *)malloc(sizeof(ccc_session_context_t));
    if (ctx == NULL) {
        return ERROR_NO_MEMORY;
    }
    
    memset(ctx, 0, sizeof(ccc_session_context_t));
    
    /* 初始化会话状态 */
    ctx->state = CCC_SESSION_STATE_IDLE;
    ctx->pairing_state = CCC_PAIRING_STATE_IDLE;
    ctx->session_timeout_ms = 300000;  /* 5分钟默认超时 */
    ctx->last_activity = ccc_get_timestamp();
    ctx->is_secure_session = false;
    
    /* 复制证书链 */
    memcpy(&ctx->device_cert_chain, &config->device_cert_chain, sizeof(ccc_cert_chain_t));
    
    *session = ctx;
    return OK;
}

/******************************************************************************
 * 开始配对流程
 ******************************************************************************/
error_t ccc_start_pairing(
    ccc_session_context_t *session,
    uint8_t *request_data,
    size_t *request_len)
{
    if (session == NULL || request_data == NULL || request_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (session->state != CCC_SESSION_STATE_IDLE) {
        return ERROR_ALREADY_INITIALIZED;
    }
    
    /* 生成临时密钥对 */
    error_t ret = g_se_interface.generate_key_pair(
        session->ephemeral_public_key,
        session->ephemeral_private_key
    );
    if (ret != OK) {
        return ret;
    }
    
    /* 生成挑战 */
    ret = ccc_generate_nonce(session->challenge, CCC_CHALLENGE_LEN);
    if (ret != OK) {
        return ret;
    }
    
    /* 构建配对请求消息 */
    /* 格式: [Version(1)][EphemeralPubKey(65)][Challenge(32)][DeviceCertLen(2)][DeviceCert(n)] */
    size_t offset = 0;
    
    /* 版本 */
    request_data[offset++] = (CCC_VERSION_MAJOR << 4) | CCC_VERSION_MINOR;
    
    /* 临时公钥 */
    memcpy(&request_data[offset], session->ephemeral_public_key, 65);
    offset += 65;
    
    /* 挑战 */
    memcpy(&request_data[offset], session->challenge, CCC_CHALLENGE_LEN);
    offset += CCC_CHALLENGE_LEN;
    
    /* 证书 - 使用 X.509 DER 序列化 */
    if (session->device_cert_chain.cert_count > 0) {
        size_t cert_len = sizeof(request_data) - offset - 2;
        error_t ret = ccc_serialize_certificate(
            &session->device_cert_chain.certs[0],
            &request_data[offset + 2],
            &cert_len
        );
        if (ret != OK) {
            return ret;
        }
        /* 证书长度 (2字节大端) */
        request_data[offset++] = (cert_len >> 8) & 0xFF;
        request_data[offset++] = cert_len & 0xFF;
        offset += cert_len;
    } else {
        /* 无证书时发送0长度 */
        request_data[offset++] = 0x00;
        request_data[offset++] = 0x00;
    }
    
    *request_len = offset;
    
    /* 更新状态 */
    session->state = CCC_SESSION_STATE_PAIRING;
    session->pairing_state = CCC_PAIRING_STATE_WAITING_VEHICLE;
    session->last_activity = ccc_get_timestamp();
    
    return OK;
}

/******************************************************************************
 * 处理配对响应
 ******************************************************************************/
error_t ccc_process_pairing_response(
    ccc_session_context_t *session,
    const uint8_t *response_data,
    size_t response_len)
{
    if (session == NULL || response_data == NULL || response_len == 0) {
        return ERROR_INVALID_PARAM;
    }
    
    if (session->pairing_state != CCC_PAIRING_STATE_WAITING_VEHICLE) {
        return ERROR_PROTOCOL_FAILURE;
    }
    
    /* 解析响应: [Version(1)][VehiclePubKey(65)][VehicleChallenge(32)][Sig(64)] */
    if (response_len < (1 + 65 + 32 + 64)) {
        return ERROR_INVALID_PARAM;
    }
    
    size_t offset = 0;
    
    /* 版本校验 */
    uint8_t version = response_data[offset++];
    uint8_t major_ver = (version >> 4) & 0x0F;
    if (major_ver != CCC_VERSION_MAJOR) {
        return CCC_ERROR_VERSION_MISMATCH;
    }
    
    /* 车辆公钥 */
    memcpy(session->vehicle_public_key, &response_data[offset], 65);
    offset += 65;
    
    /* 车辆挑战 */
    uint8_t vehicle_challenge[CCC_CHALLENGE_LEN];
    memcpy(vehicle_challenge, &response_data[offset], CCC_CHALLENGE_LEN);
    offset += CCC_CHALLENGE_LEN;
    
    /* 验证签名 */
    uint8_t signature[64];
    memcpy(signature, &response_data[offset], 64);
    
    /* TODO: 验证车辆证书和签名 */
    
    /* 计算共享秘钥 */
    error_t ret = g_se_interface.derive_shared_secret(
        session->ephemeral_private_key,
        session->vehicle_public_key,
        session->shared_secret
    );
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }
    
    /* 派生 Master Secret */
    ret = ccc_derive_session_keys(
        session->shared_secret,
        (uint8_t *)CCC_MASTER_SECRET_LABEL,
        session->master_secret,
        session->master_secret + CCC_SESSION_KEY_LEN
    );
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }
    
    /* 更新状态 */
    session->pairing_state = CCC_PAIRING_STATE_DEVICE_VERIFIED;
    session->last_activity = ccc_get_timestamp();
    
    return OK;
}

/******************************************************************************
 * 完成配对确认
 ******************************************************************************/
error_t ccc_complete_pairing(
    ccc_session_context_t *session,
    uint8_t *confirmation_data,
    size_t *confirmation_len)
{
    if (session == NULL || confirmation_data == NULL || confirmation_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (session->pairing_state != CCC_PAIRING_STATE_DEVICE_VERIFIED) {
        return ERROR_PROTOCOL_FAILURE;
    }
    
    /* 生成确认响应: 签名 + 证书 */
    size_t offset = 0;
    
    /* 对车辆挑战进行签名 */
    uint8_t challenge_response[64];
    error_t ret = g_se_interface.sign(
        session->challenge,
        CCC_CHALLENGE_LEN,
        session->device_private_key,
        challenge_response
    );
    if (ret != OK) {
        return CCC_ERROR_SIGNATURE_INVALID;
    }
    
    memcpy(&confirmation_data[offset], challenge_response, 64);
    offset += 64;
    
    /* 添加设备证书链 */
    if (session->device_cert_chain.cert_count > 0) {
        /* 证书链长度 (2字节) */
        size_t chain_data_len = 0;
        
        for (int i = 0; i < session->device_cert_chain.cert_count; i++) {
            size_t cert_len = *confirmation_len - offset - chain_data_len - 4;
            error_t ret = ccc_serialize_certificate(
                &session->device_cert_chain.certs[i],
                &confirmation_data[offset + 4 + chain_data_len],
                &cert_len
            );
            if (ret != OK) {
                return ret;
            }
            chain_data_len += cert_len;
        }
        
        confirmation_data[offset++] = (chain_data_len >> 8) & 0xFF;
        confirmation_data[offset++] = chain_data_len & 0xFF;
        offset += chain_data_len;
    } else {
        confirmation_data[offset++] = 0x00;
        confirmation_data[offset++] = 0x00;
    }
    
    *confirmation_len = offset;
    
    /* 更新状态 */
    session->pairing_state = CCC_PAIRING_STATE_COMPLETE;
    session->state = CCC_SESSION_STATE_ACTIVE;
    session->is_secure_session = true;
    session->last_activity = ccc_get_timestamp();
    
    return OK;
}

/******************************************************************************
 * 建立安全会话
 ******************************************************************************/
error_t ccc_establish_session(
    ccc_session_context_t *session,
    const uint8_t *vehicle_public_key,
    const uint8_t *vehicle_challenge)
{
    if (session == NULL || vehicle_public_key == NULL || vehicle_challenge == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 复制车辆公钥 */
    memcpy(session->vehicle_public_key, vehicle_public_key, 65);
    
    /* 生成新的临时密钥对用于会话 */
    error_t ret = g_se_interface.generate_key_pair(
        session->ephemeral_public_key,
        session->ephemeral_private_key
    );
    if (ret != OK) {
        return ret;
    }
    
    /* 计算共享秘钥 */
    ret = g_se_interface.derive_shared_secret(
        session->ephemeral_private_key,
        session->vehicle_public_key,
        session->shared_secret
    );
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }
    
    /* 派生会话密钥 */
    ret = ccc_derive_session_keys(
        session->shared_secret,
        (uint8_t *)CCC_SESSION_KEY_LABEL,
        session->session_key_enc,
        session->session_key_mac
    );
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }
    
    /* 初始化计数器 */
    session->message_counter = 0;
    session->session_nonce = 0;
    
    session->state = CCC_SESSION_STATE_ACTIVE;
    session->is_secure_session = true;
    session->last_activity = ccc_get_timestamp();
    
    return OK;
}

/******************************************************************************
 * 加密消息 (AES-128-GCM)
 ******************************************************************************/
error_t ccc_encrypt_message(
    ccc_session_context_t *session,
    ccc_command_t command,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t *encrypted_data,
    size_t *encrypted_len)
{
    if (session == NULL || payload == NULL || encrypted_data == NULL || encrypted_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (!session->is_secure_session) {
        return ERROR_NOT_INITIALIZED;
    }
    
    /* 消息格式: [Counter(4)][Cmd(1)][PayloadLen(2)][Payload(n)][Tag(16)] */
    size_t offset = 0;
    
    /* 计数器 */
    encrypted_data[offset++] = (session->message_counter >> 24) & 0xFF;
    encrypted_data[offset++] = (session->message_counter >> 16) & 0xFF;
    encrypted_data[offset++] = (session->message_counter >> 8) & 0xFF;
    encrypted_data[offset++] = session->message_counter & 0xFF;
    
    /* 命令 */
    encrypted_data[offset++] = command;
    
    /* 载荷长度 */
    encrypted_data[offset++] = (payload_len >> 8) & 0xFF;
    encrypted_data[offset++] = payload_len & 0xFF;
    
    /* TODO: AES-128-GCM 加密载荷 */
    memcpy(&encrypted_data[offset], payload, payload_len);
    offset += payload_len;
    
    /* MAC/Tag (占位) */
    memset(&encrypted_data[offset], 0, 16);
    offset += 16;
    
    session->message_counter++;
    *encrypted_len = offset;
    
    return OK;
}

/******************************************************************************
 * 解密消息
 ******************************************************************************/
error_t ccc_decrypt_message(
    ccc_session_context_t *session,
    const uint8_t *encrypted_data,
    size_t encrypted_len,
    ccc_command_t *command,
    uint8_t *payload,
    size_t *payload_len)
{
    if (session == NULL || encrypted_data == NULL || command == NULL || 
        payload == NULL || payload_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (encrypted_len < 8) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 解析消息 */
    size_t offset = 0;
    
    /* 跳过计数器 */
    offset += 4;
    
    /* 命令 */
    *command = (ccc_command_t)encrypted_data[offset++];
    
    /* 载荷长度 */
    size_t len = ((uint16_t)encrypted_data[offset] << 8) | encrypted_data[offset + 1];
    offset += 2;
    
    if (encrypted_len < (offset + len + 16)) {
        return ERROR_INVALID_PARAM;
    }
    
    /* TODO: AES-128-GCM 解密和验证 */
    memcpy(payload, &encrypted_data[offset], len);
    *payload_len = len;
    
    return OK;
}

/******************************************************************************
 * 发送车辆控制命令
 ******************************************************************************/
error_t ccc_send_vehicle_command(
    ccc_session_context_t *session,
    ccc_command_t command,
    const uint8_t *params,
    size_t params_len,
    uint8_t *response,
    size_t *response_len)
{
    if (session == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (session->state != CCC_SESSION_STATE_ACTIVE) {
        return ERROR_NOT_INITIALIZED;
    }
    
    /* 检查超时 */
    uint32_t now = ccc_get_timestamp();
    if ((now - session->last_activity) > session->session_timeout_ms) {
        session->state = CCC_SESSION_STATE_TERMINATING;
        return ERROR_TIMEOUT;
    }
    
    /* 加密命令 */
    uint8_t encrypted[512];
    size_t encrypted_len;
    error_t ret = ccc_encrypt_message(session, command, params, params_len,
                                            encrypted, &encrypted_len);
    if (ret != OK) {
        return ret;
    }
    
    /* TODO: 发送到传输层 */
    
    session->last_activity = ccc_get_timestamp();
    return OK;
}

/******************************************************************************
 * 获取车辆状态
 ******************************************************************************/
error_t ccc_get_vehicle_status(
    ccc_session_context_t *session,
    ccc_vehicle_status_t *status)
{
    if (session == NULL || status == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    uint8_t response[64];
    size_t response_len = sizeof(response);
    
    error_t ret = ccc_send_vehicle_command(
        session,
        CCC_CMD_STATUS_REQUEST,
        NULL,
        0,
        response,
        &response_len
    );
    
    if (ret != OK) {
        return ret;
    }
    
    /* 解析状态响应 */
    if (response_len >= sizeof(ccc_vehicle_status_t)) {
        memcpy(status, response, sizeof(ccc_vehicle_status_t));
    }
    
    return OK;
}

/******************************************************************************
 * 派生会话密钥 (HKDF-SHA256)
 ******************************************************************************/
error_t ccc_derive_session_keys(
    const uint8_t *shared_secret,
    const uint8_t *salt,
    uint8_t *enc_key,
    uint8_t *mac_key)
{
    if (shared_secret == NULL || salt == NULL || enc_key == NULL || mac_key == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    uint8_t prk[32];
    error_t ret = ccc_hkdf_extract(shared_secret, 32, salt, strlen((char *)salt), prk);
    if (ret != OK) {
        return ret;
    }
    
    uint8_t okm[48];
    ret = ccc_hkdf_expand(prk, 32, (uint8_t *)"encryption", 10, okm, 48);
    if (ret != OK) {
        return ret;
    }
    
    memcpy(enc_key, okm, 16);
    memcpy(mac_key, okm + 16, 16);
    
    return OK;
}

/******************************************************************************
 * 生成挑战
 ******************************************************************************/
error_t ccc_generate_challenge(uint8_t *challenge)
{
    if (challenge == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    return ccc_generate_nonce(challenge, CCC_CHALLENGE_LEN);
}

/******************************************************************************
 * 验证挑战响应
 ******************************************************************************/
error_t ccc_verify_challenge_response(
    const uint8_t *challenge,
    const uint8_t *response,
    const uint8_t *public_key)
{
    if (challenge == NULL || response == NULL || public_key == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    bool valid = g_se_interface.verify(challenge, CCC_CHALLENGE_LEN, public_key, response);
    if (!valid) {
        return CCC_ERROR_SIGNATURE_INVALID;
    }
    
    return OK;
}

/******************************************************************************
 * 销毁会话
 ******************************************************************************/
void ccc_destroy_session(ccc_session_context_t *session)
{
    if (session == NULL) {
        return;
    }
    
    /* 清除敏感数据 */
    memset(session->shared_secret, 0, sizeof(session->shared_secret));
    memset(session->master_secret, 0, sizeof(session->master_secret));
    memset(session->session_key_enc, 0, sizeof(session->session_key_enc));
    memset(session->session_key_mac, 0, sizeof(session->session_key_mac));
    memset(session->ephemeral_private_key, 0, sizeof(session->ephemeral_private_key));
    
    free(session);
}

/******************************************************************************
 * 内部辅助函数实现
 ******************************************************************************/

/* 简化的 HKDF-Extract */
static error_t ccc_hkdf_extract(
    const uint8_t *ikm, size_t ikm_len,
    const uint8_t *salt, size_t salt_len,
    uint8_t *prk)
{
    /* 使用 HMAC-SHA256 实现 */
    /* 这是简化版本，实际使用需要完整的 HMAC 实现 */
    memcpy(prk, ikm, 32);
    (void)salt;
    (void)salt_len;
    (void)ikm_len;
    return OK;
}

/* 简化的 HKDF-Expand */
static error_t ccc_hkdf_expand(
    const uint8_t *prk, size_t prk_len,
    const uint8_t *info, size_t info_len,
    uint8_t *okm, size_t okm_len)
{
    size_t n = (okm_len + 31) / 32;
    uint8_t t[32];
    size_t offset = 0;
    
    for (size_t i = 1; i <= n; i++) {
        /* T(i) = HMAC(PRK, T(i-1) | info | i) */
        /* 简化版本 */
        for (size_t j = 0; j < 32 && offset < okm_len; j++) {
            okm[offset++] = prk[j] ^ info[j % info_len] ^ (uint8_t)i;
        }
    }
    
    (void)prk_len;
    (void)t;
    return OK;
}

/* 生成随机数 */
static error_t ccc_generate_nonce(uint8_t *nonce, size_t len)
{
    /* 使用简化的伪随机生成，实际使用需要真随机数生成器 */
    static uint32_t seed = 1;
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        nonce[i] = (uint8_t)(seed >> 16);
    }
    return OK;
}

/* 获取时间戳 */
static uint32_t ccc_get_timestamp(void)
{
    /* 返回毫秒级时间戳 */
    static uint32_t counter = 0;
    return ++counter * 1000;
}
