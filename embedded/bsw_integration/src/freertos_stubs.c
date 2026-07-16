/**
 * @file freertos_stubs.c
 * @brief Minimal FreeRTOS API stubs — enough for yuleASR OS to link
 */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS globals needed by port.c */
void *pxCurrentTCB = 0;
BaseType_t xPortSwitchRequired = pdFALSE;
void vTaskSwitchContext(void) { }
void vTaskIncrementTick(void) { }

/* Task API stubs */
BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char* pcName,
                        unsigned long usStackDepth, void* pvParameters,
                        UBaseType_t uxPriority, TaskHandle_t* pxCreatedTask) {
    (void)pvTaskCode; (void)pcName; (void)usStackDepth;
    (void)pvParameters; (void)uxPriority; (void)pxCreatedTask;
    return pdPASS;
}
void vTaskDelete(TaskHandle_t xTask) { (void)xTask; }
void vTaskDelay(const TickType_t xTicksToDelay) { (void)xTicksToDelay; }
void vTaskSuspend(TaskHandle_t xTaskToSuspend) { (void)xTaskToSuspend; }
void vTaskResume(TaskHandle_t xTaskToResume) { (void)xTaskToResume; }
BaseType_t xTaskResumeFromISR(TaskHandle_t xTaskToResume) { (void)xTaskToResume; return pdFALSE; }
UBaseType_t uxTaskPriorityGet(const TaskHandle_t xTask) { (void)xTask; return 0; }
void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority) { (void)xTask; (void)uxNewPriority; }
void taskYIELD(void) {}
BaseType_t xTaskNotifyWait(unsigned long a, unsigned long b, unsigned long* c, TickType_t d) { (void)a;(void)b;(void)c;(void)d; return pdFALSE; }
BaseType_t xTaskNotify(TaskHandle_t a, unsigned long b, eNotifyAction c) { (void)a;(void)b;(void)c; return pdFALSE; }
BaseType_t xTaskNotifyFromISR(TaskHandle_t a, unsigned long b, eNotifyAction c, BaseType_t* d) { (void)a;(void)b;(void)c;(void)d; return pdFALSE; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (TaskHandle_t)0; }
eTaskState eTaskGetState(TaskHandle_t xTask) { (void)xTask; return eReady; }
TickType_t xTaskGetTickCount(void) { return 0; }

/* Scheduler */
void vTaskStartScheduler(void) {}
void vTaskEndScheduler(void) {}

/* Timer API */
void *pvTimerGetTimerID(TimerHandle_t xTimer) { (void)xTimer; return 0; }
TimerHandle_t xTimerCreate(const char *a, TickType_t b, UBaseType_t c, void *d, TimerCallbackFunction_t e) { (void)a;(void)b;(void)c;(void)d;(void)e; return (TimerHandle_t)1; }
BaseType_t xTimerStart(TimerHandle_t xTimer, TickType_t xTicksToWait) { (void)xTimer;(void)xTicksToWait; return pdPASS; }
BaseType_t xTimerStop(TimerHandle_t xTimer, TickType_t xTicksToWait) { (void)xTimer;(void)xTicksToWait; return pdPASS; }
BaseType_t xTimerChangePeriod(TimerHandle_t xTimer, TickType_t xNewPeriod, TickType_t xTicksToWait) { (void)xTimer;(void)xNewPeriod;(void)xTicksToWait; return pdPASS; }

/* Event Groups */
EventGroupHandle_t xEventGroupCreate(void) { return (EventGroupHandle_t)1; }
EventBits_t xEventGroupWaitBits(EventGroupHandle_t a, EventBits_t b, BaseType_t c, BaseType_t d, TickType_t e) { (void)a;(void)b;(void)c;(void)d;(void)e; return 0; }
EventBits_t xEventGroupSetBits(EventGroupHandle_t a, EventBits_t b) { (void)a;(void)b; return 0; }
EventBits_t xEventGroupClearBits(EventGroupHandle_t a, EventBits_t b) { (void)a;(void)b; return 0; }
EventBits_t xEventGroupGetBits(EventGroupHandle_t a) { (void)a; return 0; }

/* Semaphores */
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t a, TickType_t b) { (void)a;(void)b; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t a) { (void)a; return pdTRUE; }
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t a, BaseType_t *b) { (void)a;(void)b; return pdTRUE; }

/* Memory */
__attribute__((weak)) void *pvPortMalloc(size_t xWantedSize) { (void)xWantedSize; return 0; }
__attribute__((weak)) void vPortFree(void *pv) { (void)pv; }

/* ICCE Protocol stubs */
void icce_dk_init(void) {}
void icce_dk_early_init(void) {}
void icce_dk_late_init(void) {}
void icce_dk_on_wakeup(uint32_t wakeup_source) { (void)wakeup_source; }
void icce_dk_on_sleep(void) {}
void icce_ble_init(void) {}
void icce_ble_deinit(void) {}
void icce_ble_start_adv(void) {}
void icce_ble_stop_adv(void) {}
void icce_uwb_init(void) {}
void icce_uwb_deinit(void) {}

/* WdgIf */
Std_ReturnType WdgIf_Trigger(uint8 Device) { (void)Device; return E_OK; }
