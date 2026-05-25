# yuleDKCS 密码学库 CMake 构建配置
# ============================================================
# 构建 mbedtls (精简) + SM2/SM3/SM4 国密模块
#
# 用法:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_kw47.cmake
#   make -C build
# ============================================================

cmake_minimum_required(VERSION 3.14)
project(yuledkcs_crypto C)

# ---- 输出目录 ----
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---- 编译选项 ----
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- mbedtls 精简库 ----
# 使用项目自带的 mbedtls 源码，自定义配置
set(MBEDTLS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/mbedtls_config.h")

add_library(mbedcrypto STATIC
    mbedtls/library/aes.c
    mbedtls/library/aesni.c
    mbedtls/library/asn1parse.c
    mbedtls/library/asn1write.c
    mbedtls/library/base64.c
    mbedtls/library/bignum.c
    mbedtls/library/ctr_drbg.c
    mbedtls/library/ecdsa.c
    mbedtls/library/ecp.c
    mbedtls/library/ecp_curves.c
    mbedtls/library/entropy.c
    mbedtls/library/entropy_poll.c
    mbedtls/library/gcm.c
    mbedtls/library/hkdf.c
    mbedtls/library/md.c
    mbedtls/library/oid.c
    mbedtls/library/pem.c
    mbedtls/library/pk.c
    mbedtls/library/pkparse.c
    mbedtls/library/pkwrite.c
    mbedtls/library/platform.c
    mbedtls/library/platform_util.c
    mbedtls/library/sha256.c
    mbedtls/library/x509.c
    mbedtls/library/x509_crt.c
)

target_include_directories(mbedcrypto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/include>
    $<INSTALL_INTERFACE:include/mbedtls>
)

target_compile_definitions(mbedcrypto PRIVATE
    MBEDTLS_CONFIG_FILE="${MBEDTLS_CONFIG_FILE}"
)

# ---- SM2/SM3/SM4 国密模块 ----
add_library(sm_crypto STATIC
    sm/src/sm2.c
    sm/src/sm3.c
    sm/src/sm4.c
)

target_include_directories(sm_crypto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sm/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(sm_crypto PUBLIC mbedcrypto)

# ---- 输出库文件 ----
set(LIBRARIES
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libmbedcrypto.a
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libsm_crypto.a
)

# ---- 安装 ----
install(TARGETS mbedcrypto sm_crypto
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
