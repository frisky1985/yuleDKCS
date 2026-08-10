/**
 * freertos_stubs.c — FreeRTOS heap stubs for host-based unit tests
 *
 * key_mgmt.c (and friends) use pvPortMalloc / vPortFree from FreeRTOS heap
 * management. On the host test harness we back them with libc malloc/free so
 * the real protocol sources can link and execute. Counters let tests assert
 * on alloc/free balance (leak checks).
 */
#include <stddef.h>
#include <stdlib.h>

static unsigned long g_malloc_count = 0;
static unsigned long g_free_count = 0;

void *pvPortMalloc(size_t xSize)
{
    void *p = malloc(xSize ? xSize : 1);
    if (p != NULL) {
        g_malloc_count++;
    }
    return p;
}

void vPortFree(void *pv)
{
    if (pv != NULL) {
        g_free_count++;
    }
    free(pv);
}

/* ---- test hooks ---- */
unsigned long freertos_stubs_malloc_count(void) { return g_malloc_count; }
unsigned long freertos_stubs_free_count(void)   { return g_free_count; }
void freertos_stubs_reset_counters(void)        { g_malloc_count = 0; g_free_count = 0; }
