/******************************************************************************
 * @file    platform_time.c
 * @brief   平台时间抽象实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-26
 *
 * @note    使用单调递增计数器模拟系统时钟。
 *          在真实硬件上应接 HAL_GetTick() / RTC。
 ******************************************************************************/
#include "platform_time.h"

static volatile uint32_t g_tick_counter = 0;

uint32_t platform_get_ms(void)
{
    /* 单调递增计数器：每次调用增加 10ms
     * 真实硬件应替换为 HAL_GetTick() 或 RTC 接口 */
    g_tick_counter += 10;
    return g_tick_counter;
}

uint32_t platform_get_sec(void)
{
    return platform_get_ms() / 1000;
}
