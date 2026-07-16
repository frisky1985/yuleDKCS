/**
 * @file Dcm_Cfg_Dk.h
 * @brief yuleDKCS DCM Configuration Overrides — UDS Diagnostic Services
 *
 * Defines missing AUTOSAR DCM configuration macros and yuleDKCS-specific
 * UDS DID/RID definitions for the ICCE Digital Key ECU.
 */

#ifndef DCM_CFG_DK_H
#define DCM_CFG_DK_H

#include "Dcm_Cfg.h"   /* yuleASR base Dcm_Cfg.h */

/* =========================================================================
 * Fix: Dcm.c uses DCM_SECURITY_DELAY_TIME (not DCM_SECURITY_DELAY_TIME_MS)
 * ========================================================================= */
#ifndef DCM_SECURITY_DELAY_TIME
#define DCM_SECURITY_DELAY_TIME         DCM_SECURITY_DELAY_TIME_MS

/* =========================================================================
 * Fix: Dcm.h alias references DCM_SID_DEINIT which is not defined
 * ========================================================================= */
#ifndef DCM_SID_DEINIT
#define DCM_SID_DEINIT                   (0x06U)
#endif

#endif

/* =========================================================================
 * yuleDKCS UDS DID Definitions
 *
 * Per ICCE Digital Key + CCC + ICCOA diagnostic requirements:
 *
 * ┌────────────┬──────────┬──────────────────────────────────────────┐
 * │ DID        │ Bytes    │ Description                              │
 * ├────────────┼──────────┼──────────────────────────────────────────┤
 * │ 0xF180     │ 20       │ ECU Identification (VIN/SN/HW/SW)       │
 * │ 0xF190     │ 64       │ Digital Key Status (BLE/UWB/NFC states) │
 * │ 0xF191     │ 32       │ Digital Key Configuration               │
 * │ 0xF192     │ 128      │ Digital Key Certificate Chain           │
 * │ 0xF193     │ 8        │ Digital Key Session Info                │
 * │ 0xF194     │ 4        │ UWB Ranging Data                        │
 * │ 0xF195     │ 4        │ BLE Connection Metrics                  │
 * │ 0xF190     │ 2        │ Vehicle Lock State (read-only)          │
 * │ 0x0100     │ 8        │ Manufacturer ECU Data                   │
 * │ 0x0101     │ 4        │ Bootloader Software Version             │
 * │ 0x0102     │ 4        │ Application Software Version            │
 * │ 0x0103     │ 4        │ Hardware Version                        │
 * │ 0xF100     │ 4        │ Odometer (from vehicle CAN)             │
 * │ 0xF101     │ 1        │ Fuel / SOC Level                        │
 * │ 0xF400     │ 4        │ Timestamp (extended data record)        │
 * │ 0xF401     │ 8        │ DTC Environmental Data                  │
 * │ 0xF402     │ 4        │ Operation Cycle Counter                 │
 * └────────────┴──────────┴──────────────────────────────────────────┘
 * ========================================================================= */

/* ---- yuleDKCS DID Definitions ---- */
#define DK_DID_ECU_IDENTIFICATION       (0xF180U)
#define DK_DID_DK_STATUS                (0xF190U)
#define DK_DID_DK_CONFIG                (0xF191U)
#define DK_DID_DK_CERT_CHAIN            (0xF192U)
#define DK_DID_DK_SESSION               (0xF193U)
#define DK_DID_DK_UWB_RANGING           (0xF194U)
#define DK_DID_DK_BLE_METRICS           (0xF195U)
#define DK_DID_VEHICLE_LOCK_STATE       (0xF1A0U)
#define DK_DID_MFR_ECU_DATA             (0x0100U)
#define DK_DID_BOOT_SW_VER              (0x0101U)
#define DK_DID_APP_SW_VER               (0x0102U)
#define DK_DID_HW_VER                   (0x0103U)
#define DK_DID_ODOMETER                 (0xF100U)
#define DK_DID_SOC                      (0xF101U)

/* ---- DID Data Lengths ---- */
#define DK_DID_LEN_ECU_ID               (20U)
#define DK_DID_LEN_DK_STATUS            (64U)
#define DK_DID_LEN_DK_CONFIG            (32U)
#define DK_DID_LEN_DK_CERT_CHAIN        (128U)
#define DK_DID_LEN_DK_SESSION           (8U)
#define DK_DID_LEN_DK_UWB               (4U)
#define DK_DID_LEN_DK_BLE               (4U)
#define DK_DID_LEN_LOCK_STATE           (2U)
#define DK_DID_LEN_MFR_ECU              (8U)
#define DK_DID_LEN_BOOT_SW              (4U)
#define DK_DID_LEN_APP_SW               (4U)
#define DK_DID_LEN_HW                   (4U)
#define DK_DID_LEN_ODOMETER             (4U)
#define DK_DID_LEN_SOC                  (1U)

/* ---- yuleDKCS RID Definitions ---- */
#define DK_RID_ECU_RESET                (0x0001U)
#define DK_RID_SECURITY_SEED            (0x0002U)
#define DK_RID_DK_DIAG_SELF_TEST        (0x0100U)
#define DK_RID_DK_FACTORY_RESET         (0x0101U)

/* ---- DID Session/Security Access Permissions ---- */
#define DK_DID_PERM_READ_ONLY           (0x00U)  /* Read-only: secLevel=0 */
#define DK_DID_PERM_SECURE_READ         (0x01U)  /* Requires unlock level 1 */
#define DK_DID_PERM_SECURE_WRITE        (0x01U)  /* Write requires unlock level 1 */
#define DK_DID_PERM_FACTORY_WRITE       (0x02U)  /* Write requires unlock level 2 */

#endif /* DCM_CFG_DK_H */
