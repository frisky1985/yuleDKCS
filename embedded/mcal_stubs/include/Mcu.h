/**
 * @file Mcu.h
 * @brief yuleDKCS MCU Driver — 将包含转发到 yuleASR MCAL 实现
 */
#ifndef _DK_MCU_H_
#define _DK_MCU_H_
#include "Std_Types.h"

/* ============================================================================
 * yuleASR MCAL MCU driver (S32K312 正式实现)
 * ============================================================================ */
#include "Mcu_Cfg.h"    /* Mcu_Cfg.h in same include directory */

/* 类型定义 (与 yuleASR 保持一致) */
typedef uint8 Mcu_ModeType;
typedef uint8 Mcu_ResetType;

/* MCAL 模式宏 */
#define MCU_NORMAL  0U
#define MCU_SLEEP   1U
#define MCU_STOP    2U

/* ============================================================================
 * API 声明
 * ============================================================================ */
Std_ReturnType Mcu_Init(const Mcu_ConfigType *ConfigPtr);
void           Mcu_SetMode(Mcu_ModeType Mode);
Mcu_ModeType   Mcu_GetMode(void);
void           Mcu_PerformReset(void);
void           Mcu_DistributePllClock(void);

#endif /* _DK_MCU_H_ */
