# yuleDKCS - 数字钥匙系统

一套完整的数字钥匙解决方案，支持 CCC、ICCOA、ICCE 三大主流协议。

## 项目结构

```
yuleDKCS/
├── frontend/           # 手机端 SDK + App
│   ├── android/        # Android SDK
│   ├── android-app/    # Android Demo App
│   ├── ios/            # iOS SDK
│   └── ios-app/        # iOS Demo App
├── backend/            # 云端服务
│   ├── adapters/       # Java 协议适配器
│   ├── cloud/          # Go 服务 (HUB + DKCS)
│   └── db/             # 数据库脚本
├── embedded/           # 车端嵌入式协议栈
│   ├── ccc_protocol/   # CCC 3.0 协议栈
│   ├── iccoa_protocol/ # ICCOA DK3.0/DK4.0 协议栈
│   └── icce_protocol/  # ICCE 协议栈
└── docs/               # 项目文档
    ├── SYSTEM_ARCHITECTURE.md  # 系统架构设计
    ├── API_REFERENCE.md        # API 参考文档
    ├── DEPLOYMENT_GUIDE.md     # 部署指南
    └── SECURITY_GUIDE.md       # 安全指南
```

## 核心特性

- **多协议支持**: CCC 3.0 / ICCOA DK3.0&DK4.0 / ICCE
- **三模通信**: NFC + BLE + UWB
- **跨平台**: iOS / Android 双端 SDK
- **云端服务**: Go 高并发核心 + Java 协议适配器
- **车端嵌入式**: C 语言协议栈，支持多种 MCU

## 快速开始

### 本地开发

```bash
# 克隆仓库
git clone https://github.com/frisky1985/yuleDKCS.git
cd yuleDKCS

# 启动依赖服务 (PostgreSQL + Redis + Kafka)
docker-compose up -d

# 运行 DKCS 服务
cd backend/dkcs
go run cmd/dkcs/main.go

# 运行 HUB 服务
cd backend/cloud/hub
go run cmd/hub/main.go
```

### 部署

详见 [部署指南](docs/DEPLOYMENT_GUIDE.md)

## 文档

- [系统架构设计](docs/SYSTEM_ARCHITECTURE.md)
- [API 参考文档](docs/API_REFERENCE.md)
- [部署指南](docs/DEPLOYMENT_GUIDE.md)
- [安全指南](docs/SECURITY_GUIDE.md)

## 技术栈

| 层级 | 技术选型 |
|------|----------|
| 云端核心 | Go 1.22+ / gRPC / Kafka / PostgreSQL / Redis |
| 协议适配 | Java 17 / Spring Boot 3.2 |
| 手机端 | Kotlin (Android) / Swift (iOS) |
| 车端 | C / STM32L5 / NXP KW47A / NCJ29D6 / ST ST25R501 |
| 安全 | SE050 / TFM / AES-256-GCM / ECDSA P-256 |

## 许可证

私有项目，未经授权禁止使用。

---

**版本**: 1.0.0
**日期**: 2026-05-06
