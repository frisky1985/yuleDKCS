cmake_minimum_required(VERSION 3.14)
project(DKCS VERSION 1.0.0 LANGUAGES C)

# 设置 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# yuleASR BSW 平台依赖
# ============================================================
# yuleDKCS 运行在 yuleASR (AutoSAR BSW) 平台之上。
# yuleASR 提供 HAL/MCAL 抽象层、OS 调度、通信栈等基础服务。
#
# 配置方式 (优先级从高到低):
#   1. 通过 -DYULEASR_ROOT= 指定路径
#   2. 通过环境变量 YULEASR_ROOT 指定
#   3. 自动检测常见路径 (/opt/yuleASR, ~/yuleASR)
#   4. 若以上均无效，设置 WITHOUT_YULEASR=ON 跳过
# ============================================================
option(WITHOUT_YULEASR "Build without yuleASR BSW dependency (standalone crypto SDK only)" OFF)

if(NOT WITHOUT_YULEASR)
    # 优先级 1: CMake 变量
    if(YULEASR_ROOT)
        set(_YULEASR_ROOT "${YULEASR_ROOT}")
    # 优先级 2: 环境变量
    elseif(DEFINED ENV{YULEASR_ROOT})
        set(_YULEASR_ROOT "$ENV{YULEASR_ROOT}")
    # 优先级 3: 自动检测
    elseif(IS_DIRECTORY "/opt/yuleASR")
        set(_YULEASR_ROOT "/opt/yuleASR")
    elseif(IS_DIRECTORY "$ENV{HOME}/yuleASR")
        set(_YULEASR_ROOT "$ENV{HOME}/yuleASR")
    endif()

    if(_YULEASR_ROOT AND IS_DIRECTORY "${_YULEASR_ROOT}")
        message(STATUS "yuleASR BSW platform found: ${_YULEASR_ROOT}")

        # 方式 A: 若 yuleASR 支持 CMake 包配置，优先使用 find_package
        if(EXISTS "${_YULEASR_ROOT}/cmake/yuleASRConfig.cmake"
           OR EXISTS "${_YULEASR_ROOT}/install/lib/cmake/yuleASR/yuleASRConfig.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${_YULEASR_ROOT}"
                 "${_YULEASR_ROOT}/install")
            find_package(yuleASR REQUIRED COMPONENTS bsw hal com)
            set(YULEASR_FOUND TRUE)
            message(STATUS "yuleASR found via find_package")

        # 方式 B: 作为 subdirectory 添加
        elseif(EXISTS "${_YULEASR_ROOT}/CMakeLists.txt")
            add_subdirectory("${_YULEASR_ROOT}" "${CMAKE_BINARY_DIR}/yuleASR")
            set(YULEASR_FOUND TRUE)
            message(STATUS "yuleASR added as subdirectory")

        # 方式 C: 手动指定头文件目录和库路径
        else()
            set(YULEASR_FOUND TRUE)
            set(YULEASR_INCLUDE_DIRS
                "${_YULEASR_ROOT}/include"
                "${_YULEASR_ROOT}/install/include"
            )
            set(YULEASR_LIB_DIRS
                "${_YULEASR_ROOT}/lib"
                "${_YULEASR_ROOT}/install/lib"
            )
            message(STATUS "yuleASR using manual include/lib paths")
        endif()

        # 设置编译宏，应用层可通过 #ifdef YULEASR_PLATFORM 来条件编译
        add_definitions(-DYULEASR_PLATFORM)
    else()
        message(WARNING "yuleASR not found. Set YULEASR_ROOT or run ./scripts/setup.sh. "
                        "Building without BSW support.")
        set(WITHOUT_YULEASR ON)
    endif()
else()
    message(STATUS "yuleASR dependency disabled (WITHOUT_YULEASR=ON)")
endif()

# ============================================================
# 项目选项
# ============================================================
option(ENABLE_CCC "Enable CCC Digital Key R3 support" ON)
option(ENABLE_ICCE "Enable ICCE support" ON)
option(ENABLE_ICCOA "Enable ICCOA support" ON)
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_EXAMPLES "Build example programs" ON)

# 源文件列表
set(DKCS_SOURCES
    src/common/dkcs.c
)

# 根据选项添加协议实现
if(ENABLE_CCC)
    list(APPEND DKCS_SOURCES
        src/ccc/protocol/ccc_core.c
    )
    add_definitions(-DENABLE_CCC)
endif()

if(ENABLE_ICCE)
    list(APPEND DKCS_SOURCES
        src/icce/protocol/icce_core.c
    )
    add_definitions(-DENABLE_ICCE)
endif()

if(ENABLE_ICCOA)
    list(APPEND DKCS_SOURCES
        src/iccoa/protocol/iccoa_core.c
    )
    add_definitions(-DENABLE_ICCOA)
endif()

# 创建静态库
add_library(DKCS STATIC ${DKCS_SOURCES})

target_include_directories(DKCS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 引用 SDK 密码学库 (mbedTLS + SM2/SM3/SM4 国密)
add_subdirectory(sdk)

# 链接 yuleASR BSW 平台库
if(YULEASR_FOUND)
    # 方式 A/B: find_package / add_subdirectory 方式
    if(TARGET yuleASR::bsw)
        target_link_libraries(DKCS PUBLIC yuleASR::bsw yuleASR::hal yuleASR::com)
        target_include_directories(DKCS PUBLIC
            $<BUILD_INTERFACE:${_YULEASR_ROOT}/include>
        )
    elseif(TARGET yuleASR)
        target_link_libraries(DKCS PUBLIC yuleASR)
    # 方式 C: 手动路径
    elseif(YULEASR_INCLUDE_DIRS)
        target_include_directories(DKCS PUBLIC ${YULEASR_INCLUDE_DIRS})
        target_link_directories(DKCS PUBLIC ${YULEASR_LIB_DIRS})
        target_link_libraries(DKCS PUBLIC yuleASR_bsw yuleASR_hal)
    endif()

    message(STATUS "DKCS linked with yuleASR BSW platform")
endif()

# SDK 密码学库始终链接
target_link_libraries(DKCS PUBLIC
    mbedcrypto      # mbedtls 精简库 (ECDSA/SHA256/ASN.1)
    sm_crypto       # SM2/SM3/SM4 国密模块
)

# 设置编译选项
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(DKCS PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
        -fPIC
    )
endif()

# 安装头文件
set(PUBLIC_HEADERS
    include/dkcs.h
)

if(ENABLE_CCC)
    list(APPEND PUBLIC_HEADERS include/ccc.h)
endif()

if(ENABLE_ICCE)
    list(APPEND PUBLIC_HEADERS include/icce.h)
endif()

if(ENABLE_ICCOA)
    list(APPEND PUBLIC_HEADERS include/iccoa.h)
endif()

set_target_properties(DKCS PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 测试程序
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 模糊测试程序
option(ENABLE_FUZZING "Build AFL++ fuzzing targets" OFF)
if(ENABLE_FUZZING)
    message(STATUS "Fuzzing targets enabled")
    add_subdirectory(tests/fuzzing)
endif()

# 示例程序
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装
include(GNUInstallDirs)

install(TARGETS DKCS
    EXPORT DKCSTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DKCS
)

install(EXPORT DKCSTargets
    FILE DKCSTargets.cmake
    NAMESPACE DKCS::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DKCS
)

# 生成配置文件
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DKCSConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DKCSConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DKCSConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DKCSConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DKCSConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DKCS
)

# 打包配置
set(CPACK_PACKAGE_NAME "DKCS")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Digital Key Connectivity Stack")
set(CPACK_PACKAGE_VENDOR "YuleTech")

include(CPack)
