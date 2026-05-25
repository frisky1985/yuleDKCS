#!/bin/bash
# ============================================================
# yuleDKCS 密码学库交叉编译脚本
# 目标: NXP KW47 (ARM Cortex-M33)
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="${SCRIPT_DIR}"

# 工具链路径 (根据实际安装位置修改)
TOOLCHAIN_PATH="${TOOLCHAIN_PATH:-/tmp/gcc-arm-none-eabi-10.3-2021.10}"
TOOLCHAIN_FILE="${SDK_DIR}/cmake/toolchain_kw47.cmake"

# 编译选项
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${SDK_DIR}/build"
OUTPUT_DIR="${SDK_DIR}/output"

echo "============================================"
echo " yuleDKCS 密码学库交叉编译"
echo " 工具链: ${TOOLCHAIN_PATH}"
echo " 构建类型: ${BUILD_TYPE}"
echo "============================================"

# 检查工具链
if [ ! -f "${TOOLCHAIN_PATH}/bin/arm-none-eabi-gcc" ]; then
    echo "⚠️  工具链未找到: ${TOOLCHAIN_PATH}"
    echo "   请设置 TOOLCHAIN_PATH 环境变量指向 ARM GCC 工具链"
    echo "   下载地址: https://developer.arm.com/downloads/-/gnu-rm"
    echo ""
    echo "   示例:"
    echo "   wget https://.../gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2"
    echo "   tar xf gcc-arm-none-eabi-*.tar.bz2 -C /opt/"
    echo "   export TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-10.3-2021.10"
    exit 1
fi

export PATH="${TOOLCHAIN_PATH}/bin:${PATH}"

# 清理
rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"

# 配置
cmake -S "${SDK_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_C_FLAGS="-mcpu=cortex-m33 -mthumb -mfloat-abi=soft -Os -ffunction-sections -fdata-sections"

# 编译
cmake --build "${BUILD_DIR}" -j$(nproc)

# 输出
mkdir -p "${OUTPUT_DIR}"
cp "${BUILD_DIR}/lib/libmbedcrypto.a" "${OUTPUT_DIR}/"
cp "${BUILD_DIR}/lib/libsm_crypto.a" "${OUTPUT_DIR}/"

echo ""
echo "✅ 编译完成！"
echo "   输出: ${OUTPUT_DIR}/"
ls -lh "${OUTPUT_DIR}/"
