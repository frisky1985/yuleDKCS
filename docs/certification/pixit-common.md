# PIXIT — Protocol Implementation eXtra Information for Testing

> **Document Version**: v1.0 | **Date**: 2026-07-30
> **Product**: yuleDKCS Digital Key System (Vehicle-Side Embedded)
> **Applicable Protocols**: ICCE (T/CA 110-2020) / CCC DK 3.0 / ICCOA DK 3.0 + DK 4.0
> **Certification Bodies**: CAICT/CATARC (ICCE) · UL LLC/TÜV Rheinland/Bureau Veritas (CCC) · ICCOA Authorized Lab (ICCOA)

---

## 1. Test Device Identification

### 1.1 Device Under Test (DUT) — Vehicle-Side Module

| Item | Specification |
|:-----|:-------------|
| **Product Name** | yuleDKCS Vehicle Digital Key Module |
| **Model Number** | YDK-VCU-01 |
| **Hardware Revision** | Rev 1.0 |
| **Firmware Version** | v1.0.0 (Build 2026-07-30) |
| **Serial Number** | YDK-VCU-01-001 (Sample 1) / YDK-VCU-01-002 (Sample 2) / YDK-VCU-01-003 (Sample 3) |
| **Manufacturer** | OpenClaw Technology |
| **Manufacturing Date** | 2026-07 |

### 1.2 Hardware Component List

| Component | Part Number | Firmware Version | Driver Version | Purpose |
|:----------|:-----------|:----------------:|:--------------:|:--------|
| BLE/NFC MCU | NXP KW47A | KW47A_BLE_v2.1 | v1.0 | BLE 5.0 communication + NFC LPCD |
| UWB Module | NXP NCJ29D6 | NCJ29D6_UWB_v1.8 | v1.0 | IEEE 802.15.4z secure ranging |
| NFC Reader | ST ST25R501 | ST25R501_NFC_v2.0 | v1.0 | ISO 14443-4, NFC-F, APDU |
| Secure Element | NXP SE050 | SE050_OS_v3.4.0 | v1.0 | EAL5+ key storage + crypto |
| Main MCU | STM32L552ZE | STM32L5_HAL_v1.6 | v1.0 | System control + protocol stack |

### 1.3 Test Bed Configuration

| Component | Specification |
|:----------|:-------------|
| **Vehicle Simulator** | CAN bus simulator + I/O signal generator |
| **Phone Simulator** | yuleDKCS Test App (Android 14 / iOS 18) |
| **Cloud Simulator** | yuleDKCS Cloud Hub (Docker container) |
| **BLE Sniffer** | Ellisys Bluetooth Analyzer VX |
| **UWB Analyzer** | SPARQ UWB Module Analyzer |
| **NFC Analyzer** | Proxmark3 RDV4 |
| **Power Supply** | 12V DC automotive-grade, 3A max |

---

## 2. BLE (Bluetooth Low Energy) Configuration

### 2.1 BLE General Parameters

| Parameter | Value | Notes |
|:----------|:------|:------|
| **BLE Version** | Bluetooth 5.0 LE Only | No BR/EDR |
| **PHY** | LE 1M, LE 2M, LE Coded (S=8) | Configurable per protocol |
| **Advertising Type** | Connectable undirected | ADV_IND |
| **Advertising Interval** | 100 ms (min) / 500 ms (typical) / 1000 ms (max) | Configurable |
| **Connection Interval** | 30 ms (min) / 45 ms (typical) / 50 ms (max) | Negotiated |
| **Connection Latency** | 0 | |
| **Supervision Timeout** | 4000 ms | 4× connection interval |
| **ATT MTU** | 247 bytes (CCC/ICCE/ICCOA) | Negotiated from 23 |
| **Security Mode** | LE Secure Connections (LE SC) | Mandatory |
| **IO Capabilities** | NoInputNoOutput | OOB pairing via NFC |
| **Pairing Method** | OOB (NFC data) / Passkey Entry | Both supported |
| **Max Simultaneous Connections** | 8 | ICCE/ICCOA; CCC limits to 1 |

### 2.2 Protocol-Specific BLE GATT Profiles

#### ICCE GATT Profile (Service UUID: 0xFEFA)

| Characteristic | UUID | Properties | Permissions | Max Data Len |
|:---------------|:----:|:----------|:-----------:|:-----------:|
| Key Status | 0xFEFB | Read, Notify | Encrypted Read | 244 B |
| Ranging Data | 0xFEFC | Read, Notify | Encrypted Read | 244 B |
| Auth Challenge | 0xFEFD | Write, Indicate | Encrypted Write | 244 B |
| Control Command | 0xFEFE | Write, Notify | Encrypted Write | 244 B |
| Session Key | 0xFEFF | Write | Encrypted Write (Auth) | 244 B |

#### CCC GATT Profile (Service UUID: 0xFFD1)

| Item | Value |
|:-----|:------|
| **Number of Characteristics** | 16 (per CCC DK 3.0 specification) |
| **Characteristic Properties** | Read, Write, Notify, Indicate (varies by characteristic) |
| **Permission Level** | Encrypted Read/Write for all characteristics |
| **Service UUID** | 0xFFD1 (SIG Assigned — CCC Digital Key Service) |

#### ICCOA GATT Profile (Service UUID: 0xFEF5)

| Item | Value |
|:-----|:------|
| **Service UUID** | 0xFEF5 |
| **Characteristic Properties** | Read, Write with response, Notify |
| **Frame Format** | SOP+CMD+SEQ+LEN+PAYLOAD+CHK+EOP (DK 3.0) / Magic+Version+Type+ID+Flags+Len+Token+Payload+HMAC (DK 4.0) |
| **Transport** | GATT Write Request (phone→vehicle) / GATT Notification (vehicle→phone) |

### 2.3 BLE Test Parameters

| Parameter | ICCE | CCC | ICCOA |
|:----------|:----:|:---:|:-----:|
| Test Advertising Interval | 100 ms | 100 ms | 100 ms |
| Test Connection Interval | 30 ms | 30 ms | 30 ms |
| Test Supervision Timeout | 4000 ms | 4000 ms | 4000 ms |
| Max ATT MTU | 247 B | 247 B | 247 B |
| Max Response Time | 500 ms | 500 ms | 500 ms |

---

## 3. NFC Configuration

### 3.1 NFC Hardware

| Parameter | Value |
|:----------|:------|
| **NFC Reader IC** | ST ST25R501 |
| **Supported Protocols** | ISO/IEC 14443-4 Type A/B, ISO/IEC 18092 (NFC-F / FeliCa) |
| **LPCD (Low Power Card Detection)** | Enabled (default) |
| **LPCD Polling Interval** | 200 ms (idle), 50 ms (active detect) |
| **Operating Frequency** | 13.56 MHz |
| **Data Rates** | 106 kbps, 212 kbps, 424 kbps |
| **Max APDU Payload** | 512 bytes |

### 3.2 NFC Test Parameters

| Parameter | Value | Notes |
|:----------|:------|:------|
| Test Card Type | ISO 14443-4 Type A UID 4B/7B, FeliCa | |
| Test Distance | 0 – 4 cm | Optimal: 1–2 cm |
| Test Field Strength | 1.5 A/m (rms) @ 13.56 MHz | |
| APDU Case | Case 1–4 | Per ISO/IEC 7816-4 |
| Max APDU Response Time | 500 ms | |
| NFC OOB Data Structure | `ccc_nfc_oob_data_t` | BLE MAC + UWB params + capabilities |

---

## 4. UWB Configuration

### 4.1 UWB Hardware

| Parameter | Value |
|:----------|:------|
| **UWB IC** | NXP NCJ29D6 |
| **Standards** | IEEE 802.15.4z HRP, FiRa PHY |
| **Antenna Config** | 4-antenna array (azimuth + elevation AoA) |
| **Frequency Bands** | Channel 5 (6.5 GHz), Channel 9 (8.3 GHz) |

### 4.2 UWB Session Configuration

| Parameter | Value | Notes |
|:----------|:------|:------|
| **Default Channel** | 9 (8.3 GHz) | |
| **Alternate Channel** | 5 (6.5 GHz) | |
| **Preamble Code (Ch9)** | 12 | |
| **Preamble Code (Ch5)** | 9 | |
| **PRF Length** | 128 | |
| **SFD ID** | 0 (standard) | |
| **PHR Data Rate** | 6.81 Mb/s (standard) | |
| **Data Rate** | 6.81 Mb/s | |
| **RFrame Config** | SP3 | Use SP0/SP1 for test if needed |
| **STS Config** | STS segment 1 (32 symbols × 2) | |
| **STS Key Length** | 128-bit AES | |
| **Slot Duration** | 2400 RSTU (≈ 240 µs) | |
| **Ranging Round** | 2-slots (controller + responder) | |
| **Measurement Report** | Distance + Quality + AoA | |
| **Ranging Interval** | 50 ms (active), 200 ms (passive) | |

### 4.3 Distance Zone Thresholds (Test Values)

| Zone | Distance Range | Default Threshold | Action |
|:----|:--------------|:-----------------:|:-------|
| LOCKED | > 10 m | 1200 cm | Vehicle locked, no action |
| APPROACH | 5 – 10 m | 1000 cm | Wake BLE, prepare UWB |
| UNLOCK | 2 – 5 m | 500 cm | Prepare unlock |
| ENTRY | 0.5 – 2 m | 200 cm | Execute unlock |
| INSIDE | < 0.5 m | 50 cm | Engine start enabled |
| Hysteresis | — | 30 cm | Prevent zone flickering |

### 4.4 UWB Test Parameters

| Parameter | Value |
|:----------|:------|
| Test Ranging Accuracy Target | ≤ 10 cm (LOS, open) |
| Test Max Range | 15 m |
| Test Angle Accuracy (Azimuth) | ≤ 5° |
| Test Angle Accuracy (Elevation) | ≤ 5° |
| Test Measurement Rate | 20 Hz |
| Test Duration per Scenario | 30 s |
| Number of Test Positions | 12 (0°, 30°, 60°, ..., 330°) |

---

## 5. Security Test Parameters

### 5.1 SE050 Secure Element

| Parameter | Value |
|:----------|:------|
| **SE Chip** | NXP SE050C (EAL5+) |
| **I2C Address** | 0x48 |
| **I2C Speed** | 400 kHz (Fast Mode) |
| **SCP03 Key Set** | Pre-provisioned (Factory) |
| **SCP03 Chain Mode** | Full (ENC + MAC + DEK derived) |
| **Key Slot Map** | See §Key Slots below |
| **Transparent Object Range** | 0xF00000 – 0xF00020 |

### 5.2 SE050 Key Slots

| Slot ID | Purpose | Algorithm | Test Key ID |
|:-------:|:--------|:---------:|:-----------:|
| 0x00 | Root Key | AES-128 | — |
| 0x01 | Master Key | AES-128 | — |
| 0x02 | Device Key (Vehicle) | ECDSA P-256 / SM2 | DEV_KEY_001 |
| 0x10+ | Session Keys | AES-256 / SM4 | Per-session |

### 5.3 Test Credentials

| Credential | Format | Purpose | Source |
|:-----------|:-------|:--------|:-------|
| Root CA Certificate | X.509 (CCC) / SM2 Cert (ICCE) | Chain root | Cloud PKI / KMS |
| OEM CA Certificate | X.509 / SM2 Cert | OEM-level intermediate | Cloud PKI / KMS |
| Vehicle Certificate | X.509 / SM2 Cert | VIN-bound vehicle identity | Pre-provisioned |
| Device (Phone) Certificate | X.509 / SM2 Cert | User-bound device identity | Cloud-issued during pairing |
| Attestation Key | SE050 internal | SE identity attestation | Factory-provisioned |
| Test Nonce | 16 B random | Auth challenge | Generated per test case |

---

## 6. Protocol-Specific Test Parameters

### 6.1 ICCE-Specific

| Parameter | Value |
|:----------|:------|
| **BLE Service UUID** | 0xFEFA |
| **SM2 Curve** | GB/T 32918.1-2016 (SM2 P-256 equivalent) |
| **SM3 Digest Size** | 32 B |
| **SM4 Key Size** | 16 B (128-bit) |
| **Edge Rule Max Count** | 32 |
| **Edge Zones** | 5 (FAR / MID / NEAR / VICINITY / INTERIOR) |
| **OOB Pairing Method** | NFC / QR Code |
| **Offline Key Cache** | Enabled (default) |
| **Sync on Reconnect** | Full sync (offline operation log) |
| **Test Vehicle VIN Prefix** | YDK-ICCE-TEST-001 |

### 6.2 CCC-Specific

| Parameter | Value |
|:----------|:------|
| **BLE Service UUID** | 0xFFD1 |
| **Message Types (BLE)** | 13 types (0x01–0x40, 0xFF) |
| **Attestation Version** | 0x01 |
| **Firmware Hash Algorithm** | SHA-256 |
| **Certificate Max Length** | 256 B |
| **Attestation Max Length** | 128 B |
| **Max Keys Stored** | 8 |
| **NFC OOB Data Structure** | ccc_nfc_oob_data_t (52 B) |
| **Test Vehicle VIN Prefix** | YDK-CCC-TEST-001 |

### 6.3 ICCOA-Specific

| Parameter | DK 3.0 | DK 4.0 |
|:----------|:------:|:-------:|
| **BLE Service UUID** | 0xFEF5 | 0xFEF5 |
| **Frame Magic** | 0xAA / 0x55 (SOP/EOP) | 0x1CC0 |
| **Frame Header Size** | 7 B | 16 B |
| **Max Payload** | 244 B | 244 B |
| **Checksum / HMAC** | XOR checksum (1 B) | HMAC-SHA256 (16 B) |
| **Permission Bits** | 8 | 8 |
| **Session Token Size** | — | 4 B |
| **UWB Ranging Support** | No (optional) | Yes (mandatory for DK 4.0) |
| **Multi-Device Support** | No | Yes |
| **Test Vehicle VIN Prefix** | YDK-ICCOA-TEST-001 | YDK-ICCOA40-TEST-001 |

---

## 7. Test Environment & Communication

### 7.1 Test Lab Network Configuration

| Item | Value |
|:-----|:------|
| **DUT Network Interface** | Automotive Ethernet (100BASE-T1) via gateway |
| **Cloud Connection** | TLS 1.3 over HTTPS |
| **Test Server Host** | yuleDKCS Cloud Hub (Docker container on lab LAN) |
| **Server IP** | 192.168.10.100 (lab network) |
| **Server Port** | 443 (HTTPS), 8883 (MQTTs) |
| **Certificate** | Test CA (self-signed for lab use) |

### 7.2 Test Communication Parameters

| Parameter | Value |
|:----------|:------|
| **BLE Test RSSI Threshold** | -80 dBm (min for connection) |
| **BLE Test TX Power** | 0 dBm (default), +4 dBm (max) |
| **NFC Test Field Activation** | 13.56 MHz, continuous wave |
| **UWB Test TX Power** | -41.3 dBm/MHz (compliant with regulatory limits) |
| **Test Timeout (BLE connection)** | 30 s |
| **Test Timeout (NFC transaction)** | 5 s |
| **Test Timeout (UWB ranging)** | 10 s |
| **Test Timeout (auth/command)** | 5 s |

---

## 8. Special Test Configurations

### 8.1 Test Mode Activation

The DUT supports a **Test Mode** for certification lab testing:

| Test Mode Feature | Activation Method | Description |
|:-----------------|:-----------------|:-----------|
| **Test Mode Entry** | GPIO pin high at boot + serial command `TESTMODE 1` | Enables debug logging, extended timeouts |
| **Test Mode Exit** | Serial command `TESTMODE 0` or power cycle | Returns to production mode |
| **Factory Reset** | GPIO pin low for 10 s at boot | Clears all key material |
| **Log Level** | Serial command `LOGLEVEL <0-5>` | 0=ERROR, 3=INFO, 5=DEBUG |

### 8.2 Debug Interfaces

| Interface | Protocol | Parameters | Use |
|:----------|:---------|:-----------|:----|
| UART (Console) | 115200 baud, 8N1 | TX=PA9, RX=PA10 | Debug log, test commands |
| SWD (Debug) | ARM SWD | SWDIO=PA13, SWCLK=PA14 | Firmware flash, debug |
| I²C (SE050) | 400 kHz | SDA=PB7, SCL=PB6 | SE communication monitor |

### 8.3 Test Case Overrides

For test cases requiring specific configurations:

| Parameter | Normal Value | Test Override | Notes |
|:----------|:------------:|:-------------:|:------|
| BLE Advertising Interval | 200 ms | 100 ms | Accelerate test flow |
| BLE Supervision Timeout | 4000 ms | 6000 ms | Allow manual delays |
| UWB Ranging Interval | 100 ms | 50 ms | Faster measurement collection |
| Auth Challenge Expiry | 30 s | 60 s | Avoid test timeout failures |
| Key Validity Duration | 365 days | 24 h | Test key expiry scenarios |
| Max Uses (Temporary Key) | 1000 | 5 | Test use-count scenarios |

---

## 9. Test Samples

### 9.1 Hardware Samples Provided

| Sample ID | Serial Number | Hardware Rev | Firmware Version | Notes |
|:----------|:-------------:|:------------:|:----------------:|:------|
| DUT-001 | YDK-VCU-01-001 | Rev 1.0 | v1.0.0 | Primary test unit |
| DUT-002 | YDK-VCU-01-002 | Rev 1.0 | v1.0.0 | Spare / parallel test |
| DUT-003 | YDK-VCU-01-003 | Rev 1.0 | v1.0.0 | Backup |

### 9.2 Accessories Provided

| Accessory | Quantity | Notes |
|:----------|:--------:|:------|
| NFC Test Card (ISO 14443-4 Type A) | 2 | Pre-provisioned |
| NFC Test Card (FeliCa) | 1 | Pre-provisioned |
| Test Phone (Android 14, CCC/ICCOA/ICCE App) | 1 | Pixel 8 Pro |
| Test Phone (iOS 18, CCC App) | 1 | iPhone 16 Pro |
| BLE Sniffer Setup Guide | 1 | |
| Power Supply (12V DC) | 1 | Automotive-grade |

---

## 10. Contact Information

| Role | Name | Contact |
|:-----|:-----|:--------|
| **Technical Contact** | yuleDKCS Embedded Team | embedded@yuledkcs.com |
| **Certification Coordinator** | OpenClaw QA Team | certification@openclaw.com |
| **Emergency Contact** | On-call Engineer | +86-400-xxx-xxxx |

---

## 11. Version History

| Version | Date | Changes | Author |
|:-------:|:----:|:--------|:------:|
| v1.0 | 2026-07-30 | Initial PIXIT document (common across ICCE/CCC/ICCOA) | Hermes |
