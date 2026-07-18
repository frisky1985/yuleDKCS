/* cache_manager.h — Test stub for ICCE cache manager */
#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <stdint.h>

#define CACHE_OK            0
#define CACHE_ERR_NOT_FOUND   -1
#define CACHE_ERR_FULL        -2
#define CACHE_ERR_CORRUPT     -3

typedef struct {
    uint32_t key;
    uint8_t  data[256];
    uint16_t len;
    uint32_t timestamp;
} cache_entry_t;

int32_t cache_init(void);
int32_t cache_put(uint32_t key, const uint8_t *data, uint16_t len);
int32_t cache_get(uint32_t key, uint8_t *data, uint16_t *len);
int32_t cache_delete(uint32_t key);
int32_t cache_clear(void);

#endif /* CACHE_MANAGER_H */
