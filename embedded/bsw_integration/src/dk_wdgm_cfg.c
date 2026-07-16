/**
 * @file dk_wdgm_cfg.c
 * @brief yuleDKCS WdgM 配置 — 适配 yuleASR Wdgm.h 类型定义
 */

#include "Wdgm.h"
#include "WdgM_Cfg_Dk.h"

/* =========================================================================
 * 监督实体配置 (使用 yuleASR Wdgm 类型)
 * ========================================================================= */
static const Wdgm_SupervisedEntityConfigType WdgM_EntityConfigs[WDGM_DK_SUPERVISED_ENTITIES] = {
    { WDGM_DK_SEID_MAIN_CYCLE,      TRUE,  5U },
    { WDGM_DK_SEID_BLE_UWB,         TRUE,  2U },
    { WDGM_DK_SEID_VEHICLE_CAN,     TRUE,  1U },
};

/* =========================================================================
 * 检查点配置
 * ========================================================================= */
static const Wdgm_CheckpointConfigType WdgM_CheckpointConfigs[] = {
    /* Main Cycle 检查点 (5个) */
    { 0, WDGM_DK_SEID_MAIN_CYCLE,      1U },   /* Init */
    { 1, WDGM_DK_SEID_MAIN_CYCLE,      1U },   /* 10ms */
    { 2, WDGM_DK_SEID_MAIN_CYCLE,      1U },   /* 50ms */
    { 3, WDGM_DK_SEID_MAIN_CYCLE,      1U },   /* 100ms */
    { 4, WDGM_DK_SEID_MAIN_CYCLE,      1U },   /* Background */

    /* BLE/UWB 检查点 */
    { 0, WDGM_DK_SEID_BLE_UWB,         1U },   /* BLE 接收 */
    { 1, WDGM_DK_SEID_BLE_UWB,         1U },   /* UWB 测距 */

    /* Vehicle CAN */
    { 0, WDGM_DK_SEID_VEHICLE_CAN,     1U },   /* CAN 通信 */
};

/* =========================================================================
 * WdgM 全局配置
 * ========================================================================= */
const Wdgm_ConfigType Wdgm_Config = {
    .SEConfigs          = WdgM_EntityConfigs,
    .InitialMode        = ((void*)0),
    .SupervisionCycleMs = 10U,
    .ExpirationTolerance = 1U,
};
