/******************************************************************************
 * @file    test_se050_crypto.c
 * @brief   SE050 加密操作单元测试
 ******************************************************************************/

#include "unity.h"
#include "se050_adapter.h"
#include <string.h>

void setUp(void) {
    se050_init(NULL, 0);  /* 使用软件模式 */
}

void tearDown(void) {
    se050_deinit();
}

/* 测试: P-256 密钥对生成 */
void test_se050_generate_keypair_p256(void) {
    se050_key_id_t key_id;
    uint8_t pub_key[65];  /* 0x04 + X(32) + Y(32) */
    size_t pub_key_len = sizeof(pub_key);
    
    int ret = se050_generate_keypair(SE050_KEY_TYPE_ECC_P256, &key_id, 
                                      pub_key, &pub_key_len);
    
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    TEST_ASSERT_EQUAL(65, pub_key_len);
    TEST_ASSERT_EQUAL(0x04, pub_key[0]);  /* 未压缩格式 */
    
    /* 清理 */
    se050_delete_key(key_id);
}

/* 测试: P-384 密钥对生成 */
void test_se050_generate_keypair_p384(void) {
    se050_key_id_t key_id;
    uint8_t pub_key[97];  /* 0x04 + X(48) + Y(48) */
    size_t pub_key_len = sizeof(pub_key);
    
    int ret = se050_generate_keypair(SE050_KEY_TYPE_ECC_P384, &key_id,
                                      pub_key, &pub_key_len);
    
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    TEST_ASSERT_EQUAL(97, pub_key_len);
    
    se050_delete_key(key_id);
}

/* 测试: ECDH 密钥协商 */
void test_se050_ecdh_key_agreement(void) {
    /* Alice 密钥对 */
    se050_key_id_t alice_key;
    uint8_t alice_pub[65];
    size_t alice_pub_len = sizeof(alice_pub);
    se050_generate_keypair(SE050_KEY_TYPE_ECC_P256, &alice_key,
                            alice_pub, &alice_pub_len);
    
    /* Bob 密钥对 */
    se050_key_id_t bob_key;
    uint8_t bob_pub[65];
    size_t bob_pub_len = sizeof(bob_pub);
    se050_generate_keypair(SE050_KEY_TYPE_ECC_P256, &bob_key,
                            bob_pub, &bob_pub_len);
    
    /* Alice 计算共享密钥 */
    uint8_t alice_shared[32];
    int ret = se050_ecdh(alice_key, bob_pub, bob_pub_len, alice_shared, 32);
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    
    /* Bob 计算共享密钥 */
    uint8_t bob_shared[32];
    ret = se050_ecdh(bob_key, alice_pub, alice_pub_len, bob_shared, 32);
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    
    /* 验证两个共享密钥相同 */
    TEST_ASSERT_EQUAL_MEMORY(alice_shared, bob_shared, 32);
    
    /* 清理 */
    se050_delete_key(alice_key);
    se050_delete_key(bob_key);
}

/* 测试: AES-GCM 加解密 */
void test_se050_aes_gcm_encrypt_decrypt(void) {
    se050_key_id_t key_id;
    se050_generate_key(SE050_KEY_TYPE_AES_128, &key_id, NULL, 0);
    
    uint8_t plaintext[] = "Hello, SE050!";
    uint8_t iv[12] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                       0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
    uint8_t aad[] = "Additional data";
    uint8_t ciphertext[32];
    uint8_t tag[16];
    size_t ct_len;
    
    /* 加密 */
    int ret = se050_aes_gcm_encrypt(key_id, plaintext, sizeof(plaintext),
                                     iv, sizeof(iv), aad, sizeof(aad),
                                     ciphertext, &ct_len, tag, 16);
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    
    /* 解密 */
    uint8_t decrypted[32];
    size_t pt_len;
    ret = se050_aes_gcm_decrypt(key_id, ciphertext, ct_len,
                                 iv, sizeof(iv), aad, sizeof(aad),
                                 tag, 16, decrypted, &pt_len);
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    TEST_ASSERT_EQUAL(sizeof(plaintext), pt_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, decrypted, pt_len);
    
    se050_delete_key(key_id);
}

/* 测试: 随机数生成 */
void test_se050_random(void) {
    uint8_t random1[32];
    uint8_t random2[32];
    
    int ret = se050_random(random1, sizeof(random1));
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    
    ret = se050_random(random2, sizeof(random2));
    TEST_ASSERT_EQUAL(SE050_SUCCESS, ret);
    
    /* 验证两次随机数不同 */
    TEST_ASSERT_NOT_EQUAL_MEMORY(random1, random2, 32);
    
    /* 验证不全为零 */
    uint8_t zeros[32] = {0};
    TEST_ASSERT_NOT_EQUAL_MEMORY(random1, zeros, 32);
}

/* 测试: 边界条件 - 无效参数 */
void test_se050_invalid_params(void) {
    se050_key_id_t key_id;
    uint8_t buf[65];
    size_t len = sizeof(buf);
    
    /* 无效密钥类型 */
    int ret = se050_generate_keypair(0xFF, &key_id, buf, &len);
    TEST_ASSERT_EQUAL(SE050_ERROR_INVALID_PARAM, ret);
    
    /* 空指针 */
    ret = se050_generate_keypair(SE050_KEY_TYPE_ECC_P256, NULL, buf, &len);
    TEST_ASSERT_EQUAL(SE050_ERROR_INVALID_PARAM, ret);
}

/* 主函数 */
int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_se050_generate_keypair_p256);
    RUN_TEST(test_se050_generate_keypair_p384);
    RUN_TEST(test_se050_ecdh_key_agreement);
    RUN_TEST(test_se050_aes_gcm_encrypt_decrypt);
    RUN_TEST(test_se050_random);
    RUN_TEST(test_se050_invalid_params);
    
    return UNITY_END();
}
