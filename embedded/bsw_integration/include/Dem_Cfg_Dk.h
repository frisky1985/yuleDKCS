/**
 * @file Dem_Cfg_Dk.h
 * @brief yuleDKCS DEM Configuration Overrides — DTC Event Definitions
 *
 * Defines yuleDKCS-specific diagnostic events and DTC mappings for the
 * ICCE Digital Key ECU.
 */

#ifndef DEM_CFG_DK_H
#define DEM_CFG_DK_H

#include "Dem_Cfg.h"   /* yuleASR base Dem_Cfg.h */

/* =========================================================================
 * yuleDKCS DTC / Event Definitions
 *
 * DTC Format: 0xUUVVFF
 *   UU = System (0x01=ECU, 0x02=Communication, 0x03=Security, 0x04=Sensor)
 *   VV = Subsystem
 *   FF = Failure code
 *
 * ┌──────────────┬──────────┬────────────────────────────────────────┐
 * │ DTC          │ EventID  │ Description                           │
 * ├──────────────┼──────────┼────────────────────────────────────────┤
 * │ 0x010101     │ 1        │ ECU Internal Error                    │
 * │ 0x010102     │ 2        │ ECU Voltage Out of Range              │
 * │ 0x010103     │ 3        │ ECU Temperature Out of Range          │
 * │ 0x010104     │ 4        │ NvM CRC Error                         │
 * │ 0x010301     │ 5        │ CAN Bus Off                           │
 * │ 0x010302     │ 6        │ CAN Communication Timeout             │
 * │ 0x020101     │ 7        │ BLE Communication Error               │
 * │ 0x020102     │ 8        │ BLE Connection Lost                   │
 * │ 0x020201     │ 9        │ UWB Communication Error               │
 * │ 0x020202     │ 10       │ UWB Ranging Timeout                   │
 * │ 0x030101     │ 11       │ Security Access Violation             │
 * │ 0x030102     │ 12       │ Invalid Key Try Exceeded              │
 * │ 0x030103     │ 13       │ Certificate Expired/Revoked           │
 * │ 0x030201     │ 14       │ Secure Channel Failure                │
 * │ 0x030301     │ 15       │ Anti-Downgrade Violation              │
 * │ 0x040101     │ 16       │ Digital Key NFC Error                 │
 * └──────────────┴──────────┴────────────────────────────────────────┘
 * ========================================================================= */

/* ---- yuleDKCS Event IDs ---- */
#define DK_DEM_EVENT_ECU_INTERNAL           ((Dem_EventIdType)1U)
#define DK_DEM_EVENT_VOLTAGE_OUT_OF_RANGE   ((Dem_EventIdType)2U)
#define DK_DEM_EVENT_TEMP_OUT_OF_RANGE      ((Dem_EventIdType)3U)
#define DK_DEM_EVENT_NVM_CRC_ERROR          ((Dem_EventIdType)4U)
#define DK_DEM_EVENT_CAN_BUS_OFF            ((Dem_EventIdType)5U)
#define DK_DEM_EVENT_CAN_TIMEOUT            ((Dem_EventIdType)6U)
#define DK_DEM_EVENT_BLE_COMM_ERROR         ((Dem_EventIdType)7U)
#define DK_DEM_EVENT_BLE_CONNECTION_LOST    ((Dem_EventIdType)8U)
#define DK_DEM_EVENT_UWB_COMM_ERROR         ((Dem_EventIdType)9U)
#define DK_DEM_EVENT_UWB_RANGING_TIMEOUT    ((Dem_EventIdType)10U)
#define DK_DEM_EVENT_SEC_ACCESS_VIOLATION   ((Dem_EventIdType)11U)
#define DK_DEM_EVENT_INVALID_KEY_EXCEED     ((Dem_EventIdType)12U)
#define DK_DEM_EVENT_CERT_EXPIRED           ((Dem_EventIdType)13U)
#define DK_DEM_EVENT_SECURE_CHANNEL_FAIL    ((Dem_EventIdType)14U)
#define DK_DEM_EVENT_ANTI_DOWNGRADE_VIOL    ((Dem_EventIdType)15U)
#define DK_DEM_EVENT_NFC_ERROR              ((Dem_EventIdType)16U)

/* ---- yuleDKCS DTCs ---- */
#define DK_DTC_ECU_INTERNAL                ((Dem_DtcType)0x010101U)
#define DK_DTC_VOLTAGE_OUT_OF_RANGE        ((Dem_DtcType)0x010102U)
#define DK_DTC_TEMP_OUT_OF_RANGE           ((Dem_DtcType)0x010103U)
#define DK_DTC_NVM_CRC_ERROR               ((Dem_DtcType)0x010104U)
#define DK_DTC_CAN_BUS_OFF                 ((Dem_DtcType)0x010301U)
#define DK_DTC_CAN_TIMEOUT                 ((Dem_DtcType)0x010302U)
#define DK_DTC_BLE_COMM_ERROR              ((Dem_DtcType)0x020101U)
#define DK_DTC_BLE_CONNECTION_LOST         ((Dem_DtcType)0x020102U)
#define DK_DTC_UWB_COMM_ERROR              ((Dem_DtcType)0x020201U)
#define DK_DTC_UWB_RANGING_TIMEOUT         ((Dem_DtcType)0x020202U)
#define DK_DTC_SEC_ACCESS_VIOLATION        ((Dem_DtcType)0x030101U)
#define DK_DTC_INVALID_KEY_EXCEED          ((Dem_DtcType)0x030102U)
#define DK_DTC_CERT_EXPIRED                ((Dem_DtcType)0x030103U)
#define DK_DTC_SECURE_CHANNEL_FAIL         ((Dem_DtcType)0x030201U)
#define DK_DTC_ANTI_DOWNGRADE_VIOL         ((Dem_DtcType)0x030301U)
#define DK_DTC_NFC_ERROR                   ((Dem_DtcType)0x040101U)

/* ---- yuleDKCS Functional Unit Assignments ---- */
#define DK_DEM_FU_ECU                      (0x01U)  /* ECU Hardware */
#define DK_DEM_FU_CAN                      (0x03U)  /* CAN Communication */
#define DK_DEM_FU_BLE                      (0x04U)  /* BLE Communication */
#define DK_DEM_FU_UWB                      (0x05U)  /* UWB Communication */
#define DK_DEM_FU_SECURITY                 (0x06U)  /* Security/Safety */
#define DK_DEM_FU_NFC                      (0x07U)  /* NFC Communication */

/* ---- yuleDKCS Number of Active Events/DTCs ---- */
#define DK_DEM_NUM_ACTIVE_EVENTS           (16U)
#define DK_DEM_NUM_ACTIVE_DTCS             (16U)

#endif /* DEM_CFG_DK_H */
