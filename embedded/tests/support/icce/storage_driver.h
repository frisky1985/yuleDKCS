/* storage_driver.h — Test stub for ICCE storage driver */
#ifndef STORAGE_DRIVER_H
#define STORAGE_DRIVER_H

#include <stdint.h>

#define STORAGE_OK      0
#define STORAGE_ERR     -1

int32_t storage_init(void);
int32_t storage_write(uint32_t offset, const uint8_t *data, uint16_t len);
int32_t storage_read(uint32_t offset, uint8_t *data, uint16_t len);
int32_t storage_erase(uint32_t offset, uint16_t len);

#endif /* STORAGE_DRIVER_H */
