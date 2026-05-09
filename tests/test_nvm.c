/******************************************************************************
 * @file    test_nvm.c
 * @brief   NVM 持久化存储单元测试
 ******************************************************************************/

#include "unity.h"
#include "hal/nvm_hal.h"
#include "nvm_key_storage.h"
#include <string.h>
#include <stdio.h>

static nvm_hal_interface_t g_test_hal;
static uint8_t g_flash_buffer[4096];  /* 模拟 Flash */

/* 模拟 HAL 函数 */
static int mock_init(void) { return NVM_OK; }
static int mock_deinit(void) { return NVM_OK; }
static int mock_read(uint32_t addr, uint8_t *data, size_t len) {
    memcpy(data, &g_flash_buffer[addr], len);
    return NVM_OK;
}
static int mock_write(uint32_t addr, const uint8_t *data, size_t len) {
    memcpy(&g_flash_buffer[addr], data, len);
    return NVM_OK;
}
static int mock_erase(uint32_t addr, size_t len) {
    memset(&g_flash_buffer[addr], 0xFF, len);
    return NVM_OK;
}

void setUp(void) {
    memset(g_flash_buffer, 0xFF, sizeof(g_flash_buffer));
    g_test_hal.init = mock_init;
    g_test_hal.deinit = mock_deinit;
    g_test_hal.read = mock_read;
    g_test_hal.write = mock_write;
    g_test_hal.erase = mock_erase;
    nvm_init(&g_test_hal, (uint8_t*)"test_master_key_32_bytes!");
}

void tearDown(void) {
    nvm_deinit();
}

/* 测试: 钥匙存储和读取 */
void test_nvm_key_store_and_load(void) {
    nvm_key_record_t record;
    memset(&record, 0, sizeof(record));
    
    record.metadata.protocol = NVM_PROTOCOL_CCC;
    record.metadata.key_type = NVM_KEY_TYPE_ECC_P256;
    memcpy(record.metadata.key_id, "test_key_001", 12);
    record.metadata.created_time = 1234567890;
    record.metadata.permissions = NVM_PERM_UNLOCK | NVM_PERM_ENGINE_START;
    
    uint8_t key_data[32] = {1, 2, 3, 4, 5};
    memcpy(record.encrypted_data, key_data, 32);
    record.data_len = 32;
    
    /* 存储 */
    int ret = nvm_key_store(1, &record);
    TEST_ASSERT_EQUAL(NVM_OK, ret);
    
    /* 读取 */
    nvm_key_record_t loaded;
    ret = nvm_key_load(1, &loaded);
    TEST_ASSERT_EQUAL(NVM_OK, ret);
    
    /* 验证 */
    TEST_ASSERT_EQUAL(record.metadata.protocol, loaded.metadata.protocol);
    TEST_ASSERT_EQUAL(record.metadata.key_type, loaded.metadata.key_type);
    TEST_ASSERT_EQUAL_MEMORY(record.metadata.key_id, loaded.metadata.key_id, 12);
}

/* 测试: 钥匙删除 */
void test_nvm_key_delete(void) {
    /* 先存储一个钥匙 */
    nvm_key_record_t record;
    memset(&record, 0, sizeof(record));
    nvm_key_store(2, &record);
    
    /* 删除 */
    int ret = nvm_key_delete(2);
    TEST_ASSERT_EQUAL(NVM_OK, ret);
    
    /* 尝试读取应失败 */
    ret = nvm_key_load(2, &record);
    TEST_ASSERT_EQUAL(NVM_ERR_KEY_NOT_FOUND, ret);
}

/* 测试: 数据完整性 (CRC 验证) */
void test_nvm_data_integrity(void) {
    nvm_key_record_t record;
    memset(&record, 0, sizeof(record));
    record.metadata.protocol = NVM_PROTOCOL_ICCE;
    nvm_key_store(3, &record);
    
    /* 仿真数据损坏 */
    g_flash_buffer[100] ^= 0xFF;
    
    /* 尝试读取应检测到损坏 */
    int ret = nvm_key_load(3, &record);
    TEST_ASSERT_EQUAL(NVM_ERR_CORRUPTED, ret);
}

/* 测试: 枚举所有钥匙 */
static int g_enum_count = 0;
static void test_enum_callback(uint32_t key_id, const nvm_key_metadata_t *meta, void *user) {
    g_enum_count++;
    (void)key_id; (void)meta; (void)user;
}

void test_nvm_key_enumerate(void) {
    /* 存储多个钥匙 */
    nvm_key_record_t record;
    for (int i = 10; i < 15; i++) {
        memset(&record, 0, sizeof(record));
        nvm_key_store(i, &record);
    }
    
    /* 枚举 */
    g_enum_count = 0;
    nvm_key_enumerate(test_enum_callback, NULL);
    
    TEST_ASSERT_EQUAL(5, g_enum_count);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_nvm_key_store_and_load);
    RUN_TEST(test_nvm_key_delete);
    RUN_TEST(test_nvm_data_integrity);
    RUN_TEST(test_nvm_key_enumerate);
    
    return UNITY_END();
}
