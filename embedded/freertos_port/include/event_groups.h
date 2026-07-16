/**
 * @file event_groups.h
 * @brief FreeRTOS Event Groups API (stub for compilation)
 */

#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

#include "FreeRTOS.h"
#include "list.h"

struct EventGroupDef_t;
typedef struct EventGroupDef_t *EventGroupHandle_t;

#define eventCLEAR_ALL_BITS             0x00
#define eventWAIT_FOR_ALL_BITS          0x01
#define eventWAIT_FOR_ANY_BITS          0x00
#define eventNO_WAIT                    0x00

typedef TickType_t EventBits_t;

EventGroupHandle_t xEventGroupCreate(void);
EventGroupHandle_t xEventGroupCreateStatic(void *pxEventGroupBuffer);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToWaitFor, BaseType_t xClearOnExit, BaseType_t xWaitForAllBits, TickType_t xTicksToWait);
EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToSet);
EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToClear);
EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup);
EventBits_t xEventGroupSetBitsFromISR(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken);
void vEventGroupDelete(EventGroupHandle_t xEventGroup);

#define xEventGroupClearBitsFromISR(xEventGroup, uxBitsToClear)  xEventGroupClearBits(xEventGroup, uxBitsToClear)

#endif /* EVENT_GROUPS_H */
