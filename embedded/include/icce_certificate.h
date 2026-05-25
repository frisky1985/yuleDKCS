/******************************************************************************
 * @file    icce_certificate.h
 * @brief   ICCE 数字钥匙证书管理 - 国密算法版本
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-15
 * 
 * @note    
 *          - ICCE Digital Key 规范 - 国密算法证书体系
 *          - SM2/SM3/SM4 国密算法栈
 *          - 自定义二进制格式 (非X.509)
 *          - 支持证书链验证
 ******************************************************************************/

#ifndef ICCE_CERTIFICATE_H
#define ICCE_CERTIFICATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "icce.h"
#include "dkcs.h"

/******************************************************************************
 * ICCE 证书类型定义
 ******************************************************************************/
typedef enum {
    ICCE_CERT_TYPE_VEHICLE_CA       = 0x01,  /* 车厂根CA证书 */
    ICCE_CERT_TYPE_VEHICLE          = 0x02,  /* 车证书 */
    ICCE_CERT_TYPE_OWNER_DK         = 0x03,  /* 车主数字钥匙证书 */
    ICCE_CERT_TYPE_SHARED_DK        = 0x04,  /* 分享数字钥匙证书 */
    ICCE_CERT_TYPE_TEMP_ACCESS      = 0x05,  /* 临时访问证书 */
} icce_cert_type_t;

/******************************************************************************
 * ICCE 证书版本
 ******************************************************************************/
#define ICCE_CERT_VERSION_1         0x01
#define ICCE_CERT_CURRENT_VERSION   ICCE_CERT_VERSION_1

/******************************************************************************
 * ICCE 证书大小限制
 ******************************************************************************/
#define ICCE_MAX_CERT_SIZE          1024   /* ICCE证书最大1024字节 */
#define ICCE_MIN_CERT_SIZE          200    /* 最小证书大小 */

/******************************************************************************
 * ICCE 证书扩展类型
 ******************************************************************************/
typedef enum {
    ICCE_EXT_TYPE_KEY_USAGE         = 0x01,  /* 密钥用途 */
    ICCE_EXT_TYPE_BASIC_CONSTRAINTS = 0x02,  /* 基本约束 */
    ICCE_EXT_TYPE_SUBJECT_ALT_NAME  = 0x03,  /* 主体备用名 */
    ICCE_EXT_TYPE_AUTHORITY_KEY_ID  = 0x04,  /* 颁发者密钥标识 */
    ICCE_EXT_TYPE_PERMISSIONS       = 0x05,  /* 数字钥匙权限 */
    ICCE_EXT_TYPE_FRIEND_INFO       = 0x06,  /* 好友信息 (分享证书) */
} icce_cert_extension_type_t;

/******************************************************************************
 * ICCE 密钥用途
 ******************************************************************************/
#define ICCE_KEY_USAGE_DIGITAL_SIGNATURE    0x0001
#define ICCE_KEY_USAGE_KEY_CERT_SIGN        0x0002
#define ICCE_KEY_USAGE_CRL_SIGN             0x0004
#define ICCE_KEY_USAGE_KEY_AGREEMENT        0x0008

/******************************************************************************
 * ICCE 证书结构 (自定义二进制格式)
 ******************************************************************************/
typedef struct {
    /* 头部信息 */
    uint8_t version;                    /* 证书版本 */
    uint8_t cert_type;                  /* 证书类型 */
    uint16_t cert_len;                  /* 证书总长度 */
    
    /* 颁发者和主体 */
    uint16_t issuer_len;                /* 颁发者名称长度 */
    uint8_t issuer[64];                 /* 颁发者名称 (UTF-8) */
    uint16_t subject_len;               /* 主体名称长度 */
    uint8_t subject[64];                /* 主体名称 (UTF-8) */
    
    /* 标识信息 */
    uint8_t device_id[ICCE_DEVICE_ID_LEN];      /* 设备ID (16字节) */
    uint8_t vehicle_id[ICCE_VEHICLE_ID_LEN];    /* 车辆ID (17字节) */
    uint8_t key_id[ICCE_KEY_ID_LEN];            /* 钥匙ID (16字节) */
    
    /* 有效期 */
    uint32_t valid_from;                /* 生效时间 (Unix时间戳) */
    uint32_t valid_until;               /* 过期时间 (Unix时间戳) */
    
    /* 公钥 (SM2) */
    uint8_t public_key[ICCE_ECC_SM2_PUB_KEY_LEN];   /* 65字节未压缩公钥 */
    
    /* 扩展字段 */
    uint32_t permissions;               /* 数字钥匙权限位图 */
    uint16_t key_usage;                 /* 密钥用途 */
    uint8_t is_ca;                      /* 是否为CA证书 */
    uint8_t max_path_len;               /* 证书链最大深度 */
    
    /* 签名 (SM2-SM3) */
    uint8_t signature[ICCE_SIGNATURE_LEN];  /* 64字节签名 (r || s) */
    
    /* 原始数据缓存 */
    uint8_t raw_data[ICCE_MAX_CERT_SIZE];    /* 序列化后的证书数据 */
    uint16_t raw_len;                       /* 实际长度 */
} icce_certificate_t;

/******************************************************************************
 * ICCE 证书链结构
 ******************************************************************************/
#define ICCE_MAX_CERT_CHAIN_LEN     4

typedef struct {
    icce_certificate_t certs[ICCE_MAX_CERT_CHAIN_LEN];
    uint8_t cert_count;
} icce_cert_chain_t;

/******************************************************************************
 * ICCE 证书错误码
 ******************************************************************************/
typedef enum {
    ICCE_CERT_OK = 0,
    ICCE_CERT_ERROR_INVALID_PARAM = -300,
    ICCE_CERT_ERROR_INVALID_FORMAT = -301,
    ICCE_CERT_ERROR_EXPIRED = -302,
    ICCE_CERT_ERROR_NOT_YET_VALID = -303,
    ICCE_CERT_ERROR_INVALID_SIGNATURE = -304,
    ICCE_CERT_ERROR_UNSUPPORTED_ALGORITHM = -305,
    ICCE_CERT_ERROR_SIZE_EXCEEDED = -306,
    ICCE_CERT_ERROR_INVALID_CHAIN = -307,
    ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND = -308,
    ICCE_CERT_ERROR_REVOKED = -309,
    ICCE_CERT_ERROR_INVALID_TYPE = -310,
    ICCE_CERT_ERROR_BUFFER_TOO_SMALL = -311,
    ICCE_CERT_ERROR_DECODE_FAILED = -312,
    ICCE_CERT_ERROR_ENCODE_FAILED = -313,
} icce_cert_error_t;

/******************************************************************************
 * ICCE 证书验证配置
 ******************************************************************************/
typedef struct {
    uint32_t max_clock_skew_seconds;    /* 最大时钟偏移 (秒) */
    bool allow_self_signed;             /* 是否允许自签名证书 */
    bool strict_size_check;             /* 严格大小检查 */
    bool verify_time;                   /* 验证时间有效性 */
    bool verify_permissions;            /* 验证权限 */
} icce_cert_validator_config_t;

/******************************************************************************
 * ICCE 证书序列化/反序列化
 ******************************************************************************/

/**
 * @brief 序列化 ICCE 证书为二进制格式
 * 
 * @param cert      输入的 ICCE 证书结构
 * @param out       输出的二进制缓冲区
 * @param out_len   输入: 缓冲区大小; 输出: 实际编码长度
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_serialize_certificate(const icce_certificate_t *cert, uint8_t *out, size_t *out_len);

/**
 * @brief 从二进制格式解析 ICCE 证书
 * 
 * @param data      输入的二进制数据
 * @param data_len  数据长度
 * @param cert      输出的 ICCE 证书结构
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_parse_certificate(const uint8_t *data, size_t data_len, icce_certificate_t *cert);

/******************************************************************************
 * 证书验证
 ******************************************************************************/

/**
 * @brief 验证单个 ICCE 证书
 * 
 * @param cert              要验证的证书
 * @param trusted_pubkey    信任锚公钥 (颁发者SM2公钥)
 * @param config            验证器配置 (可为 NULL 使用默认配置)
 * 
 * @return error_t  OK 成功
 *                  ICCE_CERT_ERROR_INVALID_FORMAT - 证书格式无效
 *                  ICCE_CERT_ERROR_EXPIRED - 证书已过期
 *                  ICCE_CERT_ERROR_INVALID_SIGNATURE - 签名验证失败
 *                  ICCE_CERT_ERROR_SIZE_EXCEEDED - 证书大小超限
 */
error_t icce_verify_certificate(const icce_certificate_t *cert, 
                                 const uint8_t *trusted_pubkey,
                                 const icce_cert_validator_config_t *config);

/**
 * @brief 验证 ICCE 证书链
 * 
 * @param cert_chain    证书链
 * @param trusted_root  信任根证书
 * @param config        验证器配置
 * 
 * @return error_t  OK 成功
 *                  ICCE_CERT_ERROR_INVALID_CHAIN - 证书链无效
 *                  ICCE_CERT_ERROR_EXPIRED - 证书已过期
 *                  ICCE_CERT_ERROR_INVALID_SIGNATURE - 签名验证失败
 */
error_t icce_validate_cert_chain(
    const icce_cert_chain_t *cert_chain,
    const icce_certificate_t *trusted_root,
    const icce_cert_validator_config_t *config);

/******************************************************************************
 * 特定类型证书验证
 ******************************************************************************/

/**
 * @brief 验证车主数字钥匙证书 (Type 0x03)
 * 
 * @param cert      要验证的车主证书
 * @param ca_cert   车CA证书
 * @param config    验证器配置
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_validate_owner_cert(const icce_certificate_t *cert,
                                  const icce_certificate_t *ca_cert,
                                  const icce_cert_validator_config_t *config);

/**
 * @brief 验证分享数字钥匙证书 (Type 0x04)
 * 
 * @param cert          要验证的分享证书
 * @param signer_cert   签发者证书 (车主证书或车CA)
 * @param ca_cert       车CA证书
 * @param config        验证器配置
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_validate_shared_cert(const icce_certificate_t *cert,
                                   const icce_certificate_t *signer_cert,
                                   const icce_certificate_t *ca_cert,
                                   const icce_cert_validator_config_t *config);

/******************************************************************************
 * 证书工具函数
 ******************************************************************************/

/**
 * @brief 初始化证书结构
 * 
 * @param cert  要初始化的证书结构
 */
void icce_certificate_init(icce_certificate_t *cert);

/**
 * @brief 清零证书结构
 * 
 * @param cert  要清零的证书结构
 */
void icce_certificate_clear(icce_certificate_t *cert);

/**
 * @brief 复制证书结构
 * 
 * @param dst  目标证书结构
 * @param src  源证书结构
 */
void icce_certificate_copy(icce_certificate_t *dst, const icce_certificate_t *src);

/**
 * @brief 比较两个证书是否相同
 * 
 * @param cert1  第一个证书
 * @param cert2  第二个证书
 * 
 * @return bool  true = 相同, false = 不同
 */
bool icce_certificate_equals(const icce_certificate_t *cert1, const icce_certificate_t *cert2);

/**
 * @brief 获取证书序列化后的长度
 * 
 * @param cert  输入证书
 * 
 * @return size_t  序列化长度，失败返回 0
 */
size_t icce_get_certificate_length(const icce_certificate_t *cert);

/**
 * @brief 检查证书大小是否符合 ICCE 规范
 * 
 * @param cert  要检查的证书
 * 
 * @return bool  true = 符合规范, false = 超过限制
 */
bool icce_check_cert_size(const icce_certificate_t *cert);

/**
 * @brief 检查证书是否已过期
 * 
 * @param cert      要检查的证书
 * @param current_time  当前时间 (Unix时间戳，0表示使用系统时间)
 * 
 * @return bool  true = 已过期, false = 未过期
 */
bool icce_cert_is_expired(const icce_certificate_t *cert, uint32_t current_time);

/**
 * @brief 检查证书是否已生效
 * 
 * @param cert      要检查的证书
 * @param current_time  当前时间 (Unix时间戳，0表示使用系统时间)
 * 
 * @return bool  true = 已生效, false = 尚未生效
 */
bool icce_cert_is_valid_now(const icce_certificate_t *cert, uint32_t current_time);

/**
 * @brief 获取证书类型的文本描述
 * 
 * @param type  证书类型
 * 
 * @return const char*  类型描述字符串
 */
const char* icce_cert_type_to_string(icce_cert_type_t type);

/**
 * @brief 获取错误码的文本描述
 * 
 * @param error  错误码
 * 
 * @return const char*  错误描述字符串
 */
const char* icce_cert_error_to_string(int error);

/**
 * @brief 初始化证书验证器配置为默认值
 * 
 * @param config  要初始化的配置结构
 */
void icce_cert_validator_config_init(icce_cert_validator_config_t *config);

/******************************************************************************
 * SM2 签名验证 (证书专用)
 ******************************************************************************/

/**
 * @brief 计算证书哈希 (SM3)
 * 
 * 对证书除签名字段外的所有数据进行SM3哈希
 * 
 * @param cert      证书结构
 * @param digest    输出哈希值 (32字节)
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_cert_compute_hash(const icce_certificate_t *cert, uint8_t digest[32]);

/**
 * @brief 验证 SM2 签名
 * 
 * @param digest        消息哈希 (32字节 SM3哈希)
 * @param signature     签名 (64字节 r||s)
 * @param public_key    SM2公钥 (65字节未压缩格式)
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_sm2_verify_signature(const uint8_t digest[32],
                                   const uint8_t signature[ICCE_SIGNATURE_LEN],
                                   const uint8_t public_key[ICCE_ECC_SM2_PUB_KEY_LEN]);

/******************************************************************************
 * APDU 传输封装
 ******************************************************************************/

/**
 * @brief 将证书封装为 APDU 数据
 * 
 * 用于通过SE接口传输证书
 * 
 * @param cert          证书
 * @param apdu_data     APDU数据缓冲区
 * @param apdu_len      输入: 缓冲区大小; 输出: 实际长度
 * @param is_last       是否为最后一段 (分片传输)
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_cert_to_apdu(const icce_certificate_t *cert,
                           uint8_t *apdu_data,
                           size_t *apdu_len,
                           bool is_last);

/**
 * @brief 从 APDU 数据解析证书
 * 
 * @param apdu_data     APDU数据
 * @param apdu_len      数据长度
 * @param cert          输出的证书结构
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_cert_from_apdu(const uint8_t *apdu_data,
                             size_t apdu_len,
                             icce_certificate_t *cert);

/******************************************************************************
 * 证书生成辅助函数 (用于测试/模拟)
 ******************************************************************************/

/**
 * @brief 生成测试用自签名证书
 * 
 * @note 仅用于测试，生产环境应使用正确的密钥 hierarchy
 * 
 * @param cert          输出的证书结构
 * @param type          证书类型
 * @param subject       主题名称
 * @param subject_len   主题名称长度
 * @param key_pair      SM2密钥对
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t icce_generate_test_certificate(icce_certificate_t *cert,
                                        icce_cert_type_t type,
                                        const uint8_t *subject,
                                        size_t subject_len,
                                        const uint8_t key_pair[96]);  /* 私钥32B + 公钥65B */

#ifdef __cplusplus
}
#endif

#endif /* ICCE_CERTIFICATE_H */
