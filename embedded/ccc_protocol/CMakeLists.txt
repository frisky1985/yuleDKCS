cmake_minimum_required(VERSION 3.20)
project(ccc_digital_key C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -Os -ffreestanding)
add_compile_options(-Wno-error=unused-variable -Wno-error=unused-function -Wno-error=unused-parameter)
add_compile_options(-include hardware_abstraction.h)

file(GLOB_RECURSE SOURCES "src/*.c")
include_directories(include src/core src/nfc src/ble src/uwb src/keymgmt src/security)
include_directories(../freestanding_includes)
include_directories(../icce_protocol/src/crypto)  # crypto_engine for SHA-256/SM2 verification

add_library(ccc_dk STATIC ${SOURCES})
