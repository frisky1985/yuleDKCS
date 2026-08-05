/**
 * @file ComStack_Types.h
 * @brief Communication Stack Types — yuleDKCS BSW Integration
 *
 * AUTOSAR-standard PDU and communication type definitions.
 */

#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"

/* PDU Identifier Type */
typedef uint16 PduIdType;

/* PDU Length Type */
typedef uint16 PduLengthType;

/* PDU Information Type */
typedef struct {
    uint8* SduDataPtr;
    PduLengthType SduLength;
    uint8* MetaDataPtr;
} PduInfoType;

/* TP Parameter Type */
typedef enum {
    TP_STMIN = 0,
    TP_BS,
    TP_BC
} TPParameterType;

/* Buffer Request Return Type */
typedef enum {
    BUFREQ_OK = 0,
    BUFREQ_E_NOT_OK,
    BUFREQ_E_BUSY,
    BUFREQ_E_OVFL
} BufReq_ReturnType;

/* Protocol configuration type for network layer */
typedef uint8 PdulConfigType;

/* I-PDU Group Vector */
typedef uint8 IpduGroupVector;

/* Retry Info Type */
typedef struct {
    uint8 TpDataState;
    PduLengthType TxTpDataCnt;
} RetryInfoType;

#endif /* COMSTACK_TYPES_H */
