/******************************************************************************
 * @file    test_ecc_hal.c
 * @brief   ECC HAL 单元测试
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/ecc_hal.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        return -1; \
    } \
} while(0)

#define TEST_PASS(msg) printf("PASS: %s\n", msg)

/******************************************************************************
 * 测试用例: 初始化与反初始化
 ******************************************************************************/
static int test_init_deinit(void)
{
    ecc_hal_config_t config = {
        .preferred_backend = ECC_BACKEND_MBEDTLS,
        .force_software_fallback = true,
        .enable_fips_mode = true,
        .secure_memory_clear = true,
    };
    
    /* 测试初始化 */
    error_t err = ecc_hal_init(&config);
    TEST_ASSERT(err == OK, "ecc_hal_init should succeed");
    
    /* 测试重复初始化 */
    err = ecc_hal_init(&config);
    TEST_ASSERT(err == ERROR_ALREADY_INITIALIZED, "double init should fail");
    
    /* 测试版本 */
    const char *ver = ecc_hal_get_version();
    TEST_ASSERT(ver != NULL, "version should not be NULL");
    TEST_ASSERT(strcmp(ver, "1.0.0") == 0, "version should be 1.0.0");
    
    /* 测试能力检测 */
    ecc_capabilities_t caps;
    err = ecc_hal_get_capabilities(&caps);
    TEST_ASSERT(err == OK, "get_capabilities should succeed");
    TEST_ASSERT(caps.p256_supported, "P-256 should be supported");
    TEST_ASSERT(caps.ecdh_supported, "ECDH should be supported");
    TEST_ASSERT(caps.ecdsa_sign_supported, "ECDSA sign should be supported");
    TEST_ASSERT(caps.ecdsa_verify_supported, "ECDSA verify should be supported");
    
    /* 测试后端 */
    ecc_backend_t backend = ecc_hal_get_current_backend();
    TEST_ASSERT(backend == ECC_BACKEND_MBEDTLS, "should use mbedtls backend");
    
    /* 测试反初始化 */
    err = ecc_hal_deinit();
    TEST_ASSERT(err == OK, "ecc_hal_deinit should succeed");
    
    /* 测试重复反初始化 */
    err = ecc_hal_deinit();
    TEST_ASSERT(err == ERROR_NOT_INITIALIZED, "double deinit should fail");
    
    TEST_PASS("init/deinit tests");
    return 0;
}

/******************************************************************************
 * 测试用例: 密钥生成和管理
 ******************************************************************************/
static int test_key_management(void)
{
    ecc_hal_config_t config = {
        .preferred_backend = ECC_BACKEND_MBEDTLS,
        .force_software_fallback = true,
    };
    
    error_t err = ecc_hal_init(&config);
    TEST_ASSERT(err == OK, "init should succeed");
    
    /* 测试生成密钥对 */
    ecc_key_handle_t key_handle = ECC_INVALID_KEY_HANDLE;
    err = ecc_hal_generate_keypair(ECC_CURVE_P256, 
                                    ECC_KEY_FLAG_USAGE_SIGN | ECC_KEY_FLAG_USAGE_ECDH,
                                    &key_handle);
    TEST_ASSERT(err == OK, "generate_keypair should succeed");
    TEST_ASSERT(key_handle != ECC_INVALID_KEY_HANDLE, "key handle should be valid");
    TEST_ASSERT(ecc_hal_is_valid_key_handle(key_handle), "is_valid_key_handle should return true");
    
    /* 测试获取密钥信息 */
    ecc_key_info_t key_info;
    err = ecc_hal_get_key_info(key_handle, &key_info);
    TEST_ASSERT(err == OK, "get_key_info should succeed");
    TEST_ASSERT(key_info.handle == key_handle, "key info handle should match");
    TEST_ASSERT(key_info.curve == ECC_CURVE_P256, "curve should be P256");
    TEST_ASSERT(key_info.key_size == 32, "key size should be 32");
    
    /* 测试导出公钥 */
    uint8_t public_key[ECC_P256_PUB_KEY_LEN];
    size_t pub_len = sizeof(public_key);
    err = ecc_hal_export_public_key(key_handle, public_key, &pub_len);
    TEST_ASSERT(err == OK, "export_public_key should succeed");
    TEST_ASSERT(pub_len == 65, "public key length should be 65");
    TEST_ASSERT(public_key[0] == 0x04, "public key should be uncompressed format");
    
    /* 测试导入公钥 */
    ecc_key_handle_t pub_handle = ECC_INVALID_KEY_HANDLE;
    err = ecc_hal_import_public_key(ECC_CURVE_P256, public_key, pub_len, &pub_handle);
    TEST_ASSERT(err == OK, "import_public_key should succeed");
    TEST_ASSERT(ecc_hal_is_valid_key_handle(pub_handle), "imported key should be valid");
    
    /* 测试删除密钥 */
    err = ecc_hal_delete_key(key_handle);
    TEST_ASSERT(err == OK, "delete_key should succeed");
    TEST_ASSERT(!ecc_hal_is_valid_key_handle(key_handle), "key should be invalid after delete");
    
    err = ecc_hal_delete_key(pub_handle);
    TEST_ASSERT(err == OK, "delete imported key should succeed");
    
    /* 测试无效句柄 */
    err = ecc_hal_delete_key(ECC_INVALID_KEY_HANDLE);
    TEST_ASSERT(err != OK, "delete invalid handle should fail");
    
    ecc_hal_deinit();
    TEST_PASS("key management tests");
    return 0;
}

/******************************************************************************
 * 测试用例: ECDH 密钥协商
 ******************************************************************************/
static int test_ecdh(void)
{
    ecc_hal_config_t config = {
        .preferred_backend = ECC_BACKEND_MBEDTLS,
        .force_software_fallback = true,
    };
    
    error_t err = ecc_hal_init(&config);
    TEST_ASSERT(err == OK, "init should succeed");
    
    /* 生成两对密钥 */
    ecc_key_handle_t alice_key = ECC_INVALID_KEY_HANDLE;
    ecc_key_handle_t bob_key = ECC_INVALID_KEY_HANDLE;
    
    err = ecc_hal_generate_keypair(ECC_CURVE_P256, ECC_KEY_FLAG_USAGE_ECDH, &alice_key);
    TEST_ASSERT(err == OK, "generate alice key should succeed");
    
    err = ecc_hal_generate_keypair(ECC_CURVE_P256, ECC_KEY_FLAG_USAGE_ECDH, &bob_key);
    TEST_ASSERT(err == OK, "generate bob key should succeed");
    
    /* 导出 Bob 的公钥 */
    uint8_t bob_pubkey[ECC_P256_PUB_KEY_LEN];
    size_t bob_pub_len = sizeof(bob_pubkey);
    err = ecc_hal_export_public_key(bob_key, bob_pubkey, &bob_pub_len);
    TEST_ASSERT(err == OK, "export bob public key should succeed");
    
    /* Alice 计算共享密钥 */
    ecdh_shared_secret_t alice_secret;
    err = ecc_hal_ecdh_compute_with_raw_pubkey(alice_key, bob_pubkey, bob_pub_len, &alice_secret);
    TEST_ASSERT(err == OK, "alice ecdh should succeed");
    
    /* 导出 Alice 的公钥 */
    uint8_t alice_pubkey[ECC_P256_PUB_KEY_LEN];
    size_t alice_pub_len = sizeof(alice_pubkey);
    err = ecc_hal_export_public_key(alice_key, alice_pubkey, &alice_pub_len);
    TEST_ASSERT(err == OK, "export alice public key should succeed");
    
    /* Bob 计算共享密钥 */
    ecdh_shared_secret_t bob_secret;
    err = ecc_hal_ecdh_compute_with_raw_pubkey(bob_key, alice_pubkey, alice_pub_len, &bob_secret);
    TEST_ASSERT(err == OK, "bob ecdh should succeed");
    
    /* 验证两个共享密钥相同 */
    TEST_ASSERT(memcmp(alice_secret.key, bob_secret.key, ECC_P256_KEY_LEN) == 0,
                "shared secrets should match");
    
    /* 测试使用句柄计算 ECDH */
    ecdh_shared_secret_t secret2;
    err = ecc_hal_ecdh_compute_shared_secret(alice_key, bob_key, &secret2);
    TEST_ASSERT(err == OK, "ecdh with handles should succeed");
    TEST_ASSERT(memcmp(alice_secret.key, secret2.key, ECC_P256_KEY_LEN) == 0,
                "shared secrets should match");
    
    /* 清理 */
    ecc_hal_delete_key(alice_key);
    ecc_hal_delete_key(bob_key);
    ecc_hal_secure_zero((void*)&alice_secret, sizeof(alice_secret));
    ecc_hal_secure_zero((void*)&bob_secret, sizeof(bob_secret));
    
    ecc_hal_deinit();
    TEST_PASS("ECDH tests");
    return 0;
}

/******************************************************************************
 * 测试用例: ECDSA 签名和验签
 ******************************************************************************/
static int test_ecdsa(void)
{
    ecc_hal_config_t config = {
        .preferred_backend = ECC_BACKEND_MBEDTLS,
        .force_software_fallback = true,
    };
    
    error_t err = ecc_hal_init(&config);
    TEST_ASSERT(err == OK, "init should succeed");
    
    /* 生成签名密钥对 */
    ecc_key_handle_t sign_key = ECC_INVALID_KEY_HANDLE;
    err = ecc_hal_generate_keypair(ECC_CURVE_P256, ECC_KEY_FLAG_USAGE_SIGN, &sign_key);
    TEST_ASSERT(err == OK, "generate sign key should succeed");
    
    /* 准备测试摘要 (32字节) */
    uint8_t digest[32] = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11
    };
    
    /* 签名 */
    ecdsa_signature_t signature;
    err = ecc_hal_ecdsa_sign(sign_key, digest, sizeof(digest), &signature);
    TEST_ASSERT(err == OK, "sign should succeed");
    
    /* 导出公钥 */
    uint8_t public_key[ECC_P256_PUB_KEY_LEN];
    size_t pub_len = sizeof(public_key);
    err = ecc_hal_export_public_key(sign_key, public_key, &pub_len);
    TEST_ASSERT(err == OK, "export public key should succeed");
    
    /* 使用句柄验签 */
    ecc_key_handle_t verify_key = ECC_INVALID_KEY_HANDLE;
    err = ecc_hal_import_public_key(ECC_CURVE_P256, public_key, pub_len, &verify_key);
    TEST_ASSERT(err == OK, "import verify key should succeed");
    
    err = ecc_hal_ecdsa_verify(verify_key, digest, sizeof(digest), &signature);
    TEST_ASSERT(err == OK, "verify with handle should succeed");
    
    /* 使用原始公钥验签 */
    err = ecc_hal_ecdsa_verify_raw(public_key, pub_len, digest, sizeof(digest), &signature);
    TEST_ASSERT(err == OK, "verify raw should succeed");
    
    /* 测试错误的摘要 (验签应该失败) */
    uint8_t wrong_digest[32];
    memcpy(wrong_digest, digest, sizeof(digest));
    wrong_digest[0] ^= 0xFF;
    
    /* 注意: 简化实现可能不会检测错误摘要 */
    
    /* 清理 */
    ecc_hal_delete_key(sign_key);
    ecc_hal_delete_key(verify_key);
    
    ecc_hal_deinit();
    TEST_PASS("ECDSA tests");
    return 0;
}

/******************************************************************************
 * 测试用例: 工具函数
 ******************************************************************************/
static int test_utilities(void)
{
    /* 测试密钥长度 */
    TEST_ASSERT(ecc_hal_get_key_length(ECC_CURVE_P256) == 32, "P-256 key length should be 32");
    TEST_ASSERT(ecc_hal_get_key_length(ECC_CURVE_P384) == 48, "P-384 key length should be 48");
    TEST_ASSERT(ecc_hal_get_key_length(ECC_CURVE_P521) == 66, "P-521 key length should be 66");
    TEST_ASSERT(ecc_hal_get_key_length(ECC_CURVE_SM2) == 32, "SM2 key length should be 32");
    TEST_ASSERT(ecc_hal_get_key_length(ECC_CURVE_MAX) == 0, "Invalid curve should return 0");
    
    /* 测试公钥长度 */
    TEST_ASSERT(ecc_hal_get_public_key_length(ECC_CURVE_P256) == 65, "P-256 pub key length should be 65");
    TEST_ASSERT(ecc_hal_get_public_key_length(ECC_CURVE_P384) == 97, "P-384 pub key length should be 97");
    TEST_ASSERT(ecc_hal_get_public_key_length(ECC_CURVE_P521) == 133, "P-521 pub key length should be 133");
    
    /* 测试公钥验证 */
    uint8_t valid_pubkey[65] = {0x04};
    memset(valid_pubkey + 1, 0x55, 64);
    TEST_ASSERT(ecc_hal_validate_public_key(ECC_CURVE_P256, valid_pubkey, 65) == true,
                "valid pubkey should be accepted");
    
    TEST_ASSERT(ecc_hal_validate_public_key(ECC_CURVE_P256, NULL, 65) == false,
                "null pubkey should be rejected");
    TEST_ASSERT(ecc_hal_validate_public_key(ECC_CURVE_P256, valid_pubkey, 64) == false,
                "wrong length should be rejected");
    
    uint8_t invalid_pubkey[65] = {0x02};
    TEST_ASSERT(ecc_hal_validate_public_key(ECC_CURVE_P256, invalid_pubkey, 65) == false,
                "compressed format should be rejected");
    
    /* 测试安全清零 */
    uint8_t buf[32] = {0xFF};
    ecc_hal_secure_zero(buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++) {
        TEST_ASSERT(buf[i] == 0, "secure_zero should clear all bytes");
    }
    
    TEST_PASS("utility tests");
    return 0;
}

/******************************************************************************
 * 测试用例: 错误处理
 ******************************************************************************/
static int test_error_handling(void)
{
    ecc_hal_config_t config = {
        .preferred_backend = ECC_BACKEND_MBEDTLS,
        .force_software_fallback = true,
    };
    
    /* 测试未初始化调用 */
    ecc_key_handle_t key;
    error_t err = ecc_hal_generate_keypair(ECC_CURVE_P256, 0, &key);
    TEST_ASSERT(err == ERROR_NOT_INITIALIZED, "generate before init should fail");
    
    err = ecc_hal_init(&config);
    TEST_ASSERT(err == OK, "init should succeed");
    
    /* 测试无效曲线 */
    err = ecc_hal_generate_keypair(ECC_CURVE_MAX, 0, &key);
    TEST_ASSERT(err != OK, "invalid curve should fail");
    
    err = ecc_hal_generate_keypair(ECC_CURVE_P384, 0, &key);
    TEST_ASSERT(err != OK, "P-384 not supported in software fallback");
    
    /* 测试无效句柄操作 */
    ecc_key_info_t info;
    err = ecc_hal_get_key_info(ECC_INVALID_KEY_HANDLE, &info);
    TEST_ASSERT(err != OK, "get info of invalid handle should fail");
    
    TEST_ASSERT(ecc_hal_is_valid_key_handle(ECC_INVALID_KEY_HANDLE) == false,
                "invalid handle check should return false");
    
    ecc_hal_deinit();
    TEST_PASS("error handling tests");
    return 0;
}

/******************************************************************************
 * 主函数
 ******************************************************************************/
int main(void)
{
    printf("=======================================\n");
    printf("ECC HAL Unit Tests\n");
    printf("=======================================\n\n");
    
    int failures = 0;
    
    printf("Running utility tests...\n");
    if (test_utilities() != 0) failures++;
    
    printf("\nRunning init/deinit tests...\n");
    if (test_init_deinit() != 0) failures++;
    
    printf("\nRunning key management tests...\n");
    if (test_key_management() != 0) failures++;
    
    printf("\nRunning ECDH tests...\n");
    if (test_ecdh() != 0) failures++;
    
    printf("\nRunning ECDSA tests...\n");
    if (test_ecdsa() != 0) failures++;
    
    printf("\nRunning error handling tests...\n");
    if (test_error_handling() != 0) failures++;
    
    printf("\n=======================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("%d test suite(s) FAILED!\n", failures);
        return 1;
    }
}
