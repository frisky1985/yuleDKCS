/**
 * @file dk_ecum_callouts.c
 * @brief yuleDKCS EcuM Callout 实现 (S32K312)
 *
 * EcuM 标准 callout 回调实现:
 *   - EcuM_StartupOne / Two / Three: 多阶段初始化
 *   - EcuM_GoSleep / GoHalt / GoPoll: 休眠处理
 *   - EcuM_WakeupRestart: 唤醒恢复
 *   - EcuM_GetShutdownTarget / GetShutdownTarget: 关闭目标管理
 *
 * 这些函数由 EcuM.c 内部的状态机调用。
 */

#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "Os.h"
#include "icce_digital_key.h"

/* =========================================================================
 * 内部状态
 * ========================================================================= */
static boolean g_ecum_startup_complete = FALSE;

/* =========================================================================
 * EcuM Callout: 多阶段启动
 * ========================================================================= */

/**
 * @brief EcuM 启动第一阶段 — MCU 基础初始化
 *
 * 在 OS 启动之前执行。
 * - 配置 MCU 时钟
 * - 初始化栈指针
 * - 初始化最低限度外设
 */
void EcuM_DriverInitOne(const EcuM_ConfigType *config)
{
    (void)config;

    /* S32K312 系统初始化 */
    /* Mcu_Init(&Mcu_Config);        — 配置时钟源 */
    /* Mcu_SetMode(MCU_NORMAL);      — 设置 MCU 模式 */

    /* S32K312 WDOG 初始关闭, 由 WdgM 在主函数启动后接管 */
    /* Wdt_Disable(); */
}

/**
 * @brief EcuM 启动第二阶段 — BSW 模块初始化
 *
 * OS 启动后执行。
 * - 初始化 MCAL 驱动 (DIO, CAN, GPT, ADC, SPI, I2C)
 * - 初始化 BSW 服务层入口
 */
void EcuM_DriverInitTwo(const EcuM_ConfigType *config)
{
    (void)config;

    /* MCAL 驱动初始化 (S32K312 PDUR 坐标系) */
    /* Dio_Init(&Dio_Config);
     * Can_Init(&Can_Config);
     * Can_SetBaudrate(...);
     * Gpt_Init(&Gpt_Config);          — 为 OS tick 提供时钟
     * Spi_Init(&Spi_Config);          — BLE/UWB/NFC SPI 通信
     * I2c_Init(&I2c_Config);          — 传感器等
     * Adc_Init(&Adc_Config);          — 模拟输入
     * Wdg_Init(&Wdg_Config);          — 看门狗硬件初始化
     */
}

/**
 * @brief EcuM 启动第三阶段 — SWC 初始化
 *
 * OS 运行后, WdgM 已启用.
 * - 初始化 ICCE 数字钥匙协议栈
 * - 启动 BLE 广播
 * - 初始化 UWB 测距会话
 */
void EcuM_DriverInitThree(const EcuM_ConfigType *config)
{
    (void)config;

    /* ICCE 协议栈初始化 */
    int32_t ret = icce_dk_init();
    if (ret == ICCE_OK)
    {
        g_ecum_startup_complete = TRUE;

        /* 请求 RUN 模式 — 保持 ECU 运行态 */
        EcuM_RequestRUN(0U);
    }
    else
    {
        /* 初始化失败, 记录诊断事件 */
        /* Dem_ReportErrorStatus(DEM_EVENT_ICCE_INIT_FAILED, DEM_EVENT_STATUS_FAILED); */
    }
}

/**
 * @brief EcuM 重启恢复 — 从休眠/复位后恢复
 */
void EcuM_DriverRestart(const EcuM_ConfigType *config)
{
    (void)config;

    /* 重新初始化 ICCE 协议栈 */
    icce_dk_init();
}

/* =========================================================================
 * EcuM Callout: 抽象层驱动
 * ========================================================================= */
void EcuM_AL_DriverInitOne(const EcuM_ConfigType *config)      { (void)config; }
void EcuM_AL_DriverInitTwo(const EcuM_ConfigType *config)
{
    (void)config;
    /* BLE/UWB/NFC 硬件抽象层初始化 */
    /* HAL_BLE_Init(); */
    /* HAL_UWB_Init(); */
    /* HAL_NFC_Init(); */
}
void EcuM_AL_DriverInitThree(const EcuM_ConfigType *config)
{
    (void)config;
    /* 安全服务初始化 */
    /* HAL_SEC_Init(); */
}
void EcuM_AL_DriverRestart(const EcuM_ConfigType *config)   { (void)config; }

/* =========================================================================
 * EcuM Callout: 电源/复位
 * ========================================================================= */
void EcuM_AL_SwitchOff(void)
{
    /* 关闭外设电源 */
    icce_ble_stop_adv();
    icce_ble_deinit();
    icce_uwb_deinit();

    /* S32K312: 进入待机模式 */
    /* SMC_SetPowerMode(SMC_STOP2); */
}

void EcuM_AL_Reset(EcuM_ResetType resetType)
{
    (void)resetType;
    /* S32K312 复位 */
    /* S32_SMC_Reset(); */
}

void EcuM_AL_EnterSleep(void)
{
    /* 保存唤醒上下文 */
    /* 配置唤醒源 */
    /* 进入 STOP 模式 */
    /* SMC_SetPowerMode(SMC_STOP1); */
}

void EcuM_AL_WakeupCheck(void)
{
    /* 读取唤醒原因寄存器 */
    /* uint32 wakeup_reason = SMC_GetWakeupReason(); */
}

void EcuM_AL_WakeupValidation(void)
{
    /* 验证唤醒源 */
    /* 必要时重初始化时钟 */
}

void EcuM_AL_WakeupReaction(void)
{
    /* 恢复操作 */
    icce_ble_start_adv();
}

/* =========================================================================
 * EcuM Callout: OS 挂钩
 * ========================================================================= */
void StartupHook(void)
{
    /* OS 启动后的钩子 */
    /* 可在此处执行轻量级初始化 */
}

void ShutdownHook(StatusType Error)
{
    (void)Error;
    /* OS 关闭前的钩子 */
    /* 执行紧急关闭动作 */
}

void ErrorHook(StatusType Error)
{
    (void)Error;
    /* OS/运行时错误的钩子 */
    /* 记录错误, 必要时触发 EcuM 关闭 */
}
