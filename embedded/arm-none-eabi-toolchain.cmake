# ARM Cross-Compiler Toolchain for arm-none-eabi (bare-metal)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Prevents cmake from injecting -arch flags on macOS
set(CMAKE_SYSTEM_NAME Generic CACHE INTERNAL "Target system")

set(CMAKE_C_COMPILER /opt/homebrew/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /opt/homebrew/bin/arm-none-eabi-g++)

# Only static library linking - no need for linker tests
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
