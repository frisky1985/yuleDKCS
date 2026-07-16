/**
 * @file WdgM_Cfg_Dk.h
 * @brief yuleDKCS 看门狗管理配置 (S32K312)
 *
 * 监督实体定义:
 *   - SEID_MAIN_CYCLE:      主循环存活监督 (所有 OS 任务)
 *   - SEID_COMMUNICATION:   通��模块存活监督 (BLE + UWB)
 *   - SEID_DIAGNOSTICS:     诊断模块存活监督
 *   - SEID_STORAGE:         存储模块存活监督 (NvM)
 *   - SEID_SAFETY_MONITOR:  安全监控 (SM 相关)
 *
 * 周期:
 *   - 监督周期: 10ms (与 WdgM_MainFunction 同步)
 *   - 过期容忍: 3 个周期 (30ms)
 */

#ifndef WDGM_CFG_DK_H
#define WDGM_CFG_DK_H

#include "Std_Types.h"

/* =========================================================================
 * 使能配置
 * ========================================================================= */
#define WDGM_DK_DEV_ERROR_DETECT            STD_ON
#define WDGM_DK_VERSION_INFO_API            STD_ON
#define WDGM_DK_DEADLINE_MONITORING         STD_ON
#define WDGM_DK_ALIVE_MONITORING            STD_ON
#define WDGM_DK_LOGICAL_MONITORING          STD_OFF

/* =========================================================================
 * 监督实体数
 * ========================================================================= */
#define WDGM_DK_SUPERVISED_ENTITIES         (5U)    /* MainCycle + COMM + DIAG + STORAGE + SAFETY */

/* =========================================================================
 * 监督实体 ID (yuleDKCS 专用)
 * ========================================================================= */
#define WDGM_DK_SEID_MAIN_CYCLE             (0U)    /* 主任务存活: 10ms / 50ms / 100ms */
#define WDGM_DK_SEID_BLE_UWB                (1U)    /* BLE + UWB 通信 */
#define WDGM_DK_SEID_VEHICLE_CAN            (2U)    /* 车辆 CAN 通信 */
#define WDGM_DK_SEID_STORAGE                (3U)    /* NvM 存储 */
#define WDGM_DK_SEID_SAFETY_MONITOR         (4U)    /* 安全监控 */

/* =========================================================================
 * 超时/周期配置
 * ========================================================================= */
#define WDGM_DK_SUPERVISION_CYCLE_MS        (10U)   /* 10ms 监督周期 */
#define WDGM_DK_EXPIRATION_TOLERANCE        (3U)    /* 容忍 3 次丢失 */
#define WDGM_DK_ALIVE_THRESHOLD             (5U)    /* 存活计数器阈值 */

/* =========================================================================
 * 独立看门狗配置 (S32K312 Internal Watchdog)
 * ========================================================================= */
#define WDGM_DK_IWD_TIMEOUT_MS              (100U)
#define WDGM_DK_IWD_WINDOW_START_PERCENT    (0U)
#define WDGM_DK_IWD_WINDOW_END_PERCENT      (100U)

/* =========================================================================
 * 窗口看门狗配置
 * ========================================================================= */
#define WDGM_DK_WWD_PERIOD_MS               (50U)
#define WDGM_DK_WWD_WINDOW_START_PERCENT    (25U)
#define WDGM_DK_WWD_WINDOW_END_PERCENT      (100U)

/* =========================================================================
 * 错误处理
 * ========================================================================= */
#define WDGM_DK_FAILURE_THRESHOLD           (3U)    /* 连续失败阈值 */

#endif /* WDGM_CFG_DK_H */
