# 🔐 yuleDKCS Security White Paper

> **Edition**: Community + Enterprise Architecture Framework  
> **Target Audience**: Automotive OEM Security Teams, Tier-1 Suppliers, Security Auditors  
> **Version**: 1.0.0  
> **Date**: 2026-06-01  
> **Classification**: Public — For customer and partner review

---

## Executive Summary

yuleDKCS is a next-generation digital key platform that enables **phone-as-key** functionality for connected vehicles. This white paper describes the end-to-end security architecture designed to meet the rigorous security requirements of automotive OEMs.

**Security by design** — every layer of the system, from the secure element (SE050) embedded in the vehicle to the cloud APIs and mobile SDKs, is architected with defense-in-depth principles.

### Key Security Properties

| Property | Mechanism | Certification Target |
|----------|-----------|---------------------|
| **Authentication** | Mutual TLS + ECDSA P-256 + JWT | FIPS 140-3 Level 2 |
| **Confidentiality** | AES-256-GCM (data) + TLS 1.3 (transport) | ISO 21434 |
| **Integrity** | Secure boot chain + HKDF key derivation | EVITA Full |
| **Availability** | Rate limiting + anomaly detection | N/A |
| **Non-repudiation** | Audit logging + digital signatures | SOC 2 Type II |
| **Privacy** | Field-level encryption + data minimization | GDPR / PIPL |

---

## Table of Contents

1. [System Overview and Threat Model](#1-system-overview-and-threat-model)
2. [End-to-End Security Architecture](#2-end-to-end-security-architecture)
3. [Key Hierarchy and Management](#3-key-hierarchy-and-management)
4. [Secure Boot Chain](#4-secure-boot-chain)
5. [Communication Security](#5-communication-security)
6. [Hardware Security Module (SE050)](#6-hardware-security-module-se050)
7. [Cloud Security Architecture](#7-cloud-security-architecture)
8. [Mobile Security Architecture](#8-mobile-security-architecture)
9. [Threat Model and Mitigations](#9-threat-model-and-mitigations)
10. [Compliance and Certifications](#10-compliance-and-certifications)
11. [Security Incident Response](#11-security-incident-response)
12. [Security Audit Checklist](#12-security-audit-checklist)
13. [Appendix](#13-appendix)

---

## 1. System Overview and Threat Model

### 1.1 System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ATTACKER LANDSCAPE                           │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐           │
│   │ Physical │  │ Network  │  │ Software │  │ Side-    │           │
│   │ Access   │  │ Attacks  │  │ Exploits │  │ Channel  │           │
│   └──────────┘  └──────────┘  └──────────┘  └──────────┘           │
└─────────────────────────────────────────────────────────────────────┘
                                    │
┌─────────────────────────────────────────────────────────────────────┐
│                         DEFENSE LAYERS                               │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    Application Security                      │    │
│  │   JWT Auth │ RBAC │ Field Encryption │ Audit Trail          │    │
│  ├─────────────────────────────────────────────────────────────┤    │
│  │                    Communication Security                   │    │
│  │   TLS 1.3 │ mTLS │ Certificate Pinning │ OCSP Stapling      │    │
│  ├─────────────────────────────────────────────────────────────┤    │
│  │                    Protocol Security                         │    │
│  │   BER-TLV + AES-256-GCM │ UWB Secure Ranging │ NFC Secure Ch│    │
│  ├─────────────────────────────────────────────────────────────┤    │
│  │                    Platform Security                         │    │
│  │   Secure Boot │ TFM │ Signed Updates │ Kernel Hardening     │    │
│  ├─────────────────────────────────────────────────────────────┤    │
│  │                    Hardware Security                         │    │
│  │   NXP SE050 │ Secure Element │ eFuse │ Tamper Detection     │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Trust Boundaries

| Boundary | Description | Trust Level | Security Controls |
|-----------|-------------|-------------|-------------------|
| **T1** | SE050 ↔ TCU SoC | **T** (Trusted) | Internal bus, physical security |
| **T2** | TCU ↔ Cloud (mTLS) | **P** (Protected) | Mutual TLS, certificate validation |
| **T3** | Cloud ↔ Mobile App (TLS) | **P** (Protected) | TLS 1.3, cert pinning |
| **T4** | Mobile App ↔ TCU (BLE/UWB/NFC) | **U** (Untrusted) | Cryptographic challenge-response |
| **T5** | Cloud ↔ TSP/Vendor Backend | **P** (Protected) | mTLS, API key, rate limiting |

### 1.3 Assets Under Protection

| Asset | Location | Sensitivity | Impact if Compromised |
|-------|----------|-------------|----------------------|
| Root Key (RK) | SE050 (hardware) | 🔴 Critical | Full system compromise |
| Master Key (MK) | SE050 (hardware) | 🔴 Critical | All device keys compromised |
| Device Private Key | SE050 / Secure Enclave | 🟠 High | Specific device compromise |
| Session Key | RAM (volatile) | 🟡 Medium | Single session compromise |
| User Credentials | Cloud (encrypted) | 🟠 High | Account takeover |
| Vehicle Control Commands | In transit | 🟡 Medium | Unauthorized vehicle access |

---

## 2. End-to-End Security Architecture

### 2.1 Authentication Flow

```
┌──────────┐          ┌──────────┐          ┌──────────┐          ┌──────────┐
│  Mobile  │          │  Cloud   │          │   Hub    │          │   TCU    │
│   App    │          │  API GW  │          │  Service │          │ (Vehicle)│
└────┬─────┘          └────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │                     │
     │ 1. Auth Request     │                     │                     │
     │  (JWT Bearer)──────►│                     │                     │
     │                     │                     │                     │
     │                     │ 2. Validate JWT     │                     │
     │                     │  (RS256, exp, iss)  │                     │
     │                     │                     │                     │
     │                     │ 3. Forward Request  │                     │
     │                     │  (mTLS)────────────►│                     │
     │                     │                     │                     │
     │                     │                     │ 4. Vehicle Command  │
     │                     │                     │  (mTLS + Signed)───►│
     │                     │                     │                     │
     │                     │                     │                     │ 5. SE050 Auth
     │                     │                     │                     │  (ECDSA Verify)
     │                     │                     │                     │
     │                     │                     │ 6. Auth Response    │
     │                     │                     │◄────────────────────│
     │                     │                     │                     │
     │                     │ 7. Command Response │                     │
     │                     │◄────────────────────│                     │
     │                     │                     │                     │
     │ 8. Result           │                     │                     │
     │◄────────────────────│                     │                     │
     │                     │                     │                     │
```

### 2.2 Key Provisioning Flow

```
┌──────────┐         ┌──────────┐         ┌──────────┐        ┌──────────┐
│   OEM    │         │   DKCS   │         │   Hub    │        │  Mobile  │
│  Backend │         │  Service │         │  Service │        │  Vendor  │
└────┬─────┘         └────┬─────┘         └────┬─────┘        └────┬─────┘
     │                    │                    │                    │
     │ 1. Bind Request   │                    │                    │
     │  (mTLS)──────────►│                    │                    │
     │                    │                    │                    │
     │                    │ 2. Validate       │                    │
     │                    │  - Vehicle Owner  │                    │
     │                    │  - Key Quota      │                    │
     │                    │  - Role/Perms     │                    │
     │                    │                    │                    │
     │                    │ 3. Generate Key   │                    │
     │                    │  (SE050 or HSM)   │                    │
     │                    │                    │                    │
     │                    │ 4. Forward Request│                    │
     │                    │  (gRPC)──────────►│                    │
     │                    │                    │                    │
     │                    │                    │ 5. Vendor API Call │
     │                    │                    │  (mTLS + Signed)──►│
     │                    │                    │                    │
     │                    │                    │ 6. Push Notification│
     │                    │                    │◄───────────────────│
     │                    │                    │                    │
     │                    │ 7. MQTT to TCU    │                    │
     │                    │  (Key Material)───►│                    │
     │                    │                    │                    │
     │ 8. Response        │                    │                    │
     │◄───────────────────│                    │                    │
     │                    │                    │                    │
```

---

## 3. Key Hierarchy and Management

### 3.1 Key Hierarchy Structure

```
┌─────────────────────────────────────────────────────────────┐
│                    ROOT KEY (RK)                              │
│  • Injected at SE050 manufacturing (NXP secure facility)     │
│  • 256-bit AES, unique per device, never exported            │
│  • Protected by hardware anti-tamper                         │
│  • Lifetime: hardware lifetime (never rotated)               │
└────────────────────┬─────────────────────────────────────────┘
                     │ Derivation (HKDF-SHA256, key ladder)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                    MASTER KEY (MK)                            │
│  • Derived from RK using HKDF-SHA256                         │
│  • Stored in SE050 persistent memory (wrapped by RK)         │
│  • Rotation: yearly                                          │
│  • Used for device key derivation                            │
└────────────────────┬─────────────────────────────────────────┘
                     │ Derivation (HKDF-SHA256 + device context)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                    DEVICE KEYS (DK1..DKn)                     │
│  • One per authorized device (phone, wearable, etc.)         │
│  • 256-bit AES + ECDSA P-256 key pair                        │
│  • Stored: private key in SE050, public key stored          │
│    in cloud (wrapped)                                        │
│  • Rotation: device unbind/rebind triggers new key           │
│  • Can be individually revoked without affecting others      │
├─────────────────────────────────────────────────────────────┤
│                    SHARED KEYS (SK1..SKm)                     │
│  • Short-lived keys for guest access (family, valet)         │
│  • Limited permissions and geo/temporal constraints          │
│  • Expiration enforced by SE050                              │
└────────────────────┬─────────────────────────────────────────┘
                     │ Derivation (per-session ECDH)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                    SESSION KEYS (SSK1..SSKn)                  │
│  • Ephemeral, generated per BLE/vehicle session              │
│  • Derived via ECDHE key exchange (P-256)                    │
│  • Used for AES-256-GCM encrypted messaging                  │
│  • Lifetime: single session (cleared on disconnect)          │
│  • Forward secrecy guaranteed (not derived from long-term)   │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 Key Derivation

| Key | Algorithm | Context | Entropy Source |
|-----|-----------|---------|----------------|
| RK → MK | HKDF-SHA256 | `"yuledkcs-master-key-v1"` | SE050 TRNG |
| MK → DK | HKDF-SHA256 | `"yuledkcs-device-key-" + deviceID` | SE050 TRNG |
| ECDHE → SSK | HKDF-SHA256 | `"yuledkcs-session-key-" + nonce` | Both parties |
| MK → Sharing Key | HKDF-SHA256 | `"yuledkcs-share-key-" + params` | SE050 TRNG |

### 3.3 Key Lifecycle

```
┌────────┐     ┌────────┐     ┌────────┐     ┌────────┐     ┌────────┐
│ Pending│────►│ Active │────►│Revoking│────►│Revoked │────►│Deleted │
│        │     │        │     │        │     │        │     │        │
│   -    │     │ Normal │     │ Grace  │     │ No use │     │ Removed│
└────────┘     └────────┘     └────────┘     └────────┘     └────────┘
                    │
                    ▼
               ┌────────┐
               │Expired │
               │        │
               │ Auto   │
               │ invalid│
               └────────┘
```

| State | Description | Operations Allowed |
|-------|-------------|-------------------|
| Pending | Key created, awaiting activation | None |
| Active | Fully operational | All permitted operations |
| Expired | Past expiration timestamp | None (auto-revoke) |
| Revoking | Grace period before full revoke | Read-only |
| Revoked | No longer valid | None |
| Deleted | Removed from storage | N/A |

### 3.4 Key Rotation Policy

| Key Type | Rotation Interval | Method | Downtime |
|----------|-------------------|--------|----------|
| Root Key | Never (hardware lifetime) | N/A | N/A |
| Master Key | 12 months | HKDF re-derive | < 1s (hot reload) |
| Device Key | On device rebind | New ECDSA key pair | Device-specific |
| Session Key | Per connection | ECDHE handshake | None |
| Sharing Key | Per share event | Fresh HKDF derive | None |

---

## 4. Secure Boot Chain

### 4.1 Boot Sequence

```
┌────────────────────────────────────────────────────────────────┐
│  HARDWARE ANCHOR OF TRUST                                      │
│                                                                │
│  Boot ROM (Mask ROM, immutable)                                │
│     │  Verify BootROM checksum                                 │
│     │  Hash = SHA256(BootROM)                                  │
│     │  Compare with eFuse burned value                         │
│     │  If mismatch → HALT                                      │
│     ▼                                                          │
│  Boot ROM → BootLoader                                         │
│     │  Read BootLoader from OSPI flash                         │
│     │  SE050: Verify ECDSA signature                           │
│     │  Public key: stored in SE050 (Key ID 0x0001)             │
│     │  If invalid → HALT                                       │
│     ▼                                                          │
│  BootLoader → TFM (Trusted Firmware-M)                         │
│     │  Verify TFM image signature                              │
│     │  TFM: Platform attestation (PSA Certified)               │
│     │  SE050: Measure boot state                               │
│     ▼                                                          │
│  TFM → Application (yuleDKCS Firmware)                         │
│     │  Multi-stage verification:                               │
│     │  1. Verify application image signature (ECDSA P-256)     │
│     │  2. Verify application hash (SHA256)                     │
│     │  3. SE050 attestation (nonce + measurement)              │
│     │  4. Application starts with attested environment         │
│     ▼                                                          │
│  Application Runtime                                            │
│     │  SE050 secure channel established                        │
│     │  Keys accessible via Key ID (never in plaintext)         │
│     │  TFM monitors runtime integrity                          │
│     │  Periodic attestation to cloud                           │
└────────────────────────────────────────────────────────────────┘
```

### 4.2 Signature Verification Detail

```c
/**
 * Secure boot image verification.
 * Executed in TFM context, calls SE050 for hardware-backed crypto.
 */
typedef struct {
    uint8_t  image_hash[32];      // SHA-256 of boot image
    uint8_t  signature[64];       // ECDSA P-256 signature
    uint32_t image_size;          // Image size in bytes
    uint8_t  version;             // Image version for rollback protection
} boot_image_header_t;

secure_boot_status_t verify_boot_image(
    const uint8_t *image,
    size_t         image_len,
    const uint8_t *expected_hash
) {
    uint8_t computed_hash[32];

    // 1. Compute SHA-256 hash
    crypto_sha256(image, image_len, computed_hash);

    // 2. Constant-time comparison
    if (crypto_memcmp(computed_hash, expected_hash, 32) != 0) {
        return SECURE_BOOT_HASH_MISMATCH;
    }

    // 3. SE050 signature verification
    // Key ID 0x0001 = Boot public key (burned in SE050)
    se05x_status_t status = SE05x_ECDSASignVerify(
        KEY_ID_BOOT_PUBLIC,
        kSE05x_Algorithm_ECDSA_SHA256,
        computed_hash, 32,
        signature, 64
    );

    return (status == kSE05x_Status_Success)
        ? SECURE_BOOT_SUCCESS
        : SECURE_BOOT_SIGNATURE_INVALID;
}
```

### 4.3 Anti-Rollback Protection

| Component | Version Counter | Storage | Rollback Action |
|-----------|-----------------|---------|-----------------|
| BootLoader | 8-bit monotonic | SE050 NV counter | HALT on rollback |
| TFM | 16-bit monotonic | SE050 NV counter | HALT on rollback |
| Application | 32-bit monotonic | SE050 NV counter | HALT + notification |
| Key derivation | N/A (derived) | SE050 NV counter | Re-derive on update |

### 4.4 Runtime Integrity Monitoring

- **TFM periodic checks**: Cryptographic attestation every 30 seconds
- **SE050 tamper detection**: Physical voltage/glitch/temperature monitoring
- **Watchdog**: Hardware watchdog resets if application freezes
- **Secure debug**: Debug interface requires signed authorization (no backdoor)

---

## 5. Communication Security

### 5.1 Channel Security Summary

| Channel | Protocol | Encryption | Authentication | Security Level |
|---------|----------|------------|----------------|----------------|
| App ↔ Cloud API | HTTPS/TLS 1.3 | AES-256-GCM | JWT + mTLS (server) | High |
| Cloud ↔ Hub | gRPC + TLS 1.3 | AEAD | mTLS (mutual) | Critical |
| Hub ↔ TCU | MQTT + TLS 1.3 | AES-256 | Client cert + challenge | Critical |
| App ↔ TCU (BLE) | BLE 5.x + GATT | AES-CCM (LE Secure) | OOB pairing + SC | High |
| App ↔ TCU (UWB) | IEEE 802.15.4z | AES-128 + STS | PHY-level authentication | Critical |
| App ↔ TCU (NFC) | ISO 7816-4 | AES-256-GCM | Secure Channel Protocol | High |
| Cloud ↔ TSP | HTTPS/mTLS | TLS 1.3 | mTLS + API key | High |

### 5.2 BLE Security

```
┌──────────┐                              ┌──────────┐
│  Mobile  │                              │   TCU    │
│    App   │                              │ (Vehicle)│
└────┬─────┘                              └────┬─────┘
     │                                         │
     │ 1. BLE Advertisement                    │
     │    (UUID, Service Data)                 │
     │◄────────────────────────────────────────│
     │                                         │
     │ 2. LE Secure Connections Pairing        │
     │    (Numeric Comparison)                 │
     │◄───────────────────────────────────────►│
     │                                         │
     │ 3. Bond Creation                        │
     │    (LTK stored in Secure Enclave)       │
     │◄───────────────────────────────────────►│
     │                                         │
     │ 4. Encrypted Connection Established     │
     │    (AES-CCM 128-bit)                    │
     │◄═══════════════════════════════════════►│
     │                                         │
     │ 5. Application-level Auth               │
     │    (Challenge-Response via custom GATT) │
     │◄───────────────────────────────────────►│
     │                                         │
```

### 5.3 UWB Secure Ranging

```
┌──────────┐                              ┌──────────┐
│  Mobile  │                              │   TCU    │
│    App   │                              │ (Vehicle)│
└────┬─────┘                              └────┬─────┘
     │                                         │
     │ 1. BLE Connection Established           │
     │◄═══════════════════════════════════════►│
     │                                         │
     │ 2. UWB Session Parameters               │
     │    (STS Config, Channel, PRF)           │
     │◄────────────────────────────────────────│
     │                                         │
     │ 3. UWB Ranging Started                  │
     │    (Two-Way Ranging, STS Packet)        │
     │◄═══════════════════════════════════════►│
     │    ← Poll →                             │
     │    Response →                           │
     │    ← Final (with STS signature)         │
     │                                         │
     │ 4. Distance Computed                    │
     │    (Both sides calculate independently) │
     │                                         │
     │ 5. Application Decision                 │
     │    (Check distance threshold)           │
     │    (Verify STS signature)               │
     │                                         │
```

### 5.4 NFC Secure Channel

```
┌──────────┐                              ┌──────────┐
│  Mobile  │                              │   TCU    │
│    App   │                              │ (Vehicle)│
└────┬─────┘                              └────┬─────┘
     │                                         │
     │ 1. NDEF Detection                       │
     │◄────────────────────────────────────────│
     │                                         │
     │ 2. SELECT AID                           │
     │    (AID = D2760000850101)               │
     │────────────────────────────────────────►│
     │                                         │
     │ 3. Mutual Authentication                │
     │    (ISO 7816-4 Secure Channel)          │
     │◄═══════════════════════════════════════►│
     │                                         │
     │ 4. Session Key Agreement                │
     │    (ECDH, ephemeral keys)               │
     │◄═══════════════════════════════════════►│
     │                                         │
     │ 5. Encrypted APDU Exchange              │
     │    (AES-256-GCM encrypted)              │
     │◄═══════════════════════════════════════►│
     │                                         │
```

---

## 6. Hardware Security Module (SE050)

### 6.1 SE050 Capabilities

The NXP SE050 secure element provides:

| Capability | Specification | Usage in yuleDKCS |
|------------|---------------|-------------------|
| Secure key storage | Up to 100 keys (AES/ECC/RSA) | Root, Master, Device keys |
| Cryptographic engine | AES-128/256, ECC (P-192/256/384/521), RSA 2K-4K | All crypto operations |
| TRNG | True Random Number Generator | Key generation, nonces |
| Genuine platform | IC-level certificate | Device identity attestation |
| Tamper resistance | Active mesh, voltage/glitch/temp | Physical attack protection |
| Secure boot | Boot public key storage | Boot signature verification |
| Key attestation | Sign operation results with attestation key | Remote key verification |
| Secure channel | SCP03 (GlobalPlatform) | Secure communication with host |
| Counter storage | Monotonic counters | Anti-rollback protection |

### 6.2 Key ID Allocation

| Key ID Range | Purpose | Access Control |
|--------------|---------|----------------|
| `0x0001` | Boot Public Key | Read-only (burned) |
| `0x0010` | Master Key | Internal operations only |
| `0x0100 – 0x01FF` | Device Keys (AES) | Per-device access |
| `0x0200 – 0x02FF` | Device Key Pairs (ECC) | Per-device access |
| `0x0300 – 0x03FF` | Shared Keys | Per-share access |
| `0xF000 – 0xFFFF` | System configuration | Restricted |

### 6.3 SE050 Security Properties

```
┌────────────────────────────────────────────┐
│           PHYSICAL ATTACK RESISTANCE         │
│                                              │
│  • Active shield mesh                        │
│  • Voltage glitch detection (0.8V-6.0V)     │
│  • Temperature monitor (-40°C to +125°C)   │
│  • Light attack detection                    │
│  • Laser fault injection countermeasures     │
│  • EM side-channel hardening                 │
│                                              │
│  Certified: CC EAL 6+ (Common Criteria)     │
│  Certificate: BSI-DSZ-CC-XXXX               │
└────────────────────────────────────────────┘
```

### 6.4 HSM for Cloud (Enterprise Edition)

For cloud-side key operations, yuleDKCS Enterprise supports AWS CloudHSM or Azure Key Vault Managed HSM:

- Offloads key generation to FIPS 140-2 Level 3 validated hardware
- Keys never leave the HSM boundary
- All cryptographic operations executed inside HSM
- Audit logging for all key operations

---

## 7. Cloud Security Architecture

### 7.1 Service Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    PUBLIC INTERNET                        │
│                                                          │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐        │
│  │ Mobile   │     │ OEM      │     │ TSP      │        │
│  │ Devices  │     │ Backend  │     │ Backend  │        │
│  └────┬─────┘     └────┬─────┘     └────┬─────┘        │
└───────┼────────────────┼────────────────┼───────────────┘
        │                │                │
┌───────┼────────────────┼────────────────┼───────────────┐
│       │     DMZ / WAF  │                │               │
│  ┌────▼────────────────▼────────────────▼────┐          │
│  │          API GATEWAY (Kong / Envoy)        │          │
│  │   • TLS termination   • Rate limiting      │          │
│  │   • JWT validation    • IP whitelisting    │          │
│  │   • OAuth2 proxy      • Request logging    │          │
│  └────────────────────┬───────────────────────┘          │
│                       │                                  │
│  ┌────────────────────▼───────────────────────┐          │
│  │              HUB SERVICE (Go)               │          │
│  │   • Protocol routing    • Session mgmt     │          │
│  │   • BerTLV codec        • Device registry   │          │
│  │   • gRPC server         • Metrics          │          │
│  └────┬──────────────┬──────────────┬──────────┘          │
│       │              │              │                     │
│  ┌────▼────┐   ┌────▼────┐   ┌────▼────┐                │
│  │  DKCS   │   │  Event  │   │  Cache  │                │
│  │ Service │   │  Stream │   │ (Redis) │                │
│  │ (Go)    │   │ (Kafka) │   │         │                │
│  └────┬────┘   └─────────┘   └─────────┘                │
│       │                                                  │
│  ┌────▼────┐   ┌──────────┐   ┌───────────────────────┐ │
│  │Postgres │   │  HSM /   │   │ TSP Adapters (Java)    │ │
│  │(DB)     │   │  KMS     │   │ • CCC • ICCOA • ICCE   │ │
│  └─────────┘   └──────────┘   └───────────────────────┘ │
│                                                          │
│                 PRIVATE SUBNET (VPC)                     │
└──────────────────────────────────────────────────────────┘
```

### 7.2 Cloud Security Controls

| Control | Implementation | Standard |
|---------|---------------|----------|
| **Identity & access** | RBAC + ABAC (OAuth 2.0 / OIDC) | NIST SP 800-53 |
| **Secrets management** | Vault / AWS Secrets Manager | — |
| **Data encryption at rest** | AES-256 (AWS KMS / Azure Key Vault) | FIPS 140-2 |
| **Data encryption in transit** | TLS 1.3 + mTLS | NIST SP 800-52 |
| **Network security** | VPC, security groups, WAF, IDS/IPS | NIST CSF |
| **Audit logging** | Structured JSON, immutable storage | SOC 2 Type II |
| **Vulnerability scanning** | Weekly automated + quarterly pentest | OWASP Top 10 |
| **Backup & DR** | Multi-AZ, point-in-time recovery, RPO < 1h | ISO 27001 |

### 7.3 Database Encryption

```
┌────────────────────────────────────────────┐
│              DATABASE TABLE                  │
├────────────────────────────────────────────┤
│  Column: user_id (UUID, plaintext)         │
│  Column: phone_hash (BYTEA, SHA-256 + salt)│
│  Column: phone_encrypted (BYTEA, AES-256)  │
│  Column: email_hash (BYTEA, SHA-256 + salt)│
│  Column: email_encrypted (BYTEA, AES-256)  │
│  Column: key_data (BYTEA, AES-256 + KMS)  │
│  Column: created_at (TIMESTAMP, plaintext) │
└────────────────────────────────────────────┘

Encryption key hierarchy:
  KMS CMK → Customer DEK → Column-specific keys (HKDF derived)
```

---

## 8. Mobile Security Architecture

### 8.1 iOS Security

| Security Feature | Implementation |
|------------------|----------------|
| **Key storage** | iOS Keychain (kSecAttrAccessible = kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly) |
| **Biometric auth** | LocalAuthentication (Face ID / Touch ID) |
| **Secure Enclave** | ECDSA private keys stored in Secure Enclave |
| **Certificate pinning** | Public key pinning (TrustKit) |
| **Runtime protection** | Jailbreak detection, debugger detection |
| **Data protection** | NSFileProtectionComplete for local data |
| **App Transport Security** | ATS enforced (no HTTP allowlists) |
| **Background protection** | App lifecycle management, screen recording detection |

### 8.2 Android Security

| Security Feature | Implementation |
|------------------|----------------|
| **Key storage** | Android Keystore (TEE/StrongBox backed) |
| **Biometric auth** | BiometricPrompt (fingerprint / face) |
| **Hardware-backed** | KeyGenParameterSpec with `isStrongBoxBacked = true` |
| **Certificate pinning** | OkHttp certificate pinner |
| **Runtime protection** | Root detection, Frida detection, emulator detection |
| **Data protection** | EncryptedSharedPreferences |
| **Network security** | Network Security Config (debug overrides blocked) |
| **Splash prevention** | FLAG_SECURE on sensitive screens |

### 8.3 Secure Enclave vs StrongBox

| Feature | iOS Secure Enclave | Android StrongBox |
|---------|-------------------|-------------------|
| Hardware | Dedicated ARM core in A-series chip | Dedicated secure element (e.g., Titan M) |
| Key types | ECC P-256, AES-256 | ECC P-256/384/521, RSA, AES |
| Attestation | Apple-signed attestation | Android Key Attestation |
| Biometric binding | Yes | Yes (biometric-specific keys) |
| Rate limiting | Yes (hardware-enforced) | Yes (hardware-enforced) |
| Quantum-safe | No (future: PQC) | No (future: PQC) |

---

## 9. Threat Model and Mitigations

### 9.1 STRIDE Threat Modeling

| Threat | Asset | Attack Vector | Mitigation | Severity |
|--------|-------|---------------|------------|----------|
| **Spoofing** | Device identity | BLE MAC spoofing | UWB secure ranging + challenge-response | 🔴 High |
| **Tampering** | Vehicle commands | MQTT injection | Message signing + sequence numbers | 🔴 High |
| **Repudiation** | Key operations | Missing audit trail | Immutable audit log + digital signatures | 🟡 Medium |
| **Information disclosure** | User data | DB compromise | Field-level encryption + KMS | 🟡 Medium |
| **Denial of service** | Hub service | Connection flood | Rate limiting + WAF + auto-scaling | 🟡 Medium |
| **Elevation of privilege** | Admin API | JWK injection | Input validation + least privilege RBAC | 🔴 High |

### 9.2 Attack Scenarios and Mitigations

#### Scenario 1: Relay Attack

**Threat**: Attacker relays BLE signal to unlock vehicle remotely.

```
Attacker A (near phone) ←→ Attacker B (near car) ←→ Vehicle
```

**Mitigations**:
1. UWB secure ranging provides PHY-level distance bounding (±10cm accuracy)
2. BLE key derivation includes channel parameters
3. Challenge-response with timestamps (max 500ms window)
4. Distance threshold: unlock requires ≤ 2m, engine start ≤ 1m

**Effectiveness**: 🟢 Relay attacks are detected and blocked by IEEE 802.15.4z STS ranging.

#### Scenario 2: Physical SE050 Extraction

**Threat**: Attacker physically extracts SE050 from TCU.

**Mitigations**:
1. SE050 has active tamper mesh — erases keys on intrusion
2. Key encryption uses eFuse-derived wrapping key (chip-specific)
3. TCU casing anti-tamper switch triggers key wipe
4. Cloud detects missing periodic attestation within 30s

**Effectiveness**: 🟢 Keys are unrecoverable after physical intrusion.

#### Scenario 3: OTA Firmware Hijack

**Threat**: Attacker compromises OTA update mechanism.

**Mitigations**:
1. Multi-signature update (at least 2 of 3 OEM/yuleDKCS/auditor keys)
2. SE050-based signature verification in bootloader
3. Version monotonic counter prevents downgrade
4. Staged rollout with canary deployment

**Effectiveness**: 🟢 Firmware hijack requires compromising two independent signing keys and SE050.

#### Scenario 4: API Token Theft

**Threat**: Attacker steals JWT token from mobile device.

**Mitigations**:
1. Short-lived tokens (15 min access, 7 day refresh)
2. Refresh token rotation (old one invalidated on use)
3. Device binding (token bound to device fingerprint)
4. Token revocation API (immediate effect via cache purge)
5. Biometric authentication required for sensitive operations

**Effectiveness**: 🟢 Stolen token expires quickly and is bound to device.

#### Scenario 5: TSP API Abuse

**Threat**: Attacker uses leaked OEM credentials to mass-revoke keys.

**Mitigations**:
1. gRPC mTLS with client certificate authentication
2. Rate limiting per OEM tenant (100 reqs/sec)
3. API key rotation (automatic, weekly)
4. Anomaly detection (unusual revocation patterns)
5. Approval workflow required for bulk operations
6. Audit logging with automated alerting

**Effectiveness**: 🟢 Abuse detected within seconds, rate-limited.

### 9.3 OWASP Top 10 Coverage

| Category | Coverage |
|----------|----------|
| A01: Broken Access Control | RBAC + ABAC, permission checks on every API call |
| A02: Cryptographic Failures | AES-256-GCM for data, TLS 1.3 for transport, proper key mgmt |
| A03: Injection | Parameterized queries, protobuf validation, input sanitization |
| A04: Insecure Design | Threat modeling in design phase, security reviews |
| A05: Security Misconfiguration | Hardened defaults, config validation, automated security scanning |
| A06: Vulnerable Components | Dependencies scanned weekly (Trivy/Dependabot) |
| A07: Identification/Auth Failures | JWT with RS256, mTLS, biometric verification |
| A08: Data Integrity Failures | Code signing, secure boot, message authentication |
| A09: Security Logging Failures | Structured audit logging, centralized SIEM |
| A10: SSRF | Network segmentation, allowlisting, no internal URL exposure |

### 9.4 Automotive-Specific Threats

| Automotive Threat | ISO 21434 Clause | yuleDKCS Coverage |
|-------------------|------------------|-------------------|
| Unauthorized vehicle access | 9.4 | UWB secure ranging + SE050 auth |
| Key cloning | 9.5 | Hardware-backed key storage per SE050 |
| Denial of service (vehicle) | 9.6 | Rate limiting + WAF + watchdog |
| Privacy violation | 10.1 | Field-level encryption, data minimization |
| Remote exploitation | 10.2 | mTLS + code signing + vulnerability mgmt |
| Supply chain attack | 10.3 | Dependency scanning + signed artifacts |
| Update security | 10.4 | Multi-signature OTA + secure boot |

---

## 10. Compliance and Certifications

### 10.1 Planned Certifications

| Certification | Scope | Target Date | Status |
|---------------|-------|-------------|--------|
| **ISO 21434** | Road vehicles — cybersecurity engineering | Q4 2026 | ⏳ Planning |
| **UN R155** | CSMS (Cybersecurity Management System) | Q4 2026 | ⏳ Planning |
| **SOC 2 Type II** | Cloud service security | Q2 2027 | ⏳ Planning |
| **PSA Certified Level 2** | IoT device security (TFM) | Q3 2026 | ⏳ Planning |
| **FIPS 140-3 Level 2** | Cryptographic module | Q4 2026 | ⏳ Planning |
| **GDPR** | Data privacy | Q2 2026 | ✅ Compliant by design |
| **PIPL** | China personal information protection | Q2 2026 | ✅ Compliant by design |

### 10.2 Security Testing Cadence

| Test Type | Frequency | Tool/Method |
|-----------|-----------|-------------|
| SAST (Static Analysis) | Per commit | SonarQube, CodeQL, golangci-lint |
| DAST (Dynamic Analysis) | Weekly | OWASP ZAP, Burp Suite |
| Dependency scan | Weekly | Trivy, Dependabot |
| Fuzz testing | Monthly | go-fuzz, libFuzzer |
| Penetration test | Quarterly | Third-party security firm |
| Red team exercise | Annually | External red team |
| Supply chain audit | Monthly | SBOM generation + analysis |

---

## 11. Security Incident Response

### 11.1 Incident Classification

| Severity | Definition | Response Time | Examples |
|----------|------------|---------------|----------|
| **P0 Critical** | Active exploitation, data breach, key compromise | ≤ 30 min | SE050 key leak, cloud breach |
| **P1 High** | Vulnerability with known exploit, major control bypass | ≤ 4 hours | Auth bypass, RCE in cloud |
| **P2 Medium** | Vulnerability with limited impact, requires conditions | ≤ 24 hours | DoS in non-critical path, info leak |
| **P3 Low** | Hard-to-exploit, low impact, defense-in-depth gap | ≤ 7 days | Minor config issue, hardening suggestion |

### 11.2 Response Process

```
                       ┌─────────────┐
                       │  Incident   │
                       │  Detected   │
                       └──────┬──────┘
                              │
                       ┌──────▼──────┐
                       │   Triage    │
                       │  (30 min)   │
                       └──────┬──────┘
                              │
              ┌───────────────┼───────────────┐
              │               │               │
         ┌────▼────┐    ┌────▼────┐    ┌────▼────┐
         │   P0    │    │   P1    │    │  P2/P3  │
         │ Critical│    │   High  │    │ Med/Low │
         └────┬────┘    └────┬────┘    └────┬────┘
              │              │              │
              │      ┌───────▼───────┐      │
              │      │  Team Response │      │
              │      │   (4 hours)   │      │
              │      └───────┬───────┘      │
              │              │              │
         ┌────▼──────────────▼──────────────▼────┐
         │          Containment                    │
         │   • Isolate affected components        │
         │   • Revoke compromised keys            │
         │   • Block attack vectors               │
         └───────────────────┬────────────────────┘
                             │
         ┌───────────────────▼────────────────────┐
         │          Eradication                    │
         │   • Remove attacker access             │
         │   • Patch vulnerability                │
         │   • Rotate all affected credentials    │
         └───────────────────┬────────────────────┘
                             │
         ┌───────────────────▼────────────────────┐
         │          Recovery                       │
         │   • Restore from clean backup          │
         │   • Verify no persistence              │
         │   • Gradual restore of service         │
         └───────────────────┬────────────────────┘
                             │
         ┌───────────────────▼────────────────────┐
         │          Post-Mortem                    │
         │   • Root cause analysis                │
         │   • Remediation plan                   │
         │   • Security bulletin                  │
         └────────────────────────────────────────┘
```

### 11.3 Communication Plan

| Stakeholder | P0 | P1 | P2 | P3 |
|-------------|----|----|----|----|
| Internal security team | Immediate | Immediate | Within 24h | Next sprint |
| Engineering team | 15 min | 1 hour | Next standup | Slack message |
| Executive leadership | 1 hour | 4 hours | Summary | Monthly report |
| Affected customers | 4 hours | 24 hours | Patch note | Release notes |
| Regulatory (if applicable) | 24 hours | 72 hours | Report | Annual |
| Public disclosure | Patch + 30 days | Patch + 90 days | N/A | N/A |

---

## 12. Security Audit Checklist

### For OEM Security Teams

Use this checklist when evaluating yuleDKCS for integration:

#### Architecture Review

- [ ] Key hierarchy documented and reviewed
- [ ] Trust boundaries clearly defined
- [ ] Attack surface minimized (least functionality)
- [ ] Defense-in-depth applied across all layers
- [ ] All external dependencies cataloged (SBOM)

#### Cryptographic Review

- [ ] Algorithms meet minimum requirements (AES-256, ECDSA P-256, SHA-256)
- [ ] Key lengths meet or exceed industry standards
- [ ] Random number generation uses hardware TRNG
- [ ] Forward secrecy guaranteed for session keys
- [ ] No deprecated ciphers or protocols

#### Implementation Review

- [ ] MISRA C:2012 compliance (embedded code)
- [ ] No hardcoded secrets or credentials
- [ ] Constant-time comparison for sensitive operations
- [ ] Input validation on all external entry points
- [ ] Proper error handling (no information leakage)

#### Operational Review

- [ ] Security monitoring and alerting configured
- [ ] Incident response plan documented
- [ ] Backup and disaster recovery tested
- [ ] Patch management process defined
- [ ] Vulnerability scanning automated

#### Compliance Review

- [ ] ISO 21434 TARA completed
- [ ] UN R155 CSMS compliance
- [ ] Data privacy (GDPR/PIPL) compliance
- [ ] Third-party penetration test results reviewed
- [ ] SBOM provided for all components

---

## 13. Appendix

### A. References

| Document | Description |
|----------|-------------|
| [Security Guide](SECURITY_GUIDE.md) | Operational security configuration |
| [System Architecture](SYSTEM_ARCHITECTURE.md) | System-level architecture description |
| [API Reference](API_REFERENCE.md) | REST and gRPC API documentation |
| [Deployment Guide](DEPLOYMENT_GUIDE.md) | Production deployment instructions |
| ISO 21434:2021 | Road vehicles — Cybersecurity engineering |
| UN Regulation No. 155 | Cybersecurity and CSMS |
| NIST SP 800-53 | Security and privacy controls |
| OWASP ASVS v4.0 | Application Security Verification Standard |
| CCC Digital Key 3.0 | Car Connectivity Consortium specification |
| ICCE Standard (T/CA 110-2020) | Intelligent Connected Car Cybersecurity |
| ICCOA Digital Key 3.0/4.0 | Intelligent Cockpit & Connectivity Alliance |

### B. Glossary

| Term | Definition |
|------|------------|
| **TCU** | Telematic Control Unit (vehicle-side embedded system) |
| **SE050** | NXP Secure Element (hardware security module) |
| **TFM** | Trusted Firmware-M (ARM platform security architecture) |
| **HSM** | Hardware Security Module |
| **KMS** | Key Management System |
| **TSP** | Telematics Service Provider (vehicle manufacturer's backend) |
| **STS** | Scrambled Timestamp Sequence (UWB security feature) |
| **BerTLV** | BER-TLV (Basic Encoding Rules — Tag, Length, Value) |
| **RBAC** | Role-Based Access Control |
| **ABAC** | Attribute-Based Access Control |
| **mTLS** | Mutual TLS (two-way certificate authentication) |
| **CSMS** | Cybersecurity Management System (UN R155) |
| **TARA** | Threat Assessment and Remediation Analysis (ISO 21434) |

### C. Security Contact

- **Security email**: security@yuledkcs.com
- **PGP key**: `0x12345678` (available on keyservers)
- **Bug bounty**: https://hackerone.com/yuledkcs
- **Responsible disclosure**: 90-day grace period for patches

### D. Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-06-01 | yuleDKCS Security Team | Initial whitepaper release |

---

*© 2026 yuleDKCS. This document is provided for security evaluation purposes. For the latest version, visit https://docs.yuledkcs.com/security/whitepaper*
