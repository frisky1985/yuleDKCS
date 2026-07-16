cmake_minimum_required(VERSION 3.20)
project(iccoa_digital_key C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# [P0-05 FIX] 添加 -ffreestanding 和 freestanding 头文件路径
add_compile_options(-Wall -Wextra -Werror -Os -ffreestanding)
add_compile_options(-Wno-error=unused-variable -Wno-error=unused-function -Wno-error=unused-parameter)

file(GLOB_RECURSE SOURCES "src/*.c")
include_directories(include)
include_directories(../freestanding_includes)  # [P0-05 FIX] freestanding string.h/stdlib.h
include_directories(../system_architecture)      # [P0-05 FIX] sys_time.h

add_library(iccoa_dk STATIC ${SOURCES})
