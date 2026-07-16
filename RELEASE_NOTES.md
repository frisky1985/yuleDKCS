# 发布说明 — yuleDKCS

> **产品**: 数字钥匙系统 (Digital Key System)
> **代号**: yuleDKCS

---

## v1.0.0 — 初始发布 (草案)

> **发布日期**: TBD（待集成测试完成）
> **状态**: ⚠️ 发布候选 — 集成测试待执行

### 概述

yuleDKCS v1.0.0 是数字钥匙系统的初始正式发布版本，提供完整的三端数字钥匙解决方案——嵌入式车端固件、手机 App SDK、云端服务平台。支持 ICCE、CCC、ICCOA 三大主流数字钥匙协议。

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
| 嵌入式 MCU | - | NXP S32G2/G3, KW47A, NCJ29D6, ST25R501 |
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
