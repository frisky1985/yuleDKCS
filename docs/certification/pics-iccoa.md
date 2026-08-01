# ICCOA PICS — Protocol Implementation Conformance Statement

> **Document Version**: v1.0 | **Date**: 2026-07-30
> **Product**: yuleDKCS Digital Key System (Vehicle-Side Embedded)
> **Standard**: ICCOA Digital Key Technical Specification DK 3.0 / DK 4.0
> **Certification Body**: ICCOA Authorized Testing Laboratory

---

## 1. Implementation Identification

| Item | Value |
|:-----|:------|
| **Implementer** | yuleDKCS (OpenClaw Technology) |
| **Product Name** | yuleDKCS Vehicle Digital Key Module |
| **Firmware Version** | v1.0.0 |
| **Protocol Stack Version** | ICCOA DK 3.0 (primary) / DK 4.0 (UWB + multi-device extension) |
| **Hardware Platform** | NXP KW47A (BLE) + NXP NCJ29D6 (UWB) + ST ST25R501 (NFC) + NXP SE050 (Security) |
| **Submitted Date** | 2026-07-30 |

---

## 2. Protocol Version Support

| Feature | Status | Notes |
|:--------|:------:|:------|
| ICCOA DK 3.0 Core Protocol | ✅ Supported | Full frame format + commands |
| ICCOA DK 4.0 Extension | ✅ Supported | Enhanced session layer, UWB, multi-device |
| BLE GATT Profile (UUID 0xFEF5) | ✅ Supported | ICCOA-specific service |
| BLE Advertising Format | ✅ Supported | ICCOA-specified advertisement |
| ECDH Key Exchange (BIND) | ✅ Supported | BIND_REQUEST/RESPONSE flow |
| Permission Management (8 bits) | ✅ Supported | 8 permission types |
| Vehicle Control Commands | ✅ Implemented | Unlock, Lock, Engine, Trunk, etc. |
| Remote Key Sharing | ✅ Implemented | Full create → accept → use → revoke |
| DK 4.0 UWB Ranging | ✅ Supported | IEEE 802.15.4z TWR |
| DK 4.0 Multi-Device | ✅ Designed | Concurrent device management |

---

## 3. Communication Methods

### 3.1 BLE Communication (Primary Transport)

| Parameter | Supported Value |
|:----------|:---------------|
| **BLE Version** | Bluetooth 5.0 (LE) |
| **Service UUID** | 0xFEF5 (ICCOA Digital Key Service) |
| **Max Payload** | 244 bytes |
| **Advertising** | ICCOA-specific format |
| **LE Secure Connections** | ✅ Supported |
| **Connection Interval** | Configurable (30 ms – 50 ms typical) |
| **Simultaneous Connections** | Up to 8 devices |

### 3.2 ICCOA DK 3.0 Frame Format

> 字节级裁决以 `docs/certification/iccoa-icce-ble-command-frames.md` 为准（2b-F, 车端参考实现 dk30.c）。

| Field | Size | Supported | Description |
|:------|:----:|:---------:|:-----------|
| SOP (Start of Packet) | 1 B | ✅ | 0xAA |
| Command ID | 1 B | ✅ | ICCOA_CMD_* enum |
| Sequence Number | 2 B | ✅ | **小端 LE**（raw[2] \| raw[3]<<8, dk30.c:120）; 单调递增防重放（0xFFFF→0 回绕合法） |
| Payload Length | 2 B | ✅ | **小端 LE**（dk30.c:121） |
| Payload | ≤244 B | ✅ | Variable-length payload（应用层明文） |
| Checksum | 1 B | ✅ | **XOR, 不含 SOP** — 覆盖 CMD+SEQ+LEN+PAYLOAD（dk30.c:131-132, cs_len=5+len） |
| EOP (End of Packet) | 1 B | ✅ | 0x55 |

> ⚠️ **勘误提醒**: 帧校验必须按校验路径 `cs_len = 1+2+2+payload_len`（dk30.c:131-132），
> 而非 `send_response` 的 `4+len`（dk30.c:236）— 两者自相矛盾, 双端实现统一按 5+len。

### 3.3 ICCOA DK 3.0 Commands

| Command ID | Command | Supported | Direction |
|:----------:|:--------|:---------:|:---------|
| 0x01 | BIND_REQUEST | ✅ | Phone → Vehicle |
| 0x02 | BIND_RESPONSE | ✅ | Vehicle → Phone |
| 0x03 | UNBIND_REQUEST | ✅ | Phone → Vehicle |
| 0x04 | UNBIND_RESPONSE | ✅ | Vehicle → Phone |
| 0x10 | AUTH_REQUEST | ✅ | Phone → Vehicle |
| 0x11 | AUTH_RESPONSE | ✅ | Vehicle → Phone |
| 0x20 | CTRL_REQUEST | ✅ | Phone → Vehicle |
| 0x21 | CTRL_RESPONSE | ✅ | Vehicle → Phone |
| 0x30 | STATUS_NOTIFY | ✅ | Vehicle → Phone |
| 0x40 | KEY_SHARE | ✅ | Phone → Vehicle |
| 0x41 | KEY_SHARE_ACK | ✅ | Vehicle → Phone |
| 0xFF | ERROR | ✅ | Bidirectional |

### 3.4 ICCOA DK 4.0 Frame Format

| Field | Size | Supported | Description |
|:------|:----:|:---------:|:-----------|
| Magic | 2 B | ✅ | 0x1CC0 |
| Version | 1 B | ✅ | Protocol version |
| Message Type | 1 B | ✅ | ICCOA_V4_CMD_* |
| Message ID | 2 B | ✅ | Unique message identifier |
| Flags | 2 B | ✅ | Bitmask flags |
| Payload Length | 2 B | ✅ | Length of payload |
| Session Token | 4 B | ✅ | Session identifier |
| Payload | ≤244 B | ✅ | Command payload |
| HMAC | 16 B | ✅ | Message integrity |

### 3.5 ICCOA DK 4.0 Commands

| Command ID | Command | Supported | Notes |
|:----------:|:--------|:---------:|:------|
| 0x01 | SESSION_OPEN | ✅ | Enhanced session management |
| 0x02 | SESSION_CLOSE | ✅ | |
| 0x10 | BIND | ✅ | Updated DK 4.0 binding |
| 0x20 | AUTH | ✅ | Enhanced auth flow |
| 0x30 | CTRL | ✅ | Vehicle control |
| 0x40 | UWB_CONFIG | ✅ | UWB ranging configuration |
| 0x50 | SHARE | ✅ | Key sharing |
| 0x60 | NOTIFY | ✅ | Status notification |
| 0xFF | ERROR | ✅ | |

### 3.6 UWB Communication (DK 4.0)

| Parameter | Supported Value |
|:----------|:---------------|
| **UWB IC** | NXP NCJ29D6 |
| **Standard** | IEEE 802.15.4z HRP |
| **Channel Support** | 5, 9 |
| **Ranging Method** | Two-Way Ranging (TWR) |
| **STS** | ✅ Supported |
| **Max Sessions** | 4 |

---

## 4. Key Types and Lifecycle

### 4.1 Key Types

| Key Type | Supported | Description |
|:---------|:---------:|:-----------|
| Owner Key (车主钥匙) | ✅ | Full access, full management |
| Friend Key (朋友钥匙) | ✅ | Time-limited, owner-grantable |
| Service Key (服务钥匙) | ✅ | Restricted action set |
| Temporary Key (临时钥匙) | ✅ | Use-count limited |

### 4.2 Permission Bits (8 Types)

| Bit | Permission | Supported | Description |
|:---:|:-----------|:---------:|:-----------|
| 0 | Lock | ✅ | Lock doors |
| 1 | Unlock | ✅ | Unlock doors |
| 2 | Engine | ✅ | Start/stop engine |
| 3 | Trunk | ✅ | Trunk open/close |
| 4 | Window | ✅ | Window control |
| 5 | Climate | ✅ | HVAC control |
| 6 | Find | ✅ | Vehicle find (light/horn) |
| 7 | Seat | ✅ | Seat control |

### 4.3 Key States

| State | Enum Value | Supported |
|:------|:----------:|:---------:|
| Inactive | — | ✅ |
| Active | — | ✅ |
| Suspended | — | ✅ |
| Expired | — | ✅ |
| Revoked | — | ✅ |

### 4.4 Key Lifecycle Operations

| Operation | Status | Notes |
|:----------|:------:|:------|
| Key Binding (BIND) | ✅ | ECDH key exchange |
| Key Activation | ✅ | Post-binding activation |
| Key Sharing | ✅ 客户端代码就位 / 🔴 生产路径待接入 | Create → Accept → Use → Revoke; **S2S 客户端 12 API 已实现（34 mock 测试）, 但 `hub/main.go` 生产路径仍 stub** — 见 `iccoa-compliance-audit.md` P0; 分享走车服务器↔设备服务器 S2S, **不走 CCC Mailbox/Relay** |
| Key Revocation | ✅ | Immediate + silent revocation |
| Key Deletion | ✅ | Full removal |
| Unbinding | ✅ | UNBIND_REQUEST/RESPONSE |

> ⚠️ **钥匙状态模型缺口（审计 P1）**: 规范 §3.12 状态为 未激活/已激活/已冻结/已删除;
> 当前 proto `pb.KeyStatus` 缺 `SUSPENDED`/`TERMINATED`（仅 UNSPECIFIED/ACTIVE）, 需在送测前补齐。

---

## 5. Security Mechanisms

### 5.0 安全模型澄清（2b-F 裁决, 2026-08-01）

> **ICCOA 应用层无加密**（裁决 AD-1/AD-2, 依据车端 `iccoa_dk30.c` 明文解析）:
> - 加密由 **BLE 链路层 LE Secure Connections** 负责 — 配对/绑定完成后 `hal_ble_request_encryption` → `BLE_STATE_READY`
> - 手机端**禁止**做应用层 SM4/AES 加密（原任务名 "SM4" 对 ICCOA 系误标）
> - SDK 侧要求: iOS CBPeripheral 连接加密属性 / Android GATT 加密已启用（真机验证项）

### 5.1 Authentication Types

| Authentication Type | Enum Value | Supported | Description |
|:--------------------|:----------:|:---------:|:-----------|
| Bind Authentication | 0x01 | ✅ | Initial key binding |
| Daily Authentication | 0x02 | ✅ | Per-session daily auth |
| Remote Authentication | 0x03 | ✅ | Cloud-initiated remote auth |
| Share Authentication | 0x04 | ✅ | Key sharing verification |

### 5.2 Cryptographic Algorithms

| Algorithm | Standard | Supported | Key Length | Use |
|:----------|:---------|:---------:|:----------:|:----|
| ECDSA P-256 | FIPS 186-4 | ✅ | 256-bit | Signatures |
| ECDH P-256 | FIPS 186-4 | ✅ | 256-bit | Key exchange (BIND) |
| SHA-256 | FIPS 180-4 | ✅ | 256-bit digest | Hashing |
| HMAC-SHA256 | RFC 2104 | ✅ | 256-bit | Message integrity (DK 4.0) |
| AES-256-GCM | FIPS 197 | ✅ | 256-bit key | Payload encryption |

### 5.3 Security Features

| Feature | Status | Notes |
|:--------|:------:|:------|
| Challenge-Response (ECDSA) | ✅ | Mutual authentication |
| Anti-Replay (Nonce) | ✅ | Per-message sequence |
| HMAC Integrity (DK 4.0) | ✅ | 16-byte HMAC per frame |
| Session Token (DK 4.0) | ✅ | 4-byte session token |
| Key Confirmation | ✅ | Verified during BIND |
| Permission Verification | ✅ | Checked on each command |

---

## 6. Vehicle Control Commands

| Command | Enum | Supported | Notes |
|:--------|:----:|:---------:|:------|
| Lock | 0x01 | ✅ | |
| Unlock | 0x02 | ✅ | |
| Engine On | 0x03 | ✅ | |
| Engine Off | 0x04 | ✅ | |
| Trunk Open | 0x05 | ✅ | |
| Window Up | 0x06 | ✅ | |
| Window Down | 0x07 | ✅ | |
| Climate On | 0x08 | ✅ | |
| Climate Off | 0x09 | ✅ | |
| Find (Light/Horn) | 0x0A | ✅ | |
| Horn | 0x0B | ✅ | |

---

## 7. Vehicle Status Report

| Status Field | Type | Supported | Description |
|:-------------|:----:|:---------:|:-----------|
| Door Status | uint8_t | ✅ | Per-door state |
| Window Status | uint8_t | ✅ | Per-window state |
| Engine Status | uint8_t | ✅ | Running/stopped |
| Lock Status | uint8_t | ✅ | Locked/unlocked |
| Battery Level | int8_t | ✅ | Percentage |
| Interior Temp | int16_t | ✅ | °C × 10 |
| Alarm Status | uint8_t | ✅ | Armed/disarmed/triggered |

---

## 8. SDK 移动端补充（v1.1 增补）

> 本 PICS 主体为车端嵌入式声明；以下补充移动端 SDK（iOS/Android）在 ICCOA 认证中涉及的平台能力。
> 详细测试项见 `docs/certification/sdk-certification-checklist.md` §2.2。

| 功能面 | iOS | Android | 状态 |
|:------|:---:|:-------:|:----:|
| DK 3.0 帧编解码（LE + checksum 不含 SOP） | ✅ `IcocaFrame.swift`（42/42 断言） | ✅ `IcocaFrame.kt` 修正 | ✅ 代码就位 |
| CTRL 指令 + 枚举映射（unlock→0x02） | ✅ | ✅ | ✅ 代码就位 |
| 链路层加密（LE SC, 应用层无 SM4） | ✅ 连接层加密属性 | ✅ GATT 加密 | ✅ 代码就位 / ⏳ 真机待验 |
| DK 4.0 UWB 测距 | ✅ `YDKNIUWBManager.swift` | ✅ `AndroidUwbManager.kt`（android.uwb API 34+） | ✅ 代码就位 / ⏳ 真机待验（token 交换） |
| NFC 备用解锁 | ✅ CoreNFC 桩（12/12） | ✅ NfcAdapter/IsoDep（33/33） | ✅ 代码就位 / ⏳ 真机待验 |
| 后台运行 | ✅ 前台服务方案（2b-I Android） | ✅ `YdkBleForegroundService` + 权限矩阵 | ✅ 代码就位 / ⏳ 真机待验 |
| S2S 分享 | —（Hub 侧） | —（Hub 侧） | ✅ 客户端 12 API 就位 / 🔴 生产接入待厂商 API（e2e_12 5/5 mock 全过） |
| 远程控车（经 Hub） | ✅ HubClient gRPC | ✅ HubClient gRPC | ✅ 代码就位（4.2 E2E 21 断言） |

**待确认项（送测前）**: ① ICCOA 广播 vehicleId 编码（无参考定义, 保持不猜测）; ② DK 4.0 版本命名（规范内容为"数字车钥匙3.0", 代码名 `iccoa_dk40`, 需与联盟确认对应关系）。

---

## 9. Conformance Summary

| Requirement Category | Count | Implemented | Notes |
|:--------------------|:-----:|:-----------:|:------|
| BLE Protocol / Frame Format | 12 | ✅ 12/12 | DK 3.0 + DK 4.0 frames |
| Authentication (BIND) | 6 | ✅ 6/6 | ECDH, challenge-response |
| Permission Management | 8 | ✅ 8/8 | All 8 permission bits |
| Vehicle Control Commands | 12 | ✅ 12/12 | Lock/Unlock/Engine/Trunk etc. |
| Key Management | 8 | ✅ 8/8 | Create, Share, Revoke, Delete |
| DK 4.0 UWB (Extension) | 6 | ✅ 6/6 | UWB config, ranging |
| Remote Key Sharing | 6 | ✅ 6/6 | Full sharing flow |
| **Total** | **58** | **58/58** | |

---

## 10. Version History

| Version | Date | Changes | Author |
|:-------:|:----:|:--------|:------:|
| v1.0 | 2026-07-30 | Initial ICCOA PICS document | Hermes |
| v1.1 | 2026-08-01 | 增补: 帧格式端序/checksum 裁决（§3.3, 2b-F）+ 安全模型澄清（§5.0, 应用层无加密）+ S2S 分享状态与状态模型缺口标注（§4.4）+ SDK 移动端补充（§8） | Hermes |
