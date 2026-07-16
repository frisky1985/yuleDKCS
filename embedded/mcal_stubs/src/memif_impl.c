/**
 * @file memif_impl.c
 * @brief Memory Abstraction Interface 实现
 *
 * NvM 调用的 MemIf 底层存储操作
 * yuleDKCS 使用 S32K312 内部 Flash 作为 NVRAM 后端
 */

#include "MemIf.h"
#include "string.h"

/* ============================================================================
 * 虚拟 Flash 存储 (用于编译验证)
 *
 * 在正式硬件上:
 *   - 使用 S32K312 Flash 控制器 (FTFC) 操作
 *   - 写前需擦除 sector
 *   - 考虑 ECC / 磨损均衡
 * ============================================================================ */

#define MEMIF_NUM_BLOCKS    8U
#define MEMIF_BLOCK_SIZE    1024U

/* 内部虚拟存储 (仿真用) */
static uint8 Dk_MemStorage[MEMIF_NUM_BLOCKS][MEMIF_BLOCK_SIZE];
static boolean Dk_MemBlockValid[MEMIF_NUM_BLOCKS];

/* 状态跟踪 */
static MemIf_StatusType Dk_MemStatus[MEMIF_DEVICE_EEP + 1] = {
    MEMIF_IDLE, MEMIF_IDLE
};
static MemIf_JobResultType Dk_MemJobResult[MEMIF_DEVICE_EEP + 1] = {
    MEMIF_JOB_OK, MEMIF_JOB_OK
};

/* ============================================================================
 * MemIf_Read: 从存储读取数据
 * ============================================================================ */
Std_ReturnType MemIf_Read(uint8 DeviceId, uint16 BlockNumber, uint16 Offset,
                          uint8 *DataBufferPtr, uint16 Length)
{
    if (DeviceId > MEMIF_DEVICE_EEP) return E_NOT_OK;
    if (BlockNumber >= MEMIF_NUM_BLOCKS) return E_NOT_OK;
    if (DataBufferPtr == NULL_PTR) return E_NOT_OK;

    uint16 availBytes;
    if (Offset + Length > MEMIF_BLOCK_SIZE) {
        availBytes = MEMIF_BLOCK_SIZE - Offset;
        if (availBytes > Length) availBytes = Length;
    } else {
        availBytes = Length;
    }

    Dk_MemStatus[DeviceId] = MEMIF_BUSY;

    if (!Dk_MemBlockValid[BlockNumber]) {
        /* 块无效 — 标记读取失败, 由 NvM 回退到 ROM */
        Dk_MemStatus[DeviceId] = MEMIF_IDLE;
        Dk_MemJobResult[DeviceId] = MEMIF_JOB_FAILED;
        return E_NOT_OK;
    }

    (void)memcpy(DataBufferPtr, &Dk_MemStorage[BlockNumber][Offset], availBytes);

    Dk_MemStatus[DeviceId] = MEMIF_IDLE;
    Dk_MemJobResult[DeviceId] = MEMIF_JOB_OK;
    return E_OK;
}

/* ============================================================================
 * MemIf_Write: 写入数据到存储
 * ============================================================================ */
Std_ReturnType MemIf_Write(uint8 DeviceId, uint16 BlockNumber,
                           const uint8 *DataBufferPtr)
{
    if (DeviceId > MEMIF_DEVICE_EEP) return E_NOT_OK;
    if (BlockNumber >= MEMIF_NUM_BLOCKS) return E_NOT_OK;
    if (DataBufferPtr == NULL_PTR) return E_NOT_OK;

    Dk_MemStatus[DeviceId] = MEMIF_BUSY;

    (void)memcpy(Dk_MemStorage[BlockNumber], DataBufferPtr, MEMIF_BLOCK_SIZE);
    Dk_MemBlockValid[BlockNumber] = TRUE;

    Dk_MemStatus[DeviceId] = MEMIF_IDLE;
    Dk_MemJobResult[DeviceId] = MEMIF_JOB_OK;
    return E_OK;
}

/* ============================================================================
 * MemIf_EraseImmediateBlock: 擦除存储块
 * ============================================================================ */
Std_ReturnType MemIf_EraseImmediateBlock(uint8 DeviceId, uint16 BlockNumber)
{
    if (DeviceId > MEMIF_DEVICE_EEP) return E_NOT_OK;
    if (BlockNumber >= MEMIF_NUM_BLOCKS) return E_NOT_OK;

    Dk_MemStatus[DeviceId] = MEMIF_BUSY;

    (void)memset(Dk_MemStorage[BlockNumber], 0xFF, MEMIF_BLOCK_SIZE);
    Dk_MemBlockValid[BlockNumber] = FALSE;

    Dk_MemStatus[DeviceId] = MEMIF_IDLE;
    Dk_MemJobResult[DeviceId] = MEMIF_JOB_OK;
    return E_OK;
}

/* ============================================================================
 * MemIf_InvalidateBlock: 将块标记为无效
 * ============================================================================ */
Std_ReturnType MemIf_InvalidateBlock(uint8 DeviceId, uint16 BlockNumber)
{
    if (DeviceId > MEMIF_DEVICE_EEP) return E_NOT_OK;
    if (BlockNumber >= MEMIF_NUM_BLOCKS) return E_NOT_OK;

    Dk_MemBlockValid[BlockNumber] = FALSE;
    return E_OK;
}

/* ============================================================================
 * MemIf_GetStatus: 获取设备状态
 * ============================================================================ */
MemIf_StatusType MemIf_GetStatus(uint8 DeviceId)
{
    if (DeviceId > MEMIF_DEVICE_EEP) return MEMIF_UNINIT;
    return Dk_MemStatus[DeviceId];
}

/* ============================================================================
 * MemIf_GetJobResult: 获取作业结果
 * ============================================================================ */
MemIf_JobResultType MemIf_GetJobResult(uint8 DeviceId)
{
    if (DeviceId > MEMIF_DEVICE_EEP) return MEMIF_JOB_FAILED;
    return Dk_MemJobResult[DeviceId];
}
