/**
 * @file CccAuthentication.c
 * @brief CCC (Car Connectivity Consortium) 身份认证实现
 * 
 * 功能: 实现身份认证流程，包括证书验证、挑战-响应机制
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
 * │ 此文件的身份认证功能对应 yuleDKCS CCC 协议栈:                            │
 * │                                                                         │
 * │ Ccc_AuthenticationStart() 对应:                                         │
 * │   error_t ccc_generate_challenge(uint8_t *challenge);                    │
 * │                                                                         │
 * │ Ccc_VerifyCertificate() 对应:                                           │
 * │   error_t ccc_verify_certificate(cert, trusted_pubkey);                  │
 * │   error_t ccc_validate_cert_chain(cert_chain, trusted_root);             │
 * │                                                                         │
 * │ Ccc_GenerateAuthenticationResponse() 对应内部挑战-响应序列:              │
 * │   1. ccc_generate_challenge(&challenge)                                  │
 * │   2. sec_sign(challenge, len, signature, &sig_len)                       │
 * │   3. sec_verify(challenge, len, signature, sig_len)                      │
 * │                                                                         │
 * │ yuleASR BSW 集成说明:                                                   │
 * │ - Csm_SignatureVerify(): 验证证书签名/认证签名                           │
 * │ - Csm_SignatureGenerate(): 生成本地签名                                  │
 * │ - yuleASR DCM 可集成诊断服务:                                            │
 * │   #include \"Dcm.h\"                                                       │
 * │   Dcm_ProcessRequest(DCM_SID_SECURITY_ACCESS, ...);                      │
 * │ - yuleASR OS 超时管理:                                                   │
 * │   Os_SetRelAlarm(ALARM_CCC_CHALLENGE_TIMEOUT, 30000, 0, callback);       │
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
 * @brief 最大挑战尝试次数
 */
#define CCC_MAX_CHALLENGE_ATTEMPTS              3U

/**
 * @brief 挑战值有效期 (毫秒)
 */
#define CCC_CHALLENGE_TIMEOUT_MS                30000U

/*==================================================================================================
*                                       全局变量
==================================================================================================*/
extern Ccc_RuntimeDataType* Ccc_GetRuntimeData(void);
extern const Ccc_ConfigType* Ccc_GetConfig(void);

/*==================================================================================================
*                                       本地函数声明
==================================================================================================*/
static Ccc_ReturnType Ccc_ValidateCertFormat(const Ccc_CertificateType* cert);
static Ccc_ReturnType Ccc_VerifyCertSignature(const Ccc_CertificateType* cert);
static Ccc_ReturnType Ccc_VerifyCertChain(const Ccc_CertificateType* cert, const Ccc_CertificateType* caCert);
static Ccc_ReturnType Ccc_CheckCertValidity(const Ccc_CertificateType* cert);

/*==================================================================================================
*                                       认证API实现
==================================================================================================*/

/**
 * @brief 开始认证流程
 */
Ccc_ReturnType Ccc_AuthenticationStart(
    uint8* challenge,
    uint32* challengeLength
)
{
    Ccc_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    
    CCC_CHECK_POINTER(challenge);
    CCC_CHECK_POINTER(challengeLength);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 检查缓冲区大小 */
    if (*challengeLength < CCC_CHALLENGE_SIZE) {
        return CCC_E_BUFFER_TOO_SMALL;
    }
    
    /* 检查当前状态 */
    if (runtime->currentMode != CCC_MODE_AUTHENTICATION && 
        runtime->currentMode != CCC_MODE_OPERATIONAL) {
        return CCC_E_PARAM_MODE;
    }
    
    /* 生成随机挑战值 */
    result = Ccc_GenerateRandom(challenge, CCC_CHALLENGE_SIZE);
    if (result != CCC_E_OK) {
        return result;
    }
    
    *challengeLength = CCC_CHALLENGE_SIZE;
    
    /* 保存挑战值到会话上下文 */
    /* 实际实现中应使用安全存储 */
    
    /* 更新会话状态 */
    runtime->session.state = CCC_SESSION_STATE_AUTHENTICATING;
    
    return CCC_E_OK;
}

/**
 * @brief 完成认证流程
 */
Ccc_ReturnType Ccc_AuthenticationComplete(
    const uint8* remoteChallenge,
    const uint8* remoteSignature,
    uint32 signatureLength,
    uint8* localSignature,
    uint32* localSigLength
)
{
    Ccc_ReturnType result;
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    const Ccc_ConfigType* config = Ccc_GetConfig();
    
    CCC_CHECK_POINTER(remoteChallenge);
    CCC_CHECK_POINTER(remoteSignature);
    CCC_CHECK_POINTER(localSignature);
    CCC_CHECK_POINTER(localSigLength);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    if (runtime->session.state != CCC_SESSION_STATE_AUTHENTICATING) {
        return CCC_E_PARAM_MODE;
    }
    
    /* 验证远程签名 */
    result = Ccc_VerifySignature(
        remoteChallenge,
        CCC_CHALLENGE_SIZE,
        remoteSignature,
        signatureLength,
        runtime->pairingData.remoteCert.certificate
    );
    
    if (result != CCC_E_OK) {
        /* 认证失败，重置状态 */
        runtime->session.state = CCC_SESSION_STATE_INACTIVE;
        runtime->currentMode = CCC_MODE_AUTHENTICATION;
        return CCC_E_AUTHENTICATION_FAILED;
    }
    
    /* 生成本地签名 */
    result = Ccc_SignData(
        remoteChallenge,
        CCC_CHALLENGE_SIZE,
        localSignature,
        localSigLength
    );
    
    if (result != CCC_E_OK) {
        runtime->session.state = CCC_SESSION_STATE_INACTIVE;
        return result;
    }
    
    /* 认证成功，更新状态 */
    runtime->session.state = CCC_SESSION_STATE_ACTIVE;
    runtime->currentMode = CCC_MODE_OPERATIONAL;
    
    return CCC_E_OK;
}

/**
 * @brief 验证证书
 */
Ccc_ReturnType Ccc_VerifyCertificate(
    const Ccc_CertificateType* cert,
    const Ccc_CertificateType* caCert
)
{
    Ccc_ReturnType result;
    
    CCC_CHECK_POINTER(cert);
    
    if (runtime == NULL || !runtime->initialized) {
        return CCC_E_NOT_INITIALIZED;
    }
    
    /* 验证证书格式 */
    result = Ccc_ValidateCertFormat(cert);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 检查证书有效期 */
    result = Ccc_CheckCertValidity(cert);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 验证证书签名 */
    result = Ccc_VerifyCertSignature(cert);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 如果提供了CA证书，验证证书链 */
    if (caCert != NULL) {
        result = Ccc_VerifyCertChain(cert, caCert);
        if (result != CCC_E_OK) {
            return result;
        }
    }
    
    return CCC_E_OK;
}

/*==================================================================================================
*                                       本地函数实现
==================================================================================================*/

/**
 * @brief 验证证书格式
 */
static Ccc_ReturnType Ccc_ValidateCertFormat(const Ccc_CertificateType* cert)
{
    /* 检查证书长度 */
    if (cert->certLength == 0U || cert->certLength > CCC_MAX_CERTIFICATE_SIZE) {
        return CCC_E_CERT_INVALID;
    }
    
    /* 检查证书有效标志 */
    if (!cert->valid) {
        return CCC_E_CERT_INVALID;
    }
    
    /* 检查主题ID */
    boolean subjectIdValid = FALSE;
    for (uint32 i = 0U; i < CCC_DEVICE_ID_SIZE; i++) {
        if (cert->subjectId[i] != 0U) {
            subjectIdValid = TRUE;
            break;
        }
    }
    
    if (!subjectIdValid) {
        return CCC_E_CERT_INVALID;
    }
    
    /* 检查颁发者ID */
    boolean issuerIdValid = FALSE;
    for (uint32 i = 0U; i < CCC_DEVICE_ID_SIZE; i++) {
        if (cert->issuerId[i] != 0U) {
            issuerIdValid = TRUE;
            break;
        }
    }
    
    if (!issuerIdValid) {
        return CCC_E_CERT_INVALID;
    }
    
    /* 检查证书版本 (简化检查 - 实际应用中需要解析X.509结构) */
    if (cert->certificate[0] != 0x30) {
        /* X.509证书通常以30h开始 */
        return CCC_E_CERT_INVALID;
    }
    
    return CCC_E_OK;
}

/**
 * @brief 验证证书签名
 */
static Ccc_ReturnType Ccc_VerifyCertSignature(const Ccc_CertificateType* cert)
{
    Std_ReturnType result;
    boolean verifyResult = FALSE;
    const Ccc_ConfigType* config = Ccc_GetConfig();
    
    if (cert == NULL) {
        return CCC_E_PARAM_POINTER;
    }
    
#if (CCC_USE_CSM == STD_ON)
    /* 在实际应用中，需要:
     * 1. 解析证书获取TBS数据 (待签名内容)
     * 2. 获取签名值
     * 3. 使用颁发者公钥验证签名
     */
    
    /* 这里模拟签名验证 - 实际实现需要解析X.509结构 */
    result = Csm_SignatureVerify(
        config->csmJobId,
        CSM_OPERATIONMODE_SINGLECALL,
        cert->certificate,
        cert->certLength - 64U,  /* 假设签名长度64字节 */
        &cert->certificate[cert->certLength - 64U],
        64U,
        &verifyResult
    );
    
    if (result != E_OK) {
        return CCC_E_CRYPTO_FAILURE;
    }
    
    if (!verifyResult) {
        return CCC_E_CERT_INVALID;
    }
#else
    /* 模拟实现 - 简单检查 */
    /* 实际应用中需要完整的X.509解析和签名验证 */
    if (cert->certLength < 100U) {
        return CCC_E_CERT_INVALID;
    }
#endif
    
    return CCC_E_OK;
}

/**
 * @brief 验证证书链
 */
static Ccc_ReturnType Ccc_VerifyCertChain(
    const Ccc_CertificateType* cert,
    const Ccc_CertificateType* caCert
)
{
    Ccc_ReturnType result;
    
    if (cert == NULL || caCert == NULL) {
        return CCC_E_PARAM_POINTER;
    }
    
    /* 检查CA证书是否有效 */
    if (!caCert->valid) {
        return CCC_E_CERT_INVALID;
    }
    
    /* 验证颁发者ID是否匹配 */
    boolean issuerMatch = TRUE;
    for (uint32 i = 0U; i < CCC_DEVICE_ID_SIZE; i++) {
        if (cert->issuerId[i] != caCert->subjectId[i]) {
            issuerMatch = FALSE;
            break;
        }
    }
    
    if (!issuerMatch) {
        return CCC_E_CERT_INVALID;
    }
    
    /* 验证CA证书签名 */
    result = Ccc_VerifyCertSignature(caCert);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 验证用户证书的签名 (使用CA公钥) */
    /* 实际实现中需要使用CA的公钥验证用户证书的签名 */
    
    return CCC_E_OK;
}

/**
 * @brief 检查证书有效期
 */
static Ccc_ReturnType Ccc_CheckCertValidity(const Ccc_CertificateType* cert)
{
    /* 获取当前时间 */
    /* 实际应用中应使用RTC或其他时间源 */
    uint32 currentTime = 0U;  /* 应该获取实际时间 */
    
    /* 检查有效期起始 */
    if (cert->validFrom > currentTime && currentTime != 0U) {
        return CCC_E_CERT_INVALID;
    }
    
    /* 检查有效期截止 */
    if (cert->validUntil < currentTime && currentTime != 0U) {
        return CCC_E_CERT_EXPIRED;
    }
    
    /* 检查证书是否过期 (简单检查 - 如果validUntil为0则认为无效) */
    if (cert->validUntil == 0U) {
        return CCC_E_CERT_INVALID;
    }
    
    return CCC_E_OK;
}

/*==================================================================================================
*                                       挑战-响应机制实现
==================================================================================================*/

/**
 * @brief 生成认证响应
 * 
 * 基于挑战值生成认证响应数据
 * 
 * @param challenge 挑战值
 * @param challengeLength 挑战值长度
 * @param response 响应输出缓冲区
 * @param responseLength 响应长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_PARAM_POINTER: 参数指针为空
 *         CCC_E_CRYPTO_FAILURE: 加密操作失败
 */
Ccc_ReturnType Ccc_GenerateAuthenticationResponse(
    const uint8* challenge,
    uint32 challengeLength,
    uint8* response,
    uint32* responseLength
)
{
    Ccc_ReturnType result;
    uint8 signature[CSM_MAX_SIGNATURE_LENGTH];
    uint32 signatureLength = CSM_MAX_SIGNATURE_LENGTH;
    
    CCC_CHECK_POINTER(challenge);
    CCC_CHECK_POINTER(response);
    CCC_CHECK_POINTER(responseLength);
    
    if (challengeLength < CCC_CHALLENGE_SIZE) {
        return CCC_E_PARAM_LENGTH;
    }
    
    /* 对挑战值进行签名 */
    result = Ccc_SignData(challenge, challengeLength, signature, &signatureLength);
    if (result != CCC_E_OK) {
        return result;
    }
    
    /* 构建响应 */
    if (*responseLength < (signatureLength + CCC_DEVICE_ID_SIZE)) {
        return CCC_E_BUFFER_TOO_SMALL;
    }
    
    /* 复制设备ID */
    Ccc_RuntimeDataType* runtime = Ccc_GetRuntimeData();
    if (runtime != NULL) {
        (void)memcpy(response, runtime->pairingData.localDevice.deviceId, CCC_DEVICE_ID_SIZE);
    }
    
    /* 复制签名 */
    (void)memcpy(&response[CCC_DEVICE_ID_SIZE], signature, signatureLength);
    *responseLength = CCC_DEVICE_ID_SIZE + signatureLength;
    
    return CCC_E_OK;
}

/**
 * @brief 验证认证响应
 * 
 * 验证远程设备的认证响应
 * 
 * @param challenge 原始挑战值
 * @param challengeLength 挑战值长度
 * @param response 响应数据
 * @param responseLength 响应长度
 * @return CCC_E_OK: 验证通过
 *         CCC_E_SIGNATURE_INVALID: 签名无效
 *         CCC_E_AUTHENTICATION_FAILED: 认证失败
 */
Ccc_ReturnType Ccc_VerifyAuthenticationResponse(
    const uint8* challenge,
    uint32 challengeLength,
    const uint8* response,
    uint32 responseLength
)
{
    Ccc_ReturnType result;
    const uint8* deviceId;
    const uint8* signature;
    uint32 signatureLength;
    
    CCC_CHECK_POINTER(challenge);
    CCC_CHECK_POINTER(response);
    
    if (challengeLength < CCC_CHALLENGE_SIZE) {
        return CCC_E_PARAM_LENGTH;
    }
    
    if (responseLength < (CCC_DEVICE_ID_SIZE + 64U)) {
        return CCC_E_PARAM_LENGTH;
    }
    
    /* 解析响应 */
    deviceId = response;
    signature = &response[CCC_DEVICE_ID_SIZE];
    signatureLength = responseLength - CCC_DEVICE_ID_SIZE;
    
    /* 验证设备ID (可选 - 检查是否在已配对设备列表中) */
    (void)deviceId;  /* 实际应用中应验证设备ID */
    
    /* 验证签名 */
    result = Ccc_VerifySignature(
        challenge,
        challengeLength,
        signature,
        signatureLength,
        NULL  /* 使用已存储的公钥 */
    );
    
    if (result != CCC_E_OK) {
        return CCC_E_AUTHENTICATION_FAILED;
    }
    
    return CCC_E_OK;
}
