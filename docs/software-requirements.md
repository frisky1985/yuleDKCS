# 软件需求规格说明书 (Software Requirements Specification, SRS)

> **项目**: yuleDKCS 数字钥匙系统
> **文档编号**: yuleDKCS-SRS-001
> **版本**: 1.0.0 | **日期**: 2026-08-06 | **状态**: APPROVED (基线)
> **上游**: `specs/requirements-index.md`（9 RS + 31 SWR = 40 条系统级需求）+ 8 个 spec 文件
> **机器可读表**: `specs/requirements-shall-table.md`（供追溯工具解析）
> **过程域**: ASPICE SWE.1 (Software Requirements Analysis)

---

## 1. 引言

### 1.1 目的

本文档定义 yuleDKCS 数字钥匙系统的软件需求。系统覆盖云核心（DKCS/Hub）、协议层（BERTLV/MQTT/REST）、车端嵌入式（ICCE/CCC/ICCOA 三协议栈）与移动端（Android/iOS SDK）五个功能域，实现数字钥匙的注册、配钥、分享、撤销、车辆控制、状态上报与安全防护全生命周期能力。

### 1.2 范围

- 本文档覆盖**软件需求**（SWE.1 过程域产出物），每条需求以唯一标识 `REQ-xxx` 编号，包含 SHALL 语句与属性（域/优先级/状态/ASIL/来源）。
- 系统级需求（RS-xxx）作为需求来源与追溯上游，映射关系见 §4。
- 详细设计、架构与测试证据见 `docs/architecture.md`、`docs/qualification-strategy.md` 及 `docs/aspice/`。

### 1.3 术语与参考

| 术语 | 含义 |
|:-----|:-----|
| DKCS | Digital Key Cloud Service，云密钥编排服务 |
| Hub | Digital Key Hub，授权决策与协议转换中间层 |
| BERTLV | 私有二进制编码协议（Header E1 01 + Body + Trailer E1 FF） |
| TCU | 车端通信单元 |
| ICCE / CCC / ICCOA | 国密数字钥匙 / CCC Digital Key 3.0 / ICCOA DK 3.0-4.0 协议标准 |
| SE/TEE | 安全元件 / 可信执行环境 |

参考: `specs/requirements-index.md`、`docs/design/PRD.md`、`docs/design/API-CONTRACT.md`、`docs/design/DK-HUB-ARCHITECTURE.md`、`docs/design/HUB-DETAILED-DESIGN.md`、`embedded/*/docs/SPEC.md`、`backend/cloud/protocol/*.md`

### 1.4 需求标识与属性约定

- 每条需求唯一标识: `REQ-001` ~ `REQ-040`（按功能域分组连续编号）。
- 每条需求含至少一条 SHALL 语句（编号 `REQ-xxx-S1`…`S8`），表示强制合规约束。
- 属性: **域**（DKCS Core / Hub / Protocol / Embedded / Frontend / System）、**优先级**（P0 必须 / P1 重要 / P2 建议）、**状态**（Approved / Implemented / Proposed）、**ASIL**（ASIL-B / ASIL-B(D) / ASIL-A / QM）、**来源**（追溯至 RS-xxx / SWR-xxx）。

---

## 2. 功能域概览

| 功能域 | 需求范围 | 代码位置 | 条数 |
|:-------|:---------|:---------|:----:|
| A. 系统级 (System) | REQ-001 ~ REQ-009 | 全系统 | 9 |
| B. DKCS Core | REQ-010 ~ REQ-018 | `backend/dkcs/`, `backend/cloud/hub/` | 9 |
| C. Hub | REQ-019 ~ REQ-023 | `backend/cloud/hub/internal/` | 5 |
| D. Protocol | REQ-024 ~ REQ-027 | `backend/cloud/protocol/` | 4 |
| E. Embedded | REQ-028 ~ REQ-035 | `embedded/` | 8 |
| F. Frontend | REQ-036 ~ REQ-040 | `frontend/` | 5 |
| **合计** | | | **40** |

---

## 3. 软件需求

### 3.1 功能域 A — 系统级需求 (REQ-001 ~ REQ-009)

> 来源: `specs/requirements-index.md` §一 (RS-001~RS-009)，由系统需求下放为软件级约束。

#### REQ-001: 用户设备注册 (Device Registration)
- **域**: System | **优先级**: P0 | **状态**: Approved | **ASIL**: QM | **来源**: RS-001 (`docs/spec/spec-multi-device.md`)
- **SHALL**:
  - REQ-001-S1: The system SHALL allow a user to register a device only after successful authentication.
  - REQ-001-S2: The device registration SHALL include device capabilities (BLE/UWB/NFC/SE/OS).
  - REQ-001-S3: The system SHALL assign a unique device_id per device registration.
  - REQ-001-S4: The system SHALL return registered vehicles and existing keys upon registration.

#### REQ-002: 多设备配钥 (Multi-Device Provisioning)
- **域**: System | **优先级**: P0 | **状态**: Approved | **ASIL**: QM | **来源**: RS-002
- **SHALL**:
  - REQ-002-S1: The system SHALL provision a key to a new device based on the device's capabilities.
  - REQ-002-S2: The provisioning SHALL negotiate the optimal protocol between device and vehicle.
  - REQ-002-S3: The system SHALL generate a device-specific key bound to the device's identity.
  - REQ-002-S4: The system SHALL NOT duplicate keys; if the device already has a key, the existing key SHALL be returned.

#### REQ-003: 多设备管理 (Multi-Device Management)
- **域**: System | **优先级**: P0 | **状态**: Approved | **ASIL**: QM | **来源**: RS-003
- **SHALL**:
  - REQ-003-S1: The system SHALL allow a user to list all devices with provisioned keys.
  - REQ-003-S2: The system SHALL allow a user to remotely revoke a device's key(s).
  - REQ-003-S3: The system SHALL notify the device when its key is revoked (via push).
  - REQ-003-S4: The system SHALL support at least 5 devices per user.

#### REQ-004: 性能指标 (Performance)
- **域**: System | **优先级**: P1 | **状态**: Approved | **ASIL**: QM | **来源**: RS-004 (`docs/design/PRD.md` §6.1)
- **SHALL**:
  - REQ-004-S1: The unlock response time SHALL be ≤ 1s from user approach to door unlock.
  - REQ-004-S2: The lock response time SHALL be ≤ 1s from user departure to door lock.
  - REQ-004-S3: The App cold start time SHALL be ≤ 2s.
  - REQ-004-S4: The cloud API response time SHALL be P99 ≤ 200ms.
  - REQ-004-S5: The pairing flow SHALL complete within ≤ 3min.
  - REQ-004-S6: The remote control response time SHALL be ≤ 3s.
  - REQ-004-S7: The system throughput SHALL be ≥ 100,000 TPS.

#### REQ-005: 可用性需求 (Availability)
- **域**: System | **优先级**: P1 | **状态**: Approved | **ASIL**: QM | **来源**: RS-005
- **SHALL**:
  - REQ-005-S1: The service availability SHALL be ≥ 99.9%.
  - REQ-005-S2: The mean time to repair (MTTR) SHALL be ≤ 30min.
  - REQ-005-S3: The recovery time objective (RTO) SHALL be ≤ 4h.
  - REQ-005-S4: The recovery point objective (RPO) SHALL be ≤ 1h.
  - REQ-005-S5: Data backup SHALL occur at least hourly (incremental).

#### REQ-006: 安全性需求 (Security)
- **域**: System | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B (keys) / ASIL-A (user data) | **来源**: RS-006
- **SHALL**:
  - REQ-006-S1: The secure element SHALL be certified at least EAL5+.
  - REQ-006-S2: All communication SHALL use TLS 1.3 with forward secrecy.
  - REQ-006-S3: Identity authentication SHALL support MFA (OAuth 2.0 / OpenID Connect).
  - REQ-006-S4: Key material SHALL be stored in SE/TEE at hardware level.
  - REQ-006-S5: The system SHALL resist relay attacks via UWB Secure Ranging (IEEE 802.15.4z).
  - REQ-006-S6: The system SHALL resist replay attacks via one-time nonce.
  - REQ-006-S7: Sensitive data SHALL be transmitted end-to-end encrypted.
  - REQ-006-S8: Audit logs SHALL be retained ≥ 3 years.

#### REQ-007: 离线能力 (Offline Capability)
- **域**: System | **优先级**: P1 | **状态**: Approved | **ASIL**: QM | **来源**: RS-007
- **SHALL**:
  - REQ-007-S1: NFC tap-to-unlock SHALL NOT depend on phone battery level.
  - REQ-007-S2: Offline keys SHALL remain valid within their validity period.
  - REQ-007-S3: Offline operation records SHALL be auto-synced when network recovers.

#### REQ-008: 协议兼容性 (Protocol Compatibility)
- **域**: System | **优先级**: P0 | **状态**: Approved | **ASIL**: QM | **来源**: RS-008
- **SHALL**:
  - REQ-008-S1: The system SHALL support both ICCE (T/CA 110-2020) and CCC Digital Key 3.0 standards simultaneously.
  - REQ-008-S2: Vehicle firmware SHALL contain both ICCE and CCC stacks and SHALL auto-negotiate at pairing.
  - REQ-008-S3: The cloud SHALL manage both ICCE national-crypto certificates and CCC X.509 certificates.
  - REQ-008-S4: The App SHALL detect protocol type by vehicle VIN and select the corresponding protocol.

#### REQ-009: 用户体验 (User Experience)
- **域**: System | **优先级**: P2 | **状态**: Approved | **ASIL**: QM | **来源**: RS-009
- **SHALL**:
  - REQ-009-S1: When the owner approaches within ≤ 2m, the door SHALL auto-unlock within 1s.
  - REQ-009-S2: When the owner leaves ≥ 5m for over 30s, the door SHALL auto-lock.
  - REQ-009-S3: The unlock flow SHALL complete in background without requiring screen unlock.
  - REQ-009-S4: One vehicle SHALL support at most 5 digital keys.

---

### 3.2 功能域 B — DKCS Core (REQ-010 ~ REQ-018)

> 来源: `specs/requirements-index.md` §二 (SWR-DKC-001~009)，协议细节见 `backend/cloud/protocol/`。

#### REQ-010: 密钥绑定 (KeyBind)
- **域**: DKCS Core | **优先级**: P0 | **状态**: Implemented | **ASIL**: ASIL-B | **来源**: SWR-DKC-001
- **SHALL**:
  - REQ-010-S1: The system SHALL support KeyBind message types (1000/1001) with bidirectional BERTLV encoding.
  - REQ-010-S2: A KeyBind request SHALL contain VehicleId, DeviceId, UserId, Vendor, Protocol, KeyType, AccessLevel and DevicePubkey.
  - REQ-010-S3: The system SHALL support 4 KeyTypes: Owner(01), Friend(02), Service(03), Temporary(04).
  - REQ-010-S4: The system SHALL support a 16-bit AccessLevel bitmask (bit0~bit15).
  - REQ-010-S5: A KeyBind response SHALL contain KeyId, VehiclePubkey, SharedSecret and VehicleCert.

#### REQ-011: 密钥解绑 (KeyUnbind)
- **域**: DKCS Core | **优先级**: P0 | **状态**: Implemented | **ASIL**: ASIL-B | **来源**: SWR-DKC-002
- **SHALL**:
  - REQ-011-S1: The system SHALL support KeyUnbind message types (1002/1003).
  - REQ-011-S2: A KeyUnbind request SHALL contain KeyId and Reason.

#### REQ-012: 密钥撤销 (KeyRevoke)
- **域**: DKCS Core | **优先级**: P0 | **状态**: Implemented | **ASIL**: ASIL-B | **来源**: SWR-DKC-003
- **SHALL**:
  - REQ-012-S1: The system SHALL support KeyRevoke message types (1004/1005).
  - REQ-012-S2: The system SHALL support emergency revocation (Emergency=1).
  - REQ-012-S3: After revocation, the vehicle SHALL immediately reject unlock requests from the revoked device.

#### REQ-013: 密钥列表查询 (KeyList)
- **域**: DKCS Core | **优先级**: P1 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-DKC-004
- **SHALL**:
  - REQ-013-S1: The system SHALL support KeyList message types (1010/1011).
  - REQ-013-S2: KeyList SHALL support pagination (Page, PageSize).

#### REQ-014: 分享创建 (KeyShareCreate)
- **域**: DKCS Core | **优先级**: P0 | **状态**: Implemented | **ASIL**: ASIL-B | **来源**: SWR-DKC-005
- **SHALL**:
  - REQ-014-S1: The system SHALL support ShareCreate message types (2000/2001).
  - REQ-014-S2: ShareCreate SHALL allow setting AccessLevel, ValidFrom, ValidUntil and MaxUses.

#### REQ-015: 车辆控制指令 (VehicleCommand)
- **域**: DKCS Core | **优先级**: P0 | **状态**: Implemented | **ASIL**: ASIL-B | **来源**: SWR-DKC-006
- **SHALL**:
  - REQ-015-S1: The system SHALL support VehicleCommand message types (3000/3001).
  - REQ-015-S2: The system SHALL support 11 actions: Unlock, Lock, EngineStart, EngineStop, TrunkOpen, WindowUp, WindowDown, ClimateOn, ClimateOff, FindVehicle, Horn.
  - REQ-015-S3: The system SHALL support 5 command sources: NFC, BLE, UWB, Remote, Edge.

#### REQ-016: 车辆状态上报 (VehicleStatusReport)
- **域**: DKCS Core | **优先级**: P1 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-DKC-007
- **SHALL**:
  - REQ-016-S1: The system SHALL support VehicleStatusReport message type (3002).
  - REQ-016-S2: The report SHALL include LockStatus, EngineStatus, DoorStatus, BatteryPct, InteriorTemp and GPS.

#### REQ-017: 心跳机制 (Heartbeat)
- **域**: DKCS Core | **优先级**: P1 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-DKC-008
- **SHALL**:
  - REQ-017-S1: The system SHALL support bidirectional Heartbeat message types (9000/9001).
  - REQ-017-S2: Heartbeat SHALL include Status, CpuUsage, MemUsage and ConnCount.

#### REQ-018: 消息签名与完整性 (Message Signature & Integrity)
- **域**: DKCS Core | **优先级**: P0 | **状态**: Implemented | **ASIL**: ASIL-B | **来源**: SWR-DKC-009
- **SHALL**:
  - REQ-018-S1: Every message SHALL contain a Trailer (E1 FF) signature segment.
  - REQ-018-S2: The signature algorithm SHALL be HMAC-SHA256.
  - REQ-018-S3: The signature scope SHALL cover Header + Body.

---

### 3.3 功能域 C — Hub (REQ-019 ~ REQ-023)

> 来源: `specs/requirements-index.md` §二 (SWR-HUB-001~005)。

#### REQ-019: Registry 大小写规范化 (Registry Case Normalization)
- **域**: Hub | **优先级**: P0 | **状态**: Approved | **ASIL**: QM | **来源**: SWR-HUB-001 (`specs/spec-fix-kni.md`)
- **SHALL**:
  - REQ-019-S1: The system SHALL lowercase vendor and protocol strings before registry lookup.
  - REQ-019-S2: Registry.Register() SHALL normalize keys to lowercase.
  - REQ-019-S3: Registry.Get() SHALL normalize keys to lowercase.
  - REQ-019-S4: Normalization SHALL NOT break existing matching behavior for lowercase keys.

#### REQ-020: nil 指针安全检查 (Nil Pointer Safety)
- **域**: Hub | **优先级**: P0 | **状态**: Approved | **ASIL**: QM | **来源**: SWR-HUB-002
- **SHALL**:
  - REQ-020-S1: The system SHALL add nil safety checks before accessing the RemoteControl field.
  - REQ-020-S2: The vendor variable SHALL be normalized via strings.ToLower() at lookup points.

#### REQ-021: Hub 单元测试覆盖 (Hub Unit Test Coverage)
- **域**: Hub | **优先级**: P1 | **状态**: Approved | **ASIL**: QM | **来源**: SWR-HUB-003 (`specs/spec-fix-p0.md`)
- **SHALL**:
  - REQ-021-S1: Unit tests SHALL cover all 7 source files of `backend/cloud/hub/internal/service/` with ≥ 80% coverage.
  - REQ-021-S2: Unit tests SHALL cover `backend/cloud/hub/internal/logger/` with ≥ 85% coverage.
  - REQ-021-S3: Tests SHALL use the Go standard testing package and SHALL NOT modify production logic.

#### REQ-022: CI 覆盖率门禁 (CI Coverage Gate)
- **域**: Hub | **优先级**: P1 | **状态**: Approved | **ASIL**: QM | **来源**: SWR-HUB-004
- **SHALL**:
  - REQ-022-S1: CI SHALL enforce a coverage gate at fail-under=60 for both backend/dkcs and backend/cloud/hub.
  - REQ-022-S2: CI SHALL fail when coverage drops below the gate threshold.

#### REQ-023: CI 分层机制 (CI Layering)
- **域**: Hub | **优先级**: P1 | **状态**: Approved | **ASIL**: QM | **来源**: SWR-HUB-005
- **SHALL**:
  - REQ-023-S1: CI SHALL be structured into 3 layers (L1 unit+coverage+vet, L2 integration+SAST, L3 full/docker build).
  - REQ-023-S2: L1 SHALL be required for merge; L2/L3 SHALL run only after L1 passes.
  - REQ-023-S3: Integration tests SHALL run separately from unit tests and SHALL NOT block unit test results.
  - REQ-023-S4: gosec SHALL run on all Go code and all SAST findings SHALL be reported.

---

### 3.4 功能域 D — Protocol (REQ-024 ~ REQ-027)

> 来源: `specs/requirements-index.md` §二 (SWR-PRO-001~004)，细节见 `backend/cloud/protocol/`。

#### REQ-024: BERTLV 编码规范 (BERTLV Encoding)
- **域**: Protocol | **优先级**: P0 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-PRO-001
- **SHALL**:
  - REQ-024-S1: All HUB↔DKCS messages SHALL use BERTLV encoding.
  - REQ-024-S2: The message envelope SHALL be Header (E1 01) + Body (BERTLV) + Trailer (E1 FF).
  - REQ-024-S3: Length encoding SHALL be: 00-7F single byte, 80-FF length continuation bytes.
  - REQ-024-S4: The message header SHALL contain Version, Timestamp, MessageType, SequenceNo and DeviceId.

#### REQ-025: DKCS↔TCU MQTT 协议 (MQTT Protocol)
- **域**: Protocol | **优先级**: P0 | **状态**: Implemented | **ASIL**: ASIL-B | **来源**: SWR-PRO-002
- **SHALL**:
  - REQ-025-S1: The MQTT topic SHALL follow `digitalkey/{tsp_id}/{vehicle_id}/{resource}/{action}`.
  - REQ-025-S2: QoS 2 SHALL be used for control commands and key binding; QoS 1 for key sync; QoS 0 for heartbeat and status.
  - REQ-025-S3: The channel SHALL use mTLS mutual authentication.
  - REQ-025-S4: MQTT payloads SHALL be BERTLV encoded.

#### REQ-026: App↔HUB 协议 (App-Hub Protocol)
- **域**: Protocol | **优先级**: P0 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-PRO-003
- **SHALL**:
  - REQ-026-S1: The App↔HUB protocol SHALL be HTTPS REST with BERTLV double-layer structure.
  - REQ-026-S2: Requests SHALL authenticate via OAuth2.0 Token.
  - REQ-026-S3: Every request SHALL carry an HMAC-SHA256 signature.
  - REQ-026-S4: The API SHALL support key bind/unbind/revoke/list operations.

#### REQ-027: 错误码体系 (Error Code System)
- **域**: Protocol | **优先级**: P1 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-PRO-004
- **SHALL**:
  - REQ-027-S1: A unified error code system SHALL be used across all protocol layers.
  - REQ-027-S2: Error codes SHALL cover: request errors (1xxx), key errors (2xxx), vehicle errors (3xxx), vendor errors (4xxx).

---

### 3.5 功能域 E — Embedded (REQ-028 ~ REQ-035)

> 来源: `specs/requirements-index.md` §二 (SWR-EMB-001~008)，细节见 `embedded/*/docs/`。

#### REQ-028: NFC 通信层 (NFC Stack, ST25R501)
- **域**: Embedded | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B | **来源**: SWR-EMB-001
- **SHALL**:
  - REQ-028-S1: The NFC layer SHALL support field detection at 13.56MHz.
  - REQ-028-S2: The NFC layer SHALL support ISO 14443-A and NFC-F (FeliCa).
  - REQ-028-S3: The NFC layer SHALL support NDEF parsing.
  - REQ-028-S4: The NFC layer SHALL support NFC-F OOB pairing data exchange.
  - REQ-028-S5: The NFC layer SHALL support ISO/IEC 7816-4 APDU secure commands.

#### REQ-029: BLE 通信层 (BLE Stack, NXP KW47A)
- **域**: Embedded | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B | **来源**: SWR-EMB-002
- **SHALL**:
  - REQ-029-S1: The BLE layer SHALL support BLE 5.0 GATT Server with CCC DK Service UUID 0xFFD1.
  - REQ-029-S2: The BLE layer SHALL support NFC-assisted OOB pairing.
  - REQ-029-S3: The BLE layer SHALL use LE Secure Connections for encrypted data transfer.
  - REQ-029-S4: The BLE layer SHALL expose all CCC Digital Key GATT Profile characteristics.

#### REQ-030: UWB 测距层 (UWB Ranging, NCJ29D6)
- **域**: Embedded | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B(D) | **来源**: SWR-EMB-003
- **SHALL**:
  - REQ-030-S1: The UWB layer SHALL support IEEE 802.15.4z parameter configuration.
  - REQ-030-S2: The UWB layer SHALL support TWR (Two-Way Ranging).
  - REQ-030-S3: The UWB layer SHALL use secure ranging (STS encrypted scrambled timestamp sequence).
  - REQ-030-S4: The UWB layer SHALL classify distance zones: LOCKED/APPROACH/UNLOCK/ENTRY/INSIDE.
  - REQ-030-S5: The UWB layer SHALL support multi-anchor positioning.

#### REQ-031: ICCOA 协议栈 (ICCOA DK 3.0/4.0)
- **域**: Embedded | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B | **来源**: SWR-EMB-004
- **SHALL**:
  - REQ-031-S1: The ICCOA stack SHALL implement the ICCOA BLE broadcast format.
  - REQ-031-S2: The ICCOA stack SHALL define GATT service 0xFEF5.
  - REQ-031-S3: The ICCOA stack SHALL implement DK 3.0 frame format: SOP+CMD+SEQ+LEN+PAYLOAD+CHK+EOP.
  - REQ-031-S4: The ICCOA stack SHALL support DK 4.0 UWB, multi-device and remote sharing.
  - REQ-031-S5: The binding flow SHALL be BIND_REQUEST → BIND_RESPONSE → ECDH → complete.
  - REQ-031-S6: The ICCOA stack SHALL manage 8 permission bits (LOCK/UNLOCK/ENGINE/TRUNK/WINDOW/CLIMATE/FIND/SEAT).

#### REQ-032: ICCE 协议栈 (ICCE Stack)
- **域**: Embedded | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B | **来源**: SWR-EMB-005
- **SHALL**:
  - REQ-032-S1: The ICCE stack SHALL run on the NXP KW47A BLE module.
  - REQ-032-S2: The edge computing unit SHALL make offline decisions.
  - REQ-032-S3: Key caching SHALL follow: public key permanent / permissions 24h / token 8h.
  - REQ-032-S4: Offline decision rules SHALL combine local cache + signature verification + permission check + risk threshold.
  - REQ-032-S5: The ICCE stack SHALL support OOB pairing via NFC/QR code.
  - REQ-032-S6: The ICCE stack SHALL support national crypto algorithms SM2/SM3/SM4.

#### REQ-033: 安全芯片集成 (SE050 Integration)
- **域**: Embedded | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B | **来源**: SWR-EMB-006
- **SHALL**:
  - REQ-033-S1: The SE050 integration SHALL establish SCP03 secure channels.
  - REQ-033-S2: The SE050 integration SHALL generate and verify attestation packages.
  - REQ-033-S3: Vehicle-side verification SHALL cover certificate chain + ECDSA signature + permission validity + firmware hash.

#### REQ-034: 电源管理 (Power Management)
- **域**: Embedded | **优先级**: P1 | **状态**: Approved | **ASIL**: QM | **来源**: SWR-EMB-007
- **SHALL**:
  - REQ-034-S1: The system SHALL implement 5 power states: ACTIVE/IDLE/SLEEP/DEEPSLEEP/POWEROFF.
  - REQ-034-S2: Current SHALL be ACTIVE < 15mA, SLEEP < 100μA, DEEPSLEEP < 10μA.
  - REQ-034-S3: NFC field wake-up SHALL be < 50ms.
  - REQ-034-S4: BLE broadcast-match wake-up SHALL be < 100ms.

#### REQ-035: 安全启动 (Secure Boot)
- **域**: Embedded | **优先级**: P0 | **状态**: Approved | **ASIL**: ASIL-B | **来源**: SWR-EMB-008
- **SHALL**:
  - REQ-035-S1: The system SHALL implement 4-stage secure boot: BL → OS → APP → COMPLETE.
  - REQ-035-S2: Each stage SHALL be signature-verified.
  - REQ-035-S3: The SE050 SHALL participate in the boot chain of trust.

---

### 3.6 功能域 F — Frontend (REQ-036 ~ REQ-040)

> 来源: `specs/requirements-index.md` §二 (SWR-FE-001~005)，细节见 `docs/design/API-CONTRACT.md`。

#### REQ-036: Android SDK
- **域**: Frontend | **优先级**: P1 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-FE-001
- **SHALL**:
  - REQ-036-S1: The Android SDK SHALL require min SDK 26 (Android 8.0).
  - REQ-036-S2: The Android SDK SHALL expose KeyManager, VehicleController, ShareManager, ChannelManager and SecurityModule interfaces.
  - REQ-036-S3: The Android SDK SHALL support BLE/UWB/NFC channel selection.

#### REQ-037: iOS SDK
- **域**: Frontend | **优先级**: P1 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-FE-002
- **SHALL**:
  - REQ-037-S1: The iOS SDK SHALL require min iOS 14.0.
  - REQ-037-S2: The iOS SDK SHALL use CoreNFC, CoreBluetooth and CoreLocation frameworks.
  - REQ-037-S3: The iOS SDK SHALL expose KeyManaging, VehicleControlling, ShareManaging, ChannelManaging and SecurityManaging protocols.

#### REQ-038: RESTful API 规范
- **域**: Frontend | **优先级**: P0 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-FE-003
- **SHALL**:
  - REQ-038-S1: The REST API SHALL use base URL `https://api.digitalkey.example.com/api/v1`.
  - REQ-038-S2: The REST API SHALL authenticate via Bearer Token (JWT) over TLS 1.3.
  - REQ-038-S3: The REST API SHALL use the unified response format: code, message, data, requestId, timestamp.
  - REQ-038-S4: The REST API SHALL support user auth, vehicle management, key management, key sharing, remote control, OTA and audit log APIs.

#### REQ-039: gRPC 内部服务
- **域**: Frontend | **优先级**: P0 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-FE-004
- **SHALL**:
  - REQ-039-S1: The system SHALL provide 4 gRPC microservices: KeyService, VehicleService, KMSService, EventService.
  - REQ-039-S2: KeyService SHALL support CreateKey, GetKey, ListKeys, Suspend/Resume/RevokeKey and Create/Revoke/AcceptShare.
  - REQ-039-S3: VehicleService SHALL support ListVehicles, Bind/UnbindVehicle, SendControlCommand and CheckOtaUpdate.
  - REQ-039-S4: KMSService SHALL support GenerateKeyPair, Sign, Verify, Encrypt and Decrypt.

#### REQ-040: 消息队列 (Message Queue)
- **域**: Frontend | **优先级**: P1 | **状态**: Implemented | **ASIL**: QM | **来源**: SWR-FE-005
- **SHALL**:
  - REQ-040-S1: The system SHALL provide 9 MQ topics: key.lifecycle, key.share, vehicle.status, vehicle.control, vehicle.ota, auth, audit, notification, vehicle.telemetry.
  - REQ-040-S2: Messages SHALL use Schema Registry (Apache Avro).
  - REQ-040-S3: Consumer groups SHALL be isolated per service.

---

## 4. 需求追溯映射（REQ-xxx ↔ 上游 RS-xxx / SWR-xxx）

| REQ | 上游 ID | 来源文件 | 功能域 |
|:----|:--------|:---------|:-------|
| REQ-001 ~ 009 | RS-001 ~ 009 | `specs/requirements-index.md` §一 | System |
| REQ-010 ~ 018 | SWR-DKC-001 ~ 009 | `backend/cloud/protocol/hub-dkcs-protocol.md` 等 | DKCS Core |
| REQ-019 ~ 023 | SWR-HUB-001 ~ 005 | `specs/spec-fix-kni.md`, `specs/spec-fix-p0.md` | Hub |
| REQ-024 ~ 027 | SWR-PRO-001 ~ 004 | `backend/cloud/protocol/*.md` | Protocol |
| REQ-028 ~ 035 | SWR-EMB-001 ~ 008 | `embedded/*/docs/SPEC.md` | Embedded |
| REQ-036 ~ 040 | SWR-FE-001 ~ 005 | `docs/design/API-CONTRACT.md` | Frontend |

> 机器可读版本: `specs/requirements-shall-table.md`（每条 REQ-xxx 的 SHALL 语句表，供 `yuleosh traceability matrix` / `yuleosh evidence pack` 解析）。
> 需求 → 单测 → 合格性测试双向追溯: `.osh/evidence/traceability-matrix.md`（由工具链自动生成，见 P0-5）。

## 5. 需求属性汇总

| 状态 | 数量 | 说明 |
|:-----|:----:|:-----|
| Approved | 22 | 已评审批准（系统级 9 + Hub 5 + Embedded 8） |
| Implemented | 18 | 已有实现与协议规格（DKCS 9 + Protocol 4 + Frontend 5） |
| Proposed | 0 | 无 |

| 优先级 | 数量 | 说明 |
|:-------|:----:|:-----|
| P0 (必须) | 25 | 安全/钥匙生命周期/协议合规 |
| P1 (重要) | 14 | 性能/可用性/质量门禁/部分协议与前端 |
| P2 (建议) | 1 | 体验优化（REQ-009） |

---

*— 本文档为 yuleDKCS ASPICE CL2 证据链 SWE.1 主产出物。变更须走 `docs/impact-analysis.md` 记录流程。—*
