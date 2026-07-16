/**
 * @file rtd_adapter.c
 * @brief RTD 适配层桩实现 — yuleDKCS S32K312
 *
 * 提供 RTD 适配器的默认实现，在 RTD_ENABLED==0 时透传至 mcal_stubs。
 * 当 RTD_ENABLED==1 时调用 NXP RTD 驱动原生接口。
 *
 * @copyright YuleTech, 2026
 */

#include "rtd_adapter.h"

/* ============================================================================
 * 内部状态
 * ============================================================================ */
static boolean  s_globalInitialized = FALSE;
static uint8    s_initMask          = 0U;
static Rtd_StateType s_state        = RTD_STATE_UNINIT;

/* 模块级状态缓存 */
static Mcu_ModeType  s_mcuMode  = MCU_NORMAL;
static Wdg_ModeType  s_wdgMode  = WDGM_OFF;

/* ============================================================================
 * RTD 模式编译标志 (用于运行时查询)
 * ============================================================================ */
static const boolean s_rtdEnabled = 
    #if RTD_ENABLED
        TRUE
    #else
        FALSE
    #endif
    ;

/* ============================================================================
 * 桩模式下 MCU 复位原因寄存器模拟
 * ============================================================================ */
#if !RTD_ENABLED
static uint8  s_simResetReason = 0x01U;   /* 默认 POR */

/* S32K312 RCM (Reset Control Module) 寄存器仿真 */
#define RCM_SRS_BASE    0x4007F000UL      /* 复位状态寄存器基址 */
#define RCM_SRS_OFFSET  0x00U             /* SRS 偏移 */
#define RCM_SRS_POR     (1U << 0)         /* Power-On Reset */
#define RCM_SRS_WDOG    (1U << 1)         /* Watchdog Reset */
#define RCM_SRS_SW      (1U << 2)         /* Software Reset */
#define RCM_SRS_LVD     (1U << 3)         /* Low-Voltage Detect */
#define RCM_SRS_EXT     (1U << 4)         /* External Pin Reset */
#endif /* !RTD_ENABLED */

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 标记模块已初始化
 */
static void Rtd_MarkInit(uint8 moduleBit)
{
    s_initMask |= moduleBit;
    s_state = RTD_STATE_IDLE;
    s_globalInitialized = TRUE;
}

/**
 * @brief 检查模块是否已初始化
 * @return TRUE=已初始化; FALSE=未初始化
 */
static boolean Rtd_IsModuleInit(uint8 moduleBit)
{
    return (s_initMask & moduleBit) != 0U;
}

/* ============================================================================
 * API 实现 — Adapter 管理层
 * ============================================================================ */

Std_ReturnType Rtd_GetVersionInfo(Std_VersionInfoType *VersionInfo)
{
    if (VersionInfo == NULL_PTR)
    {
        return RTD_E_INVALID_PARAM;
    }
    VersionInfo->vendorID         = RTD_ADAPTER_VENDOR_ID;
    VersionInfo->moduleID         = RTD_ADAPTER_MODULE_ID;
    VersionInfo->sw_major_version = RTD_ADAPTER_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = RTD_ADAPTER_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = RTD_ADAPTER_SW_PATCH_VERSION;
    return RTD_E_OK;
}

Rtd_StateType Rtd_GetState(void)
{
    return s_state;
}

uint8 Rtd_GetInitMask(void)
{
    return s_initMask;
}

boolean Rtd_IsRtdEnabled(void)
{
    return s_rtdEnabled;
}

/* ============================================================================
 * API 实现 — MCU
 * ============================================================================ */

Std_ReturnType Rtd_Mcu_Init(const Mcu_ConfigType *ConfigPtr)
{
    RTD_TRACE("Rtd_Mcu_Init()");

    if (Rtd_IsModuleInit(RTD_INIT_MCU))
    {
        RTD_TRACE("  MCU already initialized, skipping");
        return RTD_E_OK;
    }

#if RTD_ENABLED
    /* RTD 模式: 调用 NXP RTD MCAL Mcu_Init */
    Std_ReturnType ret = Mcu_Init(ConfigPtr);
    if (ret != E_OK)
    {
        s_state = RTD_STATE_ERROR;
        return RTD_E_NOT_OK;
    }
#else
    /* 桩模式: 调用现有寄存器级实现 */
    Mcu_Init(ConfigPtr);
#endif /* RTD_ENABLED */

    Rtd_MarkInit(RTD_INIT_MCU);
    RTD_TRACE("  MCU initialized OK");
    return RTD_E_OK;
}

void Rtd_Mcu_SetMode(Mcu_ModeType Mode)
{
    RTD_TRACE("Rtd_Mcu_SetMode(%d)", (int)Mode);

    s_mcuMode = Mode;

#if RTD_ENABLED
    Mcu_SetMode(Mode);
#else
    Mcu_SetMode(Mode);
#endif
}

Mcu_ModeType Rtd_Mcu_GetMode(void)
{
    return s_mcuMode;
}

void Rtd_Mcu_DistributePllClock(void)
{
    RTD_TRACE("Rtd_Mcu_DistributePllClock()");

#if RTD_ENABLED
    Mcu_DistributePllClock();
#else
    Mcu_DistributePllClock();
#endif
}

uint8 Rtd_Mcu_GetResetReason(void)
{
#if RTD_ENABLED && !defined(RTD_ADAPTER_SELF_TEST)
    /* RTD 模式下查询 RCM_SRS 寄存器 */
    volatile const uint32 *rcm_srs = 
        (volatile const uint32 *)RCM_SRS_BASE;
    return (uint8)(*rcm_srs & 0x1FU);
#else
    /* 桩模式下使用模拟值 */
    return s_simResetReason;
#endif
}

void Rtd_Mcu_PerformReset(void)
{
    RTD_TRACE("Rtd_Mcu_PerformReset() — system will reset");
    Mcu_PerformReset();
    /* 永不返回 */
}

/* ============================================================================
 * API 实现 — PORT
 * ============================================================================ */

Std_ReturnType Rtd_Port_Init(const Port_ConfigType *ConfigPtr)
{
    RTD_TRACE("Rtd_Port_Init()");

    RTD_ASSERT_VALID(ConfigPtr != NULL_PTR, "Port_ConfigPtr is NULL");

    if (Rtd_IsModuleInit(RTD_INIT_PORT))
    {
        RTD_TRACE("  PORT already initialized, skipping");
        return RTD_E_OK;
    }

#if RTD_ENABLED
    Port_Init(ConfigPtr);
#else
    Port_Init(ConfigPtr);
#endif

    Rtd_MarkInit(RTD_INIT_PORT);
    RTD_TRACE("  PORT initialized OK (%u pins)", (unsigned)ConfigPtr->NumPins);
    return RTD_E_OK;
}

Std_ReturnType Rtd_Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction)
{
    RTD_TRACE("Rtd_Port_SetPinDirection(pin=%u, dir=%d)", (unsigned)Pin, (int)Direction);

    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_PORT), "PORT not initialized");

#if RTD_ENABLED
    Port_SetPinDirection(Pin, Direction);
#else
    /* 桩模式: PORT PCR 寄存器中的方向由 Port_SetPinMode 管理 */
    (void)Pin;
    (void)Direction;
#endif

    return RTD_E_OK;
}

Std_ReturnType Rtd_Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode)
{
    RTD_TRACE("Rtd_Port_SetPinMode(pin=%u, mode=%d)", (unsigned)Pin, (int)Mode);

    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_PORT), "PORT not initialized");

#if RTD_ENABLED
    Port_SetPinMode(Pin, Mode);
#else
    Port_SetPinMode(Pin, Mode);
#endif

    return RTD_E_OK;
}

/* ============================================================================
 * API 实现 — DIO
 * ============================================================================ */

Std_ReturnType Rtd_Dio_Init(const Dio_ConfigType *ConfigPtr)
{
    RTD_TRACE("Rtd_Dio_Init()");

    if (Rtd_IsModuleInit(RTD_INIT_DIO))
    {
        return RTD_E_OK;
    }

#if RTD_ENABLED
    Dio_Init(ConfigPtr);
#else
    Dio_Init(ConfigPtr);
#endif

    Rtd_MarkInit(RTD_INIT_DIO);
    return RTD_E_OK;
}

Dio_LevelType Rtd_Dio_ReadChannel(Dio_ChannelType ChannelId)
{
    RTD_TRACE("Rtd_Dio_ReadChannel(ch=%u)", (unsigned)ChannelId);

    if (!Rtd_IsModuleInit(RTD_INIT_DIO))
    {
        return STD_LOW;
    }

#if RTD_ENABLED
    return Dio_ReadChannel(ChannelId);
#else
    return Dio_ReadChannel(ChannelId);
#endif
}

Std_ReturnType Rtd_Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level)
{
    RTD_TRACE("Rtd_Dio_WriteChannel(ch=%u, level=%d)", (unsigned)ChannelId, (int)Level);

    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_DIO), "DIO not initialized");

#if RTD_ENABLED
    Dio_WriteChannel(ChannelId, Level);
#else
    Dio_WriteChannel(ChannelId, Level);
#endif

    return RTD_E_OK;
}

Dio_PortLevelType Rtd_Dio_ReadPort(Dio_PortType PortId)
{
    RTD_TRACE("Rtd_Dio_ReadPort(port=%u)", (unsigned)PortId);

    if (!Rtd_IsModuleInit(RTD_INIT_DIO))
    {
        return 0U;
    }

#if RTD_ENABLED
    return Dio_ReadPort(PortId);
#else
    return Dio_ReadPort(PortId);
#endif
}

Std_ReturnType Rtd_Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level)
{
    RTD_TRACE("Rtd_Dio_WritePort(port=%u, level=0x%04X)", (unsigned)PortId, (unsigned)Level);

    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_DIO), "DIO not initialized");

#if RTD_ENABLED
    Dio_WritePort(PortId, Level);
#else
    Dio_WritePort(PortId, Level);
#endif

    return RTD_E_OK;
}

Dio_LevelType Rtd_Dio_ReadChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr)
{
    if (ChannelGroupIdPtr == NULL_PTR)
    {
        return STD_LOW;
    }

    if (!Rtd_IsModuleInit(RTD_INIT_DIO))
    {
        return STD_LOW;
    }

#if RTD_ENABLED
    return Dio_ReadChannelGroup(ChannelGroupIdPtr);
#else
    return Dio_ReadChannelGroup(ChannelGroupIdPtr);
#endif
}

Std_ReturnType Rtd_Dio_WriteChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr, Dio_LevelType Level)
{
    RTD_ASSERT_VALID(ChannelGroupIdPtr != NULL_PTR, "ChannelGroupIdPtr is NULL");
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_DIO), "DIO not initialized");

#if RTD_ENABLED
    Dio_WriteChannelGroup(ChannelGroupIdPtr, Level);
#else
    Dio_WriteChannelGroup(ChannelGroupIdPtr, Level);
#endif

    return RTD_E_OK;
}

/* ============================================================================
 * API 实现 — ADC
 * ============================================================================ */

Std_ReturnType Rtd_Adc_Init(const Adc_ConfigType *ConfigPtr)
{
    RTD_TRACE("Rtd_Adc_Init()");

    if (Rtd_IsModuleInit(RTD_INIT_ADC))
    {
        return RTD_E_OK;
    }

#if RTD_ENABLED
    Adc_Init(ConfigPtr);
#else
    Adc_Init(ConfigPtr);
#endif

    Rtd_MarkInit(RTD_INIT_ADC);
    return RTD_E_OK;
}

Std_ReturnType Rtd_Adc_StartGroupConversion(Adc_GroupType Group)
{
    RTD_TRACE("Rtd_Adc_StartGroupConversion(group=%u)", (unsigned)Group);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_ADC), "ADC not initialized");

#if RTD_ENABLED
    Adc_StartGroupConversion(Group);
#else
    Adc_StartGroupConversion(Group);
#endif

    return RTD_E_OK;
}

Std_ReturnType Rtd_Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr)
{
    RTD_ASSERT_VALID(DataBufferPtr != NULL_PTR, "DataBufferPtr is NULL");
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_ADC), "ADC not initialized");

#if RTD_ENABLED
    return Adc_ReadGroup(Group, DataBufferPtr);
#else
    return Adc_ReadGroup(Group, DataBufferPtr);
#endif
}

Std_ReturnType Rtd_Adc_StopGroupConversion(Adc_GroupType Group)
{
    RTD_TRACE("Rtd_Adc_StopGroupConversion(group=%u)", (unsigned)Group);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_ADC), "ADC not initialized");

#if RTD_ENABLED
    Adc_StopGroupConversion(Group);
#else
    Adc_StopGroupConversion(Group);
#endif

    return RTD_E_OK;
}

Adc_StatusType Rtd_Adc_GetGroupStatus(Adc_GroupType Group)
{
    if (!Rtd_IsModuleInit(RTD_INIT_ADC))
    {
        return 0U; /* ADC_IDLE */
    }

#if RTD_ENABLED
    return Adc_GetGroupStatus(Group);
#else
    return Adc_GetGroupStatus(Group);
#endif
}

/* ============================================================================
 * API 实现 — PWM
 * ============================================================================ */

Std_ReturnType Rtd_Pwm_Init(const Pwm_ConfigType *ConfigPtr)
{
    RTD_TRACE("Rtd_Pwm_Init()");

    if (Rtd_IsModuleInit(RTD_INIT_PWM))
    {
        return RTD_E_OK;
    }

#if RTD_ENABLED
    Pwm_Init(ConfigPtr);
#else
    Pwm_Init(ConfigPtr);
#endif

    Rtd_MarkInit(RTD_INIT_PWM);
    return RTD_E_OK;
}

Std_ReturnType Rtd_Pwm_SetDutyCycle(Pwm_ChannelType Channel, Pwm_DutyCycleType Duty)
{
    RTD_TRACE("Rtd_Pwm_SetDutyCycle(ch=%u, duty=%u)", (unsigned)Channel, (unsigned)Duty);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_PWM), "PWM not initialized");

#if RTD_ENABLED
    Pwm_SetDutyCycle(Channel, Duty);
#else
    Pwm_SetDutyCycle(Channel, Duty);
#endif

    return RTD_E_OK;
}

Std_ReturnType Rtd_Pwm_SetPeriodAndDuty(Pwm_ChannelType Channel, Pwm_PeriodType Period, Pwm_DutyCycleType Duty)
{
    RTD_TRACE("Rtd_Pwm_SetPeriodAndDuty(ch=%u, period=%u, duty=%u)", 
              (unsigned)Channel, (unsigned)Period, (unsigned)Duty);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_PWM), "PWM not initialized");

#if RTD_ENABLED
    Pwm_SetPeriodAndDuty(Channel, Period, Duty);
#else
    Pwm_SetPeriodAndDuty(Channel, Period, Duty);
#endif

    return RTD_E_OK;
}

Std_ReturnType Rtd_Pwm_StartChannel(Pwm_ChannelType Channel)
{
    RTD_TRACE("Rtd_Pwm_StartChannel(ch=%u)", (unsigned)Channel);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_PWM), "PWM not initialized");

#if RTD_ENABLED
    Pwm_StartChannel(Channel);
#else
    Pwm_StartChannel(Channel);
#endif

    return RTD_E_OK;
}

Std_ReturnType Rtd_Pwm_StopChannel(Pwm_ChannelType Channel)
{
    RTD_TRACE("Rtd_Pwm_StopChannel(ch=%u)", (unsigned)Channel);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_PWM), "PWM not initialized");

#if RTD_ENABLED
    Pwm_StopChannel(Channel);
#else
    Pwm_StopChannel(Channel);
#endif

    return RTD_E_OK;
}

Pwm_DutyCycleType Rtd_Pwm_GetDutyCycle(Pwm_ChannelType Channel)
{
    if (!Rtd_IsModuleInit(RTD_INIT_PWM))
    {
        return 0U;
    }

#if RTD_ENABLED
    return Pwm_GetDutyCycle(Channel);
#else
    return Pwm_GetDutyCycle(Channel);
#endif
}

/* ============================================================================
 * API 实现 — GPT
 * ============================================================================ */

Std_ReturnType Rtd_Gpt_Init(const Gpt_ConfigType *ConfigPtr)
{
    RTD_TRACE("Rtd_Gpt_Init()");

    if (Rtd_IsModuleInit(RTD_INIT_GPT))
    {
        return RTD_E_OK;
    }

#if RTD_ENABLED
    Gpt_Init(ConfigPtr);
#else
    Gpt_Init(ConfigPtr);
#endif

    Rtd_MarkInit(RTD_INIT_GPT);
    return RTD_E_OK;
}

Std_ReturnType Rtd_Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value)
{
    RTD_TRACE("Rtd_Gpt_StartTimer(ch=%u, val=%u)", (unsigned)Channel, (unsigned)Value);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_GPT), "GPT not initialized");

#if RTD_ENABLED
    Gpt_StartTimer(Channel, Value);
#else
    Gpt_StartTimer(Channel, Value);
#endif

    return RTD_E_OK;
}

Std_ReturnType Rtd_Gpt_StopTimer(Gpt_ChannelType Channel)
{
    RTD_TRACE("Rtd_Gpt_StopTimer(ch=%u)", (unsigned)Channel);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_GPT), "GPT not initialized");

#if RTD_ENABLED
    Gpt_StopTimer(Channel);
#else
    Gpt_StopTimer(Channel);
#endif

    return RTD_E_OK;
}

Gpt_ValueType Rtd_Gpt_GetTimeElapsed(Gpt_ChannelType Channel)
{
    if (!Rtd_IsModuleInit(RTD_INIT_GPT))
    {
        return 0U;
    }

#if RTD_ENABLED
    return Gpt_GetTimeElapsed(Channel);
#else
    return Gpt_GetTimeElapsed(Channel);
#endif
}

Gpt_ValueType Rtd_Gpt_GetTimeRemaining(Gpt_ChannelType Channel)
{
    if (!Rtd_IsModuleInit(RTD_INIT_GPT))
    {
        return 0U;
    }

#if RTD_ENABLED
    return Gpt_GetTimeRemaining(Channel);
#else
    return Gpt_GetTimeRemaining(Channel);
#endif
}

Std_ReturnType Rtd_Gpt_EnableNotification(Gpt_ChannelType Channel)
{
    RTD_TRACE("Rtd_Gpt_EnableNotification(ch=%u)", (unsigned)Channel);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_GPT), "GPT not initialized");

#if RTD_ENABLED
    Gpt_EnableNotification(Channel);
#else
    Gpt_EnableNotification(Channel);
#endif

    return RTD_E_OK;
}

Std_ReturnType Rtd_Gpt_DisableNotification(Gpt_ChannelType Channel)
{
    RTD_TRACE("Rtd_Gpt_DisableNotification(ch=%u)", (unsigned)Channel);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_GPT), "GPT not initialized");

#if RTD_ENABLED
    Gpt_DisableNotification(Channel);
#else
    Gpt_DisableNotification(Channel);
#endif

    return RTD_E_OK;
}

/* ============================================================================
 * API 实现 — WDG
 * ============================================================================ */

Std_ReturnType Rtd_Wdg_Init(const Wdg_ConfigType *ConfigPtr)
{
    RTD_TRACE("Rtd_Wdg_Init()");

    if (Rtd_IsModuleInit(RTD_INIT_WDG))
    {
        return RTD_E_OK;
    }

#if RTD_ENABLED
    Wdg_Init(ConfigPtr);
#else
    Wdg_Init(ConfigPtr);
#endif

    Rtd_MarkInit(RTD_INIT_WDG);
    return RTD_E_OK;
}

Std_ReturnType Rtd_Wdg_SetMode(Wdg_ModeType Mode)
{
    RTD_TRACE("Rtd_Wdg_SetMode(mode=%d)", (int)Mode);
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_WDG), "WDG not initialized");

    s_wdgMode = Mode;

#if RTD_ENABLED
    Wdg_SetMode(Mode);
#else
    Wdg_SetMode(Mode);
#endif

    return RTD_E_OK;
}

Wdg_ModeType Rtd_Wdg_GetMode(void)
{
    return s_wdgMode;
}

Std_ReturnType Rtd_Wdg_Trigger(void)
{
    RTD_TRACE("Rtd_Wdg_Trigger()");
    RTD_ASSERT_VALID(Rtd_IsModuleInit(RTD_INIT_WDG), "WDG not initialized");

    /*
     * 看门狗触发（喂狗）操作：
     * 对 WDOG_CNT 寄存器写入刷新序列 0xA602, 0xB480。
     * S32K312 WDOG 寄存器基址: 0x40052000
     *
     * 正式实现通过 RTD Wdg_Trigger 完成。
     */
#if RTD_ENABLED
    /* RTD 模式下 Wdg_Trigger() 由 NXP RTD 提供 */
    Wdg_SetMode(s_wdgMode);   /* 触发模式设置即隐含喂狗 */
#else
    Wdg_SetMode(s_wdgMode);
#endif

    return RTD_E_OK;
}

void Rtd_Wdg_PerformReset(void)
{
    RTD_TRACE("Rtd_Wdg_PerformReset() — WDG forced reset");
    Rtd_Mcu_PerformReset();
}

Std_ReturnType Rtd_Wdg_GetVersionInfo(Std_VersionInfoType *VersionInfo)
{
    if (VersionInfo == NULL_PTR)
    {
        return RTD_E_INVALID_PARAM;
    }

#if RTD_ENABLED
    Wdg_GetVersionInfo(VersionInfo);
#else
    Wdg_GetVersionInfo(VersionInfo);
#endif

    return RTD_E_OK;
}

/* ============================================================================
 * API 实现 — 高级便利函数
 * ============================================================================ */

Std_ReturnType Rtd_InitAll(
    const Mcu_ConfigType  *McuConfig,
    const Port_ConfigType *PortConfig,
    const Dio_ConfigType  *DioConfig,
    const Adc_ConfigType  *AdcConfig,
    const Pwm_ConfigType  *PwmConfig,
    const Gpt_ConfigType  *GptConfig,
    const Wdg_ConfigType  *WdgConfig)
{
    Std_ReturnType ret;

    RTD_TRACE("Rtd_InitAll() — %s mode", s_rtdEnabled ? "RTD" : "Stub");

    /* 1. MCU 初始化 — 必须最先 */
    ret = Rtd_Mcu_Init(McuConfig);
    if (ret != RTD_E_OK) return ret;

    /* 2. PORT 初始化 */
    ret = Rtd_Port_Init(PortConfig);
    if (ret != RTD_E_OK) return ret;

    /* 3. DIO 初始化 */
    ret = Rtd_Dio_Init(DioConfig);
    if (ret != RTD_E_OK) return ret;

    /* 4. ADC 初始化 */
    ret = Rtd_Adc_Init(AdcConfig);
    if (ret != RTD_E_OK) return ret;

    /* 5. PWM 初始化 */
    ret = Rtd_Pwm_Init(PwmConfig);
    if (ret != RTD_E_OK) return ret;

    /* 6. GPT 初始化 */
    ret = Rtd_Gpt_Init(GptConfig);
    if (ret != RTD_E_OK) return ret;

    /* 7. WDG 初始化 — 必须最后（看门狗一旦使能不可撤销） */
    ret = Rtd_Wdg_Init(WdgConfig);
    if (ret != RTD_E_OK) return ret;

    s_state = RTD_STATE_IDLE;
    RTD_TRACE("Rtd_InitAll() — all modules initialized OK");
    return RTD_E_OK;
}

void Rtd_DumpDiagnostics(void)
{
    /* 运行时诊断输出 — 用于集成调试 */
    Std_VersionInfoType ver;
    (void)Rtd_GetVersionInfo(&ver);

    RTD_TRACE("=== RTD Adapter Diagnostics ===");
    RTD_TRACE("Mode:        %s", s_rtdEnabled ? "RTD" : "Stub");
    RTD_TRACE("State:       %d", (int)s_state);
    RTD_TRACE("InitMask:    0x%02X", s_initMask);
    RTD_TRACE("MCU mode:    %d", (int)s_mcuMode);
    RTD_TRACE("WDG mode:    %d", (int)s_wdgMode);
    RTD_TRACE("Version:     %u.%u.%u (vendor=0x%04X, module=0x%04X)",
              (unsigned)ver.sw_major_version,
              (unsigned)ver.sw_minor_version,
              (unsigned)ver.sw_patch_version,
              (unsigned)ver.vendorID,
              (unsigned)ver.moduleID);
    RTD_TRACE("================================");
}

/* ============================================================================
 * 强制链接锚点：确保 rtd_adapter 库不被连接器丢弃
 * ============================================================================ */
void __attribute__((used)) _rtd_adapter_anchor(void)
{
    /* 引用关键函数，防止链接器优化删除 */
    __attribute__((unused)) volatile void *dummy = 
        (void*)Rtd_GetVersionInfo;
}
