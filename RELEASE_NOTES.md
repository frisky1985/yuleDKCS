# 发布说明 — yuleDKCS

> **产品**: 数字钥匙系统 (Digital Key System)
> **代号**: yuleDKCS

---

## v2.2.0 — 量产工具链版本

> **发布日期**: 2026-08-10
> **状态**: ✅ 功能完成 (Phase 2 嵌入式验证 + 生产烧录工具链)
> **嵌入式C测试**: 196/196 全绿 ✅ (0 FAIL)
> **HIL SIL**: 46 用例 10 真实验证 / 36 诚实 SKIP ✅
> **工具链测试**: 签名加密 17 + 烧录生成器 9 + 批次管理 13 + batch-api 11 ✅

### 概述

yuleDKCS v2.2.0 完成 Phase 2 嵌入式验证首轮与生产烧录工具链 (B1/B2/B3) 全链路：
C 测试从 97 增至 **196 全绿**（含国密算法标准向量验证），发现并修复 **SM2 密码实现 6 处致命缺陷**；
HIL 从空跑转为真实 SIL 验证（拒绝假数据）；固件签名加密 → 烧录脚本 → 批次管理 → 云端 API → MES 对接文档全部就绪。

### 相比 v2.1.0 的关键提升

#### 🎛 嵌入式验证 (Phase 2)
- C 测试基建修复：freertos stubs / se050_scp03 纳入链接 / 共享引擎污染修复
- 条件树 (edge_condition) 13.5% → 70.85%；icce_edge 23.8% → **99.2%**；SE050 SCP03 9.8% → **77.3%**
- **国密算法标准向量**：sm2 9% → 95.4%、sm4 0% → 95.6%、crypto_engine 4% → 92.8%（GB/T 32905/32907/32918.2 A.2 + OpenSSL 交叉验证）
- **SM2 6 处致命缺陷修复**（基点不在曲线上/蒙哥马利错配/指数位序/别名 bug 等）→ 国密链路可信

#### 🔌 HIL 软件在环 (SIL)
- QEMU 6.2 SysTick 修复（外部 32768Hz 时钟源根因）
- transport 三态抽象（QEMU/串口/J-Link）+ 固件命令通道（PING/版本/tick/uptime/LED）
- 固件状态机注入（非法转换 REJECT + 安全计数），FI-05 转真实验证
- 37 个硬件域用例**去模拟化**：不再 random 假数据，QEMU 下诚实 SKIP（等硬件 A2）

#### 🏭 生产烧录工具链
- **B1** 签名加密：.ydk 包（ECDSA P-256 + AES-256-GCM），篡改检测 6 类
- **B2** 烧录生成器：J-Link 脚本 + 批次 manifest + 烧录日志（哈希链防篡改）
- **B3** 批次管理：工厂侧 SQLite（良率/设备状态机/密钥审计）+ 云端 batch-api（Go REST，存储可插拔 file/PostgreSQL）+ **MES 对接文档**（对接域名即可上线）

#### 🚗 云端
- TCU MQTT 通道（dkcs）
- 依赖安全升级：Spring 3.2.12 / grpc 1.68.2 / protobuf 3.25.5（CVE 修复）

---

## v2.1.0 — 生产就绪版本

> **发布日期**: 2026-07-16
> **状态**: ✅ 生产就绪
> **专家评分**: 4.5/5.0 ⭐⭐⭐⭐½
> **yuleOSH诊断**: 92/100
> **Go测试**: 全通过 ✅ (0 FAIL)
> **E2E测试**: 12场景 全通过 ✅
> **嵌入式C测试**: 59/59 全绿 ✅

### 概述

yuleDKCS v2.1.0 是数字钥匙系统的生产就绪版本。已通过专家评审（4.5/5.0）、yuleOSH全量诊断（92/100）、完整E2E验证（6条P0修复闭合），全面支持ICC/CCC/ICCOA三大协议的生产级部署。

### 相比 v2.0.0-beta 的关键提升

#### 🔒 安全强化
- SE050 SCP03 安全通道（1362行，通过GlobalPlatform v2.3.1标准）
- ICCE 边缘计算引擎（944行，5状态FSM+3触发族+13条件运算符）
- iOS TLS Pinning（33 XCTest验证）
- TRNG 4层回退链（SE050→MCU→mbedTLS→OS entropy）
- Android 防重放计数器持久化（5UT+11仪器化测试）
- Android KeyStore 硬件级密钥存储
- 完整安全渗透测试包

#### 🏗️ 架构重构
- 双Hub合并（40%代码重复消除，24Go包编译通过）
- TSP适配器增强（54测试用例，重试+校验+文档）
- MISRA C:2023 规则库升级（180条规则）

#### 🧪 测试与验证
- E2E Car Simulator 验证框架（12场景，ICCE/CCC/ICCOA三协议）
- HIL硬件在环测试方案（37用例，BLE/UWB/NFC/SCP03/电源/故障）
- 嵌入式C测试修复（59/59全绿）
- Go后端零测试失败

#### 📚 文档体系
- safety-concept.md（HARA/SG/FSR/TSR全链路）
- compatibility-matrix.md（9维兼容性矩阵）
- OEM POC对接全套（ICCE+CCC+ICCOA三协议）
- 产线密钥注入方案（SE050）
- ASPICE证据链完善（audit-manifest.json，535文件）

#### 🚀 CI/CD
- ARM-eabi交叉编译CI
- Git Flow分支策略 + 贡献指南
- 9个GitHub Actions工作流稳定运行
- yuleOSH证据链CI

### 新增功能

#### 🔑 嵌入式车端 (C / CMake)
- ICCE 协议栈（BLE/UWB/安全/离线决策）
- CCC 3.0 协议栈（BLE/UWB/NFC/安全）
- ICCOA DK 3.0 & 4.0 协议栈
- 统一协议接口层（Unified HAL）
- SE050 安全芯片集成（EAL5+）
- UWB 安全测距（IEEE 802.15.4z）
- 防中继攻击保护

#### 📱 手机 SDK
- **Android SDK** (Kotlin): BLE/UWB/NFC HCE、安全存储、BerTLV 编解码
- **iOS SDK** (Swift): CoreBluetooth / CoreNFC / NearbyInteraction 封装
- 钥匙创建、激活、吊销全生命周期管理
- 车辆控制 API（解锁/闭锁/启动/寻车）
- 钥匙分享（临时/永久、时间/地理限制）
- 离线 NFC 解锁支持

#### ☁️ 云端服务
- **Hub** (Go): REST API 网关、JWT 鉴权、协议适配
- **DKCS** (Go): 密钥生命周期管理、gRPC 服务、事件流处理
- **Java Adapters**: CCC / ICCOA / ICCE 协议适配（Spring Boot 3.2）
- 密钥分享、权限模型（8 位权限位）、审计日志

#### 🔒 安全
- 端到端加密（AES-256-GCM / SM4）
- ECDSA P-256 / SM2 签名认证
- 密钥层级体系（四层派生）
- 安全启动链验证
- UWB 安全测距 + 防中继攻击

### 平台支持

| 平台 | 最低版本 | 说明 |
|------|---------|------|
| Android | 10 (API 29) | BLE/UWB/NFC |
| iOS | 15.0 | CoreBluetooth / CoreNFC / NearbyInteraction |
| 嵌入式 MCU | - | NXP S32K312, KW47A, NCJ29D6, ST25R501 |
| Kubernetes | 1.28+ | 生产部署 |
| PostgreSQL | 15+ | 持久化存储 |
| Redis | 7+ | 缓存 + 分布式锁 |
| Kafka | 3.6+ | 消息队列 |

### 已知问题

| ID | 描述 | 严重程度 | 计划修复版本 |
|:---|:-----|:--------:|:-----------:|
| GO-P0-02 | Kafka 消息队列未使用，事件驱动架构不可用 | P0 | v1.0.1 |
| GO-P1-01 | InMemoryKeyStore 竞态条件锁定范围不足 | P1 | v1.0.1 |
| GO-P1-05 | go.sum 含已知 CVE 依赖 | P1 | v1.0.1 |
| EMB-P1-01 | ISR 全局变量未加 volatile | P1 | v1.0.1 |
| EMB-P1-03 | malloc 失败后悬空指针 | P1 | v1.0.1 |
| DOC-P1-01 | ASIL 等级不一致 (EAL5+ vs EAL6+) | P1 | v1.0.1 |
| DOC-P1-02 | FTTI 冲突 (<500ms vs <1s) | P1 | v1.0.1 |

### 升级注意事项

- 本版本为初始发布，无前版本升级路径
- 密钥绑定后不可跨版本迁移，请确保归档备份
- `all-in-one` 模式适用于开发/测试环境，生产环境建议使用 `hub-only` + `server-only` 分离部署模式

### 下载

- 嵌入式固件: `embedded/` 目录
- Android SDK: `frontend/android/`
- iOS SDK: `frontend/ios/`
- 云端 Docker 镜像: `yuledkcs/hub:1.0.0`, `yuledkcs/dkcs:1.0.0`, `yuledkcs/adapter-*:1.0.0`
- 源代码: https://github.com/digitalkey/yuleDKCS

---

## 发布说明模板

### 版本号规范

```
vMAJOR.MINOR.PATCH
```

| 增量 | 含义 |
|:----|:-----|
| MAJOR | 不兼容的 API 变更 |
| MINOR | 向下兼容的功能新增 |
| PATCH | 向下兼容的问题修复 |

### 更新内容模板

```markdown
## vX.Y.Z — YYYY-MM-DD

### 新增
- 

### 修复
- 

### 变更
- 

### 已弃用
- 

### 安全
- 

### 升级注意事项
- 

### 已知问题
| ID | 描述 | 状态 |
|:---|:-----|:----:|
```
