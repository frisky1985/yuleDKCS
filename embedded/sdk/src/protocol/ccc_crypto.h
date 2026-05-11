/******************************************************************************
 * @file    ccc_crypto.h
 * @brief   CCC 配对证书 X.509 序列化头文件
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 ******************************************************************************/

#ifndef CCC_CRYPTO_H
#define CCC_CRYPTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "ccc.h"
#include "dkcs.h"

/******************************************************************************
 * X.509 证书序列化/解析
 ******************************************************************************/

/**
 * @brief 序列化 CCC 证书为 X.509 DER 格式
 * 
 * @param cert      输入的 CCC 证书结构
 * @param out       输出的 DER 编码缓冲区
 * @param out_len   输入: 缓冲区大小; 输出: 实际编码长度
 * 
 * @return error_t  OK 成功, 其他失败
 * 
 * @note 生成的 X.509 证书包含:
 *       - 版本号 (V3)
 *       - 序列号
 *       - ECDSA with SHA-256 签名算法
 *       - 颁发者和主体名称
 *       - 有效期 (UTCTime)
 *       - EC P-256 公钥
 *       - 扩展字段 (Subject Key Identifier, Key Usage)
 *       - ECDSA 签名
 */
error_t ccc_serialize_certificate(const ccc_certificate_t *cert, uint8_t *out, size_t *out_len);

/**
 * @brief 从 X.509 DER 格式解析 CCC 证书
 * 
 * @param data      输入的 DER 编码数据
 * @param data_len  数据长度
 * @param cert      输出的 CCC 证书结构
 * 
 * @return error_t  OK 成功, 其他失败
 * 
 * @note 支持解析标准 X.509 V3 证书，包括:
 *       - ASN.1 SEQUENCE, INTEGER, BIT STRING, OID 等
 *       - UTCTime 和 GeneralizedTime 格式
 *       - 公钡提取和证书字段映射
 */
error_t ccc_parse_certificate(const uint8_t *data, size_t data_len, ccc_certificate_t *cert);

/******************************************************************************
 * 证书验证
 ******************************************************************************/

/**
 * @brief 验证单个 X.509 证书
 * 
 * @param cert              要验证的证书
 * @param trusted_pubkey    信任锚公钥 (颁发者公钥)
 * 
 * @return error_t  OK 成功
 *                  CCC_ERROR_CERT_INVALID - 证书格式无效
 *                  CCC_ERROR_CERT_EXPIRED - 证书已过期
 *                  CCC_ERROR_SIGNATURE_INVALID - 签名验证失败
 */
error_t ccc_verify_certificate(const ccc_certificate_t *cert, const uint8_t *trusted_pubkey);

/******************************************************************************
 * 证书链验证
 ******************************************************************************/

/**
 * @brief 验证 CCC 证书链
 * 
 * @param cert_chain    证书链
 * @param trusted_root  信任根证书
 * 
 * @return error_t  OK 成功
 *                  CCC_ERROR_CERT_INVALID - 证书链无效
 *                  CCC_ERROR_CERT_EXPIRED - 证书已过期
 *                  CCC_ERROR_SIGNATURE_INVALID - 签名验证失败
 * 
 * @note 从根证书开始逐级验证，确保每个证书都由其上一级正确签名
 */
error_t ccc_validate_cert_chain(
    const ccc_cert_chain_t *cert_chain,
    const ccc_certificate_t *trusted_root);

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
size_t ccc_get_certificate_length(const ccc_certificate_t *cert);

/**
 * @brief 生成自签名证书 (用于测试和开发)
 * 
 * @param cert_type     证书类型
 * @param subject_id    主体 ID (16 字节)
 * @param key_pair      密钥对: [0:65] 公钥 (未压缩) [65:97] 私钥 (简化)
 * @param validity_days 有效期天数
 * @param cert          输出证书结构
 * 
 * @return error_t  OK 成功, 其他失败
 */
error_t ccc_generate_self_signed_certificate(
    ccc_cert_type_t cert_type,
    const uint8_t *subject_id,
    const uint8_t *key_pair,
    uint32_t validity_days,
    ccc_certificate_t *cert);

#ifdef __cplusplus
}
#endif

#endif /* CCC_CRYPTO_H */
