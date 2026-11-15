#include "fsl_flash.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static uint32_t s_block_base;
static uint32_t s_total_size;
static uint32_t s_sector_size;
static uint8_t *s_bytes;
static status_t s_init_status;
static status_t s_erase_status;
static status_t s_program_status;
static uint32_t s_partial_bytes;

static int mock_range(const flash_config_t *config, uint32_t addr, uint32_t len)
{
    uint32_t off;

    if (config == NULL || s_bytes == NULL || len == 0u ||
        config->pflashBlockBase != s_block_base ||
        config->pflashTotalSize != s_total_size || addr < s_block_base) {
        return -1;
    }
    off = addr - s_block_base;
    if (off > s_total_size || len > (s_total_size - off)) {
        return -1;
    }
    return 0;
}

int mcux_mock_flash_reset(uint32_t block_base, uint32_t total_size,
                          uint32_t sector_size)
{
    long page_size = sysconf(_SC_PAGESIZE);
    void *mapped;

    if (s_bytes != NULL) {
        (void)munmap(s_bytes, s_total_size);
        s_bytes = NULL;
    }

    s_block_base = block_base;
    s_total_size = total_size;
    s_sector_size = sector_size;
    s_init_status = kStatus_Success;
    s_erase_status = kStatus_Success;
    s_program_status = kStatus_Success;
    s_partial_bytes = UINT32_MAX;

    if (page_size <= 0 || total_size == 0u || sector_size == 0u ||
        (block_base % (uint32_t)page_size) != 0u ||
        (total_size % (uint32_t)page_size) != 0u) {
        return -1;
    }
    mapped = mmap((void *)(uintptr_t)block_base, total_size,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mapped == MAP_FAILED) {
        return -1;
    }
    s_bytes = (uint8_t *)mapped;
    (void)memset(s_bytes, 0xFF, total_size);
    return 0;
}

void mcux_mock_flash_set_init_status(status_t status)
{
    s_init_status = status;
}

void mcux_mock_flash_set_erase_status(status_t status)
{
    s_erase_status = status;
}

void mcux_mock_flash_set_program_status(status_t status)
{
    s_program_status = status;
}

void mcux_mock_flash_set_program_partial(uint32_t bytes)
{
    s_partial_bytes = bytes;
}

uint8_t *mcux_mock_flash_bytes(void)
{
    return s_bytes;
}

status_t FLASH_Init(flash_config_t *config)
{
    if (config == NULL) {
        return kStatus_FLASH_InvalidArgument;
    }
    if (s_init_status != kStatus_Success) {
        return s_init_status;
    }
    if (s_bytes == NULL) {
        return kStatus_FLASH_Error;
    }
    config->pflashBlockBase = s_block_base;
    config->pflashTotalSize = s_total_size;
    config->pflashBlockCount = 1u;
    config->pflashSectorSize = s_sector_size;
    return kStatus_Success;
}

status_t FLASH_GetProperty(flash_config_t *config,
                           flash_property_tag_t property,
                           uint32_t *value)
{
    if (config == NULL || value == NULL) {
        return kStatus_FLASH_InvalidArgument;
    }
    if (property != kFLASH_PropertyPflash0SectorSize) {
        return kStatus_FLASH_InvalidArgument;
    }
    *value = config->pflashSectorSize;
    return kStatus_Success;
}

status_t FLASH_Erase(flash_config_t *config, uint32_t start,
                     uint32_t lengthInBytes, uint32_t key)
{
    if (mock_range(config, start, lengthInBytes) != 0 ||
        key != kFLASH_ApiEraseKey || s_sector_size == 0u ||
        (start % s_sector_size) != 0u ||
        (lengthInBytes % s_sector_size) != 0u) {
        return kStatus_FLASH_InvalidArgument;
    }
    if (s_erase_status != kStatus_Success) {
        return s_erase_status;
    }
    (void)memset(&s_bytes[start - s_block_base], 0xFF, lengthInBytes);
    return kStatus_Success;
}

status_t FLASH_Program(flash_config_t *config, uint32_t start,
                       uint8_t *src, uint32_t lengthInBytes)
{
    uint32_t off;
    uint32_t n;
    uint32_t i;

    if (src == NULL || mock_range(config, start, lengthInBytes) != 0 ||
        (start % 4u) != 0u || (lengthInBytes % 4u) != 0u) {
        return kStatus_FLASH_InvalidArgument;
    }
    if (s_program_status != kStatus_Success) {
        return s_program_status;
    }

    n = (s_partial_bytes < lengthInBytes) ? s_partial_bytes : lengthInBytes;
    off = start - s_block_base;
    for (i = 0u; i < n; i++) {
        s_bytes[off + i] &= src[i];
    }
    return kStatus_Success;
}
