/**
 * @file dk_com_cfg.c
 * @brief yuleDKCS COM Link-Time Configuration — Digital Key Signals & I-PDUs
 *
 * AUTOSAR COM configuration tables for ICCE Digital Key CAN communication.
 * Defines signal-to-IPDU mapping, transfer properties, and timing.
 */

/* Compiler.h must be first (defines STATIC, NULL_PTR, CONST) */
#include "Compiler.h"

/* Com_Cfg_Dk.h must come BEFORE Com.h because Com.h uses COM_NUM_IPDU_GROUPS */
#include "Com_Cfg_Dk.h"
#include "Com.h"
#include "Com_Cfg.h"

/* =========================================================================
 * Signal Configuration Table
 *
 * Each signal maps to a specific bit range within an I-PDU buffer.
 * SignalGroupRef field serves as I-PDU index (compat layer).
 * ========================================================================= */
#define COM_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

STATIC const Com_SignalConfigType Dk_ComSignals[COM_NUM_OF_SIGNALS] = {
    /* TX IPDU 0: Digital Key Status (CAN ID 0x3C1, 8 bytes) */
    {
        .SignalId         = DK_SIG_BOOT_STATUS,
        .BitPosition      = 0U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED_ON_CHANGE,
        .FilterAlgorithm  = COM_ALWAYS,
        .FilterMask       = 0U,
        .FilterX          = 0U,
        .SignalGroupRef   = 0U   /* IPDU 0 */
    },
    {
        .SignalId         = DK_SIG_BLE_CONNECTED,
        .BitPosition      = 8U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED_ON_CHANGE,
        .FilterAlgorithm  = COM_ALWAYS,
        .FilterMask       = 0U,
        .FilterX          = 0U,
        .SignalGroupRef   = 0U
    },
    {
        .SignalId         = DK_SIG_VEHICLE_LOCK_STATE,
        .BitPosition      = 16U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED_ON_CHANGE,
        .FilterAlgorithm  = COM_MASKED_NEW_DIFFERS_MASKED_OLD,
        .FilterMask       = 0x000000FFU,
        .FilterX          = 0U,
        .SignalGroupRef   = 0U
    },
    {
        .SignalId         = DK_SIG_ENGINE_STARTED,
        .BitPosition      = 24U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED_ON_CHANGE,
        .FilterAlgorithm  = COM_ALWAYS,
        .FilterMask       = 0U,
        .FilterX          = 0U,
        .SignalGroupRef   = 0U
    },

    /* RX IPDU 1: Digital Key Commands (CAN ID 0x3C2, 8 bytes) */
    {
        .SignalId         = DK_SIG_CMD_DOOR_LOCK,
        .BitPosition      = 0U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED,
        .FilterAlgorithm  = COM_ALWAYS,
        .FilterMask       = 0U,
        .FilterX          = 0U,
        .SignalGroupRef   = 1U
    },
    {
        .SignalId         = DK_SIG_CMD_ENGINE_START,
        .BitPosition      = 8U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED,
        .FilterAlgorithm  = COM_ALWAYS,
        .FilterMask       = 0U,
        .FilterX          = 0U,
        .SignalGroupRef   = 1U
    },
    {
        .SignalId         = DK_SIG_CMD_TRUNK_OPEN,
        .BitPosition      = 16U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED,
        .FilterAlgorithm  = COM_ALWAYS,
        .FilterMask       = 0U,
        .FilterX          = 0U,
        .SignalGroupRef   = 1U
    },
    {
        .SignalId         = DK_SIG_CMD_WINDOW,
        .BitPosition      = 24U,
        .BitSize          = 8U,
        .Endianness       = COM_LITTLE_ENDIAN,
        .TransferProperty = COM_TRIGGERED,
        .FilterAlgorithm  = COM_ALWAYS,
        .FilterMask       = 0U,
        .FilterX          = 0U,
        .SignalGroupRef   = 1U
    }
};

/* =========================================================================
 * I-PDU Configuration Table
 *
 * IPDU 0: TX — Digital Key Status (periodic 100ms, repeating on change)
 * IPDU 1: RX — Digital Key Commands (receive, no TX)
 * IPDU 2: TX — Diagnostic Response (from DCM, triggered)
 * IPDU 3: RX — Diagnostic Request (to DCM)
 * ========================================================================= */
STATIC const Com_IPduConfigType Dk_ComIPdus[COM_NUM_OF_IPDUS] = {
    {
        .PduId                 = DK_IPDU_TX_DK_STATUS,
        .DataLength            = 8U,                    /* 8 bytes CAN payload */
        .RepeatingEnabled      = TRUE,
        .NumRepetitions        = 2U,                    /* 2 repetitions */
        .TimeBetweenRepetitions = 20U,                  /* 20ms between reps */
        .TimePeriod            = 100U                   /* 100ms period */
    },
    {
        .PduId                 = DK_IPDU_RX_DK_COMMAND,
        .DataLength            = 8U,
        .RepeatingEnabled      = FALSE,
        .NumRepetitions        = 0U,
        .TimeBetweenRepetitions = 0U,
        .TimePeriod            = 0U                     /* RX, no periodic TX */
    },
    {
        .PduId                 = DK_IPDU_TX_DIAG_RESPONSE,
        .DataLength            = 64U,                   /* CAN-FD up to 64 bytes */
        .RepeatingEnabled      = FALSE,
        .NumRepetitions        = 0U,
        .TimeBetweenRepetitions = 0U,
        .TimePeriod            = 0U                     /* Trigger-only */
    },
    {
        .PduId                 = DK_IPDU_RX_DIAG_REQUEST,
        .DataLength            = 64U,                   /* CAN-FD up to 64 bytes */
        .RepeatingEnabled      = FALSE,
        .NumRepetitions        = 0U,
        .TimeBetweenRepetitions = 0U,
        .TimePeriod            = 0U                     /* RX only */
    }
};

/* =========================================================================
 * COM Configuration — Top Level
 * ========================================================================= */
CONST(Com_ConfigType, COM_CONST) Com_Config = {
    .Signals    = Dk_ComSignals,
    .NumSignals = COM_NUM_OF_SIGNALS,
    .IPdus      = Dk_ComIPdus,
    .NumIPdus   = COM_NUM_OF_IPDUS
};

#define COM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"
