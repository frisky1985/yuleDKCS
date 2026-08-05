/**
 * @file Mcal.h
 * @brief Microcontroller Abstraction Layer — yuleDKCS
 *
 * 底层 MCAL 基础函数, 被 CSM/Csm 引用
 */

#ifndef MCAL_H
#define MCAL_H

#include "Std_Types.h"
#include "Trng.h"

/* ============================================================================
 * 内存操作函数
 * ============================================================================ */

/** @brief 内存复制 (CSM 需要) */
static inline void Mcal_MemCopy(void *dst, const void *src, uint32 len)
{
    uint8 *d = (uint8*)dst;
    const uint8 *s = (const uint8*)src;
    uint32 i;
    for (i = 0; i < len; i++) d[i] = s[i];
}

/** @brief 内存置零 */
static inline void Mcal_MemSet(void *dst, uint8 val, uint32 len)
{
    uint8 *d = (uint8*)dst;
    uint32 i;
    for (i = 0; i < len; i++) d[i] = val;
}

/** @brief 内存比较 */
static inline uint32 Mcal_MemCmp(const void *a, const void *b, uint32 len)
{
    const uint8 *pa = (const uint8*)a;
    const uint8 *pb = (const uint8*)b;
    uint32 i;
    for (i = 0; i < len; i++) {
        if (pa[i] != pb[i]) return (uint32)(pa[i] - pb[i]);
    }
    return 0U;
}

/* ============================================================================
 * 基础硬件控制
 * ============================================================================ */

/** @brief 禁用全局中断 */
static inline void Mcal_DisableInterrupts(void)
{
    __asm volatile ("cpsid i");
}

/** @brief 使能全局中断 */
static inline void Mcal_EnableInterrupts(void)
{
    __asm volatile ("cpsie i");
}

/** @brief 数据同步屏障 */
static inline void Mcal_DataSyncBarrier(void)
{
    __asm volatile ("dsb sy");
}

/** @brief 数据内存屏障 */
static inline void Mcal_DataMemBarrier(void)
{
    __asm volatile ("dmb sy");
}

/** @brief 指令同步屏障 */
static inline void Mcal_InstSyncBarrier(void)
{
    __asm volatile ("isb sy");
}

#endif /* MCAL_H */
