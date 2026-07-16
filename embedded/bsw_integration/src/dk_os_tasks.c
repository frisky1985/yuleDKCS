/**
 * @file dk_os_tasks.c
 * @brief yuleDKCS AUTOSAR OS 任务入口实现
 *
 * 将 yuleDKCS (ICCE Digital Key) 的功能映射到 AUTOSAR OS 任务:
 *   - OsDkTask_Init:      ICCE 协议栈初始化
 *   - OsDkTask_10ms:      UWB 测距 / BLE 轮询 / 安全状态检查
 *   - OsDkTask_50ms:      边缘规则评估 / 区域变化检测
 *   - OsDkTask_100ms:     车辆状态同步 / 钥匙过期检查 / CAN 通信
 *   - OsDkTask_Background: 后台维护 / 日志 / 调试
 *
 * 命名遵守 dk_os_cfg.c 的 OsDkTask_*_Entry 约定
 */

#include "Os.h"
#include "Os_Cfg.h"
#include "EcuM.h"
#include "Wdgm.h"
#include "Com.h"
#include "Dcm.h"
#include "Dem.h"
#include "icce_digital_key.h"

/* Timer callbacks are external — provided by BSW alarm infrastructure */
extern void Os_Callback_Alarm(AlarmType AlarmID);

/* =========================================================================
 * OsDkTask_Init — 一次性初始化任务
 * ========================================================================= */
void OsDkTask_Init_Entry(void)
{
    /* EcuM 驱动初始化第三阶段 (SWC) 已在 StartupThree 完成 */
    /* 此处可以执行 OS 启动后的补充初始化 */

    /* 请求 EcuM RUN 模式 (防止 EcuM 进入 PostRun/Shutdown) */
    EcuM_RequestRUN(0U);

    /* 初始化完成后删除 Init 任务 (AUTOSAR 标准) */
    TerminateTask();
}

/* =========================================================================
 * OsDkTask_10ms — 10ms 周期任务 (高优先级)
 *
 * 负责:
 *   - UWB 测距数据读取与回调分发
 *   - BLE 数据轮询
 *   - 安全状态实时检查
 *   - WdgM 周期性触发
 * ========================================================================= */
void OsDkTask_10ms_Entry(void)
{
    /* === UWB 测距处理 === */
    /* 实际项目中此处调用 icce_uwb_get_ranging() 并分发数据 */

    /* === BLE 数据处理 === */

    /* === WdgM MainFunction (10ms 周期) === */
    Wdgm_MainFunction();

    /* === EcuM MainFunction (10ms 周期) === */
    EcuM_MainFunction();

    /* === BSW Phase 2: 诊断主函数 (10ms 周期) === */
    Dem_MainFunction();     /* DEM — DTC 老化/去抖处理 */
    Dcm_MainFunction();     /* DCM — UDS 协议处理 */

    /* 等待下一周期 (TaskType 为 extended 时可用 WaitEvent) */
    TerminateTask();
}

/* =========================================================================
 * OsDkTask_50ms — 50ms 周期任务 (中优先级)
 *
 * 负责:
 *   - 边缘规则评估 (icce_edge_evaluate)
 *   - 区域变化检测 (icce_zone_classify)
 *   - 离线决策重算
 * ========================================================================= */
void OsDkTask_50ms_Entry(void)
{
    /* === 区域状态评估 === */
    /* 从 UWB 最新测距结果中获取距离, 判断区域 */

    /* === 边缘计算规则评估 === */

    /* === BSW Phase 2: COM 接收处理 (50ms 周期) === */
    Com_MainFunctionRx();   /* COM — 接收 I-PDU 信号解包 */

    TerminateTask();
}

/* =========================================================================
 * OsDkTask_100ms — 100ms 周期任务 (低优先级)
 *
 * 负责:
 *   - 车辆状态同步 (icce_vehicle_get_status)
 *   - 钥匙过期检查 (icce_security_check_engine_start_perm)
 *   - CAN 消息收发
 *   - NvM 存储操作
 * ========================================================================= */
void OsDkTask_100ms_Entry(void)
{
    /* === 车辆状态轮询 === */
    /* icce_vehicle_get_status(&veh_status); */

    /* === 钥匙过期检查 === */

    /* === CAN 通信 / COM 发送处理 (100ms 周期) === */
    Com_MainFunctionTx();   /* COM — 周期性地发送 I-PDU */

    TerminateTask();
}

/* =========================================================================
 * OsDkTask_Background — 后台空闲任务 (最低优先级)
 *
 * 负责:
 *   - 低优先级维护
 *   - 调试日志输出
 *   - BLE 广播参数校准
 * ========================================================================= */
void OsDkTask_Background_Entry(void)
{
    /* 后台维护工作在空闲期间执行 */

    /* 注意: 背景任务应尽快返回, 不阻塞高优先级任务 */
    TerminateTask();
}
