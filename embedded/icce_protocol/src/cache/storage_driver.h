#ifndef STORAGE_DRIVER_H
#define STORAGE_DRIVER_H

#include <stdint.h>

#define STORAGE_SUCCESS 0

/* Storage Driver API */
int32_t storage_init(uint8_t *handle);
int32_t storage_write(uint8_t handle, const uint8_t *key, uint16_t key_len, const uint8_t *data, uint32_t data_len);
int32_t storage_read(uint8_t handle, const uint8_t *key, uint16_t key_len, uint8_t *data, uint32_t *data_len);
int32_t storage_delete(uint8_t handle, const uint8_t *key, uint16_t key_len);

#endif /* STORAGE_DRIVER_H */
