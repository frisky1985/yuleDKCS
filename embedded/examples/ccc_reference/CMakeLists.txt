###############################################################################
# CMakeLists.txt - CCC Reference 示例构建文件
#
# 本示例展示在 yuleDKCS 嵌入式平台上的 CCC 数字钥匙参考实现。
# 支持两种构建模式:
#   1. 作为 yuleDKCS 子项目 (add_subdirectory)
#   2. 独立构建 (standalone)
#
# 依赖:
#   - yuleDKCS (必需): embedded/lib/libDKCS.a 或嵌入式设备上的 DKCS 库
#   - yuleASR  (可选): yuleASR BSW 头文件 (OS/CSM/NvM/DCM)
#
# 使用方式:
#   cd yuleDKCS/embedded
#   mkdir -p build && cd build
#   cmake .. -DBUILD_EXAMPLES=ON -DENABLE_CCC=ON
#   make ccc_reference
#
# 独立构建:
#   cd yuleDKCS/embedded/examples/ccc_reference
#   mkdir -p build && cd build
#   cmake .. -DDKCS_ROOT=/path/to/yuleDKCS/embedded
#   make
#
# @author yuleDKCS Team
# @version 1.0.0
###############################################################################

cmake_minimum_required(VERSION 3.10)

# ── 项目名称 ─────────────────────────────────────────────────────────────
# 当作为 subdirectory 构建时,项目名称由父级 CMakeLists.txt 管理
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ccc_reference VERSION 1.0.0 LANGUAGES C)
endif()

# ── C 标准 ────────────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── 源文件列表 ──────────────────────────────────────────────────────────
set(CCC_REFERENCE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CccDigitalKey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CccKeyAgreement.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CccAuthentication.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CccSecureChannel.c
)

set(CCC_REFERENCE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CccDigitalKey.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CccTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CccIntegration.h
)

# ── 查找 yuleDKCS 依赖 ──────────────────────────────────────────────────

# 方式1: 作为 yuleDKCS 子项目构建 (由父级 Examples/CMakeLists.txt 调用)
#        此时 DKCS 库和头文件路径已由父级提供
if(TARGET DKCS)
    message(STATUS "ccc_reference: building as DKCS subproject")
    set(CCC_REFERENCE_LIBS DKCS)
    set(CCC_REFERENCE_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
else()
    # 方式2: 独立构建
    #        需要指定 DKCS_ROOT 指向 yuleDKCS/embedded 目录
    if(NOT DKCS_ROOT)
        set(DKCS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../" CACHE PATH "yuleDKCS embedded root")
    endif()

    message(STATUS "ccc_reference: standalone build, DKCS_ROOT=${DKCS_ROOT}")

    # 查找 DKCS 库
    find_library(DKCS_LIBRARY
        NAMES DKCS libDKCS.a
        PATHS
            ${DKCS_ROOT}/build
            ${DKCS_ROOT}/lib
            ${DKCS_ROOT}/build/lib
        NO_DEFAULT_PATH
    )

    if(NOT DKCS_LIBRARY)
        message(WARNING "DKCS library not found at ${DKCS_ROOT}/build/")
        message(WARNING "Build DKCS first: cd ${DKCS_ROOT} && mkdir build && cd build && cmake .. && make")
        # 仍然创建目标但标记为不可构建
        set(CCC_REFERENCE_SKIP_LINK TRUE)
    endif()

    set(CCC_REFERENCE_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${DKCS_ROOT}/include
    )

    # 方式3: 可选 - 查找 yuleASR BSW 头文件
    if(DEFINED YULE_ASR_ROOT)
        message(STATUS "ccc_reference: yuleASR BSW integration at ${YULE_ASR_ROOT}")
        list(APPEND CCC_REFERENCE_INCLUDES
            ${YULE_ASR_ROOT}/src/bsw/csm
            ${YULE_ASR_ROOT}/src/bsw/det
            ${YULE_ASR_ROOT}/src/bsw/os
            ${YULE_ASR_ROOT}/src/bsw/nvm
            ${YULE_ASR_ROOT}/src/bsw/dcm
            ${YULE_ASR_ROOT}/src/bsw/ecum
        )
        add_definitions(-DCCC_USE_CSM=STD_ON)
        add_definitions(-DCCC_DEV_ERROR_DETECT=STD_ON)
        add_definitions(-DCCC_VERSION_INFO_API=STD_ON)
    else()
        message(STATUS "ccc_reference: building without yuleASR (simulation mode)")
        add_definitions(-DCCC_USE_CSM=STD_OFF)
    endif()
endif()

# ── 编译选项 ────────────────────────────────────────────────────────────
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ccc_reference PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=implicit-function-declaration
        -Werror=return-type
        -O0                      # 调试优化级别
        -g                       # 调试信息
    )
endif()

# ── 创建目标 ────────────────────────────────────────────────────────────
if(NOT TARGET ccc_reference)
    add_executable(ccc_reference ${CCC_REFERENCE_SOURCES})

    target_include_directories(ccc_reference
        PRIVATE
            ${CCC_REFERENCE_INCLUDES}
    )

    if(CCC_REFERENCE_SKIP_LINK)
        # DKCS 库未找到,跳过链接步骤
        message(WARNING "ccc_reference: skipping link step (DKCS not available)")
    else()
        target_link_libraries(ccc_reference
            PRIVATE
                ${CCC_REFERENCE_LIBS}
                ${DKCS_LIBRARY}
                m       # math library (for potential HMAC/math usage)
        )
    endif()

    # 安装目标
    install(TARGETS ccc_reference
        RUNTIME DESTINATION bin
    )

    # 安装头文件
    install(FILES ${CCC_REFERENCE_HEADERS}
        DESTINATION include/ccc_reference
    )

    # 自定义目标: 显示集成说明
    add_custom_target(ccc_reference_info
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "============================================"
        COMMAND ${CMAKE_COMMAND} -E echo "  CCC Reference Example - Integration Info"
        COMMAND ${CMAKE_COMMAND} -E echo "============================================"
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "  yuleDKCS ccc_core:  ${DKCS_ROOT}/src/ccc/"
        COMMAND ${CMAKE_COMMAND} -E echo "  yuleDKCS headers:   ${DKCS_ROOT}/include/"
        COMMAND ${CMAKE_COMMAND} -E echo "  yuleASR BSW:        ${YULE_ASR_ROOT}"
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "  Build: make ccc_reference"
        COMMAND ${CMAKE_COMMAND} -E echo "  Run:   ./ccc_reference"
        COMMAND ${CMAKE_COMMAND} -E echo ""
    )
endif()

# ── 独立构建时的测试支持 ──────────────────────────────────────────────
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND BUILD_TESTING)
    enable_testing()
    add_test(NAME ccc_reference_smoke
        COMMAND ccc_reference
    )
endif()
