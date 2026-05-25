/**
 * @file CccDigitalKey.h
 * @brief CCC (Car Connectivity Consortium) 数字钥匙主头文件
 * 
 * 功能: 提供CCC数字钥匙核心功能的API声明
 * 符合CCC Digital Key规范 3.0
 * 
 * @author yuleASR Team
 * @version 1.0.0
 */

#ifndef CCC_DIGITAL_KEY_H
#define CCC_DIGITAL_KEY_H

/*==================================================================================================
*                                       版本信息
==================================================================================================*/
#define CCC_VENDOR_ID                           43
#define CCC_SW_MAJOR_VERSION                    1
#define CCC_SW_MINOR_VERSION                    0
#define CCC_SW_PATCH_VERSION                    0

/*==================================================================================================
*                                       包含头文件
==================================================================================================*/
#include "CccTypes.h"

/*==================================================================================================
*                                       API ID定义
==================================================================================================*/
#define CCC_API_INIT                            0x00U
#define CCC_API_DEINIT                          0x01U
#define CCC_API_PAIRING_START                   0x10U
#define CCC_API_PAIRING_COMPLETE                0x11U
#define CCC_API_AUTHENTICATION_START            0x20U
#define CCC_API_AUTHENTICATION_COMPLETE         0x21U
#define CCC_API_SESSION_ESTABLISH               0x30U
#define CCC_API_SESSION_CLOSE                   0x31U
#define CCC_API_ENCRYPT_MESSAGE                 0x40U
#define CCC_API_DECRYPT_MESSAGE                 0x41U
#define CCC_API_GENERATE_NONCE                  0x50U
#define CCC_API_VERIFY_CERTIFICATE              0x60U
#define CCC_API_SIGN_DATA                       0x61U
#define CCC_API_VERIFY_SIGNATURE                0x62U

/*==================================================================================================
*                                       宏定义
==================================================================================================*/
/**
 * @brief 开发错误检测
 */
#ifndef CCC_DEV_ERROR_DETECT
#define CCC_DEV_ERROR_DETECT                    (STD_ON)
#endif

/**
 * @brief 版本信息API
 */
#ifndef CCC_VERSION_INFO_API
#define CCC_VERSION_INFO_API                    (STD_ON)
#endif

/**
 * @brief 使用CSM服务
 */
#ifndef CCC_USE_CSM
#define CCC_USE_CSM                             (STD_ON)
#endif

/*==================================================================================================
*                                       外部函数声明
==================================================================================================*/

/**
 * @brief 初始化CCC数字钥匙模块
 * 
 * 初始化CSM服务、加载已存储的密钥和证书
 * 
 * @param config 配置指针
 * @return CCC_E_OK: 成功
 *         CCC_E_ALREADY_INITIALIZED: 已初始化
 *         CCC_E_CRYPTO_FAILURE: 加密服务初始化失败
 */
extern Ccc_ReturnType Ccc_Init(const Ccc_ConfigType* config);

/**
 * @brief 去初始化CCC数字钥匙模块
 * 
 * 清除会话、关闭密钥存储
 * 
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 */
extern Ccc_ReturnType Ccc_DeInit(void);

/**
 * @brief 获取版本信息
 * 
 * @param versioninfo 版本信息结构体指针
 */
#if (CCC_VERSION_INFO_API == STD_ON)
extern void Ccc_GetVersionInfo(Std_VersionInfoType* versioninfo);
#endif

/**
 * @brief 获取当前模式
 * 
 * @return 当前操作模式
 */
extern Ccc_ModeType Ccc_GetCurrentMode(void);

/**
 * @brief 获取会话状态
 * 
 * @param sessionState 状态输出指针
 * @return CCC_E_OK: 成功
 *         CCC_E_PARAM_POINTER: 参数指针为空
 */
extern Ccc_ReturnType Ccc_GetSessionState(Ccc_SessionStateType* sessionState);

/*==================================================================================================
*                                       配对相关API
==================================================================================================*/

/**
 * @brief 开始配对流程
 * 
 * 生成临时密钥对，发送配对请求
 * 
 * @param remoteDevice 远程设备标识
 * @param localPublicKey 本地公钥输出缓冲区
 * @param publicKeyLength 公钥长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_CRYPTO_FAILURE: 密钥生成失败
 *         CCC_E_BUFFER_TOO_SMALL: 缓冲区太小
 */
extern Ccc_ReturnType Ccc_PairingStart(
    const Ccc_DeviceIdType* remoteDevice,
    uint8* localPublicKey,
    uint32* publicKeyLength
);

/**
 * @brief 完成配对流程
 * 
 * 验证远程证书，完成密钥协商
 * 
 * @param remotePublicKey 远程公钥
 * @param remotePublicKeyLength 远程公钥长度
 * @param remoteCert 远程证书
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_CERT_INVALID: 证书无效
 *         CCC_E_CRYPTO_FAILURE: 密钥协商失败
 */
extern Ccc_ReturnType Ccc_PairingComplete(
    const uint8* remotePublicKey,
    uint32 remotePublicKeyLength,
    const Ccc_CertificateType* remoteCert
);

/*==================================================================================================
*                                       认证相关API
==================================================================================================*/

/**
 * @brief 开始认证流程
 * 
 * 生成挑战值，发送认证请求
 * 
 * @param challenge 挑战值输出缓冲区
 * @param challengeLength 挑战值长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_CRYPTO_FAILURE: 随机数生成失败
 */
extern Ccc_ReturnType Ccc_AuthenticationStart(
    uint8* challenge,
    uint32* challengeLength
);

/**
 * @brief 完成认证流程
 * 
 * 验证远程签名，生成会话密钥
 * 
 * @param remoteChallenge 远程挑战值
 * @param remoteSignature 远程签名
 * @param signatureLength 签名长度
 * @param localSignature 本地签名输出缓冲区
 * @param localSigLength 本地签名长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_SIGNATURE_INVALID: 签名验证失败
 *         CCC_E_AUTHENTICATION_FAILED: 认证失败
 */
extern Ccc_ReturnType Ccc_AuthenticationComplete(
    const uint8* remoteChallenge,
    const uint8* remoteSignature,
    uint32 signatureLength,
    uint8* localSignature,
    uint32* localSigLength
);

/**
 * @brief 验证证书
 * 
 * @param cert 证书数据
 * @param caCert CA证书 (如果为null则使用内置CA)
 * @return CCC_E_OK: 成功
 *         CCC_E_CERT_INVALID: 证书无效
 *         CCC_E_CERT_EXPIRED: 证书过期
 */
extern Ccc_ReturnType Ccc_VerifyCertificate(
    const Ccc_CertificateType* cert,
    const Ccc_CertificateType* caCert
);

/*==================================================================================================
*                                       会话管理API
==================================================================================================*/

/**
 * @brief 建立安全会话
 * 
 * 通过ECDH密钥协商和HKDF密钥派生建立安全通道
 * 
 * @param isInitiator 是否为会话发起方
 * @param remotePublicKey 远程公钥 (用于密钥协商)
 * @param remotePublicKeyLength 远程公钥长度
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_SESSION_NOT_ESTABLISHED: 会话未建立
 *         CCC_E_CRYPTO_FAILURE: 密钥派生失败
 */
extern Ccc_ReturnType Ccc_SessionEstablish(
    boolean isInitiator,
    const uint8* remotePublicKey,
    uint32 remotePublicKeyLength
);

/**
 * @brief 关闭安全会话
 * 
 * 清除会话密钥，释放资源
 * 
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 */
extern Ccc_ReturnType Ccc_SessionClose(void);

/*==================================================================================================
*                                       安全通信API
==================================================================================================*/

/**
 * @brief 加密消息
 * 
 * 使用AES-128-GCM加密消息
 * 
 * @param plaintext 明文数据
 * @param plaintextLength 明文长度
 * @param ciphertext 密文输出缓冲区
 * @param ciphertextLength 密文长度指针
 * @param authTag 认证标签输出缓冲区
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_SESSION_NOT_ESTABLISHED: 会话未建立
 *         CCC_E_CRYPTO_FAILURE: 加密失败
 */
extern Ccc_ReturnType Ccc_EncryptMessage(
    const uint8* plaintext,
    uint32 plaintextLength,
    uint8* ciphertext,
    uint32* ciphertextLength,
    uint8* authTag
);

/**
 * @brief 解密消息
 * 
 * 使用AES-128-GCM解密消息
 * 
 * @param ciphertext 密文数据
 * @param ciphertextLength 密文长度
 * @param authTag 认证标签
 * @param plaintext 明文输出缓冲区
 * @param plaintextLength 明文长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_SESSION_NOT_ESTABLISHED: 会话未建立
 *         CCC_E_CRYPTO_FAILURE: 解密失败
 *         CCC_E_MESSAGE_INVALID: 消息无效 (认证标签不匹配)
 */
extern Ccc_ReturnType Ccc_DecryptMessage(
    const uint8* ciphertext,
    uint32 ciphertextLength,
    const uint8* authTag,
    uint8* plaintext,
    uint32* plaintextLength
);

/**
 * @brief 创建安全消息
 * 
 * 创建完整的安全消息包（包含头部、载荷、认证标签）
 * 
 * @param messageType 消息类型
 * @param payload 载荷数据
 * @param payloadLength 载荷长度
 * @param message 消息输出缓冲区
 * @return CCC_E_OK: 成功
 *         CCC_E_SESSION_NOT_ESTABLISHED: 会话未建立
 *         CCC_E_BUFFER_TOO_SMALL: 缓冲区太小
 */
extern Ccc_ReturnType Ccc_CreateSecureMessage(
    Ccc_MessageType messageType,
    const uint8* payload,
    uint32 payloadLength,
    Ccc_SecureMessageType* message
);

/**
 * @brief 解析安全消息
 * 
 * 验证并解密安全消息包
 * 
 * @param message 消息数据
 * @param payload 载荷输出缓冲区
 * @param payloadLength 载荷长度指针
 * @param messageType 消息类型输出指针
 * @return CCC_E_OK: 成功
 *         CCC_E_SESSION_NOT_ESTABLISHED: 会话未建立
 *         CCC_E_MESSAGE_INVALID: 消息无效
 *         CCC_E_REPLAY_DETECTED: 检测到重放攻击
 */
extern Ccc_ReturnType Ccc_ParseSecureMessage(
    const Ccc_SecureMessageType* message,
    uint8* payload,
    uint32* payloadLength,
    Ccc_MessageType* messageType
);

/*==================================================================================================
*                                       辅助功能API
==================================================================================================*/

/**
 * @brief 生成随机数
 * 
 * 生成安全随机数用于挑战值、随机数等
 * 
 * @param randomData 输出缓冲区
 * @param length 需要的长度
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_CRYPTO_FAILURE: 随机数生成失败
 */
extern Ccc_ReturnType Ccc_GenerateRandom(uint8* randomData, uint32 length);

/**
 * @brief 计算哈希值
 * 
 * @param data 输入数据
 * @param dataLength 数据长度
 * @param hash 哈希输出缓冲区
 * @param hashLength 哈希长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_CRYPTO_FAILURE: 哈希计算失败
 */
extern Ccc_ReturnType Ccc_CalculateHash(
    const uint8* data,
    uint32 dataLength,
    uint8* hash,
    uint32* hashLength
);

/**
 * @brief 对数据进行签名
 * 
 * 使用ECDSA P-256对数据进行签名
 * 
 * @param data 待签名数据
 * @param dataLength 数据长度
 * @param signature 签名输出缓冲区
 * @param signatureLength 签名长度指针
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_KEY_NOT_FOUND: 密钥未找到
 *         CCC_E_CRYPTO_FAILURE: 签名失败
 */
extern Ccc_ReturnType Ccc_SignData(
    const uint8* data,
    uint32 dataLength,
    uint8* signature,
    uint32* signatureLength
);

/**
 * @brief 验证签名
 * 
 * 使用ECDSA P-256验证签名
 * 
 * @param data 原始数据
 * @param dataLength 数据长度
 * @param signature 签名数据
 * @param signatureLength 签名长度
 * @param publicKey 公钥 (如果为null则使用已存储的公钥)
 * @return CCC_E_OK: 成功
 *         CCC_E_NOT_INITIALIZED: 未初始化
 *         CCC_E_SIGNATURE_INVALID: 签名无效
 *         CCC_E_CRYPTO_FAILURE: 验证失败
 */
extern Ccc_ReturnType Ccc_VerifySignature(
    const uint8* data,
    uint32 dataLength,
    const uint8* signature,
    uint32 signatureLength,
    const uint8* publicKey
);

#endif /* CCC_DIGITAL_KEY_H */
