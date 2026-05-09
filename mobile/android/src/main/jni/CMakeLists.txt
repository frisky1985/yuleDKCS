cmake_minimum_required(VERSION 3.22.1)

project("yuledkcs")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add native library
add_library(
    native-lib
    SHARED
    native-lib.cpp
)

# Find and link required libraries
find_library(
    log-lib
    log
)

find_library(
    android-lib
    android
)

# Find OpenSSL or use bundled crypto
# For production, link against system OpenSSL or BoringSSL
find_package(OpenSSL QUIET)

if(OpenSSL_FOUND)
    target_link_libraries(
        native-lib
        OpenSSL::SSL
        OpenSSL::Crypto
        ${log-lib}
        ${android-lib}
    )
else()
    # Fallback: link with log library only
    # In production, vendor OpenSSL or use BoringSSL
    target_link_libraries(
        native-lib
        ${log-lib}
        ${android-lib}
    )
endif()

# Compiler flags for security
set_target_properties(
    native-lib
    PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_compile_options(
    native-lib
    PRIVATE
    -O2
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
    -Wformat
    -Wformat-security
)
