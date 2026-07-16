# yuleDKCS 集成指南

> ⚠️ **DEPRECATED** — 本文件已废弃，请参阅新版文档。
> **版本**: 1.0.0-rc.1 (DEPRECATED)
> **面向**: 第三方集成商（车厂 TSP / 手机厂商 / 出行平台）
> **最后更新**: 2026-07-07 (标记为 DEPRECATED: 2026-07-08)
> **替代**: `docs/integration-guide.md` (新版集成指南)

---

> **注意**: 以下内容来自旧版集成指南，仅供历史参考。所有新集成工作请参考
> `docs/integration-guide.md` 和 `docs/SYSTEM_ARCHITECTURE.md` 中的最新信息。

---

## 1. 系统概览

yuleDKCS 覆盖三端：嵌入式车端（固件协议栈）、手机端（Android/iOS SDK）、云端（Hub + DKCS + Java Adapters）。支持 ICCE、CCC、ICCOA 三大协议，通过 BLE/UWB/NFC 三模通信实现数字钥匙全生命周期管理。

## 2. 支持的标准

| 协议 | 标准版本 | 算法 | 对接方式 |
|:----|:--------|:-----|:---------|
| **ICCE** | T/CA 110-2020 | SM2/SM3/SM4（国密） | 车端协议栈 + 云端 ICCE 适配器 |
| **CCC** | DK 3.0 Release 1 | ECDSA P-256 / AES-256-GCM | 车端协议栈 + 云端 CCC 适配器 |
| **ICCOA** | DK 3.0 & 4.0 | ECDSA P-256 | 车端协议栈 + 云端 ICCOA 适配器 |

## 3. 集成流程

### 3.1 嵌入式端接入（车厂/Tier-1）

**前置条件**: 车辆配备 UWB (NCJ29D6) + BLE (KW47A) + NFC (ST25R501) + SE050 安全芯片。

**步骤**:
1. **硬件驱动适配**: 移植 HAL 层至目标 MCU
2. **协议栈编译**: `cd embedded/ccc_protocol && mkdir build && cd build && cmake .. && make`
3. **SE050 配置**: 初始化安全芯片，烧录车辆根密钥
4. **CAN 总线对接**: 将解锁/闭锁/启动指令映射至车辆 CAN 信号
5. **OTA 通道对接**: 集成车厂 OTA 平台
6. **车端测试**: 执行 `embedded/test_suite/` 测试用例

> 详细参考: [EMBEDDED-DEV-GUIDE.md](design/EMBEDDED-DEV-GUIDE.md)

### 3.2 App SDK 集成（手机厂商 / 第三方 App）

**Android SDK**:
```kotlin
val client = DigitalKeyClient.Builder()
    .setApiKey("your-api-key")
    .setEnvironment(Environment.PRODUCTION).build()
val key = client.keyManager.createKey(
    vehicleId = "VIN123", keyType = KeyType.OWNER,
    permissions = listOf(Permission.UNLOCK, Permission.LOCK)).await()
```

**iOS SDK**:
```swift
let client = DigitalKeyClient(apiKey: "your-api-key", environment: .production)
let key = try await client.keyManager.createKey(
    vehicleId: "VIN123", keyType: .owner, permissions: [.unlock, .lock])
```

需要声明 BLE、NFC、UWB、定位权限。详见 [APP-DEV-GUIDE.md](design/APP-DEV-GUIDE.md)。

### 3.3 云端对接（车厂 TSP）

**协议**: REST (App↔Hub) + gRPC (Hub↔DKCS) + MQTT (DKCS↔TCU)

**步骤**:
1. 基于 `adapter-core` 扩展车厂自有 TSP 适配器
2. 配置 JWT Secret 和 API Gateway
3. [车厂定制的 PKI/KMS 对接] 车厂 PKI/KMS 对接方式 — 取决于密钥材料归属策略。若密钥由车厂 CA 签发，需对接车厂 CA 服务；若由 yuleDKCS KMS 签发，使用内置密钥管理模块 `backend/cloud/protocol/mobile-sdk-spec.md` 中的证书管理接口
4. 可选接入 Kafka 事件流

> 参考: [API_REFERENCE.md](API_REFERENCE.md)、[CLOUD-DEV-GUIDE.md](design/CLOUD-DEV-GUIDE.md)、[DK-HUB-ARCHITECTURE.md](design/DK-HUB-ARCHITECTURE.md)

## 4. 环境要求

| 组件 | 开发环境 | 生产环境 |
|:----|:---------|:---------|
| Go | 1.22+ | 1.22+（容器化） |
| Java | 17+ | 17+（容器化） |
| PostgreSQL | 15+ | 15+ Primary + Replica |
| Redis | 7+ | 7+ Cluster |
| Kafka | 3.6+ | 3.6+ 3-broker |
| Kubernetes | - | 1.28+ |

## 5. 快速入门

```bash
git clone https://github.com/digitalkey/yuleDKCS.git && cd yuleDKCS
docker-compose up -d postgres redis kafka
psql -h localhost -U digitalkey -d digitalkey_db -f backend/db/schema.sql
go run ./backend/cloud/hub/cmd/yuledkcs --mode=all-in-one --http-addr=:8080 --jwt-secret=dev-secret
curl http://localhost:8080/v1/health
```

## 6. 参考文档

| 文档 | 说明 |
|:----|:-----|
| [系统架构](SYSTEM_ARCHITECTURE.md) | 整体架构、组件关系、数据流 |
| [API 参考](API_REFERENCE.md) | REST/gRPC/MQTT/SDK API 定义 |
| [部署指南](DEPLOYMENT_GUIDE.md) | Docker Compose / K8s 部署 |
| [安全指南](SECURITY_GUIDE.md) | 安全架构与最佳实践 |
| [嵌入式开发指南](design/EMBEDDED-DEV-GUIDE.md) | 车端协议栈开发 |
| [App 开发指南](design/APP-DEV-GUIDE.md) | 手机 SDK 开发 |
| [云端开发指南](design/CLOUD-DEV-GUIDE.md) | 云端服务开发 |
| [API 契约](design/API-CONTRACT.md) | 详细接口契约 |
