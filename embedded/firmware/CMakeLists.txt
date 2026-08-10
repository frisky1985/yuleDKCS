cmake_minimum_required(VERSION 3.20)
project(yuleDKCS-firmware C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Target ──────────────────────────────────────────────────
set(TARGET yuleDKCS_kw47a)
set(MCU KW47A)
set(MCU_CORE cortex-m33)

# ── Options ──────────────────────────────────────────────────
option(BUILD_CCC   "Build CCC protocol stack"   ON)
option(BUILD_ICCE  "Build ICCE protocol stack"   ON)
option(BUILD_ICCOA "Build ICCOA protocol stack"  ON)
option(USE_SE050   "Use SE050 secure element"    ON)
option(USE_FREERTOS "Use FreeRTOS"              ON)
option(ENABLE_MISRA "Enable MISRA checks"        OFF)

# ── Directories ──────────────────────────────────────────────
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# ── Hardware Abstraction Layer ───────────────────────────────
add_subdirectory(hal)

# ── Protocol Stacks ──────────────────────────────────────────
if(BUILD_CCC)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../ccc_protocol ccc)
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/../ccc_protocol/include)
endif()

if(BUILD_ICCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../icce_protocol icce)
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/../icce_protocol/include)
endif()

if(BUILD_ICCOA)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../iccoa_protocol iccoa)
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/../iccoa_protocol/include)
endif()

# ── Application ──────────────────────────────────────────────
add_subdirectory(src)

# ── SDK (external, download on first build) ──────────────────
include(FetchContent)
FetchContent_Declare(
    mcux_sdk
    GIT_REPOSITORY https://github.com/NXP/mcux-sdk.git
    GIT_TAG        release/2.16.x
)
FetchContent_MakeAvailable(mcux_sdk)

# ── Linker ───────────────────────────────────────────────────
target_link_options(${TARGET} PRIVATE
    -T ${CMAKE_SOURCE_DIR}/hal/kw47a/kw47a_flash.ld
    --specs=nano.specs --specs=nosys.specs)

# ── Compiler Flags ───────────────────────────────────────────
target_compile_options(${TARGET} PRIVATE
    -mcpu=${MCU_CORE} -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
    -Wall -Wextra -Wpedantic -Werror
    -fstack-protector-strong -fstack-clash-protection
    $<$<CONFIG:DEBUG>:-Og -g3>
    $<$<CONFIG:RELEASE>:-O2 -flto>)

target_link_options(${TARGET} PRIVATE
    -mcpu=${MCU_CORE} -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
    $<$<CONFIG:RELEASE>:-flto>)

# ── Generated Firmware ───────────────────────────────────────
set_target_properties(${TARGET} PROPERTIES
    SUFFIX ".elf"
    PREFIX "")

add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   ${TARGET}.elf ${TARGET}.hex
    COMMAND ${CMAKE_SIZE}    ${TARGET}.elf
    COMMENT "Generating firmware binaries...")
