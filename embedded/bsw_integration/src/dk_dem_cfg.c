/**
 * @file dk_dem_cfg.c
 * @brief yuleDKCS DEM Link-Time Configuration — DTC Event Definitions
 *
 * Defines ICCE Digital Key ECU diagnostic trouble codes (DTCs), events,
 * freeze frame, and extended data record configurations.
 */

#include "Compiler.h"
#include "Dem_Types.h"
#include "Dem_Cfg.h"
#include "Dem_Cfg_Dk.h"
#include "Dem_Pbcfg.h"
#include "MemMap.h"

/* =========================================================================
 * Event Parameters — yuleDKCS Digital Key DTC Events
 * ========================================================================= */
#define DEM_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

STATIC const Dem_EventParameterType Dk_DemEvents[DK_DEM_NUM_ACTIVE_EVENTS] = {
    /* Event 1: ECU Internal Error (counter-based debounce, immediate) */
    {
        .EventId                          = DK_DEM_EVENT_ECU_INTERNAL,
        .Dtc                              = DK_DTC_ECU_INTERNAL,
        .EventPriority                    = 1U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 1U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 2: Voltage Out of Range (time-based, 1s threshold) */
    {
        .EventId                          = DK_DEM_EVENT_VOLTAGE_OUT_OF_RANGE,
        .Dtc                              = DK_DTC_VOLTAGE_OUT_OF_RANGE,
        .EventPriority                    = 2U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 2U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_TIME,
        .EventCounterBased                = FALSE,
        .EventTimeBased                   = TRUE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 1000U,
        .DebounceTimePassedThresholdMs    = 1000U
    },
    /* Event 3: Temperature Out of Range (counter-based) */
    {
        .EventId                          = DK_DEM_EVENT_TEMP_OUT_OF_RANGE,
        .Dtc                              = DK_DTC_TEMP_OUT_OF_RANGE,
        .EventPriority                    = 2U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 2U,
        .EventConfirmationThreshold       = 3U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 64,
        .DebounceCounterPassedThreshold   = -64,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 4: NvM CRC Error (instant, no debounce) */
    {
        .EventId                          = DK_DEM_EVENT_NVM_CRC_ERROR,
        .Dtc                              = DK_DTC_NVM_CRC_ERROR,
        .EventPriority                    = 1U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 1U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_NONE,
        .EventCounterBased                = FALSE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 0U,
        .DebounceTimePassedThresholdMs    = 0U
    },
    /* Event 5: CAN Bus Off (monitor internal debounce) */
    {
        .EventId                          = DK_DEM_EVENT_CAN_BUS_OFF,
        .Dtc                              = DK_DTC_CAN_BUS_OFF,
        .EventPriority                    = 1U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 1U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_MONITOR,
        .EventCounterBased                = FALSE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = TRUE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 0U,
        .DebounceTimePassedThresholdMs    = 0U
    },
    /* Event 6: CAN Communication Timeout (counter-based) */
    {
        .EventId                          = DK_DEM_EVENT_CAN_TIMEOUT,
        .Dtc                              = DK_DTC_CAN_TIMEOUT,
        .EventPriority                    = 2U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 2U,
        .EventConfirmationThreshold       = 3U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 7: BLE Communication Error (counter-based) */
    {
        .EventId                          = DK_DEM_EVENT_BLE_COMM_ERROR,
        .Dtc                              = DK_DTC_BLE_COMM_ERROR,
        .EventPriority                    = 2U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 2U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 64,
        .DebounceCounterPassedThreshold   = -64,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 8: BLE Connection Lost (counter, lower priority) */
    {
        .EventId                          = DK_DEM_EVENT_BLE_CONNECTION_LOST,
        .Dtc                              = DK_DTC_BLE_CONNECTION_LOST,
        .EventPriority                    = 3U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 2U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 32,
        .DebounceCounterPassedThreshold   = -32,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 9: UWB Communication Error (counter-based) */
    {
        .EventId                          = DK_DEM_EVENT_UWB_COMM_ERROR,
        .Dtc                              = DK_DTC_UWB_COMM_ERROR,
        .EventPriority                    = 2U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 2U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 64,
        .DebounceCounterPassedThreshold   = -64,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 10: UWB Ranging Timeout (time-based) */
    {
        .EventId                          = DK_DEM_EVENT_UWB_RANGING_TIMEOUT,
        .Dtc                              = DK_DTC_UWB_RANGING_TIMEOUT,
        .EventPriority                    = 3U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 2U,
        .EventConfirmationThreshold       = 3U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_TIME,
        .EventCounterBased                = FALSE,
        .EventTimeBased                   = TRUE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 1000U,
        .DebounceTimePassedThresholdMs    = 1000U
    },
    /* Event 11: Security Access Violation (instant) */
    {
        .EventId                          = DK_DEM_EVENT_SEC_ACCESS_VIOLATION,
        .Dtc                              = DK_DTC_SEC_ACCESS_VIOLATION,
        .EventPriority                    = 1U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 1U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_NONE,
        .EventCounterBased                = FALSE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 0U,
        .DebounceTimePassedThresholdMs    = 0U
    },
    /* Event 12: Invalid Key Exceeded (counter) */
    {
        .EventId                          = DK_DEM_EVENT_INVALID_KEY_EXCEED,
        .Dtc                              = DK_DTC_INVALID_KEY_EXCEED,
        .EventPriority                    = 1U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 1U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 13: Certificate Expired (instant) */
    {
        .EventId                          = DK_DEM_EVENT_CERT_EXPIRED,
        .Dtc                              = DK_DTC_CERT_EXPIRED,
        .EventPriority                    = 1U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 1U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_NONE,
        .EventCounterBased                = FALSE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 0U,
        .DebounceTimePassedThresholdMs    = 0U
    },
    /* Event 14: Secure Channel Failure (counter) */
    {
        .EventId                          = DK_DEM_EVENT_SECURE_CHANNEL_FAIL,
        .Dtc                              = DK_DTC_SECURE_CHANNEL_FAIL,
        .EventPriority                    = 2U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 2U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 64,
        .DebounceCounterPassedThreshold   = -64,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    },
    /* Event 15: Anti-Downgrade Violation (instant) */
    {
        .EventId                          = DK_DEM_EVENT_ANTI_DOWNGRADE_VIOL,
        .Dtc                              = DK_DTC_ANTI_DOWNGRADE_VIOL,
        .EventPriority                    = 1U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 1U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_NONE,
        .EventCounterBased                = FALSE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 127,
        .DebounceCounterPassedThreshold   = -128,
        .DebounceTimeFailedThresholdMs    = 0U,
        .DebounceTimePassedThresholdMs    = 0U
    },
    /* Event 16: Digital Key NFC Error (counter-based) */
    {
        .EventId                          = DK_DEM_EVENT_NFC_ERROR,
        .Dtc                              = DK_DTC_NFC_ERROR,
        .EventPriority                    = 2U,
        .EventAvailable                   = TRUE,
        .EventReporting                   = TRUE,
        .EventFailureCycleCounterThreshold = 1U,
        .EventConfirmationThreshold       = 2U,
        .DebounceAlgorithm                = DEM_DEBOUNCE_ALGORITHM_COUNTER,
        .EventCounterBased                = TRUE,
        .EventTimeBased                   = FALSE,
        .EventMonitorInternal             = FALSE,
        .DebounceCounterFailedThreshold   = 64,
        .DebounceCounterPassedThreshold   = -64,
        .DebounceTimeFailedThresholdMs    = 100U,
        .DebounceTimePassedThresholdMs    = 100U
    }
};

/* =========================================================================
 * DTC Parameter Configuration
 * ========================================================================= */
/* Padded to DEM_NUM_DTCS entries: 16 active + (DEM_NUM_DTCS - 16) reserved */
STATIC const Dem_DtcParameterType Dk_DemDtcParams[DEM_NUM_DTCS] = {
    {DK_DTC_ECU_INTERNAL,               DEM_SEVERITY_CHECK_IMMEDIATELY, DK_DEM_FU_ECU, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_VOLTAGE_OUT_OF_RANGE,       DEM_SEVERITY_CHECK_AT_NEXT_HALT, DK_DEM_FU_ECU, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_TEMP_OUT_OF_RANGE,          DEM_SEVERITY_CHECK_AT_NEXT_HALT, DK_DEM_FU_ECU, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_NVM_CRC_ERROR,              DEM_SEVERITY_CHECK_IMMEDIATELY, DK_DEM_FU_ECU, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_CAN_BUS_OFF,                DEM_SEVERITY_CHECK_IMMEDIATELY, DK_DEM_FU_CAN, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_CAN_TIMEOUT,                DEM_SEVERITY_MAINTENANCE_ONLY, DK_DEM_FU_CAN, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_BLE_COMM_ERROR,             DEM_SEVERITY_CHECK_AT_NEXT_HALT, DK_DEM_FU_BLE, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_BLE_CONNECTION_LOST,        DEM_SEVERITY_MAINTENANCE_ONLY, DK_DEM_FU_BLE, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_UWB_COMM_ERROR,             DEM_SEVERITY_CHECK_AT_NEXT_HALT, DK_DEM_FU_UWB, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_UWB_RANGING_TIMEOUT,        DEM_SEVERITY_MAINTENANCE_ONLY, DK_DEM_FU_UWB, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_SEC_ACCESS_VIOLATION,       DEM_SEVERITY_CHECK_IMMEDIATELY, DK_DEM_FU_SECURITY, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_INVALID_KEY_EXCEED,         DEM_SEVERITY_CHECK_AT_NEXT_HALT, DK_DEM_FU_SECURITY, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_CERT_EXPIRED,               DEM_SEVERITY_CHECK_IMMEDIATELY, DK_DEM_FU_SECURITY, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_SECURE_CHANNEL_FAIL,        DEM_SEVERITY_CHECK_AT_NEXT_HALT, DK_DEM_FU_SECURITY, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_ANTI_DOWNGRADE_VIOL,        DEM_SEVERITY_CHECK_IMMEDIATELY, DK_DEM_FU_SECURITY, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    {DK_DTC_NFC_ERROR,                  DEM_SEVERITY_CHECK_AT_NEXT_HALT, DK_DEM_FU_NFC, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, TRUE, 40U, FALSE},
    /* Reserved/padding to fill DEM_NUM_DTCS array (Dcm.c iterates DEM_NUM_DTCS) */
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE},
    {0, 0, 0, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, FALSE, 40U, FALSE}
};

/* =========================================================================
 * Freeze Frame Records — DID snapshots on DTC confirmation
 * ========================================================================= */
STATIC const uint16 Dk_DemFfDids_01[] = {0xF100U, 0xF101U, 0xF400U, 0xF401U};
STATIC const uint16 Dk_DemFfDids_02[] = {0xF100U, 0xF101U, 0xF190U, 0xF400U, 0xF401U, 0xF402U};

STATIC const Dem_FreezeFrameRecordType Dk_DemFreezeFrames[] = {
    {1U, 4U, Dk_DemFfDids_01, TRUE},
    {2U, 6U, Dk_DemFfDids_02, TRUE}
};

/* =========================================================================
 * Extended Data Records
 * ========================================================================= */
STATIC const Dem_ExtendedDataRecordType Dk_DemExtDataRecs[] = {
    {1U, 4U,  TRUE, FALSE},    /* Occurrence Counter */
    {2U, 4U,  TRUE, FALSE},    /* Aging Counter */
    {3U, 8U,  TRUE, TRUE},     /* Timestamp */
    {4U, 32U, TRUE, FALSE}     /* Environmental Data */
};

/* =========================================================================
 * Indicator Configuration
 * ========================================================================= */
STATIC const Dem_IndicatorType Dk_DemIndicators[] = {
    {DEM_INDICATOR_MIL,  DEM_INDICATOR_CONTINUOUS,  3U},
    {DEM_INDICATOR_SVS,  DEM_INDICATOR_CONTINUOUS,  3U},
    {DEM_INDICATOR_AWLS, DEM_INDICATOR_BLINKING,   1U},
    {DEM_INDICATOR_PL,   DEM_INDICATOR_CONTINUOUS,  2U}
};

/* =========================================================================
 * Global DEM Configuration
 * ========================================================================= */
CONST(Dem_ConfigType, DEM_CONST) Dk_Dem_Config = {
    .EventParameters          = Dk_DemEvents,
    .NumEvents                = DK_DEM_NUM_ACTIVE_EVENTS,
    .DtcParameters            = Dk_DemDtcParams,
    .NumDtcs                  = DK_DEM_NUM_ACTIVE_DTCS,
    .FreezeFrameRecords       = Dk_DemFreezeFrames,
    .NumFreezeFrameRecords    = (uint8)(sizeof(Dk_DemFreezeFrames) / sizeof(Dk_DemFreezeFrames[0])),
    .ExtendedDataRecords      = Dk_DemExtDataRecs,
    .NumExtendedDataRecords   = (uint8)(sizeof(Dk_DemExtDataRecs) / sizeof(Dk_DemExtDataRecs[0])),
    .Indicators               = Dk_DemIndicators,
    .NumIndicators            = (uint8)(sizeof(Dk_DemIndicators) / sizeof(Dk_DemIndicators[0])),
    .DevErrorDetect           = (boolean)DEM_DEV_ERROR_DETECT,
    .VersionInfoApi           = (boolean)DEM_VERSION_INFO_API,
    .ClearDtcSupported        = (boolean)DEM_CLEAR_DTC_SUPPORTED,
    .ClearDtcLimitation       = (boolean)DEM_CLEAR_DTC_LIMITATION,
    .DtcStatusAvailabilityMask = DEM_DTC_STATUS_AVAILABILITY_MASK,
    .OBDRelevantSupport       = (boolean)DEM_OBD_RELEVANT_SUPPORT,
    .J1939Support             = (boolean)DEM_J1939_SUPPORT,
    .TriggerFimReports        = FALSE,
    .TriggerMonitorInitBeforeClearOk = TRUE,
    .ClearDTCLambdaNotification = NULL_PTR,
    .ClearDTCStartNotification   = NULL_PTR,
    .ClearDTCFinishNotification  = NULL_PTR
};

#define DEM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

/* =========================================================================
 * Dem_Config for DCM integration
 * Dem.h declares "extern const Dem_ConfigType Dem_Config"
 * C99 does not allow const-to-const initialization; alias via pointer.
 * Dem_ConfigPtr is defined here; Dem.c accesses Dem_InternalState.ConfigPtr
 * which is set during Dem_Init(). DCM integration reads Dem_Config directly.
 * ========================================================================= */
#define DEM_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

/* Assign the same initial fields. No const-to-const alias in C99. */
const Dem_ConfigType Dem_Config = {
    .EventParameters          = Dk_DemEvents,
    .NumEvents                = DK_DEM_NUM_ACTIVE_EVENTS,
    .DtcParameters            = Dk_DemDtcParams,
    .NumDtcs                  = DK_DEM_NUM_ACTIVE_DTCS,
    .FreezeFrameRecords       = Dk_DemFreezeFrames,
    .NumFreezeFrameRecords    = (uint8)(sizeof(Dk_DemFreezeFrames) / sizeof(Dk_DemFreezeFrames[0])),
    .ExtendedDataRecords      = Dk_DemExtDataRecs,
    .NumExtendedDataRecords   = (uint8)(sizeof(Dk_DemExtDataRecs) / sizeof(Dk_DemExtDataRecs[0])),
    .Indicators               = Dk_DemIndicators,
    .NumIndicators            = (uint8)(sizeof(Dk_DemIndicators) / sizeof(Dk_DemIndicators[0])),
    .DevErrorDetect           = (boolean)DEM_DEV_ERROR_DETECT,
    .VersionInfoApi           = (boolean)DEM_VERSION_INFO_API,
    .ClearDtcSupported        = (boolean)DEM_CLEAR_DTC_SUPPORTED,
    .ClearDtcLimitation       = (boolean)DEM_CLEAR_DTC_LIMITATION,
    .DtcStatusAvailabilityMask = DEM_DTC_STATUS_AVAILABILITY_MASK,
    .OBDRelevantSupport       = (boolean)DEM_OBD_RELEVANT_SUPPORT,
    .J1939Support             = (boolean)DEM_J1939_SUPPORT,
    .TriggerFimReports        = FALSE,
    .TriggerMonitorInitBeforeClearOk = TRUE,
    .ClearDTCLambdaNotification = NULL_PTR,
    .ClearDTCStartNotification   = NULL_PTR,
    .ClearDTCFinishNotification  = NULL_PTR
};

#define DEM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"
