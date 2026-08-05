/**
 * @file queue.h
 * @brief FreeRTOS Queue API (stub for compilation)
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "FreeRTOS.h"
#include "list.h"

struct QueueDefinition;
typedef struct QueueDefinition *QueueHandle_t;
typedef QueueHandle_t QueueSetHandle_t;
typedef QueueHandle_t QueueSetMemberHandle_t;

#define queueQUEUE_TYPE_BASE               0
#define queueQUEUE_TYPE_SET                1
#define queueQUEUE_TYPE_MUTEX              2
#define queueQUEUE_TYPE_COUNTING_SEMAPHORE 3
#define queueQUEUE_TYPE_BINARY_SEMAPHORE   4
#define queueQUEUE_TYPE_RECURSIVE_MUTEX    5

#define queueSEND_TO_BACK                 0
#define queueSEND_TO_FRONT                1
#define queueOVERWRITE                    2

BaseType_t xQueueGenericSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait, BaseType_t xCopyPosition);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueuePeek(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);
UBaseType_t uxQueueSpacesAvailable(QueueHandle_t xQueue);
BaseType_t xQueueGenericSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken, BaseType_t xCopyPosition);
BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxHigherPriorityTaskWoken);
QueueHandle_t xQueueGenericCreate(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType);
BaseType_t xQueueIsQueueEmptyFromISR(const QueueHandle_t xQueue);
BaseType_t xQueueIsQueueFullFromISR(const QueueHandle_t xQueue);

#define xQueueCreate(uxQueueLength, uxItemSize)  xQueueGenericCreate((uxQueueLength), (uxItemSize), queueQUEUE_TYPE_BASE)
#define xQueueSend(xQueue, pvItemToQueue, xTicksToWait)  xQueueGenericSend((xQueue), (pvItemToQueue), (xTicksToWait), queueSEND_TO_BACK)
#define xQueueSendFromISR(xQueue, pvItemToQueue, pxHigherPriorityTaskWoken)  xQueueGenericSendFromISR((xQueue), (pvItemToQueue), (pxHigherPriorityTaskWoken), queueSEND_TO_BACK)
#define xQueueReset(xQueue)                         xQueueGenericReset(xQueue, pdFALSE)
BaseType_t xQueueGenericReset(QueueHandle_t xQueue, BaseType_t xNewQueue);

#endif /* QUEUE_H */
