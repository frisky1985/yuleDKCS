/**
 * @file Port.h
 * @brief yuleDKCS Port Driver — yuleASR MCAL 转发头文件
 */
#ifndef _DK_PORT_H_
#define _DK_PORT_H_

#include "Std_Types.h"

/* ============================================================================
 * yuleASR MCAL Port 驱动 (S32K312 正式实现)
 * ============================================================================ */

/** @brief 引脚类型 */
typedef uint8 Port_PinType;

/** @brief 引脚模式类型 */
typedef uint8 Port_PinModeType;

/** @brief 引脚方向 */
typedef enum {
    PORT_PIN_IN  = 0U,
    PORT_PIN_OUT = 1U
} Port_PinDirectionType;

/** @brief 内部上拉/下拉 */
typedef enum {
    PORT_PIN_PULL_OFF  = 0U,
    PORT_PIN_PULL_UP   = 1U,
    PORT_PIN_PULL_DOWN = 2U
} Port_PinPullType;

/** @brief 驱动强度 */
typedef enum {
    PORT_PIN_DRIVE_LOW  = 0U,
    PORT_PIN_DRIVE_HIGH = 1U
} Port_PinDriveType;

/** @brief 引脚配置 */
typedef struct {
    Port_PinType         Pin;
    Port_PinModeType     Mode;
    Port_PinDirectionType Direction;
    Port_PinPullType     PullConfig;
    Port_PinDriveType    Drive;
    boolean              InitOn;
} Port_PinConfigType;

/** @brief Port 全局配置 */
typedef struct {
    const Port_PinConfigType *Pins;
    uint16                    NumPins;
    boolean                   DevErrorDetect;
} Port_ConfigType;

/* ============================================================================
 * API 声明
 * ============================================================================ */
void Port_Init(const Port_ConfigType *ConfigPtr);
void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode);
void Port_GetVersionInfo(Std_VersionInfoType *VersionInfo);

#endif /* _DK_PORT_H_ */
