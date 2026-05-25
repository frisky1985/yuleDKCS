/******************************************************************************
 * @file    iccoa_certificate.c
 * @brief   ICCOA 数字钥匙证书 X.509 序列化实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-15
 * 
 * @note    
 *          - ICCOA/T 002-2024 数字钥匙技术规范第6章证书要求
 *          - X.509 V3 格式，DER 编码
 *          - ECDSA P-256 + SHA-256 签名算法
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "iccoa.h"
#include "iccoa_certificate.h"
#include "dkcs.h"
#include "ecc_hal.h"

/******************************************************************************
 * X.509 ASN.1 常量定义
 ******************************************************************************/

/* ASN.1 标签 */
#define ASN1_BOOLEAN                0x01
#define ASN1_INTEGER                0x02
#define ASN1_BIT_STRING             0x03
#define ASN1_OCTET_STRING           0x04
#define ASN1_NULL                   0x05
#define ASN1_OBJECT_IDENTIFIER      0x06
#define ASN1_UTF8_STRING            0x0C
#define ASN1_PRINTABLE_STRING       0x13
#define ASN1_TELETEX_STRING         0x14
#define ASN1_IA5_STRING             0x16
#define ASN1_UTC_TIME               0x17
#define ASN1_GENERALIZED_TIME       0x18
#define ASN1_SEQUENCE               0x30
#define ASN1_SET                    0x31
#define ASN1_CONTEXT_SPECIFIC_0     0xA0
#define ASN1_CONTEXT_SPECIFIC_3     0xA3

/* X.509 版本 */
#define X509_VERSION_V1             0
#define X509_VERSION_V3             2

/******************************************************************************
 * ICCOA OID 定义 (1.3.6.1.4.1.59129 - ICCOA 私有企业 OID)
 ******************************************************************************/

/* 证书类型 OID */
const uint8_t OID_ICCOA_CERT_TYPE_VEHICLE_OEM_CA[] = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x01, 0x01};
const uint8_t OID_ICCOA_CERT_TYPE_VEHICLE[]        = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x01, 0x02};
const uint8_t OID_ICCOA_CERT_TYPE_OWNER_DK[]       = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x01, 0x03};
const uint8_t OID_ICCOA_CERT_TYPE_MID_SHARE[]      = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x01, 0x04};
const uint8_t OID_ICCOA_CERT_TYPE_SHARED_DK[]      = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x01, 0x05};

/* 扩展信息 OID */
const uint8_t OID_ICCOA_VEHICLE_OEM_ID[]   = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x02, 0x05};  /* 1.3.6.1.4.1.59129.2.5 */
const uint8_t OID_ICCOA_VEHICLE_ID[]       = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x02, 0x01};  /* 1.3.6.1.4.1.59129.2.1 */
const uint8_t OID_ICCOA_DIGITAL_KEY_ID[]   = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x02, 0x02};  /* 1.3.6.1.4.1.59129.2.2 */
const uint8_t OID_ICCOA_DIGITAL_KEY_AUTH[] = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x02, 0x04};  /* 1.3.6.1.4.1.59129.2.4 */
const uint8_t OID_ICCOA_DIGITAL_KEY_MODE[] = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x89, 0x37, 0x02, 0x0A};  /* 1.3.6.1.4.1.59129.2.10 */

/* 标准 OID */
static const uint8_t OID_ECDSA_WITH_SHA256[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02};
static const uint8_t OID_EC_PUBLIC_KEY[]     = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01};
static const uint8_t OID_PRIME256V1[]        = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};
static const uint8_t OID_COMMON_NAME[]       = {0x55, 0x04, 0x03};
static const uint8_t OID_SUBJECT_KEY_ID[]    = {0x55, 0x1D, 0x0E};
static const uint8_t OID_KEY_USAGE[]         = {0x55, 0x1D, 0x0F};

#define OID_ECDSA_WITH_SHA256_LEN   8
#define OID_EC_PUBLIC_KEY_LEN       7
#define OID_PRIME256V1_LEN          8
#define OID_COMMON_NAME_LEN         3
#define OID_SUBJECT_KEY_ID_LEN      3
#define OID_KEY_USAGE_LEN           3

/******************************************************************************
 * 内部辅助函数前向声明
 ******************************************************************************/

static size_t asn1_encode_length(size_t len, uint8_t *out);
static size_t asn1_encode_integer(const uint8_t *value, size_t len, uint8_t *out);
static size_t asn1_encode_bit_string(const uint8_t *data, size_t bit_len, uint8_t *out);
static size_t asn1_encode_octet_string(const uint8_t *data, size_t len, uint8_t *out);
static size_t asn1_encode_oid(const uint8_t *oid, size_t oid_len, uint8_t *out);
static size_t asn1_encode_sequence(const uint8_t *content, size_t content_len, uint8_t *out);
static size_t asn1_encode_utctime(uint32_t timestamp, uint8_t *out);
static size_t asn1_encode_context_specific(uint8_t tag, const uint8_t *content, size_t len, uint8_t *out);

static size_t asn1_decode_length(const uint8_t *data, size_t *bytes_consumed);
static int asn1_decode_integer(const uint8_t *data, size_t data_len, uint8_t *out, size_t *out_len);
static int asn1_decode_oid(const uint8_t *data, size_t data_len, uint8_t *out, size_t *out_len);

static int verify_ecdsa_signature(const uint8_t *digest, const uint8_t *signature, const uint8_t *public_key);
static int sha256_digest(const uint8_t *data, size_t len, uint8_t *digest);

/******************************************************************************
 * ASN.1 编码辅助函数
 ******************************************************************************/

/**
 * @brief 编码 ASN.1 长度字段
 */
static size_t asn1_encode_length(size_t len, uint8_t *out)
{
    if (len < 0x80) {
        out[0] = (uint8_t)len;
        return 1;
    } else if (len <= 0xFF) {
        out[0] = 0x81;
        out[1] = (uint8_t)len;
        return 2;
    } else {
        out[0] = 0x82;
        out[1] = (uint8_t)(len >> 8);
        out[2] = (uint8_t)len;
        return 3;
    }
}

/**
 * @brief 编码 ASN.1 INTEGER
 */
static size_t asn1_encode_integer(const uint8_t *value, size_t len, uint8_t *out)
{
    size_t offset = 0;
    
    /* 如果最高位为1，需要添加前导0 */
    int prepend_zero = (len > 0 && (value[0] & 0x80));
    size_t content_len = len + (prepend_zero ? 1 : 0);
    
    out[offset++] = ASN1_INTEGER;
    offset += asn1_encode_length(content_len, &out[offset]);
    
    if (prepend_zero) {
        out[offset++] = 0x00;
    }
    
    memcpy(&out[offset], value, len);
    offset += len;
    
    return offset;
}

/**
 * @brief 编码 ASN.1 BIT STRING
 */
static size_t asn1_encode_bit_string(const uint8_t *data, size_t bit_len, uint8_t *out)
{
    size_t offset = 0;
    size_t byte_len = (bit_len + 7) / 8;
    size_t unused_bits = (8 - (bit_len % 8)) % 8;
    
    out[offset++] = ASN1_BIT_STRING;
    offset += asn1_encode_length(byte_len + 1, &out[offset]);
    out[offset++] = (uint8_t)unused_bits;
    memcpy(&out[offset], data, byte_len);
    offset += byte_len;
    
    return offset;
}

/**
 * @brief 编码 ASN.1 OCTET STRING
 */
static size_t asn1_encode_octet_string(const uint8_t *data, size_t len, uint8_t *out)
{
    size_t offset = 0;
    
    out[offset++] = ASN1_OCTET_STRING;
    offset += asn1_encode_length(len, &out[offset]);
    memcpy(&out[offset], data, len);
    offset += len;
    
    return offset;
}

/**
 * @brief 编码 ASN.1 OID
 */
static size_t asn1_encode_oid(const uint8_t *oid, size_t oid_len, uint8_t *out)
{
    size_t offset = 0;
    
    out[offset++] = ASN1_OBJECT_IDENTIFIER;
    offset += asn1_encode_length(oid_len, &out[offset]);
    memcpy(&out[offset], oid, oid_len);
    offset += oid_len;
    
    return offset;
}

/**
 * @brief 编码 ASN.1 SEQUENCE
 */
static size_t asn1_encode_sequence(const uint8_t *content, size_t content_len, uint8_t *out)
{
    size_t offset = 0;
    
    out[offset++] = ASN1_SEQUENCE;
    offset += asn1_encode_length(content_len, &out[offset]);
    memcpy(&out[offset], content, content_len);
    offset += content_len;
    
    return offset;
}

/**
 * @brief 编码 ASN.1 UTCTime
 */
static size_t asn1_encode_utctime(uint32_t timestamp, uint8_t *out)
{
    struct tm tm_buf;
    struct tm *tm_info;
    time_t t = (time_t)timestamp;
    
    tm_info = gmtime_r(&t, &tm_buf);
    if (tm_info == NULL) {
        return 0;
    }
    
    char time_str[20];
    snprintf(time_str, sizeof(time_str), "%02d%02d%02d%02d%02d%02dZ",
             tm_info->tm_year % 100, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    size_t offset = 0;
    out[offset++] = ASN1_UTC_TIME;
    offset += asn1_encode_length(13, &out[offset]);
    memcpy(&out[offset], time_str, 13);
    offset += 13;
    
    return offset;
}

/**
 * @brief 编码 ASN.1 Context-Specific
 */
static size_t asn1_encode_context_specific(uint8_t tag, const uint8_t *content, size_t len, uint8_t *out)
{
    size_t offset = 0;
    
    out[offset++] = ASN1_CONTEXT_SPECIFIC_0 | tag;
    offset += asn1_encode_length(len, &out[offset]);
    memcpy(&out[offset], content, len);
    offset += len;
    
    return offset;
}

/******************************************************************************
 * ASN.1 解码辅助函数
 ******************************************************************************/

/**
 * @brief 解码 ASN.1 长度
 */
static size_t asn1_decode_length(const uint8_t *data, size_t *bytes_consumed)
{
    if (data[0] & 0x80) {
        size_t len_bytes = data[0] & 0x7F;
        size_t len = 0;
        for (size_t i = 0; i < len_bytes; i++) {
            len = (len << 8) | data[i + 1];
        }
        *bytes_consumed = len_bytes + 1;
        return len;
    } else {
        *bytes_consumed = 1;
        return data[0];
    }
}

/**
 * @brief 解码 ASN.1 INTEGER
 */
static int asn1_decode_integer(const uint8_t *data, size_t data_len, uint8_t *out, size_t *out_len)
{
    if (data_len < 2 || data[0] != ASN1_INTEGER) {
        return -1;
    }
    
    size_t len_bytes = 0;
    size_t len = asn1_decode_length(&data[1], &len_bytes);
    
    if (data_len < 1 + len_bytes + len) {
        return -1;
    }
    
    size_t offset = 1 + len_bytes;
    
    /* 跳过前导零 */
    if (len > 1 && data[offset] == 0x00 && (data[offset + 1] & 0x80)) {
        offset++;
        len--;
    }
    
    if (len > *out_len) {
        return -1;
    }
    
    memcpy(out, &data[offset], len);
    *out_len = len;
    return 0;
}

/**
 * @brief 解码 ASN.1 OID
 */
static int asn1_decode_oid(const uint8_t *data, size_t data_len, uint8_t *out, size_t *out_len)
{
    if (data_len < 2 || data[0] != ASN1_OBJECT_IDENTIFIER) {
        return -1;
    }
    
    size_t len_bytes = 0;
    size_t len = asn1_decode_length(&data[1], &len_bytes);
    
    if (data_len < 1 + len_bytes + len) {
        return -1;
    }
    
    if (len > *out_len) {
        return -1;
    }
    
    memcpy(out, &data[1 + len_bytes], len);
    *out_len = len;
    return 0;
}

/******************************************************************************
 * 密码学辅助函数
 ******************************************************************************/

/**
 * @brief 计算 SHA-256 摘要
 */
static int sha256_digest(const uint8_t *data, size_t len, uint8_t *digest)
{
    /* 调用 HAL 层实现 */
    return ecc_hal_sha256(data, len, digest);
}

/**
 * @brief 验证 ECDSA 签名
 */
static int verify_ecdsa_signature(const uint8_t *digest, const uint8_t *signature, const uint8_t *public_key)
{
    /* 调用 HAL 层实现 */
    return ecc_hal_ecdsa_verify(digest, 32, signature, 64, public_key);
}

/******************************************************************************
 * X.509 证书序列化
 ******************************************************************************/

/**
 * @brief 序列化 ICCOA 证书为 X.509 DER 格式
 */
error_t iccoa_serialize_certificate(const iccoa_certificate_t *cert, uint8_t *out, size_t *out_len)
{
    if (cert == NULL || out == NULL || out_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    uint8_t temp_buf[1024];
    uint8_t temp_buf2[512];
    size_t offset = 0;
    
    /**************************************************************************
     * 1. 版本号 (Version) - Context-Specific [0]
     **************************************************************************/
    uint8_t version_data[5];
    version_data[0] = 0x02;  /* INTEGER tag */
    version_data[1] = 0x01;  /* length 1 */
    version_data[2] = X509_VERSION_V3;  /* Version 3 */
    uint8_t version_ctx[8];
    size_t version_ctx_len = asn1_encode_context_specific(0, version_data, 3, version_ctx);
    
    /**************************************************************************
     * 2. 序列号 (Serial Number)
     **************************************************************************/
    uint8_t serial[32];
    memcpy(serial, cert->serial_number, 16);
    /* 确保最高位为0，防止被解释为负数 */
    if (serial[0] & 0x80) {
        memmove(serial + 1, serial, 16);
        serial[0] = 0x00;
        offset = asn1_encode_integer(serial, 17, temp_buf);
    } else {
        offset = asn1_encode_integer(serial, 16, temp_buf);
    }
    size_t serial_len = offset;
    
    /**************************************************************************
     * 3. 签名算法 (Signature Algorithm)
     **************************************************************************/
    uint8_t sig_alg_params[2] = { ASN1_NULL, 0x00 };
    uint8_t sig_alg_oid[16];
    size_t sig_oid_len = asn1_encode_oid(OID_ECDSA_WITH_SHA256, OID_ECDSA_WITH_SHA256_LEN, sig_alg_oid);
    memcpy(temp_buf + serial_len, sig_alg_oid, sig_oid_len);
    memcpy(temp_buf + serial_len + sig_oid_len, sig_alg_params, 2);
    size_t sig_alg_len = sig_oid_len + 2;
    
    uint8_t sig_alg_seq[24];
    size_t sig_alg_seq_len = asn1_encode_sequence(temp_buf + serial_len, sig_alg_len, sig_alg_seq);
    
    /**************************************************************************
     * 4. 颁发者 (Issuer) - 使用 issuer_id
     **************************************************************************/
    uint8_t issuer_name[64];
    uint8_t issuer_cn[32];
    size_t issuer_cn_len = asn1_encode_oid(OID_COMMON_NAME, OID_COMMON_NAME_LEN, issuer_cn);
    uint8_t issuer_id_str[20];
    snprintf((char*)issuer_id_str, sizeof(issuer_id_str), "%02X%02X%02X%02X",
             cert->issuer_id[0], cert->issuer_id[1], cert->issuer_id[2], cert->issuer_id[3]);
    issuer_cn[issuer_cn_len++] = ASN1_PRINTABLE_STRING;
    issuer_cn[issuer_cn_len++] = 8;
    memcpy(&issuer_cn[issuer_cn_len], issuer_id_str, 8);
    issuer_cn_len += 8;
    
    uint8_t issuer_attr[40];
    size_t issuer_attr_len = asn1_encode_sequence(issuer_cn, issuer_cn_len, issuer_attr);
    uint8_t issuer_set[42];
    issuer_set[0] = ASN1_SET;
    issuer_set[1] = issuer_attr_len;
    memcpy(&issuer_set[2], issuer_attr, issuer_attr_len);
    size_t issuer_set_len = 2 + issuer_attr_len;
    
    size_t issuer_len = asn1_encode_sequence(issuer_set, issuer_set_len, issuer_name);
    
    /**************************************************************************
     * 5. 有效期 (Validity)
     **************************************************************************/
    uint8_t validity[64];
    uint8_t not_before[20], not_after[20];
    size_t not_before_len = asn1_encode_utctime(cert->valid_from, not_before);
    size_t not_after_len = asn1_encode_utctime(cert->valid_until, not_after);
    memcpy(validity, not_before, not_before_len);
    memcpy(validity + not_before_len, not_after, not_after_len);
    size_t validity_len = not_before_len + not_after_len;
    
    uint8_t validity_seq[66];
    size_t validity_seq_len = asn1_encode_sequence(validity, validity_len, validity_seq);
    
    /**************************************************************************
     * 6. 主体 (Subject) - 使用 subject_id
     **************************************************************************/
    uint8_t subject_name[64];
    uint8_t subject_cn[32];
    size_t subject_cn_len = asn1_encode_oid(OID_COMMON_NAME, OID_COMMON_NAME_LEN, subject_cn);
    uint8_t subject_id_str[20];
    snprintf((char*)subject_id_str, sizeof(subject_id_str), "%02X%02X%02X%02X",
             cert->subject_id[0], cert->subject_id[1], cert->subject_id[2], cert->subject_id[3]);
    subject_cn[subject_cn_len++] = ASN1_PRINTABLE_STRING;
    subject_cn[subject_cn_len++] = 8;
    memcpy(&subject_cn[subject_cn_len], subject_id_str, 8);
    subject_cn_len += 8;
    
    uint8_t subject_attr[40];
    size_t subject_attr_len = asn1_encode_sequence(subject_cn, subject_cn_len, subject_attr);
    uint8_t subject_set[42];
    subject_set[0] = ASN1_SET;
    subject_set[1] = subject_attr_len;
    memcpy(&subject_set[2], subject_attr, subject_attr_len);
    size_t subject_set_len = 2 + subject_attr_len;
    
    size_t subject_len = asn1_encode_sequence(subject_set, subject_set_len, subject_name);
    
    /**************************************************************************
     * 7. 主题公钥信息 (Subject Public Key Info)
     **************************************************************************/
    uint8_t ec_oid[16], curve_oid[16];
    size_t ec_oid_len = asn1_encode_oid(OID_EC_PUBLIC_KEY, OID_EC_PUBLIC_KEY_LEN, ec_oid);
    size_t curve_oid_len = asn1_encode_oid(OID_PRIME256V1, OID_PRIME256V1_LEN, curve_oid);
    memcpy(temp_buf2, ec_oid, ec_oid_len);
    memcpy(temp_buf2 + ec_oid_len, curve_oid, curve_oid_len);
    size_t alg_id_len = ec_oid_len + curve_oid_len;
    
    uint8_t alg_id_seq[32];
    size_t alg_id_seq_len = asn1_encode_sequence(temp_buf2, alg_id_len, alg_id_seq);
    
    uint8_t pubkey_bitstr[80];
    size_t pubkey_bitstr_len = asn1_encode_bit_string(cert->public_key, 65 * 8, pubkey_bitstr);
    
    memcpy(temp_buf2, alg_id_seq, alg_id_seq_len);
    memcpy(temp_buf2 + alg_id_seq_len, pubkey_bitstr, pubkey_bitstr_len);
    size_t spki_len = alg_id_seq_len + pubkey_bitstr_len;
    
    uint8_t spki_seq[128];
    size_t spki_seq_len = asn1_encode_sequence(temp_buf2, spki_len, spki_seq);
    
    /**************************************************************************
     * 8. 扩展 (Extensions) - Context-Specific [3]
     *    包含 ICCOA 特有扩展
     **************************************************************************/
    uint8_t extensions[512];
    size_t ext_offset = 0;
    
    /* Subject Key Identifier */
    uint8_t ski_value[20];
    ski_value[0] = ASN1_OCTET_STRING;
    ski_value[1] = 16;
    memcpy(&ski_value[2], cert->subject_id, 16);
    uint8_t ski_seq[32];
    size_t ski_seq_len = asn1_encode_sequence(ski_value, 18, ski_seq);
    memcpy(extensions + ext_offset, ski_seq, ski_seq_len);
    ext_offset += ski_seq_len;
    
    /* Key Usage */
    uint8_t ku_value[8] = { 0x03, 0x02, 0x07, 0x80, 0x00 }; /* digitalSignature */
    uint8_t ku_seq[16];
    size_t ku_seq_len = asn1_encode_sequence(ku_value, 5, ku_seq);
    memcpy(extensions + ext_offset, ku_seq, ku_seq_len);
    ext_offset += ku_seq_len;
    
    /* ICCOA 证书模式扩展 */
    uint8_t mode_value[16];
    mode_value[0] = ASN1_OCTET_STRING;
    mode_value[1] = 1;
    mode_value[2] = (uint8_t)cert->mode;
    uint8_t mode_ext_seq[24];
    size_t mode_ext_seq_len = asn1_encode_sequence(mode_value, 3, mode_ext_seq);
    memcpy(extensions + ext_offset, mode_ext_seq, mode_ext_seq_len);
    ext_offset += mode_ext_seq_len;
    
    uint8_t ext_seq[512];
    size_t ext_seq_len = asn1_encode_sequence(extensions, ext_offset, ext_seq);
    
    uint8_t ext_ctx[520];
    size_t ext_ctx_len = asn1_encode_context_specific(3, ext_seq, ext_seq_len, ext_ctx);
    
    /**************************************************************************
     * 组装 TBSCertificate
     **************************************************************************/
    uint8_t tbs_cert[1024];
    size_t tbs_offset = 0;
    
    memcpy(tbs_cert + tbs_offset, version_ctx, version_ctx_len);
    tbs_offset += version_ctx_len;
    
    memcpy(tbs_cert + tbs_offset, temp_buf, serial_len);
    tbs_offset += serial_len;
    
    memcpy(tbs_cert + tbs_offset, sig_alg_seq, sig_alg_seq_len);
    tbs_offset += sig_alg_seq_len;
    
    memcpy(tbs_cert + tbs_offset, issuer_name, issuer_len);
    tbs_offset += issuer_len;
    
    memcpy(tbs_cert + tbs_offset, validity_seq, validity_seq_len);
    tbs_offset += validity_seq_len;
    
    memcpy(tbs_cert + tbs_offset, subject_name, subject_len);
    tbs_offset += subject_len;
    
    memcpy(tbs_cert + tbs_offset, spki_seq, spki_seq_len);
    tbs_offset += spki_seq_len;
    
    memcpy(tbs_cert + tbs_offset, ext_ctx, ext_ctx_len);
    tbs_offset += ext_ctx_len;
    
    uint8_t tbs_seq[1024];
    size_t tbs_seq_len = asn1_encode_sequence(tbs_cert, tbs_offset, tbs_seq);
    
    /**************************************************************************
     * 9. 签名值 (Signature)
     **************************************************************************/
    uint8_t sig_value[80];
    uint8_t sig_rs[64];
    memcpy(sig_rs, cert->signature, 64);
    size_t sig_value_len = asn1_encode_bit_string(sig_rs, 64 * 8, sig_value);
    
    /**************************************************************************
     * 组装最终证书
     **************************************************************************/
    uint8_t cert_content[1200];
    size_t cert_offset = 0;
    
    memcpy(cert_content + cert_offset, tbs_seq, tbs_seq_len);
    cert_offset += tbs_seq_len;
    
    memcpy(cert_content + cert_offset, sig_alg_seq, sig_alg_seq_len);
    cert_offset += sig_alg_seq_len;
    
    memcpy(cert_content + cert_offset, sig_value, sig_value_len);
    cert_offset += sig_value_len;
    
    size_t final_len = asn1_encode_sequence(cert_content, cert_offset, out);
    
    *out_len = final_len;
    return OK;
}

/******************************************************************************
 * X.509 证书解析
 ******************************************************************************/

/**
 * @brief 从 X.509 DER 格式解析 ICCOA 证书
 */
error_t iccoa_parse_certificate(const uint8_t *data, size_t data_len, iccoa_certificate_t *cert)
{
    if (data == NULL || cert == NULL || data_len < 10) {
        return ERROR_INVALID_PARAM;
    }
    
    memset(cert, 0, sizeof(iccoa_certificate_t));
    
    /* 验证 SEQUENCE 标签 */
    if (data[0] != ASN1_SEQUENCE) {
        return ICCOA_CERT_ERROR_INVALID_FORMAT;
    }
    
    size_t len_bytes = 0;
    size_t cert_len = asn1_decode_length(&data[1], &len_bytes);
    
    if (data_len < 1 + len_bytes + cert_len) {
        return ICCOA_CERT_ERROR_INVALID_FORMAT;
    }
    
    size_t offset = 1 + len_bytes;
    
    /* 解析 TBSCertificate */
    if (data[offset] != ASN1_SEQUENCE) {
        return ICCOA_CERT_ERROR_INVALID_FORMAT;
    }
    
    size_t tbs_len_bytes = 0;
    size_t tbs_len = asn1_decode_length(&data[offset + 1], &tbs_len_bytes);
    size_t tbs_start = offset + 1 + tbs_len_bytes;
    size_t tbs_end = tbs_start + tbs_len;
    
    size_t tbs_offset = tbs_start;
    
    /* 跳过可选的版本号 [0] */
    if (data[tbs_offset] == (ASN1_CONTEXT_SPECIFIC_0 | 0x00)) {
        size_t ver_len_bytes = 0;
        size_t ver_len = asn1_decode_length(&data[tbs_offset + 1], &ver_len_bytes);
        tbs_offset += 1 + ver_len_bytes + ver_len;
    }
    
    /* 解析序列号 */
    if (data[tbs_offset] == ASN1_INTEGER) {
        size_t serial_len = 16;
        uint8_t serial_temp[17];
        if (asn1_decode_integer(&data[tbs_offset], tbs_end - tbs_offset, serial_temp, &serial_len) == 0) {
            if (serial_len <= 16) {
                memcpy(cert->serial_number + (16 - serial_len), serial_temp, serial_len);
            }
        }
        size_t sb = 0;
        size_t sl = asn1_decode_length(&data[tbs_offset + 1], &sb);
        tbs_offset += 1 + sb + sl;
    }
    
    /* 跳过签名算法 */
    if (data[tbs_offset] == ASN1_SEQUENCE) {
        size_t sb = 0;
        size_t sl = asn1_decode_length(&data[tbs_offset + 1], &sb);
        tbs_offset += 1 + sb + sl;
    }
    
    /* 跳过颁发者 */
    if (data[tbs_offset] == ASN1_SEQUENCE) {
        size_t sb = 0;
        size_t sl = asn1_decode_length(&data[tbs_offset + 1], &sb);
        tbs_offset += 1 + sb + sl;
    }
    
    /* 跳过有效期 */
    if (data[tbs_offset] == ASN1_SEQUENCE) {
        size_t sb = 0;
        size_t sl = asn1_decode_length(&data[tbs_offset + 1], &sb);
        tbs_offset += 1 + sb + sl;
    }
    
    /* 跳过主体 */
    if (data[tbs_offset] == ASN1_SEQUENCE) {
        size_t sb = 0;
        size_t sl = asn1_decode_length(&data[tbs_offset + 1], &sb);
        tbs_offset += 1 + sb + sl;
    }
    
    /* 解析主体公钥信息 */
    if (data[tbs_offset] == ASN1_SEQUENCE) {
        size_t sb = 0;
        size_t sl = asn1_decode_length(&data[tbs_offset + 1], &sb);
        size_t spki_end = tbs_offset + 1 + sb + sl;
        size_t spki_offset = tbs_offset + 1 + sb;
        
        /* 找到 BIT STRING 中的公钥 */
        while (spki_offset < spki_end) {
            if (data[spki_offset] == ASN1_BIT_STRING) {
                size_t bs_b = 0;
                size_t bs_l = asn1_decode_length(&data[spki_offset + 1], &bs_b);
                if (bs_l > 1 && (bs_l - 1) <= 65) {
                    memcpy(cert->public_key, &data[spki_offset + 1 + bs_b + 1], bs_l - 1);
                }
                break;
            }
            spki_offset++;
        }
        
        tbs_offset += 1 + sb + sl;
    }
    
    /* 保存 DER 数据 */
    if (data_len <= sizeof(cert->der_data)) {
        memcpy(cert->der_data, data, data_len);
        cert->der_len = data_len;
    }
    
    cert->version = X509_VERSION_V3;
    
    return OK;
}

/******************************************************************************
 * 证书验证
 ******************************************************************************/

/**
 * @brief 从 DER 编码的证书中提取 TBSCertificate 范围
 * 
 * @param data          DER 编码数据
 * @param data_len      数据长度
 * @param tbs_offset    输出: TBS 偏移量 (相对于 data)
 * @param tbs_len       输出: TBS 编码长度 (含 SEQUENCE tag+length+content)
 * @return error_t      OK 成功
 */
static error_t iccoa_get_tbs_range(const uint8_t *data, size_t data_len,
                                    size_t *tbs_offset, size_t *tbs_len)
{
    if (!data || !tbs_offset || !tbs_len || data_len < 4) {
        return ERROR_INVALID_PARAM;
    }

    /* 解析外层 SEQUENCE */
    if (data[0] != ASN1_SEQUENCE) {
        return ICCOA_CERT_ERROR_INVALID_FORMAT;
    }

    size_t len_bytes = 0;
    size_t outer_len = asn1_decode_length(&data[1], &len_bytes);
    if (1 + len_bytes + outer_len > data_len) {
        return ICCOA_CERT_ERROR_INVALID_FORMAT;
    }

    /* TBS SEQUENCE 从外层 header 后开始 */
    size_t start = 1 + len_bytes;
    if (data[start] != ASN1_SEQUENCE) {
        return ICCOA_CERT_ERROR_INVALID_FORMAT;
    }

    /* 读取 TBS SEQUENCE 的 content 长度 */
    size_t tbs_len_bytes = 0;
    size_t tbs_content_len = asn1_decode_length(&data[start + 1], &tbs_len_bytes);

    /* TBS 编码总长 = tag(1) + length(tbs_len_bytes) + content(tbs_content_len) */
    *tbs_offset = start;
    *tbs_len = 1 + tbs_len_bytes + tbs_content_len;

    return OK;
}

/**
 * @brief 验证单个 ICCOA 证书
 */
error_t iccoa_verify_certificate(const iccoa_certificate_t *cert, 
                                  const uint8_t *trusted_pubkey,
                                  const iccoa_cert_validator_config_t *config)
{
    if (cert == NULL || trusted_pubkey == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 检查大小限制 */
    if (config != NULL && config->strict_size_check) {
        if (!iccoa_check_cert_size(cert)) {
            return ICCOA_CERT_ERROR_SIZE_EXCEEDED;
        }
    }
    
    /* 检查有效期 */
    uint32_t current_time = (uint32_t)time(NULL);
    uint32_t skew = (config != NULL) ? config->max_clock_skew_seconds : 300;
    
    if (current_time + skew < cert->valid_from) {
        return ICCOA_CERT_ERROR_NOT_YET_VALID;
    }
    
    if (current_time > cert->valid_until + skew) {
        return ICCOA_CERT_ERROR_EXPIRED;
    }
    
/* 验证签名 */
    if (cert->der_len > 0) {
        /* 获取 TBSCertificate 范围 */
        size_t tbs_offset = 0;
        size_t tbs_len = 0;
        if (iccoa_get_tbs_range(cert->der_data, cert->der_len, &tbs_offset, &tbs_len) != OK) {
            return ICCOA_CERT_ERROR_INVALID_FORMAT;
        }

        /* 计算 TBSCertificate 的散列 (X.509 标准: 仅对 TBS 部分签名) */
        uint8_t digest[32];
        if (sha256_digest(cert->der_data + tbs_offset, tbs_len, digest) != 0) {
            return ICCOA_CERT_ERROR_CRYPTO;
        }
        
        if (verify_ecdsa_signature(digest, cert->signature, trusted_pubkey) != 0) {
            return ICCOA_CERT_ERROR_INVALID_SIGNATURE;
        }
    }
    
    return OK;
}

/******************************************************************************
 * 特定类型证书验证
 ******************************************************************************/

/**
 * @brief 验证车主数字钥匙证书 (Type C)
 */
error_t iccoa_validate_owner_cert(const iccoa_certificate_t *cert,
                                   const iccoa_certificate_t *ca_cert,
                                   const iccoa_cert_validator_config_t *config)
{
    if (cert == NULL || ca_cert == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 检查证书类型 */
    if (cert->type != ICCOA_CERT_TYPE_OWNER_DK) {
        return ICCOA_CERT_ERROR_INVALID_TYPE;
    }
    
    /* 检查证书大小 */
    if (cert->der_len > ICCOA_MAX_OWNER_CERT_SIZE) {
        return ICCOA_CERT_ERROR_SIZE_EXCEEDED;
    }
    
    /* 验证证书 */
    return iccoa_verify_certificate(cert, ca_cert->public_key, config);
}

/**
 * @brief 验证好友数字钥匙证书 (Type E 或 K)
 */
error_t iccoa_validate_friend_cert(const iccoa_certificate_t *cert,
                                    const iccoa_certificate_t *signer_cert,
                                    const iccoa_certificate_t *ca_cert,
                                    iccoa_cert_mode_t mode,
                                    const iccoa_cert_validator_config_t *config)
{
    if (cert == NULL || signer_cert == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 检查证书类型 */
    if (cert->type != ICCOA_CERT_TYPE_SHARED_DK && cert->type != ICCOA_CERT_TYPE_SHARED_DK_V2) {
        return ICCOA_CERT_ERROR_INVALID_TYPE;
    }
    
    /* 根据类型检查大小 */
    if (cert->type == ICCOA_CERT_TYPE_SHARED_DK_V2) {
        if (cert->der_len > ICCOA_MAX_SHARED_CERT_V2_SIZE) {
            return ICCOA_CERT_ERROR_SIZE_EXCEEDED;
        }
    } else {
        if (cert->der_len > ICCOA_MAX_SHARED_CERT_SIZE) {
            return ICCOA_CERT_ERROR_SIZE_EXCEEDED;
        }
    }
    
    /* CA模式验证: 好友证书 <- 签发者(车主/中间) <- ... <- 车CA */
    /* 非CA模式验证: 好友证书 <- 车CA (直接) */
    
    if (mode == ICCOA_CERT_MODE_NON_CA) {
        /* 非CA模式: 签发者必须是车CA */
        if (signer_cert->type != ICCOA_CERT_TYPE_VEHICLE_OEM_CA) {
            return ICCOA_CERT_ERROR_INVALID_CHAIN;
        }
    }
    
    return iccoa_verify_certificate(cert, signer_cert->public_key, config);
}

/******************************************************************************
 * 证书工具函数
 ******************************************************************************/

/**
 * @brief 检查证书大小是否符合 ICCOA 规范
 */
bool iccoa_check_cert_size(const iccoa_certificate_t *cert)
{
    if (cert == NULL) {
        return false;
    }
    
    switch (cert->type) {
        case ICCOA_CERT_TYPE_OWNER_DK:
            return cert->der_len <= ICCOA_MAX_OWNER_CERT_SIZE;
            
        case ICCOA_CERT_TYPE_MID_SHARE:
            return cert->der_len <= ICCOA_MAX_MIDSHARE_CERT_SIZE;
            
        case ICCOA_CERT_TYPE_SHARED_DK:
            return cert->der_len <= ICCOA_MAX_SHARED_CERT_SIZE;
            
        case ICCOA_CERT_TYPE_SHARED_DK_V2:
            return cert->der_len <= ICCOA_MAX_SHARED_CERT_V2_SIZE;
            
        case ICCOA_CERT_TYPE_VEHICLE_OEM_CA:
        case ICCOA_CERT_TYPE_VEHICLE:
            /* 车CA和车证书没有大小限制 */
            return true;
            
        default:
            return false;
    }
}

/**
 * @brief 获取证书类型的文本描述
 */
const char* iccoa_cert_type_to_string(iccoa_cert_type_t type)
{
    switch (type) {
        case ICCOA_CERT_TYPE_VEHICLE_OEM_CA:
            return "VehicleOemCA";
        case ICCOA_CERT_TYPE_VEHICLE:
            return "Vehicle";
        case ICCOA_CERT_TYPE_OWNER_DK:
            return "OwnerDK";
        case ICCOA_CERT_TYPE_MID_SHARE:
            return "MidShare";
        case ICCOA_CERT_TYPE_SHARED_DK:
            return "SharedDK";
        case ICCOA_CERT_TYPE_SHARED_DK_V2:
            return "SharedDK_V2";
        default:
            return "Unknown";
    }
}

/**
 * @brief 获取证书模式的文本描述
 */
const char* iccoa_cert_mode_to_string(iccoa_cert_mode_t mode)
{
    switch (mode) {
        case ICCOA_CERT_MODE_CA:
            return "CA";
        case ICCOA_CERT_MODE_NON_CA:
            return "Non-CA";
        default:
            return "Unknown";
    }
}

/**
 * @brief 初始化证书验证器配置为默认值
 */
void iccoa_cert_validator_config_init(iccoa_cert_validator_config_t *config)
{
    if (config != NULL) {
        config->max_clock_skew_seconds = 300;  /* 5分钟 */
        config->allow_self_signed = false;
        config->strict_size_check = true;
    }
}

/**
 * @brief 清零证书结构
 */
void iccoa_certificate_clear(iccoa_certificate_t *cert)
{
    if (cert != NULL) {
        memset(cert, 0, sizeof(iccoa_certificate_t));
    }
}

/**
 * @brief 复制证书结构
 */
void iccoa_certificate_copy(iccoa_certificate_t *dst, const iccoa_certificate_t *src)
{
    if (dst != NULL && src != NULL) {
        memcpy(dst, src, sizeof(iccoa_certificate_t));
    }
}

/**
 * @brief 比较两个证书是否相同
 */
bool iccoa_certificate_equals(const iccoa_certificate_t *cert1, const iccoa_certificate_t *cert2)
{
    if (cert1 == NULL || cert2 == NULL) {
        return false;
    }
    
    return (memcmp(cert1->serial_number, cert2->serial_number, 16) == 0 &&
            cert1->type == cert2->type);
}

/**
 * @brief 获取证书序列化后的长度
 */
size_t iccoa_get_certificate_length(const iccoa_certificate_t *cert)
{
    if (cert == NULL) {
        return 0;
    }
    return cert->der_len;
}

/**
 * @brief 验证 ICCOA 证书链
 */
error_t iccoa_validate_cert_chain(
    const iccoa_cert_chain_t *cert_chain,
    const iccoa_certificate_t *trusted_root,
    iccoa_cert_mode_t mode,
    const iccoa_cert_validator_config_t *config)
{
    if (cert_chain == NULL || trusted_root == NULL || cert_chain->cert_count == 0) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 验证根证书 */
    if (cert_chain->cert_count > 0) {
        error_t err = iccoa_verify_certificate(&cert_chain->certs[cert_chain->cert_count - 1],
                                               trusted_root->public_key, config);
        if (err != OK) {
            return err;
        }
    }
    
    /* 逐级验证证书链 */
    for (int i = cert_chain->cert_count - 2; i >= 0; i--) {
        error_t err = iccoa_verify_certificate(&cert_chain->certs[i],
                                               cert_chain->certs[i + 1].public_key, config);
        if (err != OK) {
            return ICCOA_CERT_ERROR_INVALID_CHAIN;
        }
    }
    
    return OK;
}
