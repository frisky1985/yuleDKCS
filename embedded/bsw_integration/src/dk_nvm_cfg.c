/**
 * @file dk_nvm_cfg.c
 * @brief yuleDKCS NVRAM 块配置
 *
 * 定义 NvM 模块的 NVRAM 块描述符:
 *   - ICCE 钥匙配置 (密钥/证书数据) — 掉电保存
 *   - 校准数据 (BLE/UWB 参数)
 *   - DTC 冻结帧 (DEM 故障事件关联)
 *
 * 符合 AUTOSAR 4.x NvM 模块接口
 */

#include "NvM.h"
#include "NvM_Cfg.h"
#include "EcuM.h"
#include "Dem.h"

/* ============================================================================
 * NVRAM 块 ROM 缺省数据 (Flash 中的初始值)
 * ============================================================================ */

/** @brief NVRAM Block 1: ICCE 钥匙配置 (64 bytes) */
static const uint8 Dk_NvBlock_KeyConfig[64] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/** @brief NVRAM Block 2: 校准数据 (256 bytes) */
static const uint8 Dk_NvBlock_Calibration[256] = {0};

/** @brief NVRAM Block 3: DTC 冻结帧 (512 bytes) */
static const uint8 Dk_NvBlock_FaultMemory[512] = {0};

/** @brief NVRAM Block 4: BLE 绑定信息 (64 bytes) */
static const uint8 Dk_NvBlock_BleBondData[64] = {0};

/** @brief NVRAM Block 5: UWB 配置数据 (128 bytes) */
static const uint8 Dk_NvBlock_UwbConfig[128] = {0};

/** @brief NVRAM Block 6: 会话计数器 (8 bytes) */
static const uint8 Dk_NvBlock_SessionCounter[8] = {0};

/* ============================================================================
 * NVRAM 块 RAM 镜像 (掉电可写缓存)
 * ============================================================================ */

/** @brief RAM 镜像 — 钥匙配置 */
static uint8 Dk_Ram_KeyConfig[64] = {0};

/** @brief RAM 镜像 — 校准数据 */
static uint8 Dk_Ram_Calibration[256] = {0};

/** @brief RAM 镜像 — DTC 冻结帧 */
static uint8 Dk_Ram_FaultMemory[512] = {0};

/** @brief RAM 镜像 — BLE 绑定信息 */
static uint8 Dk_Ram_BleBondData[64] = {0};

/** @brief RAM 镜像 — UWB 配置 */
static uint8 Dk_Ram_UwbConfig[128] = {0};

/** @brief RAM 镜像 — 会话计数器 */
static uint8 Dk_Ram_SessionCounter[8] = {0};

/* ============================================================================
 * 回调函数
 * ============================================================================ */

/** @brief 钥匙配置块 JobEnd 回调 */
static void Dk_KeyConfig_JobEndCallback(void)
{
    /* NVRAM 读写完成后的通知 — 当前暂无需要 */
}

/** @brief 校准数据块 Init 回调 */
static void Dk_Calibration_InitCallback(void)
{
    /* 校准数据初始化后, 加载到应用 */
}

/* ============================================================================
 * NVRAM 块描述符表
 * ============================================================================ */

static const NvM_BlockDescriptorType Dk_NvBlockDescriptors[] = {
    {
        .BlockId          = NVM_BLOCK_ID_CONFIG,
        .DeviceId         = 0U,
        .BlockBaseNumber  = 0U,
        .ManagementType   = NVM_BLOCK_NATIVE,
        .NumberOfNvBlocks = 1U,
        .NumberOfDataSets = 1U,
        .NvBlockLength    = sizeof(Dk_NvBlock_KeyConfig),
        .NvBlockNum       = 0U,
        .RomBlockNum      = 0U,
        .InitCallback     = NULL_PTR,
        .JobEndCallback   = Dk_KeyConfig_JobEndCallback,
        .CrcType          = NVM_CRC_16,
        .BlockUseCrc      = TRUE,
        .BlockUseSetRamBlockStatus = FALSE,
        .BlockWriteProt   = FALSE,
        .BlockWriteOnce   = FALSE,
        .BlockAutoValidation      = TRUE,
        .BlockUseMirror           = FALSE,
        .BlockUseCompression      = FALSE,
        .RomBlockData     = Dk_NvBlock_KeyConfig,
        .RamBlockData     = Dk_Ram_KeyConfig,
        .MirrorBlockData  = NULL_PTR
    },
    {
        .BlockId          = NVM_BLOCK_ID_CALIBRATION,
        .DeviceId         = 0U,
        .BlockBaseNumber  = 1U,
        .ManagementType   = NVM_BLOCK_NATIVE,
        .NumberOfNvBlocks = 1U,
        .NumberOfDataSets = 1U,
        .NvBlockLength    = sizeof(Dk_NvBlock_Calibration),
        .NvBlockNum       = 0U,
        .RomBlockNum      = 0U,
        .InitCallback     = Dk_Calibration_InitCallback,
        .JobEndCallback   = NULL_PTR,
        .CrcType          = NVM_CRC_16,
        .BlockUseCrc      = TRUE,
        .BlockUseSetRamBlockStatus = FALSE,
        .BlockWriteProt   = FALSE,
        .BlockWriteOnce   = FALSE,
        .BlockAutoValidation      = TRUE,
        .BlockUseMirror           = FALSE,
        .BlockUseCompression      = FALSE,
        .RomBlockData     = Dk_NvBlock_Calibration,
        .RamBlockData     = Dk_Ram_Calibration,
        .MirrorBlockData  = NULL_PTR
    },
    {
        .BlockId          = NVM_BLOCK_ID_FAULT_MEMORY,
        .DeviceId         = 0U,
        .BlockBaseNumber  = 2U,
        .ManagementType   = NVM_BLOCK_NATIVE,
        .NumberOfNvBlocks = 1U,
        .NumberOfDataSets = 1U,
        .NvBlockLength    = sizeof(Dk_NvBlock_FaultMemory),
        .NvBlockNum       = 0U,
        .RomBlockNum      = 0U,
        .InitCallback     = NULL_PTR,
        .JobEndCallback   = NULL_PTR,
        .CrcType          = NVM_CRC_32,
        .BlockUseCrc      = TRUE,
        .BlockUseSetRamBlockStatus = FALSE,
        .BlockWriteProt   = FALSE,
        .BlockWriteOnce   = FALSE,
        .BlockAutoValidation      = TRUE,
        .BlockUseMirror           = FALSE,
        .BlockUseCompression      = FALSE,
        .RomBlockData     = Dk_NvBlock_FaultMemory,
        .RamBlockData     = Dk_Ram_FaultMemory,
        .MirrorBlockData  = NULL_PTR
    },
    {
        .BlockId          = NVM_BLOCK_ID_USER_DATA_1,
        .DeviceId         = 0U,
        .BlockBaseNumber  = 3U,
        .ManagementType   = NVM_BLOCK_NATIVE,
        .NumberOfNvBlocks = 1U,
        .NumberOfDataSets = 1U,
        .NvBlockLength    = sizeof(Dk_NvBlock_BleBondData),
        .NvBlockNum       = 0U,
        .RomBlockNum      = 0U,
        .InitCallback     = NULL_PTR,
        .JobEndCallback   = NULL_PTR,
        .CrcType          = NVM_CRC_16,
        .BlockUseCrc      = TRUE,
        .BlockUseSetRamBlockStatus = FALSE,
        .BlockWriteProt   = FALSE,
        .BlockWriteOnce   = FALSE,
        .BlockAutoValidation      = TRUE,
        .BlockUseMirror           = FALSE,
        .BlockUseCompression      = FALSE,
        .RomBlockData     = Dk_NvBlock_BleBondData,
        .RamBlockData     = Dk_Ram_BleBondData,
        .MirrorBlockData  = NULL_PTR
    },
    {
        .BlockId          = NVM_BLOCK_ID_USER_DATA_2,
        .DeviceId         = 0U,
        .BlockBaseNumber  = 4U,
        .ManagementType   = NVM_BLOCK_NATIVE,
        .NumberOfNvBlocks = 1U,
        .NumberOfDataSets = 1U,
        .NvBlockLength    = sizeof(Dk_NvBlock_UwbConfig),
        .NvBlockNum       = 0U,
        .RomBlockNum      = 0U,
        .InitCallback     = NULL_PTR,
        .JobEndCallback   = NULL_PTR,
        .CrcType          = NVM_CRC_16,
        .BlockUseCrc      = TRUE,
        .BlockUseSetRamBlockStatus = FALSE,
        .BlockWriteProt   = FALSE,
        .BlockWriteOnce   = FALSE,
        .BlockAutoValidation      = TRUE,
        .BlockUseMirror           = FALSE,
        .BlockUseCompression      = FALSE,
        .RomBlockData     = Dk_NvBlock_UwbConfig,
        .RamBlockData     = Dk_Ram_UwbConfig,
        .MirrorBlockData  = NULL_PTR
    },
    {
        .BlockId          = NVM_BLOCK_ID_RESERVED,
        .DeviceId         = 0U,
        .BlockBaseNumber  = 5U,
        .ManagementType   = NVM_BLOCK_NATIVE,
        .NumberOfNvBlocks = 1U,
        .NumberOfDataSets = 1U,
        .NvBlockLength    = sizeof(Dk_NvBlock_SessionCounter),
        .NvBlockNum       = 0U,
        .RomBlockNum      = 0U,
        .InitCallback     = NULL_PTR,
        .JobEndCallback   = NULL_PTR,
        .CrcType          = NVM_CRC_8,
        .BlockUseCrc      = TRUE,
        .BlockUseSetRamBlockStatus = FALSE,
        .BlockWriteProt   = FALSE,
        .BlockWriteOnce   = FALSE,
        .BlockAutoValidation      = TRUE,
        .BlockUseMirror           = FALSE,
        .BlockUseCompression      = FALSE,
        .RomBlockData     = Dk_NvBlock_SessionCounter,
        .RamBlockData     = Dk_Ram_SessionCounter,
        .MirrorBlockData  = NULL_PTR
    }
};

/* ============================================================================
 * NvM 全局配置实例
 * ============================================================================ */
const NvM_ConfigType NvM_Config = {
    .BlockDescriptors       = Dk_NvBlockDescriptors,
    .NumBlockDescriptors    = (uint16)(sizeof(Dk_NvBlockDescriptors) / sizeof(Dk_NvBlockDescriptors[0])),
    .NumOfNvBlocks          = 6U,
    .NumOfDataSets          = 6U,
    .NumOfRomBlocks         = 6U,
    .MaxNumberOfWriteRetries = 3U,
    .MaxNumberOfReadRetries  = 3U,
    .DevErrorDetect          = TRUE,
    .VersionInfoApi          = TRUE,
    .SetRamBlockStatusApi    = TRUE,
    .GetErrorStatusApi       = TRUE,
    .SetBlockProtectionApi   = TRUE,
    .GetBlockProtectionApi   = FALSE,
    .SetDataIndexApi         = TRUE,
    .GetDataIndexApi         = FALSE,
    .CancelJobApi            = TRUE,
    .KillWriteAllApi         = FALSE,
    .KillReadAllApi          = FALSE,
    .RepairDamagedBlocksApi  = FALSE,
    .CalcRamBlockCrc         = TRUE,
    .UseCrcCompMechanism     = TRUE,
    .MainFunctionPeriod      = 10U
};
