#!/bin/bash
# iOS部署脚本
# 使用方式: ./deploy_ios.sh [debug|release] [device_name]

set -e

# 配置
FLAVOR=${1:-release}
DEVICE_NAME=${2:-"iPhone 13"}
PROJECT_DIR="$(cd "$(dirname "$0")/flutter" && pwd)"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== YuleDKCS iOS 部署脚本 ===${NC}"
echo "环境: $FLAVOR"
echo "目标设备: $DEVICE_NAME"
echo "项目路径: $PROJECT_DIR"
echo ""

# 检查环境
echo -e "${YELLOW}[步骤 1/6] 检查环境...${NC}"

# 检查Flutter
if ! command -v flutter &> /dev/null; then
    echo -e "${RED}错误: 未找到Flutter。请先安装Flutter SDK。${NC}"
    exit 1
fi

# 检查Xcode
if ! command -v xcodebuild &> /dev/null; then
    echo -e "${RED}错误: 未找到Xcode。请在Mac上运行此脚本。${NC}"
    exit 1
fi

# 检查CocoaPods
if ! command -v pod &> /dev/null; then
    echo -e "${RED}错误: 未找到CocoaPods。请运行: sudo gem install cocoapods${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 环境检查通过${NC}"

# 进入项目目录
cd "$PROJECT_DIR"

# 清理旧构建
echo -e "${YELLOW}[步骤 2/6] 清理构建...${NC}"
flutter clean
rm -rf ios/Pods ios/Podfile.lock
echo -e "${GREEN}✓ 清理完成${NC}"

# 安装依赖
echo -e "${YELLOW}[步骤 3/6] 安装依赖...${NC}"
flutter pub get

cd ios
pod install --repo-update
cd ..
echo -e "${GREEN}✓ 依赖安装完成${NC}"

# 构建应用
echo -e "${YELLOW}[步骤 4/6] 构建应用 ($FLAVOR)...${NC}"

if [ "$FLAVOR" = "debug" ]; then
    flutter build ios --debug --no-codesign
else
    flutter build ios --release
fi

echo -e "${GREEN}✓ 构建完成${NC}"

# 查找连接的设备
echo -e "${YELLOW}[步骤 5/6] 查找设备...${NC}"
flutter devices

# 获取设备ID
DEVICE_ID=$(flutter devices | grep "$DEVICE_NAME" | grep -oE '[-0-9a-fA-F]{36}' | head -1)

if [ -z "$DEVICE_ID" ]; then
    echo -e "${YELLOW}未找到'$DEVICE_NAME'，尝试列出所有可用设备...${NC}"
    flutter devices
    echo -e "${YELLOW}请手动指定设备ID:${NC}"
    read -p "设备ID: " DEVICE_ID
fi

echo -e "${GREEN}✓ 设备ID: $DEVICE_ID${NC}"

# 安装应用
echo -e "${YELLOW}[步骤 6/6] 安装应用到设备...${NC}"

if [ "$FLAVOR" = "debug" ]; then
    flutter run --debug -d "$DEVICE_ID"
else
    # 使用ios-deploy安装release版本
    if command -v ios-deploy &> /dev/null; then
        APP_PATH="build/ios/iphoneos/Runner.app"
        if [ -d "$APP_PATH" ]; then
            ios-deploy --bundle "$APP_PATH" --id "$DEVICE_ID"
        else
            echo -e "${YELLOW}未找到.app文件，尝试使用flutter run...${NC}"
            flutter run --release -d "$DEVICE_ID"
        fi
    else
        echo -e "${YELLOW}提示: 安装ios-deploy以直接部署release版本: npm install -g ios-deploy${NC}"
        flutter run --release -d "$DEVICE_ID"
    fi
fi

echo -e "${GREEN}=== 部署完成 ===${NC}"
echo ""
echo "后端地址请确保配置为: https://your-server-domain.com"
echo "查看日志请运行: flutter logs"
