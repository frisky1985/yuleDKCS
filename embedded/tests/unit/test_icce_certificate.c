/******************************************************************************
 * @file    test_icce_certificate.c
 * @brief   ICCE 证书模块单元测试
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-15
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "icce.h"
#include "icce_certificate.h"

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
 * 测试用例实现
 ******************************************************************************/

/* 测试用密钥对 (占位数据) */
static uint8_t test_key_pair[96] = {
    /* 私钥 (32字芋) */
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    /* 公钥 (65字芋) */
    0x04, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE,
    0xF0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD,
    0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32,
    0x10, 0x0F, 0xED, 0xCB, 0xA9, 0x87, 0x65, 0x43,
    0x21, 0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA,
    0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22,
    0x11
};

/******************************************************************************
 * 测试: 证书初始化
 ******************************************************************************/
TEST(certificate_init)
{
    icce_certificate_t cert;
    
    icce_certificate_init(&cert);
    
    ASSERT_EQ(cert.version, ICCE_CERT_CURRENT_VERSION);
    ASSERT_EQ(cert.max_path_len, 3);
    ASSERT_EQ(cert.cert_len, 0);
}

/******************************************************************************
 * 测试: 证书清零
 ******************************************************************************/
TEST(certificate_clear)
{
    icce_certificate_t cert;
    
    /* 先创建一个有数据的证书 */
    icce_generate_test_certificate(&cert, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Test Subject", 12, test_key_pair);
    
    ASSERT_NE(cert.cert_len, 0);
    
    icce_certificate_clear(&cert);
    
    ASSERT_EQ(cert.cert_len, 0);
    ASSERT_EQ(cert.version, 0);
}

/******************************************************************************
 * 测试: 证书复制
 ******************************************************************************/
TEST(certificate_copy)
{
    icce_certificate_t cert1, cert2;
    
    icce_generate_test_certificate(&cert1, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Original", 8, test_key_pair);
    
    icce_certificate_init(&cert2);
    icce_certificate_copy(&cert2, &cert1);
    
    ASSERT_EQ(cert2.cert_type, cert1.cert_type);
    ASSERT_EQ(cert2.version, cert1.version);
    ASSERT_EQ(cert2.cert_len, cert1.cert_len);
    ASSERT_EQ(memcmp(cert2.subject, cert1.subject, cert1.subject_len), 0);
}

/******************************************************************************
 * 测试: 证书比较
 ******************************************************************************/
TEST(certificate_equals)
{
    icce_certificate_t cert1, cert2, cert3;
    
    icce_generate_test_certificate(&cert1, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Subject", 7, test_key_pair);
    icce_generate_test_certificate(&cert2, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Subject", 7, test_key_pair);
    icce_generate_test_certificate(&cert3, ICCE_CERT_TYPE_VEHICLE,
                                    (uint8_t*)"Vehicle", 7, test_key_pair);
    
    ASSERT_TRUE(icce_certificate_equals(&cert1, &cert2));
    ASSERT_FALSE(icce_certificate_equals(&cert1, &cert3));
}

/******************************************************************************
 * 测试: 证书序列化
 ******************************************************************************/
TEST(certificate_serialize)
{
    icce_certificate_t cert;
    uint8_t buffer[ICCE_MAX_CERT_SIZE];
    size_t len = sizeof(buffer);
    
    icce_generate_test_certificate(&cert, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Test Owner", 10, test_key_pair);
    
    error_t ret = icce_serialize_certificate(&cert, buffer, &len);
    
    ASSERT_EQ(ret, OK);
    ASSERT_TRUE(len >= ICCE_MIN_CERT_SIZE);
    ASSERT_TRUE(len <= ICCE_MAX_CERT_SIZE);
    
    /* 检查魔法头 */
    uint32_t magic;
    memcpy(&magic, buffer, 4);
    ASSERT_EQ(magic, 0x49434345);  /* "ICCE" */
}

/******************************************************************************
 * 测试: 证书解析
 ******************************************************************************/
TEST(certificate_parse)
{
    icce_certificate_t cert1, cert2;
    uint8_t buffer[ICCE_MAX_CERT_SIZE];
    size_t len = sizeof(buffer);
    
    icce_generate_test_certificate(&cert1, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Test Owner", 10, test_key_pair);
    memcpy(cert1.device_id, "DEVICE_ID_123456", ICCE_DEVICE_ID_LEN);
    memcpy(cert1.vehicle_id, "VEHICLE_ID_123456", ICCE_VEHICLE_ID_LEN);
    
    error_t ret = icce_serialize_certificate(&cert1, buffer, &len);
    ASSERT_EQ(ret, OK);
    
    ret = icce_parse_certificate(buffer, len, &cert2);
    ASSERT_EQ(ret, OK);
    
    ASSERT_EQ(cert2.cert_type, cert1.cert_type);
    ASSERT_EQ(cert2.version, cert1.version);
    ASSERT_EQ(cert2.subject_len, cert1.subject_len);
    ASSERT_EQ(memcmp(cert2.subject, cert1.subject, cert1.subject_len), 0);
    ASSERT_EQ(memcmp(cert2.device_id, cert1.device_id, ICCE_DEVICE_ID_LEN), 0);
    ASSERT_EQ(memcmp(cert2.vehicle_id, cert1.vehicle_id, ICCE_VEHICLE_ID_LEN), 0);
}

/******************************************************************************
 * 测试: 证书大小检查
 ******************************************************************************/
TEST(check_cert_size)
{
    icce_certificate_t cert;
    
    icce_generate_test_certificate(&cert, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Test", 4, test_key_pair);
    
    ASSERT_TRUE(icce_check_cert_size(&cert));
    
    /* 测试超大证书 */
    cert.cert_len = ICCE_MAX_CERT_SIZE + 1;
    ASSERT_FALSE(icce_check_cert_size(&cert));
    
    /* 测试过小证书 */
    cert.cert_len = ICCE_MIN_CERT_SIZE - 1;
    ASSERT_FALSE(icce_check_cert_size(&cert));
}

/******************************************************************************
 * 测试: 证书过期检查
 ******************************************************************************/
TEST(cert_expiry)
{
    icce_certificate_t cert;
    
    icce_generate_test_certificate(&cert, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Test", 4, test_key_pair);
    
    uint32_t now = (uint32_t)time(NULL);
    
    /* 测试未过期证书 */
    cert.valid_from = now - 100;
    cert.valid_until = now + 3600;
    ASSERT_FALSE(icce_cert_is_expired(&cert, now));
    ASSERT_TRUE(icce_cert_is_valid_now(&cert, now));
    
    /* 测试已过期证书 */
    cert.valid_until = now - 100;
    ASSERT_TRUE(icce_cert_is_expired(&cert, now));
    ASSERT_FALSE(icce_cert_is_valid_now(&cert, now));
    
    /* 测试尚未生效证书 */
    cert.valid_from = now + 100;
    cert.valid_until = now + 3600;
    ASSERT_FALSE(icce_cert_is_expired(&cert, now));
    ASSERT_FALSE(icce_cert_is_valid_now(&cert, now));
}

/******************************************************************************
 * 测试: 证书类型转字符串
 ******************************************************************************/
TEST(cert_type_to_string)
{
    ASSERT_TRUE(strcmp(icce_cert_type_to_string(ICCE_CERT_TYPE_VEHICLE_CA), "Vehicle CA") == 0);
    ASSERT_TRUE(strcmp(icce_cert_type_to_string(ICCE_CERT_TYPE_VEHICLE), "Vehicle") == 0);
    ASSERT_TRUE(strcmp(icce_cert_type_to_string(ICCE_CERT_TYPE_OWNER_DK), "Owner Digital Key") == 0);
    ASSERT_TRUE(strcmp(icce_cert_type_to_string(ICCE_CERT_TYPE_SHARED_DK), "Shared Digital Key") == 0);
    ASSERT_TRUE(strcmp(icce_cert_type_to_string(ICCE_CERT_TYPE_TEMP_ACCESS), "Temporary Access") == 0);
    ASSERT_TRUE(strcmp(icce_cert_type_to_string(0xFF), "Unknown") == 0);
}

/******************************************************************************
 * 测试: 错误码转字符串
 ******************************************************************************/
TEST(cert_error_to_string)
{
    ASSERT_TRUE(strcmp(icce_cert_error_to_string(ICCE_CERT_OK), "Success") == 0);
    ASSERT_TRUE(strcmp(icce_cert_error_to_string(ICCE_CERT_ERROR_INVALID_PARAM), "Invalid parameter") == 0);
    ASSERT_TRUE(strcmp(icce_cert_error_to_string(ICCE_CERT_ERROR_EXPIRED), "Certificate expired") == 0);
    ASSERT_TRUE(strcmp(icce_cert_error_to_string(ICCE_CERT_ERROR_INVALID_SIGNATURE), "Invalid signature") == 0);
    ASSERT_TRUE(strcmp(icce_cert_error_to_string(-999), "Unknown error") == 0);
}

/******************************************************************************
 * 测试: 验证器配置初始化
 ******************************************************************************/
TEST(validator_config_init)
{
    icce_cert_validator_config_t config;
    
    icce_cert_validator_config_init(&config);
    
    ASSERT_EQ(config.max_clock_skew_seconds, 300);
    ASSERT_EQ(config.allow_self_signed, false);
    ASSERT_EQ(config.strict_size_check, true);
    ASSERT_EQ(config.verify_time, true);
    ASSERT_EQ(config.verify_permissions, true);
}

/******************************************************************************
 * 测试: APDU 封装/解析
 ******************************************************************************/
TEST(cert_apdu_conversion)
{
    icce_certificate_t cert1, cert2;
    uint8_t apdu_data[ICCE_MAX_CERT_SIZE];
    size_t apdu_len = sizeof(apdu_data);
    
    icce_generate_test_certificate(&cert1, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"APDU Test", 9, test_key_pair);
    
    error_t ret = icce_cert_to_apdu(&cert1, apdu_data, &apdu_len, true);
    ASSERT_EQ(ret, OK);
    ASSERT_TRUE(apdu_len > 0);
    
    ret = icce_cert_from_apdu(apdu_data, apdu_len, &cert2);
    ASSERT_EQ(ret, OK);
    ASSERT_EQ(cert2.cert_type, cert1.cert_type);
    ASSERT_TRUE(icce_certificate_equals(&cert1, &cert2));
}

/******************************************************************************
 * 测试: 证书长度获取
 ******************************************************************************/
TEST(get_certificate_length)
{
    icce_certificate_t cert;
    
    icce_generate_test_certificate(&cert, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Length Test", 11, test_key_pair);
    
    size_t len = icce_get_certificate_length(&cert);
    ASSERT_TRUE(len >= ICCE_MIN_CERT_SIZE);
    ASSERT_TRUE(len <= ICCE_MAX_CERT_SIZE);
}

/******************************************************************************
 * 测试: 非法格式证书解析
 ******************************************************************************/
TEST(invalid_certificate_parse)
{
    icce_certificate_t cert;
    uint8_t invalid_data[100];
    
    /* 错误的魔法头 */
    memset(invalid_data, 0xFF, sizeof(invalid_data));
    error_t ret = icce_parse_certificate(invalid_data, sizeof(invalid_data), &cert);
    ASSERT_EQ(ret, ICCE_CERT_ERROR_INVALID_FORMAT);
    
    /* 过短的数据 */
    ret = icce_parse_certificate(invalid_data, 10, &cert);
    ASSERT_EQ(ret, ERROR_INVALID_PARAM);
    
    /* 过长的数据 */
    uint8_t too_long[ICCE_MAX_CERT_SIZE + 100];
    ret = icce_parse_certificate(too_long, sizeof(too_long), &cert);
    ASSERT_EQ(ret, ICCE_CERT_ERROR_SIZE_EXCEEDED);
}

/******************************************************************************
 * 测试: 不同证书类型生成
 ******************************************************************************/
TEST(different_cert_types)
{
    icce_certificate_t ca_cert, vehicle_cert, owner_cert, shared_cert, temp_cert;
    
    /* 车CA证书 */
    icce_generate_test_certificate(&ca_cert, ICCE_CERT_TYPE_VEHICLE_CA,
                                    (uint8_t*)"Vehicle CA", 10, test_key_pair);
    ASSERT_EQ(ca_cert.cert_type, ICCE_CERT_TYPE_VEHICLE_CA);
    ASSERT_EQ(ca_cert.is_ca, 1);
    
    /* 车证书 */
    icce_generate_test_certificate(&vehicle_cert, ICCE_CERT_TYPE_VEHICLE,
                                    (uint8_t*)"Vehicle", 7, test_key_pair);
    ASSERT_EQ(vehicle_cert.cert_type, ICCE_CERT_TYPE_VEHICLE);
    ASSERT_EQ(vehicle_cert.is_ca, 0);
    
    /* 车主证书 */
    icce_generate_test_certificate(&owner_cert, ICCE_CERT_TYPE_OWNER_DK,
                                    (uint8_t*)"Owner", 5, test_key_pair);
    ASSERT_EQ(owner_cert.cert_type, ICCE_CERT_TYPE_OWNER_DK);
    
    /* 分享证书 */
    icce_generate_test_certificate(&shared_cert, ICCE_CERT_TYPE_SHARED_DK,
                                    (uint8_t*)"Shared", 6, test_key_pair);
    ASSERT_EQ(shared_cert.cert_type, ICCE_CERT_TYPE_SHARED_DK);
    
    /* 临时证书 */
    icce_generate_test_certificate(&temp_cert, ICCE_CERT_TYPE_TEMP_ACCESS,
                                    (uint8_t*)"Temp", 4, test_key_pair);
    ASSERT_EQ(temp_cert.cert_type, ICCE_CERT_TYPE_TEMP_ACCESS);
}

/******************************************************************************
 * 测试主函数
 ******************************************************************************/
int main(void)
{
    printf("=================================================================\n");
    printf("ICCE Certificate Module Unit Tests\n");
    printf("=================================================================\n\n");
    
    time_t start_time = time(NULL);
    
    /* 运行所有测试 */
    RUN_TEST(certificate_init);
    RUN_TEST(certificate_clear);
    RUN_TEST(certificate_copy);
    RUN_TEST(certificate_equals);
    RUN_TEST(certificate_serialize);
    RUN_TEST(certificate_parse);
    RUN_TEST(check_cert_size);
    RUN_TEST(cert_expiry);
    RUN_TEST(cert_type_to_string);
    RUN_TEST(cert_error_to_string);
    RUN_TEST(validator_config_init);
    RUN_TEST(cert_apdu_conversion);
    RUN_TEST(get_certificate_length);
    RUN_TEST(invalid_certificate_parse);
    RUN_TEST(different_cert_types);
    
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
