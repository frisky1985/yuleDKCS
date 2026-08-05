/**
 * @file dk_dcm_cfg.c
 * @brief yuleDKCS DCM Link-Time Configuration — UDS DID & RID Tables
 *
 * Defines DIDs (Data Identifiers) and RIDs (Routine Identifiers) for the
 * ICCE Digital Key ECU's UDS diagnostic protocol implementation.
 */

#include "Compiler.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Cfg_Dk.h"
#include "ComStack_Types.h"

/* =========================================================================
 * External DID Read/Write Callback Functions
 * ========================================================================= */
extern Std_ReturnType DkDcm_ReadEcuIdentification(uint8* Data);
extern Std_ReturnType DkDcm_ReadDkStatus(uint8* Data);
extern Std_ReturnType DkDcm_ReadDkConfig(uint8* Data);
extern Std_ReturnType DkDcm_WriteDkConfig(const uint8* Data, uint16 Length);
extern Std_ReturnType DkDcm_ReadDkSession(uint8* Data);
extern Std_ReturnType DkDcm_ReadDkUwbRanging(uint8* Data);
extern Std_ReturnType DkDcm_ReadDkBleMetrics(uint8* Data);
extern Std_ReturnType DkDcm_ReadVehicleLockState(uint8* Data);
extern Std_ReturnType DkDcm_ReadMfrEcuData(uint8* Data);
extern Std_ReturnType DkDcm_ReadSwVersion(uint8* Data);
extern Std_ReturnType DkDcm_ReadHwVersion(uint8* Data);

/* =========================================================================
 * DID Configuration Table
 * ========================================================================= */
#define DCM_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

STATIC const Dcm_DIDConfigType Dk_DcmDIDs[] = {
    /* ECU Identification (read-only, all sessions) */
    {
        .DID           = DK_DID_ECU_IDENTIFICATION,
        .DataLength    = DK_DID_LEN_ECU_ID,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadEcuIdentification,
        .WriteDataFnc  = NULL_PTR
    },
    /* Digital Key Status (read-only, extended session) */
    {
        .DID           = DK_DID_DK_STATUS,
        .DataLength    = DK_DID_LEN_DK_STATUS,
        .SessionType   = DCM_EXTENDED_DIAGNOSTIC_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadDkStatus,
        .WriteDataFnc  = NULL_PTR
    },
    /* Digital Key Configuration (read/write, extended session, unlock level 1) */
    {
        .DID           = DK_DID_DK_CONFIG,
        .DataLength    = DK_DID_LEN_DK_CONFIG,
        .SessionType   = DCM_EXTENDED_DIAGNOSTIC_SESSION,
        .SecurityLevel = DK_DID_PERM_SECURE_READ,
        .ReadDataFnc   = DkDcm_ReadDkConfig,
        .WriteDataFnc  = DkDcm_WriteDkConfig
    },
    /* Digital Key Session Info (read, default + extended) */
    {
        .DID           = DK_DID_DK_SESSION,
        .DataLength    = DK_DID_LEN_DK_SESSION,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadDkSession,
        .WriteDataFnc  = NULL_PTR
    },
    /* UWB Ranging Data (read, extended session) */
    {
        .DID           = DK_DID_DK_UWB_RANGING,
        .DataLength    = DK_DID_LEN_DK_UWB,
        .SessionType   = DCM_EXTENDED_DIAGNOSTIC_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadDkUwbRanging,
        .WriteDataFnc  = NULL_PTR
    },
    /* BLE Connection Metrics (read, default session) */
    {
        .DID           = DK_DID_DK_BLE_METRICS,
        .DataLength    = DK_DID_LEN_DK_BLE,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadDkBleMetrics,
        .WriteDataFnc  = NULL_PTR
    },
    /* Vehicle Lock State (read-only, all sessions) */
    {
        .DID           = DK_DID_VEHICLE_LOCK_STATE,
        .DataLength    = DK_DID_LEN_LOCK_STATE,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadVehicleLockState,
        .WriteDataFnc  = NULL_PTR
    },
    /* Manufacturer ECU Data (read-only, default session) */
    {
        .DID           = DK_DID_MFR_ECU_DATA,
        .DataLength    = DK_DID_LEN_MFR_ECU,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadMfrEcuData,
        .WriteDataFnc  = NULL_PTR
    },
    /* Software Versions (read-only, all sessions) */
    {
        .DID           = DK_DID_BOOT_SW_VER,
        .DataLength    = DK_DID_LEN_BOOT_SW,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadSwVersion,
        .WriteDataFnc  = NULL_PTR
    },
    {
        .DID           = DK_DID_APP_SW_VER,
        .DataLength    = DK_DID_LEN_APP_SW,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadSwVersion,
        .WriteDataFnc  = NULL_PTR
    },
    {
        .DID           = DK_DID_HW_VER,
        .DataLength    = DK_DID_LEN_HW,
        .SessionType   = DCM_DEFAULT_SESSION,
        .SecurityLevel = DK_DID_PERM_READ_ONLY,
        .ReadDataFnc   = DkDcm_ReadHwVersion,
        .WriteDataFnc  = NULL_PTR
    }
};

/* =========================================================================
 * RID Configuration Table (Routine Control)
 * ========================================================================= */

/* External RID callbacks */
extern Std_ReturnType DkDcm_RoutineEcuReset(const uint8* RequestData, uint16 RequestDataLength,
                                            uint8* ResponseData, uint16* ResponseDataLength);
extern Std_ReturnType DkDcm_RoutineSecuritySeed(const uint8* RequestData, uint16 RequestDataLength,
                                                uint8* ResponseData, uint16* ResponseDataLength);
extern Std_ReturnType DkDcm_RoutineDiagSelfTest(const uint8* RequestData, uint16 RequestDataLength,
                                                uint8* ResponseData, uint16* ResponseDataLength);
extern Std_ReturnType DkDcm_RoutineFactoryReset(const uint8* RequestData, uint16 RequestDataLength,
                                                uint8* ResponseData, uint16* ResponseDataLength);

STATIC const Dcm_RIDConfigType Dk_DcmRIDs[] = {
    {
        .RID             = DK_RID_ECU_RESET,
        .SessionType     = DCM_DEFAULT_SESSION,
        .SecurityLevel   = DK_DID_PERM_READ_ONLY,
        .StartFnc        = DkDcm_RoutineEcuReset,
        .StopFnc         = NULL_PTR,
        .RequestResultFnc = NULL_PTR
    },
    {
        .RID             = DK_RID_SECURITY_SEED,
        .SessionType     = DCM_DEFAULT_SESSION,
        .SecurityLevel   = DK_DID_PERM_READ_ONLY,
        .StartFnc        = DkDcm_RoutineSecuritySeed,
        .StopFnc         = NULL_PTR,
        .RequestResultFnc = NULL_PTR
    },
    {
        .RID             = DK_RID_DK_DIAG_SELF_TEST,
        .SessionType     = DCM_EXTENDED_DIAGNOSTIC_SESSION,
        .SecurityLevel   = DK_DID_PERM_SECURE_WRITE,
        .StartFnc        = DkDcm_RoutineDiagSelfTest,
        .StopFnc         = NULL_PTR,
        .RequestResultFnc = NULL_PTR
    },
    {
        .RID             = DK_RID_DK_FACTORY_RESET,
        .SessionType     = DCM_PROGRAMMING_SESSION,
        .SecurityLevel   = DK_DID_PERM_FACTORY_WRITE,
        .StartFnc        = DkDcm_RoutineFactoryReset,
        .StopFnc         = NULL_PTR,
        .RequestResultFnc = NULL_PTR
    }
};

#define DCM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

/* =========================================================================
 * Global DCM Configuration
 * ========================================================================= */
#define DCM_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

CONST(Dcm_ConfigType, DCM_CONST) Dcm_Config = {
    .NumProtocols       = DCM_NUM_PROTOCOLS,
    .NumConnections     = DCM_NUM_CONNECTIONS,
    .NumRxPduIds        = DCM_NUM_RX_PDU_IDS,
    .NumTxPduIds        = DCM_NUM_TX_PDU_IDS,
    .NumSessions        = DCM_NUM_SESSIONS,
    .NumSecurityLevels  = DCM_NUM_SECURITY_LEVELS,
    .NumServices        = DCM_NUM_SERVICES,
    .NumDIDs            = (uint8)(sizeof(Dk_DcmDIDs) / sizeof(Dk_DcmDIDs[0])),
    .NumRIDs            = (uint8)(sizeof(Dk_DcmRIDs) / sizeof(Dk_DcmRIDs[0])),
    .DIDs               = Dk_DcmDIDs,
    .RIDs               = Dk_DcmRIDs,
    .DevErrorDetect     = (boolean)DCM_DEV_ERROR_DETECT,
    .VersionInfoApi     = (boolean)DCM_VERSION_INFO_API,
    .RespondAllRequest  = (boolean)DCM_RESPOND_ALL_REQUEST,
    .DcmTaskTime        = (boolean)DCM_TASK_TIME
};

#define DCM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"
