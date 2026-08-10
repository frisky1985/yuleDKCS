# 🚗 YuleDKCS — 数字钥匙系统 (Digital Key System)

> 新一代智能网联汽车数字钥匙解决方案：**手机即钥匙**  
> 双协议兼容（ICCE + CCC + ICCOA），三端覆盖（嵌入式 + 原生App + 云端）

## 📌 项目状态

```
✅ 架构设计完成
✅ 嵌入式三协议栈开发完成（ICCE / CCC / ICCOA DK 3.0 & 4.0）
✅ 原生 Android/iOS SDK 开发完成
✅ 云端服务开发完成（Go hub + dkcs + Java adapters）
✅ 安全设计文档完成
✅ 测试用例设计完成
⏳ 集成测试待执行
⏳ CI/CD 流水线待搭建
```

## 🏗️ 系统架构

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│  嵌入式端(车端)   │◄──►│  App端(手机端)    │◄──►│    云端(服务平台)     │
│                 │    │                  │    │                     │
│ • UWB 测距      │    │ • Android SDK    │    │ • Hub (Go)         │
│ • BLE 通信      │    │ • iOS SDK        │    │ • DKCS (Go)        │
│ • NFC 交互      │    │ • BLE/UWB/NFC    │    │ • Adapters (Java)  │
│ • 安全 SE       │    │ • BerTLV 编解码   │    │ • Redis + Kafka    │
│ • ICCE/CCC/ICCOA│    │ • Key Management │    │ • gRPC + REST      │
└─────────────────┘    └──────────────────┘    └─────────────────────┘
```

## 📁 项目结构

> 顶层按四端分类：`backend`（云端）/ `frontend`（移动端 SDK+App）/ `embedded`（车端固件）/ `mobile`（新 SDK）+ `docs`（文档）/ `tests`（跨端集成）

```
yuleDKCS/
├── 📁 backend/                # 云端服务
│   ├── cloud/hub/             #   Hub 服务 (Go, gRPC)
│   ├── cloud/protocol/        #   通信协议规范 (+ api/ 合并: sdk.proto)
│   ├── cloud/deploy/          #   部署编排 (k8s/helm/docker-compose/certs)
│   ├── dkcs/                  #   DKCS 核心服务 (Go, MQTT TCU 通道)
│   ├── adapters/              #   TSP 适配器 (Java, Spring Boot)
│   ├── db/                    #   数据库迁移
│   └── scripts/               #   构建/迁移/验证脚本 (gen-proto, migrate-dkcs, sdk-hub-e2e)
│
├── 📁 frontend/               # 移动端 SDK + App
│   ├── android/               #   Android SDK (Kotlin)
│   ├── android-app/           #   Android App (MVVM)
│   ├── ios/                   #   iOS SDK (Swift)
│   ├── ios-app/               #   iOS App (UIKit)
│   ├── ios-tests/             #   iOS 测试
│   └── examples/              #   Demo 示例 (android/ + ios/DemoApp)
│
├── 📁 embedded/               # 车端固件
│   ├── icce_protocol/         #   ICCE 协议栈（BLE/UWB/安全/离线决策）
│   ├── ccc_protocol/          #   CCC 协议栈（BLE/UWB/NFC/安全）
│   ├── iccoa_protocol/        #   ICCOA 协议栈（DK 3.0 & 4.0）
│   ├── unified_protocol/      #   统一协议接口层
│   ├── firmware/              #   固件工程 (CMake, 原顶层 firmware/)
│   ├── include/               #   跨协议接口头文件 (dk_hal/dk_interfaces/dk_protocol)
│   ├── misra-rules.yaml       #   MISRA C:2023 规则集
│   ├── tests/ test_suite/     #   测试用例套件 (Unity)
│   └── system_architecture/   #   系统架构规范
│
├── 📁 mobile/                 # 新移动端 SDK (proto 生成目标)
│   ├── android/sdk/           #   Android SDK (Kotlin)
│   └── ios/                   #   iOS SDK (SwiftPM)
│
├── 📁 docs/                   # 文档
│   ├── SYSTEM_ARCHITECTURE.md #   系统架构设计
│   ├── API_REFERENCE.md       #   API 参考文档
│   ├── SECURITY_GUIDE.md      #   安全指南
│   ├── DEPLOYMENT_GUIDE.md    #   部署指南
│   ├── reports/               #   评审/测试报告 (原顶层 reports/)
│   ├── specs/                 #   OpenSpec 规范 (原顶层 specs/)
│   └── design/                #   详细设计文档（from digital-key-project）
│       ├── PRD.md             #   产品需求规格说明书
│       ├── ARCHITECTURE.md    #   架构设计（互补视角）
│       ├── PROJECT-PLAN.md    #   项目计划书 (WBS + 排期)
│       ├── DEV-TASKS.md       #   开发任务分解
│       ├── TEST-PLAN.md       #   测试计划
│       ├── API-CONTRACT.md    #   API 契约
│       ├── EMBEDDED-DEV-GUIDE.md # 嵌入式开发指南
│       ├── APP-DEV-GUIDE.md   #   App 开发指南
│       ├── CLOUD-DEV-GUIDE.md #   云端开发指南
│       ├── CODE-REVIEW-V2.md  #   代码审查报告（通过）
│       ├── DELIVERY-REPORT.md #   交付报告
│       └── S32K3-MIGRATION-GUIDE.md # S32K3 迁移指南
│
└── 📁 tests/                  # 跨端集成/E2E/安全测试 (e2e, integration, security, hil, qemu_m33)
```

## 🔧 技术栈

| 端 | 技术 | 说明 |
|----|------|------|
| **嵌入式** | C / CMake | NXP S32G2/G3, SE050, UWB SR250, BLE KW38 |
| **Android** | Kotlin / Gradle | BLE, UWB, NFC, BerTLV |
| **iOS** | Swift / XcodeGen | CoreBluetooth, CoreNFC, CoreLocation |
| **Hub** | Go / gRPC | 协议转发、密钥管理 |
| **DKCS** | Go / PostgreSQL | 核心业务服务 |
| **Adapters** | Java / Spring Boot | TSP 对接适配层 |

## 🔒 协议支持

| 协议 | 标准 | 状态 |
|------|------|------|
| **ICCE** | T/CA 110-2020（国密 SM2/SM3/SM4） | ⚠️ 部分实现 (国密算法待集成) |
| **CCC** | Digital Key 3.0 Release 1（ECDSA） | ✅ 完成 |
| **ICCOA** | DK 3.0 & DK 4.0 | ✅ 完成 |

## 📚 文档导航

### 核心文档（yuleDKCS 原生）
- [系统架构设计](docs/SYSTEM_ARCHITECTURE.md) — 当前系统架构规范
- [API 参考文档](docs/API_REFERENCE.md) — REST/gRPC 接口定义
- [安全指南](docs/SECURITY_GUIDE.md) — 安全架构与最佳实践
- [部署指南](docs/DEPLOYMENT_GUIDE.md) — 部署流程与配置

### 设计文档（from digital-key-project）
- [产品需求 PRD](docs/design/PRD.md) — 功能定义、用户故事、验收标准（26,000字）
- [项目计划](docs/design/PROJECT-PLAN.md) — 44任务/40周/7520工时
- [测试计划](docs/design/TEST-PLAN.md) — 三端测试用例矩阵
- [API 契约](docs/design/API-CONTRACT.md) — 详细接口契约定义
- [三端开发指南](docs/design/APP-DEV-GUIDE.md) — [嵌入式](docs/design/EMBEDDED-DEV-GUIDE.md) / [App](docs/design/APP-DEV-GUIDE.md) / [云端](docs/design/CLOUD-DEV-GUIDE.md)

## 🔄 版本历史

| 分支 | 说明 |
|------|------|
| `master` | 主开发分支（yuleDKCS 原生代码 + 补充文档） |
| `legacy-digital-key-project` | 原始 digital-key-project 归档（Flutter + Go 单体） |

## 🚀 快速开始

```bash
# 嵌入式编译
cd embedded/icce_protocol && mkdir build && cd build && cmake ..

# Android SDK 构建
cd frontend/android && ./gradlew build

# iOS SDK 构建
cd frontend/ios && xcodegen generate && xcodebuild -scheme DigitalKeySDK

# yuleDKCS 统一入口（Hub + DK Server 同进程，默认模式）
go run ./backend/cloud/hub/cmd/yuledkcs --mode=all-in-one --http-addr=:8080 --jwt-secret=xxx

# 仅启动编排层（Hub），密钥材料层通过 gRPC 连车厂 DK Server
go run ./backend/cloud/hub/cmd/yuledkcs --mode=hub-only --http-addr=:8080 --jwt-secret=xxx

# 仅启动密钥材料层（DK Server），接受 Hub 的 gRPC 请求
go run ./backend/cloud/hub/cmd/yuledkcs --mode=server-only --grpc-addr=:9090

# Java Adapters 启动（不变）
cd backend/adapters && mvn spring-boot:run
```

> **部署模式说明**: 参见 [DK Hub 架构设计](docs/design/DK-HUB-ARCHITECTURE.md)

## 📄 License

本项目基于 **Apache License 2.0** 许可协议发布。

```
Copyright [2026] [Yule Technology Co., Ltd.]

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

完整许可证文本请参阅 [LICENSE](./LICENSE) 文件。
