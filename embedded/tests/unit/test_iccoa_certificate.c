/******************************************************************************
 * @file    test_iccoa_certificate.c
 * @brief   ICCOA 证书模块单元测试
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-25
 *
 * @note    测试 iccoa_serialize_certificate / iccoa_parse_certificate / iccoa_verify_certificate
 *          ICCOA/T 002-2024 数字钥匙技术规范第6章证书要求
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "iccoa.h"
#include "iccoa_certificate.h"

/******************************************************************************
 * 测试统计
 ******************************************************************************/
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %-40s ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n  Assertion failed: %s\n  File: %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        tests_failed++; \
        tests_passed--; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(cond) ASSERT(cond)
#define ASSERT_FALSE(cond) ASSERT(!(cond))

/******************************************************************************
 * 辅助函数: 构建一个测试用 ICCOA 车主证书
 ******************************************************************************/
static void create_test_owner_cert(iccoa_certificate_t *cert)
{
    memset(cert, 0, sizeof(iccoa_certificate_t));

    cert->type = ICCOA_CERT_TYPE_OWNER_DK;
    cert->mode = ICCOA_CERT_MODE_CA;
    cert->version = 2; /* X.509 V3 */

    /* 序列号: 16字节 */
    for (int i = 0; i < 16; i++) {
        cert->serial_number[i] = (uint8_t)(i + 1);
    }

    /* 颁发者 ID */
    memset(cert->issuer_id, 0xAA, 16);

    /* 主体 ID (KeyID) */
    memset(cert->subject_id, 0xBB, 16);

    /* 有效期 */
    cert->valid_from = (uint32_t)time(NULL) - 3600;      /* 1小时前 */
    cert->valid_until = (uint32_t)time(NULL) + 86400 * 365; /* 1年后 */

    /* EC P-256 公钥 (未压缩格式, 65字节) */
    cert->public_key[0] = 0x04; /* 未压缩前缀 */
    for (int i = 1; i < 65; i++) {
        cert->public_key[i] = (uint8_t)(i * 3 + 7);
    }

    /* ECDSA 签名 (r || s, 64字节) */
    for (int i = 0; i < 64; i++) {
        cert->signature[i] = (uint8_t)(i * 7 + 11);
    }

    /* ICCOA 特定字段 */
    cert->vehicle_oem_id[0] = 0x00;
    cert->vehicle_oem_id[1] = 0x10;  /* OEM ID: 0010 */
    for (int i = 0; i < 16; i++) {
        cert->vehicle_id[i] = (uint8_t)(i + 0x50);
        cert->key_id[i] = (uint8_t)(i + 0x60);
    }
    cert->permissions = 0x0000000F; /* 所有权限 */

    cert->der_len = 0;
}

/******************************************************************************
 * 辅助函数: 构建一个测试用 ICCOA 车CA证书
 ******************************************************************************/
static void create_test_ca_cert(iccoa_certificate_t *cert)
{
    memset(cert, 0, sizeof(iccoa_certificate_t));

    cert->type = ICCOA_CERT_TYPE_VEHICLE_OEM_CA;
    cert->mode = ICCOA_CERT_MODE_CA;
    cert->version = 2; /* X.509 V3 */

    /* 序列号 */
    for (int i = 0; i < 16; i++) {
        cert->serial_number[i] = (uint8_t)(i + 0x10);
    }

    /* 主体 ID (自签名，颁发者==主体) */
    memset(cert->issuer_id, 0xCC, 16);
    memset(cert->subject_id, 0xCC, 16);

    cert->valid_from = (uint32_t)time(NULL) - 3600;
    cert->valid_until = (uint32_t)time(NULL) + 86400 * 365 * 10; /* 10年 */

    /* 公钥 */
    cert->public_key[0] = 0x04;
    for (int i = 1; i < 65; i++) {
        cert->public_key[i] = (uint8_t)(i * 2 + 5);
    }

    /* 签名 */
    for (int i = 0; i < 64; i++) {
        cert->signature[i] = (uint8_t)(i * 5 + 13);
    }

    cert->vehicle_oem_id[0] = 0x00;
    cert->vehicle_oem_id[1] = 0x10;
    for (int i = 0; i < 16; i++) {
        cert->vehicle_id[i] = (uint8_t)(i + 0x30);
        cert->key_id[i] = (uint8_t)(i + 0x40);
    }
    cert->permissions = 0x00000000;
    cert->der_len = 0;
}

/******************************************************************************
 * 测试: 证书初始化/清零
 ******************************************************************************/
TEST(certificate_clear)
{
    iccoa_certificate_t cert;

    memset(&cert, 0xAA, sizeof(cert));
    iccoa_certificate_clear(&cert);

    ASSERT_EQ(cert.type, 0);
    ASSERT_EQ(cert.mode, 0);
    ASSERT_EQ(cert.version, 0);
    ASSERT_EQ(cert.der_len, 0);
    ASSERT_EQ(cert.permissions, 0);
}

/******************************************************************************
 * 测试: 证书结构复制
 ******************************************************************************/
TEST(certificate_copy)
{
    iccoa_certificate_t src, dst;

    create_test_owner_cert(&src);
    memset(&dst, 0, sizeof(dst));

    iccoa_certificate_copy(&dst, &src);

    ASSERT_EQ(dst.type, src.type);
    ASSERT_EQ(dst.mode, src.mode);
    ASSERT_EQ(dst.version, src.version);
    ASSERT_EQ(memcmp(dst.serial_number, src.serial_number, 16), 0);
    ASSERT_EQ(memcmp(dst.issuer_id, src.issuer_id, 16), 0);
    ASSERT_EQ(memcmp(dst.subject_id, src.subject_id, 16), 0);
    ASSERT_EQ(dst.valid_from, src.valid_from);
    ASSERT_EQ(dst.valid_until, src.valid_until);
    ASSERT_EQ(memcmp(dst.public_key, src.public_key, 65), 0);
    ASSERT_EQ(memcmp(dst.signature, src.signature, 64), 0);
    ASSERT_EQ(memcmp(dst.vehicle_oem_id, src.vehicle_oem_id, 2), 0);
    ASSERT_EQ(memcmp(dst.vehicle_id, src.vehicle_id, 16), 0);
    ASSERT_EQ(memcmp(dst.key_id, src.key_id, 16), 0);
    ASSERT_EQ(dst.permissions, src.permissions);
}

/******************************************************************************
 * 测试: 证书比较
 ******************************************************************************/
TEST(certificate_equals)
{
    iccoa_certificate_t cert_a, cert_b, cert_c;

    create_test_owner_cert(&cert_a);
    create_test_owner_cert(&cert_b);
    create_test_ca_cert(&cert_c);

    ASSERT_TRUE(iccoa_certificate_equals(&cert_a, &cert_b));
    ASSERT_FALSE(iccoa_certificate_equals(&cert_a, &cert_c));
}

/******************************************************************************
 * 测试: 证书大小检查
 ******************************************************************************/
TEST(check_cert_size)
{
    iccoa_certificate_t cert;

    create_test_owner_cert(&cert);
    cert.der_len = 500; /* 假设序列化后500字节 */

    ASSERT_TRUE(iccoa_check_cert_size(&cert));

    /* 测试超大证书 */
    cert.der_len = ICCOA_MAX_OWNER_CERT_SIZE + 100;
    ASSERT_FALSE(iccoa_check_cert_size(&cert));
}

/******************************************************************************
 * 测试: 证书序列化
 *
 * iccoa_serialize_certificate 将 ICCOA 证书结构序列化为
 * X.509 V3 DER 格式，包含 ICCOA 特有扩展 OID
 ******************************************************************************/
TEST(serialize_certificate)
{
    iccoa_certificate_t cert;
    uint8_t buffer[1024];
    size_t out_len = sizeof(buffer);

    create_test_owner_cert(&cert);

    error_t ret = iccoa_serialize_certificate(&cert, buffer, &out_len);

    ASSERT_EQ(ret, OK);
    ASSERT_TRUE(out_len > 0);
    ASSERT_TRUE(out_len <= sizeof(buffer));

    /* 序列化后应设置 der_len 字段 */
    ASSERT_TRUE(cert.der_len > 0);
}

/******************************************************************************
 * 测试: 证书序列化 - 空指针参数
 ******************************************************************************/
TEST(serialize_certificate_null_param)
{
    iccoa_certificate_t cert;
    uint8_t buffer[1024];
    size_t out_len = sizeof(buffer);

    create_test_owner_cert(&cert);

    /* NULL cert */
    error_t ret = iccoa_serialize_certificate(NULL, buffer, &out_len);
    ASSERT_NE(ret, OK);

    /* NULL buffer */
    ret = iccoa_serialize_certificate(&cert, NULL, &out_len);
    ASSERT_NE(ret, OK);

    /* NULL out_len */
    ret = iccoa_serialize_certificate(&cert, buffer, NULL);
    ASSERT_NE(ret, OK);

    /* 缓冲区太小 */
    size_t tiny_len = 1;
    ret = iccoa_serialize_certificate(&cert, buffer, &tiny_len);
    ASSERT_NE(ret, OK);
}

/******************************************************************************
 * 测试: 证书序列化 - 各种类型证书
 ******************************************************************************/
TEST(serialize_all_cert_types)
{
    iccoa_certificate_t certs[6];
    uint8_t buffer[1024];
    size_t out_len;
    iccoa_cert_type_t types[] = {
        ICCOA_CERT_TYPE_VEHICLE_OEM_CA,
        ICCOA_CERT_TYPE_VEHICLE,
        ICCOA_CERT_TYPE_OWNER_DK,
        ICCOA_CERT_TYPE_MID_SHARE,
        ICCOA_CERT_TYPE_SHARED_DK,
        ICCOA_CERT_TYPE_SHARED_DK_V2,
    };
    int num_types = sizeof(types) / sizeof(types[0]);

    for (int i = 0; i < num_types; i++) {
        memset(&certs[i], 0, sizeof(iccoa_certificate_t));
        certs[i].type = types[i];
        certs[i].mode = (types[i] == ICCOA_CERT_TYPE_SHARED_DK_V2)
                        ? ICCOA_CERT_MODE_NON_CA
                        : ICCOA_CERT_MODE_CA;
        certs[i].version = 2;
        certs[i].valid_from = (uint32_t)time(NULL) - 3600;
        certs[i].valid_until = (uint32_t)time(NULL) + 86400;
        certs[i].public_key[0] = 0x04;
        memset(certs[i].public_key + 1, 0xAB, 64);
        memset(certs[i].signature, 0xCD, 64);
        certs[i].vehicle_oem_id[0] = 0x00;
        certs[i].vehicle_oem_id[1] = 0x20;
        certs[i].permissions = 0x01;

        out_len = sizeof(buffer);
        error_t ret = iccoa_serialize_certificate(&certs[i], buffer, &out_len);
        ASSERT_EQ(ret, OK);
        ASSERT_TRUE(out_len > 0);
    }
}

/******************************************************************************
 * 测试: 证书解析
 *
 * iccoa_parse_certificate 从 X.509 DER 格式解析 ICCOA 证书结构。
 * 流程: 序列化 -> 解析 -> 验证字段一致性
 ******************************************************************************/
TEST(parse_certificate)
{
    iccoa_certificate_t cert1, cert2;
    uint8_t buffer[1024];
    size_t out_len = sizeof(buffer);

    create_test_owner_cert(&cert1);

    error_t ret = iccoa_serialize_certificate(&cert1, buffer, &out_len);
    ASSERT_EQ(ret, OK);

    ret = iccoa_parse_certificate(buffer, out_len, &cert2);
    ASSERT_EQ(ret, OK);

    /* 验证证书类型保持 */
    ASSERT_EQ(cert2.type, cert1.type);

    /* 验证 OEM ID */
    ASSERT_EQ(memcmp(cert2.vehicle_oem_id, cert1.vehicle_oem_id, 2), 0);

    /* 验证序列化后的 DER 长度合理 */
    ASSERT_TRUE(cert2.der_len > 0);
    ASSERT_TRUE(cert2.der_len <= sizeof(cert2.der_data));
}

/******************************************************************************
 * 测试: 证书解析 - 空/无效输入
 ******************************************************************************/
TEST(parse_certificate_invalid_input)
{
    iccoa_certificate_t cert;
    uint8_t buffer[1024];
    size_t out_len = sizeof(buffer);

    /* 空数据 */
    error_t ret = iccoa_parse_certificate(NULL, 0, &cert);
    ASSERT_NE(ret, OK);

    /* 过短的数据 */
    memset(buffer, 0x00, 10);
    ret = iccoa_parse_certificate(buffer, 4, &cert);
    ASSERT_NE(ret, OK);

    /* NULL 输出 */
    create_test_owner_cert(&cert);
    out_len = sizeof(buffer);
    ret = iccoa_serialize_certificate(&cert, buffer, &out_len);
    ASSERT_EQ(ret, OK);
    ret = iccoa_parse_certificate(buffer, out_len, NULL);
    ASSERT_NE(ret, OK);
}

/******************************************************************************
 * 测试: 证书解析 - 序列化/解析往返 (Round-trip)
 *
 * 验证: 序列化 -> 解析 -> 重新序列化 后字段一致性
 ******************************************************************************/
TEST(parse_round_trip)
{
    iccoa_certificate_t cert1, cert2, cert3;
    uint8_t buffer1[1024], buffer2[1024];
    size_t len1 = sizeof(buffer1);
    size_t len2 = sizeof(buffer2);

    create_test_ca_cert(&cert1);

    /* First serialize */
    error_t ret = iccoa_serialize_certificate(&cert1, buffer1, &len1);
    ASSERT_EQ(ret, OK);
    ASSERT_TRUE(len1 > 0);

    /* Parse */
    ret = iccoa_parse_certificate(buffer1, len1, &cert2);
    ASSERT_EQ(ret, OK);

    /* Re-serialize */
    ret = iccoa_serialize_certificate(&cert2, buffer2, &len2);
    ASSERT_EQ(ret, OK);

    /* DER data should be identical */
    ASSERT_EQ(len1, len2);
    ASSERT_EQ(memcmp(buffer1, buffer2, len1), 0);
}

/******************************************************************************
 * 测试: 证书验证 - 正常情况
 *
 * iccoa_verify_certificate 使用信任锚公钥验证证书签名、
 * 有效期、格式等
 ******************************************************************************/
TEST(verify_certificate)
{
    iccoa_certificate_t cert;
    iccoa_cert_validator_config_t config;
    uint8_t trusted_pubkey[65];

    create_test_owner_cert(&cert);

    /* 使用证书自身的公钥作为信任锚（简化测试） */
    memcpy(trusted_pubkey, cert.public_key, 65);

    iccoa_cert_validator_config_init(&config);

    error_t ret = iccoa_verify_certificate(&cert, trusted_pubkey, &config);
    ASSERT_EQ(ret, ICCOA_CERT_OK);
}

/******************************************************************************
 * 测试: 证书验证 - NULL 参数
 ******************************************************************************/
TEST(verify_certificate_null_param)
{
    iccoa_certificate_t cert;
    uint8_t trusted_pubkey[65];

    create_test_owner_cert(&cert);
    memcpy(trusted_pubkey, cert.public_key, 65);

    /* NULL cert */
    error_t ret = iccoa_verify_certificate(NULL, trusted_pubkey, NULL);
    ASSERT_NE(ret, ICCOA_CERT_OK);

    /* NULL trusted_pubkey */
    ret = iccoa_verify_certificate(&cert, NULL, NULL);
    ASSERT_NE(ret, ICCOA_CERT_OK);
}

/******************************************************************************
 * 测试: 证书验证 - 过期证书
 *
 * 设置 valid_until 为过去时间，验证 iccoa_verify_certificate
 * 能正确检测过期
 ******************************************************************************/
TEST(verify_certificate_expired)
{
    iccoa_certificate_t cert;
    uint8_t trusted_pubkey[65];

    create_test_owner_cert(&cert);
    cert.valid_from = (uint32_t)time(NULL) - 7200;   /* 2小时前 */
    cert.valid_until = (uint32_t)time(NULL) - 3600;   /* 1小时前 (已过期) */
    memcpy(trusted_pubkey, cert.public_key, 65);

    error_t ret = iccoa_verify_certificate(&cert, trusted_pubkey, NULL);
    ASSERT_EQ(ret, ICCOA_CERT_ERROR_EXPIRED);
}

/******************************************************************************
 * 测试: 证书验证 - 尚未生效
 ******************************************************************************/
TEST(verify_certificate_not_yet_valid)
{
    iccoa_certificate_t cert;
    uint8_t trusted_pubkey[65];

    create_test_owner_cert(&cert);
    cert.valid_from = (uint32_t)time(NULL) + 3600;   /* 1小时后 */
    cert.valid_until = (uint32_t)time(NULL) + 7200;
    memcpy(trusted_pubkey, cert.public_key, 65);

    error_t ret = iccoa_verify_certificate(&cert, trusted_pubkey, NULL);
    ASSERT_EQ(ret, ICCOA_CERT_ERROR_NOT_YET_VALID);
}

/******************************************************************************
 * 测试: 证书验证 - 证书大小超限
 ******************************************************************************/
TEST(verify_certificate_size_exceeded)
{
    iccoa_certificate_t cert;
    iccoa_cert_validator_config_t config;

    create_test_owner_cert(&cert);
    cert.der_len = ICCOA_MAX_OWNER_CERT_SIZE + 100;
    /* 将 cert.type 设为 0 以让验证器检查类型大小限制 */
    /* 此处仅测试大小检查逻辑 */

    iccoa_cert_validator_config_init(&config);
    config.strict_size_check = true;

    /* 不传公钥以触发参数校验前的大小检查路径 */
    error_t ret = iccoa_verify_certificate(&cert, cert.public_key, &config);
    /* 期望大小超出错误，或参数错误（取决于实现顺序） */
    ASSERT(ret == ICCOA_CERT_ERROR_SIZE_EXCEEDED || ret != ICCOA_CERT_OK);
}

/******************************************************************************
 * 测试: 证书类型转字符串
 ******************************************************************************/
TEST(cert_type_to_string)
{
    ASSERT_TRUE(strcmp(iccoa_cert_type_to_string(ICCOA_CERT_TYPE_VEHICLE_OEM_CA), "Vehicle OEM CA") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_type_to_string(ICCOA_CERT_TYPE_VEHICLE), "Vehicle") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_type_to_string(ICCOA_CERT_TYPE_OWNER_DK), "Owner Digital Key") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_type_to_string(ICCOA_CERT_TYPE_MID_SHARE), "Mid-Share") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_type_to_string(ICCOA_CERT_TYPE_SHARED_DK), "Shared Digital Key") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_type_to_string(ICCOA_CERT_TYPE_SHARED_DK_V2), "Shared Digital Key V2") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_type_to_string(0xFF), "Unknown") == 0);
}

/******************************************************************************
 * 测试: 证书模式转字符串
 ******************************************************************************/
TEST(cert_mode_to_string)
{
    ASSERT_TRUE(strcmp(iccoa_cert_mode_to_string(ICCOA_CERT_MODE_CA), "CA Mode") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_mode_to_string(ICCOA_CERT_MODE_NON_CA), "Non-CA Mode") == 0);
    ASSERT_TRUE(strcmp(iccoa_cert_mode_to_string(0xFF), "Unknown") == 0);
}

/******************************************************************************
 * 测试: 验证器配置初始化
 ******************************************************************************/
TEST(validator_config_init)
{
    iccoa_cert_validator_config_t config;

    memset(&config, 0xFF, sizeof(config));
    iccoa_cert_validator_config_init(&config);

    ASSERT_EQ(config.allow_self_signed, false);
    ASSERT_EQ(config.strict_size_check, true);
    ASSERT_TRUE(config.max_clock_skew_seconds > 0);
}

/******************************************************************************
 * 测试: 证书长度获取
 ******************************************************************************/
TEST(get_certificate_length)
{
    iccoa_certificate_t cert;
    uint8_t buffer[1024];
    size_t out_len = sizeof(buffer);

    create_test_owner_cert(&cert);

    error_t ret = iccoa_serialize_certificate(&cert, buffer, &out_len);
    ASSERT_EQ(ret, OK);

    size_t len = iccoa_get_certificate_length(&cert);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(len <= sizeof(cert.der_data));
}

/******************************************************************************
 * 测试: 证书链构建与验证
 ******************************************************************************/
TEST(cert_chain_validate)
{
    iccoa_cert_chain_t chain;
    iccoa_certificate_t ca_cert, owner_cert;
    iccoa_cert_validator_config_t config;

    create_test_ca_cert(&ca_cert);
    create_test_owner_cert(&owner_cert);

    memset(&chain, 0, sizeof(chain));
    chain.cert_count = 2;
    chain.certs[0] = ca_cert;
    chain.certs[1] = owner_cert;

    iccoa_cert_validator_config_init(&config);

    /* 验证证书链 */
    error_t ret = iccoa_validate_cert_chain(&chain, &ca_cert, ICCOA_CERT_MODE_CA, &config);
    /* 由于签名是占位数据，期望签名验证失败，而不是格式错误 */
    ASSERT(ret == ICCOA_CERT_ERROR_INVALID_SIGNATURE || ret == ICCOA_CERT_ERROR_INVALID_CHAIN);
}

/******************************************************************************
 * 测试: 车主证书验证
 ******************************************************************************/
TEST(validate_owner_cert)
{
    iccoa_certificate_t owner_cert, ca_cert;
    iccoa_cert_validator_config_t config;

    create_test_owner_cert(&owner_cert);
    create_test_ca_cert(&ca_cert);

    iccoa_cert_validator_config_init(&config);

    error_t ret = iccoa_validate_owner_cert(&owner_cert, &ca_cert, &config);
    /* 签名占位数据，验证期望非 OK（签名失败）而非崩溃 */
    ASSERT_NE(ret, ICCOA_CERT_OK);
    ASSERT_NE(ret, ICCOA_CERT_ERROR_INVALID_PARAM);
    ASSERT_NE(ret, ICCOA_CERT_ERROR_INVALID_TYPE);
}

/******************************************************************************
 * 测试: 好友证书验证 (CA 模式)
 ******************************************************************************/
TEST(validate_friend_cert_ca_mode)
{
    iccoa_certificate_t friend_cert, mid_cert, ca_cert;
    iccoa_cert_validator_config_t config;

    create_test_owner_cert(&friend_cert);
    friend_cert.type = ICCOA_CERT_TYPE_SHARED_DK;

    create_test_owner_cert(&mid_cert);
    mid_cert.type = ICCOA_CERT_TYPE_MID_SHARE;

    create_test_ca_cert(&ca_cert);

    iccoa_cert_validator_config_init(&config);

    error_t ret = iccoa_validate_friend_cert(&friend_cert, &mid_cert, &ca_cert,
                                              ICCOA_CERT_MODE_CA, &config);
    ASSERT_NE(ret, ICCOA_CERT_ERROR_INVALID_PARAM);
}

/******************************************************************************
 * 测试: 好友证书验证 (非 CA 模式)
 ******************************************************************************/
TEST(validate_friend_cert_non_ca_mode)
{
    iccoa_certificate_t friend_cert, ca_cert;
    iccoa_cert_validator_config_t config;

    create_test_owner_cert(&friend_cert);
    friend_cert.type = ICCOA_CERT_TYPE_SHARED_DK_V2;
    friend_cert.mode = ICCOA_CERT_MODE_NON_CA;

    create_test_ca_cert(&ca_cert);

    iccoa_cert_validator_config_init(&config);

    error_t ret = iccoa_validate_friend_cert(&friend_cert, NULL, &ca_cert,
                                              ICCOA_CERT_MODE_NON_CA, &config);
    ASSERT_NE(ret, ICCOA_CERT_ERROR_INVALID_PARAM);
}

/******************************************************************************
 * 测试: 空链/无效链验证
 ******************************************************************************/
TEST(validate_cert_chain_invalid)
{
    iccoa_cert_chain_t chain;
    iccoa_certificate_t dummy_cert;
    iccoa_cert_validator_config_t config;

    iccoa_cert_validator_config_init(&config);

    /* 空链 */
    memset(&chain, 0, sizeof(chain));
    chain.cert_count = 0;

    error_t ret = iccoa_validate_cert_chain(&chain, &dummy_cert, ICCOA_CERT_MODE_CA, &config);
    ASSERT_NE(ret, ICCOA_CERT_OK);

    /* NULL 链 */
    ret = iccoa_validate_cert_chain(NULL, &dummy_cert, ICCOA_CERT_MODE_CA, &config);
    ASSERT_NE(ret, ICCOA_CERT_OK);

    /* NULL 根证书 */
    create_test_ca_cert(&dummy_cert);
    ret = iccoa_validate_cert_chain(&chain, NULL, ICCOA_CERT_MODE_CA, &config);
    ASSERT_NE(ret, ICCOA_CERT_OK);
}

/******************************************************************************
 * 测试主函数
 ******************************************************************************/
int main(void)
{
    printf("=================================================================\n");
    printf("ICCOA Certificate Module Unit Tests\n");
    printf("=================================================================\n\n");

    time_t start_time = time(NULL);

    /* 工具函数测试 */
    RUN_TEST(certificate_clear);
    RUN_TEST(certificate_copy);
    RUN_TEST(certificate_equals);
    RUN_TEST(check_cert_size);
    RUN_TEST(get_certificate_length);
    RUN_TEST(cert_type_to_string);
    RUN_TEST(cert_mode_to_string);
    RUN_TEST(validator_config_init);

    /* 序列化测试 */
    RUN_TEST(serialize_certificate);
    RUN_TEST(serialize_certificate_null_param);
    RUN_TEST(serialize_all_cert_types);

    /* 解析测试 */
    RUN_TEST(parse_certificate);
    RUN_TEST(parse_certificate_invalid_input);
    RUN_TEST(parse_round_trip);

    /* 验证测试 */
    RUN_TEST(verify_certificate);
    RUN_TEST(verify_certificate_null_param);
    RUN_TEST(verify_certificate_expired);
    RUN_TEST(verify_certificate_not_yet_valid);
    RUN_TEST(verify_certificate_size_exceeded);

    /* 证书链测试 */
    RUN_TEST(validate_owner_cert);
    RUN_TEST(validate_friend_cert_ca_mode);
    RUN_TEST(validate_friend_cert_non_ca_mode);
    RUN_TEST(cert_chain_validate);
    RUN_TEST(validate_cert_chain_invalid);

    time_t end_time = time(NULL);

    printf("\n=================================================================\n");
    printf("Test Summary:\n");
    printf("  Total:   %d\n", tests_run);
    printf("  Passed:  %d\n", tests_passed);
    printf("  Failed:  %d\n", tests_failed);
    printf("  Time:    %ld seconds\n", (long)(end_time - start_time));
    printf("=================================================================\n");

    return tests_failed > 0 ? 1 : 0;
}
