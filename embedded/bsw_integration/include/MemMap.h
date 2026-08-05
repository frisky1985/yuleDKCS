/**
 * @file MemMap.h
 * @brief AUTOSAR Memory Mapping — yuleDKCS BSW Phase 1
 *
 * Maps AUTOSAR memory sections to GCC __attribute__((section(...)))
 * for S32K312 (Cortex-M7) build.
 *
 * Supported sections:
 *   CODE (.text)            — START/STOP_SEC_CODE
 *   CONST (.rodata)         — START/STOP_SEC_CONST_*
 *   VAR (.data/.bss)        — START/STOP_SEC_VAR_*
 *   CONFIG_DATA (.data)     — START/STOP_SEC_CONFIG_DATA_*
 *   CALIB (.rodata)         — START/STOP_SEC_CALIB_*
 *   INIT (.data)            — START/STOP_SEC_INIT_*
 *
 * Usage:
 *   #define OS_START_SEC_CODE
 *   #include "MemMap.h"
 *   // ... code to place ...
 *   #define OS_STOP_SEC_CODE
 *   #include "MemMap.h"
 */

#ifndef MEMMAP_H
#define MEMMAP_H

/* =========================================================================
 * Internal: Select section name based on current defined macro
 * Each AUTOSAR module uses its own prefix (e.g., OS_, ECUM_, WDGM_, DET_, BSWM_)
 * ========================================================================= */

/* ---------- CODE (Flash .text) ---------- */
#if defined(OS_START_SEC_CODE)
    #undef  OS_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.Os"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(OS_STOP_SEC_CODE)
    #undef  OS_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(ECUM_START_SEC_CODE)
    #undef  ECUM_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.EcuM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(ECUM_STOP_SEC_CODE)
    #undef  ECUM_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(WDGM_START_SEC_CODE)
    #undef  WDGM_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.WdgM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(WDGM_STOP_SEC_CODE)
    #undef  WDGM_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(DET_START_SEC_CODE)
    #undef  DET_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.Det"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(DET_STOP_SEC_CODE)
    #undef  DET_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(BSWM_START_SEC_CODE)
    #undef  BSWM_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.BswM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(BSWM_STOP_SEC_CODE)
    #undef  BSWM_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* ---------- CONST (Flash .rodata) ---------- */
#elif defined(OS_START_SEC_CONST_UNSPECIFIED)
    #undef  OS_START_SEC_CONST_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.Os"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(OS_STOP_SEC_CONST_UNSPECIFIED)
    #undef  OS_STOP_SEC_CONST_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(OS_START_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  OS_START_SEC_CONFIG_DATA_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.Os_Config"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(OS_STOP_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  OS_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(OS_START_SEC_CALIB_UNSPECIFIED)
    #undef  OS_START_SEC_CALIB_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.Os_Calib"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(OS_STOP_SEC_CALIB_UNSPECIFIED)
    #undef  OS_STOP_SEC_CALIB_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* Generic CONST (for Det, EcuM, WdgM, BswM) */
#elif defined(DET_START_SEC_CONST_UNSPECIFIED)
    #undef  DET_START_SEC_CONST_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.Det"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(DET_STOP_SEC_CONST_UNSPECIFIED)
    #undef  DET_STOP_SEC_CONST_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(ECUM_START_SEC_CONST_UNSPECIFIED)
    #undef  ECUM_START_SEC_CONST_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.EcuM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(ECUM_STOP_SEC_CONST_UNSPECIFIED)
    #undef  ECUM_STOP_SEC_CONST_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* ---------- VAR cleared/uncleared (RAM .bss / .data) ---------- */
#elif defined(OS_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  OS_START_SEC_VAR_CLEARED_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.Os"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(OS_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  OS_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(OS_START_SEC_VAR_NOINIT_UNSPECIFIED)
    #undef  OS_START_SEC_VAR_NOINIT_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.Os_NoInit"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(OS_STOP_SEC_VAR_NOINIT_UNSPECIFIED)
    #undef  OS_STOP_SEC_VAR_NOINIT_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(OS_START_SEC_VAR_FAST_UNSPECIFIED)
    #undef  OS_START_SEC_VAR_FAST_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.Os_Fast"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(OS_STOP_SEC_VAR_FAST_UNSPECIFIED)
    #undef  OS_STOP_SEC_VAR_FAST_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(OS_START_SEC_INIT_UNSPECIFIED)
    #undef  OS_START_SEC_INIT_UNSPECIFIED
    #define MEMMAP_SECTION    ".data.Os"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(OS_STOP_SEC_INIT_UNSPECIFIED)
    #undef  OS_STOP_SEC_INIT_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* Generic VAR for Det */
#elif defined(DET_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  DET_START_SEC_VAR_CLEARED_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.Det"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(DET_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  DET_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* Generic VAR for EcuM */
#elif defined(ECUM_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  ECUM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.EcuM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(ECUM_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  ECUM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* Generic VAR for WdgM */
#elif defined(WDGM_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  WDGM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.WdgM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(WDGM_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  WDGM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* Generic VAR for BswM */
#elif defined(BSWM_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  BSWM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.BswM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(BSWM_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  BSWM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* ---------- Phase 3: NvM / CSM / CryIf ---------- */

#elif defined(NVM_START_SEC_CODE)
    #undef  NVM_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.NvM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(NVM_STOP_SEC_CODE)
    #undef  NVM_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(NVM_START_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  NVM_START_SEC_CONFIG_DATA_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.NvM_Config"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(NVM_STOP_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  NVM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(NVM_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  NVM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.NvM"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(NVM_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  NVM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* --- CSM --- */
#elif defined(CSM_START_SEC_CODE)
    #undef  CSM_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.Csm"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(CSM_STOP_SEC_CODE)
    #undef  CSM_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(CSM_START_SEC_VAR_INIT_UNSPECIFIED)
    #undef  CSM_START_SEC_VAR_INIT_UNSPECIFIED
    #define MEMMAP_SECTION    ".data.Csm"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(CSM_STOP_SEC_VAR_INIT_UNSPECIFIED)
    #undef  CSM_STOP_SEC_VAR_INIT_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(CSM_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  CSM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #define MEMMAP_SECTION    ".bss.Csm"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(CSM_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #undef  CSM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(CSM_START_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  CSM_START_SEC_CONFIG_DATA_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.Csm_Config"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(CSM_STOP_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  CSM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* --- CryIf --- */
#elif defined(CRYIF_START_SEC_CODE)
    #undef  CRYIF_START_SEC_CODE
    #define MEMMAP_SECTION    ".text.CryIf"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(CRYIF_STOP_SEC_CODE)
    #undef  CRYIF_STOP_SEC_CODE
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(CRYIF_START_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  CRYIF_START_SEC_CONFIG_DATA_UNSPECIFIED
    #define MEMMAP_SECTION    ".rodata.CryIf_Config"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(CRYIF_STOP_SEC_CONFIG_DATA_UNSPECIFIED)
    #undef  CRYIF_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* ---------- Phase 2: COM / DCM / DEM / PduR ---------- */

#elif defined(COM_START_SEC_CODE) || defined(DCM_START_SEC_CODE) || defined(DEM_START_SEC_CODE) || defined(PDUR_START_SEC_CODE)
    #ifdef COM_START_SEC_CODE
        #undef COM_START_SEC_CODE
    #endif
    #ifdef DCM_START_SEC_CODE
        #undef DCM_START_SEC_CODE
    #endif
    #ifdef DEM_START_SEC_CODE
        #undef DEM_START_SEC_CODE
    #endif
    #ifdef PDUR_START_SEC_CODE
        #undef PDUR_START_SEC_CODE
    #endif
    #define MEMMAP_SECTION    ".text.Bsw"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(COM_STOP_SEC_CODE) || defined(DCM_STOP_SEC_CODE) || defined(DEM_STOP_SEC_CODE) || defined(PDUR_STOP_SEC_CODE)
    #ifdef COM_STOP_SEC_CODE
        #undef COM_STOP_SEC_CODE
    #endif
    #ifdef DCM_STOP_SEC_CODE
        #undef DCM_STOP_SEC_CODE
    #endif
    #ifdef DEM_STOP_SEC_CODE
        #undef DEM_STOP_SEC_CODE
    #endif
    #ifdef PDUR_STOP_SEC_CODE
        #undef PDUR_STOP_SEC_CODE
    #endif
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

/* Phase 2: Config data + VAR */
#elif defined(COM_START_SEC_CONFIG_DATA_UNSPECIFIED) || defined(DCM_START_SEC_CONFIG_DATA_UNSPECIFIED) || defined(DEM_START_SEC_CONFIG_DATA_UNSPECIFIED) || defined(PDUR_START_SEC_CONFIG_DATA_UNSPECIFIED)
    #ifdef COM_START_SEC_CONFIG_DATA_UNSPECIFIED
        #undef COM_START_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #ifdef DCM_START_SEC_CONFIG_DATA_UNSPECIFIED
        #undef DCM_START_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #ifdef DEM_START_SEC_CONFIG_DATA_UNSPECIFIED
        #undef DEM_START_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #ifdef PDUR_START_SEC_CONFIG_DATA_UNSPECIFIED
        #undef PDUR_START_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #define MEMMAP_SECTION    ".rodata.Bsw_Config"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION), used))

#elif defined(COM_STOP_SEC_CONFIG_DATA_UNSPECIFIED) || defined(DCM_STOP_SEC_CONFIG_DATA_UNSPECIFIED) || defined(DEM_STOP_SEC_CONFIG_DATA_UNSPECIFIED) || defined(PDUR_STOP_SEC_CONFIG_DATA_UNSPECIFIED)
    #ifdef COM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
        #undef COM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #ifdef DCM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
        #undef DCM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #ifdef DEM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
        #undef DEM_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #ifdef PDUR_STOP_SEC_CONFIG_DATA_UNSPECIFIED
        #undef PDUR_STOP_SEC_CONFIG_DATA_UNSPECIFIED
    #endif
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#elif defined(COM_START_SEC_VAR_CLEARED_UNSPECIFIED) || defined(DCM_START_SEC_VAR_CLEARED_UNSPECIFIED) || defined(DEM_START_SEC_VAR_CLEARED_UNSPECIFIED)
    #ifdef COM_START_SEC_VAR_CLEARED_UNSPECIFIED
        #undef COM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #endif
    #ifdef DCM_START_SEC_VAR_CLEARED_UNSPECIFIED
        #undef DCM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #endif
    #ifdef DEM_START_SEC_VAR_CLEARED_UNSPECIFIED
        #undef DEM_START_SEC_VAR_CLEARED_UNSPECIFIED
    #endif
    #define MEMMAP_SECTION    ".bss.Bsw"
    #define MEMMAP_ATTRIBUTE  __attribute__((section(MEMMAP_SECTION)))

#elif defined(COM_STOP_SEC_VAR_CLEARED_UNSPECIFIED) || defined(DCM_STOP_SEC_VAR_CLEARED_UNSPECIFIED) || defined(DEM_STOP_SEC_VAR_CLEARED_UNSPECIFIED)
    #ifdef COM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
        #undef COM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #endif
    #ifdef DCM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
        #undef DCM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #endif
    #ifdef DEM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
        #undef DEM_STOP_SEC_VAR_CLEARED_UNSPECIFIED
    #endif
    #undef  MEMMAP_SECTION
    #undef  MEMMAP_ATTRIBUTE

#else
    /* Fallback: no section attribute — compiler default */
    #ifndef MEMMAP_ATTRIBUTE
        #define MEMMAP_ATTRIBUTE
    #endif
#endif

/* =========================================================================
 * Compatibility defines for Os.c's SEC usage
 * ========================================================================= */
#if defined(OS_START_SEC_TEXT)
    #define OS_START_SEC_CODE
    /* include self again to apply, then restore state marker */
#endif

#if defined(OS_STOP_SEC_TEXT)
    #define OS_STOP_SEC_CODE
#endif

#if defined(OS_START_SEC_VAR)
    #define OS_START_SEC_VAR_CLEARED_UNSPECIFIED
#endif

#if defined(OS_STOP_SEC_VAR)
    #define OS_STOP_SEC_VAR_CLEARED_UNSPECIFIED
#endif

#if defined(OS_START_SEC_CONST)
    #define OS_START_SEC_CONST_UNSPECIFIED
#endif

#if defined(OS_STOP_SEC_CONST)
    #define OS_STOP_SEC_CONST_UNSPECIFIED
#endif

#endif /* MEMMAP_H */
