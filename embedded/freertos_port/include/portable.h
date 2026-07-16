/**
 * @file portable.h
 * @brief FreeRTOS Portable Layer Header
 */

#ifndef PORTABLE_H
#define PORTABLE_H

#include "portmacro.h"

#define portBYTE_ALIGNMENT          8
#define portBYTE_ALIGNMENT_MASK     (portBYTE_ALIGNMENT - 1)

void *pvPortMalloc(size_t xSize);
void vPortFree(void *pv);
void vPortInitializeHeap(void);

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack,
                                    TaskFunction_t pxCode,
                                    void *pvParameters);

BaseType_t xPortStartScheduler(void);
void vPortEndScheduler(void);
void vPortSetupTimerInterrupt(void);

/* Memory allocation schemes (heap_1..heap_5) — using heap_4 */
uint8_t *pucPortHeapGetAddress(void);

#endif /* PORTABLE_H */
