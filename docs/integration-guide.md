# yuleDKCS 三端集成指南

> **版本**: 1.0.0 | **面向**: 第三方集成商（车厂 TSP / 手机厂商 / 出行平台）
> **最后更新**: 2026-07-08 | **覆盖**: 三端联调步骤

---

## 1. 系统概览

yuleDKCS 覆盖三端：嵌入式车端（固件协议栈）、手机端（Android/iOS SDK + App）、云端（Go Hub + DKCS + Java Adapters）。支持 ICCE、CCC、ICCOA 三大协议，通过 BLE/UWB/NFC 三模通信实现数字钥匙全生命周期管理。

### 1.1 三端拓扑

```
┌─────────┐    BLE/UWB/NFC     ┌──────────┐    MQTT/TLS    ┌──────────┐
│ 车端 MCU ├───────────────────┤ 手机 App  ├───────────────┤ 云端 Hub │
│ S32G2/G3 │                    │ Android  │                │ Go 服务  │
│ SE050    │                    │ / iOS    │                │ + DKCS   │
└──────────┘                    └──────────┘               └─────┬────┘
      ┌──────────────────────────────────────────────────────────┘
      ▼
┌──────────┐
│ Java     │
│ Adapters │
│ CCC/ICCE │
│ /ICCOA   │
└──────────┘
```

### 1.2 支持的标准

| 协议 | 标准版本 | 算法 | 对接方式 |
|:----|:--------|:-----|:---------|
| **ICCE** | T/CA 110-2020 | SM2/SM3/SM4（国密） | 车端协议栈 + 云端 ICCE 适配器 |
| **CCC** | DK 3.0 Release 1 | ECDSA P-256 / AES-256-GCM | 车端协议栈 + 云端 CCC 适配器 |
| **ICCOA** | DK 3.0 & 4.0 | ECDSA P-256 | 车端协议栈 + 云端 ICCOA 适配器 |

---

## 2. 集成流程

### 2.1 嵌入式端接入（车厂/Tier-1）

**前置条件**:
- 车辆配备 UWB（NCJ29D6）+ BLE（KW47A）+ NFC（ST25R501）+ SE050 安全芯片
- MCU: NXP S32G2/G3（4×Cortex-A53 + 3×M7）

**步骤**:

```bash
# Step 1: 硬件驱动适配 — 移植 HAL 层至目标 MCU
# Step 2: 协议栈编译
cd embedded/ccc_protocol
mkdir build && cd build && cmake .. && make

# ICCE 协议栈（国密）
cd embedded/icce_protocol
mkdir build && cd build && cmake .. && make

# ICCOA 协议栈
cd embedded/iccoa_protocol
mkdir build && cd build && cmake .. && make

# Step 3: SE050 配置 — 初始化安全芯片，烧录车辆根密钥
# Step 4: CAN 总线对接 — 将解锁/闭锁/启动指令映射至车辆 CAN 信号
# Step 5: OTA 通道对接 — 集成车厂 OTA 平台
# Step 6: 车端测试 — 执行 embedded/test_suite/ 测试用例
```

> 详细参考: [design/EMBEDDED-DEV-GUIDE.md](design/EMBEDDED-DEV-GUIDE.md)

### 2.2 App SDK 集成（手机厂商 / 第三方 App）

**Android SDK**:
```kotlin
// 1. 添加 Gradle 依赖
implementation("com.digitalkey:sdk:1.0.0")

// 2. 初始化 SDK
val config = DigitalKeyConfig.Builder()
    .setApiBaseUrl("https://api.yuledkcs.com/v1")
    .setApiKey("your_api_key")
    .build()
DigitalKeySdk.initialize(applicationContext, config)

// 3. 注册车辆
val vehicle = Vehicle("VIN1234567890", "Tesla Model 3")
DigitalKeySdk.registerVehicle(vehicle)

// 4. 绑定数字钥匙
DigitalKeySdk.bindKey(vehicle, object : KeyBindCallback {
    override fun onSuccess(key: DigitalKey) { /* 钥匙绑定成功 */ }
    override fun onFailure(error: DkError) { /* 处理错误 */ }
})

// 5. 执行车辆控制
DigitalKeySdk.controlVehicle(vehicle, ControlAction.UNLOCK)
```

**iOS SDK**:
```swift
// 1. 初始化 SDK (Info.plist 添加 NFC/BLE 权限描述)
let config = DigitalKeyConfig(
    apiBaseURL: "https://api.yuledkcs.com/v1",
    apiKey: "your_api_key"
)
DigitalKeySDK.initialize(config)

// 2. 注册车辆
let vehicle = Vehicle(vin: "VIN1234567890", model: "Tesla Model 3")
DigitalKeySDK.shared.registerVehicle(vehicle)

// 3. 绑定数字钥匙
DigitalKeySDK.shared.bindKey(for: vehicle) { result in
    switch result {
    case .success(let key):  // 钥匙绑定成功
    case .failure(let error): // 处理错误
    }
}

// 4. 执行车辆控制
DigitalKeySDK.shared.controlVehicle(vehicle, action: .unlock)
```

> 详细参考: [design/APP-DEV-GUIDE.md](design/APP-DEV-GUIDE.md)

### 2.3 云端对接（车厂 TSP / 出行平台）

```bash
# 1. 启动依赖服务
docker compose up -d

# 2. 编译并启动 Hub（API 网关）
cd backend/cloud/hub && go build -o hub ./cmd/hub && ./hub --mode=hub-only

# 3. 编译并启动 DKCS（核心服务）
cd backend/dkcs && go build -o dkcs ./cmd/dkcs && ./dkcs --mode=server-only

# 4. 编译并启动 Java Adapters（协议适配器）
cd backend/adapters && mvn package && java -jar adapter-grpc-server/target/*.jar

# 5. 验证部署
curl -X POST https://api.yuledkcs.com/v1/health
```

> 详细参考: [CLOUD-DEV-GUIDE.md](design/CLOUD-DEV-GUIDE.md)

---

## 3. 三端联调步骤

### 3.1 联调前置检查清单

| 检查项 | 状态 | 说明 |
|:-------|:----:|:-----|
| 车端固件已烧录 | □ | ICCE/CCC/ICCOA 三协议栈编译通过 |
| SE050 已初始化 | □ | 安全芯片配置完成，根密钥已烧录 |
| Docker 依赖已启动 | □ | PG + Redis + Kafka |
| Hub 服务运行中 | □ | `curl localhost:8080/health` 返回 200 |
| DKCS 服务运行中 | □ | gRPC 端口 50051 可连接 |
| Java Adapters 运行中 | □ | gRPC 服务注册到 Hub |
| 手机 App 已安装 | □ | 开发调试版本 (Debug) |
| 手机已授权 | □ | BLE + Location + NFC 权限已开启 |

### 3.2 联调流程

```
Step 1: 车端 OTA 通道验证
  └─ 车端 ↔ 云端 双向通信确认
  └─ 车端上报车辆信息 (VIN, 固件版本, 协议能力)

Step 2: 钥匙预置
  └─ 云端 Hub → DKCS → 车端 SE050 写入根密钥
  └─ 验证: 车端日志显示 "Root key provisioned"

Step 3: 手机绑定
  └─ App → Hub (REST) → DKCS → 车端 (MQTT)
  └─ 手机与车端 BLE 连接握手
  └─ 验证: App 钥匙列表显示已绑定车辆

Step 4: 近场解锁 (BLE + UWB)
  └─ 手机靠近车辆 (< 2m)
  └─ BLE 连接建立 → UWB 安全测距 → 解锁指令
  └─ 验证: 车辆解锁，App 显示状态更新

Step 5: 远程控车 (MQTT)
  └─ App → Hub → DKCS → MQTT → TCU → 车端
  └─ 验证: 远程解锁/闭锁/启动成功

Step 6: NFC 刷卡解锁
  └─ 手机靠近 NFC 读卡器
  └─ 验证: NFC 刷卡解锁成功

Step 7: 钥匙分享
  └─ 主用户 → 分享钥匙 → 接收方 → 绑定
  └─ 验证: 接收方可正常解锁车辆

Step 8: 钥匙吊销
  └─ 主用户 → 吊销钥匙
  └─ 验证: 被吊销方无法再解锁车辆
```

### 3.3 验证工具

```bash
# 1. Hub 健康检查
curl -s http://localhost:8080/health | jq .

# 2. DKCS 健康检查 (gRPC)
grpcurl -plaintext localhost:50051 grpc.health.v1.Health/Check

# 3. Kafka 消息验证
docker compose exec kafka kafka-console-consumer \
  --bootstrap-server localhost:9092 \
  --topic dkcs.key.events \
  --from-beginning

# 4. 数据库查询
docker compose exec postgres psql -U user -d yuledkcs -c "SELECT * FROM keys LIMIT 5;"

# 5. Redis 检查
docker compose exec redis redis-cli KEYS 'key:*'

# 6. 集成测试套件
cd backend/dkcs && go test -tags=integration -v -count=1 ./...
```

---

## 4. 常见问题

### 4.1 BLE 连接不稳定
- 检查手机与车端的 BLE 信号强度（RSSI < -80dBm 时连接不稳定）
- 确认车端 BLE 模块处于广播模式
- 检查手机 BLE 扫描间隔（Android 需前台服务保活）

### 4.2 UWB 测距失败
- 确认手机支持 UWB（iPhone 11+ / Android 12+）
- 确认已授予 UWB 权限（iOS: NearbyInteraction / Android: UWB_RANGING）
- 车端 UWB 模块天线位置需满足覆盖要求

### 4.3 协议协商失败
- 检查车端固件版本与云端适配器版本是否匹配
- 检查 VIN 对应的 OEM 协议配置是否正确
- 查看 Hub 日志中 protocol_negotiation 的详细错误

### 4.4 钥匙绑定超时
- 检查车端网络连接（OTA 通道）
- 检查 SE050 剩余存储空间
- 确认车端时间与云端时间同步（NTP）

---

## 5. 集成测试

```bash
# 完整集成测试
make test-integration

# 仅嵌入式端测试
cd embedded && ctest

# 仅 Android 端测试
cd frontend/android && ./gradlew testDebugUnitTest

# 仅 iOS 端测试
cd frontend/ios && xcodebuild test -scheme DigitalKeySDK -sdk iphonesimulator

# 仅云端测试
cd backend/dkcs && go test -count=1 -cover ./...

# E2E 测试（需要所有服务运行中）
cd tests && docker compose -f test-docker-compose.yml up --abort-on-container-exit
```

---

## 6. 参考文档

| 文档 | 路径 |
|:-----|:-----|
| 系统架构 | `docs/SYSTEM_ARCHITECTURE.md` |
| API 参考 | `docs/API_REFERENCE.md` |
| 部署指南 | `docs/DEPLOYMENT_GUIDE.md` |
| 运维手册 | `docs/operations-manual.md` |
| 安全指南 | `docs/SECURITY_GUIDE.md` |
| 安全白皮书 | `docs/SECURITY_WHITEPAPER.md` |
| 权限模型 | `docs/PERMISSION_MODEL.md` |
| 嵌入式开发指南 | `docs/design/EMBEDDED-DEV-GUIDE.md` |
| App 开发指南 | `docs/design/APP-DEV-GUIDE.md` |
| 云端开发指南 | `docs/design/CLOUD-DEV-GUIDE.md` |
| API 契约 | `docs/design/API-CONTRACT.md` |
| OpenAPI 规范 | `docs/api/openapi.yaml` |
