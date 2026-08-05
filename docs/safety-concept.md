# yuleDKCS — 安全概念 (Safety Concept)

> **项目**: yuleDKCS 数字钥匙系统
> **版本**: 1.0.0 | **日期**: 2026-07-16 | **状态**: 正式版
> **目标 ASIL**: ASIL-B(D) (关键功能), ASIL-A/QM (非关键功能)
> **参考标准**: ISO 26262:2018, ISO 21434:2021, CCC DK 3.0, ICCOA DK 4.0, ICCE T/CA 110-2020
> **硬件平台**: NXP S32G2/G3 + SE050 (EAL 6+), NXP KW47A (BLE), NCJ29D6 (UWB), ST25R501 (NFC)

---

## Table of Contents

1. [Item Definition](#1-item-definition)
2. [Hazard Analysis and Risk Assessment (HARA)](#2-hazard-analysis-and-risk-assessment-hara)
3. [Safety Goals (SG)](#3-safety-goals-sg)
4. [Functional Safety Requirements (FSR)](#4-functional-safety-requirements-fsr)
5. [Technical Safety Requirements (TSR)](#5-technical-safety-requirements-tsr)
6. [Safety Mechanisms](#6-safety-mechanisms)
7. [Safety Architecture](#7-safety-architecture)
8. [Dependent Failure Analysis (DFA)](#8-dependent-failure-analysis-dfa)
9. [Safety Case Summary](#9-safety-case-summary)
10. [Glossary](#10-glossary)

---

## 1. Item Definition

### 1.1 System Scope

The Digital Key System (DKS) enables phone-as-key functionality — passive keyless entry, passive keyless start, remote vehicle control, and key lifecycle management — across three tiers:

- **Embedded Tier (Vehicle)**: Runs on NXP S32G2/G3 MCU with SE050 secure element. Implements ICCE/CCC/ICCOA protocol stacks. Communicates with mobile via BLE (KW47A), UWB (NCJ29D6), and NFC (ST25R501), and with cloud via MQTT/TLS.
- **Mobile Tier (App)**: Android (Kotlin, StrongBox/Android Keystore) and iOS (Swift, Secure Enclave/Keychain) SDKs. Provides user-facing key management and vehicle control.
- **Cloud Tier (Backend)**: Go HUB Gateway + Go DKCS Core + Java Protocol Adapters (CCC/ICCOA/ICCE). Handles key provisioning, sharing, revocation, and telematics service provider (TSP) integration.

### 1.2 System Boundaries

| Boundary | Included | Excluded |
|:---------|:---------|:---------|
| Function | Passive entry/passive start, NFC access, remote control, key sharing, key revocation, OTA update | Vehicle motion control, ADAS, powertrain, braking |
| Communication | BLE (LE Secure Connections), UWB (IEEE 802.15.4z STS), NFC (ISO 7816-4 SCP03), MQTT/TLS 1.3, gRPC/TLS 1.3 | In-vehicle CAN bus outside TCU, cellular baseband |
| Security | Key generation (SE050/HSM), secure storage, identity authentication, communication encryption, secure boot, SEP provisioning | OEM PKI root, cellular carrier security |

### 1.3 Operating Modes

| Mode | Description | Safety Relevance |
|:-----|:------------|:-----------------|
| Normal | User approaches/leaves vehicle, BLE+UWB handshake, door unlock/lock, engine start | High |
| NFC Mode | Phone battery depleted or offline, NFC passive entry via SE050 authentication | High |
| Remote Mode | User sends vehicle command via cloud (app → HUB → DKCS → TCU) | Medium |
| Key Provisioning | First-time key creation/binding between phone and vehicle | High |
| Key Sharing | Owner shares limited keys to family/friends via cloud | Medium |
| Key Revocation | Owner revokes shared keys or replaces lost device keys | High |
| OTA Update | TCU firmware update with SE050-verified signature chain | Critical |
| Secure Recovery | TCU enters safe state after detected tamper/boot failure | Critical |

### 1.4 Assumptions

1. **SE050 Trust Anchors**: SE050 secure element provides EAL 6+ physical and logical protection. Root Key (RK) is injected at NXP secure manufacturing facility and never exposed in plaintext.
2. **Mobile TEE**: Mobile device TEE/SE protection (iOS Secure Enclave / Android StrongBox) adequately protects app-level key material. App integrity is verified at runtime.
3. **OEM PKI**: A properly managed OEM PKI infrastructure exists to issue and rotate vehicle certificates and HSM credentials.
4. **Communication Channels**: BLE and NFC radios operate within regulatory spectral limits. UWB operates per IEEE 802.15.4z with STS enabled.
5. **No Concurrent Physical Access**: The attacker does not simultaneously possess both physical access to the vehicle interior AND compromise the SE050.

---

## 2. Hazard Analysis and Risk Assessment (HARA)

### 2.1 Severity, Exposure, and Controllability Parameters

| Parameter | Rating | Description |
|:----------|:------:|:------------|
| S0 | No injuries | No harm to persons |
| S1 | Light injury | Minor injuries, not requiring hospitalization |
| S2 | Severe injury | Possible severe or life-threatening injuries |
| S3 | Fatal injury | Life-threatening or fatal injuries |

| Parameter | Rating | Description |
|:----------|:------:|:------------|
| E1 | Very low | Occurs less than once per year for <1% of operating time |
| E2 | Low | Occurs a few times per year for 1-10% of operating time |
| E3 | Medium | Occurs monthly for 10-50% of operating time |
| E4 | High | Occurs during typical driving cycle, >50% of operating time |

| Parameter | Rating | Description |
|:----------|:------:|:------------|
| C1 | Controllable | Easily controllable by driver or majority of drivers |
| C2 | Controllable under certain conditions | Controllable by >90% of drivers |
| C3 | Difficult to control | <90% of drivers can control, or uncontrollable |

### 2.2 Hazard Events

| ID | Hazard Description | Operational Situation | S | E | C | ASIL |
|:---|:-------------------|:----------------------|:-:|:-:|:-:|:----:|
| **H-01** | **Unauthorized unlock**: Vehicle doors unlock without a valid authenticated digital key, via relay attack, protocol bypass, or cryptographic failure | User parked, vehicle unattended in public area. Attacker within BLE/UWB range (<10m) attempting relay/spoofing of phone-as-key | S2 | E3 | C2 | **ASIL B** |
| **H-02** | **Unauthorized engine start**: Internal combustion engine or electric motor starts without authenticated key present inside the passenger cabin | Vehicle parked, driver potentially nearby. Attacker exploits relay/wireless compromise to start vehicle from outside | S2 | E3 | C3 | **ASIL B** |
| **H-03** | **Unauthorized remote command**: Vehicle performs remote unlock, window roll-down, sunroof open, or climate activation without proper authenticated request | Attacker compromises cloud API token, performs replay of JWT, or exploits MQTT injection to issue unauthorized commands | S1 | E2 | C2 | **ASIL A** |
| **H-04** | **Key sharing bypass**: Shared key user gains unauthorized capabilities (e.g., start engine when limited to unlock-only; bypass geo-fence/time constraints) | Owner shares a time/geo-limited key. Attacker modifies shared key payload, replays key blob, or exploits permission model logic | S2 | E2 | C2 | **ASIL B** |
| **H-05** | **Revoked key still functional**: A key that has been revoked by the owner remains capable of unlocking/starting the vehicle due to revocation sync failure | Attacker was previously authorized (shared key user). Owner revokes key, but TCU fails to receive CRL update or ignores it | S2 | E3 | C2 | **ASIL B** |
| **H-06** | **Key cloning via key material extraction**: Attacker extracts device private key from mobile device (rooted/jailbroken) or from cloud DB, then clones it to another device for vehicle access | Attacker gains access to phone OS (malware, physical access, jailbreak) or cloud DB (SQL injection, credential leak) | S2 | E1 | C3 | **ASIL B** |
| **H-07** | **Secure boot failure with silent fallback**: Compromised or malicious firmware is loaded without detection, allowing persistent attacker control of TCU | OTA or physical attack corrupts TCU firmware. Failed secure boot does not transition to safe state but continues with arbitrary code | S3 | E2 | C3 | **ASIL B(D)** |
| **H-08** | **Rollback to vulnerable firmware version**: Attacker forces TCU to downgrade to an older firmware version with known vulnerabilities | OTA downgrade attack where version monotonic counter is bypassed or reset | S2 | E2 | C2 | **ASIL B** |
| **H-09** | **SE050 SCP03 session hijack**: Attacker hijacks an established SCP03 secure channel between host MCU and SE050, injecting malicious APDUs | Physical or proximity access to SE050 communication bus (SPI/I2C sniffing or fault injection) | S2 | E1 | C3 | **ASIL B** |
| **H-10** | **VIN/target vehicle mismatch**: Remote command or key material targets wrong vehicle (e.g., VIN misbinding in cloud, TCU identity spoofing) | Cloud misconfiguration or TCU attestation failure causes key for Vehicle A to be accepted by Vehicle B | S2 | E1 | C3 | **ASIL B** |

### 2.3 ASIL Determination Summary

| Hazard ID | S | E | C | ASIL | Rationale |
|:----------|:-:|:-:|:-:|:----:|:----------|
| H-01 | 2 | 3 | 2 | **B** | Unauthorized vehicle access can lead to theft, property damage, and harm; exposure is medium (vehicle parked in various locations); cautious driver could detect but electronic theft is difficult to counteract |
| H-02 | 2 | 3 | 3 | **B** | Unauthorized vehicle motion poses severe injury risk; high exposure (engine start per trip); very difficult for victim to control if car drives away |
| H-03 | 1 | 2 | 2 | **A** | Remote command typically involves property damage risk only; low exposure (requires active attack); controllable if owner monitors app |
| H-04 | 2 | 2 | 2 | **B** | Unauthorized enhanced privileges (e.g., engine start) from shared key bypass can lead to vehicle theft and harm; moderate exposure (key sharing in family/friend scenarios) |
| H-05 | 2 | 3 | 2 | **B** | Previously authorized but revoked individual could access vehicle; medium exposure (revocation occurs in relationship breakdown scenarios); thief posture difficult to control |
| H-06 | 2 | 1 | 3 | **B** | Key cloning allows unauthorized vehicle access; very low exposure (requires sophisticated attack); virtually impossible for victim to control remotely |
| H-07 | 3 | 2 | 3 | **B(D)** | Compromised firmware could disable braking/steering safety functions; requires elevated ASIL(D) for systematic integrity; very difficult to detect by driver |
| H-08 | 2 | 2 | 2 | **B** | Known-vulnerability firmware exposure; moderate exposure (OTA update events); difficult for non-expert driver to controll |
| H-09 | 2 | 1 | 3 | **B** | SE050 compromise enables key extraction; rare but severe; virtually uncontrollable |
| H-10 | 2 | 1 | 3 | **B** | VIN mismatch leads to wrong vehicle access; rare but severe; uncontrollable by driver occupant |

---

## 3. Safety Goals (SG)

Each Safety Goal is assigned ASIL from the parent hazard event (highest when multiple hazards map). Format follows ISO 26262-3 conventions.

| ID | Safety Goal | ASIL | Associated Hazards | FTTI | Safe State |
|:---|:------------|:----:|:-------------------|:----:|:-----------|
| **SG-01** | The system SHALL prevent vehicle unlock without a valid, authenticated, and authorized digital key, verified through cryptographic challenge-response | **ASIL B** | H-01, H-06 | <500ms | Doors remain locked |
| **SG-02** | The system SHALL prevent engine/motor start without a valid authenticated key confirmed to be physically present inside the passenger cabin via UWB ranging | **ASIL B** | H-02, H-04 | <500ms | Ignition disabled, fuel/electric cut |
| **SG-03** | The system SHALL detect and reject relay attacks on BLE/UWB ranging by requiring PHY-level distance bounding (STS) and cross-checking RSSI with UWB ToF | **ASIL B(D)** | H-01, H-02 | <100ms | Doors locked, start disabled |
| **SG-04** | The system SHALL authenticate all remote vehicle commands with dual-factor cryptographic proof: JWT session token PLUS device-bound key signature | **ASIL A** | H-03 | <1s | Command rejected, error logged |
| **SG-05** | The system SHALL enforce key permission boundaries: a shared key SHALL NOT provide capabilities beyond what the owner explicitly authorized | **ASIL B** | H-04 | <100ms | Operation denied, audit logged |
| **SG-06** | The system SHALL ensure a revoked key is rendered incapable of vehicle access within 10 seconds of revocation confirmation on the cloud | **ASIL B** | H-05 | <10s | Key usage rejected, entry/start blocked |
| **SG-07** | The system SHALL protect cryptographic key material (RK, MK, DK) against extraction or cloning at all stages: at rest (SE050), in transit (SCP03), and in use (HW-backed operations only) | **ASIL B** | H-06, H-09 | N/A (continuous) | N/A (design constraint) |
| **SG-08** | The system SHALL verify the integrity and authenticity of all executable firmware via a SE050-anchored multi-stage secure boot chain, and SHALL transition to a safe state on verification failure | **ASIL B(D)** | H-07, H-08 | <1s (boot time) | Secure recovery mode, vehicle disabled |
| **SG-09** | The system SHALL enforce monotonic version counters for all firmware images via SE050 tamper-resistant NV counters, preventing downgrade to vulnerable versions | **ASIL B** | H-08 | N/A (boot time) | Boot rejected, error reported |
| **SG-10** | The system SHALL verify TCU identity and VIN binding before accepting key provisioning or remote commands, using SE050 attestation and cloud-side certificate validation | **ASIL B** | H-10 | <1s | Provisioning/command rejected |

---

## 4. Functional Safety Requirements (FSR)

Each SG is decomposed into 2-3 FSRs with ASIL inheritance.

### 4.1 FSR for SG-01 (Unauthorized Unlock Prevention)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-01-01** | The system SHALL implement a cryptographic challenge-response protocol (ECDSA P-256) between the mobile app and the TCU over BLE or NFC before authorizing any vehicle unlock. The challenge nonce SHALL be generated by the TCU SE050 TRNG and SHALL be unique per session | **ASIL B** | Prevents replay; SE050 TRNG ensures nonce unpredictability |
| **FSR-01-02** | The system SHALL verify that the mobile device is within UWB-measured distance ≤ 2m (unlock) AND ≤ 1m (engine start) before granting access. If UWB is unavailable, unlock via BLE-only SHALL require explicit user action (NFC tap or app button press) | **ASIL B** | PHY-level distance verification defeats relay amplification |
| **FSR-01-03** | The system SHALL store all private keys on the mobile device exclusively in hardware-backed secure storage (iOS Secure Enclave / Android StrongBox), and on the vehicle exclusively in SE050. No key material SHALL leave the secure boundary in plaintext | **ASIL B** | Hardware isolation prevents software-level extraction |

### 4.2 FSR for SG-02 (Unauthorized Start Prevention)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-02-01** | The system SHALL verify dual presence criteria for engine start authorization: (1) authenticated BLE connection with the paired mobile device, AND (2) UWB-measured distance ≤ 1m indicating device inside cabin | **ASIL B** | Dual confirmation prevents relay from outside |
| **FSR-02-02** | The system SHALL reject engine start if the authenticated device's permission set does not include the `engine_start` capability, regardless of successful BLE/UWB presence | **ASIL B** | Enforces permission model at physical gate |
| **FSR-02-03** | The system SHALL periodically re-verify device presence (every 60s) during engine-on state. If presence fails (BLE disconnect + 30s timeout), the system SHALL log the event and MAY trigger a warning, but SHALL NOT stop a running engine | **ASIL B** | Safety: no sudden engine cut while driving |

### 4.3 FSR for SG-03 (Relay Attack Detection)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-03-01** | The system SHALL use UWB IEEE 802.15.4z STS (Scrambled Timestamp Sequence) mode for all ranging operations. The STS key SHALL be derived from the authenticated BLE session key to prevent ranging spoofing | **ASIL B(D)** | PHY-level STS prevents distance manipulation |
| **FSR-03-02** | The system SHALL cross-validate the UWB ToF (Time-of-Flight) measurement against the BLE RSSI estimate. If the two measurements are inconsistent by more than a configurable threshold (e.g., UWB says 1m but BLE RSSI indicates >10m), the system SHALL reject the unlock/start request | **ASIL B(D)** | Multi-path inconsistency detection reveals relay |
| **FSR-03-03** | The system SHALL enforce a maximum UWB round-trip time (RTT) threshold of 3 μs (equivalent to ~450m cable propagation delay). Any RTT exceeding this threshold SHALL be treated as a relay attack | **ASIL B(D)** | Direct detection of even wired relay delay |

### 4.4 FSR for SG-04 (Remote Command Authentication)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-04-01** | The system SHALL require a valid JWT (RS256-signed, max 15-min TTL, bound to device fingerprint) AND a device key signature (ECDSA P-256) over the command payload for every remote vehicle control operation | **ASIL A** | Dual-factor eliminates single point of credential theft |
| **FSR-04-02** | The system SHALL embed a monotonic sequence number and a timestamp (ms precision) in every remote command message. The TCU SHALL reject any command with a sequence number ≤ the last accepted sequence number | **ASIL A** | Replay protection across cloud-to-vehicle path |
| **FSR-04-03** | The system SHALL enforce TLS 1.3 with mutual TLS (mTLS) for the HUB↔DKCS↔TCU communication chain. All certificates SHALL have a maximum validity of 90 days and SHALL support OCSP stapling | **ASIL A** | MQTT/gRPC channel authenticity and forward secrecy |

### 4.5 FSR for SG-05 (Key Permission Enforcement)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-05-01** | The system SHALL encode key permissions in a signed permission token (BER-TLV structure) that is verified by the TCU SE050 at every vehicle access attempt. The token SHALL be indelibly linked to the key ID | **ASIL B** | Tamper-proof permission encoding |
| **FSR-05-02** | The system SHALL enforce geo-fencing constraints in the TCU by comparing the UWB-verified device position against the permission token's allowed regions. Operations outside the permitted geographic boundary SHALL be rejected | **ASIL B** | Geo-restriction for shared keys |
| **FSR-05-03** | The system SHALL enforce temporal constraints (valid_from, valid_until) and usage limits (max_uses) on shared keys. The TCU SHALL maintain a usage counter in SE050 monotonic NV memory | **ASIL B** | HW-counter prevents usage-count tampering |

### 4.6 FSR for SG-06 (Key Revocation)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-06-01** | The cloud service (DKCS) SHALL push revocation updates to the TCU via MQTT with TTL ≤ 10s. The TCU SHALL maintain a local Certificate Revocation List (CRL) in SE050-based storage, checked before every unlock/start authorization | **ASIL B** | Online revocation with bounded latency |
| **FSR-06-02** | The TCU SHALL query the cloud CRL upon boot and at configurable intervals (max 1 hour) during normal operation. If the cloud is unreachable for more than 24 hours, the TCU SHALL accept only the owner's primary key and reject all shared/guest keys | **ASIL B** | Offline resilience with degraded-mode safety |
| **FSR-06-03** | The TCU SHALL reject any cryptographic operation using a key whose key ID appears in the active CRL. The CRL SHALL be stored in SE0250 monotonic-write memory to prevent rollback of revocation records | **ASIL B** | Anti-rollback for revocation state |

### 4.7 FSR for SG-07 (Key Material Protection)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-07-01** | The system SHALL derive all device keys via HKDF-SHA256 from the Master Key stored in SE050. The derivation context SHALL include the device ID and a domain separator string ("yuledkcs-device-key-v1"). The private key SHALL never be stored outside SE050 in any form | **ASIL B** | Key isolation; domain separation prevents cross-protocol derivation |
| **FSR-07-02** | All communication between the host MCU (S32G2/G3) and the SE050 SHALL use the GlobalPlatform SCP03 secure channel, with session keys derived via ECDH. The system SHALL authenticate the host to SE050 before any key material operation | **ASIL B** | Protects SE050 bus from injection/sniffing |
| **FSR-07-03** | Cloud-stored key material (wrapped device keys, permission tokens) SHALL be encrypted using AES-256-GCM with a key wrapping key stored in a hardware HSM (AWS CloudHSM or equivalent FIPS 140-2 Level 3 validated device). No plaintext key material SHALL exist in cloud memory | **ASIL B** | Cloud-side key material confidentiality |

### 4.8 FSR for SG-08 (Secure Boot)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-08-01** | The system SHALL implement a multi-stage secure boot chain: Boot ROM → BootLoader → TFM → Application. Each stage SHALL verify the next stage's ECDSA P-256 signature using a public key stored in SE050 (Key ID 0x0001). On signature verification failure, the system SHALL halt and enter secure recovery mode | **ASIL B(D)** | Chain-of-trust anchored in SE050 |
| **FSR-08-02** | The system SHALL verify firmware image integrity (SHA-256 hash) AND authenticity (ECDSA P-256 signature) before execution. The SE050 SHALL perform the signature verification in hardware | **ASIL B(D)** | Hardware-backed integrity verification |
| **FSR-08-03** | On secure boot failure, the system SHALL enter a secure recovery mode that: (1) prevents any vehicle operation, (2) logs the failure to SE050 monotonically, (3) allows only signed recovery firmware to be loaded via authenticated debug interface | **ASIL B(D)** | Safe state on integrity failure |

### 4.9 FSR for SG-09 (Anti-Rollback)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-09-01** | The system SHALL maintain monotonic version counters in SE050 NV memory for each firmware component: BootLoader (8-bit), TFM (16-bit), Application (32-bit). The boot process SHALL verify that the image version ≥ the stored counter value, and SHALL update the counter on successful upgrade | **ASIL B** | HW monotonic counter prevents version rollback |
| **FSR-09-02** | The OTA update process SHALL include the target version number as part of the signed update manifest. The signing authority SHALL apply a multi-signature scheme (≥2 of 3: OEM, yuleDKCS, independent auditor) for version deployment | **ASIL B** | Multi-party authorization prevents unilateral malicious update |

### 4.10 FSR for SG-10 (VIN/Identity Binding)

| ID | Requirement | ASIL | Rationale |
|:---|:------------|:----:|:----------|
| **FSR-10-01** | The TCU SHALL present a SE050-signed attestation report containing its unique device ID, VIN, and SE050 certificate chain during initial provisioning. The cloud (DKCS) SHALL verify the attestation signature and SHALL bind all subsequent key operations to this attested identity | **ASIL B** | Vehicle identity bootstrapped from SE050 hardware trust |
| **FSR-10-02** | Every remote command message SHALL include the target VIN. The TCU SHALL verify that the VIN in the command matches its own VIN before executing any operation. Mismatches SHALL be rejected and logged | **ASIL B** | Prevents cross-vehicle command execution |

---

## 5. Technical Safety Requirements (TSR)

TSRs refine FSRs into implementation-specific technical specifications.

### 5.1 TSR for FSR-01-01 (Challenge-Response Protocol)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-01-01-A** | The challenge-response protocol SHALL use a 128-bit random nonce generated by the SE050 TRNG (`SE05x_SessionCreate`). The nonce SHALL be transmitted to the mobile device over the encrypted BLE GATT channel | TCU (Embedded) | ASIL B |
| **TSR-01-01-B** | The mobile device SHALL sign the challenge nonce combined with session context data (device ID, key ID, timestamp) using ECDSA P-256 with the device private key stored in Secure Enclave / StrongBox | App (iOS/Android) | ASIL B |
| **TSR-01-01-C** | The TCU SHALL verify the ECDSA signature using the public key stored in SE050. Verification SHALL use `SE05x_ECDSASignVerify` with constant-time comparison. On failure, the TCU SHALL increment a `auth_fail_counter` in SE050 NV memory and reject the unlock request | TCU (Embedded, SE050) | ASIL B |
| **TSR-01-01-D** | If `auth_fail_counter` exceeds 5 consecutive failures within any 60-second window, the TCU SHALL enter a 300-second lockout period during which no unlock/start attempt is accepted | TCU (Embedded) | ASIL B |

### 5.2 TSR for FSR-01-02 (UWB Distance Verification)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-01-02-A** | The UWB ranging session SHALL use IEEE 802.15.4z High Rate Pulse (HRP) mode with STS. STS key material SHALL be derived from the BLE session master key using HKDF-SHA256 with context `"yuledkcs-uwb-sts-key-v1"` | TCU (NCJ29D6), App | ASIL B(D) |
| **TSR-01-02-B** | The TCU SHALL compute UWB distance using Two-Way Ranging (TWR) with a minimum of 5 round-trip measurements. The median distance SHALL be compared against the unlock threshold (≤2m). Variance >0.3m across the 5 measurements SHALL trigger reliability degradation and request re-ranging | TCU (NCJ29D6 firmware) | ASIL B(D) |
| **TSR-01-02-C** | The TCU SHALL check both UWB ToF distance AND BLE RSSI before unlock. If BLE RSSI indicates signal strength corresponding to >10m but UWB reports <2m, the system SHALL flag a relay attack and reject. The RSSI check SHALL be performed in TCU firmware, not in the BLE controller | TCU (S32G2) | ASIL B(D) |

### 5.3 TSR for FSR-03-01 (STS Mode Enforcement)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-03-01-A** | The UWB driver SHALL configure the NCJ29D6 in STS mode before any ranging session. The configuration SHALL be validated by reading back the register value. Failure to enter STS mode SHALL block ranging — no distance measurement SHALL be accepted with STS disabled | TCU (NCJ29D6 HAL) | ASIL B(D) |
| **TSR-03-01-B** | The BLE driver (KW47A) SHALL establish LE Secure Connections pairing with Numeric Comparison association model before any key exchange or UWB session parameters are transmitted. Legacy pairing (Just Works, Passkey Entry) SHALL be rejected | TCU (KW47A HAL) | ASIL B |

### 5.4 TSR for FSR-04-02 (Sequence Number Protection)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-04-02-A** | The DKCS service SHALL assign a monotonically increasing 64-bit sequence number to each remote command, scoped per {key_id, device_id}. The number SHALL be part of the device-signed command payload | Cloud (DKCS Go) | ASIL A |
| **TSR-04-02-B** | The TCU SHALL maintain a `last_seq` value per key ID in SE050 NV memory. Any command with seq ≤ stored `last_seq` SHALL be rejected as a replay. The SE050 monotonic write SHALL be used to update `last_seq` on each accepted command | TCU (SE050) | ASIL A |

### 5.5 TSR for FSR-06-01 (CRL Management)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-06-01-A** | The CRL SHALL be stored as a sorted array of 16-bit key IDs in SE050 NV memory, supporting up to 64 revoked key entries. Each entry SHALL include a monotonic revocation counter to prevent rollback | TCU (SE050) | ASIL B |
| **TSR-06-01-B** | The MQTT CRL update message SHALL be authenticated with mTLS and SHALL include the DKCS digital signature over the CRL payload. The TCU SHALL verify the DKCS signature using a pre-installed public key before applying the update | TCU, Cloud (DKCS) | ASIL B |
| **TSR-06-01-C** | The TCU SHALL acknowledge each CRL update via MQTT PUBACK with a CRL sequence number. The cloud SHALL retransmit any unacknowledged CRL update up to 3 times with exponential backoff (1s, 3s, 9s) | Cloud (DKCS Go + MQTT) | ASIL B |

### 5.6 TSR for FSR-07-02 (SCP03 Secure Channel)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-07-02-A** | The host MCU SHALL establish an SCP03 session with the SE050 before any key operation. The SCP03 session SHALL use AES-128 for message encryption and CMAC for integrity. Session keys SHALL be derived per the GlobalPlatform Card Specification v2.3.1 | TCU (S32G2 + SE050) | ASIL B |
| **TSR-07-02-B** | If SCP03 session establishment fails (e.g., mutual authentication error, CMAC mismatch), the host SHALL retry at most once. On second failure, the host SHALL log the event to SE050 monotonic counter and enter a degraded mode where no key operation is possible | TCU (S32G2) | ASIL B |
| **TSR-07-02-C** | No cryptographic operation (sign, verify, encrypt, decrypt, key generation) SHALL be performed on the host MCU with SE050 key material. All such operations SHALL be executed inside the SE050 via SCP03-secured APDU commands | TCU (S32G2 + SE050) | ASIL B |

### 5.7 TSR for FSR-08-01 (Multi-Stage Secure Boot)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-08-01-A** | Boot ROM (mask ROM, immutable) SHALL compute SHA-256 of the BootLoader image and compare against eFuse-burned expected hash. Match failure SHALL halt the boot process. The comparison SHALL use constant-time memcmp | TCU (Boot ROM) | ASIL B(D) |
| **TSR-08-01-B** | The BootLoader SHALL load the TFM image from OSPI flash, compute SHA-256, and submit both the hash and the TFM ECDSA signature to the SE050 for verification using `SE05x_ECDSASignVerify` with the boot public key (Key ID 0x0001). Verification failure SHALL halt | TCU (BootLoader SE050) | ASIL B(D) |
| **TSR-08-01-C** | The TFM SHALL verify the Application firmware image using the same SE050 signature verification process. Additionally, the TFM SHALL perform a platform attestation (PSA Certified level 2) including measurement of boot state, and SHALL submit this attestation to the cloud on first MQTT connect | TCU (TFM) | ASIL B(D) |

### 5.8 TSR for FSR-09-01 (Monotonic Version Counters)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-09-01-A** | Version counters SHALL be stored in SE050 monotonic NV memory locations: Counter ID 0xE0 (BootLoader, 8-bit), 0xE1 (TFM, 16-bit), 0xE2 (Application, 32-bit). Write operations SHALL require SCP03-authenticated session | TCU (SE050) | ASIL B |
| **TSR-09-01-B** | The OTA update process SHALL: (1) verify the new firmware signature (multi-sig), (2) compare version >= stored counter, (3) write the new firmware, (4) atomically update the SE050 version counter, (5) reboot. Failure at any step SHALL roll back to the previous firmware and SHALL NOT increment the counter | TCU, Cloud (DKCS) | ASIL B |

### 5.9 TSR for FSR-10-01 (SE050-Attested Identity)

| ID | Requirement | Allocated To | ASIL |
|:---|:------------|:-------------|:----:|
| **TSR-10-01-A** | During provisioning, the TCU SHALL call `SE05x_GetSignedIdentification` to obtain an attestation token signed by the SE050's on-chip attestation private key. The token SHALL include: SE050 unquie ID, IC-level certificate chain, and the VIN provided by the host MCU as attestation data | TCU (SE050) | ASIL B |
| **TSR-10-01-B** | The DKCS SHALL validate the attestation token against the NXP SE050 CA root certificate. VIN binding SHALL be stored in cloud DB as an attested {VIN, SE050_ID, device_public_key} tuple. Any provisioning request with an attestation validation failure SHALL be rejected | Cloud (DKCS Go) | ASIL B |

---

## 6. Safety Mechanisms

### 6.1 Replay Attack Protection

| Mechanism | Coverage | Implementation | Verification |
|:----------|:---------|:---------------|:-------------|
| **Nonce-based challenge-response** | BLE/NFC unlock & start | 128-bit TRNG nonce per session; ECDSA P-256 response signed by device | Nonce uniqueness verified; replay test with captured auth |
| **Monotonic sequence numbers** | Remote commands (MQTT/gRPC) | 64-bit seq per key pair; SE050 stored `last_seq` | Replay identical seq → rejection; seq rollover detection |
| **Timestamp window** | All authenticated messages | ±500ms clock tolerance window (NTP-synced cloud, SE050 RTC on TCU) | Inject messages with old timestamps → rejection |
| **Session key ephemerality** | BLE connection | ECDHE per-connection key agreement; HKDF-SHA256 session key derivation with nonce | Verify forward secrecy: compromise session key does not reveal past |

### 6.2 Key Isolation and Partitioning

| Mechanism | Coverage | Implementation | Verification |
|:----------|:---------|:---------------|:-------------|
| **SE050 key compartmentalization** | RK → MK → DK hierarchy | Keys stored in SE050 by Key ID range; device keys at 0x0100-0x02FF | Extract test: verify no key cross-read possible |
| **HKDF domain separation** | Key derivation across protocols | Context strings: `"yuledkcs-ccckey-v1"`, `"yuledkcs-iccoakey-v1"`, `"yuledkcs-iccekey-v1"` | Verify CCC-derived key cannot be used for ICCE func |
| **Mobile hardware isolation** | App key on phone | iOS: Secure Enclave ECDSA keys; Android: StrongBox-backed KeyStore with isStrongBoxBacked=true | Verify key not extractable from app sandbox |
| **Cloud HSM wrapping** | Key material in cloud DB | AES-256-GCM wrapping key in HSM; KMS CMK → Customer DEK → Column-specific keys | Penetration test: DB dump reveals only ciphertext |

### 6.3 Secure Storage

| Mechanism | Coverage | Implementation | Verification |
|:----------|:---------|:---------------|:-------------|
| **SE050 persistent key storage** | Embedded keys (RK, MK, DKs) | Keys never exit SE050 in plaintext; wrapped with on-chip key tree | Tamper test: bus probe reveals encrypted objects only |
| **SCP03 channel** | Host ↔ SE050 communication | GlobalPlatform SCP03: AES-128 encryption + CMAC integrity + mutual auth | Channel MITM injection test → CMAC mismatch detected |
| **SE050 monotonic NV counters** | Version rollback, auth failures, CRL | Counter storage with write-only-increment semantics; read/write via authenticated SCP03 | Rollback test: attempt to decrement counter → blocked |
| **Mobile secure storage** | App keys on phone | iOS Keychain (kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly); Android EncryptedSharedPreferences + Android Keystore | Jailbreak/root test: key data not accessible outside app |
| **Cloud DB field-level encryption** | PII and key blobs in PostgreSQL | AES-256-GCM per-column; column keys HKDF-derived from HSM-backed DEK | DB query test: encrypted column returns opaque binary |

### 6.4 Communication Security

| Mechanism | Coverage | Implementation | Verification |
|:----------|:---------|:---------------|:-------------|
| **TLS 1.3 + mTLS** | Cloud ↔ TCU, Cloud ↔ HUB, HUB ↔ DKCS | TLS 1.3 with X25519 + AES-256-GCM; mTLS for inter-service; cert max 90d validity | SSL/TLS scan: verify no downgrade to <1.3 |
| **BLE LE Secure Connections** | Mobile ↔ TCU BLE | LE SC with Numeric Comparison; LTK stored in Secure Enclave / SE050 | BLE sniffer test: verify encrypted connection, no legacy fallback |
| **UWB STS** | Mobile ↔ TCU UWB ranging | PHY-level STS with STS key derived from BLE session key | UWB signal injection test: without STS key, range rejected |
| **NFC SCP03** | Mobile ↔ TCU NFC | ISO 7816-4 Secure Channel; ECDH session key agreement; AES-256-GCM encrypted APDUs | NFC proxy test: verify APDU encryption prevents command injection |
| **Certificate pinning** | App ↔ Cloud | Public key pinning (iOS TrustKit, Android OkHttp certificate pinner) | MITM proxy test: pin mismatch → connection refused |

### 6.5 Secure Boot Chain

| Mechanism | Coverage | Implementation | Verification |
|:----------|:---------|:---------------|:-------------|
| **Boot ROM integrity** | TCU bootstrap | Mask ROM; hash comparison with eFuse burned value; constant-time | Fault injection test: boot with corrupted BootLoader → halt |
| **BootLoader signature verification** | SE050-anchored | SE050 `SE05x_ECDSASignVerify` with Key ID 0x0001 | Signature corruption test: modified image → halt |
| **TFM attestation** | Trusted Firmware-M | PSA Level 2; boot measurement; cloud-level attestation on MQTT connect | Attestation tamper test: modified measurement → cloud alert |
| **Application verification** | yuleDKCS firmware | Multi-stage: SHA-256 hash + ECDSA signature + SE050 attestation | Rollback test: older version → version counter mismatch → halt |
| **Secure recovery mode** | Boot failure fallback | CAN bus disabled; LED fault indicator; only signed recovery image accepted via authenticated debug interface | Trigger failure → verify vehicle disabled and recovery path authenticated |

### 6.6 SE050-Specific Safety Mechanisms

| Mechanism | Description | Verification |
|:----------|:------------|:-------------|
| **Hardware tamper mesh** | Active shield over SE050 die; voltage glitch detection (±0.3V tolerance); temperature monitor (-40°C to +125°C); light sensor detection | Physical attack test: mesh breach → key erasure |
| **TRNG health monitoring** | Continuous entropy tests (SP 800-90B); failure → fallback to deterministic generator with alarm | Entropy source test: verify NIST SP 800-22 compliance |
| **Secure debug** | Debug port requires signed authorization token; no backdoor access | Debug port test: unauthorized attach → locked |
| **Key lifecycle management** | SE050-enforced key state machine (Pending → Active → Expired → Revoked → Deleted) | State machine fault test: attempt use in non-Active state → blocked |

### 6.7 Fault Detection and Response

| Fault Type | Detection Method | Response | FDTI | ASIL |
|:-----------|:-----------------|:---------|:----:|:----:|
| SE050 communication failure | SCP03 session timeout (500ms); watchdog monitoring | Enter degraded mode; log fault; no crypto ops permitted | 500ms | B |
| UWB ranging failure | ToF measurement timeout (200ms); quality metric degradation | Fall back to BLE RSSI-only with explicit user button confirmation; log event | 500ms | B(D) |
| BLE connection drop | LE Link Layer supervision timeout (default: 6× conn_interval) | Lock doors if vehicle was unlocked; abort in-progress start; no safety cut of running engine | 1s | B |
| MQTT heartbeat loss | Keep-alive mechanism (60s interval) | TCU uses last known CRL; reject new shared key operations after 24h without cloud contact | 60s | B |
| Clock drift (NTP unavailable) | SE050 RTC vs BLE timestamp delta exceeding ±2s | Reject time-sensitive operations; log clock fault | 1s | A |
| Authentication failure storm | `auth_fail_counter` > 5 per 60s window | 300-second lockout; log to SE050 monotonic counter; optionally notify cloud | 60s | B |

---

## 7. Safety Architecture

### 7.1 Safety Element Allocation

```
┌─────────────────────────────────────────────────────────────────────┐
│                          Cloud Safety Domain                         │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  DKCS Core (Go) — ASIL B elements:                          │    │
│  │  • KeyService: key CRUD, lifecycle state machine            │    │
│  │  • CommandService: remote cmd seq number, signature verify  │    │
│  │  • RevocationService: CRL generation, push, TTL enforcement │    │
│  │  • IdentityService: attestation validate, VIN binding       │    │
│  │  – HSM for key wrapping (FIPS 140-2 Level 3)               │    │
│  └─────────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  HUB Gateway (Go) — ASIL A elements:                        │    │
│  │  • TLS 1.3 termination, mTLS for inter-service              │    │
│  │  • JWT validation, rate limiting, request audit             │    │
│  └─────────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Java Adapters — QM (no safety function)                    │    │
│  │  • CCC/ICCOA/ICCE protocol translation                     │    │
│  │  • Vendor TSP API integration                              │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
                              ▲
                              │ gRPC/mTLS
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                  Embedded Safety Domain (TCU)                        │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Secure Boot Layer (S32G2/G3) — ASIL B(D)                   │    │
│  │  • Boot ROM → BootLoader → TFM → Application               │    │
│  │  • SE050-anchored signature verification                    │    │
│  │  • Monotonic version counters (SE050 NV)                    │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Protocol Stack Layer — ASIL B                               │    │
│  │  • ICCE/CCC/ICCOA unified protocol handling                 │    │
│  │  • Challenge-response auth, nonce management                │    │
│  │  • Permission token verification                            │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Ranging & Presence Layer — ASIL B(D)                       │    │
│  │  • UWB STS ranging (NCJ29D6)                                │    │
│  │  • BLE RSSI cross-check (KW47A)                             │    │
│  │  • Distance threshold enforcement                           │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Secure Element (SE050) — ASIL B, EAL 6+                    │    │
│  │  • Key storage: RK, MK, DKs in hardware-isolated slots      │    │
│  │  • Crypto ops: ECDSA P-256 sign/verify, AES-256-GCM         │    │
│  │  • SCP03 secure channel with host                           │    │
│  │  • NV monotonic counters (version, auth fail, CRL)          │    │
│  │  • TRNG                                                      │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
                              ▲
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                  Mobile Safety Domain (App)                          │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  iOS App — ASIL B (safety related)                          │    │
│  │  • Secure Enclave: ECDSA P-256 private key storage          │    │
│  │  • Keychain: session tokens, permission blobs               │    │
│  │  • TrustKit: public key pinning for API Gateway             │    │
│  │  • NEARBY_INTERACTION: UWB ranging with TCU                │    │
│  └─────────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Android App — ASIL B (safety related)                      │    │
│  │  • StrongBox/Android Keystore: ECDSA P-256 key generation   │    │
│  │  • EncryptedSharedPreferences: permission cache             │    │
│  │  • OkHttp certificate pinner                                │    │
│  │  • UWB API (> Android 12): FiRa compliant ranging          │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.2 Freedom From Interference (FFI)

| Non-Safety Element | Safety Element | Interference Type | Mitigation |
|:-------------------|:---------------|:------------------|:-----------|
| Java Adapters (QM) | DKCS Core (B) | Timing: adapter latency spikes cause command timeout | gRPC timeout (500ms) + circuit breaker; adapter health check separate transport |
| Cloud Logging (QM) | DKCS Core (B) | Memory: log overflow exhausts service memory | Structured rate-limited logging; log buffer ≤10% of heap |
| App UI (QM) | App Security Layer (B) | Data corruption: UI passes malformed auth request | Input validation at API layer boundary; protobuf schema enforcement |

### 7.3 Safe State Definitions

| Safety Goal | Safe State | Entry Condition | Exit Condition |
|:------------|:-----------|:----------------|:---------------|
| SG-01, SG-03 | Doors locked, access denied | Auth failure, relay detection, SE050 comm failure | Manual key + authenticated session re-establish |
| SG-02 | Ignition disabled, fuel/electric cut | Start auth failure, presence fail | Authenticated key presence + SE050 re-attest |
| SG-06 | Revoked key rejected | CRL entry match at auth | CRL removal by owner via cloud |
| SG-08 | Secure recovery mode, vehicle disabled | Boot verification failure | Only signed-recovery image via authenticated debug |
| SG-03 (relay) | Lockout: 300s no access | 5 consecutive auth failures in 60s | Timer expiry OR owner's master key authentication |

---

## 8. Dependent Failure Analysis (DFA)

### 8.1 Common Cause Failure Analysis

| CCF ID | Mechanism | Affected Elements | Mitigation |
|:-------|:----------|:------------------|:-----------|
| CCF-01 | Power supply failure | S32G2 + SE050 + NCJ29D6 + KW47A | Redundant voltage regulators; brownout detection; graceful shutdown with door state retention |
| CCF-02 | Clock source failure | Challenge-response timing, UWB RTT | Independent oscillators: SE050 internal RTC + S32G2 external XTAL + NCJ29D6 internal RC |
| CCF-03 | SE050 firmware corruption | All key/crypto operations | SE050 ROM is immutable; SCP03 prevents unauthorized updates to SE050 applet |
| CCF-04 | BLE firmware stack crash | BLE auth + data channel | Watchdog timer on KW47A; independent BLE controller crash does not affect UWB/NFC path |
| CCF-05 | Temperature extreme | All embedded components | SE050 rated -40°C to +125°C; S32G2 -40°C to +125°C; thermal throttling with safety margin |

### 8.2 Cascading Failure Analysis

| Cascade Scenario | Root Failure | Cascaded Effect | Mitigation |
|:-----------------|:-------------|:----------------|:-----------|
| Cloud DKCS outage | DKCS Go crash | No remote command processing, no CRL push | TCU degrades to last-known CRL; owner's primary keys still accepted for 24h; shared/guest keys rejected |
| MQTT connection loss | TCU cellular module failure | No cloud commands, no CRL updates | BLE/UWB/NFC local auth continues; CRL cached in SE050; engine start with owner key still functional |
| SE050 SCP03 failure | Host MCU I2C corruption | No crypto ops possible | TCU enters degraded mode: door locks remain mechanical; engine start disabled; recovery via secure boot |

---

## 9. Safety Case Summary

### 9.1 Argument Structure

```
G1: The yuleDKCS digital key system achieves ASIL-B(D) functional safety
│
├── G1.1: Hazards are adequately identified and classified [HARA]
│   └── Evidence: H-01 through H-10, ASIL assignment
│
├── G1.2: Safety Goals cover all identified hazards [SG]
│   └── Evidence: SG-01 through SG-10, traceability matrix
│
├── G1.3: Safety requirements are sufficient and correct [FSR + TSR]
│   └── Evidence: 25+ FSRs, 20+ TSRs with ASIL allocation
│
├── G1.4: Safety mechanisms provide adequate fault coverage [Mechanisms]
│   ├── G1.4.1: Replay protection via nonce + seq + timestamp
│   ├── G1.4.2: Key isolation via SE050 compartment + HKDF separation
│   ├── G1.4.3: Secure storage via SE050 HW + SCP03 + HSM
│   ├── G1.4.4: Communication security via TLS 1.3 + mTLS + LE SC + SCP03
│   └── G1.4.5: Secure boot via SE050 chain + monotonic counters
│
└── G1.5: Freedom from interference between safety and non-safety elements
    └── Evidence: FFI analysis, CCF/DFA
```

### 9.2 Assumptions and Dependencies

- SE050 provides EAL 6+ assurance for hardware trust anchor
- Mobile TEE (Secure Enclave/StrongBox) provides adequate isolation
- OEM PKI infrastructure is properly managed
- UWB operates per IEEE 802.15.4z with mandatory STS

### 9.3 Residual Risks

1. **Physical side-channel attacks on SE050**: While SE050 is EAL 6+ hardened, demonstrated lab attacks (DPA, EM) exist. Mitigation: key lifecycle limits exposure time; SE050 next-gen mitigations tracked.
2. **Mobile OS compromise with TEE bypass**: Root/jailbreak with TEE-level exploit could extract keys. Mitigation: runtime integrity checks; cloud-side anomaly detection on key usage patterns.
3. **Quantum computing threat**: ECDSA P-256 is not quantum-resistant. Mitigation: key design allows algorithm migration; post-quantum crypto (CRYSTALS-Dilithium, FALCON) mapped for v2.0.

---

## 10. Glossary

| Term | Definition |
|:-----|:-----------|
| **ASIL** | Automotive Safety Integrity Level (ISO 26262): A/B/C/D |
| **ASIL B(D)** | ASIL B with ASIL D systematic integrity requirements (for secure boot integrity) |
| **CRL** | Certificate Revocation List |
| **DFA** | Dependent Failure Analysis (ISO 26262-9:2018 §7) |
| **DKCS** | Digital Key Control Service (cloud core service) |
| **EAL** | Evaluation Assurance Level (Common Criteria) |
| **FDTI** | Fault Detection Time Interval |
| **FFI** | Freedom From Interference (ISO 26262-9:2018 §6) |
| **FSR** | Functional Safety Requirement |
| **FTTI** | Fault-Tolerant Time Interval |
| **HARA** | Hazard Analysis and Risk Assessment (ISO 26262-3:2018 §7) |
| **HSM** | Hardware Security Module (cloud-side) |
| **ICCE** | Intelligent Connected Car Cybersecurity standard (T/CA 110-2020) |
| **ICCOA** | Intelligent Cockpit & Connectivity Alliance Digital Key 3.0/4.0 |
| **mTLS** | Mutual TLS (two-way certificate authentication) |
| **NV Counter** | Non-Volatile monotonic counter (SE050) |
| **SCP03** | Secure Channel Protocol 03 (GlobalPlatform) |
| **SE050** | NXP Secure Element (EAL 6+, Common Criteria certified) |
| **SG** | Safety Goal (ISO 26262-3) |
| **STS** | Scrambled Timestamp Sequence (UWB IEEE 802.15.4z) |
| **TCU** | Telematic Control Unit (vehicle-side embedded system) |
| **TFM** | Trusted Firmware-M (ARM platform security architecture) |
| **ToF** | Time of Flight (UWB ranging) |
| **TRNG** | True Random Number Generator (SE050 internal) |
| **TSR** | Technical Safety Requirement |
| **TWR** | Two-Way Ranging (UWB distance measurement) |
| **VIN** | Vehicle Identification Number |

---

## References

| Document | Description |
|:---------|:------------|
| [SECURITY_WHITEPAPER.md](SECURITY_WHITEPAPER.md) | End-to-end security architecture, threat model, key hierarchy |
| [SECURITY_GUIDE.md](SECURITY_GUIDE.md) | Operational security configuration guide |
| [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md) | System-level architecture and data flow |
| ISO 26262:2018 Parts 1-12 | Road vehicles — Functional safety |
| ISO 21434:2021 | Road vehicles — Cybersecurity engineering |
| CCC Digital Key 3.0 | Car Connectivity Consortium specification |
| ICCOA DK 3.0/4.0 | ICCOA Digital Key specification |
| ICCE T/CA 110-2020 | Intelligent Connected Car Cybersecurity standard |
| NXP SE050 Datasheet | SE050 secure element capabilities and security features |
| GlobalPlatform Card Spec v2.3.1 | SCP03 secure channel protocol |

---

*© 2026 yuleDKCS. This document defines the functional safety concept for the yuleDKCS Digital Key System. For the latest version, refer to the project repository.*
