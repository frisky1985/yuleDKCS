# Digital Key Unified Protocol
cmake_minimum_required(VERSION 3.16)
project(dk_unified C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# 源文件
set(DK_UNIFIED_SOURCES
    src/dk_unified.c
)

# 头文件
set(DK_UNIFIED_HEADERS
    include/dk_unified.h
)

# 创建静态库
add_library(dk_unified STATIC ${DK_UNIFIED_SOURCES})

# 包含目录
target_include_directories(dk_unified
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../ccc_protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../iccoa_protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../icce_protocol/include
)

# 链接协议库
target_link_libraries(dk_unified
    PUBLIC
        ccc_protocol
        iccoa_protocol
        icce_protocol
)

# 安装
install(TARGETS dk_unified
    ARCHIVE DESTINATION lib
)

install(FILES ${DK_UNIFIED_HEADERS}
    DESTINATION include
)

# 示例程序
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
