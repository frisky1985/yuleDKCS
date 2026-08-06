/**
 * @file dk_hal.h
 * @brief yuleDKCS 车端 HAL 接口契约 (ASPICE SWE.2.BP2 evidence)
 *
 * 统一 HAL (hal_ble/hal_uwb/hal_nfc/hal_sec) 函数签名与参数范围。
 * 引用: docs/architecture.md §4.4, docs/aspice/SWE.4-software-arch.md §2.3
 * 需求: REQ-028 ~ REQ-035
 */
#ifndef YULEDKCS_DK_HAL_H
#define YULEDKCS_DK_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 通用返回码 ─────────────────────────────────────────────────── */
#define DK_HAL_OK         0
#define DK_HAL_ERR        (-1)
#define DK_HAL_ERR_TIMEOUT (-2)

/* ── BLE (NXP KW47A) — REQ-029 ──────────────────────────────────── */
#define DK_BLE_GATT_CCC_DK_SERVICE_UUID  0xFFD1u   /* CCC DK Service */

/* ── UWB (NXP NCJ29D6) — REQ-030 ────────────────────────────────── */
/* 距离分区: LOCKED / APPROACH / UNLOCK / ENTRY / INSIDE */
enum dk_uwb_zone {
    DK_UWB_ZONE_LOCKED   = 0,   /* > 5m, 锁定 */
    DK_UWB_ZONE_APPROACH = 1,   /* 2~5m, 接近 */
    DK_UWB_ZONE_UNLOCK   = 2,   /* 0.8~2m, 解锁区 */
    DK_UWB_ZONE_ENTRY    = 3,   /* 车内边缘 */
    DK_UWB_ZONE_INSIDE   = 4    /* 车内 */
};
#define DK_UWB_ZONE_COUNT  5

/* ── NFC (ST25R501) — REQ-028 ───────────────────────────────────── */
#define DK_NFC_FREQ_MHZ      13.56f
#define DK_NFC_APDU_MAX_LEN  256u   /* ISO 7816-4 APDU 最大长度 */

/* ── 电源状态 — REQ-034 ─────────────────────────────────────────── */
enum dk_power_state {
    DK_POWER_ACTIVE     = 0,   /* < 15mA */
    DK_POWER_IDLE       = 1,
    DK_POWER_SLEEP      = 2,   /* < 100μA */
    DK_POWER_DEEPSLEEP  = 3,   /* < 10μA */
    DK_POWER_POWEROFF   = 4
};
#define DK_POWER_STATE_COUNT   5
#define DK_POWER_WAKE_NFC_MS   50u    /* NFC 场唤醒 < 50ms */
#define DK_POWER_WAKE_BLE_MS   100u   /* BLE 广播匹配唤醒 < 100ms */

/* ── 安全启动 — REQ-035 ─────────────────────────────────────────── */
enum dk_boot_stage {
    DK_BOOT_BL      = 0,   /* Bootloader */
    DK_BOOT_OS      = 1,
    DK_BOOT_APP     = 2,
    DK_BOOT_COMPLETE = 3
};
#define DK_BOOT_STAGE_COUNT  4

/* ── HAL 函数契约（与 embedded/unified_hal/ 对齐） ───────────────── */
/* 返回 DK_HAL_OK 或负错误码；所有 buffer 参数非 NULL */
typedef int (*dk_hal_ble_send_fn)(const uint8_t *data, uint32_t len, uint32_t timeout_ms);
typedef int (*dk_hal_uwb_range_fn)(uint8_t *zone_out, uint16_t *distance_cm);
typedef int (*dk_hal_nfc_exchange_fn)(const uint8_t *apdu, uint32_t apdu_len,
                                      uint8_t *resp, uint32_t *resp_len);
typedef int (*dk_hal_sec_sign_fn)(const uint8_t *digest, uint32_t digest_len,
                                  uint8_t *sig, uint32_t *sig_len);

/* ── SE050 安全芯片 — REQ-033 ───────────────────────────────────── */
#define DK_SE050_SCP03_CHANNELS  3u     /* SCP03 通道数 */
#define DK_SE050_SIG_ECDSA_LEN   64u    /* ECDSA P-256 签名 64B */

#ifdef __cplusplus
}
#endif

#endif /* YULEDKCS_DK_HAL_H */
