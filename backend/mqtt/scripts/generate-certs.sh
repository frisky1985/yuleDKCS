#!/bin/bash

# MQTT TLS 证书生成脚本
# 为 EMQ X MQTT Broker 生成自签名证书

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CERTS_DIR="${SCRIPT_DIR}/../certs"
CA_KEY_SIZE=4096
SERVER_KEY_SIZE=2048
DAYS_VALID=3650

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== MQTT TLS 证书生成脚本 ===${NC}"
echo ""

# 创建证书目录
mkdir -p "${CERTS_DIR}"
cd "${CERTS_DIR}"

# 检查是否已存在证书
if [ -f "ca.crt" ] && [ -f "server.crt" ] && [ -f "server.key" ]; then
    echo -e "${YELLOW}警告: 证书文件已存在${NC}"
    read -p "是否要重新生成? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${GREEN}保留现有证书，退出...${NC}"
        exit 0
    fi
fi

echo -e "${GREEN}步骤1: 生成 CA 私钥...${NC}"
openssl genrsa -out ca.key ${CA_KEY_SIZE}

echo -e "${GREEN}步骤2: 生成 CA 证书...${NC}"
openssl req -new -x509 -days ${DAYS_VALID} -key ca.key -out ca.crt \
    -subj "/C=CN/ST=Shanghai/L=Shanghai/O=yuleDKCS/OU=Security/CN=yuleDKCS MQTT CA/emailAddress=admin@yuledkcs.local"

echo -e "${GREEN}步骤3: 生成服务器私钥...${NC}"
openssl genrsa -out server.key ${SERVER_KEY_SIZE}

echo -e "${GREEN}步骤4: 生成服务器 CSR...${NC}"
openssl req -new -key server.key -out server.csr \
    -subj "/C=CN/ST=Shanghai/L=Shanghai/O=yuleDKCS/OU=IoT/CN=emqx.yuledkcs.local/emailAddress=mqtt@yuledkcs.local" \
    -addext "subjectAltName=DNS:emqx.yuledkcs.local,DNS:emqx,DNS:localhost,IP:127.0.0.1,IP:0.0.0.0"

echo -e "${GREEN}步骤5: 生成服务器证书...${NC}"
# 创建延伸使用配置文件
openssl x509 -req -days ${DAYS_VALID} -in server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out server.crt \
    -extensions v3_req -extfile <(cat <<EOF
[v3_req]
subjectAltName = @alt_names
[alt_names]
DNS.1 = emqx.yuledkcs.local
DNS.2 = emqx
DNS.3 = localhost
IP.1 = 127.0.0.1
IP.2 = 0.0.0.0
EOF
)

echo -e "${GREEN}步骤6: 生成客户端证书 (用于双向 TLS 认证)...${NC}"
openssl genrsa -out client.key ${SERVER_KEY_SIZE}
openssl req -new -key client.key -out client.csr \
    -subj "/C=CN/ST=Shanghai/L=Shanghai/O=yuleDKCS/OU=Client/CN=mqtt-client/emailAddress=client@yuledkcs.local"
openssl x509 -req -days ${DAYS_VALID} -in client.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out client.crt

echo -e "${GREEN}步骤7: 清理临时文件...${NC}"
rm -f server.csr client.csr ca.srl

echo -e "${GREEN}步骤8: 设置文件权限...${NC}"
chmod 600 *.key
chmod 644 *.crt

echo ""
echo -e "${GREEN}=== 证书生成完成 ===${NC}"
echo ""
echo "生成的文件:"
echo "  - ${CERTS_DIR}/ca.crt      (CA 证书)"
echo "  - ${CERTS_DIR}/ca.key      (CA 私钥)"
echo "  - ${CERTS_DIR}/server.crt  (服务器证书)"
echo "  - ${CERTS_DIR}/server.key  (服务器私钥)"
echo "  - ${CERTS_DIR}/client.crt  (客户端证书)"
echo "  - ${CERTS_DIR}/client.key  (客户端私钥)"
echo ""
echo -e "${YELLOW}证书有效期: ${DAYS_VALID} 天${NC}"
echo ""
echo "验证证书:"
openssl x509 -in server.crt -text -noout | grep -E "Subject:|Issuer:|Not Before|Not After"
