/******************************************************************************
 * @file    platform_time.h
 * @brief   平台时间抽象接口
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-26
 *
 * @note    所有模块统一通过此接口获取时间戳，避免分散的 TODO 占位。
 *          嵌入式目标上应对接 RTC 或 systick；测试环境使用单调递增计数器。
 ******************************************************************************/
#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取当前时间戳（毫秒）
 *
 * 返回值不保证绝对精度，仅保证单调递增，适用于：
 *   - 会话过期判断
 *   - 速率限制窗口
 *   - 超时等待
 *   - 活动时间更新
 *
 * @return 单调递增的毫秒级时间戳
 */
uint32_t platform_get_ms(void);

/**
 * @brief 获取当前时间戳（秒）
 *
 * 基于 platform_get_ms() 换算，适用于有效期、日志等粗粒度场景。
 *
 * @return 单调递增的秒级时间戳
 */
uint32_t platform_get_sec(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_TIME_H */
