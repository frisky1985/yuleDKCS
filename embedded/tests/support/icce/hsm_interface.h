/* hsm_interface.h — Test stub for ICCE HSM (Hardware Security Module) */
#ifndef HSM_INTERFACE_H
#define HSM_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#define HSM_OK              0
#define HSM_ERR             -1
#define HSM_ERR_NOT_FOUND   -2

int32_t hsm_init(void);
int32_t hsm_store_key(uint32_t slot, const uint8_t *key, uint16_t len);
int32_t hsm_load_key(uint32_t slot, uint8_t *key, uint16_t *len);
int32_t hsm_delete_key(uint32_t slot);
int32_t hsm_get_free_slots(uint32_t *count);

#endif /* HSM_INTERFACE_H */
