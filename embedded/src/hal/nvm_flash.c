/******************************************************************************
 * @file    nvm_flash.c
 * @brief   STM32 Flash 底层驱动实现
 * @version 1.0.0
 ******************************************************************************/

#include "hal/nvm_hal.h"
#include <string.h>

#ifdef HAL_FLASH_MODULE_ENABLED

/* STM32 Flash 操作实现 */

#define FLASH_MAGIC_HEADER      0x564D4E00u  /* "NVM" */
#define FLASH_VERSION           0x00010000u  /* v1.0.0 */
#define FLASH_CRC32_POLYNOMIAL  0x04C11DB7u

static uint32_t flash_calculate_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i] << 24;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x80000000u) ? FLASH_CRC32_POLYNOMIAL : 0);
        }
    }
    return ~crc;
}

int nvm_flash_init(nvm_hal_context_t *ctx, uint32_t base_addr, uint32_t size) {
    if (ctx == NULL || base_addr == 0 || size == 0) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    ctx->base_addr = base_addr;
    ctx->total_size = size;
    ctx->page_size = FLASH_PAGE_SIZE;  /* 依赖于具体芯片 */
    ctx->initialized = 1;
    
    /* 检查 Flash 是否已有有效数据 */
    uint32_t *magic = (uint32_t *)base_addr;
    if (*magic == FLASH_MAGIC_HEADER) {
        /* 验证 CRC */
        uint32_t stored_crc = *(uint32_t *)(base_addr + 8);
        uint32_t calc_crc = flash_calculate_crc32(
            (uint8_t *)(base_addr + 12), 
            *(uint32_t *)(base_addr + 4) - 12
        );
        if (stored_crc != calc_crc) {
            return NVM_ERR_CORRUPTED;
        }
    }
    
    return NVM_OK;
}

int nvm_flash_read(nvm_hal_context_t *ctx, uint32_t offset, uint8_t *data, size_t len) {
    if (!ctx || !ctx->initialized || !data || offset + len > ctx->total_size) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    memcpy(data, (void *)(ctx->base_addr + offset), len);
    return NVM_OK;
}

int nvm_flash_write(nvm_hal_context_t *ctx, uint32_t offset, const uint8_t *data, size_t len) {
    if (!ctx || !ctx->initialized || !data || offset + len > ctx->total_size) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    HAL_FLASH_Unlock();
    
    /* STM32 需要按双字/四字写入 */
    uint32_t addr = ctx->base_addr + offset;
    size_t i = 0;
    
    /* 对齐到 8 字节边界 */
    while (i < len && (addr + i) % 8 != 0) {
        uint64_t value = data[i];
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, value);
        i++;
    }
    
    /* 批量写入 8 字节 */
    while (i + 7 < len) {
        uint64_t value = 0;
        memcpy(&value, &data[i], 8);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, value);
        i += 8;
    }
    
    /* 剩余字节 */
    while (i < len) {
        uint64_t value = data[i];
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, value);
        i++;
    }
    
    HAL_FLASH_Lock();
    return NVM_OK;
}

int nvm_flash_erase(nvm_hal_context_t *ctx, uint32_t offset, size_t len) {
    if (!ctx || !ctx->initialized) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    HAL_FLASH_Unlock();
    
    /* 计算需要擦除的页面 */
    uint32_t start_page = (ctx->base_addr + offset) / ctx->page_size;
    uint32_t end_page = (ctx->base_addr + offset + len - 1) / ctx->page_size;
    
    FLASH_EraseInitTypeDef erase_config = {0};
    erase_config.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_config.Page = start_page;
    erase_config.NbPages = end_page - start_page + 1;
    
    uint32_t error = 0;
    HAL_FLASHEx_Erase(&erase_config, &error);
    
    HAL_FLASH_Lock();
    return (error == 0xFFFFFFFFu) ? NVM_OK : NVM_ERR_IO;
}

int nvm_flash_get_info(nvm_hal_context_t *ctx, nvm_hal_info_t *info) {
    if (!ctx || !info) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    info->total_size = ctx->total_size;
    info->page_size = ctx->page_size;
    info->block_size = ctx->page_size;
    info->erase_cycles = 10000;  /* 典型值 */
    info->data_retention_years = 30;
    
    return NVM_OK;
}

#endif /* HAL_FLASH_MODULE_ENABLED */
