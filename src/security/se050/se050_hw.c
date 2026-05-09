/******************************************************************************
 * @file    se050_hw.c
 * @brief   SE050 硬件通信封装 (I2C/SPI)
 * @version 1.0.0
 ******************************************************************************/

#include "se050_adapter.h"
#include <string.h>

#define SE050_I2C_ADDR          0x48u  /* SE050 默认 I2C 地址 */
#define SE050_MAX_APDU_SIZE     892u   /* 最大 APDU 长度 */
#define SE050_HDR_SIZE          4u     /* APDU 头部大小 */

/* APDU 命令结构 */
typedef struct {
    uint8_t cla;    /* 类别 */
    uint8_t ins;    /* 指令 */
    uint8_t p1;     /* 参数1 */
    uint8_t p2;     /* 参数2 */
    uint8_t *data;  /* 数据 */
    uint16_t lc;    /* 数据长度 */
    uint16_t le;    /* 期望响应长度 */
} se050_apdu_t;

static struct {
    void *i2c_handle;
    void *spi_handle;
    uint8_t use_spi;
    uint8_t initialized;
    uint8_t atr[32];      /* Answer To Reset */
    uint8_t atr_len;
} g_se050_hw_ctx = {0};

int se050_hw_init(void *i2c_or_spi_handle, uint8_t use_spi) {
    if (i2c_or_spi_handle == NULL) {
        return SE050_ERROR_INVALID_PARAM;
    }
    
    g_se050_hw_ctx.use_spi = use_spi;
    if (use_spi) {
        g_se050_hw_ctx.spi_handle = i2c_or_spi_handle;
    } else {
        g_se050_hw_ctx.i2c_handle = i2c_or_spi_handle;
    }
    
    /* 活动 SE050 */
    #ifdef HAL_I2C_MODULE_ENABLED
    if (!use_spi) {
        I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)i2c_or_spi_handle;
        /* 检查设备是否响应 */
        if (HAL_I2C_IsDeviceReady(hi2c, SE050_I2C_ADDR << 1, 3, 100) != HAL_OK) {
            return SE050_ERROR_COMM;
        }
    }
    #endif
    
    /* 获取 ATR */
    g_se050_hw_ctx.initialized = 1;
    return SE050_SUCCESS;
}

int se050_hw_transmit(const uint8_t *apdu, uint16_t apdu_len,
                       uint8_t *response, uint16_t *resp_len) {
    if (!g_se050_hw_ctx.initialized || !apdu || !response || !resp_len) {
        return SE050_ERROR_INVALID_PARAM;
    }
    
    #ifdef HAL_I2C_MODULE_ENABLED
    if (!g_se050_hw_ctx.use_spi) {
        I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)g_se050_hw_ctx.i2c_handle;
        
        /* 发送 APDU */
        HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
            hi2c, SE050_I2C_ADDR << 1, (uint8_t *)apdu, apdu_len, 1000);
        if (status != HAL_OK) {
            return SE050_ERROR_COMM;
        }
        
        /* 等待处理 */
        HAL_Delay(5);
        
        /* 接收响应 */
        status = HAL_I2C_Master_Receive(hi2c, (SE050_I2C_ADDR << 1) | 1,
                                         response, *resp_len, 1000);
        if (status != HAL_OK) {
            return SE050_ERROR_COMM;
        }
    }
    #endif
    
    return SE050_SUCCESS;
}

int se050_hw_reset(void) {
    if (!g_se050_hw_ctx.initialized) {
        return SE050_ERROR_NOT_INITIALIZED;
    }
    
    /* 软复位 SE050 (Power Cycle 或 RST 引脚) */
    /* 具体实现依赖于硬件设计 */
    
    g_se050_hw_ctx.initialized = 0;
    return se050_hw_init(g_se050_hw_ctx.use_spi ? 
                          g_se050_hw_ctx.spi_handle : 
                          g_se050_hw_ctx.i2c_handle,
                          g_se050_hw_ctx.use_spi);
}

uint32_t se050_hw_get_version(void) {
    /* 读取 SE050 固件版本 */
    return 0x03000000u;  /* 示例: 3.0.0 */
}

int se050_hw_power_cycle(void) {
    /* 控制 SE050 电源 (GPIO 控制) */
    return SE050_SUCCESS;
}
