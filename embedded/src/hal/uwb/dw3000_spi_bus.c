/******************************************************************************
 * @file    dw3000_spi_bus.c
 * @brief   DW3000 SPI 总线注册与管理
 * @author  YuleTech
 * @date    2026-05-25
 ******************************************************************************/
#include "dw3000_spi_bus.h"

/* 全局 SPI 总线指针 — 默认为 NULL (未注册) */
static const dw3000_spi_bus_t *s_spi_bus = NULL;

void dw3000_spi_bus_register(const dw3000_spi_bus_t *bus)
{
    s_spi_bus = bus;
}

const dw3000_spi_bus_t *dw3000_spi_bus_get(void)
{
    return s_spi_bus;
}
