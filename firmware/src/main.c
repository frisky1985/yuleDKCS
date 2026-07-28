/*
 * main.c — yuleDKCS Firmware Entry Point
 * Target: NXP KW47A
 *
 * Initializes hardware, security element, protocol stacks,
 * then enters the main event loop.
 */

#include "hal_interface.h"
#include "kw47a_board.h"

/* Protocol stack entry points */
extern int ccc_dk_init(void);
extern int icce_dk_init(void);
extern int iccoa_dk_init(void);

/* RTOS task handles (when USE_FREERTOS is enabled) */
#if defined(USE_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"

static void ble_task(void *param);
static void uwb_task(void *param);
static void protocol_task(void *param);
#else
/* Bare-metal event loop */
static void event_loop_poll(void);
#endif

/* ── System Initialization ───────────────────────────────── */
static int system_init(void)
{
    kw47a_board_init();
    kw47a_clock_init();
    kw47a_pinmux_init();
    kw47a_uart_init(115200);

    hal_debug_printf("\n\n=== yuleDKCS Firmware v" FW_VERSION " ===\n");
    hal_debug_printf("Target: NXP KW47A\n");

    if (hal_se050_init() != 0) {
        hal_debug_printf("FATAL: SE050 init failed\n");
        return -1;
    }
    hal_debug_printf("SE050 secure element ready\n");

    if (hal_ble_init() != 0) {
        hal_debug_printf("FATAL: BLE init failed\n");
        return -1;
    }

    if (hal_storage_init() != 0) {
        hal_debug_printf("WARN: Storage init failed\n");
    }

    return 0;
}

/* ── Protocol Stack Initialization ───────────────────────── */
static int protocol_init(void)
{
#if defined(BUILD_CCC)
    if (ccc_dk_init() != 0) {
        hal_debug_printf("WARN: CCC init failed\n");
    } else {
        hal_debug_printf("CCC protocol stack ready\n");
    }
#endif
#if defined(BUILD_ICCE)
    if (icce_dk_init() != 0) {
        hal_debug_printf("WARN: ICCE init failed\n");
    } else {
        hal_debug_printf("ICCE protocol stack ready\n");
    }
#endif
#if defined(BUILD_ICCOA)
    if (iccoa_dk_init() != 0) {
        hal_debug_printf("WARN: ICCOA init failed\n");
    } else {
        hal_debug_printf("ICCOA protocol stack ready\n");
    }
#endif
    return 0;
}

/* ── Main Entry Point ────────────────────────────────────── */
int main(void)
{
    if (system_init() != 0) {
        /* Blink error LED */
        while (1);
    }

    protocol_init();

    hal_debug_printf("yuleDKCS ready — awaiting BLE connection\n");

#if defined(USE_FREERTOS)
    xTaskCreate(ble_task, "BLE", 1024, NULL, 5, NULL);
    xTaskCreate(uwb_task, "UWB", 1024, NULL, 4, NULL);
    xTaskCreate(protocol_task, "PROTO", 2048, NULL, 3, NULL);
    vTaskStartScheduler();
#else
    while (1) {
        event_loop_poll();
    }
#endif

    return 0;
}

#if !defined(USE_FREERTOS)
static void event_loop_poll(void)
{
    /* Poll NFC for card presence */
    uint8_t nfc_buf[256];
    uint16_t nfc_len = 0;
    if (hal_nfc_poll(nfc_buf, &nfc_len, 100) == 0 && nfc_len > 0) {
        ccc_dk_core_process_nfc(nfc_buf, nfc_len);
    }

    /* Poll BLE for events */
    ble_adapter_poll();

    /* Sleep until next event */
    hal_delay_ms(10);
}
#endif
