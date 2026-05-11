#!/bin/bash

# yuleDKCS MQTT Broker 部署脚本
# 一键部署 EMQ X MQTT Broker 并配置 TLS 加密和 JWT 认证

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_NAME="yuledkcs"
COMPOSE_FILE="${SCRIPT_DIR}/docker-compose.mqtt.yml"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# 显示帮助
show_help() {
    cat <<EOF
yuleDKCS MQTT Broker 部署脚本

用法: $0 [COMMAND] [OPTIONS]

命令:
    deploy      完整部署（默认）
    start       启动服务
    stop        停止服务
    restart     重启服务
    status      查看状态
    logs        查看日志
    certs       只生成证书
    clean       清理数据和容器
    health      健康检查

选项:
    -h, --help      显示帮助
    -f, --force     强制重新部署
    -v, --verbose   详细输出

示例:
    $0                      # 完整部署
    $0 deploy               # 完整部署
    $0 start                # 启动服务
    $0 stop                 # 停止服务
    $0 logs -f              # 实时查看日志
    $0 health               # 健康检查
EOF
}

# 检查 Docker 和 Docker Compose
check_docker() {
    log_step "检查 Docker 环境..."
    
    if ! command -v docker &> /dev/null; then
        log_error "Docker 未安装"
        exit 1
    fi
    
    if ! command -v docker-compose &> /dev/null; then
        log_error "Docker Compose 未安装"
        exit 1
    fi
    
    if ! docker info &> /dev/null; then
        log_error "Docker 守护进程未运行"
        exit 1
    fi
    
    log_info "Docker 环境正常"
}

# 检查网络
prepare_network() {
    log_step "检查 Docker 网络..."
    
    if ! docker network ls | grep -q "yuledkcs-network"; then
        log_info "创建 yuledkcs-network 网络..."
        docker network create yuledkcs-network
    else
        log_info "网络 yuledkcs-network 已存在"
    fi
}

# 生成证书
generate_certificates() {
    log_step "生成 TLS 证书..."
    
    CERTS_DIR="${SCRIPT_DIR}/mqtt/certs"
    
    if [ -f "${CERTS_DIR}/server.crt" ] && [ -f "${CERTS_DIR}/server.key" ] && [ -f "${CERTS_DIR}/ca.crt" ]; then
        if [ "$FORCE" != "true" ]; then
            log_warn "证书已存在，使用 --force 重新生成"
            return 0
        fi
    fi
    
    bash "${SCRIPT_DIR}/mqtt/scripts/generate-certs.sh"
    
    if [ $? -ne 0 ]; then
        log_error "证书生成失败"
        exit 1
    fi
    
    log_info "证书生成成功"
}

# 启动服务
start_services() {
    log_step "启动 MQTT 服务..."
    
    cd "${SCRIPT_DIR}"
    
    # 检查环境变量
    if [ -z "$YULEDKCS_JWT_SECRET" ]; then
        export YULEDKCS_JWT_SECRET="your-p...-key"
        log_warn "YULEDKCS_JWT_SECRET 未设置，使用默认值"
    fi
    
    docker-compose -f "${COMPOSE_FILE}" up -d
    
    if [ $? -ne 0 ]; then
        log_error "服务启动失败"
        exit 1
    fi
    
    log_info "服务启动成功"
}

# 停止服务
stop_services() {
    log_step "停止 MQTT 服务..."
    
    cd "${SCRIPT_DIR}"
    docker-compose -f "${COMPOSE_FILE}" down
    
    log_info "服务已停止"
}

# 查看状态
show_status() {
    log_step "服务状态"
    
    echo ""
    echo "容器状态:"
    docker-compose -f "${COMPOSE_FILE}" ps
    
    echo ""
    echo "端口监听:"
    echo "  - MQTT (1883):     tcp://localhost:1883"
    echo "  - MQTT TLS (8883): ssl://localhost:8883"
    echo "  - WebSocket (8083): ws://localhost:8083"
    echo "  - WS TLS (8084):   wss://localhost:8084"
    echo "  - Dashboard (18083): http://localhost:18083"
    
    echo ""
    echo "默认凭证:"
    echo "  - Dashboard: admin / public"
}

# 查看日志
show_logs() {
    docker-compose -f "${COMPOSE_FILE}" logs "$@"
}

# 清理数据
clean_data() {
    log_step "清理 MQTT 数据..."
    
    read -p "确定要删除所有 MQTT 数据吗? (这将删除所有配置和日志) [y/N]: " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "取消清理"
        return 0
    fi
    
    cd "${SCRIPT_DIR}"
    docker-compose -f "${COMPOSE_FILE}" down -v
    
    log_info "数据已清理"
}

# 健康检查
health_check() {
    log_step "执行健康检查..."
    
    local failed=0
    
    # 检查 EMQ X
    echo "检查 EMQ X..."
    if docker ps | grep -q "yuledkcs-emqx"; then
        log_info "EMQ X 容器运行中"
        
        # API 健康检查
        if curl -s http://localhost:18083/api/v5/status | grep -q "running"; then
            log_info "EMQ X API 响应正常"
        else
            log_warn "EMQ X API 未响应"
            failed=$((failed + 1))
        fi
    else
        log_error "EMQ X 容器未运行"
        failed=$((failed + 1))
    fi
    
    # 检查端口
    echo ""
    echo "检查端口监听..."
    local ports=(1883 8883 8083 8084 18083)
    for port in "${ports[@]}"; do
        if nc -z localhost $port 2>/dev/null; then
            log_info "端口 $port 监听中"
        else
            log_warn "端口 $port 未监听"
            failed=$((failed + 1))
        fi
    done
    
    # 检查证书
    echo ""
    echo "检查证书..."
    CERTS_DIR="${SCRIPT_DIR}/mqtt/certs"
    if [ -f "${CERTS_DIR}/server.crt" ]; then
        local expiry=$(openssl x509 -in "${CERTS_DIR}/server.crt" -noout -enddate | cut -d= -f2)
        log_info "服务器证书有效期至: $expiry"
    else
        log_warn "服务器证书不存在"
        failed=$((failed + 1))
    fi
    
    echo ""
    if [ $failed -eq 0 ]; then
        log_info "健康检查通过！"
    else
        log_error "健康检查失败: $failed 项"
        return 1
    fi
}

# 完整部署
deploy() {
    log_info "开始部署 MQTT Broker..."
    
    check_docker
    prepare_network
    generate_certificates
    start_services
    
    echo ""
    log_info "等待服务启动..."
    sleep 10
    
    health_check
    
    echo ""
    log_info "部署完成！"
    echo ""
    echo "访问信息:"
    echo "  Dashboard: http://localhost:18083"
    echo "  用户名: admin"
    echo "  密码: public"
    echo ""
    echo "MQTT 连接:"
    echo "  mqtt://localhost:1883 (无加密)"
    echo "  ssl://localhost:8883 (TLS)"
    echo "  ws://localhost:8083 (WebSocket)"
    echo "  wss://localhost:8084 (WebSocket TLS)"
}

# 主函数
main() {
    COMMAND=${1:-deploy}
    shift || true
    
    # 解析选项
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -f|--force)
                FORCE="true"
                shift
                ;;
            -v|--verbose)
                set -x
                shift
                ;;
            *)
                break
                ;;
        esac
    done
    
    case $COMMAND in
        deploy)
            deploy
            ;;
        start)
            start_services
            ;;
        stop)
            stop_services
            ;;
        restart)
            stop_services
            sleep 2
            start_services
            ;;
        status)
            show_status
            ;;
        logs)
            show_logs "$@"
            ;;
        certs)
            generate_certificates
            ;;
        clean)
            clean_data
            ;;
        health)
            health_check
            ;;
        *)
            log_error "未知命令: $COMMAND"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
