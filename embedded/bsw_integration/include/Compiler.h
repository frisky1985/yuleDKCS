/**
 * @file Compiler.h
 * @brief AUTOSAR Compiler Abstraction Stub — yuleDKCS BSW Phase 1
 *
 * 提供 AUTOSAR Compiler.h 的基本定义。
 * 实际产品由 NXP S32 SDK 的 Compiler.h 提供。
 */

#ifndef COMPILER_H
#define COMPILER_H

/*
 * Include Std_Types.h first so its FUNC/CONST/P2VAR definitions take
 * priority. Our guarded (ifndef) definitions below will then be no-ops.
 */
#ifndef _DK_STANDALONE_CHECK_
#include "Std_Types.h"
#endif

/* 函数修饰符 — guard against redefinitions */
#ifndef FUNC
    #define FUNC(ret, memclass)             ret
#endif
#ifndef P2VAR
    #define P2VAR(ptrtype, memclass, ptrclass) ptrtype *
#endif
#ifndef P2CONST
    #define P2CONST(ptrtype, memclass, ptrclass) const ptrtype *
#endif
#ifndef CONST
    #define CONST(consttype, memclass)      const consttype
#endif
#ifndef COM_CONST
    #define COM_CONST                      AUTOMATIC
#endif
#ifndef DEM_CONST
    #define DEM_CONST                      AUTOMATIC
#endif
#ifndef DCM_CONST
    #define DCM_CONST                      AUTOMATIC
#endif
#ifndef VAR
    #define VAR(vartype, memclass)          vartype
#endif
#ifndef STATIC
    #define STATIC                          static
#endif
#ifndef NULL_PTR
    #define NULL_PTR                        ((void*)0)
#endif

/* 内存映射宏 (默认) */
#ifndef AUTOMATIC
    #define AUTOMATIC
#endif
#ifndef TYPEDEF
    #define TYPEDEF
#endif

/* 函数定义宏 */
#ifndef LOCAL_INLINE
    #define LOCAL_INLINE                    static inline
#endif

#endif /* COMPILER_H */
