/**
 * @file dk_os_cfg.c
 * @brief yuleDKCS AUTOSAR OS 配置表 (任务/警报/资源)
 *
 * 基于 yuleASR Os_Cfg.c 模板, 适配 yuleDKCS 应用需求:
 *   5 tasks: Init, 10ms, 50ms, 100ms, Background
 *   4 alarms: 10ms x2, 50ms, 100ms
 *   3 resources: BLE/UWB shared, NvM, CAN
 *
 * 编译时与 yuleASR 的 Os_Cfg.c 二选一 (通过 -DDK_OS_CONFIG)
 */

#include "Os.h"
#include "Os_Internal.h"
#include "Os_Cfg.h"
#include "Os_Cfg_Dk.h"
#include "EcuM_Cfg.h"          /* for ECUM_MAIN_FUNCTION_PERIOD */

/* =========================================================================
 * 任务入口声明
 * ========================================================================= */
extern void OsDkTask_Init_Entry(void);
extern void OsDkTask_10ms_Entry(void);
extern void OsDkTask_50ms_Entry(void);
extern void OsDkTask_100ms_Entry(void);
extern void OsDkTask_Background_Entry(void);

/* =========================================================================
 * 警报回调声明
 * ========================================================================= */
static void OsDkAlarm_10ms_Callback(void);
static void OsDkAlarm_50ms_Callback(void);
static void OsDkAlarm_100ms_Callback(void);
static void OsDkAlarm_EcuM_Callback(void);

/* =========================================================================
 * 任务配置表
 * ========================================================================= */
static Os_TaskConfigType Os_TaskConfigs[OS_DK_NUM_TASKS] = {
    /* Task 0: Init — ICCE 协议栈初始化 */
    {
        .TaskID             = OsDkTask_Init,
        .FreeRTOS_Task      = NULL,
        .FreeRTOS_EventGroup= NULL,
        .Priority           = OS_DK_PRIORITY_CRITICAL,
        .IsAutoStart        = TRUE,
        .IsExtended         = FALSE,
        .EntryPoint         = OsDkTask_Init_Entry
    },

    /* Task 1: 10ms 周期 — UWB/BLE 实时处理 */
    {
        .TaskID             = OsDkTask_10ms,
        .FreeRTOS_Task      = NULL,
        .FreeRTOS_EventGroup= NULL,
        .Priority           = OS_DK_PRIORITY_HIGH,
        .IsAutoStart        = TRUE,
        .IsExtended         = FALSE,
        .EntryPoint         = OsDkTask_10ms_Entry
    },

    /* Task 2: 50ms 周期 — 边缘规则/区域评估 */
    {
        .TaskID             = OsDkTask_50ms,
        .FreeRTOS_Task      = NULL,
        .FreeRTOS_EventGroup= NULL,
        .Priority           = OS_DK_PRIORITY_NORMAL,
        .IsAutoStart        = TRUE,
        .IsExtended         = FALSE,
        .EntryPoint         = OsDkTask_50ms_Entry
    },

    /* Task 3: 100ms 周期 — 车辆状态/CAN/s */
    {
        .TaskID             = OsDkTask_100ms,
        .FreeRTOS_Task      = NULL,
        .FreeRTOS_EventGroup= NULL,
        .Priority           = OS_DK_PRIORITY_LOW,
        .IsAutoStart        = TRUE,
        .IsExtended         = FALSE,
        .EntryPoint         = OsDkTask_100ms_Entry
    },

    /* Task 4: 背景任务 */
    {
        .TaskID             = OsDkTask_Background,
        .FreeRTOS_Task      = NULL,
        .FreeRTOS_EventGroup= NULL,
        .Priority           = OS_DK_PRIORITY_IDLE,
        .IsAutoStart        = TRUE,
        .IsExtended         = FALSE,
        .EntryPoint         = OsDkTask_Background_Entry
    }
};

/* =========================================================================
 * 警报配置表 (Alarm → Task 映射)
 * ========================================================================= */
static Os_AlarmConfigType Os_AlarmConfigs[OS_DK_NUM_ALARMS] = {
    /* Alarm 0: 10ms — 触发 10ms 任务 */
    {
        .AlarmID            = OsDkAlarm_10ms_Task,
        .Increment          = OS_DK_ALARM_PERIOD_10MS,
        .Cycle              = OS_DK_ALARM_PERIOD_10MS,
        .ExpiryTick         = 0U,
        .State              = OS_ALARM_UNUSED,
        .Callback           = OsDkAlarm_10ms_Callback,
        .FreeRTOS_Timer     = NULL
    },

    /* Alarm 1: 50ms — 触发 50ms 任务 */
    {
        .AlarmID            = OsDkAlarm_50ms_Task,
        .Increment          = OS_DK_ALARM_PERIOD_50MS,
        .Cycle              = OS_DK_ALARM_PERIOD_50MS,
        .ExpiryTick         = 0U,
        .State              = OS_ALARM_UNUSED,
        .Callback           = OsDkAlarm_50ms_Callback,
        .FreeRTOS_Timer     = NULL
    },

    /* Alarm 2: 100ms — 触发 100ms 任务 */
    {
        .AlarmID            = OsDkAlarm_100ms_Task,
        .Increment          = OS_DK_ALARM_PERIOD_100MS,
        .Cycle              = OS_DK_ALARM_PERIOD_100MS,
        .ExpiryTick         = 0U,
        .State              = OS_ALARM_UNUSED,
        .Callback           = OsDkAlarm_100ms_Callback,
        .FreeRTOS_Timer     = NULL
    },

    /* Alarm 3: 10ms — EcuM_MainFunction 周期触发 */
    {
        .AlarmID            = OsDkAlarm_EcuM_MainFunction,
        .Increment          = ECUM_MAIN_FUNCTION_PERIOD,
        .Cycle              = ECUM_MAIN_FUNCTION_PERIOD,
        .ExpiryTick         = 0U,
        .State              = OS_ALARM_UNUSED,
        .Callback           = OsDkAlarm_EcuM_Callback,
        .FreeRTOS_Timer     = NULL
    }
};

/* =========================================================================
 * 资源配置表
 * ========================================================================= */
static Os_ResourceConfigType Os_ResourceConfigs[OS_DK_NUM_RESOURCES] = {
    /* Resource 0: BLE/UWB 共享外设 */
    {
        .ResID              = OsDkRes_BleUwbShared,
        .FreeRTOS_Mutex     = NULL,
        .OwnerTask          = 0U,
        .NestCount          = 0U,
        .IsCeilingPriority  = FALSE,
        .CeilingPriority    = 0U
    },

    /* Resource 1: NvM 存储 */
    {
        .ResID              = OsDkRes_NvMBlock,
        .FreeRTOS_Mutex     = NULL,
        .OwnerTask          = 0U,
        .NestCount          = 0U,
        .IsCeilingPriority  = FALSE,
        .CeilingPriority    = 0U
    },

    /* Resource 2: CAN 总线 */
    {
        .ResID              = OsDkRes_CanBus,
        .FreeRTOS_Mutex     = NULL,
        .OwnerTask          = 0U,
        .NestCount          = 0U,
        .IsCeilingPriority  = FALSE,
        .CeilingPriority    = 0U
    }
};

/* =========================================================================
 * OS 全局状态初始化
 * ========================================================================= */
Os_GlobalStateType Os_GlobalState = {
    .IsRunning          = FALSE,
    .CurrentAppMode     = OSDEFAULTAPPMODE,
    .NumTasks           = OS_DK_NUM_TASKS,
    .NumAlarms          = OS_DK_NUM_ALARMS,
    .NumResources       = OS_DK_NUM_RESOURCES,
    .Tasks              = Os_TaskConfigs,
    .Alarms             = Os_AlarmConfigs,
    .Resources          = Os_ResourceConfigs
};

/* =========================================================================
 * 警报回调函数
 * ========================================================================= */

/* Alarm callback dispatcher — 在警报到期时被 OS 调用 */
void Os_Callback_Alarm(AlarmType AlarmID)
{
    switch (AlarmID)
    {
        case OsDkAlarm_10ms_Task:
            /* 激活 10ms 任务 */
            ActivateTask(OsDkTask_10ms);
            break;

        case OsDkAlarm_50ms_Task:
            ActivateTask(OsDkTask_50ms);
            break;

        case OsDkAlarm_100ms_Task:
            ActivateTask(OsDkTask_100ms);
            break;

        case OsDkAlarm_EcuM_MainFunction:
            /* EcuM_MainFunction 由 EcuM 内部调度 */
            /* 空操作, EcuM_MainFunction 在 10ms task 内调用 */
            break;

        default:
            /* 未知 Alarm ID — 无操作 */
            break;
    }
}

/* 各 Alarm 的具体回调包装 */
static void OsDkAlarm_10ms_Callback(void)   { Os_Callback_Alarm(OsDkAlarm_10ms_Task); }
static void OsDkAlarm_50ms_Callback(void)   { Os_Callback_Alarm(OsDkAlarm_50ms_Task); }
static void OsDkAlarm_100ms_Callback(void)  { Os_Callback_Alarm(OsDkAlarm_100ms_Task); }
static void OsDkAlarm_EcuM_Callback(void)   { Os_Callback_Alarm(OsDkAlarm_EcuM_MainFunction); }
