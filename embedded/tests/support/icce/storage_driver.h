/* storage_driver.h — Test stub for ICCE storage driver
 *
 * Matches the real API signature (handle + key-based KV), so
 * edge_condition.c compiles against it in unit-test builds.
 */
#ifndef STORAGE_DRIVER_H
#define STORAGE_DRIVER_H

#include <stdint.h>

#define STORAGE_SUCCESS 0
#define STORAGE_ERR     -1

/* Storage Driver API (handle + key-based KV, mirrors icce_protocol/src/cache/storage_driver.h) */
int32_t storage_init(uint8_t *handle);
int32_t storage_write(uint8_t handle, const uint8_t *key, uint16_t key_len,
                      const uint8_t *data, uint32_t data_len);
int32_t storage_read(uint8_t handle, const uint8_t *key, uint16_t key_len,
                     uint8_t *data, uint32_t *data_len);
int32_t storage_delete(uint8_t handle, const uint8_t *key, uint16_t key_len);

#endif /* STORAGE_DRIVER_H */
