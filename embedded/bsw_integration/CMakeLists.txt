# ============================================================================
# yuleDKCS BSW Phase 1+2 — OS + EcuM + WdgM + COM + DCM + DEM 集成
# 目标 MCU: NXP S32K312 (ARM Cortex-M7)
# 工具链: arm-none-eabi-gcc
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(yuleDKCS_BSW_Phase2 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# 第三方库路径
# ============================================================================
set(YULEASR_ROOT     $ENV{HOME}/.openclaw/workspace/yuleASR)
set(YULEDKCS_ROOT    $ENV{HOME}/yuleDKCS)

if(NOT EXISTS ${YULEASR_ROOT})
    message(FATAL_ERROR "yuleASR not found at ${YULEASR_ROOT}. Set YULEASR_ROOT or clone the repo.")
endif()

if(NOT EXISTS ${YULEDKCS_ROOT})
    message(FATAL_ERROR "yuleDKCS not found at ${YULEDKCS_ROOT}.")
endif()

message(STATUS "yuleASR root: ${YULEASR_ROOT}")
message(STATUS "yuleDKCS root: ${YULEDKCS_ROOT}")

# ============================================================================
# 编译参数
# ============================================================================
set(ARM_CPU_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")

set(CMAKE_C_FLAGS "${ARM_CPU_FLAGS} -Wall -Wextra -Werror -Os -ffreestanding -g3 -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -mthumb-interwork" CACHE STRING "C flags" FORCE)

# ============================================================================
# BSW 集成包含路径
# ============================================================================
# IMPORTANT: Order matters — freertos_port/include must come BEFORE yuleASR's OS include
# so that our FreeRTOS port headers (FreeRTOSConfig, portmacro, etc.) take priority
# over the old yuleASR FreeRTOS stub.
set(BSW_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include                     # MemMap.h, Compiler.h, Det.h, SchM_BswM.h
    ${YULEDKCS_ROOT}/embedded/freertos_port/include         # FreeRTOSConfig.h, portmacro.h, timers.h, etc.
    ${YULEASR_ROOT}/src/bsw/os/include                      # Os.h, Os_Cfg.h, Std_Types.h, FreeRTOS.h
    ${YULEASR_ROOT}/src/bsw/services/ecum/include           # EcuM.h, EcuM_Cfg.h
    ${YULEASR_ROOT}/src/bsw/services/wdgm/include           # Wdgm.h, Wdgm_Cfg.h
    ${YULEASR_ROOT}/src/bsw/services/det/include            # Det.h, Det_Cfg.h, Det_MemMap.h
    ${YULEASR_ROOT}/src/bsw/services/bswm/include           # BswM.h, BswM_Cfg.h
    ${YULEASR_ROOT}/src/bsw/services/schm/include           # SchM.h
    ${YULEASR_ROOT}/src/bsw/services/nvm/include            # NvM.h, NvM_Cfg.h
    ${YULEASR_ROOT}/src/bsw/services/csm/include            # Csm.h, Csm_Types.h, Csm_Cfg.h, Csm_MemMap.h
    ${YULEASR_ROOT}/src/bsw/services/cryif/include         # CryIf.h, CryIf_Cfg.h, CryIf_Types.h
	${YULEDKCS_ROOT}/embedded/mcal_stubs/include             # Mcal.h, MemIf.h
    ${YULEASR_ROOT}/src/bsw/services/comm/include           # ComM.h
    ${YULEASR_ROOT}/src/bsw/services/com/include            # Com.h (BSW COM)
    ${YULEASR_ROOT}/src/bsw/services/dcm/include            # Dcm.h (UDS DCM)
    ${YULEASR_ROOT}/src/bsw/services/dem/include            # Dem.h (DTC DEM)
    ${YULEASR_ROOT}/src/bsw/services/pdur/include           # PduR.h (PDU Router)
    ${YULEASR_ROOT}/src/bsw/ecual/wdgif/include             # WdgIf.h
    ${YULEDKCS_ROOT}/embedded/freestanding_includes         # string.h, stdlib.h
)

# ============================================================================
# yuleDKCS 应用包含路径
# ============================================================================
set(DK_INCLUDE_DIRS
    ${YULEDKCS_ROOT}/embedded/icce_protocol/include
    ${YULEDKCS_ROOT}/embedded/icce_protocol/src/crypto
    ${YULEDKCS_ROOT}/embedded/icce_protocol/src/ble
    ${YULEDKCS_ROOT}/embedded/icce_protocol/src/cache
    ${YULEDKCS_ROOT}/embedded/icce_protocol/src/decision
    ${YULEDKCS_ROOT}/embedded/icce_protocol/src/security
    ${YULEDKCS_ROOT}/embedded/icce_protocol/src/vehicle
    ${YULEDKCS_ROOT}/embedded/system_architecture
    ${YULEDKCS_ROOT}/embedded/freestanding_includes
)

# ============================================================================
# 编译选项: yuleDKCS 配置
# ============================================================================
add_compile_definitions(
    CONFIG_ENABLE_CRYPTO
    DK_BSW_PHASE1                        # 标记 BSW Phase 1 集成
    DK_BSW_PHASE2                        # 标记 BSW Phase 2 集成
)

# ============================================================================
# FreeRTOS Port — S32K312 (Cortex-M7)
# ============================================================================
add_library(freertos_port STATIC
    ${YULEDKCS_ROOT}/embedded/freertos_port/src/port.c
)

target_include_directories(freertos_port
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# OS 模块 — 使用 yuleDKCS 定制的 OS 配置 (代替 yuleASR 默认 Os_Cfg.c)
# ============================================================================
add_library(bsw_os STATIC
    ${YULEASR_ROOT}/src/bsw/os/src/Os.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_os_cfg.c            # yuleDKCS 定制: 任务/警报/资源表
)

target_include_directories(bsw_os
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# Det 模块 — Development Error Tracer
# ============================================================================
add_library(bsw_det STATIC
    ${YULEASR_ROOT}/src/bsw/services/det/src/Det.c
)

target_include_directories(bsw_det
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# BswM 模块 — Basic Software Mode Manager (正式实现 + 配置表)
# ============================================================================
add_library(bsw_bswm STATIC
    ${YULEASR_ROOT}/src/bsw/services/bswm/src/BswM.c
    ${YULEASR_ROOT}/src/bsw/services/bswm/src/BswM_Lcfg.c   # 模式请求端口/规则/动作配置
)

target_include_directories(bsw_bswm
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# MCAL Stubs — S32K312 驱动桩 (Phase 1 编译通过即可)
# ============================================================================
add_library(mcal_stubs STATIC
    ${YULEDKCS_ROOT}/embedded/mcal_stubs/src/mcal_stubs.c
)

target_include_directories(mcal_stubs
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${YULEDKCS_ROOT}/embedded/mcal_stubs/include
)

# ============================================================================
# RTD Adapter — NXP Real-Time Drivers 集成适配层
# ============================================================================
# 提供统一的 RTD_ 前缀 API，支持编译期 RTD/桩模式切换。
# 默认 RTD_ENABLED=0 使用寄存器级桩；RTD_ENABLED=1 调用 NXP RTD 驱动。
#
# 切换方式:
#   add_compile_definitions(RTD_ENABLED=1)
#   并设置 RTD 包含路径（见 mcal/README_RTD.md）
# ============================================================================
add_library(rtd_adapter STATIC
    ${YULEDKCS_ROOT}/embedded/bsw_integration/mcal/rtd_adapter.c
)

target_include_directories(rtd_adapter
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${YULEDKCS_ROOT}/embedded/mcal_stubs/include
        ${YULEDKCS_ROOT}/embedded/bsw_integration/mcal
)

# ============================================================================
# RTD Adapter 自检测试（主机平台编译）
# ============================================================================
set(RTD_ADAPTER_TEST_SRCS
    ${YULEDKCS_ROOT}/embedded/bsw_integration/mcal/test_rtd_adapter.c
    ${YULEDKCS_ROOT}/embedded/bsw_integration/mcal/rtd_adapter.c
    ${YULEDKCS_ROOT}/embedded/mcal_stubs/src/mcal_stubs.c
)

add_executable(test_rtd_adapter ${RTD_ADAPTER_TEST_SRCS})

target_include_directories(test_rtd_adapter
    PRIVATE
        ${BSW_INCLUDE_DIRS}
        ${YULEDKCS_ROOT}/embedded/mcal_stubs/include
        ${YULEDKCS_ROOT}/embedded/bsw_integration/mcal
        ${YULEDKCS_ROOT}/embedded/freestanding_includes
)

# 主机平台编译时关闭 -ffreestanding 和其他目标专有 flags
if(CMAKE_SYSTEM_NAME STREQUAL "Generic" AND CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    # 交叉编译模式: 仅编译适配层库，自测在主机上运行
    # 覆盖 test target 编译器为主机 gcc/clang
    set_target_properties(test_rtd_adapter PROPERTIES
        COMPILE_FLAGS "-DRTD_ADAPTER_SELF_TEST -fno-builtin -Wall -Wextra"
    )
    message(STATUS "RTD Adapter self-test: use host-compiled variant (bsw_rtd_adapter_test)")
endif()

# ============================================================================
# EcuM 模块 — 使用 yuleDKCS 定制的 EcuM callout
# ============================================================================
add_library(bsw_ecum STATIC
    ${YULEASR_ROOT}/src/bsw/services/ecum/src/EcuM.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_ecum_callouts.c     # yuleDKCS 定制: callout + 唤醒源
)

target_include_directories(bsw_ecum
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${DK_INCLUDE_DIRS}
)

# ============================================================================
# WdgM 模块 — 使用 yuleDKCS 定制的看门狗配置
# ============================================================================
add_library(bsw_wdgm STATIC
    ${YULEASR_ROOT}/src/bsw/services/wdgm/src/Wdgm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_wdgm_cfg.c          # yuleDKCS 定制: 监督实体/检查点
)

target_include_directories(bsw_wdgm
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# COM 模块 — Communication Stack (CAN Signal → I-PDU 映射)
# ============================================================================
add_library(bsw_com STATIC
    ${YULEASR_ROOT}/src/bsw/services/com/src/Com.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_com_cfg.c            # yuleDKCS COM 配置: 信号/IPDU 映射
)

target_include_directories(bsw_com
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# COM 编译：提供 yuleASR Com_Cfg.h 缺少的宏定义
target_compile_definitions(bsw_com PRIVATE
    COM_NUM_OF_IPDUS=4
    COM_NUM_OF_SIGNALS=8
    COM_MAX_IPDU_BUFFER_SIZE=64
    COM_NUM_OF_IPDU_GROUPS=2
    COM_NUM_IPDU_GROUPS=2
    COM_NUM_OF_SIGNAL_GROUPS=2
    COM_GATEWAY_SUPPORT=0
    COM_NUM_SIGNAL_GW_MAPPINGS=0
)

# ============================================================================
# PduR 模块 — PDU Router (配置路由路径)
# ============================================================================
add_library(bsw_pdur STATIC
    ${YULEASR_ROOT}/src/bsw/services/pdur/src/PduR.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_pdur_lcfg.c        # yuleDKCS PduR 配置: DK 路由表
)

target_include_directories(bsw_pdur
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# DCM 模块 — Diagnostic Communication Manager (UDS 协议栈)
# ============================================================================
add_library(bsw_dcm STATIC
    ${YULEASR_ROOT}/src/bsw/services/dcm/src/Dcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_dcm_cfg.c            # yuleDKCS DCM 配置: DIDs/RIDs
)

target_include_directories(bsw_dcm
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# DCM 编译：提供 yuleASR 缺少的宏定义
target_compile_definitions(bsw_dcm PRIVATE
    DCM_SECURITY_DELAY_TIME=DCM_SECURITY_DELAY_TIME_MS
    DCM_SID_DEINIT=6
    DCM_E_REQUEST_SEQUENCE_ERROR=DCM_E_REQUESTSEQUENCEERROR
    Dem_GetDTCStatus=Dem_GetStatusOfDTC
)

# ============================================================================
# DEM 模块 — Diagnostic Event Manager (DTC 事件管理)
# ============================================================================
add_library(bsw_dem STATIC
    ${YULEASR_ROOT}/src/bsw/services/dem/src/Dem.c
    ${YULEASR_ROOT}/src/bsw/services/dem/src/Dem_Int.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_dem_cfg.c            # yuleDKCS DEM 配置: DTC 事件表
)

target_include_directories(bsw_dem
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# DEM 编译：提供 yuleASR 缺少的宏定义
target_compile_definitions(bsw_dem PRIVATE
    DEM_SID_GETOPERATIONCYCLESTATE=17
)

# ============================================================================
# yuleDKCS UDS Diagnostic Callbacks (DID read/write implementations)
# ============================================================================
add_library(bsw_dk_diag STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_diag_callbacks.c
)

target_include_directories(bsw_dk_diag
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# NvM — NVRAM 管理器 (正式实现)
# ============================================================================
add_library(bsw_nvm STATIC
    ${YULEASR_ROOT}/src/bsw/services/nvm/src/NvM.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_nvm_cfg.c          # yuleDKCS NVRAM 块配置
)

target_include_directories(bsw_nvm
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# NvM 编译定义
# NvM.c 引用 MemIf.h — 在 mcal_stubs/include 下
target_compile_definitions(bsw_nvm PRIVATE
    NVM_DEV_ERROR_DETECT=STD_ON
)

# ============================================================================
# MemIf — Memory Abstraction Interface (NvM 底层存储驱动)
# ============================================================================
add_library(bsw_memif STATIC
    ${YULEDKCS_ROOT}/embedded/mcal_stubs/src/memif_impl.c
)

target_include_directories(bsw_memif
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${YULEDKCS_ROOT}/embedded/mcal_stubs/include
)

# ============================================================================
# CryIf — Crypto Interface (CSM 底层密码桥接)
# ============================================================================
add_library(bsw_cryif STATIC
    ${YULEASR_ROOT}/src/bsw/services/cryif/src/CryIf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_cryif_cfg.c        # yuleDKCS CryIf 配置
)

target_include_directories(bsw_cryif
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# CSM — Crypto Services Manager (正式实现)
# ============================================================================
add_library(bsw_csm STATIC
    ${YULEASR_ROOT}/src/bsw/services/csm/src/Csm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_csm_cfg.c          # yuleDKCS CSM 密钥/作业配置
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_csm_callouts.c     # yuleDKCS CSM 回调: 加密/存储/随机数
)

target_include_directories(bsw_csm
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

target_compile_definitions(bsw_csm PRIVATE
    CSM_CFG_DEV_ERROR_DETECT=STD_ON
    CSM_CFG_ASYNC_SUPPORT=STD_ON
    CSM_CFG_QUEUE_SUPPORT=STD_ON
    CSM_CFG_KEY_PERSISTENCE_SUPPORT=STD_ON
    CSM_CFG_RANDOM_GENERATOR_SUPPORT=STD_ON
    CSM_CFG_DEM_INTEGRATION=STD_OFF
    CSM_DEV_ERROR_DETECT=STD_ON
    CSM_VERSION_INFO_API=STD_ON
)

# ============================================================================
# BSW Stubs — ComM, BswM_EcuM_CurrentWakeup 等 (不再包含 NvM)
# ============================================================================
add_library(bsw_stubs STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bsw_stubs.c
)

target_include_directories(bsw_stubs
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${YULEDKCS_ROOT}/embedded/mcal_stubs/include
)

# ============================================================================
# 应用任务 — OS 任务入口 (yuleDKCS 功能)
# ============================================================================
add_library(bsw_app_tasks STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dk_os_tasks.c
)

target_include_directories(bsw_app_tasks
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${DK_INCLUDE_DIRS}
)

# ============================================================================
# Stack / Heap / Stubs 模块
# ============================================================================
add_library(bsw_platform STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/startup_s32k312.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/freertos_stubs.c
)

target_include_directories(bsw_platform
    PUBLIC  ${BSW_INCLUDE_DIRS}
)

# ============================================================================
# 可执行文件: yuleDKCS_bsw.elf (S32K312 固件映像)
# ============================================================================
add_executable(yuleDKCS_bsw.elf
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
)

target_include_directories(yuleDKCS_bsw.elf
    PRIVATE
        ${BSW_INCLUDE_DIRS}
        ${DK_INCLUDE_DIRS}
        ${YULEDKCS_ROOT}/embedded/mcal_stubs/include
)

set(BSW_LINK_LIBS
    bsw_app_tasks
    bsw_os
    bsw_det
    bsw_ecum
    bsw_bswm
    bsw_nvm
    bsw_memif
    bsw_csm
    bsw_cryif
    bsw_com
    bsw_pdur
    bsw_dcm
    bsw_dem
    bsw_dk_diag
    bsw_stubs
    bsw_wdgm
    rtd_adapter
    mcal_stubs
    freertos_port
    bsw_platform
)

target_link_libraries(yuleDKCS_bsw.elf ${BSW_LINK_LIBS})

set_target_properties(yuleDKCS_bsw.elf PROPERTIES
    LINK_FLAGS "-Wl,--start-group -Wl,--end-group -T${S32K312_LINKER_SCRIPT} -Wl,-Map=yuleDKCS_bsw.map -nostdlib -lgcc"
)

# ============================================================================
# 链接器脚本 (S32K312)
# ============================================================================
set(S32K312_LINKER_SCRIPT ${YULEASR_ROOT}/src/platform/s32k312/linker/s32k312.ld)

if(EXISTS ${S32K312_LINKER_SCRIPT})
    set_target_properties(yuleDKCS_bsw.elf PROPERTIES
        LINK_FLAGS "-T${S32K312_LINKER_SCRIPT} -Wl,-Map=yuleDKCS_bsw.map -nostdlib -lgcc -Wl,--no-warn-mismatch"
    )
    message(STATUS "Linker script: ${S32K312_LINKER_SCRIPT}")
else()
    message(WARNING "S32K312 linker script not found at ${S32K312_LINKER_SCRIPT}. Using default.")
    set_target_properties(yuleDKCS_bsw.elf PROPERTIES
        LINK_FLAGS "-Wl,-Map=yuleDKCS_bsw.map -nostartfiles"
    )
endif()

# ============================================================================
# 后处理: 生成二进制/十六进制/反汇编
# ============================================================================
add_custom_command(TARGET yuleDKCS_bsw.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O binary yuleDKCS_bsw.elf yuleDKCS_bsw.bin
    COMMAND arm-none-eabi-objcopy -O ihex   yuleDKCS_bsw.elf yuleDKCS_bsw.hex
    COMMAND arm-none-eabi-objdump -S -d     yuleDKCS_bsw.elf > yuleDKCS_bsw.dis
    COMMENT "📦 生成: yuleDKCS_bsw.bin / yuleDKCS_bsw.hex / yuleDKCS_bsw.dis"
)

# ============================================================================
# 输出目标
# ============================================================================
add_custom_target(bsw_phase1_all
    DEPENDS yuleDKCS_bsw.elf
    COMMENT "✅ BSW Phase 1 构建完成: yuleDKCS_bsw.elf"
)

add_custom_target(bsw_phase1_size
    COMMAND arm-none-eabi-size yuleDKCS_bsw.elf
    DEPENDS yuleDKCS_bsw.elf
    COMMENT "📊 BSW Phase 1 映像大小统计"
)

add_custom_target(bsw_phase1_clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "🧹 清理 BSW Phase 1 构建目录"
)

# ============================================================================
# 构建验证信息
# ============================================================================
add_custom_target(bsw_phase1_info
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== yuleDKCS BSW Phase 1 ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  OS:   yuleASR AUTOSAR OS (FreeRTOS 封装)"
    COMMAND ${CMAKE_COMMAND} -E echo "  EcuM: yuleASR ECU State Manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  WdgM: yuleASR Watchdog Manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  Det:  yuleASR Development Error Tracer"
    COMMAND ${CMAKE_COMMAND} -E echo "  BswM: yuleASR Basic Software Mode Manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  App:  ICCE Digital Key (yuleDKCS)"
    COMMAND ${CMAKE_COMMAND} -E echo "  MCU:  NXP S32K312"
    COMMAND ${CMAKE_COMMAND} -E echo "================================"
)

message(STATUS "")
# ============================================================================
# Phase 2 自定义目标
# ============================================================================

add_custom_target(bsw_phase2_all
    DEPENDS yuleDKCS_bsw.elf
    COMMENT "✅ BSW Phase 2 构建完成: COM + DCM + DEM"
)

add_custom_target(bsw_phase2_size
    COMMAND arm-none-eabi-size yuleDKCS_bsw.elf
    COMMAND arm-none-eabi-size yuleDKCS_bsw.bin
    DEPENDS yuleDKCS_bsw.elf
    COMMENT "📊 BSW Phase 2 映像大小统计"
)

add_custom_target(bsw_phase2_clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "🧹 清理 BSW Phase 2 构建目录"
)

add_custom_target(bsw_phase2_info
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== yuleDKCS BSW Phase 2 ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  OS:    yuleASR AUTOSAR OS (FreeRTOS)"
    COMMAND ${CMAKE_COMMAND} -E echo "  EcuM:  yuleASR ECU State Manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  WdgM:  yuleASR Watchdog Manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  COM:   yuleASR AUTOSAR COM (CAN Signal Mapping)"
    COMMAND ${CMAKE_COMMAND} -E echo "  PduR:  yuleASR PDU Router"
    COMMAND ${CMAKE_COMMAND} -E echo "  DCM:   yuleASR UDS Diagnostic Stack"
    COMMAND ${CMAKE_COMMAND} -E echo "  DEM:   yuleASR DTC Event Manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  App:   ICCE Digital Key (yuleDKCS)"
    COMMAND ${CMAKE_COMMAND} -E echo "  MCU:   NXP S32K312 (Cortex-M7)"
    COMMAND ${CMAKE_COMMAND} -E echo "==================================="
)

# ============================================================================
# Phase 3 自定义目标
# ============================================================================

add_custom_target(bsw_phase3_all
    DEPENDS yuleDKCS_bsw.elf
    COMMENT "✅ BSW Phase 3 构建完成: NvM + CSM + CryIf + MCAL"
)

add_custom_target(bsw_phase3_size
    COMMAND arm-none-eabi-size yuleDKCS_bsw.elf
    DEPENDS yuleDKCS_bsw.elf
    COMMENT "📊 BSW Phase 3 映像大小统计"
)

add_custom_target(bsw_phase3_clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "🧹 清理 BSW Phase 3 构建目录"
)

add_custom_target(bsw_phase3_info
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== yuleDKCS BSW Phase 3 ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  NvM:   yuleASR NVRAM Manager (正式实现)"
    COMMAND ${CMAKE_COMMAND} -E echo "  MemIf: yuleDKCS Memory Abstraction Interface"
    COMMAND ${CMAKE_COMMAND} -E echo "  CSM:   yuleASR Crypto Services Manager (正式实现)"
    COMMAND ${CMAKE_COMMAND} -E echo "  CryIf: yuleASR Crypto Interface (正式实现)"
    COMMAND ${CMAKE_COMMAND} -E echo "  MCAL:  yuleASR S32K312 驱动 (正式头文件)"
    COMMAND ${CMAKE_COMMAND} -E echo "  BswM:  yuleASR BswM 正式实现 (替换桩)"
    COMMAND ${CMAKE_COMMAND} -E echo "  密钥:  ICCE 主密钥/会话/存储/诊断/安全启动/通信"
    COMMAND ${CMAKE_COMMAND} -E echo "  MCU:   NXP S32K312 (Cortex-M7)"
    COMMAND ${CMAKE_COMMAND} -E echo "==================================="
)

message(STATUS "")
message(STATUS "=== yuleDKCS BSW Phase 3 配置完毕 ===")
message(STATUS "yuleASR: ${YULEASR_ROOT}")
message(STATUS "yuleDKCS: ${YULEDKCS_ROOT}")
message(STATUS "目标 MCU: NXP S32K312 (Cortex-M7)")
message(STATUS "模块: OS + Det + BswM + EcuM + WdgM + COM + PduR + DCM + DEM + NvM + MemIf + CSM + CryIf + MCAL")
message(STATUS "应用: ICCE Digital Key Protocol Stack")
