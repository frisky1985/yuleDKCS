cmake_minimum_required(VERSION 3.20)
project(secboot_tool C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(secboot_tool
    secboot_tool.c
    ${ROOT}/secure_boot/src/secure_boot.c
    ${ROOT}/icce_protocol/src/crypto/crypto_engine.c
    ${ROOT}/icce_protocol/src/crypto/crypto_utils.c
    ${ROOT}/icce_protocol/src/crypto/p256.c
    ${ROOT}/icce_protocol/src/crypto/sm2.c
    ${ROOT}/icce_protocol/src/crypto/sm3.c
    ${ROOT}/icce_protocol/src/crypto/sm4.c
    ${ROOT}/ccc_protocol/src/security/crypto_random.c
)

target_include_directories(secboot_tool PRIVATE
    ${ROOT}/secure_boot/include
    ${ROOT}/icce_protocol/include
    ${ROOT}/icce_protocol/src/crypto
    ${ROOT}/ccc_protocol/include
)

target_compile_definitions(secboot_tool PRIVATE
    CONFIG_ENABLE_CRYPTO
    USE_SM_CRYPTO
)

target_compile_options(secboot_tool PRIVATE -Wall -Wextra)
target_link_libraries(secboot_tool PRIVATE m)
