/** @file test_cdr.c
 * @brief CDR序列化测试
 *
 * @copyright Copyright (c) 2024 YuleTech
 * @license MIT
 */

#include "unity.h"
#include "microdds/cdr.h"

static uint8_t g_buffer[256];
static CDR_Buffer g_cdr_buffer;

void setUp(void) {
    CDR_Buffer_init(&g_cdr_buffer, g_buffer, sizeof(g_buffer), CDR_ENDIAN_LITTLE);
}

void tearDown(void) {
    CDR_Buffer_deinit(&g_cdr_buffer);
}

void test_cdr_init_valid_params(void) {
    CDR_Buffer buffer;
    DDS_ReturnCode_t rc = CDR_Buffer_init(&buffer, g_buffer, sizeof(g_buffer), CDR_ENDIAN_LITTLE);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
}

void test_cdr_init_null_buffer(void) {
    CDR_Buffer buffer;
    DDS_ReturnCode_t rc = CDR_Buffer_init(&buffer, NULL, sizeof(g_buffer), CDR_ENDIAN_LITTLE);
    TEST_ASSERT_EQUAL(DDS_RETCODE_BAD_PARAMETER, rc);
}

void test_cdr_serialize_uint8(void) {
    uint8_t value = 0x42;
    DDS_ReturnCode_t rc = CDR_serialize_uint8(&g_cdr_buffer, value);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    TEST_ASSERT_EQUAL(1, g_cdr_buffer.write_pos);
    TEST_ASSERT_EQUAL(0x42, g_buffer[0]);
}

void test_cdr_serialize_deserialize_uint32(void) {
    uint32_t write_val = 0x12345678;
    DDS_ReturnCode_t rc = CDR_serialize_uint32(&g_cdr_buffer, write_val);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    
    uint32_t read_val;
    rc = CDR_deserialize_uint32(&g_cdr_buffer, &read_val);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    TEST_ASSERT_EQUAL(write_val, read_val);
}

void test_cdr_serialize_deserialize_string(void) {
    const char* write_str = "Hello";
    DDS_ReturnCode_t rc = CDR_serialize_string(&g_cdr_buffer, write_str);
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    
    char read_str[16];
    rc = CDR_deserialize_string(&g_cdr_buffer, read_str, sizeof(read_str));
    TEST_ASSERT_EQUAL(DDS_RETCODE_OK, rc);
    TEST_ASSERT_EQUAL_STRING(write_str, read_str);
}

void test_cdr_get_serialized_data_length(void) {
    CDR_serialize_uint32(&g_cdr_buffer, 0x12345678);
    uint32_t len = CDR_Buffer_get_serialized_data_length(&g_cdr_buffer);
    TEST_ASSERT_EQUAL(4, len);
}

void test_cdr_reset(void) {
    CDR_serialize_uint32(&g_cdr_buffer, 0x12345678);
    CDR_Buffer_reset(&g_cdr_buffer);
    TEST_ASSERT_EQUAL(0, g_cdr_buffer.write_pos);
    TEST_ASSERT_EQUAL(0, g_cdr_buffer.read_pos);
}

void test_cdr_swap16(void) {
    uint16_t result = CDR_swap16(0x1234);
    TEST_ASSERT_EQUAL(0x3412, result);
}

void test_cdr_swap32(void) {
    uint32_t result = CDR_swap32(0x12345678);
    TEST_ASSERT_EQUAL(0x78563412, result);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_cdr_init_valid_params);
    RUN_TEST(test_cdr_init_null_buffer);
    RUN_TEST(test_cdr_serialize_uint8);
    RUN_TEST(test_cdr_serialize_deserialize_uint32);
    RUN_TEST(test_cdr_serialize_deserialize_string);
    RUN_TEST(test_cdr_get_serialized_data_length);
    RUN_TEST(test_cdr_reset);
    RUN_TEST(test_cdr_swap16);
    RUN_TEST(test_cdr_swap32);
    
    return UNITY_END();
}
