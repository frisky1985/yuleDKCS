/**
 * @file CccTypes.h
 * @brief CCC (Car Connectivity Consortium) 数字钥匙类型定义
 * 
 * 功能: 定义CCC数字钥匙协议使用的所有数据类型、枚举和结构体
 * 符合CCC Digital Key规范 3.0
 * 
 * @author yuleASR Team
 * @version 1.0.0
 */

#ifndef CCC_TYPES_H
#define CCC_TYPES_H

/*==================================================================================================
*                                       版本信息
==================================================================================================*/
#define CCC_TYPES_VENDOR_ID                     43
#define CCC_TYPES_SW_MAJOR_VERSION              1
#define CCC_TYPES_SW_MINOR_VERSION              0
#define CCC_TYPES_SW_PATCH_VERSION              0

/*==================================================================================================
*                                       包含头文件
==================================================================================================*/
#include "Std_Types.h"
#include "Csm_Types.h"

/*==================================================================================================
*                                       宏定义
==================================================================================================*/
/**
 * @brief CCC版本信息
 */
#define CCC_PROTOCOL_VERSION_MAJOR              3
#define CCC_PROTOCOL_VERSION_MINOR              0

/**
 * @brief 密钥长度定义
 */
#define CCC_ECC_P256_KEY_SIZE                   32U     /* P-256曲线密钥长度 */
#define CCC_ECC_P256_PUBLIC_KEY_SIZE            65U     /* P-256公钥长度 (0x04 + x + y) */
#define CCC_AES_KEY_SIZE                        16U     /* AES-128密钥长度 */
#define CCC_AES_IV_SIZE                         12U     /* AES-GCM IV长度 */
#define CCC_AES_TAG_SIZE                        16U     /* AES-GCM认证标签长度 */
#define CCC_HKDF_INFO_SIZE                      32U     /* HKDF信息长度 */
#define CCC_CHALLENGE_SIZE                      32U     /* 挑战值长度 */
#define CCC_SESSION_ID_SIZE                     16U     /* 会话ID长度 */
#define CCC_NONCE_SIZE                          16U     /* 随机数长度 */

/**
 * @brief 证书相关定义
 */
#define CCC_MAX_CERTIFICATE_SIZE                1024U   /* 最大证书长度 */
#define CCC_MAX_CERT_CHAIN_SIZE                 2048U   /* 最大证书链长度 */

/**
 * @brief 消息相关定义
 */
#define CCC_MAX_MESSAGE_SIZE                    512U    /* 最大消息长度 */
#define CCC_MAX_PAYLOAD_SIZE                    256U    /* 最大载荷长度 */

/**
 * @brief 设备标识长度
 */
#define CCC_DEVICE_ID_SIZE                      16U     /* 设备ID长度 */

/*==================================================================================================
*                                       错误码定义
==================================================================================================*/
/**
 * @brief CCC错误码
 */
typedef enum
{
    CCC_E_OK = 0,                       /* 成功 */
    CCC_E_NOT_INITIALIZED,              /* 未初始化 */
    CCC_E_ALREADY_INITIALIZED,          /* 已初始化 */
    CCC_E_PARAM_POINTER,                /* 参数指针错误 */
    CCC_E_PARAM_LENGTH,                 /* 参数长度错误 */
    CCC_E_PARAM_MODE,                   /* 参数模式错误 */
    CCC_E_CRYPTO_FAILURE,               /* 加密操作失败 */
    CCC_E_KEY_NOT_FOUND,                /* 密钥未找到 */
    CCC_E_KEY_INVALID,                  /* 密钥无效 */
    CCC_E_CERT_INVALID,                 /* 证书无效 */
    CCC_E_CERT_EXPIRED,                 /* 证书过期 */
    CCC_E_SIGNATURE_INVALID,            /* 签名无效 */
    CCC_E_AUTHENTICATION_FAILED,        /* 认证失败 */
    CCC_E_SESSION_NOT_ESTABLISHED,      /* 会话未建立 */
    CCC_E_SESSION_EXPIRED,              /* 会话过期 */
    CCC_E_REPLAY_DETECTED,              /* 检测到重放攻击 */
    CCC_E_MESSAGE_INVALID,              /* 消息无效 */
    CCC_E_BUFFER_TOO_SMALL,             /* 缓冲区太小 */
    CCC_E_NOT_SUPPORTED,                /* 不支持的操作 */
    CCC_E_INTERNAL_ERROR                /* 内部错误 */
} Ccc_ReturnType;

/*==================================================================================================
*                                       枚举类型定义
==================================================================================================*/
/**
 * @brief CCC操作模式
 */
typedef enum
{
    CCC_MODE_UNINITIALIZED = 0,         /* 未初始化 */
    CCC_MODE_PAIRING,                   /* 配对模式 */
    CCC_MODE_AUTHENTICATION,            /* 认证模式 */
    CCC_MODE_OPERATIONAL,               /* 操作模式 */
    CCC_MODE_ERROR                      /* 错误模式 */
} Ccc_ModeType;

/**
 * @brief 会话状态
 */
typedef enum
{
    CCC_SESSION_STATE_INACTIVE = 0,     /* 未激活 */
    CCC_SESSION_STATE_NEGOTIATING,      /* 协商中 */
    CCC_SESSION_STATE_AUTHENTICATING,   /* 认证中 */
    CCC_SESSION_STATE_ACTIVE,           /* 激活 */
    CCC_SESSION_STATE_CLOSING           /* 关闭中 */
} Ccc_SessionStateType;

/**
 * @brief 密钥类型
 */
typedef enum
{
    CCC_KEY_TYPE_EPHEMERAL = 0,         /* 临时密钥 */
    CCC_KEY_TYPE_LONG_TERM,             /* 长期密钥 */
    CCC_KEY_TYPE_SESSION,               /* 会话密钥 */
    CCC_KEY_TYPE_MASTER                 /* 主密钥 */
} Ccc_KeyType;

/**
 * @brief 设备角色
 */
typedef enum
{
    CCC_ROLE_VEHICLE = 0,               /* 车辆端 */
    CCC_ROLE_MOBILE_DEVICE,             /* 移动设备端 */
    CCC_ROLE_SERVER                     /* 服务器端 */
} Ccc_RoleType;

/**
 * @brief 消息类型
 */
typedef enum
{
    CCC_MSG_NONE = 0,                   /* 无 */
    CCC_MSG_PAIRING_REQUEST,            /* 配对请求 */
    CCC_MSG_PAIRING_RESPONSE,           /* 配对响应 */
    CCC_MSG_AUTHENTICATION_REQUEST,     /* 认证请求 */
    CCC_MSG_AUTHENTICATION_RESPONSE,    /* 认证响应 */
    CCC_MSG_CHALLENGE,                  /* 挑战 */
    CCC_MSG_CHALLENGE_RESPONSE,         /* 挑战响应 */
    CCC_MSG_SECURE_MESSAGE,             /* 安全消息 */
    CCC_MSG_ERROR                       /* 错误消息 */
} Ccc_MessageType;

/*==================================================================================================
*                                       结构体定义
==================================================================================================*/
/**
 * @brief 设备标识
 */
typedef struct
{
    uint8 deviceId[CCC_DEVICE_ID_SIZE];     /* 设备唯一标识 */
    Ccc_RoleType role;                      /* 设备角色 */
    uint16 protocolVersion;                 /* 协议版本 */
} Ccc_DeviceIdType;

/**
 * @brief ECC P-256密钥对
 */
typedef struct
{
    uint8 privateKey[CCC_ECC_P256_KEY_SIZE];            /* 私钥 */
    uint8 publicKey[CCC_ECC_P256_PUBLIC_KEY_SIZE];      /* 公钥 (未压缩格式) */
    boolean valid;                                      /* 密钥有效标志 */
} Ccc_EccKeyPairType;

/**
 * @brief 对称密钥
 */
typedef struct
{
    uint8 key[CCC_AES_KEY_SIZE];            /* 密钥数据 */
    uint8 iv[CCC_AES_IV_SIZE];              /* 初始化向量 */
    boolean valid;                          /* 密钥有效标志 */
} Ccc_SymmetricKeyType;

/**
 * @brief 派生密钥数据
 */
typedef struct
{
    uint8 sharedSecret[CCC_ECC_P256_KEY_SIZE];  /* 共享密钥 */
    uint8 salt[CCC_ECC_P256_KEY_SIZE];          /* 盐值 */
    uint8 info[CCC_HKDF_INFO_SIZE];             /* 信息 */
    uint32 infoLength;                          /* 信息长度 */
} Ccc_DerivationDataType;

/**
 * @brief 会话密钥
 */
typedef struct
{
    Ccc_SymmetricKeyType encryptionKey;         /* 加密密钥 */
    Ccc_SymmetricKeyType macKey;                /* MAC密钥 */
    uint32 keyId;                               /* 密钥ID */
    boolean valid;                              /* 密钥有效标志 */
} Ccc_SessionKeyType;

/**
 * @brief 会话上下文
 */
typedef struct
{
    uint8 sessionId[CCC_SESSION_ID_SIZE];       /* 会话ID */
    Ccc_SessionStateType state;                 /* 会话状态 */
    Ccc_EccKeyPairType ephemeralKey;            /* 临时密钥对 */
    Ccc_SessionKeyType sessionKeys;             /* 会话密钥 */
    uint8 localNonce[CCC_NONCE_SIZE];           /* 本地随机数 */
    uint8 remoteNonce[CCC_NONCE_SIZE];          /* 远程随机数 */
    uint32 sequenceNumber;                      /* 序列号 (防重放) */
    uint32 timestamp;                           /* 时间戳 */
    boolean isInitiator;                        /* 是否为发起方 */
} Ccc_SessionContextType;

/**
 * @brief 证书信息
 */
typedef struct
{
    uint8 certificate[CCC_MAX_CERTIFICATE_SIZE];    /* 证书数据 */
    uint32 certLength;                              /* 证书长度 */
    uint8 issuerId[CCC_DEVICE_ID_SIZE];             /* 颁发者ID */
    uint8 subjectId[CCC_DEVICE_ID_SIZE];            /* 主题ID */
    uint32 validFrom;                               /* 有效期起始 */
    uint32 validUntil;                              /* 有效期截止 */
    boolean valid;                                  /* 证书有效标志 */
} Ccc_CertificateType;

/**
 * @brief 签名数据
 */
typedef struct
{
    uint8 signature[CSM_MAX_SIGNATURE_LENGTH];  /* 签名数据 */
    uint32 signatureLength;                     /* 签名长度 */
    uint8 challenge[CCC_CHALLENGE_SIZE];        /* 挑战值 */
    uint32 algorithm;                           /* 签名算法 */
} Ccc_SignatureDataType;

/**
 * @brief 安全消息头
 */
typedef struct
{
    uint8 sessionId[CCC_SESSION_ID_SIZE];       /* 会话ID */
    uint32 sequenceNumber;                      /* 序列号 */
    uint32 timestamp;                           /* 时间戳 */
    uint16 messageType;                         /* 消息类型 */
    uint16 payloadLength;                       /* 载荷长度 */
} Ccc_MessageHeaderType;

/**
 * @brief 安全消息
 */
typedef struct
{
    Ccc_MessageHeaderType header;               /* 消息头 */
    uint8 payload[CCC_MAX_PAYLOAD_SIZE];        /* 载荷 */
    uint8 authTag[CCC_AES_TAG_SIZE];            /* 认证标签 */
} Ccc_SecureMessageType;

/**
 * @brief 配对数据
 */
typedef struct
{
    Ccc_DeviceIdType localDevice;               /* 本地设备 */
    Ccc_DeviceIdType remoteDevice;              /* 远程设备 */
    Ccc_EccKeyPairType ephemeralKey;            /* 临时密钥对 */
    uint8 localRandom[CCC_NONCE_SIZE];          /* 本地随机数 */
    uint8 remoteRandom[CCC_NONCE_SIZE];         /* 远程随机数 */
    Ccc_CertificateType localCert;              /* 本地证书 */
    Ccc_CertificateType remoteCert;             /* 远程证书 */
} Ccc_PairingDataType;

/**
 * @brief CCC配置
 */
typedef struct
{
    Ccc_DeviceIdType deviceId;                  /* 设备标识 */
    Ccc_RoleType role;                          /* 设备角色 */
    uint32 keyStorageId;                        /* 密钥存储ID */
    uint32 certStorageId;                       /* 证书存储ID */
    uint32 csmKeyId;                            /* CSM密钥ID */
    uint32 csmJobId;                            /* CSM作业ID */
    boolean useSecureStorage;                   /* 使用安全存储 */
} Ccc_ConfigType;

/**
 * @brief CCC运行时数据
 */
typedef struct
{
    Ccc_ModeType currentMode;                   /* 当前模式 */
    Ccc_SessionContextType session;             /* 会话上下文 */
    Ccc_PairingDataType pairingData;            /* 配对数据 */
    uint32 sessionCounter;                      /* 会话计数器 */
    boolean initialized;                        /* 初始化标志 */
} Ccc_RuntimeDataType;

#endif /* CCC_TYPES_H */
