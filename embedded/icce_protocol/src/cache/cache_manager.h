#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* Cache Result Codes */
typedef enum {
    CACHE_SUCCESS = 0,
    CACHE_ERR_INVALID_PARAM = -1,
    CACHE_ERR_INVALID_KEY = -2,
    CACHE_ERR_INVALID_VALUE = -3,
    CACHE_ERR_NOT_FOUND = -4,
    CACHE_ERR_EXPIRED = -5,
    CACHE_ERR_STORAGE_FULL = -6,
} cache_result_t;

/* Cache Type */
typedef enum {
    CACHE_TYPE_MEMORY = 0,
    CACHE_TYPE_PERSISTENT = 1,
    CACHE_TYPE_SECURE = 2,
} cache_type_t;

/* Cache Policy */
typedef enum {
    CACHE_POLICY_FIFO = 0,
    CACHE_POLICY_LRU = 1,
    CACHE_POLICY_TTL = 2,
} cache_policy_t;

/* Cache Config */
typedef struct {
    cache_type_t type;
    cache_policy_t policy;
    uint32_t max_size;
    uint32_t default_ttl;
    bool enable_sync;
    uint32_t sync_interval;
} cache_config_t;

/* Cache Item */
typedef struct {
    uint8_t key[64];
    uint16_t key_len;
    uint8_t *value;
    uint32_t value_len;
    uint32_t expiry_time;
} cache_item_t;

/* Cache Stats */
typedef struct {
    uint32_t total_items;
    uint32_t hit_count;
    uint32_t miss_count;
    uint32_t eviction_count;
    uint32_t sync_count;
    uint32_t memory_usage;
    float hit_rate;
} cache_stats_t;

/* Cache Manager API */
cache_result_t cache_init(const cache_config_t *config);
cache_result_t cache_get(const uint8_t *key, uint16_t key_len, uint8_t *value, uint32_t *value_len);
cache_result_t cache_set(const uint8_t *key, uint16_t key_len, const uint8_t *value, uint32_t value_len, uint32_t ttl);
cache_result_t cache_delete(const uint8_t *key, uint16_t key_len);
cache_result_t cache_clear(void);
cache_result_t cache_exists(const uint8_t *key, uint16_t key_len, bool *exists);
cache_result_t cache_get_stats(cache_stats_t *stats);
cache_result_t cache_sync(void);
cache_result_t cache_set_policy(cache_policy_t policy);
cache_result_t cache_preload(const cache_item_t *items, uint32_t count);

#endif /* CACHE_MANAGER_H */
