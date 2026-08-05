/**
 * @file main.c
 * @brief yuleDKCS BSW Phase 1 — 主入口: OS + EcuM + WdgM 集成
 *
 * 集成 yuleASR AUTOSAR BSW:
 *   - OS: FreeRTOS 封装的 OSEK AUTOSAR OS
 *   - EcuM: 多阶段启动/关闭/休眠状态机
 *   - WdgM: 看门狗管理/监督
 *
 * 目标 MCU: NXP S32K312
 * 应用负载: ICCE Digital Key Protocol Stack
 *           (BLE + UWB + 边缘计算 + 安全认证 + 车辆接口)
 */

#include "Os.h"
#include "Os_Cfg.h"
#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "WdgM.h"
#include "WdgM_Cfg.h"

/* Phase 2: BSW COM / DCM / DEM 头文件 */
#include "Com.h"
#include "Com_Cfg.h"
#include "Com_Cfg_Dk.h"
#include "Dcm.h"
#include "Dcm_Cfg_Dk.h"
#include "Dem.h"
#include "Dem_Cfg_Dk.h"
#include "PduR.h"
#include "PduR_Cfg.h"

/* Phase 3: BSW NvM / CSM / CryIf / MCAL 头文件 */
#include "NvM.h"
#include "NvM_Cfg.h"
#include "Csm.h"
#include "Csm_Cfg.h"
#include "CryIf.h"
#include "CryIf_Cfg.h"
#include "Mcu.h"
#include "Dio.h"
#include "Port.h"

/* 数字钥匙协议栈头文件 */
#include "icce_digital_key.h"

/* yuleDKCS OS 配置 (含 BLE/UWB 唤醒源定义) */
#include "Os_Cfg_Dk.h"

/* =========================================================================
 * 外部引用: OS 任务入口 (定义在 dk_os_tasks.c)
 * ========================================================================= */
extern void OsDkTask_Init_Entry(void);
extern void OsDkTask_10ms_Entry(void);
extern void OsDkTask_50ms_Entry(void);
extern void OsDkTask_100ms_Entry(void);
extern void OsDkTask_Background_Entry(void);

/* =========================================================================
 * EcuM 全局配置 (callout 回调由 EcuM 状态机调用)
 * ========================================================================= */
/** @brief 唤醒源配置: BLE(0x8000) + UWB(0x0020) + CAN(0x0040) + GPIO(0x2000) + POWER(0x0001) + RESET(0x0002) */
static const EcuM_WakeupSourceConfigType Dk_WakeupSourceConfigs[] = {
    { ECUM_WKSOURCE_POWER,          100U, TRUE, 50U },
    { ECUM_WKSOURCE_RESET,          100U, TRUE, 50U },
    { ECUM_WKSOURCE_TIMER,          100U, TRUE, 50U },
    { ECUM_WKSOURCE_CAN,            100U, TRUE, 50U },
    { ECUM_WKSOURCE_GPIO,           100U, TRUE, 50U },
    { ECUM_WKSOURCE_BLE,            200U, TRUE, 100U },   /* yuleDKCS BLE 唤醒 */
    { ECUM_WKSOURCE_UWB,            200U, TRUE, 100U },   /* yuleDKCS UWB 识别 */
};

/** @brief EcuM 全局配置实例 */
const EcuM_ConfigType EcuM_Config = {
    .WakeupSources        = Dk_WakeupSourceConfigs,
    .NumWakeupSources     = sizeof(Dk_WakeupSourceConfigs) / sizeof(Dk_WakeupSourceConfigs[0]),
    .ComMConfigEnabled    = TRUE,
    .NvmConfigEnabled     = TRUE,
    .WdgMConfigEnabled    = TRUE,
};

/* =========================================================================
 * EcuM 模式/请求管理
 * ========================================================================= */
static EcuM_UserType Dk_MainUser = 0U;   /* 主 RUN 请求用户 */

/* =========================================================================
 * EcuM Callout: 启动第一阶段 (MCU 最小初始化)
 * ========================================================================= */
void EcuM_DriverInitOne(const EcuM_ConfigType *config)
{
    (void)config;

    /* S32K312 最低限度的 MCU 初始化:
     *  - 时钟源: 外部晶振 -> PLL -> 系统时钟
     *  - 内核外设: SysTick, NVIC
     *  - 看门狗: 初始关闭, 由 WdgM 接管
     */
    /* Mcu_Init(&Mcu_Config); */
    /* Mcu_SetMode(MCU_NORMAL); */
}

/** @brief EcuM Callout: 启动第二阶段 (BSW 模块初始化) */
void EcuM_DriverInitTwo(const EcuM_ConfigType *config)
{
    (void)config;

    /* Phase 1: 初始化 MCAL 和 BSW 服务层 */
    /* Det_Init();              — 开发错误检测 (若启用) */

    /* Phase 3: MCAL 驱动初始化 */
    /* Mcu_Init(&Mcu_Config);   — MCU 时钟/PLL 初始化 */
    /* Port_Init(&Port_Config); — GPIO 引脚模式初始化 */

    /* Phase 3: NvM 初始化 (必须先于 CSM, 因为 CSM 依赖 NvM 存储密钥) */
    /* NvM_Init(&NvM_Config);   — NVRAM 管理器 */

    /* Phase 3: CryIf / CSM 初始化 */
    /* CryIf_Init(&CryIf_Config); — 密码接口 */
    /* Csm_Init(&Csm_Config);     — 加密服务管理器 */

    /* Phase 2: 初始化通信和诊断栈 */
    PduR_Init(&PduR_Config);    /* PDU Router — 必须先于 COM/DCM */
    Com_Init(&Com_Config);      /* COM — CAN 信号路由 */
    Dem_Init(&Dem_Config);      /* DEM — DTC 事件管理 (先于 DCM) */
    Dcm_Init(&Dcm_Config);      /* DCM — UDS 诊断栈 */

    Dcm_Start();  /* 启动 DCM 协议处理 */
}

/** @brief EcuM Callout: 启动第三阶段 (SWC 初始化) */
void EcuM_DriverInitThree(const EcuM_ConfigType *config)
{
    (void)config;

    /* Phase 2: 启动 DEM 操作周期 */
    Dem_SetOperationCycleState(DEM_OPCYC_IGNITION, DEM_CYCLE_STATE_START);
    Dem_SetOperationCycleState(DEM_OPCYC_POWER, DEM_CYCLE_STATE_START);

    /* Phase 3: 读取 NVRAM 数据到缓存 */
    /* NvM_ReadAll(); */

    /* Phase 3: 加载 CSM 密钥 (从 NVRAM 或 SE050) */
    /* Csm_RegisterCallback(CSM_JOB_ID_KEY_GENERATE, ...); */

    /* ICCE 数字钥匙协议栈初始化 */
    icce_dk_init();
}

/** @brief EcuM Callout: 重启恢复 */
void EcuM_DriverRestart(const EcuM_ConfigType *config)
{
    (void)config;
    /* 从休眠/复位恢复时的外设重初始化 */
    icce_ble_init();
    icce_uwb_init();
    icce_ble_start_adv();
}

/** @brief EcuM Callout: 抽象层 — 驱动初始化第一阶段 */
void EcuM_AL_DriverInitOne(const EcuM_ConfigType *config)
{
    (void)config;
    /* 抽象层: BLE/UWB/NFC 外设的上电时序 */
    /* HAL 层的 init 通常延迟到 StartupTwo */
}

/** @brief EcuM Callout: 抽象层 — 驱动初始化第二阶段 */
void EcuM_AL_DriverInitTwo(const EcuM_ConfigType *config)
{
    (void)config;
    /* 初始化 BLE 硬件抽象层 */
    /* HAL_BLE_Init(); */
    /* 初始化 UWB 硬件抽象层 */
    /* HAL_UWB_Init(); */
    /* 初始化 NFC 硬件抽象层 */
    /* HAL_NFC_Init(); */
    /* 初始化安全元件 */
    /* HAL_SEC_Init(); */
}

/** @brief EcuM Callout: 抽象层 — 驱动初始化第三阶段 */
void EcuM_AL_DriverInitThree(const EcuM_ConfigType *config)
{
    (void)config;
    /* 应用层安全服务初始化 */
    /* SEC_MGR_Init(); */
}

/** @brief EcuM Callout: 抽象层 — 重启恢复 */
void EcuM_AL_DriverRestart(const EcuM_ConfigType *config)
{
    (void)config;
}

/** @brief EcuM Callout: 切断电源 */
void EcuM_AL_SwitchOff(void)
{
    /* 关闭外设电源 */
    icce_ble_stop_adv();
    icce_ble_deinit();
    icce_uwb_deinit();
    /* HAL_PWR_SetState(POWER_STATE_OFF); */
}

/** @brief EcuM Callout: MCU 复位 */
void EcuM_AL_Reset(EcuM_ResetType resetType)
{
    (void)resetType;
    /* 执行系统复位 */
    /* NVIC_SystemReset(); */
}

/** @brief EcuM Callout: 进入休眠 */
void EcuM_AL_EnterSleep(void)
{
    /* 配置休眠模式:
     *   - 关闭不必要的外设时钟
     *   - 使能唤醒源 (BLE 低功耗扫描 / UWB 被动监听)
     *   - 进入 STOP 模式
     */
    /* HAL_PWR_SetState(POWER_STATE_DEEP_SLEEP); */
}

/** @brief EcuM Callout: 唤醒检查 */
void EcuM_AL_WakeupCheck(void)
{
    /* 检查是哪个唤醒源触发 */
    /* 从 PMU/SCG 寄存器读取唤醒原因 */
}

/** @brief EcuM Callout: 唤醒验证 */
void EcuM_AL_WakeupValidation(void)
{
    /* 验证唤醒源的合法性 */
    /* 重初始化必要的外设 */
}

/** @brief EcuM Callout: 唤醒后续动作 */
void EcuM_AL_WakeupReaction(void)
{
    /* 恢复 BLE 广播 / UWB 测距会话 */
    icce_ble_start_adv();
}

/* =========================================================================
 * 主入口: 启动 AUTOSAR BSW
 * ========================================================================= */
int main(void)
{
    /* 1. 最小启动: 关中断, 初始化堆栈 */
    DisableAllInterrupts();

    /* 2. EcuM 启动第一阶段: MCU 最小初始化 */
    EcuM_Init();

    /* 3. EcuM 将内部完成 StartupOne 并调用 StartOS()
     *    由 StartOS() 启动 FreeRTOS 调度器进入多任务环境 */

    /* 注意: 正常流程下不会到达此处 */
    return 0;
}
