/******************************************************************************
 * @file    nvm_eeprom.c
 * @brief   I2C EEPROM 驱动实现 (24Cxx 系列)
 * @version 1.0.0
 ******************************************************************************/

#include "hal/nvm_hal.h"
#include <string.h>

#ifdef HAL_I2C_MODULE_ENABLED

#define EEPROM_PAGE_SIZE        32u
#define EEPROM_WRITE_DELAY_MS   5u
#define EEPROM_MAX_RETRIES      10u

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t dev_addr;          /* I2C 设备地址 (左移一位) */
    uint32_t capacity;          /* 容量 (字芋) */
    uint8_t page_buffer[EEPROM_PAGE_SIZE];
} eeprom_context_t;

static eeprom_context_t g_eeprom_ctx;

int nvm_eeprom_init(void *i2c_handle, uint16_t dev_addr, uint32_t capacity) {
    if (i2c_handle == NULL || capacity == 0) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    g_eeprom_ctx.hi2c = (I2C_HandleTypeDef *)i2c_handle;
    g_eeprom_ctx.dev_addr = dev_addr << 1;  /* HAL 需要 8 位地址 */
    g_eeprom_ctx.capacity = capacity;
    
    /* 检测设备 */
    if (HAL_I2C_IsDeviceReady(g_eeprom_ctx.hi2c, g_eeprom_ctx.dev_addr, 
                               EEPROM_MAX_RETRIES, 100) != HAL_OK) {
        return NVM_ERR_IO;
    }
    
    return NVM_OK;
}

int nvm_eeprom_read(uint32_t addr, uint8_t *data, size_t len) {
    if (addr + len > g_eeprom_ctx.capacity || data == NULL) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    /* 连续读取模式 */
    HAL_StatusTypeDef status;
    
    if (g_eeprom_ctx.capacity > 65536u) {
        /* 24C256/512 需要 2 字节地址 */
        status = HAL_I2C_Mem_Read(g_eeprom_ctx.hi2c, g_eeprom_ctx.dev_addr,
                                   addr, I2C_MEMADD_SIZE_16BIT,
                                   data, len, 1000);
    } else {
        /* 24C02/04/08/16 使用 1 字节地址 */
        status = HAL_I2C_Mem_Read(g_eeprom_ctx.hi2c, g_eeprom_ctx.dev_addr,
                                   addr & 0xFF, I2C_MEMADD_SIZE_8BIT,
                                   data, len, 1000);
    }
    
    return (status == HAL_OK) ? NVM_OK : NVM_ERR_IO;
}

int nvm_eeprom_write(uint32_t addr, const uint8_t *data, size_t len) {
    if (addr + len > g_eeprom_ctx.capacity || data == NULL) {
        return NVM_ERR_INVALID_PARAM;
    }
    
    size_t written = 0;
    while (written < len) {
        /* 计算当前页剩余空间 */
        uint32_t page_offset = (addr + written) % EEPROM_PAGE_SIZE;
        uint32_t to_write = EEPROM_PAGE_SIZE - page_offset;
        if (to_write > len - written) {
            to_write = len - written;
        }
        
        /* 写入一页 */
        HAL_StatusTypeDef status;
        if (g_eeprom_ctx.capacity > 65536u) {
            status = HAL_I2C_Mem_Write(g_eeprom_ctx.hi2c, g_eeprom_ctx.dev_addr,
                                        addr + written, I2C_MEMADD_SIZE_16BIT,
                                        (uint8_t *)data + written, to_write, 1000);
        } else {
            status = HAL_I2C_Mem_Write(g_eeprom_ctx.hi2c, g_eeprom_ctx.dev_addr,
                                        (addr + written) & 0xFF, I2C_MEMADD_SIZE_8BIT,
                                        (uint8_t *)data + written, to_write, 1000);
        }
        
        if (status != HAL_OK) {
            return NVM_ERR_IO;
        }
        
        /* 等待写入完成 */
        HAL_Delay(EEPROM_WRITE_DELAY_MS);
        
        written += to_write;
    }
    
    return NVM_OK;
}

int nvm_eeprom_erase(uint32_t addr, size_t len) {
    /* EEPROM 需要手动擦除 (u5199入 0xFF) */
    uint8_t erase_pattern[64];
    memset(erase_pattern, 0xFF, sizeof(erase_pattern));
    
    size_t erased = 0;
    while (erased < len) {
        size_t chunk = (len - erased < sizeof(erase_pattern)) ? 
                       (len - erased) : sizeof(erase_pattern);
        int ret = nvm_eeprom_write(addr + erased, erase_pattern, chunk);
        if (ret != NVM_OK) {
            return ret;
        }
        erased += chunk;
    }
    
    return NVM_OK;
}

#endif /* HAL_I2C_MODULE_ENABLED */
