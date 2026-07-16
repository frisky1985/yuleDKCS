/**
 * @file projdefs.h
 * @brief FreeRTOS Project Definitions
 */

#ifndef PROJDEFS_H
#define PROJDEFS_H

typedef void (*TaskFunction_t)(void *);

#define pdTRUE          ((BaseType_t)1)
#define pdFALSE         ((BaseType_t)0)
#define pdPASS          (pdTRUE)
#define pdFAIL          (pdFALSE)
#define pdMS_TO_TICKS(xTimeInMs)    ((TickType_t)((xTimeInMs) / portTICK_PERIOD_MS))

#define errQUEUE_EMPTY  ((BaseType_t)0)
#define errQUEUE_FULL   ((BaseType_t)0)

#define tskIDLE_PRIORITY    ((UBaseType_t)0)

/* 已在 FreeRTOSConfig.h 中定义 */
#ifndef configMINIMAL_STACK_SIZE
#define configMINIMAL_STACK_SIZE    256
#endif

#endif /* PROJDEFS_H */
