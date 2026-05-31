/**
 * @file sys_time.h
 * @brief System Time Abstraction Layer
 * 
 * Provides a monotonic millisecond tick for embedded time-keeping.
 * The caller must implement sys_tick_get_ms() for the target platform.
 * 
 * On FreeRTOS: return xTaskGetTickCount() * portTICK_PERIOD_MS
 * On baremetal with SysTick: return systick_ms
 * On POSIX simulation: return clock() / (CLOCKS_PER_SEC / 1000)
 */

#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get monotonic system tick in milliseconds
 * 
 * @return uint32_t Milliseconds since system start.
 *         Wraps around after ~49.7 days (2^32 ms).
 *         For relative comparisons, uint32_t subtraction
 *         correctly handles wrap-around as unsigned.
 */
uint32_t sys_tick_get_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_TIME_H */
