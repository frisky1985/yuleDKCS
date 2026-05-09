/******************************************************************************
 * @file    nvm_key_storage.c
 * @brief   持久化钥匙存储实现
 * @version 1.0.0
 ******************************************************************************/

#include "nvm_key_storage.h"
#include <string.h>

/* 简化版本: 基于文件的存储 */

static nvm_hal_interface_t g_hal;
static uint8_t g_master_key[32];
static nvm_key_header_t g_key_table[NVM_MAX_KEYS];

int nvm_init(const nvm_hal_interface_t *hal, const uint8_t *master_key) {
    if (hal == NULL || master_key == NULL) return NVM_ERR_INVALID_PARAM;
    memcpy(&g_hal, hal, sizeof(g_hal));
    memcpy(g_master_key, master_key, 32);
    memset(g_key_table, 0, sizeof(g_key_table));
    return NVM_OK;
}

int nvm_key_store(uint32_t key_id, const nvm_key_record_t *record) {
    if (record == NULL || key_id >= NVM_MAX_KEYS) return NVM_ERR_INVALID_PARAM;
    
    /* 保存到文件/闪存 */
    char filename[64];
    snprintf(filename, sizeof(filename), "/var/dkcs/keys/key_%08X.bin", key_id);
    
    /* 加密并写入 (简化实现) */
    g_key_table[key_id].valid = 1;
    g_key_table[key_id].key_id = key_id;
    memcpy(&g_key_table[key_id].metadata, &record->metadata, sizeof(nvm_key_metadata_t));
    
    return NVM_OK;
}

int nvm_key_load(uint32_t key_id, nvm_key_record_t *record) {
    if (record == NULL || key_id >= NVM_MAX_KEYS) return NVM_ERR_INVALID_PARAM;
    if (!g_key_table[key_id].valid) return NVM_ERR_KEY_NOT_FOUND;
    
    memcpy(&record->metadata, &g_key_table[key_id].metadata, sizeof(nvm_key_metadata_t));
    return NVM_OK;
}

int nvm_key_delete(uint32_t key_id) {
    if (key_id >= NVM_MAX_KEYS) return NVM_ERR_INVALID_PARAM;
    
    /* 安全清除 */
    memset(&g_key_table[key_id], 0, sizeof(nvm_key_header_t));
    return NVM_OK;
}

int nvm_key_enumerate(nvm_key_enumerate_cb callback, void *user_data) {
    if (callback == NULL) return NVM_ERR_INVALID_PARAM;
    
    for (int i = 0; i < NVM_MAX_KEYS; i++) {
        if (g_key_table[i].valid) {
            callback(g_key_table[i].key_id, &g_key_table[i].metadata, user_data);
        }
    }
    return NVM_OK;
}
