/**
 * @file CccSecureChannel.c
 * @brief CCC (Car Connectivity Consortium) 安全通信实现
 * 
 * 功能: 实现AES-128-GCM加解密、消息认证、重放防护
 * 符合CCC Digital Key规范 3.0
 * 
 * @note 此示例已整合到 yuleDKCS/embedded/examples/ccc_reference/
 *       集成 yuleDKCS ccc_core + yuleASR BSW
 *
 * @author yuleASR Team / yuleDKCS Team
 * @version 2.0.0
 */

/*==================================================================================================
*                                       包含头文件
==================================================================================================*/
#include "CccDigitalKey.h"
#include "CccIntegration.h"  /* yuleDKCS + yuleASR 集成说明 */
#include "Csm.h"             /* yuleASR CSM 加密服务管理 */
#include <string.h>

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ yuleDKCS ccc_core 集成说明                                              │
 * │                                                                         │
 * │ 此文件的安全通信功能对应 yuleDKCS CCC 协议栈:                            │
 * │                                                                         │
 * │ Ccc_EncryptMessage() 对应:                                              │
 * │   error_t ccc_encrypt_message(session, command, payload, len,           │
 * │                                encrypted, &encrypted_len);               │
 * │ 内部调用 NXP SE050 硬件加速 AES-GCM                                     │
 * │                                                                         │
 * │ Ccc_DecryptMessage() 对应:                                              │
 * │   error_t ccc_decrypt_message(session, encrypted, len,                  │
 * │                                &command, payload, &payload_len);         │
 * │                                                                         │
 * │ Ccc_CreateSecureMessage() / Ccc_ParseSecureMessage() 对应:              │
 * │   ccc_send_vehicle_command() 和内部消息序列化/反序列化                   │
 * │                                                                         │
 * │ Ccc_GenerateMac() / Ccc_VerifyMac() 对应:                               │
 * │   error_t ccc_compute_mac(mac_key, message, len, mac);                   │
 * │   error_t ccc_verify_mac(mac_key, message, len, mac);                    │
 * │                                                                         │
 * │ yuleASR BSW 集成说明:                                                   │
 * │ - Csm_Encrypt(): AES-128-GCM 加密 (通过 yuleASR CSM)                   │
 * │ - Csm_Decrypt(): AES-128-GCM 解密                                      │
 * │ - Csm_MacGenerate(): 消息认证码生成 (HMAC-SHA256)                       │
 * │ - Csm_MacVerify(): 消息认证码验证                                      │
 * │                                                                         │
 * │ 蓝牙/BLE 传输层可通过 yuleDKCS BLE 模块配合:                             │
 * │   #include "ccc/ccc_digital_key.h"                                       │
 * │   ble_kw47a_send(conn_handle, data, len);                                │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

/*==================================================================================================
*                                       宏定义
==================================================================================================*/
/**
 * @brief 检查指针是否为空
 */
#define CCC_CHECK_POINTER(ptr) \
    do { \
        if ((ptr) == NULL) { \
            return CCC_E_PARAM_POINTER; \
        } \
    } while (0)

/**
 * @brief 最大序列号
 */
#define CCC_MAX_SEQUENCE_NUMBER                 0xFFFFFFFFU

/**
 * @brief 重放窗口大小
 */
#define CCC_REPLAY_WINDOW_SIZE                  32U

/**
 * @brief 会话超时时间 (毫秒)
 */
#define CCC_SESSION_TIMEOUT_MS                  300000U  /* 5分钟 */

/*==================================================================================================
*                                       全局变量
==================================================================================================*/
extern Ccc_RuntimeDataType* Ccc_GetRuntimeData(void);
extern const Ccc_ConfigType* Ccc_GetConfig(void);

/*==================================================================================================
*                                       本地函数声明
==================================================================================================*/
static Ccc_ReturnType Ccc_CheckReplay(uint32 sequenceNumber);
static void Ccc_UpdateReplayWindow(uint32 sequenceNumber);
static Ccc_ReturnType Ccc_ValidateSecureMessage(const Ccc_SecureMessageType* message);
static void Ccc_IncrementSequenceNumber(void);

/*==================================================================================================
*                                       安全通信API实现
==================================================================================================*/

/**
 * @brief 加密消息
 */
Ccc_ReturnType Ccc_EncryptMessage(
    const uint8* plaintext,
    uint32 plaintextLength,
    uint8* ciphertext,
    uint32* ciphertextLength,
    uint8* authTag
)
{
    Std_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    const Ccc_ConfigType* config = Ccc_GetConfig();
    uint32 encryptedLength;
    
    CCC_CHECK_POINTER(plaintext);
    CCC_CHECK_POINTER(ciphertext);
    CCC_CHECK_POINTER(ciphertextLength);
    CCC_CHECK_POINTER(authTag);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 检查会话状态 */
    if (runtime->session.state != CCC_SESSION_STATE_ACTIVE) {
        return CCC_E_SESSION_NOT_ESTABLISHED;
    }
    
    /* 检查密钥是否有效 */
    if (!runtime->session.sessionKeys.valid) {
        return CCC_E_KEY_INVALID;
    }
    
    /* 检查缓冲区大小 */
    if (*ciphertextLength < plaintextLength) {
        return CCC_E_BUFFER_TOO_SMALL;
    }
    
#if (CCC_USE_CSM == STD_ON)
    /* 设置作业密钥 */
    result = Csm_JobKeySetUp(config->csmJobId, config->csmKeyId);
    if (result != E_OK) {
        return CCC_E_KEY_NOT_FOUND;
    }
    
    /* 设置IV */
    result = Csm_KeyElementSet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_IV,
        runtime->session.sessionKeys.encryptionKey.iv,
        CCC_AES_IV_SIZE
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* 设置加密密钥 */
    result = Csm_KeyElementSet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_SECRET,
        runtime->session.sessionKeys.encryptionKey.key,
        CCC_AES_KEY_SIZE
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* AES-128-GCM加密 */
    encryptedLength = *ciphertextLength;
    result = Csm_Encrypt(
        config->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        plaintext,
        plaintextLength,
        ciphertext,
        &encryptedLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    *ciphertextLength = encryptedLength;
    
    /* 获取认证标签 */
    uint32 tagLength = CCC_AES_TAG_SIZE;
    result = Csm_KeyElementGet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_TAG,
        authTag,
        &tagLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 - 实际应用中应使用真实的AES-GCM实现 */
    /* 简单异或加密 (仅用于演示) */
    for (uint32 i = 0U; i < plaintextLength; i++) {
        ciphertext[i] = plaintext[i] ^ runtime->session.sessionKeys.encryptionKey.key[i % CCC_AES_KEY_SIZE];
    }
    *ciphertextLength = plaintextLength;
    
    /* 生成模拟认证标签 */
    for (uint32 i = 0U; i < CCC_AES_TAG_SIZE; i++) {
        authTag[i] = runtime->session.sessionKeys.macKey.key[i];
    }
#endif
    
    /* 更新IV (每次加密后IV应该变化) */
    /* 在GCM中，IV通常是固定的nonce，但可以使用计数器模式 */
    
    return CCC_E_OK;
}

/**
 * @brief 解密消息
 */
Ccc_ReturnType Ccc_DecryptMessage(
    const uint8* ciphertext,
    uint32 ciphertextLength,
    const uint8* authTag,
    uint8* plaintext,
    uint32* plaintextLength
)
{
    Std_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    const Ccc_ConfigType* config = Ccc_GetConfig();
    uint32 decryptedLength;
    boolean verifyResult = FALSE;
    
    CCC_CHECK_POINTER(ciphertext);
    CCC_CHECK_POINTER(authTag);
    CCC_CHECK_POINTER(plaintext);
    CCC_CHECK_POINTER(plaintextLength);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 检查会话状态 */
    if (runtime->session.state != CCC_SESSION_STATE_ACTIVE) {
        return CCC_E_SESSION_NOT_ESTABLISHED;
    }
    
    /* 检查密钥是否有效 */
    if (!runtime->session.sessionKeys.valid) {
        return CCC_E_KEY_INVALID;
    }
    
    /* 检查缓冲区大小 */
    if (*plaintextLength < ciphertextLength) {
        return CCC_E_BUFFER_TOO_SMALL;
    }
    
#if (CCC_USE_CSM == STD_ON)
    /* 设置认证标签 */
    result = Csm_KeyElementSet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_TAG,
        authTag,
        CCC_AES_TAG_SIZE
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* 设置IV */
    result = Csm_KeyElementSet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_IV,
        runtime->session.sessionKeys.encryptionKey.iv,
        CCC_AES_IV_SIZE
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* AES-128-GCM解密 */
    decryptedLength = *plaintextLength;
    result = Csm_Decrypt(
        config->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        ciphertext,
        ciphertextLength,
        plaintext,
        &decryptedLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    *plaintextLength = decryptedLength;
    
    /* 验证认证标签 */
    result = Csm_MacVerify(
        config->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        plaintext,
        decryptedLength,
        authTag,
        CCC_AES_TAG_SIZE,
        &verifyResult
    );
    
    if (result != E_OK || !verifyResult) {
        return CCC_E_MESSAGE_INVALID;
    }
#else
    /* 模拟实现 */
    /* 简单异或解密 */
    for (uint32 i = 0U; i < ciphertextLength; i++) {
        plaintext[i] = ciphertext[i] ^ runtime->session.sessionKeys.encryptionKey.key[i % CCC_AES_KEY_SIZE];
    }
    *plaintextLength = ciphertextLength;
    
    /* 模拟验证标签 */
    verifyResult = TRUE;
    for (uint32 i = 0U; i < CCC_AES_TAG_SIZE; i++) {
        if (authTag[i] != runtime->session.sessionKeys.macKey.key[i]) {
            verifyResult = FALSE;
            break;
        }
    }
    
    if (!verifyResult) {
        return CCC_E_MESSAGE_INVALID;
    }
#endif
    
    return CCC_E_OK;
}

/**
 * @brief 创建安全消息
 */
Ccc_ReturnType Ccc_CreateSecureMessage(
    Ccc_MessageType messageType,
    const uint8* payload,
    uint32 payloadLength,
    Ccc_SecureMessageType* message
)
{
    Ccc_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    uint8 encryptedPayload[CCC_MAX_PAYLOAD_SIZE];
    uint32 encryptedLength = CCC_MAX_PAYLOAD_SIZE;
    uint8 authTag[CCC_AES_TAG_SIZE];
    
    CCC_CHECK_POINTER(payload);
    CCC_CHECK_POINTER(message);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    if (runtime->session.state != CCC_SESSION_STATE_ACTIVE) {
        return CCC_E_SESSION_NOT_ESTABLISHED;
    }
    
    if (payloadLength > CCC_MAX_PAYLOAD_SIZE) {
        return CCC_E_PARAM_LENGTH;
    }
    
    /* 加密载荷 */
    result = Ccc_EncryptMessage(
        payload,
        payloadLength,
        encryptedPayload,
        &encryptedLength,
        authTag
    );
    
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 填充消息头 */
    (void)memcpy(message->header.sessionId, runtime->session.sessionId, CCC_SESSION_ID_SIZE);
    message->header.sequenceNumber = runtime->session.sequenceNumber;
    message->header.timestamp = runtime->session.timestamp;
    message->header.messageType = (uint16)messageType;
    message->header.payloadLength = (uint16)encryptedLength;
    
    /* 复制加密载荷 */
    (void)memcpy(message->payload, encryptedPayload, encryptedLength);
    
    /* 复制认证标签 */
    (void)memcpy(message->authTag, authTag, CCC_AES_TAG_SIZE);
    
    /* 增加序列号 */
    Ccc_IncrementSequenceNumber();
    
    return CCC_E_OK;
}

/**
 * @brief 解析安全消息
 */
Ccc_ReturnType Ccc_ParseSecureMessage(
    const Ccc_SecureMessageType* message,
    uint8* payload,
    uint32* payloadLength,
    Ccc_MessageType* messageType
)
{
    Ccc_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    
    CCC_CHECK_POINTER(message);
    CCC_CHECK_POINTER(payload);
    CCC_CHECK_POINTER(payloadLength);
    CCC_CHECK_POINTER(messageType);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    if (runtime->session.state != CCC_SESSION_STATE_ACTIVE) {
        return CCC_E_SESSION_NOT_ESTABLISHED;
    }
    
    /* 验证消息格式 */
    result = Ccc_ValidateSecureMessage(message);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 检查会话ID */
    boolean sessionIdMatch = TRUE;
    for (uint32 i = 0U; i < CCC_SESSION_ID_SIZE; i++) {
        if (message->header.sessionId[i] != runtime->session.sessionId[i]) {
            sessionIdMatch = FALSE;
            break;
        }
    }
    
    if (!sessionIdMatch) {
        return CCC_E_SESSION_NOT_ESTABLISHED;
    }
    
    /* 检查重放攻击 */
    result = Ccc_CheckReplay(message->header.sequenceNumber);
    if (result != CCC_E_OK) {
        return CCC_E_REPLAY_DETECTED;
    }
    
    /* 解密载荷 */
    result = Ccc_DecryptMessage(
        message->payload,
        message->header.payloadLength,
        message->authTag,
        payload,
        payloadLength
    );
    
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 更新重放防护窗口 */
    Ccc_UpdateReplayWindow(message->header.sequenceNumber);
    
    /* 返回消息类型 */
    *messageType = (Ccc_MessageType)message->header.messageType;
    
    return CCC_E_OK;
}

/*==================================================================================================
*                                       本地函数实现
==================================================================================================*/

/**
 * @brief 检查重放攻击
 */
static Ccc_ReturnType Ccc_CheckReplay(uint32 sequenceNumber)
{
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    
    if (runtime == NULL) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 检查序列号是否已处理过 */
    if (sequenceNumber <= runtime->session.sequenceNumber - CCC_REPLAY_WINDOW_SIZE) {
        /* 序列号太老，可能是重放攻击 */
        return CCC_E_REPLAY_DETECTED;
    }
    
    if (sequenceNumber <= runtime->session.sequenceNumber) {
        /* 在窗口内，检查是否重复 */
        uint32 diff = runtime->session.sequenceNumber - sequenceNumber;
        if (diff < CCC_REPLAY_WINDOW_SIZE) {
            /* 检查位图 - 实际应用中需要维护位图 */
            /* 这里简化处理 */
        }
    }
    
    return CCC_E_OK;
}

/**
 * @brief 更新重放防护窗口
 */
static void Ccc_UpdateReplayWindow(uint32 sequenceNumber)
{
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    
    if (runtime == NULL) {
        return;
    }
    
    /* 更新序列号 */
    if (sequenceNumber > runtime->session.sequenceNumber) {
        runtime->session.sequenceNumber = sequenceNumber;
    }
}

/**
 * @brief 验证安全消息格式
 */
static Ccc_ReturnType Ccc_ValidateSecureMessage(const Ccc_SecureMessageType* message)
{
    /* 检查载荷长度 */
    if (message->header.payloadLength > CCC_MAX_PAYLOAD_SIZE) {
        return CCC_E_MESSAGE_INVALID;
    }
    
    /* 检查消息类型 */
    if (message->header.messageType == (uint16)CCC_MSG_NONE ||
        message->header.messageType >= (uint16)CCC_MSG_ERROR) {
        return CCC_E_MESSAGE_INVALID;
    }
    
    /* 检查时间戳 (可选 - 检查消息是否过期) */
    /* 实际应用中应检查时间戳是否在允许的范围内 */
    
    return CCC_E_OK;
}

/**
 * @brief 增加序列号
 */
static void Ccc_IncrementSequenceNumber(void)
{
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    
    if (runtime == NULL) {
        return;
    }
    
    if (runtime->session.sequenceNumber >= CCC_MAX_SEQUENCE_NUMBER) {
        /* 序列号溢出，需要重新建立会话 */
        runtime->session.state = CCC_SESSION_STATE_INACTIVE;
    } else {
        runtime->session.sequenceNumber++;
    }
}

/*==================================================================================================
*                                       辅助函数
==================================================================================================*/

/**
 * @brief 生成消息认证码 (MAC)
 * 
 * 使用HMAC-SHA256生成消息认证码
 * 
 * @param message 消息数据
 * @param messageLength 消息长度
 * @param mac MAC输出缓冲区
 * @param macLength MAC长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_PARAM_POINTER: 参数指针为空
 *         CCC_E_CRYPTO_FAILURE: MAC生成失败
 */
Ccc_ReturnType Ccc_GenerateMac(
    const uint8* message,
    uint32 messageLength,
    uint8* mac,
    uint32* macLength
)
{
    Std_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    const Ccc_ConfigType* config = Ccc_GetConfig();
    
    CCC_CHECK_POINTER(message);
    CCC_CHECK_POINTER(mac);
    CCC_CHECK_POINTER(macLength);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    if (!runtime->session.sessionKeys.valid) {
        return CCC_E_KEY_INVALID;
    }
    
#if (CCC_USE_CSM == STD_ON)
    /* 设置MAC密钥 */
    result = Csm_KeyElementSet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_SECRET,
        runtime->session.sessionKeys.macKey.key,
        CCC_AES_KEY_SIZE
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* 生成MAC */
    result = Csm_MacGenerate(
        config->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        message,
        messageLength,
        mac,
        macLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 */
    if (*macLength >= CCC_AES_KEY_SIZE) {
        for (uint32 i = 0U; i < CCC_AES_KEY_SIZE; i++) {
            mac[i] = message[i % messageLength] ^ runtime->session.sessionKeys.macKey.key[i];
        }
        *macLength = CCC_AES_KEY_SIZE;
    }
#endif
    
    return CCC_E_OK;
}

/**
 * @brief 验证消息认证码 (MAC)
 * 
 * @param message 消息数据
 * @param messageLength 消息长度
 * @param mac MAC值
 * @param macLength MAC长度
 * @return CCC_E_OK: 验证通过
 *         CCC_E_MESSAGE_INVALID: MAC不匹配
 *         CCC_E_CRYPTO_FAILURE: 验证失败
 */
Ccc_ReturnType Ccc_VerifyMac(
    const uint8* message,
    uint32 messageLength,
    const uint8* mac,
    uint32 macLength
)
{
    Std_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    const Ccc_ConfigType* config = Ccc_GetConfig();
    boolean verifyResult = FALSE;
    
    CCC_CHECK_POINTER(message);
    CCC_CHECK_POINTER(mac);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    if (!runtime->session.sessionKeys.valid) {
        return CCC_E_KEY_INVALID;
    }
    
#if (CCC_USE_CSM == STD_ON)
    /* 设置MAC密钥 */
    result = Csm_KeyElementSet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_SECRET,
        runtime->session.sessionKeys.macKey.key,
        CCC_AES_KEY_SIZE
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* 验证MAC */
    result = Csm_MacVerify(
        config->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        message,
        messageLength,
        mac,
        macLength,
        &verifyResult
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    if (!verifyResult) {
        return CCC_E_MESSAGE_INVALID;
    }
#else
    /* 模拟实现 */
    verifyResult = TRUE;
    uint8 expectedMac[CCC_AES_KEY_SIZE];
    uint32 expectedMacLength = CCC_AES_KEY_SIZE;
    
    Ccc_ReturnType macResult = Ccc_GenerateMac(message, messageLength, expectedMac, &expectedMacLength);
    if (macResult != CCC_E_OK) {
        return macResult;
    }
    
    for (uint32 i = 0U; i < macLength && i < expectedMacLength; i++) {
        if (mac[i] != expectedMac[i]) {
            verifyResult = FALSE;
            break;
        }
    }
    
    if (!verifyResult) {
        return CCC_E_MESSAGE_INVALID;
    }
#endif
    
    return CCC_E_OK;
}
