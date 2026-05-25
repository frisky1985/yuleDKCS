# yuleDKCS 部署清单

> 执行部署前请逐项确认

## ✅ 部署前准备

### 服务器资源
- [ ] 云服务器IP地址/域名
- [ ] SSH访问权限 (密钥或密码)
- [ ] 防火墙规则配置 (9632, 80, 443, 1883, 8883端口)
- [ ] SSL证书 (自签名或Let's Encrypt)

### iOS部署
- [ ] Mac电脑 (必须)
- [ ] Xcode 15.0+ 安装完成
- [ ] Flutter SDK 3.19+ 配置正确
- [ ] Apple Developer Account (年费付费账号)
- [ ] iOS Distribution Certificate 生成
- [ ] Provisioning Profile 配置
- [ ] iPhone 13 物理设备

### KW47烧录
- [ ] KW47开发板 (FRDM-KW47B42Z)
- [ ] J-Link调试器 或 LPC-Link2
- [ ] USB串口转换器
- [ ] MCUXpresso IDE 安装完成
- [ ] ARM GCC工具链安装
- [ ] SWD连接线

---

## 📋 部署步骤

### Phase 1: Backend部署 (2-4小时)

**步骤1: 服务器环境准备**
```bash
# 在服务器上执行
sudo apt update && sudo apt install -y docker.io docker-compose-plugin
sudo systemctl enable docker
sudo usermod -aG docker $USER
# 重新登录使docker组生效
```

**步骤2: 文件上传**
```bash
# 在开发机上执行
scp -r deploy/docker user@server:/opt/yuledkcs/
scp deploy/docker-compose.production.yml user@server:/opt/yuledkcs/
```

**步骤3: 环境配置**
```bash
# 在服务器上
ssh user@server
cd /opt/yuledkcs

# 创建环境变量文件
cat > .env << 'EOF'
DB_PASSWORD=your_secure_db_password_$(date +%s)
JWT_SECRET=$(openssl rand -base64 32)
GRAFANA_PASSWORD=admin
EOF
```

**步骤4: 启动服务**
```bash
docker compose -f docker-compose.production.yml up -d

# 验证部署
curl http://localhost:8080/health
curl http://localhost:8080/api/v1/version
```

---

### Phase 2: iOS部署 (2-3小时)

**步骤1: 环境准备**
```bash
# 在Mac上
flutter doctor
sudo xcode-select --switch /Applications/Xcode.app
sudo xcodebuild -license accept
```

**步骤2: 配置后端地址**
```bash
cd mobile/flutter

# 编辑 lib/config/api_config.dart
cat > lib/config/api_config.dart << 'EOF'
class ApiConfig {
  static const String baseUrl = 'https://your-server-domain.com/api/v1';
  static const String mqttBroker = 'your-server-domain.com';
  static const int mqttPort = 8883;
  static const bool enableDebugLogging = false;
  static const int connectionTimeout = 30;
}
EOF
```

**步骤3: 签名配置**
```bash
# 打开Xcode
open ios/Runner.xcworkspace

# 在Xcode中配置:
# 1. 选择Runner目标
# 2. Signing & Capabilities -> Team: 选择你的Apple ID
# 3. Bundle Identifier: 设置为唯一ID (如 com.yourcompany.yuledkcs)
```

**步骤4: 构建和安装**
```bash
# 运行部署脚本
chmod +x mobile/deploy_ios.sh
./mobile/deploy_ios.sh release "iPhone 13"
```

---

### Phase 3: KW47烧录 (1-2小时)

**步骤1: 工具安装**
```bash
# Ubuntu系统
sudo apt install gcc-arm-none-eabi

# 或下载NXP MCUXpresso SDK
wget https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-sdk-for-kw47b42z:SDK_2_x_KW47B42Z
```

**步骤2: 构建固件**
```bash
cd embedded
mkdir -p build/kw47 && cd build/kw47

cmake ../.. \
    -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchain_kw47.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_CCC=ON \
    -DENABLE_ICCOA=ON \
    -DENABLE_ICCE=ON \
    -DBUILD_EXAMPLES=ON

make -j$(nproc)
```

**步骤3: 烧录**
```bash
# 方式一: 使用J-Link
JLinkExe -device KW47B42ZxxxA -if SWD -speed 4000
# 在J-Link提示符下:
connect
loadfile build/kw47/examples/yuledkcs_kw47.hex
r
g
qc

# 方式二: 使用MCUXpresso
# 1. 导入项目
# 2. 选择构建配置
# 3. 点击Debug按钮
```

---

## 🔍 验证清单

### 服务器验证
- [ ] API健康检查: `curl http://localhost:8080/health`
- [ ] 数据库连接: `docker compose exec postgres psql -U yuledkcs -c "SELECT 1"`
- [ ] Redis连接: `docker compose exec redis redis-cli ping`
- [ ] MQTT连接: `mosquitto_sub -h localhost -t test`
- [ ] 日志查看: `docker compose logs -f api`

### iOS验证
- [ ] 应用启动正常
- [ ] 能够登录/注册
- [ ] 能够扫描蓝牙设备
- [ ] 能够连接后端API
- [ ] 能够发送MQTT消息

### KW47验证
- [ ] 串口输出: "YuleDKCS Booting..."
- [ ] BLE广播正常
- [ ] 能够被iPhone发现
- [ ] 配对流程完成
- [ ] 数字钥匙功能正常

---

## 🚨 问题排除

### 常见问题

**服务器问题**
```bash
# 容器无法启动
docker compose logs <service-name>

# 数据库连接失败
docker compose exec api env | grep DB_

# 端口占用
sudo lsof -i :8080
```

**iOS问题**
```bash
# 签名失败
codesign -d --entitlements - build/ios/iphoneos/Runner.app

# 设备未找到
flutter devices
xcrun simctl list devices

# 构建失败
flutter clean
rm -rf ios/Pods ios/Podfile.lock
flutter pub get
cd ios && pod install
```

**KW47问题**
```bash
# 连接失败
JLinkExe -device KW47B42ZxxxA -if SWD -speed 4000
# 检查SWD连接线

# 烧录失败
# 检查电源供应
# 检查Bootloader模式
```

---

## 📁 相关文件

| 文件 | 路径 | 说明 |
|------|------|------|
| 部署计划 | `DEPLOYMENT_PLAN.md` | 详细部署文档 |
| Docker Compose | `deploy/docker-compose.production.yml` | 生产环境配置 |
| Dockerfile | `backend/Dockerfile` | 服务构建 |
| iOS部署脚本 | `mobile/deploy_ios.sh` | 自动化部署 |
| KW47工具链 | `embedded/cmake/toolchain_kw47.cmake` | 交叉编译 |
| KW47链接脚本 | `embedded/cmake/kw47.ld` | 内存映射 |

---

*最后更新: 2026-05-16*
