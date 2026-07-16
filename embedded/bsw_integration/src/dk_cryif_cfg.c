/**
 * @file dk_cryif_cfg.c
 * @brief yuleDKCS CryIf 配置 — 密钥/通道/作业映射
 *
 * CryIf 是 CSM 和底层 Crypto Driver (SE050/tinycrypt) 的桥梁
 * 本配置定义密钥 ID 到硬件通道的映射关系
 */

#include "CryIf.h"
#include "CryIf_Cfg.h"
#include "CryIf_Types.h"

/* ============================================================================
 * Channel 配置 (符合 CryIf_Types.h 结构)
 * ============================================================================ */
static const CryIf_ChannelCfgType Dk_CryIf_Channels[] = {
    {
        .cryIfChannelId   = 0U,
        .driverObjectIndex = 0U,
        .driverIndex      = 0U,
        .maxKeySize       = 256U,
        .maxJobSize       = 1024U
    },
    {
        .cryIfChannelId   = 1U,
        .driverObjectIndex = 1U,
        .driverIndex      = 0U,
        .maxKeySize       = 256U,
        .maxJobSize       = 1024U
    }
};

/* ============================================================================
 * 密钥配置 (符合 CryIf_Types.h 结构)
 * ============================================================================ */
static const CryIf_KeyCfgType Dk_CryIf_Keys[] = {
    {
        .cryIfKeyId       = 0U,
        .cryptoKeyId      = 0U,
        .driverIndex      = 0U,
        .securityLevel    = 3U   /* CRYIF_SEC_LEVEL_3 */
    },
    {
        .cryIfKeyId       = 1U,
        .cryptoKeyId      = 1U,
        .driverIndex      = 0U,
        .securityLevel    = 2U
    },
    {
        .cryIfKeyId       = 2U,
        .cryptoKeyId      = 2U,
        .driverIndex      = 0U,
        .securityLevel    = 1U
    },
    {
        .cryIfKeyId       = 3U,
        .cryptoKeyId      = 3U,
        .driverIndex      = 0U,
        .securityLevel    = 0U   /* CRYIF_SEC_LEVEL_NONE */
    }
};

/* ============================================================================
 * CryIf General Config (嵌套结构)
 * ============================================================================ */
static const CryIf_GeneralCfgType Dk_CryIf_GeneralCfg = {
    .maxChannels        = 2U,
    .maxKeys            = 4U,
    .maxJobs            = 8U,
    .versionInfoApi     = 1U,
    .keyElementCopyApi  = 1U,
    .keyValidCheckApi   = 1U,
    .channelConfig      = Dk_CryIf_Channels,
    .keyConfig          = Dk_CryIf_Keys
};

/* ============================================================================
 * CryIf 全局配置
 * ============================================================================ */
const CryIf_ConfigType CryIf_Config = {
    .generalConfig      = &Dk_CryIf_GeneralCfg,
    .numChannels        = 2U,
    .numKeys            = 4U
};
