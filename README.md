# yuleDKCS - 数字钥匙连接系统

[![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)](CHANGELOG_v2.0.0.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

yuleDKCS（Yule Digital Key Connectivity Stack）是一个完整的车辆数字钥匙解决方案，支持 CCC、ICCOA、ICCE 等主流数字钥匙协议，提供从车辆嵌入式到云端、移动端的全栈技术实现。

## 项目结构

```
yuleDKCS/
├── backend/          # 后端服务 - Go语言实现
├── frontend/         # 前端管理后台 - React + TypeScript
├── mobile/           # 移动SDK - Android/iOS/Flutter
├── embedded/         # 嵌入式SDK - C/C++
├── docs/             # 文档
├── deploy/           # 部署配置
└── README.md         # 项目说明
```

### 模块说明

#### 🔧 backend/ - 后端服务
- **技术栈**: Go, PostgreSQL, Redis, MQTT, WebSocket
- **功能**: API服务、数据库管理、消息中介、设备管理
- **特色**: 多租户支持、实时通信、分布式部署
- [查看详情](backend/README.md)

#### 🎮 frontend/ - 前端管理后台
- **技术栈**: React 18, TypeScript, Material-UI
- **功能**: 钥匙管理、车辆管理、数据可视化、监控面板
- **特色**: 响应式设计、暗色主题、实时更新
- [查看详情](frontend/README.md)

#### 📱 mobile/ - 移动SDK
- **Android**: Kotlin, BLE 5.0, Android Keystore
- **iOS**: Swift, CoreBluetooth, Secure Enclave
- **Flutter**: Dart, 跨平台支持
- **功能**: 钥匙管理、BLE连接、车辆控制、实时状态
- [查看详情](mobile/android/README.md) | [iOS指南](mobile/ios/README.md)

#### 🚗 embedded/ - 嵌入式SDK
- **技术栈**: C/C++, AUTOSAR, FreeRTOS, SE050安全芯片
- **功能**: 多协议支持(CCC/ICCOA/ICCE)、UWB定位、安全加密
- **特色**: ASIL-D功能安全等级、AUTOSAR集成、SE050硬件加密
- [查看详情](embedded/sdk/README.md)

#### 📚 docs/ - 文档
- `架构设计`: 系统架构、模块设计
- `协议规范`: CCC/ICCOA/ICCE协议实现细节
- `API文档`: REST API和SDK接口
- `使用指南`: 快速入门、部署指南
- `测试报告`: 测试覆盖率、MISRA合规
- [查看所有文档](docs/)

#### 🚀 deploy/ - 部署配置
- Docker配置和组合
- Kubernetes Helm Charts
- 各环境配置文件
- 部署自动化脚本
- [查看详情](deploy/)

## 快速开始

### 环境要求

- **后端**: Go 1.21+, PostgreSQL 14+, Redis 7+
- **前端**: Node.js 18+, npm 9+
- **移动**: Android Studio Hedgehog+ / Xcode 15+
- **嵌入式**: GCC 11+, CMake 3.20+, ARM GCC Toolchain

### 安装和运行

```bash
# 克隆仓库
git clone https://github.com/frisky1985/yuleDKCS.git
cd yuleDKCS

# 启动后端
cd backend
go mod download
go run main.go

# 启动前端(新窗口)
cd ../frontend
npm install
npm run dev

# 构建嵌入式SDK
cd ../embedded
mkdir build && cd build
cmake ..
make
```

## 功能特性

### 🔑 数字钥匙管理
- 多协议支持（CCC/ICCOA/ICCE）
- 钥匙分享和授权管理
- 使用记录和审计日志
- 有效期和权限精细控制

### 📡 车辆连接
- BLE 5.0 低功耗连接
- UWB 精确定位（可选）
- 多车辆同时管理
- 离线操作支持

### 🚗 车辆控制
- 解锁/锁车
- 引擎启动/停止
- 车窗/后备箱控制
- 寻车功能

### 🔐 安全保障
- SE050硬件安全元
- ECC P-256 加密签名
- SCP03 安全通道
- 生物识别验证

## 开发指南

### 代码规范
- 嵌入式代码遵守 MISRA C:2012 规范
- Go 代码使用 gofmt 和 golangci-lint
- TypeScript 使用 ESLint + Prettier

### 测试
```bash
# 后端测试
cd backend
go test ./...

# 前端测试
cd frontend
npm run test

# 嵌入式测试
cd embedded/build
make test
```

### 文档更新
请确保代码变更同步更新相关文档：
- API变更→更新 `docs/api/`
- 架构变更→更新 `docs/architecture/`
- 协议变更→更新 `docs/protocol-specs/`

## 项目进度

- [x] 系统架构设计
- [x] 后端服务开发
- [x] 前端管理后台
- [x] Android SDK
- [x] iOS SDK
- [x] 嵌入式SDK
- [x] 协议实现(CCC/ICCOA/ICCE)
- [x] 安全加密模块
- [x] UWB定位支持
- [x] 自动化部署

## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'feat: add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 打开 Pull Request

请确保使用 [Conventional Commits](https://www.conventionalcommits.org/) 提交规范。

## 版本历史

- **v2.0.0** (2026-05-11) - 完整的数字钥匙生态系统
- **v1.0.0** (2026-03-01) - 首个正式版本

查看完整版本历史：[CHANGELOG](docs/guides/CHANGELOG_v2.0.0.md)

## 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情

## 联系我们

- 邮箱: support@yuledkcs.com
- 问题反馈: [GitHub Issues](https://github.com/frisky1985/yuleDKCS/issues)
- 文档网站: https://docs.yuledkcs.com

---

<p align="center">
  <strong>yuleDKCS</strong> - 让数字钥匙连接更安全、更便捷
</p>
