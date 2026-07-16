/**
 * @file bsw_stubs.c
 * @brief BSW Service Stubs — yuleDKCS Phase 1/3
 *
 * 剩余需要桩的函数:
 *   - ComM: 通信管理 (COM stack 实际在 yuleASR 中有, 但 ComM 作为可选模块)
 *   - BswM_EcuM_CurrentWakeup: BswM 的回调 (BswM 正式实现已包含)
 *   - NVM_REMOVED: NvM 已替换为正式实现 (dk_nvm_cfg.c + NvM.c)
 *
 * Phase 3: NvM 和 CSM 正式实现, 不再需要桩
 */

#include "Std_Types.h"
#include "ComM.h"
#include "BswM.h"
#include "EcuM.h"
#include "WdgM.h"
#include "Dem.h"
#include "Dem_Types.h"

/* =========================================================================
 * ComM Stubs (通信管理 — 可选, yuleASR 有 ComM 但 yuleDKCS 用 PduR 直接管理)
 * ========================================================================= */
void ComM_Init(void) {}
void ComM_DeInit(void) {}

/* =========================================================================
 * SchM Stubs (called by EcuM)
 * ========================================================================= */
void SchM_Init(void) {}
void SchM_Deinit(void) {}

/* =========================================================================
 * BswM extra callbacks (BswM.c 正式实现中已包含)
 * ========================================================================= */
void BswM_EcuM_CurrentWakeup(EcuM_WakeupSourceType Sources, EcuM_WakeupStatusType Status) {
    (void)Sources;
    (void)Status;
}

/* =========================================================================
 * Dem API Stubs: Dem_GetDTCStatus wrapper (demanded by Dcm.c)
 * ========================================================================= */
Std_ReturnType Dem_GetDTCStatus(Dem_DtcType DTC,
                                Dem_DTCOriginType DTCOrigin,
                                Dem_UdsStatusByteType* DTCStatus)
{
    return Dem_GetStatusOfDTC(DTC, DTCOrigin, DTCStatus);
}

/* =========================================================================
 * WdgM Force-Link: ensure the Wdgm static library object is linked.
 * ========================================================================= */
void (*volatile const _dk_wdgm_force)(const void*) = (void (*)(const void*))WdgM_Init;

/* =========================================================================
 * 空函数: 确保某些弱符号不被链接器丢弃
 * ========================================================================= */
void __attribute__((weak)) _dk_bsw_phase3_anchor(void) {}
