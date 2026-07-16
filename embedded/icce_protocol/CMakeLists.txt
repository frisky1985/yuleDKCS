cmake_minimum_required(VERSION 3.20)
project(icce_digital_key C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -Os -ffreestanding)
add_compile_options(-Wno-error=unused-variable -Wno-error=unused-function -Wno-error=unused-parameter)
add_compile_options(-Wno-error=sign-compare)  # offline_decision.c has int vs uint32_t
add_compile_definitions(CONFIG_ENABLE_CRYPTO)

file(GLOB_RECURSE SOURCES "src/*.c")
include_directories(include)
include_directories(src/crypto)   # 国密算法层
include_directories(src/ble)
include_directories(src/cache)
include_directories(src/decision)
include_directories(src/security)
include_directories(src/vehicle)
include_directories(../system_architecture)  # sys_time.h
include_directories(../freestanding_includes)  # string.h, stdlib.h stubs for freestanding

# 使用国密算法 (注释掉此项则默认使用 P-256/SHA-256/AES-256-GCM)
# add_compile_definitions(USE_SM_CRYPTO)

add_library(icce_dk STATIC ${SOURCES})
