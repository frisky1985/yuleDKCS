/******************************************************************************
 * @file    iccoa_certificate.h
 * @brief   ICCOA 数字钥匙证书 X.509 序列化
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-15
 * 
 * @note    
 *          - ICCOA/T 002-2024 数字钥匙技术规范第6章证书要求
 *          - X.509 V3 格式，DER 编码
 *          - ECDSA P-256 + SHA-256 签名算法
 *          - 支持 CA模式/非CA模式 双模式
 ******************************************************************************/

#ifndef ICCOA_CERTIFICATE_H
#define ICCOA_CERTIFICATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "iccoa.h"
#include "dkcs.h"

/******************************************************************************
 * ICCOA 证书类型定义 (符合 ICCOA DK 4.0)
 ******************************************************************************/
typedef enum {
    ICCOA_CERT_TYPE_VEHICLE_OEM_CA   = 0x01,  /* A: 车企服务器 CA 证书 */
    ICCOA_CERT_TYPE_VEHICLE          = 0x02,  /* B: 车证书 */
    ICCOA_CERT_TYPE_OWNER_DK         = 0x03,  /* C: 车主数字钥匙证书 */
    ICCOA_CERT_TYPE_MID_SHARE        = 0x04,  /* D: 中间分享证书 */
    ICCOA_CERT_TYPE_SHARED_DK        = 0x05,  /* E: 好友数字钥匙证书 */
    ICCOA_CERT_TYPE_SHARED_DK_V2     = 0x0B,  /* K: 好友证书 V2 (非CA模式) */
} iccoa_cert_type_t;

/******************************************************************************
 * ICCOA 证书模式
 ******************************************************************************/
typedef enum {
    ICCOA_CERT_MODE_CA     = 0x00,  /* CA 模式: 传统 PKI 链 */
    ICCOA_CERT_MODE_NON_CA = 0x01,  /* 非 CA 模式: 简化链 (ICCOA 4.0 新增) */
} iccoa_cert_mode_t;

/******************************************************************************
 * 证书大小限制 (字节)
 ******************************************************************************/
#define ICCOA_MAX_OWNER_CERT_SIZE       700   /* 车主证书最大 700 字节 */
#define ICCOA_MAX_MIDSHARE_CERT_SIZE    700   /* 中间分享证书最大 700 字节 */
#define ICCOA_MAX_SHARED_CERT_SIZE      700   /* 好友证书最大 700 字节 */
#define ICCOA_MAX_SHARED_CERT_V2_SIZE   800   /* 好友证书 V2 最大 800 字节 */

/******************************************************************************
 * ICCOA OID 定义 (1.3.6.1.4.1.59129 - ICCOA 私有企业 OID)
 ******************************************************************************/
/* 证书类型 OID */
extern const uint8_t OID_ICCOA_CERT_TYPE_VEHICLE_OEM_CA[];
extern const uint8_t OID_ICCOA_CERT_TYPE_VEHICLE[];
extern const uint8_t OID_ICCOA_CERT_TYPE_OWNER_DK[];
extern const uint8_t OID_ICCOA_CERT_TYPE_MID_SHARE[];
extern const uint8_t OID_ICCOA_CERT_TYPE_SHARED_DK[];

/* 扩展信息 OID */
extern const uint8_t OID_ICCOA_VEHICLE_OEM_ID[];    /* 车企唯一标识符 */
extern const uint8_t OID_ICCOA_VEHICLE_ID[];        /* 车辆唯一标识符 */
extern const uint8_t OID_ICCOA_DIGITAL_KEY_ID[];    /* 数字钥匙唯一标识符 */
extern const uint8_t OID_ICCOA_DIGITAL_KEY_AUTH[];  /* 数字钥匙权限 */
extern const uint8_t OID_ICCOA_DIGITAL_KEY_MODE[];  /* 证书体系模式 */

#define OID_ICCOA_LEN                   9   /* OID长度: 1.3.6.1.4.1.59129.x */

/******************************************************************************
 * ICCOA 证书结构
 ******************************************************************************/
typedef struct {
    /* 证书类型 */
    iccoa_cert_type_t type;
    iccoa_cert_mode_t mode;
    
    /* 版本和序列号 */
    uint8_t version;                    /* X.509 版本 (V3 = 2) */
    uint8_t serial_number[16];          /* 序列号 */
    
    /* 颁发者和主体 */
    uint8_t issuer_id[16];              /* 颁发者 ID */
    uint8_t subject_id[16];             /* 主体 ID (KeyID 或 VehicleID) */
    
    /* 有效期 */
    uint32_t valid_from;                /* 生效时间 (Unix 时间戳) */
    uint32_t valid_until;               /* 过期时间 (Unix 时间戳) */
    
    /* 密钥 */
    uint8_t public_key[65];             /* EC P-256 公钥 (未压缩格式) */
    uint8_t signature[64];              /* ECDSA 签名 (r || s) */
    
    /* ICCOA 特定字段 */
    uint8_t vehicle_oem_id[2];          /* 车企唯一标识符 (2字节) */
    uint8_t vehicle_id[16];             /* 车辆唯一标识符 (16字节) */
    uint8_t key_id[16];                 /* 数字钥匙唯一标识符 (16字节) */
    uint32_t permissions;               /* 数字钥匙权限位图 */
    
    /* DER 编码缓冲区 */
    uint8_t der_data[1024];             /* DER 编码数据 */
    uint16_t der_len;                   /* DER 数据长度 */
} iccoa_certificate_t;

/******************************************************************************
 * ICCOA 证书链结构
 ******************************************************************************/
#define ICCOA_MAX_CERT_CHAIN_LEN        4   /* 最大证书链长度 */

typedef struct {
    iccoa_certificate_t certs[ICCOA_MAX_CERT_CHAIN_LEN];
    uint8_t cert_count;
} iccoa_cert_chain_t;

/******************************************************************************
 * 证书验证器配置
 ******************************************************************************/
typedef struct {
    uint32_t max_clock_skew_seconds;    /* 最大时钟偏移 (秒) */
    bool allow_self_signed;             /* 是否允许自签名证书 */
    bool strict_size_check;             /* 严格大小检查 */
} iccoa_cert_validator_config_t;

/******************************************************************************
 * ICCOA 证书错误码
 ******************************************************************************/
typedef enum {
    ICCOA_CERT_OK = 0,
    ICCOA_CERT_ERROR_INVALID_PARAM = -200,
    ICCOA_CERT_ERROR_INVALID_FORMAT = -201,
    ICCOA_CERT_ERROR_EXPIRED = -202,
    ICCOA_CERT_ERROR_NOT_YET_VALID = -203,
    ICCOA_CERT_ERROR_INVALID_SIGNATURE = -204,
    ICCOA_CERT_ERROR_UNSUPPORTED_ALGORITHM = -205,
    ICCOA_CERT_ERROR_SIZE_EXCEEDED = -206,
    ICCOA_CERT_ERROR_INVALID_CHAIN = -207,
    ICCOA_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND = -208,
    ICCOA_CERT_ERROR_REVOKED = -209,
    ICCOA_CERT_ERROR_INVALID_MODE = -210,
	ICCOA_CERT_ERROR_INVALID_TYPE = -211,
	ICCOA_CERT_ERROR_CRYPTO = -212,
} iccoa_cert_error_t;

/******************************************************************************
 * X.509 证书序列化/解析
 ******************************************************************************/

/**
 * @brief 序列化 ICCOA 证书为 X.509 DER 格式
 * 
 * @param cert      输入的 ICCOA 证书结构
 * @param out       输出的 DER 编码缓冲区
 * @param out_len   输入: 缓冲区大小; 输出: 实际编码长度
 * 
 * @return error_t  OK 成功, 其他失败
 * 
 * @note 生成的 X.509 证书包含:
 *       - 版本号 V3
 *       - 序列号
 *       - ECDSA with SHA-256 签名算法
 *       - 颁发者和主体名称
 *       - 有效期 (UTCTime)
 *       - EC P-256 公钥
 *       - ICCOA 特有扩展字段 (OID: 1.3.6.1.4.1.59129.x)
 *       - ECDSA 签名
 */
error_t iccoa_serialize_certificate(const iccoa_certificate_t *cert, uint8_t *out, size_t *out_len);

/**
 * @brief 从 X.509 DER 格式解析 ICCOA 证书
 * 
 * @param data      输入的 DER 编码数据
 * @param data_len  数据长度
 * @param cert      输出的 ICCOA 证书结构
 * 
 * @return error_t  OK 成功, 其他失败
 * 
 * @note 支持解析标准 X.509 V3 证书，包括:
 *       - ASN.1 SEQUENCE, INTEGER, BIT STRING, OID 等
 *       - ICCOA 私有扩展 OID
 *       - 公钥提取和证书字段映射
 */
error_t iccoa_parse_certificate(const uint8_t *data, size_t data_len, iccoa_certificate_t *cert);

/******************************************************************************
 * 证书验证
 ******************************************************************************/

/**
 * @brief 验证单个 ICCOA 证书
 * 
 * @param cert              要验证的证书
 * @param trusted_pubkey    信任锚公钥 (颁发者公钥)
 * @param config            验证器配置 (可为 NULL 使用默认配置)
 * 
 * @return error_t  OK 成功
 *                  ICCOA_CERT_ERROR_INVALID_FORMAT - 证书格式无效
 *                  ICCOA_CERT_ERROR_EXPIRED - 证书已过期
 *                  ICCOA_CERT_ERROR_INVALID_SIGNATURE - 签名验证失败
 *                  ICCOA_CERT_ERROR_SIZE_EXCEEDED - 证书大小超限
 */
error_t iccoa_verify_certificate(const iccoa_certificate_t *cert, 
                                  const uint8_t *trusted_pubkey,
                                  const iccoa_cert_validator_config_t *config);

/******************************************************************************
 * 证书链验证
 ******************************************************************************/

/**
 * @brief 验证 ICCOA 证书链
 * 
 * @param cert_chain    证书链
 * @param trusted_root  信任根证书
 * @param mode          证书模式 (CA/非CA)
 * @param config        验证器配置
 * 
 * @return error_t  OK 成功
 *                  ICCOA_CERT_ERROR_INVALID_CHAIN - 证书链无效
 *                  ICCOA_CERT_ERROR_EXPIRED - 证书已过期
 *                  ICCOA_CERT_ERROR_INVALID_SIGNATURE - 签名验证失败
 * 
 * @note 从根证书开始逐级验证，确保每个证书都由其上一级正确签名
 *       非CA模式下，好友证书直接由车CA签发，跳过中间证书
 */
error_t iccoa_validate_cert_chain(
    const iccoa_cert_chain_t *cert_chain,
    const iccoa_certificate_t *trusted_root,
    iccoa_cert_mode_t mode,
    const iccoa_cert_validator_config_t *config);

/******************************************************************************
 * 特定类型证书验证
 ******************************************************************************/

/**
 * @brief 验证车主数字钥匙证书 (Type C)
 * 
 * @param cert      要验证的车主证书
 * @param ca_cert   车CA证书
 * @param config    验证器配置
 * 
 * @return error_t  OK 成功, 其他失败
 * 
 * @note 检查:
 *       - 证书类型为 ICCOA_CERT_TYPE_OWNER_DK
 *       - 大小不超过 700 字节
 *       - 签名由车CA验证
 *       - 有效期合法
 */
error_t iccoa_validate_owner_cert(const iccoa_certificate_t *cert,
                                   const iccoa_certificate_t *ca_cert,
                                   const iccoa_cert_validator_config_t *config);

/**
 * @brief 验证好友数字钥匙证书 (Type E 或 K)
 * 
 * @param cert          要验证的好友证书
 * @param signer_cert   签发者证书 (车主证书或车CA证书)
 * @param ca_cert       车CA证书 (非CA模式时可为NULL)
 * @param mode          证书模式
 * @param config        验证器配置
 * 
 * @return error_t  OK 成功, 其他失败
 * 
 * @note CA模式: 好友证书 <- 中间分享证书 <- 车主证书 <- 车CA
 *       非CA模式: 好友证书 <- 车CA (跳过中间层)
 */
error_t iccoa_validate_friend_cert(const iccoa_certificate_t *cert,
                                    const iccoa_certificate_t *signer_cert,
                                    const iccoa_certificate_t *ca_cert,
                                    iccoa_cert_mode_t mode,
                                    const iccoa_cert_validator_config_t *config);

/******************************************************************************
 * 证书工具函数
 ******************************************************************************/

/**
 * @brief 获取证书序列化后的长度
 * 
 * @param cert  输入证书
 * 
 * @return size_t  DER 编码长度，失败返回 0
 */
size_t iccoa_get_certificate_length(const iccoa_certificate_t *cert);

/**
 * @brief 检查证书大小是否符合 ICCOA 规范
 * 
 * @param cert  要检查的证书
 * 
 * @return bool  true = 符合规范, false = 超过限制
 */
bool iccoa_check_cert_size(const iccoa_certificate_t *cert);

/**
 * @brief 获取证书类型的文本描述
 * 
 * @param type  证书类型
 * 
 * @return const char*  类型描述字符串
 */
const char* iccoa_cert_type_to_string(iccoa_cert_type_t type);

/**
 * @brief 获取证书模式的文本描述
 * 
 * @param mode  证书模式
 * 
 * @return const char*  模式描述字符串
 */
const char* iccoa_cert_mode_to_string(iccoa_cert_mode_t mode);

/**
 * @brief 初始化证书验证器配置为默认值
 * 
 * @param config  要初始化的配置结构
 */
void iccoa_cert_validator_config_init(iccoa_cert_validator_config_t *config);

/**
 * @brief 清零证书结构
 * 
 * @param cert  要清零的证书结构
 */
void iccoa_certificate_clear(iccoa_certificate_t *cert);

/**
 * @brief 复制证书结构
 * 
 * @param dst  目标证书结构
 * @param src  源证书结构
 */
void iccoa_certificate_copy(iccoa_certificate_t *dst, const iccoa_certificate_t *src);

/**
 * @brief 比较两个证书是否相同
 * 
 * @param cert1  第一个证书
 * @param cert2  第二个证书
 * 
 * @return bool  true = 相同, false = 不同
 */
bool iccoa_certificate_equals(const iccoa_certificate_t *cert1, const iccoa_certificate_t *cert2);

#ifdef __cplusplus
}
#endif

#endif /* ICCOA_CERTIFICATE_H */
