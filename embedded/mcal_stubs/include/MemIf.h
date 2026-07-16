/**
 * @file MemIf.h
 * @brief Memory Abstraction Interface — yuleDKCS
 *
 * NvM 通过 MemIf 访问底层存储 (Flash/EEP)
 * 本例中实现为直接 S32K312 Flash 操作
 */

#ifndef MEMIF_H
#define MEMIF_H

#include "Std_Types.h"

/* MemIf 设备 ID */
#define MEMIF_DEVICE_FLS    0U   /* Flash 设备 */
#define MEMIF_DEVICE_EEP    1U   /* EEP 设备 */

/* MemIf 状态 */
typedef enum {
    MEMIF_UNINIT = 0,
    MEMIF_IDLE,
    MEMIF_BUSY,
    MEMIF_BUSY_INTERNAL
} MemIf_StatusType;

/* MemIf 作业结果 */
typedef enum {
    MEMIF_JOB_OK = 0,
    MEMIF_JOB_FAILED,
    MEMIF_JOB_PENDING,
    MEMIF_JOB_CANCELED
} MemIf_JobResultType;

/* ============================================================================
 * API 声明 (NvM 使用)
 * ============================================================================ */
Std_ReturnType   MemIf_Read(uint8 DeviceId, uint16 BlockNumber, uint16 Offset,
                            uint8 *DataBufferPtr, uint16 Length);
Std_ReturnType   MemIf_Write(uint8 DeviceId, uint16 BlockNumber,
                             const uint8 *DataBufferPtr);
Std_ReturnType   MemIf_EraseImmediateBlock(uint8 DeviceId, uint16 BlockNumber);
Std_ReturnType   MemIf_InvalidateBlock(uint8 DeviceId, uint16 BlockNumber);
MemIf_StatusType MemIf_GetStatus(uint8 DeviceId);
MemIf_JobResultType MemIf_GetJobResult(uint8 DeviceId);

#endif /* MEMIF_H */
