/**
 * \file    mbedtls_config.h
 * \brief   mbedtls 精简配置 — yuleDKCS KW47 项目
 * 
 * 仅启用数字钥匙所需的密码学原语:
 * - ECDSA P-256 + SHA-256
 * - HKDF-SHA256 密钥派生
 * - AES-128-GCM 加密
 * - CTR_DRBG + 熵源 (随机数)
 * - ASN.1 + X.509 (证书处理)
 * 
 * 禁用所有不需要的 TLS/SSL/文件IO/网络功能
 */
#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* ====================================================================
 * 启用模块
 * ==================================================================== */

/* --- 基础 --- */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE

/* --- 大数运算 (ECC 依赖) --- */
#define MBEDTLS_BIGNUM_C

/* --- 椭圆曲线 --- */
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDSA_C

/* --- 哈希 + HMAC --- */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA224_C

/* --- HKDF 密钥派生 --- */
#define MBEDTLS_HKDF_C

/* --- 随机数 --- */
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES

/* --- AES + GCM 加密 --- */
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C

/* --- ASN.1 + 证书 --- */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_OID_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C

/* --- 内存分配 --- */
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C

#endif /* MBEDTLS_CONFIG_H */
