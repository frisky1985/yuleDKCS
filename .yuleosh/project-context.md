# yuleDKCS — 项目上下文 (Project Context)

> 生成时间: 2026-07-07
> 基于: README.md + docs/design/PRD.md + docs/SYSTEM_ARCHITECTURE.md

---

## 0.1 目标硬件平台

| 维度 | 决策 |
|:-----|:-----|
| MCU 型号 | NXP S32G2/G3 (车规级网关处理器, 4×Cortex-A53 + 3×M7) |
| 安全芯片 | NXP SE050 (eSE, 支持 EAL6+ 安全等级) |
| UWB 模块 | NXP SR250 / Qorvo DW3000 系列 |
| BLE 模块 | NXP KW38 / KW45 |
| 存储 | 外部 Flash (QSPI NOR) + 板载 eMMC |
| 通信 | CAN FD (车辆内部总线) + Ethernet (SOME/IP) |

## 0.2 ASIL 目标

| 功能域 | ASIL 等级 | 说明 |
|:-------|:---------|:-----|
| 车门解锁 | ASIL-B | 非预期解锁有安全风险 |
| 发动机启动 | ASIL-B | 防盗相关，防止非授权启动 |
| BLE 通信 | QM | 连接管理，非安全关键 |
| UWB 测距 | ASIL-B(D) | 距离测量精度影响解锁判定 |
| NFC 刷卡 | QM | 被动式，可降级处理 |
| 密钥管理 (SE) | ASIL-B | 密钥存储和安全运算 |
| 远程控车 | ASIL-A | 非实时非安全关键 |
| 钥匙分享管理 | QM | 业务逻辑 |

## 0.3 BSW 平台

| 组件 | 来源 | 状态 |
|:-----|:-----|:----:|
| MCAL | yuleASR (S32K312/S32G2 MCAL 模块) | 待集成 |
| ECUAL: CanIf | yuleASR | 待集成 |
| ECUAL: LinIf | yuleASR | 待集成 |
| Services: COM | yuleASR (Signal→I-PDU 映射) | 待集成 |
| Services: DCM | yuleASR (UDS 诊断协议栈) | 待集成 |
| Services: DEM | yuleASR (DTC 管理) | 待集成 |
| Services: NvM | yuleASR (NVRAM 管理) | 待集成 |
| Services: OS | yuleASR (OSEK AUTOSAR OS) | 待集成 |
| Services: EcuM | yuleASR (ECU 状态管理) | 待集成 |
| Services: WdgM | yuleASR (看门狗管理) | 待集成 |
| Crypto: CSM | yuleASR (mbedTLS + SecOC) | 待集成 |

## 0.4 工具链

| 工具 | 用途 | 状态 |
|:-----|:-----|:----:|
| yuleOSH CLI | 全流程编排 | ✅ 已初始化 |
| cppcheck | MISRA C:2023 静态分析 | ✅ 可用 |
| gcc-arm-none-eabi | 嵌入式交叉编译 | 待确认 |
| golangci-lint | Go 静态分析 | ✅ (yuleDKCS 已配 .golangci.yml) |
| gcov/lcov | 代码覆盖率 | ✅ 可用 |
| Docker | 容器化 CI | ✅ (Dockerfile 就绪) |

## 0.5 项目概览

| 维度 | 说明 |
|:-----|:-----|
| 项目名称 | yuleDKCS — 数字钥匙系统 |
| 三端架构 | 嵌入式(C) + 移动 App (Android Kotlin / iOS Swift) + 云端 (Go + Java) |
| 协议标准 | ICCE (T/CA 110-2020) + CCC Digital Key 3.0 + ICCOA DK 3.0 & 4.0 |
| 安全等级 | EAL5+ (SE), 端到端加密, 防中继攻击 |
| 现有代码 | Embedded 三协议栈 ✅ / Android SDK+App ✅ / iOS SDK+App ✅ / Go Hub+DKCS ✅ / Java Adapters ✅ |
| 现有文档 | PRD / 架构设计 / API 契约 / 测试计划 / 安全指南 / 部署指南 / 开发指南 / 代码审查 |

## 0.6 待办项 (P0a 未覆盖)

- [ ] 确认嵌入式交叉编译工具链版本 (gcc-arm-none-eabi 具体版本)
- [ ] 确认 Docker 基础镜像选型 (Ubuntu 22.04 / Alpine)
- [ ] 建立 MISRA C:2023 规则基线 (基于 yuleDKCS 代码风格)
