/**
 * @file semphr.h
 * @brief FreeRTOS Semaphore API (stub for compilation)
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "queue.h"

typedef QueueHandle_t SemaphoreHandle_t;

#define semBINARY_SEMAPHORE_QUEUE_LENGTH   ((uint8_t)1)
#define semSEMAPHORE_QUEUE_ITEM_LENGTH     ((uint8_t)0)

SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex);

#define xSemaphoreCreateBinaryStatic(xStatic)   xSemaphoreCreateBinary()

#endif /* SEMAPHORE_H */
