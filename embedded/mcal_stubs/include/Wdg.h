/**
 * @file Wdg.h
 * @brief yuleDKCS WDG Driver — yuleASR MCAL 转发头文件
 */
#ifndef _DK_WDG_H_
#define _DK_WDG_H_

#include "Std_Types.h"

/* ============================================================================
 * yuleASR MCAL WDG 驱动 (S32K312 正式实现)
 * ============================================================================ */

/** @brief WDG 模式 */
typedef enum {
    WDGM_OFF = 0U,
    WDGM_SLOW = 1U,
    WDGM_FAST = 2U
} Wdg_ModeType;

/** @brief WDG 通道配置 */
typedef struct {
    uint8        ChannelId;
    Wdg_ModeType DefaultMode;
    uint32       TimeoutMs;
} Wdg_ChannelConfigType;

/** @brief WDG 配置 */
typedef struct {
    const Wdg_ChannelConfigType *Channels;
    uint16                       NumChannels;
    boolean                      DevErrorDetect;
} Wdg_ConfigType;

/* ============================================================================
 * API 声明
 * ============================================================================ */
void           Wdg_Init(const Wdg_ConfigType *ConfigPtr);
void           Wdg_SetMode(Wdg_ModeType Mode);
Wdg_ModeType   Wdg_GetMode(void);
void           Wdg_PerformReset(void);
void           Wdg_GetVersionInfo(Std_VersionInfoType *VersionInfo);

#endif /* _DK_WDG_H_ */
