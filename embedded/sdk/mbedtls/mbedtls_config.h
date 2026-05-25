/**
 * \file    mbedtls_config.h
 * \brief   mbedtls 精简配置 — yuleDKCS KW47 项目
 * 
 * 仅启用 ICCE/CCC/ICCOA 数字钥匙所需的密码学原语:
 * - ECDSA P-256 + SHA-256 (CCC/ICCOA)
 * - bignum + ECP (SM2 使用)
 * - CTR_DRBG + 熵源 (随机数)
 * - ASN.1 解析 (证书处理)
 * 
 * 禁用所有不需要的 TLS/SSL/文件IO/网络功能
 */
#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* ====================================================================
 * 启用模块 — 按需精减
 * ==================================================================== */

/* 基础 */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE

/* 大数运算 (SM2 依赖) */
#define MBEDTLS_BIGNUM_C

/* 椭圆曲线 (SM2 + ECDSA P-256) */
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED    /* P-256 / secp256r1 */
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDSA_DETERMINISTIC

/* 哈希 */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C                    /* SHA-256 */
#define MBEDTLS_SHA224_C                    /* SHA-224 (SHA-256依赖) */

/* 随机数 */
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES

/* 加密 */
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C

/* ASN.1 证书解析 */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C

/* 内存分配 */
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C       /* 静态内存池 */

/* 错误字符串 (可禁用以节省空间) */
/* #define MBEDTLS_ERROR_C */              /* 禁用错误字符串 */
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE

/* ====================================================================
 * 显式禁用 — 节省空间
 * ==================================================================== */
#define MBEDTLS_SSL_PROTO_TLS1_2
/* #define MBEDTLS_AES_ROM_TABLES */        /* 节省RAM */
#define MBEDTLS_ECP_FIXED_POINT_OPTIM       /* 提升 ECP 性能 */

/* ====================================================================
 * 调试 (发布时禁用)
 * ==================================================================== */
/* #define MBEDTLS_DEBUG_C */

/* ====================================================================
 * 检查配置一致性
 * ==================================================================== */
#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
