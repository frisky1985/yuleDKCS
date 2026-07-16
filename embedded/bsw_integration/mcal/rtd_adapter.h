/**
 * @file rtd_adapter.h
 * @brief NXP RTD Driver Integration Adapter — yuleDKCS S32K312
 *
 * 统一接口抽象层，位于 BSW/App 与 NXP Real-Time Drivers (RTD) 之间。
 *
 * ## 设计目标
 *
 * 1. 提供稳定、一致的 API 命名空间（RTD_ 前缀），屏蔽 RTD 版本差异
 * 2. 支持编译期桩/RTD 切换（通过 RTD_ENABLED 宏）
 * 3. 运行时初始化状态检查，防止未初始化访问
 * 4. 可插拔调试/追踪钩子
 * 5. 向下兼容现有 mcal_stubs（register-level 实现）
 *
 * ## 切换方式
 *
 * - 默认模式 (RTD_ENABLED=0): 透传至 mcal_stubs 的 register-level 实现
 * - RTD 模式 (RTD_ENABLED=1): 调用 NXP RTD 驱动接口
 *   - 需包含 RTD 头文件路径（见 README_RTD.md）
 *   - 编译时链接 RTD 库
 *
 * ## 依赖的头文件
 *
 * - Std_Types.h: AUTOSAR 标准类型
 * - Mcu.h / Dio.h / Port.h / Adc.h / Pwm.h / Gpt.h / Wdg.h: MCAL 接口
 *
 * @copyright YuleTech, 2026
 */

#ifndef RTD_ADAPTER_H
#define RTD_ADAPTER_H

/* ============================================================================
 * Includes
 * ============================================================================ */
#include "Std_Types.h"
#include "Compiler.h"      /* NULL_PTR, FUNC, P2VAR etc. */
#include "Mcu.h"
#include "Dio.h"
#include "Port.h"
#include "Adc.h"
#include "Pwm.h"
#include "Gpt.h"
#include "Wdg.h"
#include "Mcu_Cfg.h"       /* Mcu_ConfigType, Mcu_ModeType */
#include "string.h"        /* memset, memcpy */

/* ============================================================================
 * Compile-Time Configuration
 * ============================================================================
 * @def RTD_ENABLED
 * 将此宏定义为 1 以启用真实 NXP RTD 驱动接口。
 * 默认 0 —— 使用 mcal_stubs 的寄存器级桩实现。
 *
 * @def RTD_ASSERT_ENABLED
 * 启用运行时参数校验断言。默认 1。
 *
 * @def RTD_TRACE_ENABLED
 * 启用调试追踪（通过 printf 或 DET 回调）。默认 0。
 */
#ifndef RTD_ENABLED
#define RTD_ENABLED                 0U
#endif

#ifndef RTD_ASSERT_ENABLED
#define RTD_ASSERT_ENABLED          1U
#endif

#ifndef RTD_TRACE_ENABLED
#define RTD_TRACE_ENABLED           0U
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */
#define RTD_ADAPTER_VENDOR_ID       0x0001U   /* YuleTech */
#define RTD_ADAPTER_MODULE_ID       0x00F0U   /* RTD Adapter Module ID */
#define RTD_ADAPTER_SW_MAJOR_VERSION 1U
#define RTD_ADAPTER_SW_MINOR_VERSION 0U
#define RTD_ADAPTER_SW_PATCH_VERSION 0U

/* ============================================================================
 * Standard Return Codes
 * ============================================================================ */
#define RTD_E_OK                    E_OK
#define RTD_E_NOT_OK                ((Std_ReturnType)1U)
#define RTD_E_UNINIT                ((Std_ReturnType)2U)  /* 模块未初始化 */
#define RTD_E_INVALID_PARAM         ((Std_ReturnType)3U)  /* 无效参数 */

/* ============================================================================
 * Debug / Trace Helpers
 * ============================================================================ */
#if RTD_TRACE_ENABLED
#include <stdio.h>
#define RTD_TRACE(...)   do { printf("[RTD] " __VA_ARGS__); printf("\n"); } while(0)
#else
#define RTD_TRACE(...)   ((void)0)
#endif

#if RTD_ASSERT_ENABLED
#define RTD_ASSERT_VALID(cond, msg)  do { \
    if (!(cond)) { \
        RTD_TRACE("ASSERT fail: %s at %s:%d", msg, __FILE__, __LINE__); \
        return RTD_E_INVALID_PARAM; \
    } \
} while(0)
#define RTD_ASSERT_VOID(cond, msg)   do { \
    if (!(cond)) { \
        RTD_TRACE("ASSERT fail: %s at %s:%d", msg, __FILE__, __LINE__); \
        return; \
    } \
} while(0)
#else
#define RTD_ASSERT_VALID(cond, msg)  ((void)0)
#define RTD_ASSERT_VOID(cond, msg)   ((void)0)
#endif

/* ============================================================================
 * Type Forward Declarations (typedeffed via MCAL headers)
 * ============================================================================ */

/* ============================================================================
 * RTD Adapter Global State
 * ============================================================================ */
typedef enum {
    RTD_STATE_UNINIT = 0U,    /* 未初始化 */
    RTD_STATE_IDLE   = 1U,    /* 已初始化，空闲 */
    RTD_STATE_BUSY   = 2U,    /* 正在操作 */
    RTD_STATE_ERROR  = 3U     /* 错误状态 */
} Rtd_StateType;

/* ============================================================================
 * Module-Level Initialization Bitmask
 * ============================================================================ */
#define RTD_INIT_MCU    (1U << 0)
#define RTD_INIT_PORT   (1U << 1)
#define RTD_INIT_DIO    (1U << 2)
#define RTD_INIT_ADC    (1U << 3)
#define RTD_INIT_PWM    (1U << 4)
#define RTD_INIT_GPT    (1U << 5)
#define RTD_INIT_WDG    (1U << 6)

/* ============================================================================
 * API — RTD Adapter Management
 * ============================================================================ */

/**
 * @brief 获取 RTD Adapter 版本信息
 * @param VersionInfo 输出参数，接收版本信息
 * @return E_OK 成功
 */
Std_ReturnType Rtd_GetVersionInfo(Std_VersionInfoType *VersionInfo);

/**
 * @brief 获取 RTD Adapter 内部运行状态
 * @return 当前状态 (Rtd_StateType)
 */
Rtd_StateType Rtd_GetState(void);

/**
 * @brief 查询初始化位掩码
 * @return 当前已初始化的模块位掩码
 */
uint8 Rtd_GetInitMask(void);

/* ============================================================================
 * API — MCU Driver Wrappers
 * ============================================================================ */

/**
 * @brief 初始化 MCU 模块（时钟、PLL、复位原因）
 * @param ConfigPtr MCU 配置指针，NULL 时使用默认配置
 * @return RTD_E_OK 成功；RTD_E_INVALID_PARAM 参数无效
 */
Std_ReturnType Rtd_Mcu_Init(const Mcu_ConfigType *ConfigPtr);

/**
 * @brief 设置 MCU 运行模式
 * @param Mode 目标模式 (MCU_NORMAL / MCU_SLEEP / MCU_STOP)
 */
void Rtd_Mcu_SetMode(Mcu_ModeType Mode);

/**
 * @brief 获取当前 MCU 模式
 * @return 当前模式
 */
Mcu_ModeType Rtd_Mcu_GetMode(void);

/**
 * @brief 分配 PLL 时钟到各外设总线
 *
 * 必须在时钟配置完成后调用。在 RTD 模式下调用 RTD MCAL 的
 * Mcu_DistributePllClock()；桩模式下为无操作。
 */
void Rtd_Mcu_DistributePllClock(void);

/**
 * @brief 获取 MCU 复位原因
 * @return 复位原因编码（低字节表示复位源）
 *
 * 复位源定义（S32K312）:
 *   bit 0: POR (上电复位)
 *   bit 1: WDOG (看门狗复位)
 *   bit 2: SW (软件复位)
 *   bit 3: LVD (低电压检测)
 *   bit 4: EXTERNAL (外部复位引脚)
 */
uint8 Rtd_Mcu_GetResetReason(void);

/**
 * @brief 执行 MCU 系统复位
 * @note 此函数不返回
 */
void Rtd_Mcu_PerformReset(void);

/* ============================================================================
 * API — PORT Driver Wrappers
 * ============================================================================ */

/**
 * @brief 初始化全部 PORT 引脚配置
 * @param ConfigPtr Port 配置指针（不可为 NULL）
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Port_Init(const Port_ConfigType *ConfigPtr);

/**
 * @brief 设置单个引脚方向（输入/输出）
 * @param Pin       引脚编号
 * @param Direction 方向 (PORT_PIN_IN / PORT_PIN_OUT)
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);

/**
 * @brief 设置引脚复用功能模式
 * @param Pin  引脚编号
 * @param Mode 复用模式 (ALT0-ALT7)
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode);

/* ============================================================================
 * API — DIO Driver Wrappers
 * ============================================================================ */

/**
 * @brief 初始化 DIO 模块
 * @param ConfigPtr DIO 配置
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Dio_Init(const Dio_ConfigType *ConfigPtr);

/**
 * @brief 读取单个 GPIO 引脚电平
 * @param ChannelId 通道号（高4位=端口号，低4位=引脚号）
 * @return STD_HIGH 或 STD_LOW
 */
Dio_LevelType Rtd_Dio_ReadChannel(Dio_ChannelType ChannelId);

/**
 * @brief 写入单个 GPIO 引脚电平
 * @param ChannelId 通道号
 * @param Level     电平 (STD_HIGH / STD_LOW)
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);

/**
 * @brief 读取整个 GPIO 端口
 * @param PortId 端口号 (0-4 对应 PORTA-E)
 * @return 端口电平值（各引脚位）
 */
Dio_PortLevelType Rtd_Dio_ReadPort(Dio_PortType PortId);

/**
 * @brief 写入整个 GPIO 端口
 * @param PortId 端口号
 * @param Level  端口电平值
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);

/**
 * @brief 读取通道组
 * @param ChannelGroupIdPtr 通道组描述
 * @return 组电平值
 */
Dio_LevelType Rtd_Dio_ReadChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr);

/**
 * @brief 写入通道组
 * @param ChannelGroupIdPtr 通道组描述
 * @param Level             电平值
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Dio_WriteChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr, Dio_LevelType Level);

/* ============================================================================
 * API — ADC Driver Wrappers
 * ============================================================================ */

/**
 * @brief 初始化 ADC 模块
 * @param ConfigPtr ADC 配置
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Adc_Init(const Adc_ConfigType *ConfigPtr);

/**
 * @brief 启动指定 ADC 组的转换
 * @param Group ADC 组号
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Adc_StartGroupConversion(Adc_GroupType Group);

/**
 * @brief 读取 ADC 组转换结果
 * @param Group          组号
 * @param DataBufferPtr  输出缓冲区
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr);

/**
 * @brief 停止指定 ADC 组的转换
 * @param Group 组号
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Adc_StopGroupConversion(Adc_GroupType Group);

/**
 * @brief 获取 ADC 组状态
 * @param Group 组号
 * @return 组状态
 */
Adc_StatusType Rtd_Adc_GetGroupStatus(Adc_GroupType Group);

/* ============================================================================
 * API — PWM Driver Wrappers
 * ============================================================================ */

/**
 * @brief 初始化 PWM 模块
 * @param ConfigPtr PWM 配置
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Pwm_Init(const Pwm_ConfigType *ConfigPtr);

/**
 * @brief 设置 PWM 通道占空比
 * @param Channel PWM 通道
 * @param Duty    占空比 (0-10000 表示 0.00%-100.00%)
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Pwm_SetDutyCycle(Pwm_ChannelType Channel, Pwm_DutyCycleType Duty);

/**
 * @brief 同时设置 PWM 通道周期和占空比
 * @param Channel 通道
 * @param Period  周期
 * @param Duty    占空比
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Pwm_SetPeriodAndDuty(Pwm_ChannelType Channel, Pwm_PeriodType Period, Pwm_DutyCycleType Duty);

/**
 * @brief 启动 PWM 通道输出
 * @param Channel 通道
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Pwm_StartChannel(Pwm_ChannelType Channel);

/**
 * @brief 停止 PWM 通道输出
 * @param Channel 通道
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Pwm_StopChannel(Pwm_ChannelType Channel);

/**
 * @brief 获取 PWM 通道当前占空比
 * @param Channel 通道
 * @return 当前占空比
 */
Pwm_DutyCycleType Rtd_Pwm_GetDutyCycle(Pwm_ChannelType Channel);

/* ============================================================================
 * API — GPT Driver Wrappers
 * ============================================================================ */

/**
 * @brief 初始化 GPT 定时器模块
 * @param ConfigPtr GPT 配置
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Gpt_Init(const Gpt_ConfigType *ConfigPtr);

/**
 * @brief 启动定时器
 * @param Channel 定时器通道
 * @param Value   装载值（计数周期）
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value);

/**
 * @brief 停止定时器
 * @param Channel 通道
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Gpt_StopTimer(Gpt_ChannelType Channel);

/**
 * @brief 获取自启动以来经过的时间
 * @param Channel 通道
 * @return 已过去的时间（计数值）
 */
Gpt_ValueType Rtd_Gpt_GetTimeElapsed(Gpt_ChannelType Channel);

/**
 * @brief 获取剩余时间
 * @param Channel 通道
 * @return 剩余计数值
 */
Gpt_ValueType Rtd_Gpt_GetTimeRemaining(Gpt_ChannelType Channel);

/**
 * @brief 启用定时器中断通知
 * @param Channel 通道
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Gpt_EnableNotification(Gpt_ChannelType Channel);

/**
 * @brief 禁用定时器中断通知
 * @param Channel 通道
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Gpt_DisableNotification(Gpt_ChannelType Channel);

/* ============================================================================
 * API — WDG Driver Wrappers
 * ============================================================================ */

/**
 * @brief 初始化看门狗
 * @param ConfigPtr WDG 配置
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Wdg_Init(const Wdg_ConfigType *ConfigPtr);

/**
 * @brief 设置看门狗模式
 * @param Mode (WDGM_OFF / WDGM_SLOW / WDGM_FAST)
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Wdg_SetMode(Wdg_ModeType Mode);

/**
 * @brief 获取当前看门狗状态
 * @return 当前模式
 */
Wdg_ModeType Rtd_Wdg_GetMode(void);

/**
 * @brief 触发看门狗（喂狗）
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Wdg_Trigger(void);

/**
 * @brief 看门狗触发系统复位
 * @note 此函数不返回
 */
void Rtd_Wdg_PerformReset(void);

/**
 * @brief 获取 WDG 版本信息
 * @param VersionInfo 输出参数
 * @return RTD_E_OK 或错误码
 */
Std_ReturnType Rtd_Wdg_GetVersionInfo(Std_VersionInfoType *VersionInfo);

/* ============================================================================
 * API — Application-Level Convenience Wrappers
 * ============================================================================ */

/**
 * @brief 一次性初始化所有已使能的 MCAL 模块
 *
 * 按顺序初始化：MCU → PORT → DIO → ADC → PWM → GPT → WDG
 * 模块初始化后不会再重复初始化（受 init mask 保护）。
 *
 * @param McuConfig   MCU 配置（NULL 使用默认）
 * @param PortConfig  PORT 配置（不可为 NULL）
 * @param DioConfig   DIO 配置（可为 NULL 使用默认）
 * @param AdcConfig   ADC 配置（可为 NULL）
 * @param PwmConfig   PWM 配置（可为 NULL）
 * @param GptConfig   GPT 配置（可为 NULL）
 * @param WdgConfig   WDG 配置（可为 NULL）
 * @return RTD_E_OK 全部成功；否则第一个失败模块的错误码
 */
Std_ReturnType Rtd_InitAll(
    const Mcu_ConfigType  *McuConfig,
    const Port_ConfigType *PortConfig,
    const Dio_ConfigType  *DioConfig,
    const Adc_ConfigType  *AdcConfig,
    const Pwm_ConfigType  *PwmConfig,
    const Gpt_ConfigType  *GptConfig,
    const Wdg_ConfigType  *WdgConfig
);

/**
 * @brief 检查指定 MCAL 驱动是否为 RTD 模式
 * @return TRUE 表示编译为 RTD 模式；FALSE 表示使用桩模式
 */
boolean Rtd_IsRtdEnabled(void);

/**
 * @brief 输出 RTD 适配器运行时诊断信息
 *
 * 将当前状态打印到调试端口（或记录到 DET）。
 * 用于集成调试和确认。
 */
void Rtd_DumpDiagnostics(void);

#endif /* RTD_ADAPTER_H */
