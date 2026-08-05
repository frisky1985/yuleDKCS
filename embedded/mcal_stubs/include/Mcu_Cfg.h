/**
 * @file Mcu_Cfg.h
 * @brief yuleDKCS MCU Configuration — S32K312
 */
#ifndef _DK_MCU_CFG_H_
#define _DK_MCU_CFG_H_

#include "Std_Types.h"

/** @brief MCU 时钟配置 */
typedef struct {
    uint8 PllSource;       /* 0=IRC, 1=SXOSC, 2=FXOSC */
    uint32 PllOutputFreq;  /* Hz */
    uint32 CoreFreq;       /* Hz */
    uint32 BusFreq;        /* Hz */
    uint32 FlashFreq;      /* Hz */
} Mcu_ClockConfigType;

/** @brief MCU 复位原因 */
typedef struct {
    uint8 ResetCause;
} Mcu_ResetReasonType;

/** @brief MCU 全局配置 */
typedef struct {
    const Mcu_ClockConfigType *ClockConfig;
    const Mcu_ResetReasonType *ResetConfig;
    boolean PllEnabled;
} Mcu_ConfigType;

/* S32K312 默认时钟配置 */
#define MCU_PLL_SRC_SXOSC   1U
#define MCU_CORE_FREQ_120MHZ 120000000UL
#define MCU_BUS_FREQ_60MHZ   60000000UL
#define MCU_FLASH_FREQ_24MHZ 24000000UL

#endif /* _DK_MCU_CFG_H_ */
