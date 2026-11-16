/*
 * main.c — yuleDKCS Firmware Entry Point
 * Target: NXP KW47A
 *
 * Initializes hardware, security element, protocol stacks,
 * then enters the main event loop.
 */

#include "hal_interface.h"
#include "kw47a_board.h"
#include "kw47a_nv.h"
#include "bringup_selftest.h"
#include "ccc_digital_key.h"

#if defined(SECURE_BOOT)
#include "secure_boot.h"
#endif

/* Protocol stack entry points */
extern int ccc_dk_init(void);
extern int icce_dk_init(void);
extern int iccoa_dk_init(void);

/* Returned by system_init() when the secure-boot gate fails. */
#define INIT_ERR_SECURE_BOOT (-2)

#if defined(SECURE_BOOT)
/*
 * Secure-boot integration hooks. An assembly/provisioning step provides these
 * symbols (typically a generated manifest object plus the boot partition and
 * the trusted public key). They are weak so host builds still link and run.
 */
extern const uint8_t  sb_embedded_manifest[]   __attribute__((weak));
extern const uint32_t sb_embedded_manifest_len __attribute__((weak));
extern const uint8_t  sb_embedded_image[]      __attribute__((weak));
extern const uint32_t sb_embedded_image_len    __attribute__((weak));
extern const uint8_t  sb_embedded_pubkey[64]   __attribute__((weak));
extern const uint8_t  sb_embedded_pubkey_type  __attribute__((weak));

static int secure_boot_precheck(void)
{
    uint32_t image_len;
    sb_verify_result_t result;
    int rc;

    /* No manifest linked in: development / host-sim image, skip the gate. */
    if ((uintptr_t)(const void *)sb_embedded_manifest == (uintptr_t)0) {
#if defined(YULE_PRODUCTION_BUILD)
        /* 量产镜像必须带签名清单，缺清单即拒绝启动。 */
        hal_debug_printf("SECURE_BOOT: no manifest (FATAL)\n");
        return -1;
#else
        hal_debug_printf("SECURE_BOOT: no manifest (SKIP)\n");
        return 0;
#endif
    }

    /* A manifest without its image or trust anchor cannot be verified. */
    if (((uintptr_t)(const void *)&sb_embedded_manifest_len == (uintptr_t)0) ||
        ((uintptr_t)(const void *)sb_embedded_image == (uintptr_t)0) ||
        ((uintptr_t)(const void *)&sb_embedded_image_len == (uintptr_t)0) ||
        ((uintptr_t)(const void *)sb_embedded_pubkey == (uintptr_t)0) ||
        ((uintptr_t)(const void *)&sb_embedded_pubkey_type == (uintptr_t)0)) {
        hal_debug_printf("SECURE_BOOT: incomplete manifest bundle\n");
        return -1;
    }

    image_len = sb_embedded_image_len;
    rc = sb_verify_image(sb_embedded_image, (size_t)image_len,
                         sb_embedded_manifest, (size_t)sb_embedded_manifest_len,
                         sb_embedded_pubkey, sb_embedded_pubkey_type,
                         0U, &result);
    if (rc != SB_OK) {
        hal_debug_printf("SECURE_BOOT: FAIL (err=%d)\n", rc);
        return -1;
    }

    hal_debug_printf("SECURE_BOOT: OK (image_version=%u, counter=%u)\n",
                     (unsigned)result.image_version,
                     (unsigned)result.anti_rollback);
    return 0;
}
#endif /* SECURE_BOOT */

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

#if defined(SECURE_BOOT)
    if (secure_boot_precheck() != 0) {
        hal_debug_printf("FATAL: secure boot verification failed — halted\n");
        return INIT_ERR_SECURE_BOOT;
    }
#endif

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
#if defined(YULE_PRODUCTION_BUILD)
        hal_debug_printf("FATAL: production requires persistent storage\n");
        return -1;
#endif
    }

    /* 绑定板级 NV Flash/OTP；失败则保持未绑定并降级为纯 RAM. */
    if (kw47a_nv_init() != 0) {
        hal_debug_printf("WARN: NV persistence not bound (RAM-only)\n");
#if defined(YULE_PRODUCTION_BUILD)
        /* 无持久化 = 锁定/CRL/密钥水位无法跨复位保持, 量产必须拒绝启动. */
        hal_debug_printf("FATAL: production requires bound NV persistence\n");
        return -1;
#endif
    }

    /* 从 Flash/OTP 恢复产线锁定与 CRL；未绑定 NV 时降级为纯 RAM. */
    {
        ccc_status_t st = sec_security_init();

        if (st != CCC_OK) {
            hal_debug_printf("WARN: security NV restore failed (%d)\n", (int)st);
#if defined(YULE_PRODUCTION_BUILD)
            /* 恢复失败意味着锁定/CRL 状态不可信, 必须 fail-closed. */
            hal_debug_printf("FATAL: production security state unavailable\n");
            return -1;
#endif
        }
        if (sec_provisioning_locked()) {
            hal_debug_printf("SECURITY: provisioning locked\n");
        }
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
    int init_rc = system_init();

    if (init_rc != 0) {
#if defined(FIRMWARE_HOST_SIM)
        /* Host simulation: return an error instead of spinning forever so CI
         * can detect a failed secure-boot gate. */
        return (init_rc == INIT_ERR_SECURE_BOOT) ? 2 : 1;
#else
        /* Target: unrecoverable error — remain in a locked halt loop. */
        while (1);
#endif
    }

    protocol_init();

#if defined(BRINGUP_SELFTEST)
    (void)bringup_selftest_run(NULL);
#endif

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
    (void)hal_nfc_poll(nfc_buf, &nfc_len, 100);

    /* Sleep until next event */
    hal_delay_ms(10);
}
#endif
