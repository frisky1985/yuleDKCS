#!/bin/bash
#
# yuleDKCS Embedded 构建脚本 (含 yuleASR BSW 依赖)
#
# 用法:
#   ./scripts/build.sh                    # Release 构建
#   ./scripts/build.sh --debug            # Debug 构建
#   ./scripts/build.sh --clean            # 清理并重建
#   ./scripts/build.sh --help             # 帮助
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
EMBEDDED_DIR="${PROJECT_ROOT}/embedded"
BUILD_DIR="${EMBEDDED_DIR}/build"

# 默认配置
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN_BUILD=false
EXTRA_CMAKE_ARGS=()

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC}  $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

print_usage() {
    cat << EOF
用法: $0 [选项]

选项:
  --debug, -d      Debug 构建模式 (默认: Release)
  --clean, -c      清理旧的构建目录后重建
  --toolchain PATH 指定工具链文件路径
  --help, -h       显示此帮助信息

环境变量:
  YULEASR_ROOT     yuleASR 根目录 (若未设置则自动检测)
  BUILD_TYPE       构建类型 (Release/Debug)
  CMAKE_TOOLCHAIN_FILE  交叉编译工具链

示例:
  $0                          # Release 构建
  $0 --debug --clean          # Debug 模式，清理重建
  YULEASR_ROOT=/opt/yuleASR $0  # 指定 yuleASR 路径
EOF
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug|-d)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean|-c)
            CLEAN_BUILD=true
            shift
            ;;
        --toolchain)
            EXTRA_CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$2")
            shift 2
            ;;
        --help|-h)
            print_usage
            exit 0
            ;;
        *)
            log_error "未知参数: $1"
            print_usage
            exit 1
            ;;
    esac
done

# --------------------------------------------------
# 检测 yuleASR 环境
# --------------------------------------------------
detect_yuleasr() {
    # 优先使用环境变量
    if [ -n "${YULEASR_ROOT:-}" ] && [ -d "$YULEASR_ROOT" ]; then
        log_info "使用环境变量 YULEASR_ROOT=${YULEASR_ROOT}"
        return 0
    fi
    
    # 常见安装路径检测
    for candidate in /opt/yuleASR ~/yuleASR "${PROJECT_ROOT}/../yuleASR"; do
        candidate="$(eval echo "$candidate")"
        if [ -d "$candidate" ] && [ -f "$candidate/CMakeLists.txt" ]; then
            export YULEASR_ROOT="$candidate"
            log_info "自动检测到 yuleASR: ${YULEASR_ROOT}"
            return 0
        fi
    done
    
    log_warn "未找到 yuleASR，请设置 YULEASR_ROOT 环境变量或运行 ./scripts/setup.sh"
    log_warn "将尝试仅编译 SDK 密码学库（无 BSW 依赖）"
    EXTRA_CMAKE_ARGS+=("-DWITHOUT_YULEASR=ON")
}

detect_yuleasr

# --------------------------------------------------
# 清理重建
# --------------------------------------------------
if [ "$CLEAN_BUILD" = true ]; then
    log_info "清理构建目录: ${BUILD_DIR}"
    rm -rf "$BUILD_DIR"
fi

# --------------------------------------------------
# CMake 配置与构建
# --------------------------------------------------
log_info "配置 CMake (${BUILD_TYPE})..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$EMBEDDED_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DYULEASR_ROOT="${YULEASR_ROOT:-}" \
    "${EXTRA_CMAKE_ARGS[@]}"

log_info "构建中..."
make -j"$(nproc)"

echo ""
echo "======================================"
echo " 构建完成!"
echo "======================================"
echo ""
echo "构建类型: ${BUILD_TYPE}"
echo "输出目录: ${BUILD_DIR}"
echo ""
echo "产物:"
find "${BUILD_DIR}" -name "*.a" -o -name "*.elf" -o -name "*.hex" 2>/dev/null | \
    while read -r f; do
        echo "  $(realpath --relative-to="${BUILD_DIR}" "$f")"
    done
echo ""
