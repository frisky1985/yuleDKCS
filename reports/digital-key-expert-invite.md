# 数字钥匙行业专家 — 三端综合评审邀请

> **状态**: ⏳ 待激活（等待 yuleDKCS loop 修复完成）
> **编制日期**: 2026-07-16
> **编制人**: 小马（Hermes，质量架构师）

---

## 1. 背景

yuleDKCS 数字钥匙系统已完成初步构建，当前正在经过多轮 CI/自动化修复（loop 修复）。**本邀请在 loop 修复全部完成后正式生效**，届时将激活为期 2-3 轮的专家评审会话。

项目定位为 **全栈数字钥匙参考实现**，覆盖 ICCE、CCC、ICCOA 三大主流数字钥匙协议栈，车端-移动端-云端三端闭环。

---

## 2. 评审范围

### 2.1 系统全景

```
车端 (Embedded C, 137 文件) ← BLE/UWB/NFC → 手机端 (Kotlin + Swift, ~130 文件)
        ↑                                           ↑
        |  gRPC (车云通信)                           |  HTTPS (REST API)
        ↓                                           ↓
云端 (Go Backend, ~256 文件)
 ├── DKCS 核心服务 (gRPC + PostgreSQL)
 ├── Hub 编排服务 (gRPC)
 ├── 通信协议规范
 └── TSP 适配器 (Java Spring Boot)
```

### 2.2 车端（Embedded C）

| 模块 | 路径 | 说明 |
|------|------|------|
| ICCE 协议栈 | `embedded/icce_protocol/` | BLE/UWB/NFC 协议实现 |
| CCC 协议栈 | `embedded/ccc_protocol/` | CCC 规范实现 |
| ICCOA 协议栈 | `embedded/iccoa_protocol/` | DK 3.0/4.0 协议栈 |
| BSW 集成层 | `embedded/bsw_integration/` | 基础软件集成 |
| MCAL 桩函数 | `embedded/mcal_stubs/` | MCAL 层桩函数 |
| 故障注入框架 | `embedded/fault-inject/` | 故障注入测试框架 |
| 测试用例 | `embedded/test_suite/`, `embedded/tests/` | 47 个 C Unity 测试用例 |

### 2.3 移动端（Kotlin + Swift）

| 模块 | 路径 | 说明 |
|------|------|------|
| Android SDK | `frontend/android/` | Kotlin SDK 核心库 |
| Android 演示 App | `frontend/android-app/` | 演示应用 |
| iOS SDK | `frontend/ios/` | Swift SDK（XcodeGen 项目） |
| iOS 演示 App | `frontend/ios-app/` | 演示应用 |
| Android 测试 | `frontend/android-app/` 等 | Espresso UI 测试 |
| iOS 测试 | `frontend/ios-app/`, `frontend/ios-tests/` | XCTest 测试 |

### 2.4 云端（Go Backend）

| 模块 | 路径 | 说明 |
|------|------|------|
| DKCS 核心服务 | `backend/dkcs/` | gRPC 服务 + PostgreSQL 持久化 |
| Hub 编排服务 | `backend/hub/` | 车云 gRPC 编排 |
| 云端基础设施 | `backend/cloud/` | 公共库/基础设施 |
| 数据库层 | `backend/db/` | 数据库模型和迁移 |
| 适配器 | `backend/adapters/` | TSP 适配器（Java Spring Boot） |

---

## 3. 评审维度

### 3.1 协议标准符合性

- ICCE 协议栈实现与 ICCE 标准的偏差分析
- CCC 协议栈对 CCC 规范的覆盖度及其合规性
- ICCOA DK 3.0/4.0 协议实现的规范符合度
- 跨协议统一通信层设计是否合理

### 3.2 架构合理性

- 三端之间的通信交互是否完备（车↔云 gRPC、手机↔云 HTTPS）
- BLE/UWB/NFC 通信层抽象是否合理
- 车端协议栈（ICCE/CCC/ICCOA）的统一协议层设计评价
- 手机 SDK 与演示 App 之间的职责分离
- 云服务（Hub ↔ DKCS）之间的分层与耦合度

### 3.3 安全性

- 密钥材料的生成、分发、存储、销毁全生命周期管理
- BLE/UWB/NFC 通信加密和身份认证
- gRPC/HTTPS 通信的 TLS/mTLS 配置和证书管理
- 防重放攻击机制
- 车端安全存储（Secure Element / TEE 抽象）

### 3.4 可生产性

- 代码成熟度（编译配置、依赖管理、错误处理）
- CI/CD 流水线覆盖和自动化测试集成
- Docker Compose 部署配置合理性
- 测试覆盖（单元测试 47C + 78 Espresso + 32 XCTest）
- 测试基础设施（模拟器、桩、故障注入）
- 跨平台兼容性（Android / iOS / 嵌入式 FreeRTOS）

### 3.5 文档完备性

- 代码注释和接口说明是否充分
- 架构文档、部署文档、API 文档
- 协议文档（Wire format / State machine）
- 新人上手引导（README、CONTRIBUTING）
- ASPICE 合规痕迹（需求追溯、测试用例对应）

---

## 4. 评估标准

| 维度 | 权重 | 评分标准 |
|------|------|----------|
| **Socket（协议规范覆盖）** | 30% | ICCE/CCC/ICCOA 三大协议的规范实现覆盖度 |
| **通信交互（端到端协议合规）** | 25% | 车↔云 gRPC、手机↔云 HTTPS 的协议交互是否符合规范 |
| **ASPICE 合规痕迹** | 20% | 需求→设计→实现→测试的全链路追溯性 |
| **业务功能完整性** | 25% | 数字钥匙全生命周期（生成→分发→使用→吊销）功能覆盖 |

评分等级：🥇 Excellent（≥90%）| 🥈 Good（75-89%）| 🥉 Satisfactory（60-74%）| ⚠️ Needs Improvement（<60%）

---

## 5. 评审流程

```
Phase 1: 独立审查（3 轮 subagent 会话语）
  ├── Round 1: 车端嵌入式 — 协议栈 + BSW + 测试
  ├── Round 2: 移动端 SDK — Android + iOS
  └── Round 3: 云端 — DKCS + Hub + 适配器

Phase 2: 综合报告
  ├── 各维度评分矩阵
  ├── Top-5 风险项 + 建议修复优先级
  └── 出厂检查清单

Phase 3: 评审工单
  ├── 对应 github issue / 飞书工单
  └── 责任人指派 + 截止日期
```

---

## 6. 产出物

评审完成后交付：

1. **`reports/digital-key-review-report.md`** — 完整评审报告（评分矩阵 + 风险项 + 修复建议）
2. **`reports/digital-key-qa-checklist.md`** — 出厂质量检查清单
3. 评审 commit comment / GitHub issue 引用

---

## 7. 前置条件（激活触发器）

本评审在满足以下条件后激活：

- [ ] yuleDKCS CI loop 全部修复完成（无红态）
- [ ] `embedded/tests/` 47 个 Unity 测试全部通过
- [ ] 前端编译无错误（Android `./gradlew assembleDebug` + iOS xcodebuild）
- [ ] 后端 `go build ./...` 无错误
- [ ] Docker Compose staging 环境可正常启动
- [ ] 三方测试桩/模拟器可用

---

## 8. 专家签名

```
评审专家: [TBD — 数字钥匙行业资深专家]
从业经验: 15+ 年 (车联网 / 数字钥匙 / DMS)
标准背景: ICCE / CCC / ICCOA
全栈范畴: 车端嵌入式 BLE/UWB/NFC → 手机端 SDK → 云端 KMS
```

---

> **📋 本邀请文档已就绪，等待 loop 修复完成后的激活通知。**
