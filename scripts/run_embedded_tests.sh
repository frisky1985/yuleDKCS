#!/usr/bin/env bash
#===============================================================================
# run_embedded_tests.sh — Vehicle C-Code Test Runner
#
# Builds and runs the Unity-based embedded C unit test suite on CI.
# Supports two modes:
#   native       — compiles with host GCC (x86_64/arm64); real CCC/Unified/ICCE
#                  sources are replaced by stubs.c (MCU-specific code needs
#                  cross-compilation toolchain)
#   cross        — compiles with arm-none-eabi-gcc (ARM target); requires
#                  ARM GCC toolchain + MCU SDK headers
#
# Usage:
#   ./scripts/run_embedded_tests.sh [mode]
#     mode: native (default) | cross
#
# Exit codes:
#   0 — all tests pass
#   1 — build or test failure
#===============================================================================

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
EMBEDDED_DIR="$PROJECT_ROOT/embedded"
TESTS_DIR="$EMBEDDED_DIR/tests"
REPORTS_DIR="$PROJECT_ROOT/reports"

MODE="${1:-native}"

echo "============================================"
echo "  Vehicle C-Code Test Runner"
echo "  Mode: $MODE"
echo "  Dir:  $EMBEDDED_DIR"
echo "============================================"

#--- Check prerequisites ------------------------------------------------
case "$MODE" in
    native)
        CC="${CC:-gcc}"
        if ! command -v "$CC" &>/dev/null; then
            echo "ERROR: $CC not found. Install GCC or set CC=..."
            exit 1
        fi
        ;;
    cross)
        CC="${CC:-arm-none-eabi-gcc}"
        if ! command -v "$CC" &>/dev/null; then
            echo "ERROR: $CC not found."
            echo "Install ARM GCC toolchain:"
            echo "  brew install arm-none-eabi-gcc"
            echo "  or download from ARM Developer"
            exit 1
        fi
        ;;
    *)
        echo "ERROR: Unknown mode '$MODE'. Use 'native' or 'cross'."
        exit 1
        ;;
esac

echo "Using compiler: $(command -v "$CC")"
echo ""

#--- Download Unity (if not present) ------------------------------------
UNITY_DIR="$TESTS_DIR/vendor/unity"
if [ ! -f "$UNITY_DIR/unity.h" ]; then
    echo "--- Downloading Unity test framework ---"
    mkdir -p "$UNITY_DIR"
    curl -sL "https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.h" \
        -o "$UNITY_DIR/unity.h"
    curl -sL "https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.c" \
        -o "$UNITY_DIR/unity.c"
    curl -sL "https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity_internals.h" \
        -o "$UNITY_DIR/unity_internals.h"
    echo "Unity downloaded to $UNITY_DIR"
fi

#--- Build and Run Tests ------------------------------------------------
echo "--- Building and running tests ---"
cd "$TESTS_DIR"

# For cross-compilation, add extra -D flags for MCU-specific defines
EXTRA_FLAGS=""
if [ "$MODE" = "cross" ]; then
    EXTRA_FLAGS="CFLAGS_EXTRA=-DARM_CROSS_COMPILE"
    echo "INFO: Cross-compilation mode. MCU-specific defines may be needed."
    echo "      Check embedded/tests/Makefile for CCC/ICCE source compile rules."
fi

# Run make — exit on first failure
if [ "$MODE" = "native" ]; then
    # Native: ICCOA real sources compile; CCC/Unified via stubs
    make -C "$TESTS_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)" \
        test_all CC="$CC" 2>&1
    RESULT=$?
else
    # Cross: all sources compile natively with MCU toolchain
    # Uncomment CCC/ICCE/Unified compile rules in Makefile first
    make -C "$TESTS_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)" \
        test_all CC="$CC" $EXTRA_FLAGS 2>&1
    RESULT=$?
fi

#--- Generate Report ----------------------------------------------------
mkdir -p "$REPORTS_DIR"
REPORT_FILE="$REPORTS_DIR/embedded-tests-verification.md"

{
    echo "# 嵌入式 C 测试验证报告"
    echo ""
    echo "**日期**: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "**编译器**: $CC ($(command -v "$CC"))"
    echo "**运行模式**: $MODE"
    echo ""

    if [ $RESULT -eq 0 ]; then
        echo "## ✅ 所有测试通过"
    else
        echo "## ❌ 存在测试失败 (退出码: $RESULT)"
    fi
    echo ""

    # Parse test results from build output
    echo "| 测试套件 | 用例数 | 结果 |"
    echo "|----------|--------|------|"

    BUILD_DIR="$TESTS_DIR/build"
    for suite_info in "iccoa_core:.iccoa_core_out" "iccoa_ble:.iccoa_ble_out" "ccc:.ccc_out" "unified:.unified_out"; do
        suite_name="${suite_info%%:*}"
        outfile_name="${suite_info##*:}"
        outfile="$BUILD_DIR/$outfile_name"
        if [ -f "$outfile" ]; then
            count=$(grep -oE '[0-9]+ Tests' "$outfile" 2>/dev/null | grep -oE '[0-9]+' || echo "?")
            failures=$(grep -oE '[0-9]+ Failures' "$outfile" 2>/dev/null | grep -oE '[0-9]+' || echo "?")
            if [ "$failures" = "0" ]; then
                result_str="✅ 通过"
            else
                result_str="❌ 失败 ($failures)"
            fi
            echo "| $suite_name | $count | $result_str |"
        fi
    done

    if [ ! -f "$BUILD_DIR/.iccoa_core_out" ]; then
        echo ""
        echo "> 详细结果请查看构建日志。"
    fi
    echo ""

    echo "## 测试用例清单"
    echo ""
    echo "### ICCOA 数字钥匙核心 ($(grep -c 'RUN_TEST(' "$TESTS_DIR/test/test_iccoa_dk_core.c" || echo 0) 用例)"
    echo "\`\`\`"
    grep 'void test_' "$TESTS_DIR/test/test_iccoa_dk_core.c" | sed 's/.*void test_/  - test_/' | sed 's/(void)//'
    echo "\`\`\`"
    echo ""

    echo "### ICCOA BLE ($(grep -c 'RUN_TEST(' "$TESTS_DIR/test/test_iccoa_ble.c" || echo 0) 用例)"
    echo "\`\`\`"
    grep 'void test_' "$TESTS_DIR/test/test_iccoa_ble.c" | sed 's/.*void test_/  - test_/' | sed 's/(void)//'
    echo "\`\`\`"
    echo ""

    echo "### CCC 数字钥匙核心 ($(grep -c 'RUN_TEST(' "$TESTS_DIR/test/test_ccc_dk_core.c" || echo 0) 用例)"
    echo "\`\`\`"
    grep 'void test_' "$TESTS_DIR/test/test_ccc_dk_core.c" | sed 's/.*void test_/  - test_/' | sed 's/(void)//'
    echo "\`\`\`"
    echo ""

    echo "### 统一接口 ($(grep -c 'RUN_TEST(' "$TESTS_DIR/test/test_unified.c" || echo 0) 用例)"
    echo "\`\`\`"
    grep 'void test_' "$TESTS_DIR/test/test_unified.c" | sed 's/.*void test_/  - test_/' | sed 's/(void)//'
    echo "\`\`\`"
    echo ""

    echo "## 工具链要求"
    echo ""
    echo "### Native (x86_64 / arm64)"
    echo "- GCC 或 Clang (已安装: $(gcc --version 2>/dev/null | head -1 || clang --version 2>/dev/null | head -1))"
    echo "- Unity 测试框架 (自动下载到 \`tests/vendor/unity/\`)"
    echo "- ICCOA 协议源文件 + stubs.c 提供 CCC/Unified API (MCU 相关代码由 Stub 替代)"
    echo ""
    echo "### ARM 交叉编译"
    echo "- \`arm-none-eabi-gcc\` 工具链"
    echo "- MCU SDK 头文件 (如 S32K312, KW47A, NCJ29D6 等)"
    echo "- 需要启用在 Makefile 中注释的 CCC/ICCE/Unified 源文件编译规则"
    echo ""

    echo "## 注意事项"
    echo ""
    echo "1. **测试文件不自带 return value**: \`run_*_tests()\` 调用 \`UNITY_END()\` 但未返回其值。"
    echo "   CI 脚本通过解析 Unity stdout 中的 \"0 Failures\" 字符串来判断通过/失败。"
    echo "2. **CCC BLE/UWB/NFC/Security 源文件**: MCU 专用寄存器定义 (如 \`KW47A_CS_PORT\`) 在"
    echo "   x86_64/arm64 上不可用。这些模块由 \`tests/support/stubs.c\` 中的 mock 实现替代。"
    echo "3. **统一接口源文件**: 依赖 ICCE 头文件中非标准类型 (如 \`sint32\`)。"
    echo "   Stub 实现 (\`stubs.c\`) 提供完整替代 API。"
    echo "4. **编译日志**: 构建产物在 \`embedded/tests/build/\` 目录。"
} > "$REPORT_FILE"

echo ""
echo "Report saved to: $REPORT_FILE"
exit $RESULT
