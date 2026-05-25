/******************************************************************************
 * @file    icce_certificate.c
 * @brief   ICCE 数字钥匙证书管理实现 - 国密算法版本
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-15
 * 
 * @note    
 *          - ICCE Digital Key 规范
 *          - SM2/SM3/SM4 国密算法
 *          - 自定义二进制格式 (非X.509)
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "icce.h"
#include "icce_certificate.h"
#include "dkcs.h"
#include "sm3.h"
#include "sm2.h"

/******************************************************************************
 * 内部常量定义
 ******************************************************************************/

/* 证书魔法头 */
#define ICCE_CERT_MAGIC                 0x49434345   /* "ICCE" */

/* 证书版本 */
#define ICCE_CERT_VERSION_1             0x01

/* 字段标识 */
#define ICCE_FIELD_VERSION              0x01
#define ICCE_FIELD_CERT_TYPE            0x02
#define ICCE_FIELD_CERT_LEN             0x03
#define ICCE_FIELD_ISSUER               0x04
#define ICCE_FIELD_SUBJECT              0x05
#define ICCE_FIELD_DEVICE_ID            0x06
#define ICCE_FIELD_VEHICLE_ID           0x07
#define ICCE_FIELD_KEY_ID               0x08
#define ICCE_FIELD_VALID_FROM           0x09
#define ICCE_FIELD_VALID_UNTIL          0x0A
#define ICCE_FIELD_PUBLIC_KEY           0x0B
#define ICCE_FIELD_SIGNATURE            0x0C
#define ICCE_FIELD_PERMISSIONS          0x0D
#define ICCE_FIELD_KEY_USAGE            0x0E
#define ICCE_FIELD_IS_CA                0x0F
#define ICCE_FIELD_MAX_PATH_LEN         0x10

#define ICCE_FIELD_END_MARKER           0xFF

/******************************************************************************
 * 内部辅助函数前向声明
 ******************************************************************************/

static size_t encode_field_uint8(uint8_t field_id, uint8_t value, uint8_t *out);
static size_t encode_field_uint16(uint8_t field_id, uint16_t value, uint8_t *out);
static size_t encode_field_uint32(uint8_t field_id, uint32_t value, uint8_t *out);
static size_t encode_field_data(uint8_t field_id, const uint8_t *data, uint16_t len, uint8_t *out);

static int decode_field_header(const uint8_t *data, size_t data_len, 
                                uint8_t *field_id, uint16_t *field_len, size_t *header_len);

static error_t sm3_hash(const uint8_t *data, size_t len, uint8_t digest[32]);

/******************************************************************************
 * 证书初始化和清理
 ******************************************************************************/

void icce_certificate_init(icce_certificate_t *cert)
{
    if (cert == NULL) {
        return;
    }
    
    memset(cert, 0, sizeof(icce_certificate_t));
    cert->version = ICCE_CERT_CURRENT_VERSION;
    cert->max_path_len = 3;  /* 默认最大3级证书链 */
}

void icce_certificate_clear(icce_certificate_t *cert)
{
    if (cert == NULL) {
        return;
    }
    
    /* 安全清除敏感数据 */
    memset(cert->public_key, 0, sizeof(cert->public_key));
    memset(cert->signature, 0, sizeof(cert->signature));
    memset(cert->raw_data, 0, sizeof(cert->raw_data));
    
    /* 清除整个结构 */
    memset(cert, 0, sizeof(icce_certificate_t));
}

void icce_certificate_copy(icce_certificate_t *dst, const icce_certificate_t *src)
{
    if (dst == NULL || src == NULL) {
        return;
    }
    
    memcpy(dst, src, sizeof(icce_certificate_t));
}

bool icce_certificate_equals(const icce_certificate_t *cert1, const icce_certificate_t *cert2)
{
    if (cert1 == NULL || cert2 == NULL) {
        return false;
    }
    
    /* 比较关键字段 */
    if (cert1->version != cert2->version ||
        cert1->cert_type != cert2->cert_type ||
        cert1->cert_len != cert2->cert_len) {
        return false;
    }
    
    /* 比较标识信息 */
    if (memcmp(cert1->device_id, cert2->device_id, ICCE_DEVICE_ID_LEN) != 0 ||
        memcmp(cert1->key_id, cert2->key_id, ICCE_KEY_ID_LEN) != 0) {
        return false;
    }
    
    /* 比较公钥 */
    if (memcmp(cert1->public_key, cert2->public_key, ICCE_ECC_SM2_PUB_KEY_LEN) != 0) {
        return false;
    }
    
    return true;
}

/******************************************************************************
 * 字段编码辅助函数
 ******************************************************************************/

static size_t encode_field_uint8(uint8_t field_id, uint8_t value, uint8_t *out)
{
    out[0] = field_id;
    out[1] = 1;  /* 长度 = 1 */
    out[2] = value;
    return 3;
}

static size_t encode_field_uint16(uint8_t field_id, uint16_t value, uint8_t *out)
{
    out[0] = field_id;
    out[1] = 2;  /* 长度 = 2 */
    out[2] = (value >> 8) & 0xFF;
    out[3] = value & 0xFF;
    return 4;
}

static size_t encode_field_uint32(uint8_t field_id, uint32_t value, uint8_t *out)
{
    out[0] = field_id;
    out[1] = 4;  /* 长度 = 4 */
    out[2] = (value >> 24) & 0xFF;
    out[3] = (value >> 16) & 0xFF;
    out[4] = (value >> 8) & 0xFF;
    out[5] = value & 0xFF;
    return 6;
}

static size_t encode_field_data(uint8_t field_id, const uint8_t *data, uint16_t len, uint8_t *out)
{
    out[0] = field_id;
    out[1] = (len >> 8) & 0xFF;
    out[2] = len & 0xFF;
    if (len > 0 && data != NULL) {
        memcpy(&out[3], data, len);
    }
    return 3 + len;
}

/******************************************************************************
 * 证书序列化
 ******************************************************************************/

error_t icce_serialize_certificate(const icce_certificate_t *cert, uint8_t *out, size_t *out_len)
{
    if (cert == NULL || out == NULL || out_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (*out_len < ICCE_MIN_CERT_SIZE) {
        return ICCE_CERT_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    
    /* 魔法头 (4字节) */
    uint32_t magic = ICCE_CERT_MAGIC;
    memcpy(&out[offset], &magic, 4);
    offset += 4;
    
    /* 版本 */
    offset += encode_field_uint8(ICCE_FIELD_VERSION, cert->version, &out[offset]);
    
    /* 证书类型 */
    offset += encode_field_uint8(ICCE_FIELD_CERT_TYPE, cert->cert_type, &out[offset]);
    
    /* 颁发者 */
    offset += encode_field_data(ICCE_FIELD_ISSUER, cert->issuer, cert->issuer_len, &out[offset]);
    
    /* 主体 */
    offset += encode_field_data(ICCE_FIELD_SUBJECT, cert->subject, cert->subject_len, &out[offset]);
    
    /* 设备ID */
    offset += encode_field_data(ICCE_FIELD_DEVICE_ID, cert->device_id, ICCE_DEVICE_ID_LEN, &out[offset]);
    
    /* 车辆ID */
    offset += encode_field_data(ICCE_FIELD_VEHICLE_ID, cert->vehicle_id, ICCE_VEHICLE_ID_LEN, &out[offset]);
    
    /* 钥匙ID */
    offset += encode_field_data(ICCE_FIELD_KEY_ID, cert->key_id, ICCE_KEY_ID_LEN, &out[offset]);
    
    /* 有效期 */
    offset += encode_field_uint32(ICCE_FIELD_VALID_FROM, cert->valid_from, &out[offset]);
    offset += encode_field_uint32(ICCE_FIELD_VALID_UNTIL, cert->valid_until, &out[offset]);
    
    /* 公钥 */
    offset += encode_field_data(ICCE_FIELD_PUBLIC_KEY, cert->public_key, ICCE_ECC_SM2_PUB_KEY_LEN, &out[offset]);
    
    /* 权限 */
    offset += encode_field_uint32(ICCE_FIELD_PERMISSIONS, cert->permissions, &out[offset]);
    
    /* 密钥用途 */
    offset += encode_field_uint16(ICCE_FIELD_KEY_USAGE, cert->key_usage, &out[offset]);
    
    /* CA标志 */
    offset += encode_field_uint8(ICCE_FIELD_IS_CA, cert->is_ca ? 1 : 0, &out[offset]);
    
    /* 最大路径长度 */
    offset += encode_field_uint8(ICCE_FIELD_MAX_PATH_LEN, cert->max_path_len, &out[offset]);
    
    /* 结束标记 */
    offset += encode_field_uint8(ICCE_FIELD_END_MARKER, 0, &out[offset]);
    
    /* 签名 (64字节) */
    offset += encode_field_data(ICCE_FIELD_SIGNATURE, cert->signature, ICCE_SIGNATURE_LEN, &out[offset]);
    
    /* 证书总长度 (包含证书头到签名的全部数据) */
    /* 这里我们不重新编码，直接使用offset作为总长度 */
    
    /* 更新cert_len字段 */
    cert->cert_len = (uint16_t)offset;
    
    /* 检查大小限制 */
    if (offset > ICCE_MAX_CERT_SIZE) {
        return ICCE_CERT_ERROR_SIZE_EXCEEDED;
    }
    
    *out_len = offset;
    
    /* 保存到 raw_data */
    if (cert->raw_len == 0) {
        memcpy(cert->raw_data, out, offset);
        cert->raw_len = (uint16_t)offset;
    }
    
    return OK;
}

/******************************************************************************
 * 字段解码辅助函数
 ******************************************************************************/

static int decode_field_header(const uint8_t *data, size_t data_len,
                                uint8_t *field_id, uint16_t *field_len, size_t *header_len)
{
    if (data_len < 1) {
        return -1;
    }
    
    *field_id = data[0];
    
    if (*field_id == ICCE_FIELD_END_MARKER) {
        *field_len = 0;
        *header_len = 1;
        return 0;
    }
    
    if (data_len < 3) {
        return -1;
    }
    
    *field_len = ((uint16_t)data[1] << 8) | data[2];
    *header_len = 3;
    
    return 0;
}

/******************************************************************************
 * 证书解析
 ******************************************************************************/

error_t icce_parse_certificate(const uint8_t *data, size_t data_len, icce_certificate_t *cert)
{
    if (data == NULL || cert == NULL || data_len < ICCE_MIN_CERT_SIZE) {
        return ERROR_INVALID_PARAM;
    }
    
    if (data_len > ICCE_MAX_CERT_SIZE) {
        return ICCE_CERT_ERROR_SIZE_EXCEEDED;
    }
    
    /* 检查魔法头 */
    uint32_t magic;
    memcpy(&magic, data, 4);
    if (magic != ICCE_CERT_MAGIC) {
        return ICCE_CERT_ERROR_INVALID_FORMAT;
    }
    
    /* 清零输出结构 */
    memset(cert, 0, sizeof(icce_certificate_t));
    
    size_t offset = 4;  /* 跳过魔法头 */
    
    while (offset < data_len) {
        uint8_t field_id;
        uint16_t field_len;
        size_t header_len;
        
        if (decode_field_header(&data[offset], data_len - offset, 
                                 &field_id, &field_len, &header_len) < 0) {
            return ICCE_CERT_ERROR_DECODE_FAILED;
        }
        
        offset += header_len;
        
        if (field_id == ICCE_FIELD_END_MARKER) {
            break;
        }
        
        /* 检查数据长度 */
        if (offset + field_len > data_len) {
            return ICCE_CERT_ERROR_DECODE_FAILED;
        }
        
        const uint8_t *field_data = &data[offset];
        offset += field_len;
        
        /* 解析各字段 */
        switch (field_id) {
            case ICCE_FIELD_VERSION:
                if (field_len == 1) cert->version = field_data[0];
                break;
                
            case ICCE_FIELD_CERT_TYPE:
                if (field_len == 1) cert->cert_type = field_data[0];
                break;
                
            case ICCE_FIELD_ISSUER:
                cert->issuer_len = field_len < 64 ? field_len : 64;
                memcpy(cert->issuer, field_data, cert->issuer_len);
                break;
                
            case ICCE_FIELD_SUBJECT:
                cert->subject_len = field_len < 64 ? field_len : 64;
                memcpy(cert->subject, field_data, cert->subject_len);
                break;
                
            case ICCE_FIELD_DEVICE_ID:
                if (field_len == ICCE_DEVICE_ID_LEN) {
                    memcpy(cert->device_id, field_data, ICCE_DEVICE_ID_LEN);
                }
                break;
                
            case ICCE_FIELD_VEHICLE_ID:
                if (field_len == ICCE_VEHICLE_ID_LEN) {
                    memcpy(cert->vehicle_id, field_data, ICCE_VEHICLE_ID_LEN);
                }
                break;
                
            case ICCE_FIELD_KEY_ID:
                if (field_len == ICCE_KEY_ID_LEN) {
                    memcpy(cert->key_id, field_data, ICCE_KEY_ID_LEN);
                }
                break;
                
            case ICCE_FIELD_VALID_FROM:
                if (field_len == 4) {
                    cert->valid_from = ((uint32_t)field_data[0] << 24) |
                                       ((uint32_t)field_data[1] << 16) |
                                       ((uint32_t)field_data[2] << 8) |
                                       field_data[3];
                }
                break;
                
            case ICCE_FIELD_VALID_UNTIL:
                if (field_len == 4) {
                    cert->valid_until = ((uint32_t)field_data[0] << 24) |
                                        ((uint32_t)field_data[1] << 16) |
                                        ((uint32_t)field_data[2] << 8) |
                                        field_data[3];
                }
                break;
                
            case ICCE_FIELD_PUBLIC_KEY:
                if (field_len == ICCE_ECC_SM2_PUB_KEY_LEN) {
                    memcpy(cert->public_key, field_data, ICCE_ECC_SM2_PUB_KEY_LEN);
                }
                break;
                
            case ICCE_FIELD_SIGNATURE:
                if (field_len == ICCE_SIGNATURE_LEN) {
                    memcpy(cert->signature, field_data, ICCE_SIGNATURE_LEN);
                }
                break;
                
            case ICCE_FIELD_PERMISSIONS:
                if (field_len == 4) {
                    cert->permissions = ((uint32_t)field_data[0] << 24) |
                                        ((uint32_t)field_data[1] << 16) |
                                        ((uint32_t)field_data[2] << 8) |
                                        field_data[3];
                }
                break;
                
            case ICCE_FIELD_KEY_USAGE:
                if (field_len == 2) {
                    cert->key_usage = ((uint16_t)field_data[0] << 8) | field_data[1];
                }
                break;
                
            case ICCE_FIELD_IS_CA:
                if (field_len == 1) cert->is_ca = (field_data[0] != 0);
                break;
                
            case ICCE_FIELD_MAX_PATH_LEN:
                if (field_len == 1) cert->max_path_len = field_data[0];
                break;
                
            default:
                /* 忽略未知字段 */
                break;
        }
    }
    
    /* 跳过结束标记后的签名 */
    if (offset + ICCE_SIGNATURE_LEN <= data_len) {
        memcpy(cert->signature, &data[offset], ICCE_SIGNATURE_LEN);
        offset += ICCE_SIGNATURE_LEN;
    }
    
    cert->cert_len = (uint16_t)data_len;
    cert->raw_len = (uint16_t)data_len;
    memcpy(cert->raw_data, data, data_len);
    
    /* 检查必需字段 */
    if (cert->version == 0 || cert->cert_type == 0) {
        return ICCE_CERT_ERROR_INVALID_FORMAT;
    }
    
    return OK;
}

/******************************************************************************
 * SM3 哈希实现
 ******************************************************************************/

static error_t sm3_hash(const uint8_t *data, size_t len, uint8_t digest[32])
{
    if (data == NULL || digest == NULL) {
        return ERROR_INVALID_PARAM;
    }

    sm3_digest(data, len, digest);
    return OK;
}

/******************************************************************************
 * SM2 签名验证
 ******************************************************************************/

error_t icce_sm2_verify_signature(const uint8_t digest[32],
                                   const uint8_t signature[ICCE_SIGNATURE_LEN],
                                   const uint8_t public_key[ICCE_ECC_SM2_PUB_KEY_LEN])
{
    if (digest == NULL || signature == NULL || public_key == NULL) {
        return ERROR_INVALID_PARAM;
    }

    int ret = sm2_verify(public_key, digest, signature);
    if (ret == 0) return OK;
    if (ret == -2) return ERROR_INVALID_PARAM;
    return ICCE_ERR_VERIFY_SIGNATURE;
}

/******************************************************************************
 * 证书哈希计算
 ******************************************************************************/

error_t icce_cert_compute_hash(const icce_certificate_t *cert, uint8_t digest[32])
{
    if (cert == NULL || digest == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 使用序列化后的数据计算哈希 (排除签名字段) */
    uint8_t buffer[ICCE_MAX_CERT_SIZE];
    size_t len = sizeof(buffer);
    
    /* 复制证书并清除签名 */
    icce_certificate_t temp_cert;
    memcpy(&temp_cert, cert, sizeof(icce_certificate_t));
    memset(temp_cert.signature, 0, sizeof(temp_cert.signature));
    
    error_t ret = icce_serialize_certificate(&temp_cert, buffer, &len);
    if (ret != OK) {
        return ret;
    }
    
    /* 计算SM3哈希 - 只对证书头部分哈希，不包括签名 */
    /* 需要重新序列化确保不包含签名 */
    /* 实际应计算 raw_data 中除去签名部分的哈希 */
    
    /* 简化实现: 对 raw_data 中排除最后64字芋签名的数据进行哈希 */
    size_t hash_len = cert->raw_len;
    if (hash_len > ICCE_SIGNATURE_LEN) {
        hash_len -= ICCE_SIGNATURE_LEN;
    }
    
    return sm3_hash(cert->raw_data, hash_len, digest);
}

/******************************************************************************
 * 证书验证
 ******************************************************************************/

error_t icce_verify_certificate(const icce_certificate_t *cert,
                                 const uint8_t *trusted_pubkey,
                                 const icce_cert_validator_config_t *config)
{
    if (cert == NULL || trusted_pubkey == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 使用默认配置 */
    icce_cert_validator_config_t default_config;
    if (config == NULL) {
        icce_cert_validator_config_init(&default_config);
        config = &default_config;
    }
    
    /* 检查证书大小 */
    if (config->strict_size_check && !icce_check_cert_size(cert)) {
        return ICCE_CERT_ERROR_SIZE_EXCEEDED;
    }
    
    /* 检查时间有效性 */
    if (config->verify_time) {
        if (icce_cert_is_expired(cert, 0)) {
            return ICCE_CERT_ERROR_EXPIRED;
        }
        if (!icce_cert_is_valid_now(cert, 0)) {
            return ICCE_CERT_ERROR_NOT_YET_VALID;
        }
    }
    
    /* 计算证书哈希 */
    uint8_t digest[32];
    error_t ret = icce_cert_compute_hash(cert, digest);
    if (ret != OK) {
        return ret;
    }
    
    /* 验证 SM2 签名 */
    ret = icce_sm2_verify_signature(digest, cert->signature, trusted_pubkey);
    if (ret != OK) {
        return ICCE_CERT_ERROR_INVALID_SIGNATURE;
    }
    
    return OK;
}

/******************************************************************************
 * 证书链验证
 ******************************************************************************/

error_t icce_validate_cert_chain(const icce_cert_chain_t *cert_chain,
                                  const icce_certificate_t *trusted_root,
                                  const icce_cert_validator_config_t *config)
{
    if (cert_chain == NULL || trusted_root == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (cert_chain->cert_count == 0 || cert_chain->cert_count > ICCE_MAX_CERT_CHAIN_LEN) {
        return ICCE_CERT_ERROR_INVALID_CHAIN;
    }
    
    /* 使用默认配置 */
    icce_cert_validator_config_t default_config;
    if (config == NULL) {
        icce_cert_validator_config_init(&default_config);
        config = &default_config;
    }
    
    /* 从叶子证书开始逐级验证到根 */
    const uint8_t *current_pubkey = NULL;
    
    for (int i = cert_chain->cert_count - 1; i >= 0; i--) {
        const icce_certificate_t *cert = &cert_chain->certs[i];
        
        if (i == cert_chain->cert_count - 1) {
            /* 最顶层应由根证书验证 */
            current_pubkey = trusted_root->public_key;
        } else {
            /* 下一层的公钥验证当前层 */
            current_pubkey = cert_chain->certs[i + 1].public_key;
        }
        
        error_t ret = icce_verify_certificate(cert, current_pubkey, config);
        if (ret != OK) {
            return ret;
        }
        
        /* 检查证书链深度 */
        if (cert->max_path_len < (uint8_t)(cert_chain->cert_count - i - 1)) {
            return ICCE_CERT_ERROR_INVALID_CHAIN;
        }
    }
    
    return OK;
}

/******************************************************************************
 * 特定类型证书验证
 ******************************************************************************/

error_t icce_validate_owner_cert(const icce_certificate_t *cert,
                                  const icce_certificate_t *ca_cert,
                                  const icce_cert_validator_config_t *config)
{
    if (cert == NULL || ca_cert == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 检查证书类型 */
    if (cert->cert_type != ICCE_CERT_TYPE_OWNER_DK) {
        return ICCE_CERT_ERROR_INVALID_TYPE;
    }
    
    /* 检查CA类型 */
    if (ca_cert->cert_type != ICCE_CERT_TYPE_VEHICLE_CA) {
        return ICCE_CERT_ERROR_INVALID_TYPE;
    }
    
    /* 验证签名 */
    error_t ret = icce_verify_certificate(cert, ca_cert->public_key, config);
    if (ret != OK) {
        return ret;
    }
    
    return OK;
}

error_t icce_validate_shared_cert(const icce_certificate_t *cert,
                                   const icce_certificate_t *signer_cert,
                                   const icce_certificate_t *ca_cert,
                                   const icce_cert_validator_config_t *config)
{
    if (cert == NULL || signer_cert == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 检查证书类型 */
    if (cert->cert_type != ICCE_CERT_TYPE_SHARED_DK) {
        return ICCE_CERT_ERROR_INVALID_TYPE;
    }
    
    /* 检查签发者类型 - 可以是车主证书或车CA */
    if (signer_cert->cert_type != ICCE_CERT_TYPE_OWNER_DK &&
        signer_cert->cert_type != ICCE_CERT_TYPE_VEHICLE_CA) {
        return ICCE_CERT_ERROR_INVALID_TYPE;
    }
    
    /* 如果签发者是车主证书，需要验证车主证书 */
    if (signer_cert->cert_type == ICCE_CERT_TYPE_OWNER_DK) {
        if (ca_cert == NULL) {
            return ERROR_INVALID_PARAM;
        }
        error_t ret = icce_validate_owner_cert(signer_cert, ca_cert, config);
        if (ret != OK) {
            return ret;
        }
    }
    
    /* 验证分享证书 */
    error_t ret = icce_verify_certificate(cert, signer_cert->public_key, config);
    if (ret != OK) {
        return ret;
    }
    
    return OK;
}

/******************************************************************************
 * 工具函数
 ******************************************************************************/

size_t icce_get_certificate_length(const icce_certificate_t *cert)
{
    if (cert == NULL) {
        return 0;
    }
    
    return cert->cert_len;
}

bool icce_check_cert_size(const icce_certificate_t *cert)
{
    if (cert == NULL) {
        return false;
    }
    
    return (cert->cert_len >= ICCE_MIN_CERT_SIZE && cert->cert_len <= ICCE_MAX_CERT_SIZE);
}

bool icce_cert_is_expired(const icce_certificate_t *cert, uint32_t current_time)
{
    if (cert == NULL) {
        return true;
    }
    
    if (current_time == 0) {
        current_time = (uint32_t)time(NULL);
    }
    
    return (current_time > cert->valid_until);
}

bool icce_cert_is_valid_now(const icce_certificate_t *cert, uint32_t current_time)
{
    if (cert == NULL) {
        return false;
    }
    
    if (current_time == 0) {
        current_time = (uint32_t)time(NULL);
    }
    
    return (current_time >= cert->valid_from && current_time <= cert->valid_until);
}

const char* icce_cert_type_to_string(icce_cert_type_t type)
{
    switch (type) {
        case ICCE_CERT_TYPE_VEHICLE_CA:
            return "Vehicle CA";
        case ICCE_CERT_TYPE_VEHICLE:
            return "Vehicle";
        case ICCE_CERT_TYPE_OWNER_DK:
            return "Owner Digital Key";
        case ICCE_CERT_TYPE_SHARED_DK:
            return "Shared Digital Key";
        case ICCE_CERT_TYPE_TEMP_ACCESS:
            return "Temporary Access";
        default:
            return "Unknown";
    }
}

const char* icce_cert_error_to_string(int error)
{
    switch (error) {
        case ICCE_CERT_OK:
            return "Success";
        case ICCE_CERT_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case ICCE_CERT_ERROR_INVALID_FORMAT:
            return "Invalid certificate format";
        case ICCE_CERT_ERROR_EXPIRED:
            return "Certificate expired";
        case ICCE_CERT_ERROR_NOT_YET_VALID:
            return "Certificate not yet valid";
        case ICCE_CERT_ERROR_INVALID_SIGNATURE:
            return "Invalid signature";
        case ICCE_CERT_ERROR_UNSUPPORTED_ALGORITHM:
            return "Unsupported algorithm";
        case ICCE_CERT_ERROR_SIZE_EXCEEDED:
            return "Certificate size exceeded";
        case ICCE_CERT_ERROR_INVALID_CHAIN:
            return "Invalid certificate chain";
        case ICCE_CERT_ERROR_TRUST_ANCHOR_NOT_FOUND:
            return "Trust anchor not found";
        case ICCE_CERT_ERROR_REVOKED:
            return "Certificate revoked";
        case ICCE_CERT_ERROR_INVALID_TYPE:
            return "Invalid certificate type";
        case ICCE_CERT_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case ICCE_CERT_ERROR_DECODE_FAILED:
            return "Decode failed";
        case ICCE_CERT_ERROR_ENCODE_FAILED:
            return "Encode failed";
        default:
            return "Unknown error";
    }
}

void icce_cert_validator_config_init(icce_cert_validator_config_t *config)
{
    if (config == NULL) {
        return;
    }
    
    config->max_clock_skew_seconds = 300;    /* 5分钟时钟偏移容忍 */
    config->allow_self_signed = false;        /* 不允许自签名 */
    config->strict_size_check = true;         /* 严格大小检查 */
    config->verify_time = true;               /* 验证时间 */
    config->verify_permissions = true;        /* 验证权限 */
}

/******************************************************************************
 * APDU 传输封装
 ******************************************************************************/

error_t icce_cert_to_apdu(const icce_certificate_t *cert,
                           uint8_t *apdu_data,
                           size_t *apdu_len,
                           bool is_last)
{
    if (cert == NULL || apdu_data == NULL || apdu_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 如果证书已经序列化，直接复制 */
    if (cert->raw_len > 0 && cert->raw_len <= ICCE_MAX_CERT_SIZE) {
        if (*apdu_len < cert->raw_len) {
            return ICCE_CERT_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(apdu_data, cert->raw_data, cert->raw_len);
        *apdu_len = cert->raw_len;
        return OK;
    }
    
    /* 否则先序列化 */
    return icce_serialize_certificate(cert, apdu_data, apdu_len);
}

error_t icce_cert_from_apdu(const uint8_t *apdu_data,
                             size_t apdu_len,
                             icce_certificate_t *cert)
{
    if (apdu_data == NULL || cert == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (apdu_len < ICCE_MIN_CERT_SIZE || apdu_len > ICCE_MAX_CERT_SIZE) {
        return ICCE_CERT_ERROR_INVALID_FORMAT;
    }
    
    return icce_parse_certificate(apdu_data, apdu_len, cert);
}

/******************************************************************************
 * 测试证书生成 (仅用于测试)
 ******************************************************************************/

error_t icce_generate_test_certificate(icce_certificate_t *cert,
                                        icce_cert_type_t type,
                                        const uint8_t *subject,
                                        size_t subject_len,
                                        const uint8_t key_pair[96])
{
    if (cert == NULL || subject == NULL || key_pair == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 初始化证书 */
    icce_certificate_init(cert);
    
    cert->cert_type = (uint8_t)type;
    
    /* 复制主题 */
    cert->subject_len = (uint16_t)(subject_len < 64 ? subject_len : 64);
    memcpy(cert->subject, subject, cert->subject_len);
    
    /* 复制公钥 (key_pair[32:97] 是公钥) */
    memcpy(cert->public_key, &key_pair[32], ICCE_ECC_SM2_PUB_KEY_LEN);
    
    /* 设置有效期 (1年) */
    cert->valid_from = (uint32_t)time(NULL);
    cert->valid_until = cert->valid_from + 365 * 24 * 3600;
    
    /* 设置权限 */
    cert->permissions = 0xFFFFFFFF;  /* 全部权限 */
    cert->key_usage = ICCE_KEY_USAGE_DIGITAL_SIGNATURE | ICCE_KEY_USAGE_KEY_AGREEMENT;
    
    /* 设置CA标志 */
    cert->is_ca = (type == ICCE_CERT_TYPE_VEHICLE_CA) ? 1 : 0;
    
    /* 生成测试签名 (使用哈希值作为占位签名) */
    /* 注意: 这是测试用的占位签名，不具备真实安全性 */
    memset(cert->signature, 0xAA, ICCE_SIGNATURE_LEN);
    
    /* 序列化 */
    size_t len = sizeof(cert->raw_data);
    error_t ret = icce_serialize_certificate(cert, cert->raw_data, &len);
    if (ret != OK) {
        return ret;
    }
    
    cert->raw_len = (uint16_t)len;
    cert->cert_len = (uint16_t)len;
    
    return OK;
}
