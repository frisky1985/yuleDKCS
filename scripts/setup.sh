#!/bin/bash
#
# yuleASR 平台环境一键配置脚本
#
# 功能:
#   1. 克隆 yuleASR 仓库到 YULEASR_ROOT
#   2. 初始化 submodules
#   3. 设置环境变量 YULEASR_ROOT
#   4. 可选: 构建 yuleASR BSW 库
#
# 用法:
#   ./scripts/setup.sh                    # 完整安装
#   ./scripts/setup.sh --prefix /opt/yuleASR   # 指定安装路径
#   ./scripts/setup.sh --skip-build       # 仅克隆，不构建
#   ./scripts/setup.sh --help             # 显示帮助
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 默认配置
YULEASR_REPO="git@github.com:frisky1985/yuleASR.git"
YULEASR_ROOT="${YULEASR_ROOT:-/opt/yuleASR}"
YULEASR_BRANCH="main"
SKIP_BUILD=false

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
  --prefix DIR       yuleASR 安装路径 (默认: /opt/yuleASR)
  --branch NAME      yuleASR 分支名 (默认: main)
  --skip-build       仅克隆，不执行 CMake 构建
  --help             显示此帮助信息

环境变量:
  YULEASR_ROOT       可替代 --prefix

示例:
  $0                        # 完整安装到 /opt/yuleASR
  $0 --prefix ~/yuleASR     # 安装到用户目录
  $0 --skip-build           # 仅拉取代码，不编译
EOF
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            YULEASR_ROOT="$2"
            shift 2
            ;;
        --branch)
            YULEASR_BRANCH="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
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

echo "======================================"
echo " yuleASR 平台环境配置"
echo "======================================"
echo " 目标路径: ${YULEASR_ROOT}"
echo " 仓库:     ${YULEASR_REPO}"
echo " 分支:     ${YULEASR_BRANCH}"
echo "======================================"
echo ""

# --------------------------------------------------
# 前置检查
# --------------------------------------------------
log_info "检查依赖工具..."

check_dep() {
    if ! command -v "$1" &>/dev/null; then
        log_error "未找到必需工具: $1"
        exit 1
    fi
    log_info "  ✓ $1 ($(command -v "$1"))"
}

check_dep git
check_dep cmake
check_dep make

# --------------------------------------------------
# 克隆 yuleASR
# --------------------------------------------------
if [ -d "$YULEASR_ROOT" ]; then
    log_warn "目录 ${YULEASR_ROOT} 已存在，尝试更新..."
    cd "$YULEASR_ROOT"
    
    if [ -d ".git" ]; then
        git pull origin "$YULEASR_BRANCH"
        log_info "yuleASR 已更新到最新版本"
    else
        log_warn "${YULEASR_ROOT} 不是 git 仓库，跳过更新"
    fi
else
    log_info "克隆 yuleASR 仓库..."
    mkdir -p "$(dirname "$YULEASR_ROOT")"
    git clone "$YULEASR_REPO" -b "$YULEASR_BRANCH" "$YULEASR_ROOT"
    log_info "yuleASR 克隆完成"
fi

# --------------------------------------------------
# 初始化 submodules
# --------------------------------------------------
log_info "初始化 yuleASR submodules..."
cd "$YULEASR_ROOT"
git submodule update --init --recursive
log_info "Submodules 初始化完成"

# --------------------------------------------------
# 设置环境变量
# --------------------------------------------------
log_info "配置环境变量 YULEASR_ROOT=${YULEASR_ROOT}"

# 写到 profile 片段，方便用户永久配置
PROFILE_FILE="${HOME}/.yuleasr_env"
cat > "$PROFILE_FILE" << EOF
# yuleASR 环境配置 (由 setup.sh 自动生成)
export YULEASR_ROOT="${YULEASR_ROOT}"
export YULEASR_BUILD="\${YULEASR_ROOT}/build"
EOF

log_info "环境变量文件已生成: ${PROFILE_FILE}"
log_info "运行以下命令加载: source ${PROFILE_FILE}"

# 也尝试写入 .bashrc (仅在不存在时)
if ! grep -q "yuleasr_env" "${HOME}/.bashrc" 2>/dev/null; then
    {
        echo ""
        echo "# yuleASR environment (added by setup.sh)"
        echo "if [ -f \"${PROFILE_FILE}\" ]; then"
        echo "    source \"${PROFILE_FILE}\""
        echo "fi"
    } >> "${HOME}/.bashrc"
    log_info "已追加 source 到 ~/.bashrc"
fi

# 当前会话环境变量
export YULEASR_ROOT
export YULEASR_BUILD="${YULEASR_ROOT}/build"

# --------------------------------------------------
# 构建 yuleASR BSW 库
# --------------------------------------------------
if [ "$SKIP_BUILD" = false ]; then
    log_info "构建 yuleASR BSW 库..."
    
    mkdir -p "$YULEASR_BUILD"
    cd "$YULEASR_BUILD"
    
    cmake "$YULEASR_ROOT" \
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Release} \
        -DCMAKE_INSTALL_PREFIX="${YULEASR_ROOT}/install"
    
    make -j"$(nproc)"
    make install
    
    log_info "yuleASR BSW 构建完成"
    log_info "库文件: ${YULEASR_ROOT}/install/lib"
    log_info "头文件: ${YULEASR_ROOT}/install/include"
else
    log_info "跳过 yuleASR 构建 (--skip-build)"
fi

# --------------------------------------------------
# 完成
# --------------------------------------------------
echo ""
echo "======================================"
echo " yuleASR 环境配置完成!"
echo "======================================"
echo ""
echo "环境变量已设置:"
echo "  YULEASR_ROOT=${YULEASR_ROOT}"
echo "  YULEASR_BUILD=${YULEASR_BUILD}"
echo ""
echo "在构建 yuleDKCS embedded 时，CMake 会自动检测 YULEASR_ROOT"
echo "来引用 yuleASR 的头文件和库。"
echo ""
echo "快速构建:"
echo "  source ${PROFILE_FILE}"
echo "  cd ${PROJECT_ROOT} && ./scripts/build.sh"
echo ""
