# ICCE PICS — Protocol Implementation Conformance Statement

> **Document Version**: v1.0 | **Date**: 2026-07-30
> **Product**: yuleDKCS Digital Key System (Vehicle-Side Embedded)
> **Standard**: T/CA 110-2020 — 智能网联汽车数字钥匙系统技术要求
> **Certification Body**: 中国信通院 (CAICT) / 中国汽车技术研究中心 (CATARC)

---

## 1. Implementation Identification

| Item | Value |
|:-----|:------|
| **Implementer** | yuleDKCS (OpenClaw Technology) |
| **Product Name** | yuleDKCS Vehicle Digital Key Module |
| **Firmware Version** | v1.0.0 |
| **Protocol Stack Version** | ICCE 1.0 (T/CA 110-2020) |
| **Hardware Platform** | NXP KW47A (BLE/NFC MCU) + NXP SE050 (Secure Element) + NXP NCJ29D6 (UWB) + ST ST25R501 (NFC Reader) |
| **Submitted Date** | 2026-07-30 |

---

## 2. Protocol Version Support

| Feature | Status | Notes |
|:--------|:------:|:------|
| ICCE T/CA 110-2020 Core Specification | ✅ Supported | Full implementation |
| ICCE BLE Profile (Service UUID 0xFEFA/0xFEF5) | ✅ Supported | Advertising + GATT |
| ICCE UWB FiRa Integration | ✅ Supported | Edge zone management |
| ICCE Edge Computing Module | ✅ Supported | Local rule engine |
| ICCE Vehicle TCU Integration | ✅ Supported | CAN/I2C bridge |
| ICCE Cloud KMS Integration | ✅ Supported | SM2/SM3/SM4 |

---

## 3. Communication Methods

### 3.1 BLE Communication (Bluetooth Low Energy)

| Parameter | Supported Value |
|:----------|:---------------|
| **BLE Version** | Bluetooth 5.0 (LE) |
| **Service UUID** | 0xFEFA (ICCE Digital Key Service) |
| **Characteristic UUIDs** | 0xFEFB (Key Status), 0xFEFC (Ranging Data), 0xFEFD (Auth Challenge), 0xFEFE (Control Command), 0xFEFF (Session Key) |
| **Max Payload** | 244 bytes (ATT MTU - 3) |
| **Advertising Interval** | 100 ms – 1000 ms (configurable) |
| **Connection Interval** | 30 ms – 50 ms |
| **LE Secure Connections** | ✅ Supported |
| **OOB Pairing (NFC)** | ✅ Supported |
| **BLE Coded PHY (Long Range)** | ✅ Supported |
| **BLE 2M PHY** | ✅ Supported |
| **Simultaneous Connections** | Up to 8 devices |

### 3.2 NFC Communication

| Parameter | Supported Value |
|:----------|:---------------|
| **NFC Protocol** | ISO/IEC 14443 Type A |
| **NFC Reader IC** | ST ST25R501 |
| **LPCD (Low Power Card Detection)** | ✅ Supported |
| **OOB Data Exchange** | ✅ Supported (pairing bootstrap) |
| **Offline NFC Unlock** | ✅ Supported (phone power-off scenario) |
| **Communication Distance** | 0 – 4 cm |
| **Data Rate** | 106 / 212 / 424 kbps |

### 3.3 UWB Communication

| Parameter | Supported Value |
|:----------|:---------------|
| **UWB IC** | NXP NCJ29D6 |
| **Protocol** | IEEE 802.15.4z HRP |
| **Channel Support** | 5 (6.5 GHz), 6 (7.9 GHz), 8 (7.9 GHz), 9 (8.3 GHz) |
| **Ranging Method** | Two-Way Ranging (TWR) |
| **STS (Scrambled Timestamp)** | ✅ Supported |
| **Max Ranging Sessions** | 4 |
| **Max Anchors** | 8 |
| **Ranging Accuracy** | ≤ 10 cm (open environment) |
| **ROLE** | Controller / Responder |

### 3.4 SE050 Secure Element

| Parameter | Supported Value |
|:----------|:---------------|
| **SE Chip** | NXP SE050 (EAL5+) |
| **SCP03 Secure Channel** | ✅ Supported |
| **Key Storage** | Transparent Object (0xF00000+), up to 8 keys |
| **Key Slots** | 0x00 (Root), 0x01 (Master), 0x02 (Device), 0x10+ (Session) |
| **I2C Address** | 0x48 |

---

## 4. Key Types and Lifecycle

### 4.1 Key Types

| Key Type | Supported | Description |
|:---------|:---------:|:-----------|
| Owner Key (Primary) | ✅ | Full access, full management rights |
| Friend Key | ✅ | Limited admin, grantable by owner |
| Service Key | ✅ | Time-limited, restricted actions |
| Temporary Key (Valet) | ✅ | Use-count limited, time-bound |
| Device Key (SE050) | ✅ | Hardware-bound key pair |

### 4.2 Key States (Lifecycle)

| State | Supported | Description |
|:------|:---------:|:-----------|
| Created (初始) | ✅ | Key template generated in cloud |
| Pre-Paired (预配对) | ✅ | Vehicle received key, awaiting phone |
| Paired (配对完成) | ✅ | Mutual auth complete, shared secret established |
| Active (激活) | ✅ | Key is valid for use |
| Suspended (暂停) | ✅ | Temporarily disabled |
| Expired (过期) | ✅ | Time-based key expiry |
| Revoked (吊销) | ✅ | Remotely disabled by owner/cloud |
| Deleted (删除) | ✅ | Fully removed from vehicle |

### 4.3 Key Lifecycle Operations

| Operation | Supported | Protocol Mapping |
|:----------|:---------:|:----------------|
| Key Provisioning (KeyBind) | ✅ | BERTLV KeyBind |
| Key Activation | ✅ | BLE command |
| Key Authorization (ShareCreate) | ✅ | BLE command |
| Key Revocation | ✅ | BLE/Cloud command |
| Key Rotation | ✅ | Cloud-initiated |
| Key Deletion | ✅ | BLE/Cloud command |
| Key Suspension | ✅ | Cloud command |

---

## 5. Security Mechanisms

### 5.1 Cryptographic Algorithms

| Algorithm | Standard | Supported | Key Length | Use |
|:----------|:---------|:---------:|:----------:|:----|
| SM2 Sign/Verify | GB/T 32918.1-2016 | ✅ | 256-bit | Certificate chain, auth |
| SM2 Key Exchange | GB/T 32918.1-2016 | ✅ | 256-bit | Session key agreement |
| SM3 Hash | GB/T 32905-2016 | ✅ | 256-bit digest | Integrity checking |
| SM4-GCM Encrypt | GB/T 32907-2016 | ✅ | 128-bit key | Secure channel payload |
| ECDSA P-256* | FIPS 186-4 | ✅ | 256-bit | Cross-protocol fallback |
| AES-256-GCM* | FIPS 197 | ✅ | 256-bit | Cross-protocol fallback |

*\* Supported for CCC/ICCOA protocol paths, not used in native ICCE mode.*

### 5.2 Certificate Chain

| Item | Status | Details |
|:-----|:------:|:--------|
| SM2 Root CA | ✅ | Managed by Cloud KMS |
| OEM Intermediate CA | ✅ | Per-OEM certificate |
| Vehicle Certificate | ✅ | Issued per vehicle (VIN-bound) |
| Device Certificate | ✅ | Issued per phone/user |
| Certificate Revocation | ✅ | CRL + OCSP support |
| Certificate Validation | ✅ | Full chain verification |

### 5.3 Secure Channel

| Mechanism | Status | Notes |
|:----------|:------:|:------|
| ECDH Key Agreement (SM2) | ✅ | Session key establishment |
| Challenge-Response Auth | ✅ | Nonce + signature |
| Anti-Replay (Nonce) | ✅ | Per-session random challenge |
| Anti-Relay (UWB STS) | ✅ | UWB Scrambled Timestamp |
| Encrypted Payload (SM4-GCM) | ✅ | Confidentiality + integrity |
| SE050 SCP03 | ✅ | Secure channel to SE |

### 5.4 Key Storage

| Storage Type | Algorithm | Security Level |
|:-------------|:---------:|:--------------|
| SE050 Transparent Object | SM2/SM3/SM4 | EAL5+ hardware |
| Secure Flash (fallback) | AES-256 encrypted | MCU internal flash |
| Memory (volatile sessions) | RAM + secure zero | Runtime only |

---

## 6. Offline Capabilities

| Capability | Status | Description |
|:-----------|:------:|:------------|
| Local Key Cache | ✅ | Cached key material for offline use |
| Offline Auth Decision | ✅ | Edge computing unit evaluates locally |
| NFC Offline Unlock | ✅ | Phone power-off NFC tap |
| Edge Rule Engine | ✅ | 32 rules, 8 actions each |
| Zone-Based Triggers | ✅ | 5 zones (FAR/MID/NEAR/VICINITY/INTERIOR) |
| Reconnect Synchronization | ✅ | Offline operation log sync on reconnect |
| Offline Timestamp Check | ✅ | Local clock for key validity |

---

## 7. Vehicle Control Commands

| Command | Supported | Notes |
|:--------|:---------:|:------|
| Unlock | ✅ | ICCE control command format |
| Lock | ✅ | |
| Engine Start | ✅ | |
| Engine Stop | ✅ | |
| Climate Control | ✅ | |
| Lights | ✅ | |
| Horn | ✅ | |
| Custom Actions | ✅ | Edge programmable actions |

---

## 8. Edge Computing (ICCE-Specific)

| Feature | Status | Details |
|:--------|:------:|:--------|
| Edge Rule Engine | ✅ | ICCE_EDGE_MAX_RULES = 32 |
| Trigger Sources | ✅ | Zone enter/exit, distance, time, gesture, BLE RSSI |
| Action Types | ✅ | Unlock, Lock, Start, Stop, Climate, Lights, Horn |
| Offline Rule Evaluation | ✅ | Full local processing |
| Rule Priority | ✅ | 0 (low) – 255 (critical) |
| Zone Definitions | ✅ | 5 configurable zones with inner/outer thresholds |

---

## 9. Conformance Summary

| Requirement Category | Count | Implemented | Notes |
|:--------------------|:-----:|:-----------:|:------|
| BLE Communication | 10 | ✅ 10/10 | Advertising, GATT, Pairing |
| Vehicle Control | 8 | ✅ 8/8 | All ICCE core commands |
| Key Lifecycle | 12 | ✅ 12/12 | Provisioning → Deletion |
| SM Crypto (国密) | 8 | ✅ 8/8 | SM2, SM3, SM4 |
| Offline Capability | 6 | ✅ 6/6 | Cache, Edge, NFC |
| Security / SE | 8 | ✅ 8/8 | SCP03, Attestation, CertChain |
| **Total** | **52** | **52/52** | |

---

## 10. Version History

| Version | Date | Changes | Author |
|:-------:|:----:|:--------|:------:|
| v1.0 | 2026-07-30 | Initial ICCE PICS document | Hermes |
