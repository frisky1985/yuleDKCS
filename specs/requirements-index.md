# yuleDKCS 完整需求清单

> **版本**: v1.0 | **日期**: 2026-07-18  
> **来源扫描**: `docs/spec/spec-multi-device.md`, `specs/spec-fix-p0.md`, `specs/spec-fix-kni.md`, `docs/design/PRD.md`, `docs/design/API-CONTRACT.md`, `embedded/ccc_protocol/docs/SPEC.md`, `embedded/iccoa_protocol/docs/SPEC.md`, `embedded/icce_protocol/docs/technical_specification.md`, `backend/cloud/protocol/*.md`

---

## 模块分组

| 模块 | 前缀 | 描述 |
|------|------|------|
| DKCS Core | SWR-DKC | 云核心 DKCS 服务 (backend/dkcs, backend/cloud/hub) |
| Hub | SWR-HUB | Hub 中间层 (adapter, service, logger, 协议转换) |
| Protocol | SWR-PRO | 私有协议 BERTLV (HUB↔DKCS, DKCS↔TCU, App↔HUB) |
| Embedded | SWR-EMB | 车端嵌入式系统 (CCC/ICCOA/ICCE 协议栈, BLE/UWB/NFC) |
| Frontend | SWR-FE | App 端 SDK/API (iOS/Android/Cloud REST) |

---

## 一、系统级需求 (System Requirements)

### RS-001: 用户设备注册
- **Module**: DKCS Core
- **Source**: `docs/spec/spec-multi-device.md`
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  1. The system SHALL allow a user to register a device after authentication
  2. The device registration SHALL include device capabilities (BLE/UWB/NFC/SE/OS)
  3. The system SHALL assign a unique device_id per device registration
  4. The system SHALL return registered vehicles and existing keys upon registration

### RS-002: 多设备配钥
- **Module**: DKCS Core
- **Source**: `docs/spec/spec-multi-device.md`
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  5. The system SHALL provision a key to a new device based on the device's capabilities
  6. The provisioning SHALL negotiate the optimal protocol between device and vehicle
  7. The system SHALL generate a device-specific key (bound to this device's identity)
  8. The system SHALL NOT duplicate keys—if device already has a key, return existing

### RS-003: 多设备管理
- **Module**: DKCS Core
- **Source**: `docs/spec/spec-multi-device.md`
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  9. The system SHALL allow a user to list all devices with provisioned keys
  10. The system SHALL allow a user to remotely revoke a device's key(s)
  11. The system SHALL notify the device when its key is revoked (via push)
  12. The system SHALL limit the number of devices per user (5 minimum)

### RS-004: 性能指标
- **Module**: All
- **Source**: `docs/design/PRD.md` §6.1
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  13. 解锁响应时间 ≤ 1s (从用户靠近到车门解锁)
  14. 上锁响应时间 ≤ 1s (从用户离开到车门上锁)
  15. App 冷启动时间 ≤ 2s
  16. 云端 API 响应时间 P99 ≤ 200ms
  17. 配对流程完成时间 ≤ 3min
  18. 远程控制响应时间 ≤ 3s
  19. 系统吞吐量 ≥ 100,000 TPS

### RS-005: 可用性需求
- **Module**: Cloud
- **Source**: `docs/design/PRD.md` §6.2
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  20. 服务可用性 ≥ 99.9%
  21. 故障恢复时间 (MTTR) ≤ 30min
  22. 灾备恢复时间 (RTO) ≤ 4h
  23. 数据丢失容限 (RPO) ≤ 1h
  24. 数据备份频率 ≥ 每小时增量

### RS-006: 安全性需求
- **Module**: All
- **Source**: `docs/design/PRD.md` §6.3, `embedded/system_architecture/SPEC.md` §4
- **Status**: PROPOSED
- **ASIL**: ASIL-B (keys), ASIL-A (user data)
- **SHALL**:
  25. 安全芯片等级 ≥ EAL5+
  26. 通信加密 TLS 1.3 + 前向保密
  27. 身份认证 MFA 多因素认证 (OAuth 2.0 / OpenID Connect)
  28. 密钥存储在 SE/TEE 硬件级
  29. 防中继攻击 via UWB Secure Ranging (IEEE 802.15.4z)
  30. 防重放攻击 via 一次性随机数 (Nonce)
  31. 端到端加密，敏感数据密文传输
  32. 审计日志保留 ≥ 3 年

### RS-007: 离线能力
- **Module**: Embedded + Frontend
- **Source**: `docs/design/PRD.md` §3.2.2
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  33. NFC 刷卡解锁不依赖手机电量 (手机没电时可用)
  34. 离线钥匙在有效期内持续有效
  35. 离线期间操作记录待网络恢复后自动同步

### RS-008: 协议兼容性
- **Module**: Embedded + Protocol
- **Source**: `docs/design/PRD.md` §5, `embedded/iccoa_protocol/docs/SPEC.md`, `embedded/ccc_protocol/docs/SPEC.md`
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  36. 同时支持 ICCE (T/CA 110-2020) 与 CCC Digital Key 3.0 两大标准
  37. 车端固件同时包含 ICCE 和 CCC 协议栈，配对时自动协商
  38. 双证书支持：云端同时管理 ICCE 国密证书和 CCC X.509 证书
  39. App 根据车辆 VIN 识别协议类型，自动选用对应协议

### RS-009: 用户体验
- **Module**: Frontend + Embedded
- **Source**: `docs/design/PRD.md` §4.2
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  40. 车主携带手机走近车辆 (≤2m)，车门在 1 秒内自动解锁
  41. 车主离开车辆 (≥5m) 超过 30 秒，车门自动上锁
  42. 解锁过程手机无需解锁屏幕 (后台静默完成)
  43. 同一车辆最多支持 5 把数字钥匙

---

## 二、软件级需求 (Software Requirements)

### DKCS Core 模块

#### Module: DKCS Core (backend/dkcs, backend/cloud/hub)

#### SWR-DKC-001: 密钥绑定 (KeyBind)
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.1
- **Source**: `backend/cloud/protocol/dkcs-tcu-protocol.md` §3.2
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: ASIL-B
- **SHALL**:
  - 支持 KeyBind (1000/1001) 消息类型，双向 BERTLV 编码
  - KeyBind 请求必须包含 VehicleId, DeviceId, UserId, Vendor, Protocol, KeyType, AccessLevel, DevicePubkey
  - 支持 4 种 KeyType: Owner(01), Friend(02), Service(03), Temporary(04)
  - 支持 16 位 AccessLevel 位掩码 (bit0~bit15)
  - 响应包含 KeyId, VehiclePubkey, SharedSecret, VehicleCert

#### SWR-DKC-002: 密钥解绑 (KeyUnbind)
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.2
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: ASIL-B
- **SHALL**:
  - 支持 KeyUnbind (1002/1003) 消息类型
  - 请求包含 KeyId + Reason

#### SWR-DKC-003: 密钥撤销 (KeyRevoke)
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.3
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: ASIL-B
- **SHALL**:
  - 支持 KeyRevoke (1004/1005) 消息类型
  - 支持紧急撤销 (Emergency=1) 模式
  - 撤销后车端即时拒绝该设备的解锁请求 (来自 spec-multi-device)

#### SWR-DKC-004: 密钥列表查询 (KeyList)
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.4
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: QM
- **SHALL**:
  - 支持 KeyList (1010/1011) 消息类型
  - 支持分页查询 (Page, PageSize)

#### SWR-DKC-005: 分享创建 (KeyShareCreate)
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.5
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: ASIL-B
- **SHALL**:
  - 支持 ShareCreate (2000/2001) 消息类型
  - 可设置 AccessLevel, ValidFrom, ValidUntil, MaxUses

#### SWR-DKC-006: 车辆控制指令 (VehicleCommand)
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.6
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: ASIL-B
- **SHALL**:
  - 支持 VehicleCommand (3000/3001) 消息类型
  - 支持 11 种 Action: Unlock, Lock, EngineStart, EngineStop, TrunkOpen, WindowUp, WindowDown, ClimateOn, ClimateOff, FindVehicle, Horn
  - 支持 5 种 Source: NFC, BLE, UWB, Remote, Edge

#### SWR-DKC-007: 车辆状态上报 (VehicleStatusReport)
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.7
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: QM
- **SHALL**:
  - 支持 VehicleStatusReport (3002) 消息类型
  - 上报内容含 LockStatus, EngineStatus, DoorStatus, BatteryPct, InteriorTemp, GPS 等

#### SWR-DKC-008: 心跳机制
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §3.8
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: QM
- **SHALL**:
  - 支持 Heartbeat (9000/9001) 双向消息类型
  - 心跳包含 Status, CpuUsage, MemUsage, ConnCount

#### SWR-DKC-009: 消息签名与完整性
- **Source**: `backend/cloud/protocol/hub-dkcs-protocol.md` §4
- **Status**: IMPLEMENTED (protocol spec)
- **ASIL**: ASIL-B
- **SHALL**:
  - 每个消息必须包含 Trailer (E1 FF) 签名段
  - 签名算法: HMAC-SHA256
  - 签名范围: Header + Body

### Hub 模块

#### SWR-HUB-001: Registry 大小写规范化
- **Source**: `specs/spec-fix-kni.md`
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  - Lowercase vendor and protocol strings before registry lookup
  - Normalize keys to lowercase in Registry.Register()
  - Normalize keys to lowercase in Registry.Get()
  - NOT break existing matching behavior for lowercase keys

#### SWR-HUB-002: nil 指针安全检查
- **Source**: `specs/spec-fix-kni.md`
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  - Add nil safety check before accessing RemoteControl field
  - Actually call strings.ToLower() on the vendor variable
  - Normalize vendor/protocol to lowercase at lookup points

#### SWR-HUB-003: 单元测试覆盖
- **Source**: `specs/spec-fix-p0.md` (FIX-001, FIX-002)
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  - Add unit tests for `backend/cloud/hub/internal/service/` covering all 7 source files
  - Reach at least 80% test coverage for the service package
  - Add unit tests for `backend/cloud/hub/internal/logger/`
  - Reach at least 85% test coverage for the logger package
  - Use Go standard testing package
  - NOT modify production code logic

#### SWR-HUB-004: CI 覆盖率门禁
- **Source**: `specs/spec-fix-p0.md` (FIX-003)
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  - Enforce a coverage gate at fail-under=60 in CI
  - Fail the CI run when coverage drops below 60%
  - Apply the gate to both backend/dkcs and backend/cloud/hub
  - Implement the gate via go test -coverprofile plus custom shell check

#### SWR-HUB-005: CI 分层机制
- **Source**: `specs/spec-fix-p0.md` (FIX-004, FIX-005, FIX-006)
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  - Restructure CI into 3 layers (L1/L2/L3)
  - Include unit tests, coverage gate, and go vet in L1
  - Include integration tests and SAST scan in L2
  - Include full build and docker build in L3
  - Require L1 for merge
  - Run L2 and L3 only after L1 passes
  - Run integration tests separately from unit tests
  - NOT block unit test results on integration test outcome
  - Run gosec on all Go code in CI
  - Report all SAST findings in CI output

### Protocol 模块

#### SWR-PRO-001: BERTLV 编码规范
- **Source**: `backend/cloud/protocol/encoding-rules.md`, `backend/cloud/protocol/hub-dkcs-protocol.md` §1.2
- **Status**: IMPLEMENTED
- **ASIL**: QM
- **SHALL**:
  - 所有 HUB↔DKCS 消息使用 BERTLV 编码
  - 消息结构: Header (E1 01) + Body (BERTLV) + Trailer (E1 FF)
  - Length 编码规则: 00-7F 单字节, 80-FF 后续字节表示长度
  - 消息头部必须包含 Version, Timestamp, MessageType, SequenceNo, DeviceId

#### SWR-PRO-002: DKCS↔TCU MQTT 协议
- **Source**: `backend/cloud/protocol/dkcs-tcu-protocol.md`
- **Status**: IMPLEMENTED
- **ASIL**: ASIL-B
- **SHALL**:
  - Topic 格式: `digitalkey/{tsp_id}/{vehicle_id}/{resource}/{action}`
  - QoS 2 (Exactly-once) 用于控制指令和密钥绑定
  - QoS 1 (At-least-once) 用于密钥同步
  - QoS 0 (At-most-once) 用于心跳和状态上报
  - mTLS 双向认证
  - MQTT payload 使用 BERTLV 编码

#### SWR-PRO-003: App↔HUB 协议
- **Source**: `backend/cloud/protocol/app-hub-protocol.md`
- **Status**: IMPLEMENTED
- **ASIL**: QM
- **SHALL**:
  - HTTPS REST + BERTLV 双层结构
  - OAuth2.0 Token 认证
  - 每个请求携带签名 (HMAC-SHA256)
  - 支持密钥绑定/解绑/撤销/列表 API

#### SWR-PRO-004: 错误码体系
- **Source**: `backend/cloud/protocol/error-codes.md`, `backend/cloud/protocol/hub-dkcs-protocol.md` §5
- **Status**: IMPLEMENTED
- **ASIL**: QM
- **SHALL**:
  - 统一错误码体系跨所有协议层
  - 覆盖: 请求错误 (1xxx), 密钥错误 (2xxx), 车辆错误 (3xxx), 厂商错误 (4xxx)

### Embedded 模块

#### SWR-EMB-001: NFC 通信层 (ST25R501)
- **Source**: `embedded/ccc_protocol/docs/SPEC.md` §2
- **Status**: PROPOSED (spec completed)
- **ASIL**: ASIL-B
- **SHALL**:
  - NFC 场检测 (13.56MHz)
  - 支持 ISO 14443-A, NFC-F (FeliCa)
  - NDEF 解析
  - 触碰配对 (NFC-F OOB 配对数据交换)
  - ISO/IEC 7816-4 APDU 安全指令

#### SWR-EMB-002: BLE 通信层 (NXP KW47A)
- **Source**: `embedded/ccc_protocol/docs/SPEC.md` §3
- **Status**: PROPOSED (spec completed)
- **ASIL**: ASIL-B
- **SHALL**:
  - BLE 5.0, GATT Server (CCC DK Service UUID 0xFFD1)
  - OOB 配对 (NFC 辅助)
  - LE Secure Connections
  - 加密数据传输
  - 支持 CCC Digital Key GATT Profile 全部 characteristics

#### SWR-EMB-003: UWB 测距层 (NXP NCJ29D6)
- **Source**: `embedded/ccc_protocol/docs/SPEC.md` §4
- **Status**: PROPOSED (spec completed)
- **ASIL**: ASIL-B
- **SHALL**:
  - IEEE 802.15.4z 参数配置
  - TWR 双向测距 (Two-Way Ranging)
  - 安全测距 (STS 加密, Scrambled Timestamp Sequence)
  - 距离区域划分: LOCKED/APPROACH/UNLOCK/ENTRY/INSIDE
  - 多锚点定位

#### SWR-EMB-004: ICCOA DK 3.0/4.0 协议栈
- **Source**: `embedded/iccoa_protocol/docs/SPEC.md`
- **Status**: PROPOSED (spec completed)
- **ASIL**: ASIL-B
- **SHALL**:
  - ICCOA BLE 广播格式
  - GATT 服务定义 (0xFEF5)
  - DK 3.0 帧格式: SOP+CMD+SEQ+LEN+PAYLOAD+CHK+EOP
  - DK 4.0 UWB 支持, 多设备, 远程分享
  - 绑定流程: BIND_REQUEST → BIND_RESPONSE → ECDH → 完成
  - 权限管理: 8 种权限位 (LOCK/UNLOCK/ENGINE/TRUNK/WINDOW/CLIMATE/FIND/SEAT)

#### SWR-EMB-005: ICCE 协议栈
- **Source**: `embedded/icce_protocol/docs/technical_specification.md`
- **Status**: PROPOSED (spec completed)
- **ASIL**: ASIL-B
- **SHALL**:
  - 基于 NXP KW47A BLE 模块
  - 边缘计算单元进行离线决策
  - 钥匙缓存策略 (公钥永久/权限24h/令牌8h)
  - 离线决策规则: 本地缓存 + 签名验证 + 权限检查 + 风险阈值
  - 支持 OOB 配对 (NFC/QR 码传递公钥)
  - 国密算法 SM2/SM3/SM4 支持 ICCE 协议

#### SWR-EMB-006: 安全芯片 (SE050) 集成
- **Source**: `embedded/ccc_protocol/docs/SPEC.md` §6, `embedded/system_architecture/SPEC.md` §4
- **Status**: PROPOSED (spec completed)
- **ASIL**: ASIL-B
- **SHALL**:
  - SCP03 安全通道建立
  - Attestation 证明包生成与验证
  - 车端验签: 证书链验证 + ECDSA 签名验证 + 权限有效期检查 + 固件哈希检查

#### SWR-EMB-007: 电源管理
- **Source**: `embedded/system_architecture/SPEC.md` §5
- **Status**: PROPOSED
- **ASIL**: QM
- **SHALL**:
  - 5 级电源状态: ACTIVE/IDLE/SLEEP/DEEPSLEEP/POWEROFF
  - ACTIVE < 15mA, SLEEP < 100μA, DEEPSLEEP < 10μA
  - NFC 场唤醒 < 50ms
  - BLE 广播匹配唤醒 < 100ms

#### SWR-EMB-008: 安全启动
- **Source**: `embedded/system_architecture/SPEC.md` §4.4
- **Status**: PROPOSED
- **ASIL**: ASIL-B
- **SHALL**:
  - 4 阶段安全启动: BL → OS → APP → COMPLETE
  - 每阶段签名验证
  - SE050 参与启动链信任

### Frontend 模块

#### SWR-FE-001: Android SDK
- **Source**: `backend/cloud/protocol/mobile-sdk-spec.md` §2
- **Status**: IMPLEMENTED (API spec)
- **ASIL**: QM
- **SHALL**:
  - Min SDK 26 (Android 8.0)
  - 权限: NFC, BLE, Location, Camera
  - 提供 KeyManager, VehicleController, ShareManager, ChannelManager, SecurityModule 接口
  - BLE/UWB/NFC 通道选择

#### SWR-FE-002: iOS SDK
- **Source**: `backend/cloud/protocol/mobile-sdk-spec.md` §3
- **Status**: IMPLEMENTED (API spec)
- **ASIL**: QM
- **SHALL**:
  - Min iOS 14.0
  - Frameworks: CoreNFC, CoreBluetooth, CoreLocation
  - 提供 KeyManaging, VehicleControlling, ShareManaging, ChannelManaging, SecurityManaging 协议

#### SWR-FE-003: RESTful API 规范
- **Source**: `docs/design/API-CONTRACT.md` §3
- **Status**: IMPLEMENTED (API spec)
- **ASIL**: QM
- **SHALL**:
  - 基础 URL: `https://api.digitalkey.example.com/api/v1`
  - 认证: Bearer Token (JWT)
  - TLS 1.3
  - 统一响应格式: code, message, data, requestId, timestamp
  - 支持: 用户认证, 车辆管理, 钥匙管理, 钥匙分享, 远程控车, OTA 升级, 审计日志 API

#### SWR-FE-004: gRPC 内部服务
- **Source**: `docs/design/API-CONTRACT.md` §4
- **Status**: IMPLEMENTED (proto spec)
- **ASIL**: QM
- **SHALL**:
  - 4 组 gRPC 微服务: KeyService, VehicleService, KMSService, EventService
  - Protobuf 定义在 `digitalkey/` 包下
  - KeyService: CreateKey, GetKey, ListKeys, Suspend/Resume/RevokeKey, Create/Revoke/AcceptShare
  - VehicleService: ListVehicles, Bind/UnbindVehicle, SendControlCommand, CheckOtaUpdate
  - KMSService: GenerateKeyPair, Sign, Verify, Encrypt, Decrypt

#### SWR-FE-005: 消息队列
- **Source**: `docs/design/API-CONTRACT.md` §5
- **Status**: IMPLEMENTED (topic spec)
- **ASIL**: QM
- **SHALL**:
  - 9 个 MQ Topic: key.lifecycle, key.share, vehicle.status, vehicle.control, vehicle.ota, auth, audit, notification, vehicle.telemetry
  - Schema Registry (Apache Avro)
  - Consumer Group 隔离

---

## 三、需求统计

| 类别 | 数量 | IMPLEMENTED | PROPOSED |
|------|------|-------------|----------|
| 系统级 (RS) | 9 | 0 | 9 |
| DKCS Core (SWR-DKC) | 9 | 9 | 0 |
| Hub (SWR-HUB) | 5 | 0 | 5 |
| Protocol (SWR-PRO) | 4 | 4 | 0 |
| Embedded (SWR-EMB) | 8 | 0 | 8 |
| Frontend (SWR-FE) | 5 | 5 | 0 |
| **总计** | **40** | **18** | **22** |
