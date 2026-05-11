cmake_minimum_required(VERSION 3.10)
project(DKCS VERSION 1.0.0 LANGUAGES C)

# 设置 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 选项
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
