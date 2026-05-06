cmake_minimum_required(VERSION 3.20)
project(ccc_digital_key C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -Os)

file(GLOB_RECURSE SOURCES "src/*.c")
include_directories(include src/core src/nfc src/ble src/uwb src/keymgmt src/security)

add_library(ccc_dk STATIC ${SOURCES})
