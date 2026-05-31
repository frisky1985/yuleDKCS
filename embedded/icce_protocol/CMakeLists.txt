cmake_minimum_required(VERSION 3.20)
project(icce_digital_key C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -Os)

file(GLOB_RECURSE SOURCES "src/*.c")
include_directories(include)
include_directories(src/crypto)   # 国密算法层

# 使用国密算法 (注释掉此项则默认使用 P-256/SHA-256/AES-256-GCM)
# add_compile_definitions(USE_SM_CRYPTO)

add_library(icce_dk STATIC ${SOURCES})
