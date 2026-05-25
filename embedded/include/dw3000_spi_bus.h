/******************************************************************************
 * @file    dw3000_spi_bus.h
 * @brief   DW3000 SPI 总线抽象接口
 * @author  YuleTech
 * @date    2026-05-25
 *
 * @note    平台无关的 SPI 操作接口，支持多种后端:
 *          - NXP KW47: fsl_dspi.h
 *          - STM32: HAL SPI
 *          - 测试: mock SPI
 ******************************************************************************/
#ifndef DW3000_SPI_BUS_H
#define DW3000_SPI_BUS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SPI 传输状态 */
typedef enum {
    DW3000_SPI_OK = 0,
    DW3000_SPI_ERROR_PARAM = -1,
    DW3000_SPI_ERROR_TIMEOUT = -2,
    DW3000_SPI_ERROR_BUSY = -3,
} dw3000_spi_status_t;

/**
 * @brief DW3000 SPI 总线操作接口
 *
 * 所有函数返回 0 成功，负值失败。
 */
typedef struct {
    /**
     * @brief 从 DW3000 寄存器读取数据
     * @param reg_addr  寄存器地址 (含 R/W bit 控制)
     * @param data      输出缓冲区
     * @param len       读取字节数
     * @return dw3000_spi_status_t
     */
    int (*read)(uint32_t reg_addr, uint8_t *data, size_t len);

    /**
     * @brief 向 DW3000 寄存器写入数据
     * @param reg_addr  寄存器地址
     * @param data      输入数据
     * @param len       写入字节数
     * @return dw3000_spi_status_t
     */
    int (*write)(uint32_t reg_addr, const uint8_t *data, size_t len);

    /**
     * @brief SPI 全双工传输 (用于批量数据交换)
     * @param tx_data   发送数据缓冲区 (可为 NULL)
     * @param rx_data   接收数据缓冲区 (可为 NULL)
     * @param len       传输字节数
     * @return dw3000_spi_status_t
     */
    int (*transfer)(const uint8_t *tx_data, uint8_t *rx_data, size_t len);
} dw3000_spi_bus_t;

/**
 * @brief 注册 SPI 总线实现
 *
 * 必须在调用任何 dw3000_* 函数前注册。
 * 传入的 bus 指针必须在整个生命周期有效。
 *
 * @param bus  SPI 总线操作接口 (非 NULL)
 */
void dw3000_spi_bus_register(const dw3000_spi_bus_t *bus);

/**
 * @brief 获取当前注册的 SPI 总线
 * @return SPI 总线指针 (未注册时返回 NULL)
 */
const dw3000_spi_bus_t *dw3000_spi_bus_get(void);

/**
 * @brief 默认 SPI 总线 (软件回退实现)
 *
 * 在无硬件环境下，使用内存模拟 SPI 寄存器。
 * 注册方式: dw3000_spi_bus_register(dw3000_spi_bus_default());
 *
 * @return 默认 SPI 总线指针 (静态全局)
 */
const dw3000_spi_bus_t *dw3000_spi_bus_default(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3000_SPI_BUS_H */
