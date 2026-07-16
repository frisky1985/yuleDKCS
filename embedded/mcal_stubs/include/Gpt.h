/**
 * @file Gpt.h
 * @brief yuleDKCS GPT Driver — yuleASR MCAL 转发头文件
 */
#ifndef _DK_GPT_H_
#define _DK_GPT_H_

#include "Std_Types.h"

/* ============================================================================
 * yuleASR MCAL GPT 驱动 (S32K312 正式实现)
 * ============================================================================ */

/** @brief GPT 通道类型 */
typedef uint8 Gpt_ChannelType;

/** @brief GPT 计数值类型 */
typedef uint32 Gpt_ValueType;

/** @brief GPT 模式 */
typedef enum {
    GPT_MODE_ONESHOT = 0U,
    GPT_MODE_CONTINUOUS = 1U
} Gpt_ModeType;

/** @brief GPT 通知使能 */
typedef enum {
    GPT_NOTIFICATION_DISABLE = 0U,
    GPT_NOTIFICATION_ENABLE  = 1U
} Gpt_NotificationType;

/** @brief GPT 通道配置 */
typedef struct {
    Gpt_ChannelType     Channel;
    Gpt_ModeType        Mode;
    Gpt_NotificationType Notification;
    Gpt_ValueType       DefaultPeriod;
} Gpt_ChannelConfigType;

/** @brief GPT 配置 */
typedef struct {
    const Gpt_ChannelConfigType *Channels;
    uint16                       NumChannels;
    boolean                      DevErrorDetect;
} Gpt_ConfigType;

/* ============================================================================
 * API 声明
 * ============================================================================ */
void           Gpt_Init(const Gpt_ConfigType *ConfigPtr);
void           Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value);
void           Gpt_StopTimer(Gpt_ChannelType Channel);
Gpt_ValueType  Gpt_GetTimeElapsed(Gpt_ChannelType Channel);
Gpt_ValueType  Gpt_GetTimeRemaining(Gpt_ChannelType Channel);
void           Gpt_EnableNotification(Gpt_ChannelType Channel);
void           Gpt_DisableNotification(Gpt_ChannelType Channel);
void           Gpt_GetVersionInfo(Std_VersionInfoType *VersionInfo);

#endif /* _DK_GPT_H_ */
