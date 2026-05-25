/******************************************************************************
 * @file    dw3000_spi_bus_default.c
 * @brief   DW3000 SPI 总线默认实现 (软件回退/模拟)
 * @author  YuleTech
 * @date    2026-05-25
 *
 * @note    使用静态缓冲区模拟 SPI 寄存器操作。
 *          仅用于开发和测试环境，生产环境应替换为硬件 SPI 驱动。
 ******************************************************************************/
#include "dw3000_spi_bus.h"
#include <string.h>
#include <stddef.h>

/* ====================================================================
 * 模拟 SPI 寄存器空间 (256KB)
 * 生产环境应替换为 NXP fsl_dspi 或 STM32 HAL SPI 实现
 * ==================================================================== */
#define MOCK_REG_SPACE_SIZE (256 * 1024)
static uint8_t s_mock_regs[MOCK_REG_SPACE_SIZE];
static int s_mock_initialized = 0;

/* ====================================================================
 * SPI 时钟延迟 (模拟传输耗时)
 * ==================================================================== */
static void spi_delay_us(unsigned int us)
{
    /* 简单忙等循环 — 生产环境应替换为硬件定时器 */
    volatile unsigned int loops = us * 10;
    while (loops-- > 0) {
        __asm__ volatile("nop");
    }
}

/* ====================================================================
 * SPI 读操作 (模拟)
 * ==================================================================== */
static int mock_spi_read(uint32_t reg_addr, uint8_t *data, size_t len)
{
    if (!data || len == 0) return DW3000_SPI_ERROR_PARAM;

    /* 初始化模拟空间 */
    if (!s_mock_initialized) {
        memset(s_mock_regs, 0, sizeof(s_mock_regs));
        s_mock_initialized = 1;
    }

    /* 地址范围检查 */
    size_t offset = (size_t)(reg_addr & 0x3FFFF);
    if (offset + len > MOCK_REG_SPACE_SIZE) {
        return DW3000_SPI_ERROR_PARAM;
    }

    /* 模拟 SPI 传输延迟 */
    spi_delay_us(1);

    /* 从模拟寄存器读取 */
    memcpy(data, s_mock_regs + offset, len);
    return DW3000_SPI_OK;
}

/* ====================================================================
 * SPI 写操作 (模拟)
 * ==================================================================== */
static int mock_spi_write(uint32_t reg_addr, const uint8_t *data, size_t len)
{
    if (!data || len == 0) return DW3000_SPI_ERROR_PARAM;

    if (!s_mock_initialized) {
        memset(s_mock_regs, 0, sizeof(s_mock_regs));
        s_mock_initialized = 1;
    }

    size_t offset = (size_t)(reg_addr & 0x3FFFF);
    if (offset + len > MOCK_REG_SPACE_SIZE) {
        return DW3000_SPI_ERROR_PARAM;
    }

    spi_delay_us(1);
    memcpy(s_mock_regs + offset, data, len);
    return DW3000_SPI_OK;
}

/* ====================================================================
 * SPI 全双工传输 (模拟)
 * ==================================================================== */
static int mock_spi_transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t len)
{
    if (!tx_data && !rx_data) return DW3000_SPI_ERROR_PARAM;
    if (len == 0) return DW3000_SPI_OK;

    spi_delay_us(len);

    /* 模拟 SPI 全双工: tx → rx 直通 */
    if (tx_data && rx_data) {
        memcpy(rx_data, tx_data, len);
    } else if (tx_data) {
        /* 只写 */
    } else {
        /* 只读 — 填充 0 */
        memset(rx_data, 0, len);
    }

    return DW3000_SPI_OK;
}

/* ====================================================================
 * SPI 总线接口实例
 * ==================================================================== */
static const dw3000_spi_bus_t s_default_spi_bus = {
    .read     = mock_spi_read,
    .write    = mock_spi_write,
    .transfer = mock_spi_transfer,
};

const dw3000_spi_bus_t *dw3000_spi_bus_default(void)
{
    return &s_default_spi_bus;
}
