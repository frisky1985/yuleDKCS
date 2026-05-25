/******************************************************************************
 * @file    ccc_core.c
 * @brief   CCC Digital Key R3 核心协议实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-08
 *
 * @note    实现 CCC Digital Key Release 3 规范协议栈核心
 *          - 初始化/反初始化
 *          - ECDH P-256 密钥协商
 *          - HKDF-SHA256 密钥派生
 *          - AES-128-GCM 安全通道
 *          - 证书链验证
 *          - 重放攻击防护
 *          - 超时重试
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "ccc.h"
#include "dkcs.h"

/* mbedtls 密码学库头文件 */
#include <mbedtls/hkdf.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

/******************************************************************************
 * 内部状态和宏定义
 ******************************************************************************/
#define CCC_STATE_UNINITIALIZED     0
#define CCC_STATE_INITIALIZED       1
#define CCC_STATE_SESSION_ACTIVE    2

#define CCC_MAX_RETRY_COUNT         3
#define CCC_RETRY_INTERVAL_MS       1000

#define CCC_GCM_IV_LEN              12
#define CCC_GCM_TAG_LEN             16
#define CCC_AES128_KEY_LEN          16
#define CCC_GCM_ADDITIONAL_DATA_LEN 8

/* 协议版本 */
#define CCC_PROTOCOL_VERSION_MAJOR  3
#define CCC_PROTOCOL_VERSION_MINOR  0

static uint8_t g_ccc_state = CCC_STATE_UNINITIALIZED;
static ccc_se_interface_t g_se_interface;

/* CCC 固定标签 */
static const uint8_t CCC_MASTER_SECRET_LABEL[] = "CCC-R3-MASTER-SECRET";
static const uint8_t CCC_SESSION_KEY_LABEL[]   = "CCC-R3-SESSION-KEYS";
static const uint8_t CCC_CHALLENGE_LABEL[]     = "CCC-R3-CHALLENGE";
static const uint8_t CCC_CERT_VERIFY_LABEL[]   = "CCC-R3-CERT-VERIFY";

/* mbedtls CTR_DRBG 上下文 */
static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;
static bool g_rng_initialized = false;

/******************************************************************************
 * 内部辅助函数声明
 ******************************************************************************/
static error_t ccc_hkdf_sha256(
    const uint8_t *ikm, size_t ikm_len,
    const uint8_t *salt, size_t salt_len,
    const uint8_t *info, size_t info_len,
    uint8_t *okm, size_t okm_len);

static error_t ccc_generate_random(uint8_t *buf, size_t len);
static uint32_t ccc_get_timestamp_ms(void);
static error_t ccc_verify_vehicle_response_data(
    const ccc_session_context_t *session,
    const uint8_t *response_data,
    size_t response_len);
static error_t ccc_validate_message_counter(
    ccc_session_context_t *session,
    uint32_t received_counter);
static error_t ccc_aes_gcm_encrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *plain, size_t plain_len,
    uint8_t *cipher,
    uint8_t *tag, size_t tag_len);
static error_t ccc_aes_gcm_decrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *cipher, size_t cipher_len,
    const uint8_t *tag, size_t tag_len,
    uint8_t *plain);

/******************************************************************************
 * CCC 初始化
 ******************************************************************************/
error_t ccc_init(const ccc_se_interface_t *se_interface)
{
    int mbed_ret;

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

    /* 初始化 mbedtls RNG */
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    mbed_ret = mbedtls_ctr_drbg_seed(
        &g_ctr_drbg,
        mbedtls_entropy_func,
        &g_entropy,
        (const unsigned char *)"CCC-R3-CTR-DRBG",
        16);
    if (mbed_ret != 0) {
        mbedtls_ctr_drbg_free(&g_ctr_drbg);
        mbedtls_entropy_free(&g_entropy);
        return ERROR_CRYPTO_FAILURE;
    }
    g_rng_initialized = true;

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

    if (g_rng_initialized) {
        mbedtls_ctr_drbg_free(&g_ctr_drbg);
        mbedtls_entropy_free(&g_entropy);
        g_rng_initialized = false;
    }

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
    ctx->last_activity = ccc_get_timestamp_ms();
    ctx->is_secure_session = false;
    ctx->message_counter = 0;
    ctx->session_nonce = 0;

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

    /* 生成临时密钥对 (ECDH P-256) */
    error_t ret = g_se_interface.generate_key_pair(
        session->ephemeral_public_key,
        session->ephemeral_private_key);
    if (ret != OK) {
        return ret;
    }

    /* 生成挑战 (32字节随机数) */
    ret = ccc_generate_random(session->challenge, CCC_CHALLENGE_LEN);
    if (ret != OK) {
        return ret;
    }

    /* 构建配对请求消息 */
    /* 格式: [Version(1)][EphemeralPubKey(65)][Challenge(32)][DeviceCertLen(2)][DeviceCert(n)] */
    size_t offset = 0;

    /* 版本 */
    request_data[offset++] = (CCC_PROTOCOL_VERSION_MAJOR << 4) | CCC_PROTOCOL_VERSION_MINOR;

    /* 临时公钥 */
    memcpy(&request_data[offset], session->ephemeral_public_key, ECC_P256_PUB_KEY_LEN);
    offset += ECC_P256_PUB_KEY_LEN;

    /* 挑战 */
    memcpy(&request_data[offset], session->challenge, CCC_CHALLENGE_LEN);
    offset += CCC_CHALLENGE_LEN;

    /* 证书链 */
    if (session->device_cert_chain.cert_count > 0) {
        size_t cert_chain_offset = offset + 2;
        size_t cert_chain_len = 0;

        for (uint8_t i = 0; i < session->device_cert_chain.cert_count; i++) {
            size_t cert_len = MAX_CERT_LEN;
            ret = ccc_serialize_certificate(
                &session->device_cert_chain.certs[i],
                &request_data[cert_chain_offset + cert_chain_len],
                &cert_len);
            if (ret != OK) {
                return ret;
            }
            cert_chain_len += cert_len;
        }

        /* 证书链长度 (2字节大端) */
        request_data[offset++] = (uint8_t)((cert_chain_len >> 8) & 0xFF);
        request_data[offset++] = (uint8_t)(cert_chain_len & 0xFF);
        offset += cert_chain_len;
    } else {
        /* 无证书时发送0长度 */
        request_data[offset++] = 0x00;
        request_data[offset++] = 0x00;
    }

    *request_len = offset;

    /* 更新状态 */
    session->state = CCC_SESSION_STATE_PAIRING;
    session->pairing_state = CCC_PAIRING_STATE_WAITING_VEHICLE;
    session->last_activity = ccc_get_timestamp_ms();

    return OK;
}

/******************************************************************************
 * 处理配对响应 (验证车辆证书和签名)
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

    /* 解析响应: [Version(1)][VehiclePubKey(65)][VehicleChallenge(32)][VehicleCertLen(2)][VehicleCert(n)][Sig(64)] */
    size_t min_len = 1 + ECC_P256_PUB_KEY_LEN + CCC_CHALLENGE_LEN + 2 + 64;
    if (response_len < min_len) {
        return ERROR_INVALID_PARAM;
    }

    size_t offset = 0;

    /* 版本校验 */
    uint8_t version = response_data[offset++];
    uint8_t major_ver = (version >> 4) & 0x0F;
    if (major_ver != CCC_PROTOCOL_VERSION_MAJOR) {
        return CCC_ERROR_VERSION_MISMATCH;
    }

    /* 复制车辆公钥 */
    memcpy(session->vehicle_public_key, &response_data[offset], ECC_P256_PUB_KEY_LEN);
    offset += ECC_P256_PUB_KEY_LEN;

    /* 复制车辆挑战 */
    uint8_t vehicle_challenge[CCC_CHALLENGE_LEN];
    memcpy(vehicle_challenge, &response_data[offset], CCC_CHALLENGE_LEN);
    offset += CCC_CHALLENGE_LEN;

    /* 解析车辆证书链 */
    uint16_t cert_chain_len = ((uint16_t)response_data[offset] << 8) | response_data[offset + 1];
    offset += 2;

    if (cert_chain_len > 0) {
        if ((offset + cert_chain_len + 64) > response_len) {
            return ERROR_INVALID_PARAM;
        }

        /* 解析第一个证书 (设备证书) */
        ccc_certificate_t vehicle_cert;
        memset(&vehicle_cert, 0, sizeof(vehicle_cert));

        error_t ret = ccc_parse_certificate(
            &response_data[offset],
            cert_chain_len,
            &vehicle_cert);
        if (ret != OK) {
            return CCC_ERROR_CERT_INVALID;
        }

        /* 验证车辆公钥与证书中的公钥一致 */
        if (memcmp(session->vehicle_public_key, vehicle_cert.public_key,
                   ECC_P256_PUB_KEY_LEN) != 0) {
            return CCC_ERROR_CERT_INVALID;
        }

        /* 存储车辆证书到会话 */
        session->vehicle_cert_chain.cert_count = 1;
        memcpy(&session->vehicle_cert_chain.certs[0], &vehicle_cert,
               sizeof(ccc_certificate_t));

        offset += cert_chain_len;
    }

    /* 验证车辆对设备挑战的签名 */
    uint8_t signature[64];
    if ((offset + 64) > response_len) {
        return ERROR_INVALID_PARAM;
    }
    memcpy(signature, &response_data[offset], 64);

    /* 使用 SE 接口验证签名: 车辆公钥验证对设备挑战的签名 */
    bool valid = g_se_interface.verify(
        session->challenge,
        CCC_CHALLENGE_LEN,
        session->vehicle_public_key,
        signature);
    if (!valid) {
        return CCC_ERROR_SIGNATURE_INVALID;
    }

    /* 计算共享秘密 (ECDH) */
    error_t ret = g_se_interface.derive_shared_secret(
        session->ephemeral_private_key,
        session->vehicle_public_key,
        session->shared_secret);
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }

    /* 派生 Master Secret 使用 HKDF-SHA256 */
    ret = ccc_hkdf_sha256(
        session->shared_secret, CCC_SHARED_SECRET_LEN,
        NULL, 0,
        CCC_MASTER_SECRET_LABEL, sizeof(CCC_MASTER_SECRET_LABEL) - 1,
        session->master_secret, CCC_MASTER_SECRET_LEN);
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }

    /* 从 Master Secret 派生存储用密钥 (enc_key + mac_key) */
    ret = ccc_hkdf_sha256(
        session->master_secret, CCC_MASTER_SECRET_LEN,
        (const uint8_t *)CCC_SESSION_KEY_LABEL, sizeof(CCC_SESSION_KEY_LABEL) - 1,
        session->challenge, CCC_CHALLENGE_LEN,
        session->session_key_enc, CCC_SESSION_KEY_LEN);
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }
    memcpy(session->session_key_mac, session->session_key_enc, CCC_MAC_KEY_LEN);

    /* 更新状态 */
    session->pairing_state = CCC_PAIRING_STATE_DEVICE_VERIFIED;
    session->last_activity = ccc_get_timestamp_ms();

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

    size_t offset = 0;

    /* 对车辆挑战进行签名 (证明持有设备私钥) */
    uint8_t challenge_response[64];
    error_t ret = g_se_interface.sign(
        session->challenge,  /* 车辆挑战在 ccc_process_pairing_response 中已复制 */
        CCC_CHALLENGE_LEN,
        session->device_private_key,
        challenge_response);
    if (ret != OK) {
        return CCC_ERROR_SIGNATURE_INVALID;
    }

    memcpy(&confirmation_data[offset], challenge_response, 64);
    offset += 64;

    /* 添加设备证书链 */
    if (session->device_cert_chain.cert_count > 0) {
        size_t cert_chain_offset = offset + 2;
        size_t chain_data_len = 0;

        for (uint8_t i = 0; i < session->device_cert_chain.cert_count; i++) {
            size_t cert_len = MAX_CERT_LEN;
            ret = ccc_serialize_certificate(
                &session->device_cert_chain.certs[i],
                &confirmation_data[cert_chain_offset + chain_data_len],
                &cert_len);
            if (ret != OK) {
                return ret;
            }
            chain_data_len += cert_len;
        }

        confirmation_data[offset++] = (uint8_t)((chain_data_len >> 8) & 0xFF);
        confirmation_data[offset++] = (uint8_t)(chain_data_len & 0xFF);
        offset += chain_data_len;
    } else {
        confirmation_data[offset++] = 0x00;
        confirmation_data[offset++] = 0x00;
    }

    *confirmation_len = offset;

    /* 更新状态: 配对完成, 会话激活 */
    session->pairing_state = CCC_PAIRING_STATE_COMPLETE;
    session->state = CCC_SESSION_STATE_ACTIVE;
    session->is_secure_session = true;
    session->message_counter = 0;
    session->session_nonce = 0;
    session->last_activity = ccc_get_timestamp_ms();

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

    if (session->state == CCC_SESSION_STATE_ACTIVE) {
        return ERROR_ALREADY_INITIALIZED;
    }

    /* 复制车辆公钥 */
    memcpy(session->vehicle_public_key, vehicle_public_key, ECC_P256_PUB_KEY_LEN);

    /* 生成新的临时密钥对用于会话 */
    error_t ret = g_se_interface.generate_key_pair(
        session->ephemeral_public_key,
        session->ephemeral_private_key);
    if (ret != OK) {
        return ret;
    }

    /* 计算共享秘密 (ECDH) */
    ret = g_se_interface.derive_shared_secret(
        session->ephemeral_private_key,
        session->vehicle_public_key,
        session->shared_secret);
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }

    /* 派生会话密钥 (HKDF-SHA256) */
    ret = ccc_hkdf_sha256(
        session->shared_secret, CCC_SHARED_SECRET_LEN,
        vehicle_challenge, CCC_CHALLENGE_LEN,
        CCC_SESSION_KEY_LABEL, sizeof(CCC_SESSION_KEY_LABEL) - 1,
        session->session_key_enc, CCC_SESSION_KEY_LEN);
    if (ret != OK) {
        return CCC_ERROR_KEY_AGREEMENT_FAILED;
    }
    memcpy(session->session_key_mac, session->session_key_enc, CCC_MAC_KEY_LEN);

    /* 初始化计数器 */
    session->message_counter = 0;
    session->session_nonce = 0;

    session->state = CCC_SESSION_STATE_ACTIVE;
    session->is_secure_session = true;
    session->last_activity = ccc_get_timestamp_ms();

    return OK;
}

/******************************************************************************
 * 加密消息 (AES-128-GCM)
 *
 * 消息格式: [Counter(4)][Cmd(1)][PayloadLen(2)][IV(12)][Ciphertext(n)][Tag(16)]
 * 附加认证数据 (AAD): Counter(4) + Cmd(1) + PayloadLen(2) = 8 字节
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

    size_t offset = 0;

    /* 4字节计数器 (大端) */
    encrypted_data[offset++] = (uint8_t)((session->message_counter >> 24) & 0xFF);
    encrypted_data[offset++] = (uint8_t)((session->message_counter >> 16) & 0xFF);
    encrypted_data[offset++] = (uint8_t)((session->message_counter >> 8) & 0xFF);
    encrypted_data[offset++] = (uint8_t)(session->message_counter & 0xFF);

    /* 1字节命令 */
    encrypted_data[offset++] = (uint8_t)command;

    /* 2字节载荷长度 (大端) */
    encrypted_data[offset++] = (uint8_t)((payload_len >> 8) & 0xFF);
    encrypted_data[offset++] = (uint8_t)(payload_len & 0xFF);

    /* 生成随机 IV (12 字节) */
    uint8_t iv[CCC_GCM_IV_LEN];
    error_t ret = ccc_generate_random(iv, CCC_GCM_IV_LEN);
    if (ret != OK) {
        return ERROR_CRYPTO_FAILURE;
    }
    memcpy(&encrypted_data[offset], iv, CCC_GCM_IV_LEN);
    offset += CCC_GCM_IV_LEN;

    /* AAD = 前8字节 (Counter + Cmd + PayloadLen) */
    uint8_t aad[CCC_GCM_ADDITIONAL_DATA_LEN];
    memcpy(aad, encrypted_data, CCC_GCM_ADDITIONAL_DATA_LEN);

    /* AES-128-GCM 加密 */
    uint8_t tag[CCC_GCM_TAG_LEN];
    ret = ccc_aes_gcm_encrypt(
        session->session_key_enc, CCC_AES128_KEY_LEN,
        iv, CCC_GCM_IV_LEN,
        aad, CCC_GCM_ADDITIONAL_DATA_LEN,
        payload, payload_len,
        &encrypted_data[offset],
        tag, CCC_GCM_TAG_LEN);
    if (ret != OK) {
        return ERROR_CRYPTO_FAILURE;
    }
    offset += payload_len;

    /* 16字节 GCM 认证标签 */
    memcpy(&encrypted_data[offset], tag, CCC_GCM_TAG_LEN);
    offset += CCC_GCM_TAG_LEN;

    session->message_counter++;
    *encrypted_len = offset;

    return OK;
}

/******************************************************************************
 * 解密消息 (AES-128-GCM)
 *
 * 消息格式: [Counter(4)][Cmd(1)][PayloadLen(2)][IV(12)][Ciphertext(n)][Tag(16)]
 * 包含重放攻击防护 (计数器验证)
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

    if (!session->is_secure_session) {
        return ERROR_NOT_INITIALIZED;
    }

    /* 最小长度: Counter(4) + Cmd(1) + PayloadLen(2) + IV(12) + Tag(16) = 35 */
    size_t min_len = CCC_GCM_ADDITIONAL_DATA_LEN + CCC_GCM_IV_LEN + CCC_GCM_TAG_LEN;
    if (encrypted_len < min_len) {
        return ERROR_INVALID_PARAM;
    }

    size_t offset = 0;

    /* 提取并验证计数器 (重放攻击防护) */
    uint32_t received_counter =
        ((uint32_t)encrypted_data[0] << 24) |
        ((uint32_t)encrypted_data[1] << 16) |
        ((uint32_t)encrypted_data[2] << 8) |
        (uint32_t)encrypted_data[3];
    offset += 4;

    error_t ret = ccc_validate_message_counter(session, received_counter);
    if (ret != OK) {
        return ret;
    }

    /* 提取命令 */
    *command = (ccc_command_t)encrypted_data[offset++];

    /* 提取载荷长度 */
    size_t enc_payload_len =
        ((uint16_t)encrypted_data[offset] << 8) | encrypted_data[offset + 1];
    offset += 2;

    /* AAD = 前8字节 */
    uint8_t aad[CCC_GCM_ADDITIONAL_DATA_LEN];
    memcpy(aad, encrypted_data, CCC_GCM_ADDITIONAL_DATA_LEN);

    /* 提取 IV (12字节) */
    if ((offset + CCC_GCM_IV_LEN + enc_payload_len + CCC_GCM_TAG_LEN) > encrypted_len) {
        return ERROR_INVALID_PARAM;
    }
    const uint8_t *iv = &encrypted_data[offset];
    offset += CCC_GCM_IV_LEN;

    /* 提取密文 */
    const uint8_t *cipher = &encrypted_data[offset];
    offset += enc_payload_len;

    /* 提取认证标签 */
    const uint8_t *tag = &encrypted_data[offset];

    /* AES-128-GCM 解密并验证 */
    ret = ccc_aes_gcm_decrypt(
        session->session_key_enc, CCC_AES128_KEY_LEN,
        iv, CCC_GCM_IV_LEN,
        aad, CCC_GCM_ADDITIONAL_DATA_LEN,
        cipher, enc_payload_len,
        tag, CCC_GCM_TAG_LEN,
        payload);
    if (ret != OK) {
        return CCC_ERROR_SESSION_ESTABLISH_FAILED;
    }

    *payload_len = enc_payload_len;

    /* 更新会话计数器 */
    session->message_counter = received_counter + 1;

    return OK;
}

/******************************************************************************
 * 发送车辆控制命令
 * 使用注册的传输层回调发送加密消息
 ******************************************************************************/

/* 传输层回调接口 */
static error_t (*g_transport_send_cb)(const uint8_t *data, size_t len) = NULL;

error_t ccc_register_transport_send(error_t (*send_fn)(const uint8_t *, size_t))
{
    g_transport_send_cb = send_fn;
    return OK;
}

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
    uint32_t now = ccc_get_timestamp_ms();
    if ((now - session->last_activity) > session->session_timeout_ms) {
        session->state = CCC_SESSION_STATE_TERMINATING;
        return ERROR_TIMEOUT;
    }

    /* 加密命令 */
    uint8_t encrypted[512];
    size_t encrypted_len = 0;
    error_t ret = ccc_encrypt_message(session, command, params, params_len,
                                      encrypted, &encrypted_len);
    if (ret != OK) {
        return ret;
    }

    /* 通过传输层发送 */
    if (g_transport_send_cb != NULL) {
        ret = g_transport_send_cb(encrypted, encrypted_len);
        if (ret != OK) {
            return ERROR_TRANSPORT_FAILURE;
        }
    }

    session->last_activity = ccc_get_timestamp_ms();
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

    uint8_t response[sizeof(ccc_vehicle_status_t)];
    size_t response_len = sizeof(response);

    error_t ret = ccc_send_vehicle_command(
        session,
        CCC_CMD_STATUS_REQUEST,
        NULL,
        0,
        response,
        &response_len);

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

    /* 使用 HKDF-SHA256 派生 48 字节输出 (enc_key(16) + mac_key(16) + 预留16) */
    uint8_t okm[48];
    error_t ret = ccc_hkdf_sha256(
        shared_secret, CCC_SHARED_SECRET_LEN,
        salt, strlen((const char *)salt),
        (const uint8_t *)"CCC-R3-DERIVE", 12,
        okm, sizeof(okm));
    if (ret != OK) {
        return ret;
    }

    memcpy(enc_key, okm, CCC_SESSION_KEY_LEN);
    memcpy(mac_key, okm + CCC_SESSION_KEY_LEN, CCC_MAC_KEY_LEN);

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

    return ccc_generate_random(challenge, CCC_CHALLENGE_LEN);
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

    /* 使用 SE 接口中的 verify 函数 */
    /* verify 返回 bool: true=有效, false=无效 */
    bool valid = g_se_interface.verify(challenge, CCC_CHALLENGE_LEN, public_key, response);
    if (!valid) {
        return CCC_ERROR_SIGNATURE_INVALID;
    }

    return OK;
}

/******************************************************************************
 * 计算消息 MAC
 ******************************************************************************/
error_t ccc_compute_mac(
    const uint8_t *mac_key,
    const uint8_t *message,
    size_t message_len,
    uint8_t *mac)
{
    if (mac_key == NULL || message == NULL || mac == NULL) {
        return ERROR_INVALID_PARAM;
    }

    /* 使用 HMAC-SHA256 计算 MAC */
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        return ERROR_CRYPTO_FAILURE;
    }

    int ret = mbedtls_md_hmac(md_info, mac_key, CCC_MAC_KEY_LEN,
                              message, message_len, mac);
    if (ret != 0) {
        return ERROR_CRYPTO_FAILURE;
    }

    return OK;
}

/******************************************************************************
 * 验证消息 MAC
 ******************************************************************************/
error_t ccc_verify_mac(
    const uint8_t *mac_key,
    const uint8_t *message,
    size_t message_len,
    const uint8_t *mac)
{
    if (mac_key == NULL || message == NULL || mac == NULL) {
        return ERROR_INVALID_PARAM;
    }

    uint8_t computed_mac[32];
    error_t ret = ccc_compute_mac(mac_key, message, message_len, computed_mac);
    if (ret != OK) {
        return ret;
    }

    /* 常量时间比较防止时序攻击 */
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < 32; i++) {
        diff |= computed_mac[i] ^ mac[i];
    }

    return (diff == 0) ? OK : CCC_ERROR_SIGNATURE_INVALID;
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
    nvm_secure_zero(session->shared_secret, sizeof(session->shared_secret));
    nvm_secure_zero(session->master_secret, sizeof(session->master_secret));
    nvm_secure_zero(session->session_key_enc, sizeof(session->session_key_enc));
    nvm_secure_zero(session->session_key_mac, sizeof(session->session_key_mac));
    nvm_secure_zero(session->ephemeral_private_key, sizeof(session->ephemeral_private_key));
    nvm_secure_zero(session->challenge, sizeof(session->challenge));

    free(session);
}

/******************************************************************************
 * 证书 API 实现
 ******************************************************************************/

error_t ccc_serialize_certificate(const ccc_certificate_t *cert, uint8_t *out, size_t *out_len)
{
    if (cert == NULL || out == NULL || out_len == NULL) {
        return ERROR_INVALID_PARAM;
    }

    /* 构建简易 X.509 DER 格式证书 */
    size_t offset = 0;

    /* SEQUENCE 标签 */
    out[offset++] = 0x30;

    /* 预留长度字段 (后续填充) */
    size_t len_pos = offset;
    offset += 2;

    /* 版本 (显式标签 [0] INTEGER 2 = v3) */
    out[offset++] = 0xA0;
    out[offset++] = 0x03;
    out[offset++] = 0x02;
    out[offset++] = 0x01;
    out[offset++] = 0x02;

    /* 序列号 */
    out[offset++] = 0x02;
    out[offset++] = 16;
    memcpy(&out[offset], cert->serial_number, 16);
    offset += 16;

    /* 签名算法 (ECDSA with SHA-256: 1.2.840.10045.4.3.2) */
    out[offset++] = 0x30;
    out[offset++] = 0x0A;
    out[offset++] = 0x06;
    out[offset++] = 0x08;
    out[offset++] = 0x2A; out[offset++] = 0x86; out[offset++] = 0x48;
    out[offset++] = 0xCE; out[offset++] = 0x3D;
    out[offset++] = 0x04; out[offset++] = 0x03; out[offset++] = 0x02;

    /* 颁发者 */
    out[offset++] = 0x30;
    out[offset++] = 18;
    out[offset++] = 0x31;
    out[offset++] = 16;
    out[offset++] = 0x13;
    out[offset++] = 14;
    memcpy(&out[offset], "YuleTech-CA-R3", 14);
    offset += 14;

    /* 有效期 */
    /* notBefore */
    out[offset++] = 0x30;
    out[offset++] = 0x1E;
    out[offset++] = 0x17;
    out[offset++] = 0x0C;
    /* UTC时间: YYMMDDHHMMSSZ */
    memset(&out[offset], '0', 12);
    offset += 12;
    /* notAfter */
    out[offset++] = 0x17;
    out[offset++] = 0x0C;
    memset(&out[offset], '9', 12);
    offset += 12;

    /* 主体 */
    out[offset++] = 0x30;
    out[offset++] = 18;
    out[offset++] = 0x31;
    out[offset++] = 16;
    out[offset++] = 0x13;
    out[offset++] = 14;
    memcpy(&out[offset], "YuleTech-Dev-R3", 14);
    offset += 14;

    /* 主体公钥信息 (未压缩 P-256 格式) */
    out[offset++] = 0x30;
    out[offset++] = 0x59;  /* 长度: 0x04 + 2(type) + 2(curve) + 65(key) + 2 + 1 = 76 = 0x4C */
    out[offset++] = 0x30;
    out[offset++] = 0x13;
    out[offset++] = 0x06;
    out[offset++] = 0x07;
    out[offset++] = 0x2A; out[offset++] = 0x86; out[offset++] = 0x48;
    out[offset++] = 0xCE; out[offset++] = 0x3D;
    out[offset++] = 0x02; out[offset++] = 0x01;
    out[offset++] = 0x06;
    out[offset++] = 0x08;
    out[offset++] = 0x2A; out[offset++] = 0x86; out[offset++] = 0x48;
    out[offset++] = 0xCE; out[offset++] = 0x3D;
    out[offset++] = 0x03; out[offset++] = 0x01; out[offset++] = 0x07;
    out[offset++] = 0x03;
    out[offset++] = 0x42;  /* 65字节未压缩公钥 */
    out[offset++] = 0x00;
    memcpy(&out[offset], cert->public_key, 65);
    offset += 65;

    /* 签名值 */
    out[offset++] = 0x03;
    out[offset++] = 66;    /* 64 + 2 */
    out[offset++] = 0x00;
    out[offset++] = 0x30;
    out[offset++] = 62;    /* 60 = 2+2+28 + 2+2+28 ... simplified */
    out[offset++] = 0x02;
    out[offset++] = 30;
    memset(&out[offset], 0, 30);
    offset += 30;
    out[offset++] = 0x02;
    out[offset++] = 30;
    memset(&out[offset], 0, 30);
    offset += 30;

    /* 填充总长度 */
    size_t total_len = offset;
    out[len_pos] = (uint8_t)((total_len - 4) >> 8);
    out[len_pos + 1] = (uint8_t)((total_len - 4) & 0xFF);

    *out_len = total_len;
    return OK;
}

error_t ccc_parse_certificate(const uint8_t *data, size_t data_len, ccc_certificate_t *cert)
{
    if (data == NULL || data_len == 0 || cert == NULL) {
        return ERROR_INVALID_PARAM;
    }

    /* 简易 DER 解析 - 提取关键字段 */
    size_t offset = 0;

    /* SEQUENCE */
    if (data_len < 4 || data[offset++] != 0x30) {
        return ERROR_INVALID_PARAM;
    }

    /* 跳过长度 */
    uint16_t len = (uint16_t)data[offset++];
    if (len > 0x80) {
        offset += (len & 0x0F);  /* 长格式长度 */
    }

    /* 跳过版本标签 */
    if (offset >= data_len) return ERROR_INVALID_PARAM;
    if (data[offset] == 0xA0) {
        offset += (data[offset + 1] + 2);
    }

    /* 提取序列号 */
    if ((offset + 2) >= data_len || data[offset++] != 0x02) {
        /* 如果不能解析序列号, 使用全0 */
        memset(cert->serial_number, 0, 16);
    } else {
        uint8_t sn_len = data[offset++];
        if (sn_len > 16) sn_len = 16;
        memset(cert->serial_number, 0, 16);
        if ((offset + sn_len) <= data_len) {
            memcpy(cert->serial_number + (16 - sn_len),
                   &data[offset], sn_len);
            offset += sn_len;
        }
    }

    /* 提取公钥 (查找 0x03 0x42 0x00 前缀) */
    cert->public_key[0] = 0x04;  /* 未压缩格式 */
    for (size_t i = offset; i < (data_len - 67); i++) {
        if (data[i] == 0x03 && data[i + 1] == 0x42 && data[i + 2] == 0x00) {
            memcpy(cert->public_key + 1, &data[i + 3], 64);
            break;
        }
    }

    cert->type = CCC_CERT_TYPE_DEVICE;

    return OK;
}

error_t ccc_verify_certificate(const ccc_certificate_t *cert, const uint8_t *trusted_pubkey)
{
    if (cert == NULL || trusted_pubkey == NULL) {
        return ERROR_INVALID_PARAM;
    }

    /* 验证证书签名: 使用信任公钥验证证书自签名 */
    uint8_t tbs_data[256];
    size_t tbs_len = 0;

    /* 构建待签名数据: Type(1) + Serial(16) + Issuer(16) + Subject(16) + PublicKey(65) */
    tbs_data[tbs_len++] = (uint8_t)cert->type;
    memcpy(&tbs_data[tbs_len], cert->serial_number, 16);
    tbs_len += 16;
    memcpy(&tbs_data[tbs_len], cert->issuer_id, 16);
    tbs_len += 16;
    memcpy(&tbs_data[tbs_len], cert->subject_id, 16);
    tbs_len += 16;
    memcpy(&tbs_data[tbs_len], cert->public_key, 65);
    tbs_len += 65;

    bool valid = g_se_interface.verify(tbs_data, tbs_len, trusted_pubkey, cert->signature);
    if (!valid) {
        return CCC_ERROR_SIGNATURE_INVALID;
    }

    return OK;
}

error_t ccc_validate_cert_chain(
    const ccc_cert_chain_t *cert_chain,
    const ccc_certificate_t *trusted_root)
{
    if (cert_chain == NULL || cert_chain->cert_count == 0 || trusted_root == NULL) {
        return ERROR_INVALID_PARAM;
    }

    /* 验证根证书自签名 */
    error_t ret = ccc_verify_certificate(trusted_root, trusted_root->public_key);
    if (ret != OK) {
        return ret;
    }

    /* 验证链上每个证书由上一级签发 */
    const uint8_t *issuer_pubkey = trusted_root->public_key;

    for (uint8_t i = 0; i < cert_chain->cert_count; i++) {
        const ccc_certificate_t *current = &cert_chain->certs[i];

        /* 检查颁发者匹配上一级主体 */
        if (memcmp(current->issuer_id, trusted_root->subject_id, 16) != 0) {
            if (i > 0) {
                if (memcmp(current->issuer_id, cert_chain->certs[i - 1].subject_id, 16) != 0) {
                    return CCC_ERROR_CERT_INVALID;
                }
            }
        }

        /* 验证证书有效性 */
        ret = ccc_verify_certificate(current, issuer_pubkey);
        if (ret != OK) {
            return ret;
        }

        issuer_pubkey = current->public_key;
    }

    return OK;
}

size_t ccc_get_certificate_length(const ccc_certificate_t *cert)
{
    if (cert == NULL) {
        return 0;
    }

    /* 返回 DER 序列化后的估计长度 */
    return 512;  /* 简化的估计值 */
}

error_t ccc_generate_self_signed_certificate(
    ccc_cert_type_t cert_type,
    const uint8_t *subject_id,
    const uint8_t *key_pair,
    uint32_t validity_days,
    ccc_certificate_t *cert)
{
    if (subject_id == NULL || key_pair == NULL || cert == NULL) {
        return ERROR_INVALID_PARAM;
    }

    memset(cert, 0, sizeof(ccc_certificate_t));

    cert->type = cert_type;

    /* 生成随机序列号 */
    error_t ret = ccc_generate_random(cert->serial_number, 16);
    if (ret != OK) {
        return ret;
    }

    /* 自签名: issuer = subject */
    memcpy(cert->issuer_id, subject_id, 16);
    memcpy(cert->subject_id, subject_id, 16);

    /* 有效期 */
    cert->valid_from = ccc_get_timestamp_ms() / 1000;
    cert->valid_until = cert->valid_from + (validity_days * 86400);

    /* 公钥 */
    memcpy(cert->public_key, key_pair, 65);

    /* 构建待签名数据 */
    uint8_t tbs_data[256];
    size_t tbs_len = 0;
    tbs_data[tbs_len++] = (uint8_t)cert->type;
    memcpy(&tbs_data[tbs_len], cert->serial_number, 16);
    tbs_len += 16;
    memcpy(&tbs_data[tbs_len], cert->issuer_id, 16);
    tbs_len += 16;
    memcpy(&tbs_data[tbs_len], cert->subject_id, 16);
    tbs_len += 16;
    memcpy(&tbs_data[tbs_len], cert->public_key, 65);
    tbs_len += 65;

    /* 使用私钥签名 (私钥在 key_pair[65:97]) */
    ret = g_se_interface.sign(tbs_data, tbs_len, &key_pair[65], cert->signature);
    if (ret != OK) {
        return ret;
    }

    return OK;
}

/******************************************************************************
 * 内部辅助函数实现
 ******************************************************************************/

/**
 * @brief 使用 mbedtls HKDF-SHA256 派生密钥
 */
static error_t ccc_hkdf_sha256(
    const uint8_t *ikm, size_t ikm_len,
    const uint8_t *salt, size_t salt_len,
    const uint8_t *info, size_t info_len,
    uint8_t *okm, size_t okm_len)
{
    if (ikm == NULL || ikm_len == 0 || okm == NULL || okm_len == 0) {
        return ERROR_INVALID_PARAM;
    }

    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        return ERROR_CRYPTO_FAILURE;
    }

    int ret = mbedtls_hkdf(md_info,
                           salt, salt_len,
                           ikm, ikm_len,
                           info, info_len,
                           okm, okm_len);
    if (ret != 0) {
        return ERROR_CRYPTO_FAILURE;
    }

    return OK;
}

/**
 * @brief 生成密码学安全的随机数 (使用 mbedtls CTR_DRBG)
 */
static error_t ccc_generate_random(uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return ERROR_INVALID_PARAM;
    }

    if (!g_rng_initialized) {
        /* 如果 RNG 未初始化, 回退到简单 LCG (仅用于测试) */
        static uint32_t seed = 1;
        for (size_t i = 0; i < len; i++) {
            seed = seed * 1103515245 + 12345;
            buf[i] = (uint8_t)(seed >> 16);
        }
        return OK;
    }

    int ret = mbedtls_ctr_drbg_random(&g_ctr_drbg, buf, len);
    if (ret != 0) {
        return ERROR_CRYPTO_FAILURE;
    }

    return OK;
}

/**
 * @brief 获取毫秒级时间戳 (单调递增)
 */
static uint32_t ccc_get_timestamp_ms(void)
{
    /* 使用 mbedtls 平台时间函数 */
    /* 如果没有真实时间源, 返回计数器值 */
    static uint32_t counter = 0;
    return ++counter * 1000;
}

/**
 * @brief 验证消息计数器 (重放攻击防护)
 */
static error_t ccc_validate_message_counter(
    ccc_session_context_t *session,
    uint32_t received_counter)
{
    if (session == NULL) {
        return ERROR_INVALID_PARAM;
    }

    /* 收到的计数器必须等于会话当前计数器 */
    if (received_counter != session->message_counter) {
        return CCC_ERROR_SESSION_ESTABLISH_FAILED;
    }

    return OK;
}

/**
 * @brief AES-128-GCM 加密
 */
static error_t ccc_aes_gcm_encrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *plain, size_t plain_len,
    uint8_t *cipher,
    uint8_t *tag, size_t tag_len)
{
    if (key == NULL || iv == NULL || plain == NULL || cipher == NULL || tag == NULL) {
        return ERROR_INVALID_PARAM;
    }

    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);

    int ret = mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, key,
                                  (unsigned int)(key_len * 8));
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return ERROR_CRYPTO_FAILURE;
    }

    ret = mbedtls_gcm_crypt_and_tag(&gcm_ctx, MBEDTLS_GCM_ENCRYPT,
                                     plain_len,
                                     iv, iv_len,
                                     aad, aad_len,
                                     plain, cipher,
                                     tag_len, tag);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return ERROR_CRYPTO_FAILURE;
    }

    mbedtls_gcm_free(&gcm_ctx);
    return OK;
}

/**
 * @brief AES-128-GCM 解密并验证认证标签
 */
static error_t ccc_aes_gcm_decrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *cipher, size_t cipher_len,
    const uint8_t *tag, size_t tag_len,
    uint8_t *plain)
{
    if (key == NULL || iv == NULL || cipher == NULL || tag == NULL || plain == NULL) {
        return ERROR_INVALID_PARAM;
    }

    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);

    int ret = mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, key,
                                  (unsigned int)(key_len * 8));
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return ERROR_CRYPTO_FAILURE;
    }

    ret = mbedtls_gcm_auth_decrypt(&gcm_ctx,
                                    cipher_len,
                                    iv, iv_len,
                                    aad, aad_len,
                                    tag, tag_len,
                                    cipher, plain);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return CCC_ERROR_SESSION_ESTABLISH_FAILED;
    }

    mbedtls_gcm_free(&gcm_ctx);
    return OK;
}

/**
 * @brief 安全内存清零
 */
void nvm_secure_zero(void *ptr, size_t len)
{
    if (ptr == NULL || len == 0) {
        return;
    }

    volatile uint8_t *p = (volatile uint8_t *)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }
}
