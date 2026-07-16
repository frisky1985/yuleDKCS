/**
 * @file Os_Cfg_Dk.h
 * @brief yuleDKCS AUTOSAR OS 配置覆盖头文件
 *
 * 针对 yuleDKCS (ICCE Digital Key on S32K312) 优化 OS 配置:
 *   - 任务数: 7 (Init + 10ms + 50ms + 100ms + Background + BLE + UWB)
 *   - 警报数: 5 (10ms x2 + 50ms + 100ms + 后台维护)
 *   - 资源数: 3 (BLE/UWB 共享资源 + NvM + CAN)
 *   - 计数器: 单计数器, 1ms 精度
 */

#ifndef OS_CFG_DK_H
#define OS_CFG_DK_H

#include "Std_Types.h"

/* =========================================================================
 * 任务配置
 * ========================================================================= */
#define OS_DK_NUM_TASKS                     (5U)    /* Init + 10ms + 50ms + 100ms + Background */

/* Task IDs — 与 yuleDKCS 应用对应 */
#define OsDkTask_Init                       (0U)    /* ICCE 协议栈初始化 */
#define OsDkTask_10ms                       (1U)    /* UWB 测距 + BLE 轮询 + WdgM + EcuM */
#define OsDkTask_50ms                       (2U)    /* 边缘规则 + 区域评估 */
#define OsDkTask_100ms                      (3U)    /* 车辆状态 + CAN + 钥匙检查 */
#define OsDkTask_Background                 (4U)    /* 后台维护 */

/* 优先级 (数字越大优先级越高 — AUTOSAR OSEK 标准) */
#define OS_DK_PRIORITY_IDLE                 (1U)
#define OS_DK_PRIORITY_LOW                  (2U)
#define OS_DK_PRIORITY_NORMAL               (3U)
#define OS_DK_PRIORITY_HIGH                 (4U)
#define OS_DK_PRIORITY_CRITICAL             (5U)

/* =========================================================================
 * 警报配置
 * ========================================================================= */
#define OS_DK_NUM_ALARMS                    (4U)

/* Alarm IDs */
#define OsDkAlarm_10ms_Task                 (0U)    /* 触发 OsDkTask_10ms */
#define OsDkAlarm_50ms_Task                 (1U)    /* 触发 OsDkTask_50ms */
#define OsDkAlarm_100ms_Task                (2U)    /* 触发 OsDkTask_100ms */
#define OsDkAlarm_EcuM_MainFunction         (3U)    /* EcuM_MainFunction 周期触发 */

#define OS_DK_ALARM_PERIOD_10MS             (10U)
#define OS_DK_ALARM_PERIOD_50MS             (50U)
#define OS_DK_ALARM_PERIOD_100MS            (100U)

/* =========================================================================
 * 资源配置
 * ========================================================================= */
#define OS_DK_NUM_RESOURCES                 (3U)

/* Resource IDs */
#define OsDkRes_BleUwbShared                (0U)    /* BLE/UWB 共享外设 */
#define OsDkRes_NvMBlock                    (1U)    /* NvM 非易失存储 */
#define OsDkRes_CanBus                      (2U)    /* CAN 总线 */

/* =========================================================================
 * 计数器配置
 * ========================================================================= */
#define OS_DK_COUNTER_TICKS_PER_MS          (1U)
#define OS_DK_COUNTER_MAX_ALLOWED           (0xFFFFFFFFU)
#define OS_DK_COUNTER_MIN_CYCLE             (1U)

/* =========================================================================
 * 栈大小配置
 * ========================================================================= */
#define OS_DK_TASK_STACK_SIZE               (4096U)     /* 4KB 每任务, 栈溢出防护 */
#define OS_DK_ISR_STACK_SIZE                (2048U)     /* 8KB ISR stack (配置见链接脚本) */

/* =========================================================================
 * 兼容性定义 (OS_TimingProtection 等需要)
 * ========================================================================= */
#define OS_TASK_COUNT                       (OS_DK_NUM_TASKS)
#define OS_ALARM_COUNT                      (OS_DK_NUM_ALARMS)
#define OS_RESOURCE_COUNT                   (OS_DK_NUM_RESOURCES)

/* =========================================================================
 * 唤醒源配置 (yuleDKCS 专用)
 * ========================================================================= */
#define ECUM_WKSOURCE_BLE                   0x00008000u
#define ECUM_WKSOURCE_UWB                   0x00010000u

#endif /* OS_CFG_DK_H */
