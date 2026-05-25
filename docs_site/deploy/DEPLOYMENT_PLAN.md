# yuleDKCS 三端部署计划

> 创建时间: 2026-05-16  
> 适用版本: v2.0.0+  
> 部署目标: Backend服务器 + iPhone 13 + KW47

---

## 📋 目录

- [部署概览](#部署概览)
- [Phase 1: Backend服务器部署](#phase-1-backend服务器部署)
- [Phase 2: Mobile iOS部署](#phase-2-mobile-ios部署)
- [Phase 3: Embedded KW47烧录](#phase-3-embedded-kw47烧录)
- [联调测试](#联调测试)
- [回滚方案](#回滚方案)

---

## 🚀 部署概览

### 部署架构

```
┌──────────────────────────────────────────────────────────────┐
│                    云服务器 (Backend)                         │
│  ├── API Gateway (Nginx)                                     │
│  ├── yuleDKCS API (Go)                                       │
│  ├── PostgreSQL                                              │
│  ├── Redis                                                   │
│  └── MQTT Broker (EMQX)                                      │
└──────────────────────────────────────────────────────────────┘
                              │
           ┌──────────────────────────────────────────────────────────────┐
│                              ▼                              │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              iPhone 13 (Mobile App)                        │  │
│  │  ├── Flutter App                                        │  │
│  │  ├── CCC/ICCOA/ICCE SDK                                 │  │
│  │  ├── BLE Connection                                     │  │
│  │  ├── UWB Ranging                                        │  │
│  │  └── iOS Native Bridge                                  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                            BLE/UWB                             │
│                              ▼                                │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │           KW47 (Embedded MCU)                             │  │
│  │  ├── BLE Stack (NXP BLE)                                │  │
│  │  ├── UWB Stack (DW3000)                                 │  │
│  │  ├── DKCS Core (CCC/ICCOA/ICCE)                         │  │
│  │  ├── SE050 Crypto                                       │  │
│  │  └── CAN/LIN Interface                                  │  │
│  └───────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

### 部署时间线

| 阶段 | 任务 | 预计时间 | 依赖 |
|------|------|---------|------|
| Phase 1 | Backend服务器部署 | 2-4小时 | 服务器访问权限 |
| Phase 2 | Mobile iOS部署 | 2-3小时 | Mac + Xcode + 开发者账号 |
| Phase 3 | Embedded KW47烧录 | 1-2小时 | J-Link + MCUXpresso |
| Phase 4 | 联调测试 | 2-4小时 | 三端就绪 |

### 风险评估

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|---------|
| 服务器环境不兼容 | 中 | 高 | 提供Docker部署方案 |
| iOS签名失败 | 中 | 中 | 检查Provisioning Profile |
| KW47烧录失败 | 低 | 高 | 准备Bootloader恢复 |
| 三端通信失败 | 中 | 高 | 逐层排查网络连接 |

---

## Phase 1: Backend服务器部署

### 1.1 环境要求

**服务器配置**
```yaml
CPU: 4核+
内存: 8GB+
存储: 100GB SSD
OS: Ubuntu 22.04 LTS / CentOS 8
网络: 公网IP + 9632端口开放
```

**软件依赖**
```yaml
Docker: 24.0+
Docker Compose: 2.20+
Nginx: 1.24+
PostgreSQL: 15+
Redis: 7+
```

### 1.2 部署步骤

#### 步骤1: 服务器准备
```bash
# 登录服务器
ssh user@your-server-ip

# 更新系统
sudo apt update && sudo apt upgrade -y

# 安装Docker
sudo apt install -y docker.io docker-compose-plugin

# 启动Docker
sudo systemctl enable docker
sudo systemctl start docker

# 创建部署目录
mkdir -p /opt/yuledkcs
cd /opt/yuledkcs
```

#### 步骤2: 配置文件准备
```bash
# 从开发机复制配置
scp -r deploy/docker/* user@server:/opt/yuledkcs/

# 创建环境变量文件
cat > .env << 'EOF'
# 数据库配置
DB_HOST=postgres
DB_PORT=5432
DB_NAME=yuledkcs
DB_USER=yuledkcs
DB_PASSWORD=your_secure_password_here

# JWT配置
JWT_SECRET=your_jwt_secret_key_here
JWT_EXPIRE_HOURS=24

# MQTT配置
MQTT_BROKER=emqx
MQTT_PORT=1883
MQTT_WS_PORT=8083
MQTT_SSL_PORT=8883

# 服务配置
API_PORT=8080
LOG_LEVEL=info
ENV=production

# 安全配置
ENABLE_HTTPS=true
SSL_CERT_PATH=/etc/nginx/ssl/cert.pem
SSL_KEY_PATH=/etc/nginx/ssl/key.pem
EOF
```

#### 步骤3: 启动服务
```bash
# 拉取最新镜像
docker-compose pull

# 启动服务
docker-compose up -d

# 检查状态
docker-compose ps
docker-compose logs -f api
```

### 1.3 验证部署

```bash
# 健康检查
curl http://localhost:8080/health

# 版本信息
curl http://localhost:8080/api/v1/version

# 数据库连接
docker-compose exec postgres psql -U yuledkcs -c "\dt"
```

---

## Phase 2: Mobile iOS部署

### 2.1 环境要求

**开发环境**
```yaml
系统: macOS 14.0+
Xcode: 15.0+
Flutter: 3.19+
Dart: 3.3+
CocoaPods: 1.15+
```

**账号要求**
```yaml
Apple Developer Account: 必需
iOS Distribution Certificate: 必需
Provisioning Profile: 必需
App ID: com.yuledkcs.app
```

### 2.2 项目配置

#### 步骤1: 获取源代码
```bash
# 克隆项目
git clone https://github.com/frisky1985/yuleDKCS.git
cd yuleDKCS/mobile/flutter

# 安装依赖
flutter pub get

# 进入iOS目录
cd ios
pod install
cd ..
```

#### 步骤2: iOS项目配置
```bash
# 生成Xcode工程
flutter build ios --release

# 打开Xcode
open ios/Runner.xcworkspace
```

**Xcode中的配置:**
1. 选择Runner项目
2. 配置Signing & Capabilities
3. 选择开发老账号
4. 更新Bundle Identifier
5. 配置App Groups（如需要）

#### 步骤3: 后端地址配置

编辑 `lib/config/api_config.dart`:
```dart
class ApiConfig {
  static const String baseUrl = 'https://your-server-domain.com/api/v1';
  static const String mqttBroker = 'your-server-domain.com';
  static const int mqttPort = 8883; // SSL端口
  
  // 设备特定配置
  static const bool enableDebugLogging = false;
  static const int connectionTimeout = 30;
}
```

### 2.3 构建与安装

#### 步骤1: Archive构建
```bash
# 清理构建
flutter clean
flutter pub get

# 构建Release版本
flutter build ios --release

# 或者在Xcode中 Archive
# Product -> Archive
```

#### 步骤2: 安装到iPhone 13

**方式一: 通过TestFlight (推荐)**
```
1. 在App Store Connect创建新应用
2. 上传Archive到App Store Connect
3. 添加内部测试员
4. 通过TestFlight安装
```

**方式二: 直接安装 (开发测试)**
```bash
# 使用flutter直接安装
flutter run --release -d "iPhone 13"

# 或使用ios-deploy
ios-deploy --bundle build/ios/iphoneos/Runner.app --id <device-id>
```

### 2.4 验证安装

```bash
# 列出连接的设备
flutter devices

# 查看日志
flutter logs

# 确认网络连接
curl -v https://your-server-domain.com/api/v1/health
```

---

## Phase 3: Embedded KW47烧录

### 3.1 环境要求

**开发环境**
```yaml
系统: Windows 10/11 或 Ubuntu 22.04
IDE: MCUXpresso IDE v11.8+ 或 VS Code + CMake
调试器: J-Link Plus / LPC-Link2
编译器: GCC ARM Embedded 12.2+
```

**硬件连接**
```yaml
KW47开发板: FRDM-KW47
调试接口: SWD (10-pin)
串口: USB-to-UART (115200 baud)
电源: USB 5V 或 外部电源 3.3V
```

### 3.2 工具安装

#### MCUXpresso IDE安装
```bash
# 下载MCUXpresso
wget https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-integrated-development-environment-ide:MCUXpresso-IDE

# 安装
sudo dpkg -i mcuxpressoide.deb

# 安装J-Link驱动
sudo dpkg -i JLink_Linux_x86_64.deb
```

#### 交叉编译工具链
```bash
# 安装ARM GCC
sudo apt install gcc-arm-none-eabi

# 验证安装
arm-none-eabi-gcc --version
```

### 3.3 项目配置

#### 步骤1: 获取SDK
```bash
# 克隆项目
git clone https://github.com/frisky1985/yuleDKCS.git
cd yuleDKCS/embedded

# 创建构建目录
mkdir -p build/kw47
cd build/kw47
```

#### 步骤2: 创建KW47工具链配置
```cmake
# cmake/toolchain_kw47.cmake
cat > ../../cmake/toolchain_kw47.cmake << 'EOF'
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

set(CMAKE_C_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g")

set(CMAKE_EXE_LINKER_FLAGS "-T${CMAKE_SOURCE_DIR}/cmake/kw47.ld")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
EOF
```

#### 步骤3: KW47链接脚本
```cmake
# cmake/kw47.ld (简化版)
MEMORY
{
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 2048K
}

SECTIONS
{
    .text :
    {
        *(.vectors)
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : AT(ADDR(.text) + SIZEOF(.text))
    {
        *(.data*)
    } > RAM

    .bss :
    {
        *(.bss*)
    } > RAM
}
```

### 3.4 编译与烧录

#### 步骤1: CMake构建
```bash
# 在build/kw47目录下
export KW47_SDK=/path/to/mcuxpresso/sdk/KW47B42Z

cmake ../.. \
    -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchain_kw47.cmake \
    -DKW47_SDK=${KW47_SDK} \
    -DENABLE_CCC=ON \
    -DENABLE_ICCOA=ON \
    -DENABLE_ICCE=ON \
    -DBUILD_EXAMPLES=ON

make -j$(nproc)
```

#### 步骤2: 使用J-Link烧录
```bash
# 连接KW47
JLinkExe -device KW47B42ZxxxA -if SWD -speed 4000

# 在J-Link命令行中
connect
loadfile build/kw47/examples/yuledkcs_kw47.hex
r
g
qc
```

#### 步骤3: 使用MCUXpresso烧录
```
1. 导入项目: File -> Import -> Existing Projects into Workspace
2. 选择embedded目录
3. 构建项目: Project -> Build Project
4. 配置调试: Run -> Debug Configurations
5. 选择J-Link调试器
6. 点击Debug按钮烧录并调试
```

### 3.5 验证烧录

```bash
# 连接串口
screen /dev/ttyUSB0 115200

# 或使用minicom
minicom -D /dev/ttyUSB0 -b 115200

# 期望输出
# [YuleDKCS] Booting...
# [YuleDKCS] BLE Stack initialized
# [YuleDKCS] UWB Stack initialized
# [YuleDKCS] Waiting for connection...
```

---

## Phase 4: 联调测试

### 4.1 测试检查清单

**网络连接测试**
```bash
# 1. 验证后端可访问
curl https://your-server-domain.com/api/v1/health

# 2. 验证MQTT连接
mosquitto_pub -h your-server-domain.com -p 8883 --cafile ca.crt -t test -m "hello"

# 3. 验证iPhone网络
# 在iPhone上打开Safari访问 https://your-server-domain.com
```

**蓝牙配对测试**
```
1. 打开iPhone上的YuleDKCS应用
2. 点击"添加钥匙"
3. 选择"配对新车辆"
4. 确认KW47开发板已上电并广播
5. 应用应该发现KW47设备
6. 开始配对流程
```

**功能测试**
```
☐ 配对流程
☐ 解锁/上锁
☐ 远程启动
☐ UWB测距
☐ 钥匙分享
☐ 钥匙撤销
```

### 4.2 常见问题排除

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| iPhone无法发现KW47 | BLE未广播 | 检查KW47电源和串口输出 |
| 配对超时 | 网络延迟 | 检查后端日志，确认MQTT连接 |
| 证书验证失败 | 时间不同步 | 同步所有设备时间 |
| UWB测距失败 | 射频干扰 | 远离WiFi路由器等射频设备 |

---

## 回滚方案

### Backend回滚
```bash
# 停止当前版本
docker-compose down

# 回滚到上一版本
docker-compose pull yuledkcs:previous-version
docker-compose up -d

# 验证
curl http://localhost:8080/health
```

### iOS回滚
```bash
# 通过TestFlight回滚
# 设置 -> 应用 -> YuleDKCS -> 之前版本

# 或重新安装旧版本
flutter build ios --release -d "iPhone 13" --flavor production
```

### KW47回滚
```bash
# 烧录备份固件
JLinkExe -device KW47B42ZxxxA -if SWD -speed 4000
loadfile backup.hex
r
g
qc
```

---

## 附录

### A. 快速命令参考

```bash
# Backend
make docker-up          # 启动Docker环境
make migrate-up         # 数据库迁移
make test               # 运行测试

# Mobile
flutter run --release   # 运行Release版本
flutter build ios       # 构建iOS
flutter install         # 安装到设备

# Embedded
make build              # 构建image
make flash              # 烧录固件
make debug              # 启动调试
```

### B. 常用配置文件路径

```
backend/
  ├── Dockerfile
  ├── docker-compose.yml
  ├── .env.production
  └── config/
      └── production.yaml

mobile/flutter/
  ├── ios/Runner.xcworkspace
  ├── lib/config/api_config.dart
  └── pubspec.yaml

embedded/
  ├── CMakeLists.txt
  ├── cmake/toolchain_kw47.cmake
  └── cmake/kw47.ld
```

---

*文档版本: 1.0.0*  
*最后更新: 2026-05-16*