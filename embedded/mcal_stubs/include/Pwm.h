/**
 * @file Pwm.h
 * @brief yuleDKCS PWM Driver — yuleASR MCAL 转发头文件
 */
#ifndef _DK_PWM_H_
#define _DK_PWM_H_

#include "Std_Types.h"

/* ============================================================================
 * yuleASR MCAL PWM 驱动 (S32K312 正式实现)
 * ============================================================================ */

/** @brief PWM 通道类型 */
typedef uint8 Pwm_ChannelType;

/** @brief PWM 占空比类型 (0-10000 表示 0.00%-100.00%) */
typedef uint16 Pwm_DutyCycleType;

/** @brief PWM 周期类型 */
typedef uint32 Pwm_PeriodType;

/** @brief PWM 极性 */
typedef enum {
    PWM_HIGH_ACTIVE  = 0U,
    PWM_LOW_ACTIVE   = 1U
} Pwm_PolarityType;

/** @brief PWM 通道配置 */
typedef struct {
    Pwm_ChannelType  Channel;
    Pwm_PeriodType   DefaultPeriod;
    Pwm_DutyCycleType DefaultDutyCycle;
    Pwm_PolarityType Polarity;
} Pwm_ChannelConfigType;

/** @brief PWM 配置 */
typedef struct {
    const Pwm_ChannelConfigType *Channels;
    uint16                       NumChannels;
    boolean                      DevErrorDetect;
} Pwm_ConfigType;

/* ============================================================================
 * API 声明
 * ============================================================================ */
void              Pwm_Init(const Pwm_ConfigType *ConfigPtr);
void              Pwm_SetDutyCycle(Pwm_ChannelType Channel, Pwm_DutyCycleType Duty);
void              Pwm_SetPeriodAndDuty(Pwm_ChannelType Channel, Pwm_PeriodType Period, Pwm_DutyCycleType Duty);
void              Pwm_StartChannel(Pwm_ChannelType Channel);
void              Pwm_StopChannel(Pwm_ChannelType Channel);
Pwm_DutyCycleType Pwm_GetDutyCycle(Pwm_ChannelType Channel);
void              Pwm_GetVersionInfo(Std_VersionInfoType *VersionInfo);

#endif /* _DK_PWM_H_ */
