/**
 * @file Com_Cfg_Dk.h
 * @brief yuleDKCS COM Configuration Overrides — Digital Key Signal Mappings
 *
 * Defines missing AUTOSAR COM configuration macros and yuleDKCS-specific
 * signal/IPDU group definitions for the ICCE Digital Key communication.
 */

#ifndef COM_CFG_DK_H
#define COM_CFG_DK_H

#include "Com_Cfg.h"   /* yuleASR base Com_Cfg.h */

/* =========================================================================
 * yuleDKCS COM Topology
 *
 * Signal → IPDU mapping for Digital Key CAN communication:
 *
 * ┌──────────────┬──────────────────────┬──────────────────────────────┐
 * │ IPDU         │ Signals              │ Purpose                      │
 * ├──────────────┼──────────────────────┼──────────────────────────────┤
 * │ IPDU_TX_DK   │ DK_Status, DK_Vehicle│ TX: Digital Key status to CAN│
 * │ IPDU_RX_DK   │ DK_Command, DK_Auth  │ RX: Digital Key commands    │
 * │ IPDU_TX_DIAG │ Diag_Response (DCM)  │ TX: UDS diagnostic response │
 * │ IPDU_RX_DIAG │ Diag_Request  (DCM)  │ RX: UDS diagnostic request  │
 * └──────────────┴──────────────────────┴──────────────────────────────┘
 * ========================================================================= */

/* ---- I-PDU Count, Signal Count, Buffer Size ---- */
/* Defined via target_compile_definitions in CMakeLists.txt:
 *   COM_NUM_OF_IPDUS=4
 *   COM_NUM_OF_SIGNALS=8
 *   COM_MAX_IPDU_BUFFER_SIZE=64
 *   COM_NUM_OF_IPDU_GROUPS=2
 *   COM_NUM_IPDU_GROUPS=2
 *   COM_NUM_OF_SIGNAL_GROUPS=2
 *   COM_GATEWAY_SUPPORT=0
 *   COM_NUM_SIGNAL_GW_MAPPINGS=0
 */

/* ---- yuleDKCS Specific Defines ---- */
#define COM_NUM_DK_IPDUS                (4U)
#define COM_NUM_DK_SIGNALS              (8U)

/* ---- yuleDKCS Signal IDs ---- */
/* TX IPDU 0: Digital Key Status */
#define DK_SIG_BOOT_STATUS              ((Com_SignalIdType)0U)  /* uint8, 8bit @ pos 0 */
#define DK_SIG_BLE_CONNECTED           ((Com_SignalIdType)1U)  /* uint8, 8bit @ pos 8 */
#define DK_SIG_VEHICLE_LOCK_STATE       ((Com_SignalIdType)2U)  /* uint8, 8bit @ pos 16 */
#define DK_SIG_ENGINE_STARTED           ((Com_SignalIdType)3U)  /* uint8, 8bit @ pos 24 */

/* RX IPDU 1: Digital Key Commands */
#define DK_SIG_CMD_DOOR_LOCK            ((Com_SignalIdType)4U)  /* uint8, 8bit @ pos 0 */
#define DK_SIG_CMD_ENGINE_START         ((Com_SignalIdType)5U)  /* uint8, 8bit @ pos 8 */
#define DK_SIG_CMD_TRUNK_OPEN           ((Com_SignalIdType)6U)  /* uint8, 8bit @ pos 16 */
#define DK_SIG_CMD_WINDOW               ((Com_SignalIdType)7U)  /* uint8, 8bit @ pos 24 */

/* ---- yuleDKCS I-PDU IDs ---- */
#define DK_IPDU_TX_DK_STATUS            ((PduIdType)0U)
#define DK_IPDU_RX_DK_COMMAND           ((PduIdType)1U)
#define DK_IPDU_TX_DIAG_RESPONSE        ((PduIdType)2U)
#define DK_IPDU_RX_DIAG_REQUEST         ((PduIdType)3U)

/* ---- yuleDKCS I-PDU Group IDs ---- */
#define DK_IPDU_GRP_CAN_COMM            ((Com_IpduGroupIdType)0U)
#define DK_IPDU_GRP_DIAG                ((Com_IpduGroupIdType)1U)

#endif /* COM_CFG_DK_H */
