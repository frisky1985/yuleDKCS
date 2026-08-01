# CCC PICS — Protocol Implementation Conformance Statement

> **Document Version**: v1.0 | **Date**: 2026-07-30
> **Product**: yuleDKCS Digital Key System (Vehicle-Side Embedded)
> **Standard**: CCC Digital Key Release 3.0 (including R3 maintenance updates)
> **Certification Body**: UL LLC / TÜV Rheinland / Bureau Veritas

---

## 1. Implementation Identification

| Item | Value |
|:-----|:------|
| **Implementer** | yuleDKCS (OpenClaw Technology) |
| **Product Name** | yuleDKCS Vehicle Digital Key Module |
| **Firmware Version** | v1.0.0 |
| **Protocol Stack Version** | CCC Digital Key 3.0 |
| **Hardware Platform** | NXP KW47A (BLE) + NXP NCJ29D6 (UWB) + ST ST25R501 (NFC) + NXP SE050 (Security) |
| **Submitted Date** | 2026-07-30 |

---

## 2. Protocol Version Support

| Feature | Status | Notes |
|:--------|:------:|:------|
| CCC Digital Key 3.0 Core Specification | ✅ Supported | Full implementation |
| CCC-TS-101 **v4.0.0** BLE Secure Channel | ✅ Supported | 2b-E 按 v4.0.0 实现（HKDF SystemKeys + SCP03 风格加密）, 见 `ccc-ts101-ble-secure-channel.md` |
| CCC GATT Profile (UUID 0xFFD1) | ⚠️ R3.0 遗留 | 16 GATT characteristics; **v4.0.0 生产路径改用 0xFFF5 + SPSM/DK Version 特征（L2CAP 传输）** |
| NFC ISO 14443-4 Activation | ✅ Supported | ST25R501 driver |
| NFC-F (FeliCa) Support | ✅ Supported | NDEF parsing |
| ISO/IEC 7816-4 APDU Commands | ✅ Supported | Secure APDU |
| UWB IEEE 802.15.4z | ✅ Supported | TWR + STS |
| SE050 SCP03 Secure Channel | ✅ Supported | |
| ECDSA P-256 Sign/Verify | ✅ Implemented | HMAC-SHA256 + ECDSA |
| Passive Entry (UWB) | ✅ Supported | 5 distance zones |
| Remote Key Provisioning | ✅ Supported | Cloud-initiated |
| Key Sharing | ✅ Implemented | Friend, Temporary, Valet |
| Offline Operation | ✅ Designed | Edge computing unit |

---

## 3. Communication Methods

### 3.1 NFC Communication

| Parameter | Supported Value |
|:----------|:---------------|
| **NFC Protocol** | ISO/IEC 14443-4 Type A/B |
| **FeliCa (NFC-F)** | ✅ Supported (Type F) |
| **NFC Reader IC** | ST ST25R501 |
| **LPCD (Low Power Card Detection)** | ✅ Supported |
| **ISO/IEC 7816-4 APDU** | ✅ Supported |
| **NDEF Message Parsing** | ✅ Supported |
| **OOB Data Structure** | `ccc_nfc_oob_data_t` (BLE MAC, UWB session ID, channel, preamble code, capability, OOB data) |
| **Card Emulation Mode** | ✅ Supported |
| **Data Rates** | 106 / 212 / 424 kbps |
| **Max APDU Payload** | 512 bytes |
| **Communication Distance** | 0 – 4 cm |

### 3.2 BLE Communication

| Parameter | Supported Value |
|:----------|:---------------|
| **BLE Version** | Bluetooth 5.0 (LE) |
| **Service UUID** | 0xFFD1 (CCC Digital Key Service — SIG Assigned) |
| **Number of Characteristics** | 16 |
| **Max Payload** | 244 bytes (ATT MTU - 3) |
| **Max Connections** | 1 (single phone) |
| **Advertising Data Max** | 31 bytes |
| **LE Secure Connections** | ✅ Supported |
| **OOB Pairing (NFC)** | ✅ Supported (`ble_oob_pair`) |
| **Connection Interval** | Configurable (30 ms – 50 ms typical) |
| **Supervision Timeout** | Configurable |
| **Message Types** | PairRequest/Response, KeyCreate/Delete/Share, AuthRequest/Response, UWBConfig, StateNotify, Error |

### 3.3 BLE GATT Profile (UUID 0xFFD1)

| Characteristic | UUID | Properties | Permissions | Supported |
|:--------------|:----:|:----------|:-----------:|:---------:|
| CCC Digital Key Service | 0xFFD1 | — | — | ✅ |
| *16 CCC-defined characteristics* | per CCC spec | Read/Write/Notify/Indicate | Encrypted | ✅ |

### 3.4 UWB Secure Ranging

| Parameter | Supported Value |
|:----------|:---------------|
| **UWB IC** | NXP NCJ29D6 |
| **Standard** | IEEE 802.15.4z HRP |
| **Supported Channels** | 5 (6.5 GHz), 9 (8.3 GHz) |
| **Preamble Codes** | 9 (ch5), 12 (ch9) |
| **PRF Length** | 128 |
| **Ranging Method** | Two-Way Ranging (TWR) |
| **STS (Scrambled Timestamp)** | ✅ Supported (anti-relay) |
| **RFrame Configurations** | SP0, SP1, SP3 |
| **Max Sessions** | 4 |
| **Distance Zones** | LOCKED (>10m), APPROACH (5-10m), UNLOCK (2-5m), ENTRY (0-2m), INSIDE (<0.5m) |
| **Ranging Accuracy** | ≤ 10 cm |
| **Distance Thresholds** | Configurable via `distance_threshold_t` |

### 3.5 SE050 Secure Element

| Parameter | Supported Value |
|:----------|:---------------|
| **SE Chip** | NXP SE050 (EAL5+) |
| **SCP03 Version** | Full (ENC + MAC + DEK keys) |
| **Key Slots** | Root (0x00), Master (0x01), Device (0x02), Session (0x10+) |
| **Transparent Objects** | 0xF00000 – 0xF00020 (32 slots) |
| **Max Keys Stored** | 8 |
| **I2C Address** | 0x48 |
| **Attestation** | ✅ Supported (`ccc_attestation_t`) |
| **Firmware Hash Verification** | ✅ Supported (SHA-256) |

### 3.6 BLE Secure Channel — CCC-TS-101 v4.0.0（2b-E 增补）

> 依据: `docs/certification/ccc-ts101-ble-secure-channel.md`（唯一依据, 所有字节级声明可追溯到 PDF 页码）

| 参数 | 值 | 出处 |
|:-----|:---|:-----|
| **系统密钥派生** | HKDF-SHA256 (RFC 5869), IKM=SK (SPAKE2+ 共享密钥 32 B), Info="SystemKeys", Salt=NULL; 输出 Kenc/Kmac/Krmac/LTSS 各 128 bit（flag true 时 +Kble_intro/Kble_oob_master） | §18.4.9, PDF p.429 |
| **命令加密** | GPC_SPE_014 §6.2.6 = SCP03 风格: AES-128-CBC (ICV=counter block) + CMAC-AES-128 8 B, MAC chaining | §18.4.12, PDF p.429-430 |
| **响应加密** | GPC_SPE_014 §6.2.7: counter block `8000...h || counter`; R-MAC 用 Krmac 验证 | §18.4.13, PDF p.430 |
| **DK 消息帧头** | **4 字节**: MsgHeader(1, Bit[5:0]=Type) + PayloadHeader(1, Message ID) + Length(2, **大端**) | 表 19-19, PDF p.449 |
| **消息类型** | 0=Framework, 1=SE, 2=UWB Ranging Service, 3=DK Event, 4=Vehicle OEM App, 5=Supplementary, 6=Head Unit Pairing | 表 19-21 |
| **APDU 封装** | DK_APDU_RQ=0x0B / DK_APDU_RS=0x0C; class byte 00h(SELECT)/80h(非安全)/84h(安全消息) | §19.3.2, PDF p.459-460 |
| **传输** | L2CAP LE credit-based connection (SPSM 0x0080-0x00FF, UUID_SPSM=`D3B5A130-...`); 链路须先 LE 加密, 未加密 5 s 断开 | §19.2.1.7 / §19.2.2, PDF p.445 |
| **广告格式** | Legacy LE 1M PHY, ADV_IND; AD1: 0xFFF5 (CCC_DK_UUID); AD2: Service Data 128-bit UUID `5810bbc0-...` + IntentConfiguration + Vehicle Brand ID | §19.2.1.3, PDF p.437-438 |
| **测试向量** | §19.3 SELECT APDU 示例 wire 级比对（0x010B0013 00A40400...） | PDF p.451 |

---

## 4. Key Types and Lifecycle

### 4.1 Key Types

| Key Type | Enum Value | Supported | Description |
|:---------|:----------:|:---------:|:-----------|
| Owner Key | 0x01 | ✅ | Full access rights |
| Friend Key | 0x02 | ✅ | Limited admin, owner-shareable |
| Service Key | 0x03 | ✅ | Service provider access |
| Temporary Key (Valet) | 0x04 | ✅ | Time/use-limited |

### 4.2 Key States

| State | Enum Value | Supported | Description |
|:------|:----------:|:---------:|:-----------|
| Inactive | 0x00 | ✅ | Key created but not active |
| Active | 0x01 | ✅ | Key is valid for vehicle access |
| Suspended | 0x02 | ✅ | Temporarily disabled |
| Expired | 0x03 | ✅ | Validity period elapsed |
| Revoked | 0x04 | ✅ | Permanently invalidated |

### 4.3 Access Rights

| Right | Bit | Supported | Description |
|:------|:---:|:---------:|:-----------|
| Lock/Unlock | 0 | ✅ | Door lock and unlock |
| Engine Start | 1 | ✅ | Start/stop engine |
| Trunk | 2 | ✅ | Trunk open/close |
| Windows | 3 | ✅ | Window control |
| Sunroof | 4 | ✅ | Sunroof control |
| Climate | 5 | ✅ | HVAC control |
| Seat | 6 | ✅ | Seat adjustment |
| Fuel Door | 7 | ✅ | Fuel door release |

### 4.4 Key Lifecycle Operations

| Operation | Supported | Notes |
|:----------|:---------:|:------|
| Key Creation | ✅ | Cloud-initiated, SE050 signed |
| Key Activation | ✅ | BLE command via GATT |
| Key Sharing | ✅ | `key_share()` with type + duration |
| Key Revocation | ✅ | `key_revoke()` |
| Key Suspension | ✅ | `key_suspend()` |
| Key Resume | ✅ | `key_resume()` |
| Key Deletion | ✅ | `key_delete()` |
| Key Validation | ✅ | `key_validate()` checks cert + signature + expiry |

---

## 5. Security Mechanisms

### 5.1 Cryptographic Algorithms

| Algorithm | Standard | Supported | Key Length | Use |
|:----------|:---------|:---------:|:----------:|:----|
| ECDSA P-256 Sign | FIPS 186-4 | ✅ | 256-bit | Certificate chain, attestation |
| ECDSA P-256 Verify | FIPS 186-4 | ✅ | 256-bit | Signature verification |
| SHA-256 | FIPS 180-4 | ✅ | 256-bit digest | Hashing |
| HMAC-SHA256 | RFC 2104 | ✅ | 256-bit | Key derivation, integrity |
| AES-256-GCM | FIPS 197 | ✅ | 256-bit key | Payload encryption |
| ECDH P-256 | FIPS 186-4 | ✅ | 256-bit | Key agreement |

### 5.2 Certificate Chain

| Item | Status | Details |
|:-----|:------:|:--------|
| CCC Root CA | ✅ | Managed by Cloud PKI |
| OEM CA | ✅ | Per-OEM intermediate CA |
| Vehicle Certificate | ✅ | VIN-bound X.509 certificate |
| Device Certificate | ✅ | Per-device (phone) X.509 |
| Certificate Validation | ✅ | Full chain verification |
| Attestation Generation | ✅ | SE050-signed device attestation |

### 5.3 Secure Channel

| Mechanism | Status | Notes |
|:----------|:------:|:------|
| SCP03 Secure Channel | ✅ | SE050 communication |
| Challenge-Response Auth | ✅ | Nonce + ECDSA signature |
| Anti-Replay (Nonce) | ✅ | Per-session random challenge |
| Anti-Relay (UWB STS) | ✅ | Scrambled Timestamp |
| Encrypted Payload (AES-256-GCM) | ✅ | Confidentiality + integrity |
| Key Confirmation | ✅ | Mutual authentication |

### 5.4 Attestation Structure

| Field | Size | Description |
|:------|:----:|:-----------|
| Version | 1 B | Attestation version |
| Nonce | 16 B | Challenge nonce |
| Device ID | 16 B | Unique device identifier |
| Key ID | 16 B | Associated key identifier |
| Key Type | 1 B | Key type enum |
| Access Rights | 4 B | Permission bitmask |
| Firmware Hash | 32 B | SHA-256 of firmware |
| Security State | 1 B | SE security state |
| Attestation Cert | ≤256 B | SE050 attestation certificate |
| Signature | 64 B | ECDSA P-256 signature |

---

## 6. Passive Entry (CCC-Specific)

| Feature | Status | Description |
|:--------|:------:|:-----------|
| Approach Detection (UWB) | ✅ | ≤2m unlock within 1s |
| Auto-Lock on Departure | ✅ | ≥5m distance, >30s delay |
| Phone Screen-Off Unlock | ✅ | Background BLE + UWB |
| Distance Zone Change | ✅ | 5 configurable zones |
| Hysteresis | ✅ | 30 cm configurable |

---

## 7. Vehicle Control Commands

| Command | BLE Message | Supported |
|:--------|:-----------:|:---------:|
| Unlock | 0x20 (Auth) → 0x21 | ✅ |
| Lock | 0x20 (Auth) → 0x21 | ✅ |
| Engine Start | 0x20 (Auth) → 0x21 | ✅ |
| Engine Stop | 0x20 (Auth) → 0x21 | ✅ |
| Trunk Open | 0x20 (Auth) → 0x21 | ✅ |
| Window Control | 0x20 (Auth) → 0x21 | ✅ |
| Sunroof Control | 0x20 (Auth) → 0x21 | ✅ |
| Climate Control | 0x20 (Auth) → 0x21 | ✅ |

---

## 8. SDK 移动端补充（v1.1 增补）

> 本 PICS 主体为车端嵌入式声明；以下补充移动端 SDK（iOS/Android）在 CCC 认证中涉及的平台能力。
> 详细测试项见 `docs/certification/sdk-certification-checklist.md` §2.1。

| 功能面 | iOS | Android | 状态 |
|:------|:---:|:-------:|:----:|
| BLE 安全通道（v4.0.0） | ✅ `CCCSecureChannel.swift` + `CCCCommandFrame.swift`（16/16 断言 + wire 级 9/9） | ✅ `CCCSecureChannel.kt` + `CccFrame.kt`（测试就位） | ✅ 代码就位 |
| 后台连接恢复 | ✅ restore identifier + willRestoreState + 唤醒 options | ✅ 前台服务 + autoConnect | ✅ 代码就位 / ⏳ 真机待验 |
| NFC OOB 配对 | ✅ `YDKCoreNFCManager.swift`（CoreNFC 桩 12/12 断言） | ✅ `AndroidNfcManager.kt`（NfcAdapter/IsoDep 33/33） | ✅ 代码就位 / ⏳ 真机待验（entitlement/tech-list） |
| UWB 测距 | ✅ `YDKNIUWBManager.swift`（NearbyInteraction） | ✅ `AndroidUwbManager.kt`（android.uwb API 34+） | ✅ 代码就位 / ⏳ 真机待验（token 交换） |
| 分享（Relay/Mailbox） | ✅ MailboxClient (gRPC) + ShareFlow（7/7 独立验证） | ✅ MailboxClient + ShareFlow（16 wire 用例） | ✅ 链路就位 / ⏳ 物理机 E2E 单列 |
| 远程控车（经 Hub） | ✅ HubClient gRPC | ✅ HubClient gRPC | ✅ 代码就位（4.2 E2E 21 断言） |

**分享依赖**: Relay Server 侧认证声明见 `docs/compliance/PICS_PIXIT_RELAY.md`（6 API + 状态机 + Push）。

---

## 9. Conformance Summary

| Requirement Category | Count | Implemented | Notes |
|:--------------------|:-----:|:-----------:|:------|
| NFC Communication | 10 | ✅ 10/10 | ISO 14443-4, NDEF, APDU |
| BLE GATT Profile | 10 | ✅ 10/10 | All characteristics |
| UWB Secure Ranging | 8 | ✅ 8/8 | TWR, STS, zones |
| Security / SE | 10 | ✅ 10/10 | SCP03, Attestation, ECDSA |
| Passive Entry | 6 | ✅ 6/6 | UWB approach, auto-lock |
| Key Lifecycle | 10 | ✅ 10/10 | Create → Revoke |
| Vehicle Control | 8 | ✅ 8/8 | All CCC core commands |
| **Total** | **62** | **62/62** | |

---

## 10. Version History

| Version | Date | Changes | Author |
|:-------:|:----:|:--------|:------:|
| v1.0 | 2026-07-30 | Initial CCC PICS document | Hermes |
| v1.1 | 2026-08-01 | 增补: CCC-TS-101 v4.0.0 BLE 安全通道（§3.6, 2b-E 裁决）+ SDK 移动端补充（§8）+ GATT 0xFFD1 标注为 R3.0 遗留 | Hermes |
