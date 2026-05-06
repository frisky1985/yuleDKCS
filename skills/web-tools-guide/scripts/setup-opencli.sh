#!/usr/bin/env bash
# ============================================================================
# opencli 安装脚本（Web Tools Guide Skill）
# 适用于：Ubuntu 22.04+ / Debian 12+（需要已安装 OpenClaw + Node.js 18+）
# 用途：安装 opencli CLI，下载 Browser Bridge 插件，重启浏览器加载插件
# 使用：bash scripts/setup-opencli.sh
# 幂等：可重复运行，每步操作会检查当前状态，避免副作用
# ============================================================================

set -euo pipefail

# ============================================================
# 可配置变量
# ============================================================

CDP_PORT=9222                                            # Chrome DevTools Protocol 端口
OPENCLI_EXTENSION_DIR="/root/.openclaw/opencli-extension" # Browser Bridge 插件存放目录
GITHUB_RELEASE_BASE="https://github.com/jackwener/opencli/releases" # GitHub Releases 地址

# ============================================================
# 辅助函数
# ============================================================

info()  { echo "[INFO] $*"; }
ok()    { echo "[ OK ] $*"; }
warn()  { echo "[WARN] $*"; }
fail()  { echo "[FAIL] $*"; exit 1; }

has_cmd() { command -v "$1" >/dev/null 2>&1; }

# 加载 Node.js 环境（NVM + pnpm）
# 非交互式 shell 不会自动加载 .bashrc，需要手动 source
load_node_env() {
    export NVM_DIR="${HOME}/.nvm"
    if [ -s "${NVM_DIR}/nvm.sh" ]; then
        . "${NVM_DIR}/nvm.sh"
    fi
    export PNPM_HOME="${HOME}/.local/share/pnpm"
    export PATH="${PNPM_HOME}:${PATH}"
}

# 检测 CDP 端口是否正常响应
is_cdp_alive() {
    curl -s --max-time 2 "http://localhost:${CDP_PORT}/json/version" &>/dev/null
}

# ============================================================
# 前置检查
# ============================================================

preflight() {
    info "前置检查..."

    [[ "$(uname -s)" == "Linux" ]] || fail "仅支持 Linux（当前: $(uname -s)）"
    has_cmd node                   || fail "未找到 node — 请先安装 Node.js 18+"
    has_cmd npm                    || fail "未找到 npm — 请先安装 Node.js 18+"

    ok "前置检查通过"
}

# ============================================================
# 第一步: 安装 opencli CLI（全局）
# ============================================================

install_opencli_cli() {
    info "检查 opencli CLI..."

    if has_cmd opencli; then
        ok "opencli CLI 已安装: $(which opencli)"
        return 0
    fi

    info "安装 opencli CLI..."
    npm install -g @jackwener/opencli

    if ! has_cmd opencli; then
        fail "opencli 安装失败"
    fi

    ok "opencli CLI 安装完成"
}

# ============================================================
# 第二步: 下载 Browser Bridge 插件（从 GitHub Releases）
# ============================================================

download_browser_bridge() {
    info "检查 Browser Bridge 插件..."

    local ext_dir="${OPENCLI_EXTENSION_DIR}"

    # 已存在 manifest.json 说明之前下载过，跳过
    if [ -f "${ext_dir}/manifest.json" ]; then
        ok "Browser Bridge 插件已存在: ${ext_dir}"
        return 0
    fi

    has_cmd curl || fail "未找到 curl — 无法下载 Browser Bridge 插件"
    has_cmd unzip || fail "未找到 unzip — 无法解压 Browser Bridge 插件"

    # 获取 opencli 版本号，用于拼接 release 下载地址
    local version
    version=$(opencli --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1) || true
    if [ -z "$version" ]; then
        warn "无法获取 opencli 版本号，使用 latest release"
    fi

    # 优先按版本下载，失败则回退到 latest
    local download_url
    local tmp_zip
    tmp_zip=$(mktemp /tmp/opencli-extension-XXXXXX.zip)

    local downloaded=false
    if [ -n "$version" ]; then
        download_url="${GITHUB_RELEASE_BASE}/download/v${version}/opencli-extension.zip"
        info "尝试下载 Browser Bridge 插件 v${version}..."
        if curl -fsSL --max-time 60 -o "$tmp_zip" "$download_url" 2>/dev/null; then
            downloaded=true
        else
            warn "v${version} 下载失败，回退到 latest release"
        fi
    fi

    if [ "$downloaded" = false ]; then
        # 通过 GitHub API 获取 latest release 的 opencli-extension.zip 下载地址
        download_url="${GITHUB_RELEASE_BASE}/latest/download/opencli-extension.zip"
        info "下载 Browser Bridge 插件（latest release）..."
        if ! curl -fsSL --max-time 60 -o "$tmp_zip" "$download_url" 2>/dev/null; then
            rm -f "$tmp_zip"
            fail "Browser Bridge 插件下载失败，请检查网络或手动下载: ${GITHUB_RELEASE_BASE}"
        fi
    fi

    # 解压到目标目录
    info "解压 Browser Bridge 插件到 ${ext_dir}..."
    mkdir -p "$ext_dir"
    unzip -qo "$tmp_zip" -d "$ext_dir"
    rm -f "$tmp_zip"

    # 验证解压结果（manifest.json 是 Chrome 扩展的必备文件）
    if [ ! -f "${ext_dir}/manifest.json" ]; then
        # 可能 zip 内有一层子目录，尝试提升
        local sub_dir
        sub_dir=$(find "$ext_dir" -maxdepth 1 -mindepth 1 -type d | head -1)
        if [ -n "$sub_dir" ] && [ -f "${sub_dir}/manifest.json" ]; then
            # 将子目录内容移到 ext_dir
            mv "${sub_dir}"/* "$ext_dir/" 2>/dev/null || true
            mv "${sub_dir}"/.[!.]* "$ext_dir/" 2>/dev/null || true
            rmdir "$sub_dir" 2>/dev/null || true
        fi
    fi

    if [ ! -f "${ext_dir}/manifest.json" ]; then
        fail "Browser Bridge 插件解压异常：未找到 manifest.json"
    fi

    ok "Browser Bridge 插件下载完成: ${ext_dir}"
}

# 获取 opencli extension 路径
get_opencli_extension_dir() {
    local ext_dir="${OPENCLI_EXTENSION_DIR}"
    if [ -d "$ext_dir" ] && [ -f "${ext_dir}/manifest.json" ]; then
        echo "$ext_dir"
    fi
}

# ============================================================
# 第三步: 重启浏览器加载 opencli 插件
# 从 /proc 提取当前 Chrome 命令行，杀掉进程，追加 extension 参数后重新启动
# ============================================================

# 从 /proc/<pid>/cmdline 提取 Chrome 进程的完整命令行
# 返回以空格分隔的参数列表
get_chrome_cmdline_by_pid() {
    local pid="$1"
    if [ -f "/proc/${pid}/cmdline" ]; then
        # cmdline 以 NUL 分隔，转为空格
        tr '\0' ' ' < "/proc/${pid}/cmdline" | sed 's/ *$//'
    fi
}

# 查找正在监听 CDP_PORT 的 Chrome 主进程 PID
# 返回 PID 或空字符串
find_chrome_pid_by_cdp_port() {
    local pid
    # 方法1: 通过 ss/netstat 查找监听端口的进程
    if has_cmd ss; then
        pid=$(ss -tlnp "sport = :${CDP_PORT}" 2>/dev/null \
            | grep -oP 'pid=\K[0-9]+' | head -1) || true
    fi
    # 方法2: 通过 lsof
    if [ -z "${pid:-}" ] && has_cmd lsof; then
        pid=$(lsof -ti :${CDP_PORT} -sTCP:LISTEN 2>/dev/null | head -1) || true
    fi
    # 方法3: 通过 /proc 遍历查找含 remote-debugging-port 的 chrome 进程
    if [ -z "${pid:-}" ]; then
        pid=$(ps aux | grep '[c]hrome.*--remote-debugging-port='"${CDP_PORT}" \
            | awk '{print $2}' | head -1) || true
    fi
    echo "${pid:-}"
}

# 从 /proc 提取当前 Chrome 命令行，杀掉进程，追加 extension 参数后重新启动
reload_browser_process() {
    local ext_dir="$1"

    local chrome_pid
    chrome_pid=$(find_chrome_pid_by_cdp_port)
    if [ -z "$chrome_pid" ]; then
        warn "CDP 端口 ${CDP_PORT} 无响应，且未找到 Chrome 进程"
        warn "跳过浏览器插件加载（opencli CLI 仍可使用，但 Browser Bridge 不可用）"
        return 0
    fi

    info "找到 Chrome 进程 PID: ${chrome_pid}"

    # 检查该进程是否已加载了 extension
    local current_cmdline
    current_cmdline=$(get_chrome_cmdline_by_pid "$chrome_pid")
    if echo "$current_cmdline" | grep -q "load-extension=.*${ext_dir}"; then
        ok "浏览器已加载 opencli 插件，无需重启"
        return 0
    fi

    # 提取原始命令行，准备追加 extension 参数
    if [ -z "$current_cmdline" ]; then
        warn "无法读取 Chrome 进程命令行（/proc/${chrome_pid}/cmdline）"
        warn "跳过浏览器插件加载"
        return 0
    fi

    info "提取 Chrome 原始启动命令..."

    # 移除命令行中可能已有的旧 extension 参数
    local clean_cmdline
    clean_cmdline=$(echo "$current_cmdline" \
        | sed 's/--disable-extensions-except=[^ ]* *//g' \
        | sed 's/--load-extension=[^ ]* *//g')

    # 追加 extension 参数
    local new_cmdline="${clean_cmdline} --disable-extensions-except=${ext_dir} --load-extension=${ext_dir}"

    info "终止当前 Chrome 进程 (PID: ${chrome_pid})..."
    kill "$chrome_pid" 2>/dev/null || true
    # 等待进程退出（最多 5 秒）
    local wait_count=10
    while [ $wait_count -gt 0 ] && kill -0 "$chrome_pid" 2>/dev/null; do
        sleep 0.5
        wait_count=$((wait_count - 1))
    done
    # 如果还没退出，强制杀掉
    if kill -0 "$chrome_pid" 2>/dev/null; then
        warn "Chrome 进程未响应 SIGTERM，发送 SIGKILL..."
        kill -9 "$chrome_pid" 2>/dev/null || true
        sleep 1
    fi

    info "使用新参数重新启动 Chrome..."
    # 后台启动，重定向输出到日志文件
    local log_file="/tmp/opencli-chrome-restart.log"
    nohup bash -c "exec ${new_cmdline}" > "$log_file" 2>&1 &
    local new_pid=$!
    info "Chrome 已启动 (PID: ${new_pid})，日志: ${log_file}"

    wait_cdp_ready "cat ${log_file}"
}

# 等待 CDP 端口就绪（最多 15 秒）
wait_cdp_ready() {
    local debug_hint="${1:-}"
    local retries=30
    while [ $retries -gt 0 ]; do
        if is_cdp_alive; then
            ok "浏览器重启成功，CDP 端口 ${CDP_PORT} 就绪，opencli 插件已加载"
            return 0
        fi
        sleep 0.5
        retries=$((retries - 1))
    done
    if [ -n "$debug_hint" ]; then
        fail "浏览器重启超时，查看日志: ${debug_hint}"
    else
        fail "浏览器重启超时"
    fi
}

# 主入口: 查找运行中的 Chrome 进程，杀掉后带 extension 参数重启
reload_browser_with_extension() {
    info "检查浏览器是否已加载 opencli 插件..."

    local ext_dir
    ext_dir=$(get_opencli_extension_dir)
    if [ -z "$ext_dir" ]; then
        fail "未找到 opencli extension，请先运行安装和下载步骤"
    fi

    # 检查 CDP 端口是否存活
    if is_cdp_alive; then
        reload_browser_process "$ext_dir"
        return $?
    fi

    # CDP 端口无响应
    warn "CDP 端口 ${CDP_PORT} 无响应，未检测到运行中的浏览器"
    warn "请先启动浏览器（带 --remote-debugging-port=${CDP_PORT} 参数）"
    warn "跳过浏览器插件加载（opencli CLI 仍可使用，但 Browser Bridge 不可用）"
    return 0
}

# ============================================================
# 验证
# ============================================================

verify() {
    info "验证 opencli 状态..."

    if ! has_cmd opencli; then
        fail "opencli 未安装"
    fi

    local doctor_output
    doctor_output=$(opencli doctor 2>&1) || true

    if echo "$doctor_output" | grep -q "Extension.*OK"; then
        ok "opencli doctor: 全部正常"
    else
        warn "opencli doctor 存在异常:"
        echo "$doctor_output" | head -10 | sed 's/^/  /'
        echo ""
        warn "Browser Bridge 可能未就绪（opencli CLI 命令仍可使用）"
        warn "如果浏览器刚重启，请等待几秒后重试: opencli doctor"
    fi
}

# ============================================================
# 主流程
# ============================================================

main() {
    echo ""
    echo "=== opencli 安装（Web Tools Guide Skill） ==="
    echo ""

    load_node_env
    preflight

    echo ""
    install_opencli_cli           # 第一步: 安装 opencli CLI

    echo ""
    download_browser_bridge       # 第二步: 下载 Browser Bridge 插件（从 GitHub Releases）

    echo ""
    reload_browser_with_extension # 第三步: 重启浏览器加载插件

    echo ""
    verify                        # 验证

    echo ""
    ok "opencli 安装完成！可通过 'opencli --help' 查看可用命令"
    echo ""
}

main "$@"
