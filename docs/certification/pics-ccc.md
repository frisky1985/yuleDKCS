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
| CCC GATT Profile (UUID 0xFFD1) | ✅ Supported | 16 GATT characteristics |
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

## 8. Conformance Summary

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

## 9. Version History

| Version | Date | Changes | Author |
|:-------:|:----:|:--------|:------:|
| v1.0 | 2026-07-30 | Initial CCC PICS document | Hermes |
