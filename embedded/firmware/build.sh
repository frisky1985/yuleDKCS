#!/bin/bash
# yuleDKCS KW47A Firmware Build Script
#
# Prerequisites:
#   cmake >= 3.20, arm-none-eabi-gcc, ninja
#   MCUXpresso SDK downloaded to ../mcux-sdk/
#
# Usage:
#   ./build.sh              # Debug build
#   ./build.sh release      # Release build
#   ./build.sh clean        # Clean build

set -e

BUILD_TYPE="${1:-debug}"
BUILD_DIR="build/${BUILD_TYPE}"

case "${BUILD_TYPE}" in
    debug)    CMAKE_BUILD=Debug ;;
    release)  CMAKE_BUILD=Release ;;
    clean)    rm -rf build && echo "Cleaned." && exit 0 ;;
    *)        echo "Usage: $0 [debug|release|clean]" && exit 1 ;;
esac

cmake -B "${BUILD_DIR}"     -DCMAKE_TOOLCHAIN_FILE="${PWD}/cmake/arm-none-eabi.cmake"     -DCMAKE_BUILD_TYPE="${CMAKE_BUILD}"     -GNinja

cmake --build "${BUILD_DIR}" -j$(nproc)

echo ""
echo "=== Firmware Build Complete ==="
ls -lh "${BUILD_DIR}"/yuleDKCS_kw47a.* 2>/dev/null
