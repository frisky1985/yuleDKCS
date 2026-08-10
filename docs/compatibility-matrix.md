# 🧩 yuleDKCS 兼容性矩阵

> **文档版本**: 1.0.0  
> **创建日期**: 2026-07-16  
> **依据**: [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md)、[SECURITY_WHITEPAPER.md](SECURITY_WHITEPAPER.md)、[digital-key-expert-review.md](reports/digital-key-expert-review.md)  
> **范围**: 覆盖车端（Embedded C）、移动端（Android SDK / iOS SDK）、云端（Go / Java）全平台

---

## 1. 协议标准兼容性

| 协议 | 版本 | 车端 | Android | iOS | 云端 | 覆盖度 | 状态 |
|------|------|------|---------|-----|------|--------|------|
| **ICCE** | T/CA 110-2020 | ✅ BLE 0xFEFA UUID、UWB 测距、边缘计算触发框架 | ✅ ICCE BLE 自适应 | ✅ ICCE BLE 自适应 | ✅ IcceAdapter (gRPC) | ~85% | ⚠️ 边缘引擎 stub，SM2 验签待完成（见 `sec_verify()` TODO） |
| **CCC Digital Key** | 3.0 | ✅ NFC OOB 交换、BLE GATT 0xFFD1、UWB FiRa 测距、SE050 安全通道、10 状态机 | ✅ CCC 协议自适应 | ✅ CCC 协议自适应（Apple Wallet） | ✅ CccAdapter (gRPC) | ~90% | ✅ 最完整协议栈；⚠️ SCP03 硬件通道、X.509 证书链待补全 |
| **ICCOA Digital Key** | DK 3.0 + DK 4.0 | ✅ 双帧结构（`iccoa_dk30_frame_t` / `iccoa_dk40_frame_t`）、BLE UUID 0xFEF5、8 类权限 | ✅ ICCOA BLE 自适应 | ✅ ICCOA BLE 自适应 | ✅ IccoaAdapter (gRPC) | ~85% | ⚠️ DK40 HMAC 算法需与 ICCOA DS v4.0 对齐；session 持久化待补全 |
| **Unified HAL** | — | ✅ `dk_unified.h` 统一 API、12 项设备能力位、5 级区域（`dk_zone_e`） | N/A | N/A | ✅ Unified Key Service（协议协商 + 自动路由） | ~80% | ⚠️ 内部路由实现在车端以 stub 为主，云端统一层完整 |

### 协议标准参考

- ICCE T/CA 110-2020：车联网通信引擎标准，BLE UUID 0xFEFA 系列
- CCC Digital Key 3.0：Car Connectivity Consortium 规范，NFC OOB + BLE GATT 0xFFD1 + UWB FiRa
- ICCOA DK 3.0/DK 4.0：ICCE 开放联盟规范，BLE UUID 0xFEF5

> 来源：[专家评审 §1 - 协议标准符合性](reports/digital-key-expert-review.md) | [架构设计 §1.2 核心特性](SYSTEM_ARCHITECTURE.md#12-核心特性)

---

## 2. 硬件兼容性

| 组件 | 型号 | 协议/接口 | 车端驱动 | 加密能力 | 状态 |
|------|------|-----------|----------|---------|------|
| **MCU** | NXP S32K312 | ARM Cortex-M7（S32K3 系列，片载 HSE 安全引擎） | ✅ 架构层支持 | 安全启动链 (SE050/HSE) | ⚠️ CI 中仅 gcc 编译，生产交叉编译（ARM-eabi）未纳入 CI |
| **BLE SoC** | NXP KW47A | BLE 5.x GATT | ✅ `hal_ble.h` + `ble_kw47a.c` | AES-CCM 128-bit（LE Secure Connections） | ✅ 驱动层存在，三协议 BLE 操作复用 |
| **UWB SoC** | NXP SR250 / NCJ29D6 | IEEE 802.15.4z (HRP) | ✅ `hal_uwb.h` + `uwb_ncj29d6.c` | AES-128 + STS 安全测距 | ⚠️ FiRa 测距实现为 stub，生产部署需替换为真实驱动 |
| **NFC Reader** | ST ST25R501 | ISO/IEC 14443 A/B, ISO 7816-4 | ✅ `hal_nfc.h` + `nfc_st25r501.c` | AES-256-GCM 安全通道 | ✅ 驱动层存在，NDEF + APDU 交互支持 |
| **Secure Element** | NXP SE050 | I²C / SPI, SCP03 安全通道 | ✅ `hal_sec.h` + SE050 驱动框架 | AES-128/256, ECC P-192/256/384/521, RSA 2K–4K, TRNG | ⚠️ `se05x_open_session` 等核心函数为 stub（P0 投产前必须解决） |
| **HSM (云端)** | AWS CloudHSM / Azure Managed HSM | PKCS#11 / JCE | N/A | FIPS 140-2 Level 3 | ✅ Enterprise Edition 可选 |

### 车端硬件架构

| 层级 | 组件 | 通信方式 |
|------|------|----------|
| 应用处理器 | NXP S32K312 (MCU + MPU) | 内部总线 |
| 无线连接 | NXP KW47A (BLE) + NCJ29D6 (UWB) | SPI / UART |
| NFC 读卡器 | ST ST25R501 | SPI / I²C |
| 安全元件 | NXP SE050 | I²C (SCP03 安全通道) |

> 来源：[架构设计 §1.3 技术栈](SYSTEM_ARCHITECTURE.md#13-技术栈) | [安全白皮书 §6.2 Key ID 分配](SECURITY_WHITEPAPER.md#62-key-id-allocation) | [专家评审 §2 - 三端架构合理性](reports/digital-key-expert-review.md)

---

## 3. 平台兼容性

| 平台 | 最低版本 | SDK 版本 | 测试框架 | 测试用例数 | 测试状态 |
|------|---------|----------|---------|-----------|---------|
| **iOS** | 15.0+（推荐 16.0+） | Swift SDK (CryptoKit、CoreNFC、CoreBluetooth、NearbyInteraction) | XCTest | 32 | ✅ 安全设计达量产标准（Keychain-first API Key）；⚠️ 无 TLS Pinning、需补充 |
| **Android** | 12+（推荐 13+） | Kotlin SDK (Android Keystore StrongBox、BLE Scanner、HCE、UWB API) | Espresso | 78 | ✅ BLE 管理器三协议自适应；⚠️ EncryptedSharedPrefs → 建议改用 KeyStore 直接管理 |
| **Go** | 1.21+（实际使用 1.22+） | gRPC-go、Sarama (Kafka)、pgx (PostgreSQL)、go-redis | Go testing + Testify | ~150+ (DKCS ~100 + Hub ~50) | ✅ 幂等性保障、状态机完整、事件驱动；生产级质量 |
| **Java** | 17+ | Spring Boot 3.2、gRPC-java、Protobuf 3 | JUnit (待补充) | 0 | ⚠️ TSP 适配器无单元测试，需补充 mock 测试 |
| **Embedded C** | MISRA C:2012 | Unity 测试框架 + FreeRTOS | Unity + CMock | 47 | ✅ CI 集成 Unity 测试；⚠️ ARM-eabi 交叉编译 CI 缺失 |

### 平台 SDK 架构对称性

| 功能模块 | Android SDK | iOS SDK |
|----------|------------|---------|
| 统一入口 | `DigitalKeyClient` | `DigitalKeyClient` |
| 密钥管理 | `KeyManager` | `KeyManager` |
| 车辆控制 | `VehicleManager` / `VehicleController` | `VehicleManager` / `VehicleController` |
| BLE 通信 | `BleManager`（三协议自适应） | `BleManager`（CoreBluetooth 封装） |
| NFC 通信 | `NfcManager`（HCE） | `NfcManager`（CoreNFC HCE） |
| UWB 通信 | `UwbManager` | `UwbManager`（NearbyInteraction） |
| 安全存储 | AndroidKeyStore + EncryptedSharedPrefs | iOS Keychain（硬件加密，行业标杆） |
| 加密引擎 | `CryptoEngine` | `CryptoEngine`（CryptoKit） |
| 错误码 | `DkError` | `DigitalKeyError` |
| 异步模型 | Kotlin Coroutines | Combine |

> 来源：[架构设计 §3.2 手机端架构](SYSTEM_ARCHITECTURE.md#32-手机端架构-frontend) | [专家评审 §3 安全性](reports/digital-key-expert-review.md)

---

## 4. 通信协议兼容性

| 通信方式 | 标准/协议 | 加密 | 认证 | 距离/范围 | 状态 |
|---------|----------|------|------|-----------|------|
| **BLE** | 4.2 / 5.x (GATT, LE Secure Connections) | AES-CCM 128-bit | OOB Pairing + Numeric Comparison + Challenge-Response | ~10m | ✅ 三协议统一 BLE 抽象；⚠️ Android `WRITE_TYPE_DEFAULT` 不支持动态协商，部分车端需 `WRITE_TYPE_NO_RESPONSE` |
| **UWB** | IEEE 802.15.4z (HRP) | AES-128 + STS Scrambled Timestamp Sequence | PHY-level 认证 + 双向测距（±10cm） | ~0–50m | ✅ STS 安全测距框架；⚠️ FiRa 测距为 stub，生产需补全 |
| **NFC** | ISO/IEC 14443 Type A/B, ISO 7816-4 | AES-256-GCM（Secure Channel） | ECDH 密钥协商 + Mutual Auth | ~4cm | ✅ 支持 NDEF + Secure APDU 交换；车端 ST25R501 + SE050 安全通道 |
| **App ↔ Cloud** | HTTPS / TLS 1.3 | AES-256-GCM (TLS AEAD) | JWT Bearer + Server Cert Pinning | N/A | ⚠️ iOS `URLSession` 无自定 TLS Pinning（建议 `SecTrustEvaluate` + public key hash） |
| **Cloud ↔ Hub** | gRPC + TLS 1.3 | AEAD | mTLS 双向证书认证 | N/A | ✅ Helm 含 mTLS secrets 模板；Proto3 定义完整 |
| **Hub ↔ TCU** | MQTT + TLS 1.3 | AES-256 | 客户端证书 + Challenge-Response | N/A | ✅ 车联网行业标准，QoS 支持 |
| **Cloud ↔ TSP** | HTTPS / mTLS | TLS 1.3 | mTLS + API Key | N/A | ⚠️ TSP 适配器实现偏 skeleton，需真机对接验证 |

### 通信安全等级

| 安全边界 | 信任等级 | 安全控制 | 参考 |
|---------|---------|---------|------|
| **T1**: SE050 ↔ TCU SoC | **T** (Trusted) | 内部总线 + 物理防护 | [安全白皮书 §1.2](SECURITY_WHITEPAPER.md#12-trust-boundaries) |
| **T2**: TCU ↔ Cloud | **P** (Protected) | mTLS + 证书验证 | [安全白皮书 §1.2](SECURITY_WHITEPAPER.md#12-trust-boundaries) |
| **T3**: Cloud ↔ Mobile | **P** (Protected) | TLS 1.3 + cert pinning | [安全白皮书 §1.2](SECURITY_WHITEPAPER.md#12-trust-boundaries) |
| **T4**: Mobile ↔ TCU (BLE/UWB/NFC) | **U** (Untrusted) | 加密 Challenge-Response | [安全白皮书 §1.2](SECURITY_WHITEPAPER.md#12-trust-boundaries) |
| **T5**: Cloud ↔ TSP/Vendor | **P** (Protected) | mTLS + API Key + 限流 | [安全白皮书 §1.2](SECURITY_WHITEPAPER.md#12-trust-boundaries) |

> 来源：[安全白皮书 §1.2 信任边界](SECURITY_WHITEPAPER.md#12-trust-boundaries)、[安全白皮书 §5.1 通道安全总结](SECURITY_WHITEPAPER.md#51-channel-security-summary) | [架构设计 §6.1 协议栈选择](SYSTEM_ARCHITECTURE.md#61-协议栈选择)

---

## 5. 部署环境兼容性

| 环境 | 版本要求 | 配置方式 | 高可用 | 状态 |
|------|---------|---------|-------|------|
| **Kubernetes** | 1.25+ | Helm Chart（DKCS + Hub + EMQX + Kafka + PostgreSQL + Redis + Ingress + HPA + PDB） | ✅ 多副本 + HPA + PDB；3 AZ | ✅ Helm 完整部署方案；⚠️ 无灰度发布/金丝雀策略 |
| **Docker** | 24+ | Docker Compose（开发 + 监控两套） | N/A | ✅ 多环境多编排文件 |
| **PostgreSQL** | 15+ | Primary + Replica 架构 | ✅ Multi-AZ, PITR, RPO < 1h | ✅ 字段级加密（AES-256 + KMS + HKDF 列密钥派生） |
| **Redis** | 7+ | Cluster 模式 | ✅ 集群 + 分布式锁 | ✅ 缓存 + 分布式锁 + Session 存储 |
| **Kafka** | 3.x | 3 broker cluster | ✅ 副本因子 ≥ 3 | ✅ 异步事件流 + 消息持久化 |

### 监控与可观测性兼容性

| 组件 | 集成方式 | 状态 |
|------|---------|------|
| **Prometheus** | Go 服务指标 + Java 服务指标 + K8s 指标 + 自定义业务指标 | ✅ Service Alerts 已定义 |
| **Grafana** | 服务健康看板 + 业务指标看板 + 告警规则 | ✅ 看板模板存在 |
| **ELK / EFK** | Elasticsearch + Logstash/Fluentd + Kibana | ✅ 统一日志采集管道 |
| **Jaeger** | 分布式追踪 + 性能分析 | ✅ 链路追踪集成 |
| **Cert-Manager** | Let's Encrypt 自动证书管理 | ✅ 集成在 Helm Chart |

> 来源：[架构设计 §7 部署架构](SYSTEM_ARCHITECTURE.md#7-部署架构) | [安全白皮书 §7.2 数据库加密](SECURITY_WHITEPAPER.md#72-database-encryption) | [专家评审 §4 - 可生产性](reports/digital-key-expert-review.md)

---

## 6. 加密算法兼容性

| 算法 | 用途 | 密钥长度 | 标准合规 | 车端 | Android | iOS | 云端 | 状态 |
|------|------|---------|---------|------|---------|-----|------|------|
| **AES-256-GCM** | 数据加密（对称） | 256 bit | FIPS 140-3 Level 2 | ✅ SE050 | ✅ AndroidKeyStore | ✅ Keychain | ✅ KMS | ✅ 全线支持 |
| **ECDSA P-256** | 数字签名 | 256 bit | FIPS 186-4 | ✅ SE050 | ✅ StrongBox | ✅ Secure Enclave | ✅ HSM | ✅ 全线支持 |
| **ECDH P-256** | 密钥协商 | 256 bit | NIST SP 800-56A | ✅ SE050 | ✅ Secure Enclave | ✅ Secure Enclave | ✅ HSM | ✅ 会话密钥前向保密 |
| **HKDF-SHA256** | 密钥派生 | 256 bit (output) | NIST SP 800-108 | ✅ SE050 | ✅ CryptoEngine | ✅ CryptoEngine | ✅ 软件实现 | ✅ 密钥层级派生统一 |
| **SHA-256** | 哈希 | 256 bit | FIPS 180-4 | ✅ SE050 + TFM | ✅ 平台 API | ✅ CryptoKit | ✅ 软件实现 | ✅ |
| **SM2** | 国密签名（ICCE） | 256 bit | GMT 0003 | ⚠️ 软件回退路径存在（`#ifdef USE_SM_CRYPTO`），`sec_verify()` 中 SM2 待完成 | N/A | N/A | N/A | ⚠️ ICCE 国密路径待补全 |
| **SM3** | 国密哈希（ICCE） | 256 bit | GMT 0004 | ⚠️ 软件回退路径存在 | N/A | N/A | N/A | ⚠️ 同 SM2，随 ICCE 一起补全 |
| **AES-CCM** | BLE LE Secure | 128 bit | BLE Core Spec 5.x | ✅ KW47A | ✅ 平台 BLE | ✅ CoreBluetooth | N/A | ✅ |

### 密钥层级兼容性

| 密钥层级 | 生成方式 | 存储位置 | 轮换周期 | 车端 | 移动端 | 云端 |
|---------|---------|---------|---------|------|--------|------|
| **Root Key (RK)** | SE050 制造注入 | SE050（硬件防篡改） | 硬件生命周期 | ✅ | N/A | N/A |
| **Master Key (MK)** | HKDF-SHA256(RK) | SE050（RK 加密包裹） | 12 个月 | ✅ | N/A | N/A |
| **Device Key (DK)** | HKDF-SHA256(MK + device_id) | SE050 / Keychain / KeyStore | 设备解绑时 | ✅ | ✅ | ✅ hash 仅存于 DB |
| **Session Key (SSK)** | ECDHE + HKDF | RAM（临时） | 单次会话 | ✅ | ✅ | N/A |
| **Shared Key (SK)** | HKDF-SHA256(MK + params) | SE050 / Keychain / KeyStore | 按分享配置 | ✅ | ✅ | ✅ |

> 来源：[安全白皮书 §3.1 密钥层级结构](SECURITY_WHITEPAPER.md#31-key-hierarchy-structure)、[安全白皮书 §3.2 密钥派生](SECURITY_WHITEPAPER.md#32-key-derivation) | [架构设计 §5.1 密钥层级](SYSTEM_ARCHITECTURE.md#51-密钥层级)

---

## 7. 测试与 CI/CD 兼容性

| CI 工作流 | 平台 | 工具 | 兼容状态 | 备注 |
|----------|------|------|---------|------|
| Go Build + Test | Go 1.21+ | Go testing + Testify | ✅ | 覆盖率门控 ≥60%，PR Comment 自动发布 |
| Lint | Go 1.21+ | golangci-lint | ✅ | 强制 CI 门禁 |
| Android CI | Android 12+ | Gradle + Espresso | ✅ | SDK + App 构建 |
| iOS CI | iOS 15.0+ | Xcode + XCTest | ✅ | SDK + App 构建 |
| Embedded C CI | Ubuntu gcc | Unity + Makefile | ✅ | 集成到主 CI |
| MISRA-C:2012 | C | cppcheck | ✅ | 静态分析门禁 |
| Fault Injection CI | C | 故障注入框架 | ✅ | 嵌入式故障模拟 |
| Java Adapter CI | Java 17+ | Spring Boot Maven | ✅ | 构建验证 |
| **交叉编译 CI** | **ARM-eabi** | **GCC ARM Toolchain** | ⛔ **缺失** | 生产部署前需补充 |
| E2E Integration | Docker + K8s | 自定义脚本 | ⚠️ 基础流程覆盖 | 6 场景（Discovery/Binding/PE/RC/NFC/OTA） |
| Cross-platform E2E | 手机↔车端↔云端 | Appium/Detox | ⛔ **缺失** | 专家建议 P2 实施 |
| 安全测试 | 多端 | OWASP ZAP + Burp + Fuzz | ⚠️ 基础覆盖 | Fuzz 1 个 (BERTLV 解码器) |

> 来源：[专家评审 §4 - 可生产性](reports/digital-key-expert-review.md) | [安全白皮书 §10.2 安全测试节奏](SECURITY_WHITEPAPER.md#102-security-testing-cadence)

---

## 8. 已知兼容性风险与处理计划

| 序号 | 风险 | 影响端 | 严重等级 | 计划修复版本 | 状态 |
|------|------|--------|---------|-------------|------|
| 1 | SE050 SCP03 硬件通道路由为 stub，真实安全通道未实现 | 车端 | 🔴 P0 | v1.1.0 | ⏳ 规划中（1 人月） |
| 2 | ICCE 边缘计算引擎 `icce_edge_*` 核心逻辑为 stub | 车端 | 🔴 P0 | v1.1.0 | ⏳ 规划中（2 周） |
| 3 | Android EncryptedSharedPrefs 存储钥匙元数据，与 KeyStore 安全边界断裂 | Android | 🔴 P0 | v1.1.0 | ⏳ 规划中（1 周） |
| 4 | ICCE SM2 验签 `sec_verify()` 待完成 | 车端 | 🟠 P1 | v1.1.0 | ⏳ 待实现 |
| 5 | ICCE UWB FiRa 测距为 stub | 车端 | 🟠 P1 | v1.1.0 | ⏳ 待实现 |
| 6 | 双 Hub 目录代码重复（`backend/hub/` 和 `backend/cloud/hub/`） | 云端 | 🟠 P1 | v1.1.0 | ⏳ 规划中（1 周） |
| 7 | iOS `URLSession` 未实施 TLS Pinning | iOS | 🟠 P1 | v1.1.0 | ⏳ 规划中（3 天） |
| 8 | ARM-eabi 交叉编译 CI 缺失 | 车端 | 🟠 P1 | v1.1.0 | ⏳ 规划中（2 天） |
| 9 | ICCOA DK 4.0 HMAC 算法需对齐 | 车端 + 云端 | 🟠 P1 | v1.1.0 | ⏳ 规划中（2 天） |
| 10 | Android `transactionCounter` 未跨进程持久化，重启后可重置 | Android | 🟡 P2 | v1.2.0 | ⏳ 长期优化 |
| 11 | Android BLE `WRITE_TYPE_DEFAULT` 不支持动态协商 | Android | 🟡 P2 | v1.2.0 | ⏳ 长期优化 |
| 12 | iOS 15.0 兼容性确认（NearbyInteraction UWB 最低 iOS 16.0） | iOS | 🟡 P2 | 文档澄清 | ⏳ 已记录 |
| 13 | TSP 适配器需真机对接验证 | 云端 | 🟡 P2 | v1.2.0 | ⏳ 计划与 1–2 家 OEM POC 联调 |
| 14 | 无灰度发布策略（staging + 金丝雀） | 云端 | 🟡 P2 | v1.2.0 | ⏳ 规划中 |
| 15 | Java 适配器无单元测试 | 云端 | 🟡 P2 | v1.2.0 | ⏳ 需补充 mock 测试 |

### 风险等级定义

| 等级 | 定义 | 响应时限 | 示例 |
|------|------|---------|------|
| 🔴 **P0 Critical** | 投产前必须解决 | ≤ 投产前 | SE050 硬件集成、ICCE 边缘引擎、Android 安全存储 |
| 🟠 **P1 High** | 投产前建议解决 | ≤ 投产前 | TLS Pinning、交叉编译 CI、DK40 HMAC 对齐 |
| 🟡 **P2 Medium** | 投产后 3 个月内 | ≤ 3 个月 | 灰度发布、TSP 真机联调、跨平台 E2E |

> 来源：[专家评审 - 最终建议](reports/digital-key-expert-review.md#-最终建议)

---

## 9. 行业对标兼容性

| 对比项 | yuleDKCS | 行业典型 OEM 方案 |
|--------|---------|-----------------|
| 协议覆盖广度 | ICCE + CCC 3.0 + ICCOA DK 3.0/4.0 | 通常仅 1–2 协议 |
| 移动端覆盖 | Android + iOS 双平台 SDK | 通常仅 1 平台或 SDK 形式 |
| 车端硬件兼容 | NXP S32K312 + KW47A + NCJ29D6 + ST25R501 + SE050 | 通常绑定单一芯片方案 |
| 云端部署 | K8s + Helm + Docker（3 种编排） | 多数有 |
| 安全芯片 | SE050 (CC EAL 6+) | 行业标配 SE 或 TEE |
| 加密算法 | AES-256 + ECDSA P-256 + SM2/SM3（ICCE 国密） | 通常仅国际算法 |
| 密钥层级 | 5 级（RK → MK → DK → SSK → SK） | 3–4 级常见 |
| 测试覆盖 | 多层级多框架（Unity/Espresso/XCTest/Go test） | 差异大 |

> 来源：[专家评审 - 行业对标](reports/digital-key-expert-review.md#行业对标)

---

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| 1.0.0 | 2026-07-16 | 初始版本，基于系统架构、安全白皮书、专家评审创建 |

---

*本文档应与 [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md)、[SECURITY_WHITEPAPER.md](SECURITY_WHITEPAPER.md) 及 [专家评审报告](reports/digital-key-expert-review.md) 配套阅读。*
