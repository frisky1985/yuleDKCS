#!/usr/bin/env bash
#
# deploy-docs.sh — yuleDKCS 文档站部署脚本
#
# 将 MkDocs 构建的文档站部署到 GitHub Pages (gh-pages 分支)
#
# 用法:
#   ./scripts/deploy-docs.sh              # 构建并部署
#   ./scripts/deploy-docs.sh --build-only # 仅构建，不推送
#   ./scripts/deploy-docs.sh --serve      # 本地预览
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SITE_DIR="$PROJECT_ROOT/site"

cd "$PROJECT_ROOT"

echo "=== yuleDKCS 文档站部署脚本 ==="
echo "项目根目录: $PROJECT_ROOT"

# --- 检查依赖 ---
check_deps() {
  if ! command -v mkdocs &>/dev/null; then
    echo "❌ 未找到 mkdocs，请安装：pip install mkdocs mkdocs-material"
    exit 1
  fi
}

# --- 构建文档站 ---
build() {
  echo ""
  echo "📦 正在构建文档站..."
  mkdocs build --clean --site-dir "$SITE_DIR"
  echo "✅ 构建完成: $SITE_DIR"
}

# --- 部署到 gh-pages ---
deploy() {
  echo ""
  echo "🚀 正在部署到 GitHub Pages..."
  mkdocs gh-deploy --force --clean
  echo "✅ 部署完成！"
  echo "   文档站地址: https://frisky1985.github.io/yuleDKCS/"
}

# --- 本地预览 ---
serve() {
  echo ""
  echo "🌐 启动本地预览服务器 (http://127.0.0.1:8000)"
  mkdocs serve
}

# --- 主逻辑 ---
check_deps

case "${1:-}" in
  --build-only|-b)
    build
    ;;
  --serve|-s)
    serve
    ;;
  --help|-h)
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  (无)          构建并部署到 GitHub Pages"
    echo "  --build-only  仅构建，不部署"
    echo "  --serve       本地预览"
    echo "  --help        显示此帮助"
    exit 0
    ;;
  *)
    build
    deploy
    ;;
esac

echo ""
echo "=== 完成 ==="
