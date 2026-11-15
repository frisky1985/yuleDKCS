#ifndef MCUX_MOCK_FSL_FLASH_H
#define MCUX_MOCK_FSL_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t status_t;

enum {
    kStatus_Success = 0,
    kStatus_FLASH_Error = 100,
    kStatus_FLASH_InvalidArgument = 101,
    kStatus_FLASH_EraseFailure = 102,
    kStatus_FLASH_ProgramFailure = 103
};

typedef enum {
    kFLASH_PropertyPflash0SectorSize = 0
} flash_property_tag_t;

#define kFLASH_ApiEraseKey 0x6B65666Cu

typedef struct {
    uint32_t pflashBlockBase;
    uint32_t pflashTotalSize;
    uint32_t pflashBlockCount;
    uint32_t pflashSectorSize;
} flash_config_t;

status_t FLASH_Init(flash_config_t *config);
status_t FLASH_GetProperty(flash_config_t *config,
                           flash_property_tag_t property,
                           uint32_t *value);
status_t FLASH_Erase(flash_config_t *config, uint32_t start,
                     uint32_t lengthInBytes, uint32_t key);
status_t FLASH_Program(flash_config_t *config, uint32_t start,
                       uint8_t *src, uint32_t lengthInBytes);

/* Host-only mock controls. */
int mcux_mock_flash_reset(uint32_t block_base, uint32_t total_size,
                          uint32_t sector_size);
void mcux_mock_flash_set_init_status(status_t status);
void mcux_mock_flash_set_erase_status(status_t status);
void mcux_mock_flash_set_program_status(status_t status);
void mcux_mock_flash_set_program_partial(uint32_t bytes);
uint8_t *mcux_mock_flash_bytes(void);

#ifdef __cplusplus
}
#endif

#endif /* MCUX_MOCK_FSL_FLASH_H */
