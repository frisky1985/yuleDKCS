#!/bin/bash

###############################################################################
# yuleDKCS MQTT Broker 部署脚本
#
# 功能:
#   - 生成 SSL/TLS 证书
#   - 启动 EMQ X MQTT Broker
#   - 配置认证和 ACL
#   - 健康检查
#
# 使用方法:
#   ./deploy-mqtt.sh [start|stop|restart|status|logs|cert]
###############################################################################

set -e

# 配置参数
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MQTT_DIR="${SCRIPT_DIR}/mqtt"
CERTS_DIR="${MQTT_DIR}/certs"
CONFIG_DIR="${MQTT_DIR}/config"
COMPOSE_FILE="${SCRIPT_DIR}/docker-compose.mqtt.yml"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 创建目录结构
create_directories() {
    log_info "创建目录结构..."
    mkdir -p "${CERTS_DIR}"
    mkdir -p "${CONFIG_DIR}"
    log_success "目录创建完成"
}

# 生成自签名证书
generate_certs() {
    log_info "生成 SSL/TLS 证书..."
    
    cd "${CERTS_DIR}"
    
    # 检查是否已存在证书
    if [ -f "server.crt" ] && [ -f "server.key" ]; then
        log_warn "证书已存在，跳过生成"
        return 0
    fi
    
    # 生成 CA 私钥
    openssl genrsa -out ca.key 2048 2>/dev/null
    
    # 生成 CA 证书
    openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
        -subj "/C=CN/ST=Shanghai/L=Shanghai/O=YuleTech/OU=DKCS/CN=YuleDKCS-CA" 2>/dev/null
    
    # 生成服务器私钥
    openssl genrsa -out server.key 2048 2>/dev/null
    
    # 生成服务器 CSR
    openssl req -new -key server.key -out server.csr \
        -subj "/C=CN/ST=Shanghai/L=Shanghai/O=YuleTech/OU=DKCS/CN=mqtt.yuledkcs.local" 2>/dev/null
    
    # 使用 CA 签署服务器证书
    openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
        -CAcreateserial -out server.crt -days 365 2>/dev/null
    
    # 生成客户端证书 (可选)
    openssl genrsa -out client.key 2048 2>/dev/null
    openssl req -new -key client.key -out client.csr \
        -subj "/C=CN/ST=Shanghai/L=Shanghai/O=YuleTech/OU=DKCS/CN=client" 2>/dev/null
    openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key \
        -CAcreateserial -out client.crt -days 365 2>/dev/null
    
    # 设置权限
    chmod 600 *.key
    chmod 644 *.crt
    
    log_success "证书生成完成"
    log_info "  CA 证书: ${CERTS_DIR}/ca.crt"
    log_info "  服务器证书: ${CERTS_DIR}/server.crt"
    log_info "  服务器私钥: ${CERTS_DIR}/server.key"
}

# 生成 ACL 配置文件
generate_acl() {
    log_info "生成 ACL 配置文件..."
    
    cat > "${CONFIG_DIR}/acl.conf" << 'EOF'
%% yuleDKCS MQTT ACL 配置
%% 格式: {allow|deny}, {user|ipaddr|all}, {pubsub|publish|subscribe}, [{"#"|"$SYS/#"|"vehicle/+/status"}]

%% 允许系统用户访问所有主题
{allow, {user, "admin"}, pubsub, ["#"]}.

%% 允许车主访问自己的车辆
{allow, all, publish, ["vehicle/${clientid}/command"]}.
{allow, all, subscribe, ["vehicle/${clientid}/status"]}.

%% 允许车主订阅自己所有车辆的状态
{allow, all, subscribe, ["vehicle/+/status"]}.

%% 允许车辆发布状态更新
{allow, all, publish, ["vehicle/+/status"]}.

%% 允许命令结果回调
{allow, all, subscribe, ["vehicle/+/command/result"]}.
{allow, all, publish, ["vehicle/+/command/result"]}.

%% 允许用户发送心跳
{allow, all, publish, ["heartbeat"]}.

%% 拒绝其他所有访问
{deny, all}.
EOF

    log_success "ACL 配置文件生成完成"
}

# 检查环境
check_environment() {
    log_info "检查部署环境..."
    
    # 检查 Docker
    if ! command -v docker &> /dev/null; then
        log_error "Docker 未安装"
        exit 1
    fi
    
    # 检查 Docker Compose
    if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
        log_error "Docker Compose 未安装"
        exit 1
    fi
    
    # 检查网络
    if ! docker network ls | grep -q "yuledkcs_default"; then
        log_warn "yuledkcs_default 网络不存在，创建中..."
        docker network create yuledkcs_default || true
    fi
    
    log_success "环境检查通过"
}

# 启动 MQTT Broker
start_mqtt() {
    log_info "启动 MQTT Broker..."
    
    # 确保目录和证书存在
    create_directories
    generate_certs
    generate_acl
    
    # 导入环境变量
    if [ -f "${SCRIPT_DIR}/.env" ]; then
        source "${SCRIPT_DIR}/.env"
    fi
    
    export EMQX_ADMIN_PASSWORD=${EMQX_ADMIN_PASSWORD:-YuleDKCS2024}
    export JWT_SECRET=${JWT_SECRET:-yuleDKCS-secret-key}
    export WEBHOOK_TOKEN=${WEBHOOK_TOKEN:-webhook-secret-token}
    
    # 启动服务
    cd "${SCRIPT_DIR}"
    
    if command -v docker-compose &> /dev/null; then
        docker-compose -f "${COMPOSE_FILE}" up -d
    else
        docker compose -f "${COMPOSE_FILE}" up -d
    fi
    
    log_success "MQTT Broker 启动成功"
    
    # 等待健康检查
    log_info "等待健康检查..."
    sleep 5
    
    # 检查状态
    if docker ps | grep -q "yuledkcs-mqtt"; then
        log_success "MQTT Broker 运行正常"
        show_status
    else
        log_error "MQTT Broker 启动失败"
        show_logs
        exit 1
    fi
}

# 停止 MQTT Broker
stop_mqtt() {
    log_info "停止 MQTT Broker..."
    
    cd "${SCRIPT_DIR}"
    
    if command -v docker-compose &> /dev/null; then
        docker-compose -f "${COMPOSE_FILE}" down
    else
        docker compose -f "${COMPOSE_FILE}" down
    fi
    
    log_success "MQTT Broker 已停止"
}

# 重启 MQTT Broker
restart_mqtt() {
    log_info "重启 MQTT Broker..."
    stop_mqtt
    sleep 2
    start_mqtt
}

# 显示状态
show_status() {
    log_info "MQTT Broker 状态:"
    echo ""
    docker ps --filter "name=yuledkcs-mqtt" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
    echo ""
    log_info "访问地址:"
    echo "  - MQTT:      mqtt://localhost:1883"
    echo "  - MQTT/TLS:  mqtts://localhost:8883"
    echo "  - WebSocket: ws://localhost:8083"
    echo "  - WS/TLS:    wss://localhost:8084"
    echo "  - Dashboard: http://localhost:18083"
    echo ""
    log_info "默认账号:"
    echo "  用户名: admin"
    echo "  密码: ${EMQX_ADMIN_PASSWORD:-YuleDKCS2024}"
}

# 显示日志
show_logs() {
    cd "${SCRIPT_DIR}"
    
    if command -v docker-compose &> /dev/null; then
        docker-compose -f "${COMPOSE_FILE}" logs -f
    else
        docker compose -f "${COMPOSE_FILE}" logs -f
    fi
}

# 显示帮助
show_help() {
    cat << EOF
YuleDKCS MQTT Broker 部署脚本

用法:
    $0 [COMMAND]

命令:
    start    - 启动 MQTT Broker (包含证书生成)
    stop     - 停止 MQTT Broker
    restart  - 重启 MQTT Broker
    status   - 显示运行状态
    logs     - 查看实时日志
    cert     - 只生成 SSL 证书
    help     - 显示此帮助信息

环境变量:
    EMQX_ADMIN_PASSWORD - Dashboard 管理员密码 (默认: YuleDKCS2024)
    JWT_SECRET          - JWT 签名秘钥 (默认: yuleDKCS-secret-key)
    WEBHOOK_TOKEN       - WebHook 验证 Token

示例:
    # 启动
    ./deploy-mqtt.sh start

    # 使用自定义密码
    EMQX_ADMIN_PASSWORD=MySecurePassword ./deploy-mqtt.sh start

    # 查看状态
    ./deploy-mqtt.sh status
EOF
}

# 主函数
main() {
    case "${1:-start}" in
        start)
            check_environment
            start_mqtt
            ;;
        stop)
            stop_mqtt
            ;;
        restart)
            restart_mqtt
            ;;
        status)
            show_status
            ;;
        logs)
            show_logs
            ;;
        cert)
            create_directories
            generate_certs
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            log_error "未知命令: $1"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
