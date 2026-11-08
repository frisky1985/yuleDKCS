/**
 * @file Std_Types.h
 * @brief Minimal AUTOSAR Std_Types.h fallback (yuleDKCS BSW)
 *
 * Used ONLY when the external yuleASR BSW stack (which ships the real
 * Std_Types.h) is not available. Provides the standard AUTOSAR base
 * types so the self-contained MCAL/RTD adapter targets can still build
 * in degraded mode.
 *
 * When yuleASR is present, this file is NOT on the include path and
 * yuleASR's canonical Std_Types.h is used instead.
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

/* ── AUTOSAR standard integer types ── */
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef uint64_t  uint64;
typedef int8_t    sint8;
typedef int16_t   sint16;
typedef int32_t   sint32;
typedef int64_t   sint64;
typedef uint8_t   boolean;

/* ── Boolean values ── */
#ifndef TRUE
#define TRUE  ((boolean)1U)
#endif
#ifndef FALSE
#define FALSE ((boolean)0U)
#endif

/* ── Return type ── */
typedef uint8_t   Std_ReturnType;

#ifndef E_OK
#define E_OK        ((Std_ReturnType)0U)
#endif
#ifndef E_NOT_OK
#define E_NOT_OK    ((Std_ReturnType)1U)
#endif

/* ── Version info structure (AUTOSAR SWS_StdTypes) ── */
typedef struct {
    uint16_t vendorID;
    uint16_t moduleID;
    uint8_t  sw_major_version;
    uint8_t  sw_minor_version;
    uint8_t  sw_patch_version;
} Std_VersionInfoType;

/* ── Version info ── */
#define STD_TYPES_SW_MAJOR_VERSION  1U
#define STD_TYPES_SW_MINOR_VERSION  0U
#define STD_TYPES_SW_PATCH_VERSION  0U

/* ── NULL pointer ── */
#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

#endif /* STD_TYPES_H */
