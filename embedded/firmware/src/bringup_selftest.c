/**
 * @file bringup_selftest.c
 * @brief G1 hardware bring-up self-test (HAL-level only).
 *
 * Deliberately uses only hal_interface.h so the same binary/check sequence
 * runs on the EVB and in host simulation (hardware checks become SKIP).
 */

#include "bringup_selftest.h"
#include "hal_interface.h"

#if defined(SECURE_BOOT)
#include "secure_boot.h"
#if defined(BUILD_CCC)
#include "crypto_random.h"
#endif
#endif

#include <string.h>

enum { BK_PASS = 0, BK_FAIL = 1, BK_SKIP = 2 };

static bringup_result_t g_res;

static void report(uint8_t id, const char *name, int status, const char *detail)
{
    static const char *tag[] = { "PASS", "FAIL", "SKIP" };

    if (status == BK_PASS)       g_res.passed++;
    else if (status == BK_FAIL)  g_res.failed++;
    else                         g_res.skipped++;

    if (detail != NULL) {
        hal_debug_printf("[BRINGUP] %02u %-18s %s (%s)\n", id, name, tag[status], detail);
    } else {
        hal_debug_printf("[BRINGUP] %02u %-18s %s\n", id, name, tag[status]);
    }
}

/* 01 — SysTick/time base advances */
static void check_time(void)
{
    uint32_t t0 = hal_time_ms();
    hal_delay_ms(5);
    uint32_t t1 = hal_time_ms();

    report(1, "time-base", (t1 > t0) ? BK_PASS : BK_FAIL, NULL);
}

/* 02 — Persistent storage round-trip */
static void check_storage(void)
{
    static const uint8_t  pattern[4] = { 0xA5, 0x5A, 0x01, 0xFE };
    uint8_t  readback[4] = { 0 };
    uint16_t len = sizeof(readback);
    int      ok;

    ok = (hal_storage_init() == 0) &&
         (hal_storage_write("bk", pattern, sizeof(pattern)) == 0) &&
         (hal_storage_read("bk", readback, &len) == 0) &&
         (len == sizeof(pattern)) &&
         (memcmp(pattern, readback, sizeof(pattern)) == 0);

    (void)hal_storage_delete("bk");
    report(2, "storage", ok ? BK_PASS : BK_FAIL, NULL);
}

/* 03 — SE050 secure element reachable (T=1' + SCP03 ready) */
static void check_se050(void)
{
#ifdef FIRMWARE_HOST_SIM
    report(3, "se050-init", BK_SKIP, "host sim");
#else
    report(3, "se050-init", (hal_se050_init() == 0) ? BK_PASS : BK_FAIL, NULL);
#endif
}

/* 04 — SE050 TRNG produces non-constant output */
static void check_entropy(void)
{
#ifdef FIRMWARE_HOST_SIM
    report(4, "se050-trng", BK_SKIP, "host sim");
#else
    uint8_t rnd[16];
    uint8_t nonzero = 0;
    uint8_t i;

    memset(rnd, 0, sizeof(rnd));
    if (hal_se050_generate_random(rnd, sizeof(rnd)) != 0) {
        report(4, "se050-trng", BK_FAIL, "read error");
        return;
    }
    for (i = 0; i < sizeof(rnd); i++) {
        if (rnd[i] != 0U) nonzero = 1;
    }
    report(4, "se050-trng", nonzero ? BK_PASS : BK_FAIL, NULL);
#endif
}

/* 05 — BLE controller start + advertising */
static void check_ble(void)
{
#ifdef FIRMWARE_HOST_SIM
    report(5, "ble", BK_SKIP, "host sim");
#else
    static const uint8_t adv[] = { 0x02, 0x01, 0x06 };
    int ok = (hal_ble_init() == 0) &&
             (hal_ble_advertise_start(adv, sizeof(adv)) == 0);

    report(5, "ble", ok ? BK_PASS : BK_FAIL, NULL);
#endif
}

/* 06 — NFC reader (ST25R501) initialises */
static void check_nfc(void)
{
#ifdef FIRMWARE_HOST_SIM
    report(6, "nfc", BK_SKIP, "host sim");
#else
    report(6, "nfc", (hal_nfc_init() == 0) ? BK_PASS : BK_FAIL, NULL);
#endif
}

/* 07 — UWB ranging engine start/stop */
static void check_uwb(void)
{
#ifdef FIRMWARE_HOST_SIM
    report(7, "uwb", BK_SKIP, "host sim");
#else
    int ok = (hal_uwb_init() == 0) &&
             (hal_uwb_start_ranging(1) == 0) &&
             (hal_uwb_stop_ranging() == 0);

    report(7, "uwb", ok ? BK_PASS : BK_FAIL, NULL);
#endif
}

/* 08 — Secure-boot KAT (sign -> verify -> tamper -> reject) */
#if defined(SECURE_BOOT)
/* KAT vector mirrors tests/support/cert_fixtures.h k_device_priv/k_device_pub
 * (P-256, test-only key). */
static const uint8_t k_sb_kat_priv[32] = {
    0xAC,0x1C,0xB5,0xD8,0xAE,0xD2,0x55,0x21,0x09,0xF2,0x28,0xB0,
    0xD0,0x6B,0xA8,0x39,0x60,0xE2,0x5E,0xBE,0x14,0x52,0xB4,0x6B,
    0xCD,0x0D,0xC9,0xD8,0x8E,0xB5,0x1A,0xD6,
};
static const uint8_t k_sb_kat_pub[64] = {
    0xF0,0x6E,0xC6,0x11,0x20,0xDD,0xC9,0xB0,0xEF,0xF2,0x6E,0x26,
    0x07,0xEF,0x40,0x45,0x09,0x4D,0x7C,0x73,0xE4,0x90,0xFE,0xC5,
    0x1D,0x8B,0xDC,0x28,0x18,0x44,0x4D,0x7D,0x29,0x5F,0xEC,0xF7,
    0x51,0x53,0x58,0xCA,0x86,0xB4,0x2D,0xD0,0xE6,0x0F,0x91,0xFE,
    0x02,0xC6,0x2D,0xA7,0x95,0x80,0xBA,0x3C,0x6B,0xFB,0x4B,0x15,
    0x80,0xFD,0x58,0x0B,
};

static void check_secure_boot(void)
{
    static const uint8_t image[] = "YULE-SECURE-BOOT-KAT";
    static const uint8_t key_id[SB_KEY_ID_SIZE] = {
        0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
        0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x01,
    };
    uint8_t  manifest[SB_MANIFEST_SIZE];
    size_t   manifest_len = 0;
    uint8_t  tampered[sizeof(image)];
    sb_verify_result_t result;
    int ok;

#if defined(BUILD_CCC)
    (void)crypto_random_init();
#endif

    ok = (sb_manifest_sign(k_sb_kat_priv, SB_KEY_TYPE_P256, 7U, 5U,
                           image, sizeof(image), key_id,
                           manifest, sizeof(manifest), &manifest_len) == SB_OK) &&
         (manifest_len == SB_MANIFEST_SIZE) &&
         (sb_verify_image(image, sizeof(image),
                          manifest, manifest_len,
                          k_sb_kat_pub, SB_KEY_TYPE_P256,
                          5U, &result) == SB_OK);

    if (ok) {
        (void)memcpy(tampered, image, sizeof(image));
        tampered[0] ^= 0x01U;
        ok = (sb_verify_image(tampered, sizeof(tampered),
                              manifest, manifest_len,
                              k_sb_kat_pub, SB_KEY_TYPE_P256,
                              5U, &result) == SB_ERR_BAD_HASH);
    }

    report(8, "secure-boot", ok ? BK_PASS : BK_FAIL, NULL);
}
#endif /* SECURE_BOOT */

int bringup_selftest_run(bringup_result_t *result)
{
    memset(&g_res, 0, sizeof(g_res));

    hal_debug_printf("\n[BRINGUP] === G1 hardware self-test ===\n");

    check_time();
    check_storage();
    check_se050();
    check_entropy();
    check_ble();
    check_nfc();
    check_uwb();
#if defined(SECURE_BOOT)
    check_secure_boot();
#endif

    hal_debug_printf("[BRINGUP] RESULT: %u passed, %u failed, %u skipped\n",
                     (unsigned)g_res.passed, (unsigned)g_res.failed,
                     (unsigned)g_res.skipped);

    if (result != NULL) *result = g_res;

    return (g_res.failed == 0U) ? 0 : -1;
}
