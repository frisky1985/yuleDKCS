#ifndef MCUX_MOCK_FSL_CLOCK_H
#define MCUX_MOCK_FSL_CLOCK_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_Type;

#define SysTick ((SysTick_Type *)0xE000E010u)
#define kBLE_LLParams 0

uint32_t CLOCK_GetCoreSysClkFreq(void);
uint32_t CLOCK_GetLpspiClkFreq(uint32_t instance);
uint32_t CLOCK_GetLpi2cClkFreq(uint32_t instance);
void SDK_DelayAtLeastUs(uint32_t delay_us, uint32_t core_clock_hz);
uint32_t xTaskGetTickCount(void);

#endif /* MCUX_MOCK_FSL_CLOCK_H */
