/**
 * @file cache_manager.c
 * @brief 缓存管理模块实现
 * @version 1.0
 * @date 2026-05-05
 * 
 * 提供内存和持久化缓存功能
 */

#include "cache_manager.h"
#include "storage_driver.h"
#include <string.h>
#include <stdlib.h>

/* 私有定义 */
#define MAX_CACHE_ITEMS         256
#define HASH_TABLE_SIZE         512
#define MAX_KEY_LENGTH          64
#define MAX_VALUE_LENGTH        4096

/* 缓存项结构 */
typedef struct cache_entry {
    uint8_t key[MAX_KEY_LENGTH];
    uint16_t key_len;
    uint8_t *value;
    uint32_t value_len;
    uint32_t creation_time;
    uint32_t expiry_time;
    uint32_t access_count;
    uint32_t last_access_time;
    uint8_t flags;
    uint8_t in_use;
    struct cache_entry *prev;
    struct cache_entry *next;
    struct cache_entry *hash_next;
} cache_entry_t;

/* 缓存管理器实例 */
typedef struct {
    bool initialized;
    cache_type_t type;
    cache_policy_t policy;
    uint32_t max_size;
    uint32_t default_ttl;
    bool enable_sync;
    uint32_t sync_interval;
    
    /* LRU链表 */
    cache_entry_t *lru_head;
    cache_entry_t *lru_tail;
    
    /* 哈希表 */
    cache_entry_t *hash_table[HASH_TABLE_SIZE];
    
    /* 项目池 */
    cache_entry_t item_pool[MAX_CACHE_ITEMS];
    uint32_t item_count;
    
    /* 统计 */
    uint32_t hit_count;
    uint32_t miss_count;
    uint32_t eviction_count;
    uint32_t sync_count;
    
    /* 持久化存储 */
    uint8_t storage_handle;
} cache_manager_t;

/* 全局实例 */
static cache_manager_t g_cache = {0};

/* 前向声明 */
static uint32_t hash_key(const uint8_t *key, uint16_t key_len);
static cache_entry_t* find_entry(const uint8_t *key, uint16_t key_len);
static void move_to_head(cache_entry_t *entry);
static void remove_from_list(cache_entry_t *entry);
static void evict_lru(void);
static int32_t save_to_storage(const cache_entry_t *entry);
static int32_t load_from_storage(cache_entry_t *entry);
static uint32_t get_current_time(void);

/*============================================================================
 * 公开API实现
 *===========================================================================*/

cache_result_t cache_init(const cache_config_t *config)
{
    if (g_cache.initialized) {
        return CACHE_SUCCESS;
    }
    
    if (!config) {
        return CACHE_ERR_INVALID_PARAM;
    }
    
    /* 初始化实例 */
    memset(&g_cache, 0, sizeof(cache_manager_t));
    
    g_cache.type = config->type;
    g_cache.policy = config->policy;
    g_cache.max_size = config->max_size;
    g_cache.default_ttl = config->default_ttl;
    g_cache.enable_sync = config->enable_sync;
    g_cache.sync_interval = config->sync_interval;
    
    /* 初始化存储 */
    if (g_cache.type == CACHE_TYPE_PERSISTENT || 
        g_cache.type == CACHE_TYPE_SECURE) {
        if (storage_init(&g_cache.storage_handle) != STORAGE_SUCCESS) {
            return CACHE_ERR_STORAGE_FULL;
        }
    }
    
    g_cache.initialized = true;
    return CACHE_SUCCESS;
}

cache_result_t cache_get(const uint8_t *key, uint16_t key_len,
                         uint8_t *value, uint32_t *value_len)
{
    if (!g_cache.initialized || !key || !value || !value_len) {
        return CACHE_ERR_INVALID_KEY;
    }
    
    cache_entry_t *entry = find_entry(key, key_len);
    
    if (!entry || !entry->in_use) {
        g_cache.miss_count++;
        return CACHE_ERR_NOT_FOUND;
    }
    
    /* 检查是否过期 */
    uint32_t current_time = get_current_time();
    if (entry->expiry_time > 0 && current_time > entry->expiry_time) {
        /* 删除过期项 */
        entry->in_use = 0;
        remove_from_list(entry);
        g_cache.miss_count++;
        return CACHE_ERR_EXPIRED;
    }
    
    /* 检查缓冲区大小 */
    if (*value_len < entry->value_len) {
        g_cache.miss_count++;
        return CACHE_ERR_INVALID_VALUE;
    }
    
    /* 复制数据 */
    memcpy(value, entry->value, entry->value_len);
    *value_len = entry->value_len;
    
    /* 更新访问统计 */
    entry->access_count++;
    entry->last_access_time = current_time;
    
    /* 更新LRU */
    if (g_cache.policy == CACHE_POLICY_LRU) {
        move_to_head(entry);
    }
    
    g_cache.hit_count++;
    
    return CACHE_SUCCESS;
}

cache_result_t cache_set(const uint8_t *key, uint16_t key_len,
                         const uint8_t *value, uint32_t value_len,
                         uint32_t ttl)
{
    if (!g_cache.initialized || !key || !value) {
        return CACHE_ERR_INVALID_KEY;
    }
    
    if (key_len > MAX_KEY_LENGTH) {
        return CACHE_ERR_INVALID_KEY;
    }
    
    if (value_len > MAX_VALUE_LENGTH) {
        return CACHE_ERR_INVALID_VALUE;
    }
    
    /* 查找现有项 */
    cache_entry_t *entry = find_entry(key, key_len);
    
    if (entry && entry->in_use) {
        /* 更新现有项 */
        if (entry->value) {
            free(entry->value);
        }
        
        entry->value = (uint8_t*)malloc(value_len);
        if (!entry->value) {
            entry->in_use = 0;
            return CACHE_ERR_STORAGE_FULL;
        }
        
        memcpy(entry->value, value, value_len);
        entry->value_len = value_len;
        
        uint32_t current_time = get_current_time();
        entry->expiry_time = (ttl > 0) ? (current_time + ttl * 1000) : 0;
        entry->last_access_time = current_time;
        
        if (g_cache.policy == CACHE_POLICY_LRU) {
            move_to_head(entry);
        }
    } else {
        /* 检查容量 */
        if (g_cache.item_count >= MAX_CACHE_ITEMS) {
            evict_lru();
            
            if (g_cache.item_count >= MAX_CACHE_ITEMS) {
                return CACHE_ERR_STORAGE_FULL;
            }
        }
        
        /* 查找空闲槽 */
        entry = NULL;
        for (int i = 0; i < MAX_CACHE_ITEMS; i++) {
            if (!g_cache.item_pool[i].in_use) {
                entry = &g_cache.item_pool[i];
                break;
            }
        }
        
        if (!entry) {
            return CACHE_ERR_STORAGE_FULL;
        }
        
        /* 初始化新项 */
        memcpy(entry->key, key, key_len);
        entry->key_len = key_len;
        
        entry->value = (uint8_t*)malloc(value_len);
        if (!entry->value) {
            return CACHE_ERR_STORAGE_FULL;
        }
        
        memcpy(entry->value, value, value_len);
        entry->value_len = value_len;
        
        uint32_t current_time = get_current_time();
        entry->creation_time = current_time;
        entry->expiry_time = (ttl > 0) ? (current_time + ttl * 1000) : 
                                          (g_cache.default_ttl > 0 ? 
                                           current_time + g_cache.default_ttl * 1000 : 0);
        entry->access_count = 0;
        entry->last_access_time = current_time;
        entry->in_use = 1;
        
        /* 添加到哈希表 */
        uint32_t hash = hash_key(key, key_len);
        entry->hash_next = g_cache.hash_table[hash];
        g_cache.hash_table[hash] = entry;
        
        /* 添加到LRU链表 */
        entry->prev = NULL;
        entry->next = g_cache.lru_head;
        if (g_cache.lru_head) {
            g_cache.lru_head->prev = entry;
        }
        g_cache.lru_head = entry;
        if (!g_cache.lru_tail) {
            g_cache.lru_tail = entry;
        }
        
        g_cache.item_count++;
    }
    
    /* 持久化 */
    if (g_cache.enable_sync && 
        (g_cache.type == CACHE_TYPE_PERSISTENT || 
         g_cache.type == CACHE_TYPE_SECURE)) {
        save_to_storage(entry);
        g_cache.sync_count++;
    }
    
    return CACHE_SUCCESS;
}

cache_result_t cache_delete(const uint8_t *key, uint16_t key_len)
{
    if (!g_cache.initialized || !key) {
        return CACHE_ERR_INVALID_KEY;
    }
    
    cache_entry_t *entry = find_entry(key, key_len);
    
    if (!entry || !entry->in_use) {
        return CACHE_ERR_NOT_FOUND;
    }
    
    /* 从哈希表移除 */
    uint32_t hash = hash_key(key, key_len);
    cache_entry_t *prev = NULL;
    cache_entry_t *curr = g_cache.hash_table[hash];
    
    while (curr) {
        if (curr == entry) {
            if (prev) {
                prev->hash_next = curr->hash_next;
            } else {
                g_cache.hash_table[hash] = curr->hash_next;
            }
            break;
        }
        prev = curr;
        curr = curr->hash_next;
    }
    
    /* 从LRU链表移除 */
    remove_from_list(entry);
    
    /* 释放内存 */
    if (entry->value) {
        free(entry->value);
        entry->value = NULL;
    }
    
    entry->in_use = 0;
    g_cache.item_count--;
    
    return CACHE_SUCCESS;
}

cache_result_t cache_clear(void)
{
    if (!g_cache.initialized) {
        return CACHE_ERR_INVALID_PARAM;
    }
    
    /* 释放所有值 */
    for (int i = 0; i < MAX_CACHE_ITEMS; i++) {
        if (g_cache.item_pool[i].in_use && g_cache.item_pool[i].value) {
            free(g_cache.item_pool[i].value);
        }
        g_cache.item_pool[i].in_use = 0;
    }
    
    /* 清空哈希表 */
    memset(g_cache.hash_table, 0, sizeof(g_cache.hash_table));
    
    /* 清空LRU链表 */
    g_cache.lru_head = NULL;
    g_cache.lru_tail = NULL;
    
    g_cache.item_count = 0;
    
    return CACHE_SUCCESS;
}

cache_result_t cache_exists(const uint8_t *key, uint16_t key_len, bool *exists)
{
    if (!g_cache.initialized || !key || !exists) {
        return CACHE_ERR_INVALID_KEY;
    }
    
    cache_entry_t *entry = find_entry(key, key_len);
    *exists = (entry && entry->in_use);
    
    return CACHE_SUCCESS;
}

cache_result_t cache_get_stats(cache_stats_t *stats)
{
    if (!g_cache.initialized || !stats) {
        return CACHE_ERR_INVALID_PARAM;
    }
    
    stats->total_items = g_cache.item_count;
    stats->hit_count = g_cache.hit_count;
    stats->miss_count = g_cache.miss_count;
    stats->eviction_count = g_cache.eviction_count;
    stats->sync_count = g_cache.sync_count;
    
    /* 计算内存使用 */
    uint32_t memory_usage = 0;
    for (int i = 0; i < MAX_CACHE_ITEMS; i++) {
        if (g_cache.item_pool[i].in_use) {
            memory_usage += g_cache.item_pool[i].value_len;
        }
    }
    stats->memory_usage = memory_usage;
    
    /* 计算命中率 */
    uint32_t total = g_cache.hit_count + g_cache.miss_count;
    stats->hit_rate = (total > 0) ? 
                      (float)g_cache.hit_count / total : 0.0f;
    
    return CACHE_SUCCESS;
}

cache_result_t cache_sync(void)
{
    if (!g_cache.initialized) {
        return CACHE_ERR_INVALID_PARAM;
    }
    
    if (!g_cache.enable_sync) {
        return CACHE_SUCCESS;
    }
    
    /* 同步所有项到存储 */
    for (int i = 0; i < MAX_CACHE_ITEMS; i++) {
        if (g_cache.item_pool[i].in_use) {
            save_to_storage(&g_cache.item_pool[i]);
        }
    }
    
    g_cache.sync_count++;
    
    return CACHE_SUCCESS;
}

cache_result_t cache_set_policy(cache_policy_t policy)
{
    if (!g_cache.initialized) {
        return CACHE_ERR_INVALID_PARAM;
    }
    
    g_cache.policy = policy;
    return CACHE_SUCCESS;
}

cache_result_t cache_preload(const cache_item_t *items, uint32_t count)
{
    if (!g_cache.initialized || !items) {
        return CACHE_ERR_INVALID_PARAM;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t ttl = 0;
        if (items[i].expiry_time > 0) {
            uint32_t current = get_current_time();
            ttl = (items[i].expiry_time > current) ? 
                  (items[i].expiry_time - current) / 1000 : 0;
        }
        
        cache_result_t result = cache_set(items[i].key, items[i].key_len,
                                          items[i].value, items[i].value_len,
                                          ttl);
        if (result != CACHE_SUCCESS) {
            return result;
        }
    }
    
    return CACHE_SUCCESS;
}

/*============================================================================
 * 私有函数实现
 *===========================================================================*/

static uint32_t hash_key(const uint8_t *key, uint16_t key_len)
{
    uint32_t hash = 5381;
    for (int i = 0; i < key_len; i++) {
        hash = ((hash << 5) + hash) + key[i];
    }
    return hash % HASH_TABLE_SIZE;
}

static cache_entry_t* find_entry(const uint8_t *key, uint16_t key_len)
{
    uint32_t hash = hash_key(key, key_len);
    
    cache_entry_t *entry = g_cache.hash_table[hash];
    while (entry) {
        if (entry->key_len == key_len && 
            memcmp(entry->key, key, key_len) == 0) {
            return entry;
        }
        entry = entry->hash_next;
    }
    
    return NULL;
}

static void move_to_head(cache_entry_t *entry)
{
    if (entry == g_cache.lru_head) {
        return;
    }
    
    remove_from_list(entry);
    
    entry->prev = NULL;
    entry->next = g_cache.lru_head;
    if (g_cache.lru_head) {
        g_cache.lru_head->prev = entry;
    }
    g_cache.lru_head = entry;
    
    if (!g_cache.lru_tail) {
        g_cache.lru_tail = entry;
    }
}

static void remove_from_list(cache_entry_t *entry)
{
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        g_cache.lru_head = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        g_cache.lru_tail = entry->prev;
    }
}

static void evict_lru(void)
{
    if (!g_cache.lru_tail) {
        return;
    }
    
    cache_entry_t *entry = g_cache.lru_tail;
    
    cache_delete(entry->key, entry->key_len);
    g_cache.eviction_count++;
}

static int32_t save_to_storage(const cache_entry_t *entry)
{
    if (!entry || !g_cache.storage_handle) {
        return -1;
    }
    
    /* 构建存储键 */
    uint8_t storage_key[8];
    storage_key[0] = 'C';
    storage_key[1] = 'A';
    storage_key[2] = 'C';
    storage_key[3] = 'H';
    *((uint32_t*)&storage_key[4]) = hash_key(entry->key, entry->key_len);
    
    /* 写入存储 */
    return storage_write(g_cache.storage_handle, 
                         storage_key, sizeof(storage_key),
                         entry->value, entry->value_len);
}

static int32_t load_from_storage(cache_entry_t *entry)
{
    if (!entry || !g_cache.storage_handle) {
        return -1;
    }
    
    /* 构建存储键 */
    uint8_t storage_key[8];
    storage_key[0] = 'C';
    storage_key[1] = 'A';
    storage_key[2] = 'C';
    storage_key[3] = 'H';
    *((uint32_t*)&storage_key[4]) = hash_key(entry->key, entry->key_len);
    
    /* 读取存储 */
    uint32_t len = entry->value_len;
    return storage_read(g_cache.storage_handle,
                        storage_key, sizeof(storage_key),
                        entry->value, &len);
}

static uint32_t get_current_time(void)
{
    /* TODO: 实际时间获取 */
    return 0;
}

/*============================================================================
 * End of file
 *===========================================================================*/
