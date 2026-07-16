/**
 * @file timers.h
 * @brief FreeRTOS Software Timer API (stub for compilation)
 */

#ifndef TIMERS_H
#define TIMERS_H

#include "FreeRTOS.h"
#include "list.h"
#include "queue.h"

struct tmrTimerControl;
typedef struct tmrTimerControl *TimerHandle_t;

typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

#define tmrNO_DELAY                     0
#define tmrCOMMAND_EXECUTE_CALLBACK     0
#define tmrCOMMAND_START                1
#define tmrCOMMAND_STOP                 2
#define tmrCOMMAND_CHANGE_PERIOD        3
#define tmrCOMMAND_RESET                4
#define tmrCOMMAND_START_FROM_ISR       5
#define tmrCOMMAND_STOP_FROM_ISR        6
#define tmrCOMMAND_RESET_FROM_ISR       7
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR 8

TimerHandle_t xTimerCreate(const char * const pcTimerName,
                           const TickType_t xTimerPeriodInTicks,
                           const UBaseType_t uxAutoReload,
                           void * const pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction);

BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer);
BaseType_t xTimerStart(TimerHandle_t xTimer, TickType_t xTicksToWait);
BaseType_t xTimerStop(TimerHandle_t xTimer, TickType_t xTicksToWait);
BaseType_t xTimerChangePeriod(TimerHandle_t xTimer, TickType_t xNewPeriod, TickType_t xTicksToWait);
BaseType_t xTimerReset(TimerHandle_t xTimer, TickType_t xTicksToWait);

BaseType_t xTimerStartFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xTimerStopFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xTimerChangePeriodFromISR(TimerHandle_t xTimer, TickType_t xNewPeriod, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xTimerResetFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken);

void *pvTimerGetTimerID(TimerHandle_t xTimer);
void vTimerSetTimerID(TimerHandle_t xTimer, void *pvNewID);

#endif /* TIMERS_H */
