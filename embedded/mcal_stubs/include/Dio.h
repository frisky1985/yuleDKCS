/**
 * @file Dio.h
 * @brief yuleDKCS DIO Driver — yuleASR MCAL 转发头文件
 */
#ifndef _DK_DIO_H_
#define _DK_DIO_H_

#include "Std_Types.h"

/* ============================================================================
 * yuleASR MCAL DIO 驱动 (S32K312 正式实现)
 * ============================================================================ */

/** @brief DIO 通道类型 */
typedef uint8 Dio_ChannelType;

/** @brief DIO 端口类型 */
typedef uint8 Dio_PortType;

/** @brief DIO 电平类型 */
typedef uint8 Dio_LevelType;

/** @brief DIO 端口电平类型 */
typedef uint16 Dio_PortLevelType;

/** @brief DIO 通道组 */
typedef struct {
    Dio_PortType      Port;
    Dio_ChannelType   Offset;
    uint8             Mask;
} Dio_ChannelGroupType;

/** @brief 通道配置 */
typedef struct {
    Dio_ChannelType  Channel;
    Dio_LevelType    DefaultLevel;
} Dio_ChannelConfigType;

/** @brief DIO 配置 */
typedef struct {
    const Dio_ChannelConfigType *Channels;
    uint16                       NumChannels;
    boolean                      DevErrorDetect;
} Dio_ConfigType;

/* 电平定义 */
#ifndef STD_HIGH
#define STD_HIGH   1U
#endif
#ifndef STD_LOW
#define STD_LOW    0U
#endif

/* ============================================================================
 * API 声明
 * ============================================================================ */
Dio_LevelType     Dio_ReadChannel(Dio_ChannelType ChannelId);
void              Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);
void              Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);
Dio_LevelType     Dio_ReadChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr);
void              Dio_WriteChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr, Dio_LevelType Level);
void              Dio_GetVersionInfo(Std_VersionInfoType *VersionInfo);
void              Dio_Init(const Dio_ConfigType *ConfigPtr);

#endif /* _DK_DIO_H_ */
