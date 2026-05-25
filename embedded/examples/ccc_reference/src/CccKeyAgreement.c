/**
 * @file CccKeyAgreement.c
 * @brief CCC (Car Connectivity Consortium) 密钥协商实现
 * 
 * 功能: 实现ECDH P-256密钥协商和HKDF-SHA256密钥派生
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
 * │ 此文件的密钥协商功能对应 yuleDKCS 的 CCC 协议栈:                         │
 * │                                                                         │
 * │ Ccc_PairingStart() 对应:                                                │
 * │   error_t ccc_create_pairing_session(config, &session);                  │
 * │   error_t ccc_start_pairing(session, req_data, &req_len);                │
 * │                                                                         │
 * │ Ccc_PairingComplete() 对应:                                             │
 * │   error_t ccc_process_pairing_response(session, resp, resp_len);         │
 * │   error_t ccc_complete_pairing(session, confirm, &confirm_len);          │
 * │                                                                         │
 * │ Ccc_SessionEstablish() 对应:                                            │
 * │   error_t ccc_establish_session(session, vehicle_pub, challenge);        │
 * │                                                                         │
 * │ ECDH 共享密钥计算 对应 yuleDKCS 安全模块:                                │
 * │   error_t (*derive_shared_secret)(priv_key, pub_key, shared_secret)      │
 * │   在 ccc_se_interface_t 中定义,由 SE050 硬件加速实现                      │
 * │                                                                         │
 * │ HKDF 密钥派生 对应:                                                      │
 * │   error_t ccc_derive_session_keys(shared_secret, salt, enc_key, mac_key) │
 * │                                                                         │
 * │ yuleASR BSW 集成说明:                                                   │
 * │ - Csm_KeyExchangeCalcSecret(): ECDH 共享密钥计算                         │
 * │ - Csm_MacGenerate(): HMAC-SHA256 用于 HKDF Extract/Expand               │
 * │ - Csm_KeyGenerate(): ECC P-256 密钥对生成                                │
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
 * @brief HKDF抽取次数
 */
#define CCC_HKDF_EXTRACT_ITERATIONS             1U
#define CCC_HKDF_EXPAND_ITERATIONS              1U

/**
 * @brief 密钥派生信息字符串
 */
#define CCC_KDF_INFO_ENCRYPTION                 "CCC-DK-Encryption"
#define CCC_KDF_INFO_MAC                        "CCC-DK-MAC"
#define CCC_KDF_INFO_SESSION                    "CCC-DK-Session"

/*==================================================================================================
*                                       全局变量
==================================================================================================*/
extern Ccc_RuntimeDataType* Ccc_GetRuntimeData(void);
extern const Ccc_ConfigType* Ccc_GetConfig(void);

/*==================================================================================================
*                                       本地函数声明
==================================================================================================*/
static Ccc_ReturnType Ccc_GenerateEphemeralKey(Ccc_EccKeyPairType* keyPair);
static Ccc_ReturnType Ccc_CalculateSharedSecret(
    const Ccc_EccKeyPairType* localKey,
    const uint8* remotePublicKey,
    uint32 remotePublicKeyLength,
    uint8* sharedSecret
);
static Ccc_ReturnType Ccc_HkdfExtract(
    const uint8* salt,
    uint32 saltLength,
    const uint8* ikm,
    uint32 ikmLength,
    uint8* prk
);
static Ccc_ReturnType Ccc_HkdfExpand(
    const uint8* prk,
    uint32 prkLength,
    const uint8* info,
    uint32 infoLength,
    uint8* okm,
    uint32 okmLength
);

/*==================================================================================================
*                                       配对API实现
==================================================================================================*/

/**
 * @brief 开始配对流程
 */
Ccc_ReturnType Ccc_PairingStart(
    const Ccc_DeviceIdType* remoteDevice,
    uint8* localPublicKey,
    uint32* publicKeyLength
)
{
    Ccc_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    const Ccc_ConfigType* config = Ccc_GetConfig();
    
    CCC_CHECK_POINTER(remoteDevice);
    CCC_CHECK_POINTER(localPublicKey);
    CCC_CHECK_POINTER(publicKeyLength);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 检查缓冲区大小 */
    if (*publicKeyLength < CCC_ECC_P256_PUBLIC_KEY_SIZE) {
        return CCC_E_BUFFER_TOO_SMALL;
    }
    
    /* 保存远程设备信息 */
    runtime->pairingData.remoteDevice = *remoteDevice;
    
    /* 生成临时密钥对 */
    result = Ccc_GenerateEphemeralKey(&runtime->pairingData.ephemeralKey);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 复制公钥到输出缓冲区 */
    (void)memcpy(localPublicKey, 
                 runtime->pairingData.ephemeralKey.publicKey, 
                 CCC_ECC_P256_PUBLIC_KEY_SIZE);
    *publicKeyLength = CCC_ECC_P256_PUBLIC_KEY_SIZE;
    
    /* 生成本地随机数 */
    result = Ccc_GenerateRandom(runtime->pairingData.localRandom, CCC_NONCE_SIZE);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 设置当前模式为配对模式 */
    runtime->currentMode = CCC_MODE_PAIRING;
    
    return CCC_E_OK;
}

/**
 * @brief 完成配对流程
 */
Ccc_ReturnType Ccc_PairingComplete(
    const uint8* remotePublicKey,
    uint32 remotePublicKeyLength,
    const Ccc_CertificateType* remoteCert
)
{
    Ccc_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    uint8 sharedSecret[CCC_ECC_P256_KEY_SIZE];
    
    CCC_CHECK_POINTER(remotePublicKey);
    CCC_CHECK_POINTER(remoteCert);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    if (runtime->currentMode != CCC_MODE_PAIRING) {
        return CCC_E_PARAM_MODE;
    }
    
    /* 检查公钥长度 */
    if (remotePublicKeyLength != CCC_ECC_P256_PUBLIC_KEY_SIZE) {
        return CCC_E_PARAM_LENGTH;
    }
    
    /* 验证远程证书 */
    result = Ccc_VerifyCertificate(remoteCert, NULL);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 保存远程证书 */
    runtime->pairingData.remoteCert = *remoteCert;
    
    /* 计算共享密钥 */
    result = Ccc_CalculateSharedSecret(
        &runtime->pairingData.ephemeralKey,
        remotePublicKey,
        remotePublicKeyLength,
        sharedSecret
    );
    
    if (result != CCC_E_OK) {
        /* 清除敏感数据 */
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        return result;
    }
    
    /* 存储长期密钥 (实际应用中应使用安全存储) */
    /* 此处省略详细实现 */
    
    /* 清除临时密钥 */
    (void)memset(&runtime->pairingData.ephemeralKey, 0, sizeof(Ccc_EccKeyPairType));
    (void)memset(sharedSecret, 0, sizeof(sharedSecret));
    
    /* 设置当前模式 */
    runtime->currentMode = CCC_MODE_AUTHENTICATION;
    
    return CCC_E_OK;
}

/*==================================================================================================
*                                       会话管理API实现
==================================================================================================*/

/**
 * @brief 建立安全会话
 */
Ccc_ReturnType Ccc_SessionEstablish(
    boolean isInitiator,
    const uint8* remotePublicKey,
    uint32 remotePublicKeyLength
)
{
    Ccc_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    const Ccc_ConfigType* config = Ccc_GetConfig();
    uint8 sharedSecret[CCC_ECC_P256_KEY_SIZE];
    uint8 prk[CCC_ECC_P256_KEY_SIZE];
    uint8 keyMaterial[CCC_AES_KEY_SIZE * 2U];
    Ccc_DerivationDataType kdfData;
    
    CCC_CHECK_POINTER(remotePublicKey);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 检查是否在认证模式 */
    if (runtime->currentMode != CCC_MODE_AUTHENTICATION && 
        runtime->currentMode != CCC_MODE_OPERATIONAL) {
        return CCC_E_PARAM_MODE;
    }
    
    /* 检查公钥长度 */
    if (remotePublicKeyLength != CCC_ECC_P256_PUBLIC_KEY_SIZE) {
        return CCC_E_PARAM_LENGTH;
    }
    
    /* 生成会话临时密钥对 */
    result = Ccc_GenerateEphemeralKey(&runtime->session.ephemeralKey);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 计算共享密钥 */
    result = Ccc_CalculateSharedSecret(
        &runtime->session.ephemeralKey,
        remotePublicKey,
        remotePublicKeyLength,
        sharedSecret
    );
    
    if (result != CCC_E_OK) {
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        return result;
    }
    
    /* 准备HKDF数据 */
    (void)memset(&kdfData, 0, sizeof(kdfData));
    (void)memcpy(kdfData.sharedSecret, sharedSecret, CCC_ECC_P256_KEY_SIZE);
    
    /* 生成盐值 */
    result = Ccc_GenerateRandom(kdfData.salt, CCC_ECC_P256_KEY_SIZE);
    if (result != CCC_E_OK) {
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        return result;
    }
    
    /* HKDF Extract */
    result = Ccc_HkdfExtract(
        kdfData.salt,
        CCC_ECC_P256_KEY_SIZE,
        sharedSecret,
        CCC_ECC_P256_KEY_SIZE,
        prk
    );
    
    if (result != CCC_E_OK) {
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        (void)memset(prk, 0, sizeof(prk));
        return result;
    }
    
    /* HKDF Expand - 派生加密密钥和MAC密钥 */
    (void)memcpy(kdfData.info, CCC_KDF_INFO_ENCRYPTION, sizeof(CCC_KDF_INFO_ENCRYPTION));
    kdfData.infoLength = sizeof(CCC_KDF_INFO_ENCRYPTION);
    
    result = Ccc_HkdfExpand(
        prk,
        CCC_ECC_P256_KEY_SIZE,
        kdfData.info,
        kdfData.infoLength,
        keyMaterial,
        CCC_AES_KEY_SIZE * 2U
    );
    
    if (result != CCC_E_OK) {
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        (void)memset(prk, 0, sizeof(prk));
        (void)memset(keyMaterial, 0, sizeof(keyMaterial));
        return result;
    }
    
    /* 设置会话密钥 */
    (void)memcpy(runtime->session.sessionKeys.encryptionKey.key, 
                 keyMaterial, 
                 CCC_AES_KEY_SIZE);
    (void)memcpy(runtime->session.sessionKeys.macKey.key, 
                 &keyMaterial[CCC_AES_KEY_SIZE], 
                 CCC_AES_KEY_SIZE);
    
    /* 生成IV */
    result = Ccc_GenerateRandom(runtime->session.sessionKeys.encryptionKey.iv, 
                                 CCC_AES_IV_SIZE);
    if (result != CCC_E_OK) {
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        (void)memset(prk, 0, sizeof(prk));
        (void)memset(keyMaterial, 0, sizeof(keyMaterial));
        return result;
    }
    
    /* 生成会话ID */
    result = Ccc_GenerateRandom(runtime->session.sessionId, CCC_SESSION_ID_SIZE);
    if (result != CCC_E_OK) {
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        (void)memset(prk, 0, sizeof(prk));
        (void)memset(keyMaterial, 0, sizeof(keyMaterial));
        return result;
    }
    
    /* 生成随机数 */
    result = Ccc_GenerateRandom(runtime->session.localNonce, CCC_NONCE_SIZE);
    if (result != CCC_E_OK) {
        (void)memset(sharedSecret, 0, sizeof(sharedSecret));
        (void)memset(prk, 0, sizeof(prk));
        (void)memset(keyMaterial, 0, sizeof(keyMaterial));
        return result;
    }
    
    /* 设置会话参数 */
    runtime->session.isInitiator = isInitiator;
    runtime->session.state = CCC_SESSION_STATE_ACTIVE;
    runtime->session.sequenceNumber = 0U;
    runtime->session.sessionKeys.valid = TRUE;
    
    /* 清除敏感数据 */
    (void)memset(sharedSecret, 0, sizeof(sharedSecret));
    (void)memset(prk, 0, sizeof(prk));
    (void)memset(keyMaterial, 0, sizeof(keyMaterial));
    
    /* 设置当前模式为操作模式 */
    runtime->currentMode = CCC_MODE_OPERATIONAL;
    
    return CCC_E_OK;
}

/**
 * @brief 关闭安全会话
 */
Ccc_ReturnType Ccc_SessionClose(void)
{
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 清除会话密钥 */
    if (runtime->session.sessionKeys.valid) {
        (void)memset(&runtime->session.sessionKeys, 0, sizeof(Ccc_SessionKeyType));
    }
    
    /* 清除临时密钥 */
    (void)memset(&runtime->session.ephemeralKey, 0, sizeof(Ccc_EccKeyPairType));
    
    /* 清除会话上下文 */
    (void)memset(&runtime->session.localNonce, 0, CCC_NONCE_SIZE);
    (void)memset(&runtime->session.remoteNonce, 0, CCC_NONCE_SIZE);
    
    /* 重置会话状态 */
    runtime->session.state = CCC_SESSION_STATE_INACTIVE;
    runtime->session.sequenceNumber = 0U;
    runtime->session.timestamp = 0U;
    
    /* 设置当前模式 */
    runtime->currentMode = CCC_MODE_AUTHENTICATION;
    
    return CCC_E_OK;
}

/*==================================================================================================
*                                       本地函数实现
==================================================================================================*/

/**
 * @brief 生成临时ECC密钥对
 */
static Ccc_ReturnType Ccc_GenerateEphemeralKey(Ccc_EccKeyPairType* keyPair)
{
    Std_ReturnType result;
    const Ccc_ConfigType* config = Ccc_GetConfig();
    uint32 keyLength = CCC_ECC_P256_KEY_SIZE;
    uint32 pubKeyLength = CCC_ECC_P256_PUBLIC_KEY_SIZE;
    
    if (keyPair == NULL) {
        return CCC_E_PARAM_POINTER;
    }
    
    /* 清除密钥缓冲区 */
    (void)memset(keyPair, 0, sizeof(Ccc_EccKeyPairType));
    
#if (CCC_USE_CSM == STD_ON)
    /* 生成密钥对 */
    result = Csm_KeyGenerate(config->csmKeyId);
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* 获取私钥 */
    result = Csm_KeyElementGet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_PRIVATE,
        keyPair->privateKey,
        &keyLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* 获取公钥 */
    result = Csm_KeyElementGet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_PUBLIC,
        keyPair->publicKey,
        &pubKeyLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 - 实际应用中应使用真实的密钥生成 */
    /* 生成私钥 (模拟) */
    for (uint32 i = 0U; i < CCC_ECC_P256_KEY_SIZE; i++) {
        keyPair->privateKey[i] = (uint8)(0xAA + i);
    }
    
    /* 生成公钥 (模拟 - 未压缩格式: 0x04 + x + y) */
    keyPair->publicKey[0] = 0x04;  /* 未压缩标识 */
    for (uint32 i = 1U; i < CCC_ECC_P256_PUBLIC_KEY_SIZE; i++) {
        keyPair->publicKey[i] = (uint8)(0xBB + i);
    }
#endif
    
    keyPair->valid = TRUE;
    return CCC_E_OK;
}

/**
 * @brief 计算共享密钥
 */
static Ccc_ReturnType Ccc_CalculateSharedSecret(
    const Ccc_EccKeyPairType* localKey,
    const uint8* remotePublicKey,
    uint32 remotePublicKeyLength,
    uint8* sharedSecret
)
{
    Std_ReturnType result;
    const Ccc_ConfigType* config = Ccc_GetConfig();
    
    if (localKey == NULL || remotePublicKey == NULL || sharedSecret == NULL) {
        return CCC_E_PARAM_POINTER;
    }
    
    if (!localKey->valid) {
        return CCC_E_KEY_INVALID;
    }
    
#if (CCC_USE_CSM == STD_ON)
    /* 计算共享密钥 */
    result = Csm_KeyExchangeCalcSecret(
        config->csmKeyId,
        remotePublicKey,
        remotePublicKeyLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    /* 获取共享密钥 */
    uint32 secretLength = CCC_ECC_P256_KEY_SIZE;
    result = Csm_KeyElementGet(
        config->csmKeyId,
        CSM_KEY_ELEMENT_TYPE_SECRET,
        sharedSecret,
        &secretLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 - 实际应用中应使用真实的ECDH计算 */
    for (uint32 i = 0U; i < CCC_ECC_P256_KEY_SIZE; i++) {
        sharedSecret[i] = (uint8)(localKey->privateKey[i] ^ remotePublicKey[i + 1]);
    }
#endif
    
    return CCC_E_OK;
}

/**
 * @brief HKDF Extract阶段
 */
static Ccc_ReturnType Ccc_HkdfExtract(
    const uint8* salt,
    uint32 saltLength,
    const uint8* ikm,
    uint32 ikmLength,
    uint8* prk
)
{
    Std_ReturnType result;
    const Ccc_ConfigType* config = Ccc_GetConfig();
    uint32 prkLength = CCC_ECC_P256_KEY_SIZE;
    
    if (salt == NULL || ikm == NULL || prk == NULL) {
        return CCC_E_PARAM_POINTER;
    }
    
#if (CCC_USE_CSM == STD_ON)
    /* 使用HMAC-SHA256计算PRK */
    uint8 hmacData[CCC_ECC_P256_KEY_SIZE * 2U];
    uint32 hmacDataLength = 0U;
    
    /* HMAC-SHA256(ikm, salt) */
    (void)memcpy(hmacData, salt, saltLength);
    hmacDataLength += saltLength;
    (void)memcpy(&hmacData[hmacDataLength], ikm, ikmLength);
    hmacDataLength += ikmLength;
    
    result = Csm_MacGenerate(
        config->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        hmacData,
        hmacDataLength,
        prk,
        &prkLength
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
#else
    /* 模拟实现 */
    for (uint32 i = 0U; i < CCC_ECC_P256_KEY_SIZE; i++) {
        prk[i] = (uint8)(salt[i % saltLength] ^ ikm[i % ikmLength]);
    }
#endif
    
    return CCC_E_OK;
}

/**
 * @brief HKDF Expand阶段
 */
static Ccc_ReturnType Ccc_HkdfExpand(
    const uint8* prk,
    uint32 prkLength,
    const uint8* info,
    uint32 infoLength,
    uint8* okm,
    uint32 okmLength
)
{
    Std_ReturnType result;
    const Ccc_ConfigType* config = Ccc_GetConfig();
    uint32 iteration = 1U;
    uint32 done = 0U;
    uint8 prev[CCC_ECC_P256_KEY_SIZE];
    uint32 prevLength = 0U;
    
    if (prk == NULL || info == NULL || okm == NULL) {
        return CCC_E_PARAM_POINTER;
    }
    
    (void)memset(prev, 0, sizeof(prev));
    
    while (done < okmLength) {
        uint8 t[CCC_ECC_P256_KEY_SIZE + 256U];
        uint32 tLength = 0U;
        
        /* T(i) = HMAC-SHA256(PRK, T(i-1) | info | i) */
        if (prevLength > 0U) {
            (void)memcpy(t, prev, prevLength);
            tLength += prevLength;
        }
        
        (void)memcpy(&t[tLength], info, infoLength);
        tLength += infoLength;
        t[tLength++] = (uint8)iteration;
        
#if (CCC_USE_CSM == STD_ON)
        result = Csm_MacGenerate(
            config->csmJobId,
            CSM_OPERATIONMODE_SINGLECALL,
            t,
            tLength,
            prev,
            &prevLength
        );
        
        if (result != E_OK) {
            return CCC_E_CRYPTO_FAILURE;
        }
#else
        /* 模拟实现 */
        prevLength = CCC_ECC_P256_KEY_SIZE;
        for (uint32 i = 0U; i < prevLength; i++) {
            prev[i] = (uint8)(prk[i] ^ t[i % tLength] ^ iteration);
        }
#endif
        
        /* 复制到输出 */
        uint32 copyLen = (okmLength - done < prevLength) ? (okmLength - done) : prevLength;
        (void)memcpy(&okm[done], prev, copyLen);
        done += copyLen;
        iteration++;
        
        /* 防止死循环 */
        if (iteration > 255U) {
            return CCC_E_CRYPTO_FAILURE;
        }
    }
    
    return CCC_E_OK;
}
