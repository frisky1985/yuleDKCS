cmake_minimum_required(VERSION 3.20)
project(yuleDKCS-firmware C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Target name ─────────────────────────────────────────────
set(TARGET yuleDKCS_fw)
set(FW_VERSION "2.3.0-rc1")

# ── Options ─────────────────────────────────────────────────
option(BUILD_CCC          "Build CCC protocol stack"   ON)
option(BUILD_ICCE         "Build ICCE protocol stack"   ON)
option(BUILD_ICCOA        "Build ICCOA protocol stack"  ON)
option(USE_SE050          "Use SE050 secure element"    ON)
option(FIRMWARE_HOST_SIM  "Build host-simulation firmware (no MCU/SDK)" OFF)
option(BRINGUP_SELFTEST   "Run hardware bring-up self-test at boot"   OFF)
option(SECURE_BOOT        "Enable secure boot gate"                    OFF)

# Auto-detect: if no cross-compiler, default to host simulation so the
# firmware is buildable in CI / developer environments without NXP SDK.
if(NOT CMAKE_CROSSCOMPILING AND NOT FIRMWARE_HOST_SIM)
    set(FIRMWARE_HOST_SIM ON)
    message(STATUS "yuleDKCS firmware: HOST SIMULATION build (cross-compiler not detected)")
endif()

# ── HAL ─────────────────────────────────────────────────────
add_subdirectory(hal)

# ── Protocol stacks (define static libs) ────────────────────
set(PROTOCOL_LIBS "")
set(PROTOCOL_INCLUDES "")

if(BUILD_CCC)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../ccc_protocol ccc)
    list(APPEND PROTOCOL_LIBS ccc_dk)
    list(APPEND PROTOCOL_INCLUDES ${CMAKE_SOURCE_DIR}/../ccc_protocol/include)
endif()
if(BUILD_ICCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../icce_protocol icce)
    list(APPEND PROTOCOL_LIBS icce_dk)
    list(APPEND PROTOCOL_INCLUDES ${CMAKE_SOURCE_DIR}/../icce_protocol/include)
endif()
if(BUILD_ICCOA)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../iccoa_protocol iccoa)
    list(APPEND PROTOCOL_LIBS iccoa_dk)
    list(APPEND PROTOCOL_INCLUDES ${CMAKE_SOURCE_DIR}/../iccoa_protocol/include)
endif()

# ── Application (creates ${TARGET}) ─────────────────────────
add_subdirectory(src)

# Propagate protocol include directories to the application.
target_include_directories(${TARGET} PRIVATE ${PROTOCOL_INCLUDES})

# Link HAL + protocol stacks.
target_link_libraries(${TARGET} PRIVATE hal ${PROTOCOL_LIBS})

# Common compile definitions.
target_compile_definitions(${TARGET} PRIVATE FW_VERSION="${FW_VERSION}")
if(BRINGUP_SELFTEST)
    target_compile_definitions(${TARGET} PRIVATE BRINGUP_SELFTEST=1)
endif()
if(SECURE_BOOT)
    target_compile_definitions(${TARGET} PRIVATE SECURE_BOOT=1)
endif()
if(BUILD_CCC)
    target_compile_definitions(${TARGET} PRIVATE BUILD_CCC=1)
endif()
if(BUILD_ICCE)
    target_compile_definitions(${TARGET} PRIVATE BUILD_ICCE=1)
endif()
if(BUILD_ICCOA)
    target_compile_definitions(${TARGET} PRIVATE BUILD_ICCOA=1)
endif()

set_target_properties(${TARGET} PROPERTIES SUFFIX ".elf" PREFIX "")

# ── Mode-specific configuration ─────────────────────────────
if(FIRMWARE_HOST_SIM)
    # Host simulation: no MCU flags, no NXP SDK. Useful for CI build
    # integrity and protocol-stack link verification.
    target_compile_definitions(${TARGET} PRIVATE FIRMWARE_HOST_SIM=1)
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra)
else()
    # Real target: NXP KW47A, MCUXpresso SDK, ARM Cortex-M33.
    set(MCU_CORE cortex-m33)
    include(FetchContent)
    FetchContent_Declare(
        mcux_sdk
        GIT_REPOSITORY https://github.com/NXP/mcux-sdk.git
        GIT_TAG        release/2.16.x
    )
    FetchContent_MakeAvailable(mcux_sdk)

    target_compile_definitions(${TARGET} PRIVATE USE_SE050=1)
    target_compile_options(${TARGET} PRIVATE
        -mcpu=${MCU_CORE} -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
        -Wall -Wextra -Wpedantic -Werror
        -fstack-protector-strong -fstack-clash-protection
        $<$<CONFIG:DEBUG>:-Og -g3>
        $<$<CONFIG:RELEASE>:-O2 -flto>)

    target_link_options(${TARGET} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/hal/kw47a/kw47a_flash.ld
        -mcpu=${MCU_CORE} -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
        --specs=nano.specs --specs=nosys.specs
        $<$<CONFIG:RELEASE>:-flto>)

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}> ${TARGET}.hex
        COMMAND ${CMAKE_SIZE}    $<TARGET_FILE:${TARGET}>
        COMMENT "Generating firmware binaries (bin/hex)...")
endif()
