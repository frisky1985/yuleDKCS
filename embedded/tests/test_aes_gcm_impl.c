/******************************************************************************
 * @file    test_aes_gcm_impl.c
 * @brief   AES-GCM 加密实现测试
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-09
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "se050_crypto.h"
#include "dkcs.h"

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: Expected %d, got %d at line %d\n", (int)(expected), (int)(actual), __LINE__); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("FAIL: Pointer is NULL at line %d\n", __LINE__); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) \
    do { \
        if (memcmp((expected), (actual), (len)) != 0) { \
            printf("FAIL: Memory not equal at line %d\n", __LINE__); \
            return -1; \
        } \
    } while(0)

/******************************************************************************
 * 测试用例
 ******************************************************************************/

/* 测试: ccc_encrypt_aes_gcm 和 ccc_decrypt_aes_gcm */
static int test_ccc_aes_gcm_roundtrip(void)
{
    uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    uint8_t iv[12] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
    };
    uint8_t aad[] = "Additional authenticated data";
    uint8_t plaintext[] = "Hello, AES-GCM!";
    uint8_t ciphertext[32];
    uint8_t tag[16];
    uint8_t decrypted[32];
    error_t err;
    
    printf("Testing AES-GCM roundtrip...\n");
    
    /* 加密 */
    err = ccc_encrypt_aes_gcm(
        key, iv,
        aad, sizeof(aad) - 1,
        plaintext, sizeof(plaintext) - 1,
        ciphertext, tag
    );
    TEST_ASSERT_EQUAL(OK, err);
    
    /* 验证密文不等于明文 */
    if (memcmp(ciphertext, plaintext, sizeof(plaintext) - 1) == 0) {
        printf("FAIL: Ciphertext equals plaintext (not encrypted)\n");
        return -1;
    }
    
    /* 解密 */
    err = ccc_decrypt_aes_gcm(
        key, iv,
        aad, sizeof(aad) - 1,
        ciphertext, sizeof(plaintext) - 1,
        tag, decrypted
    );
    TEST_ASSERT_EQUAL(OK, err);
    
    /* 验证解密结果 */
    TEST_ASSERT_EQUAL_MEMORY(plaintext, decrypted, sizeof(plaintext) - 1);
    
    printf("  PASS: AES-GCM roundtrip\n");
    return 0;
}

/* 测试: 无 AAD 的加密 */
static int test_ccc_aes_gcm_no_aad(void)
{
    uint8_t key[16] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    };
    uint8_t iv[12] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B
    };
    uint8_t plaintext[] = "Test message without AAD";
    uint8_t ciphertext[32];
    uint8_t tag[16];
    uint8_t decrypted[32];
    error_t err;
    
    printf("Testing AES-GCM without AAD...\n");
    
    /* 加密 - 不使用 AAD */
    err = ccc_encrypt_aes_gcm(
        key, iv,
        NULL, 0,  /* 无 AAD */
        plaintext, sizeof(plaintext) - 1,
        ciphertext, tag
    );
    TEST_ASSERT_EQUAL(OK, err);
    
    /* 解密 - 不使用 AAD */
    err = ccc_decrypt_aes_gcm(
        key, iv,
        NULL, 0,  /* 无 AAD */
        ciphertext, sizeof(plaintext) - 1,
        tag, decrypted
    );
    TEST_ASSERT_EQUAL(OK, err);
    
    TEST_ASSERT_EQUAL_MEMORY(plaintext, decrypted, sizeof(plaintext) - 1);
    
    printf("  PASS: AES-GCM without AAD\n");
    return 0;
}

/* 测试: 错误的 Tag 导致解密失败 */
static int test_ccc_aes_gcm_wrong_tag(void)
{
    uint8_t key[16] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F
    };
    uint8_t iv[12] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B
    };
    uint8_t plaintext[] = "Tamper test";
    uint8_t ciphertext[32];
    uint8_t tag[16];
    uint8_t decrypted[32];
    error_t err;
    
    printf("Testing AES-GCM tag verification...\n");
    
    /* 加密 */
    err = ccc_encrypt_aes_gcm(
        key, iv,
        NULL, 0,
        plaintext, sizeof(plaintext) - 1,
        ciphertext, tag
    );
    TEST_ASSERT_EQUAL(OK, err);
    
    /* 篡改 Tag */
    tag[0] ^= 0xFF;
    
    /* 解密应该失败 */
    err = ccc_decrypt_aes_gcm(
        key, iv,
        NULL, 0,
        ciphertext, sizeof(plaintext) - 1,
        tag, decrypted
    );
    
    /* 期望解密失败 */
    if (err == OK) {
        printf("FAIL: Decryption should fail with wrong tag\n");
        return -1;
    }
    
    printf("  PASS: Tag verification works\n");
    return 0;
}

/* 测试: 密钥生成和管理 */
static int test_key_generation(void)
{
    error_t err;
    uint32_t key_id = 0x1234;
    bool exists;
    
    printf("Testing key generation...\n");
    
    /* 生成 AES-128 密钥 */
    err = ccc_generate_key(key_id, SE050_KEY_TYPE_AES_128, 0, false);
    TEST_ASSERT_EQUAL(OK, err);
    
    /* 检查密钥是否存在 */
    err = se050_key_exists(key_id, &exists, NULL);
    TEST_ASSERT_EQUAL(OK, err);
    TEST_ASSERT_EQUAL(true, exists);
    
    /* 重复生成应该失败 */
    err = ccc_generate_key(key_id, SE050_KEY_TYPE_AES_128, 0, false);
    TEST_ASSERT_EQUAL(ERROR_ALREADY_INITIALIZED, err);
    
    /* 删除密钥 */
    err = se050_delete_key(key_id);
    TEST_ASSERT_EQUAL(OK, err);
    
    /* 再次检查应该不存在 */
    err = se050_key_exists(key_id, &exists, NULL);
    TEST_ASSERT_EQUAL(OK, err);
    TEST_ASSERT_EQUAL(false, exists);
    
    printf("  PASS: Key generation and management\n");
    return 0;
}

/* 测试: 随机数生成 */
static int test_random_generation(void)
{
    uint8_t random1[32];
    uint8_t random2[32];
    error_t err;
    int i;
    bool all_zero;
    
    printf("Testing random number generation...\n");
    
    err = se050_generate_random(random1, sizeof(random1));
    TEST_ASSERT_EQUAL(OK, err);
    
    err = se050_generate_random(random2, sizeof(random2));
    TEST_ASSERT_EQUAL(OK, err);
    
    /* 验证两次随机数不完全相同 */
    if (memcmp(random1, random2, sizeof(random1)) == 0) {
        printf("FAIL: Random values are identical\n");
        return -1;
    }
    
    /* 验证不全为零 */
    all_zero = true;
    for (i = 0; i < sizeof(random1); i++) {
        if (random1[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        printf("FAIL: Random values are all zeros\n");
        return -1;
    }
    
    printf("  PASS: Random number generation\n");
    return 0;
}

/* 测试: 无效参数 */
static int test_invalid_params(void)
{
    uint8_t key[16] = {0};
    uint8_t iv[12] = {0};
    uint8_t plaintext[] = "test";
    uint8_t ciphertext[32];
    uint8_t tag[16];
    error_t err;
    
    printf("Testing invalid parameters...\n");
    
    /* NULL 密钥 */
    err = ccc_encrypt_aes_gcm(NULL, iv, NULL, 0, plaintext, 4, ciphertext, tag);
    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAM, err);
    
    /* NULL IV */
    err = ccc_encrypt_aes_gcm(key, NULL, NULL, 0, plaintext, 4, ciphertext, tag);
    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAM, err);
    
    /* NULL 明文 */
    err = ccc_encrypt_aes_gcm(key, iv, NULL, 0, NULL, 4, ciphertext, tag);
    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAM, err);
    
    /* NULL 密文 */
    err = ccc_encrypt_aes_gcm(key, iv, NULL, 0, plaintext, 4, NULL, tag);
    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAM, err);
    
    /* NULL Tag */
    err = ccc_encrypt_aes_gcm(key, iv, NULL, 0, plaintext, 4, ciphertext, NULL);
    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAM, err);
    
    printf("  PASS: Invalid parameter handling\n");
    return 0;
}

/******************************************************************************
 * 主函数
 ******************************************************************************/
int main(void)
{
    int failures = 0;
    error_t err;
    
    printf("========================================\n");
    printf("AES-GCM Implementation Test Suite\n");
    printf("========================================\n\n");
    
    /* 初始化 */
    err = se050_crypto_init();
    if (err != OK) {
        printf("FAIL: Crypto initialization failed\n");
        return 1;
    }
    
    /* 运行测试 */
    if (test_ccc_aes_gcm_roundtrip() != 0) failures++;
    if (test_ccc_aes_gcm_no_aad() != 0) failures++;
    if (test_ccc_aes_gcm_wrong_tag() != 0) failures++;
    if (test_key_generation() != 0) failures++;
    if (test_random_generation() != 0) failures++;
    if (test_invalid_params() != 0) failures++;
    
    /* 清理 */
    se050_crypto_deinit();
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }
    printf("========================================\n");
    
    return failures;
}
