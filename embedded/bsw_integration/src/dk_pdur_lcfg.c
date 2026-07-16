/**
 * @file dk_pdur_lcfg.c
 * @brief yuleDKCS PDU Router Link-Time Configuration — Digital Key Routing
 *
 * Defines PDU routing paths for yuleDKCS:
 *   COM IPDU 0 (DK Status)    → CanIf TX
 *   COM IPDU 1 (DK Commands)  ← CanIf RX
 *   COM IPDU 2 (Diag Resp)    → DCM ↔ CanIf TX
 *   COM IPDU 3 (Diag Req)     ← CanIf RX → DCM
 */

#include "Compiler.h"
#include "PduR.h"
#include "PduR_Cfg.h"
#include "Com_Cfg_Dk.h"   /* for DK_IPDU_* definitions */

/* =========================================================================
 * Destination PDU Configurations
 * ========================================================================= */
#define PDUR_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"

/** @brief DK Status TX: COM → CanIf */
STATIC const PduR_DestPduConfigType Dk_PduR_TxDkStatus_Dest[] = {
    {
        .DestPduId = (PduIdType)0U,      /* CAN HW: CAN1 TX buffer 0 */
        .DestModule = PDUR_MODULE_CANIF,
        .Processing = PDUR_DESTPDU_PROCESSING_IMMEDIATE,
        .FifoDepth = 0U
    }
};

/** @brief DK Commands RX: CanIf → COM */
STATIC const PduR_DestPduConfigType Dk_PduR_RxDkCmd_Dest[] = {
    {
        .DestPduId = DK_IPDU_RX_DK_COMMAND,
        .DestModule = PDUR_MODULE_COM,
        .Processing = PDUR_DESTPDU_PROCESSING_IMMEDIATE,
        .FifoDepth = 0U
    }
};

/** @brief Diagnostic Response TX: DCM → CanIf */
STATIC const PduR_DestPduConfigType Dk_PduR_TxDiagResp_Dest[] = {
    {
        .DestPduId = (PduIdType)1U,      /* CAN HW: CAN1 TX buffer 1 */
        .DestModule = PDUR_MODULE_CANIF,
        .Processing = PDUR_DESTPDU_PROCESSING_IMMEDIATE,
        .FifoDepth = 0U
    }
};

/** @brief Diagnostic Request RX: CanIf → DCM */
STATIC const PduR_DestPduConfigType Dk_PduR_RxDiagReq_Dest[] = {
    {
        .DestPduId = DK_IPDU_RX_DIAG_REQUEST,
        .DestModule = PDUR_MODULE_DCM,
        .Processing = PDUR_DESTPDU_PROCESSING_IMMEDIATE,
        .FifoDepth = 0U
    }
};

/* =========================================================================
 * Routing Path Configurations
 * ========================================================================= */

/**
 * Path 0: COM TX DK Status → CanIf
 * Path 1: CanIf RX → COM RX DK Commands
 * Path 2: DCM TX Diagnostic Response → CanIf
 * Path 3: CanIf RX → DCM RX Diagnostic Request
 */
STATIC const PduR_RoutingPathConfigType Dk_PduR_RoutingPaths[] = {
    {
        .SrcPdu = {
            .SourcePduId   = DK_IPDU_TX_DK_STATUS,
            .SourceModule  = PDUR_MODULE_COM,
            .SduLength     = 8U          /* CAN Classic 8 bytes */
        },
        .DestPdus       = Dk_PduR_TxDkStatus_Dest,
        .NumDestPdus    = 1U,
        .PathType       = PDUR_ROUTING_PATH_DIRECT,
        .GatewayOperation = FALSE
    },
    {
        .SrcPdu = {
            .SourcePduId   = DK_IPDU_RX_DK_COMMAND,
            .SourceModule  = PDUR_MODULE_CANIF,
            .SduLength     = 8U
        },
        .DestPdus       = Dk_PduR_RxDkCmd_Dest,
        .NumDestPdus    = 1U,
        .PathType       = PDUR_ROUTING_PATH_DIRECT,
        .GatewayOperation = FALSE
    },
    {
        .SrcPdu = {
            .SourcePduId   = DK_IPDU_TX_DIAG_RESPONSE,
            .SourceModule  = PDUR_MODULE_DCM,
            .SduLength     = 64U         /* CAN-FD */
        },
        .DestPdus       = Dk_PduR_TxDiagResp_Dest,
        .NumDestPdus    = 1U,
        .PathType       = PDUR_ROUTING_PATH_DIRECT,
        .GatewayOperation = FALSE
    },
    {
        .SrcPdu = {
            .SourcePduId   = DK_IPDU_RX_DIAG_REQUEST,
            .SourceModule  = PDUR_MODULE_CANIF,
            .SduLength     = 64U
        },
        .DestPdus       = Dk_PduR_RxDiagReq_Dest,
        .NumDestPdus    = 1U,
        .PathType       = PDUR_ROUTING_PATH_DIRECT,
        .GatewayOperation = FALSE
    }
};

/* =========================================================================
 * Routing Path Groups
 * ========================================================================= */
STATIC const PduIdType Dk_PduR_Group_Com[] = {
    DK_IPDU_TX_DK_STATUS, DK_IPDU_RX_DK_COMMAND
};
STATIC const PduIdType Dk_PduR_Group_Diag[] = {
    DK_IPDU_TX_DIAG_RESPONSE, DK_IPDU_RX_DIAG_REQUEST
};

STATIC const PduR_RoutingPathGroupConfigType Dk_PduR_RoutingPathGroups[] = {
    {
        .GroupId         = PDUR_ROUTING_PATH_GROUP_0,
        .PduIds          = Dk_PduR_Group_Com,
        .NumPduIds       = 2U,
        .DefaultEnabled  = TRUE
    },
    {
        .GroupId         = PDUR_ROUTING_PATH_GROUP_1,
        .PduIds          = Dk_PduR_Group_Diag,
        .NumPduIds       = 2U,
        .DefaultEnabled  = TRUE
    },
    {
        .GroupId         = PDUR_ROUTING_PATH_GROUP_2,
        .PduIds          = NULL_PTR,
        .NumPduIds       = 0U,
        .DefaultEnabled  = FALSE
    },
    {
        .GroupId         = PDUR_ROUTING_PATH_GROUP_3,
        .PduIds          = NULL_PTR,
        .NumPduIds       = 0U,
        .DefaultEnabled  = FALSE
    }
};

/* =========================================================================
 * Global PduR Configuration
 * ========================================================================= */
const PduR_ConfigType PduR_Config = {
    .RoutingPaths       = Dk_PduR_RoutingPaths,
    .NumRoutingPaths    = 4U,
    .RoutingPathGroups  = Dk_PduR_RoutingPathGroups,
    .NumRoutingPathGroups = 4U,
    .DevErrorDetect     = TRUE,
    .VersionInfoApi     = TRUE
};

#define PDUR_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "MemMap.h"
