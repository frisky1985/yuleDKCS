/**
 * @file Adc.h
 * @brief yuleDKCS ADC Driver — yuleASR MCAL 转发头文件
 */
#ifndef _DK_ADC_H_
#define _DK_ADC_H_

#include "Std_Types.h"

/* ============================================================================
 * yuleASR MCAL ADC 驱动 (S32K312 正式实现)
 * ============================================================================ */

/** @brief ADC 组类型 */
typedef uint8 Adc_GroupType;

/** @brief ADC 值类型 */
typedef uint16 Adc_ValueGroupType;

/** @brief ADC 状态类型 */
typedef uint8 Adc_StatusType;

/** @brief ADC 分辨率 */
typedef enum {
    ADC_RES_8_BIT  = 0U,
    ADC_RES_10_BIT = 1U,
    ADC_RES_12_BIT = 2U
} Adc_ResolutionType;

/** @brief ADC 通道配置 */
typedef struct {
    Adc_GroupType   Group;
    uint8           Channel;
    Adc_ResolutionType Resolution;
} Adc_ChannelConfigType;

/** @brief ADC 配置 */
typedef struct {
    const Adc_ChannelConfigType *Channels;
    uint16                       NumChannels;
    boolean                      DevErrorDetect;
} Adc_ConfigType;

/* ============================================================================
 * API 声明
 * ============================================================================ */
void              Adc_Init(const Adc_ConfigType *ConfigPtr);
void              Adc_StartGroupConversion(Adc_GroupType Group);
void              Adc_StopGroupConversion(Adc_GroupType Group);
Std_ReturnType    Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr);
void              Adc_GetVersionInfo(Std_VersionInfoType *VersionInfo);
Adc_StatusType    Adc_GetGroupStatus(Adc_GroupType Group);

#endif /* _DK_ADC_H_ */
