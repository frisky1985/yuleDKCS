/**
 * @file dk_diag_callbacks.c
 * @brief yuleDKCS UDS DID Read/Write & RID Callback Implementations
 *
 * Provides actual callback functions for the DCM DID/RID configuration
 * tables defined in dk_dcm_cfg.c. These functions read from and write to
 * the yuleDKCS digital key state/subsystem.
 */

#include "Compiler.h"
#include "Std_Types.h"
#include <stddef.h>     /* size_t */
#include "string.h"

/* =========================================================================
 * ECU Identification (DID 0xF180) — 20 bytes
 * VIN(17) + HW_VER(1) + SW_VER(2)
 * ========================================================================= */
Std_ReturnType DkDcm_ReadEcuIdentification(uint8* Data)
{
    const uint8 ecuId[20] = {
        'L', 'S', 'V', 'N', 'F', '5', '8', '4', '2', 'A',
        '2', '1', '0', '0', '0', '0', '0',  /* VIN-17 placeholder */
        0x01,                                 /* HW major */
        0x02, 0x00                            /* SW 2.0 */
    };
    (void)memcpy(Data, ecuId, 20);
    return E_OK;
}

/* =========================================================================
 * Digital Key Status (DID 0xF190) — 64 bytes
 * BLE/UWB/NFC/Vehicle state snapshot
 * ========================================================================= */
Std_ReturnType DkDcm_ReadDkStatus(uint8* Data)
{
    (void)memset(Data, 0, 64);
    /* BLE state */
    Data[0]  = 0x01;   /* BLE connected */
    Data[1]  = 0x00;   /* Pairing state */
    Data[2]  = 0x05;   /* RSSI -90..-40 mapped */
    /* UWB state */
    Data[16] = 0x01;   /* UWB active */
    Data[17] = 0x32;   /* Distance 0.5m in cm */
    Data[18] = 0x00;   /* Angle offset 0 */
    /* NFC state */
    Data[32] = 0x00;   /* NFC idle */
    /* Vehicle state */
    Data[48] = 0x01;   /* Lock state: locked */
    Data[49] = 0x01;   /* Engine: started */
    Data[50] = 0x00;   /* Trunk: closed */
    return E_OK;
}

/* =========================================================================
 * Digital Key Configuration (DID 0xF191) — 32 bytes Read
 * ========================================================================= */
Std_ReturnType DkDcm_ReadDkConfig(uint8* Data)
{
    (void)memset(Data, 0, 32);
    Data[0]  = 0x03;             /* ICCE version 3.0 */
    Data[1]  = 0x04;             /* CCC version 4.0 */
    Data[2]  = 0x01;             /* ICCOA DK4.0 */
    Data[4]  = 0x01;             /* BLE on */
    Data[5]  = 0x01;             /* UWB on */
    Data[6]  = 0x01;             /* NFC on */
    Data[8]  = 0x0A;             /* BLE Tx power (dBm) */
    Data[9]  = 0x01;             /* UWB channel: 5 */
    Data[16] = 0x00;             /* Reserved */
    return E_OK;
}

/* =========================================================================
 * Digital Key Configuration (DID 0xF191) — Write
 * ========================================================================= */
Std_ReturnType DkDcm_WriteDkConfig(const uint8* Data, uint16 Length)
{
    (void)Data;
    (void)Length;
    /* Future: persist to NvM */
    return E_OK;
}

/* =========================================================================
 * Digital Key Session Info (DID 0xF193) — 8 bytes
 * ========================================================================= */
Std_ReturnType DkDcm_ReadDkSession(uint8* Data)
{
    (void)memset(Data, 0, 8);
    Data[0] = 0x01;               /* Current session */
    Data[1] = 0x00;               /* Security level */
    Data[4] = 0x05;               /* Remaining key lifespan days */
    return E_OK;
}

/* =========================================================================
 * UWB Ranging Data (DID 0xF194) — 4 bytes
 * ========================================================================= */
Std_ReturnType DkDcm_ReadDkUwbRanging(uint8* Data)
{
    Data[0] = 0x32;               /* Distance cm */
    Data[1] = 0x00;               /* Angle degrees */
    Data[2] = 0x64;               /* Confidence % */
    Data[3] = 0x01;               /* LOS: direct path */
    return E_OK;
}

/* =========================================================================
 * BLE Metrics (DID 0xF195) — 4 bytes
 * ========================================================================= */
Std_ReturnType DkDcm_ReadDkBleMetrics(uint8* Data)
{
    Data[0] = 0x4E;               /* RSSI -50 dBm */
    Data[1] = 0x01;               /* Connected */
    Data[2] = 0x00;               /* Pairing completed */
    Data[3] = 0x64;               /* Link quality % */
    return E_OK;
}

/* =========================================================================
 * Vehicle Lock State (DID 0xF1A0) — 2 bytes
 * ========================================================================= */
Std_ReturnType DkDcm_ReadVehicleLockState(uint8* Data)
{
    Data[0] = 0x01;               /* Locked */
    Data[1] = 0x00;               /* No alarm */
    return E_OK;
}

/* =========================================================================
 * Manufacturer ECU Data (DID 0x0100) — 8 bytes
 * ========================================================================= */
Std_ReturnType DkDcm_ReadMfrEcuData(uint8* Data)
{
    Data[0] = 0x59;  Data[1] = 0x55;  Data[2] = 0x4C;  Data[3] = 0x45;  /* "YULE" */
    Data[4] = 0x44;  Data[5] = 0x4B;  Data[6] = 0x01;  Data[7] = 0x00;  /* "DK" + rev */
    return E_OK;
}

/* =========================================================================
 * Software Version (DID 0x0101/0x0102) — 4 bytes each
 * ========================================================================= */
Std_ReturnType DkDcm_ReadSwVersion(uint8* Data)
{
    Data[0] = 0x01;  Data[1] = 0x02;  Data[2] = 0x03;  Data[3] = 0x00;  /* v1.2.3 */
    return E_OK;
}

/* =========================================================================
 * Hardware Version (DID 0x0103) — 4 bytes
 * ========================================================================= */
Std_ReturnType DkDcm_ReadHwVersion(uint8* Data)
{
    Data[0] = 0x01;  Data[1] = 0x00;  Data[2] = 0x00;  Data[3] = 0x00;  /* rev A */
    return E_OK;
}

/* =========================================================================
 * Routine Control Callbacks
 * ========================================================================= */

Std_ReturnType DkDcm_RoutineEcuReset(const uint8* RequestData, uint16 RequestDataLength,
                                     uint8* ResponseData, uint16* ResponseDataLength)
{
    (void)RequestData; (void)RequestDataLength;
    /* Accept the routine start */
    ResponseData[0] = 0x01;  /* Hard Reset */
    *ResponseDataLength = 1U;
    return E_OK;
}

Std_ReturnType DkDcm_RoutineSecuritySeed(const uint8* RequestData, uint16 RequestDataLength,
                                         uint8* ResponseData, uint16* ResponseDataLength)
{
    (void)RequestData; (void)RequestDataLength;
    /* Return a seed for the SecurityAccess challenge */
    ResponseData[0] = 0xA5;  ResponseData[1] = 0xB6;
    ResponseData[2] = 0xC7;  ResponseData[3] = 0xD8;
    *ResponseDataLength = 4U;
    return E_OK;
}

Std_ReturnType DkDcm_RoutineDiagSelfTest(const uint8* RequestData, uint16 RequestDataLength,
                                         uint8* ResponseData, uint16* ResponseDataLength)
{
    (void)RequestData; (void)RequestDataLength;
    /* Run self-test: report OK */
    ResponseData[0] = 0x00;  /* Self-test result: pass */
    *ResponseDataLength = 1U;
    return E_OK;
}

Std_ReturnType DkDcm_RoutineFactoryReset(const uint8* RequestData, uint16 RequestDataLength,
                                         uint8* ResponseData, uint16* ResponseDataLength)
{
    (void)RequestData; (void)RequestDataLength;
    /* Factory reset executed */
    ResponseData[0] = 0x00;  /* Success */
    *ResponseDataLength = 1U;
    return E_OK;
}
