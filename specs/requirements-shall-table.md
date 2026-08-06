# yuleDKCS 软件需求 SHALL 表（机器可读版）

> **用途**: 供 `yuleosh traceability matrix` / `yuleosh evidence pack` 解析 REQ-xxx 需求
> **权威文档**: `docs/software-requirements.md`（完整 SRS，含全部 SHALL 语句与属性）
> **上游**: `specs/requirements-index.md`（RS-xxx / SWR-xxx 体系）
> **版本**: 1.0.0 | **日期**: 2026-08-06 | **状态**: APPROVED

本表为 SRS 的机器可读投影，每条 REQ-xxx 对应一行，语句以 SHALL 开头（强制合规约束）。详细多子句 SHALL 见 SRS。

## REQ 需求表

| ID | SHALL 语句 | ASIL | 范围 |
|:---|:-----------|:-----|:-----|
| REQ-001 | SHALL allow a user to register a device after authentication, with unique device_id and capability reporting | QM | 全部 |
| REQ-002 | SHALL provision a device-specific key based on device capabilities with optimal protocol negotiation and no key duplication | QM | 全部 |
| REQ-003 | SHALL support multi-device management: list devices, remote key revocation with push notification, and ≥5 devices per user | QM | 全部 |
| REQ-004 | SHALL meet performance targets: unlock/lock ≤1s, App cold start ≤2s, cloud P99 ≤200ms, pairing ≤3min, remote control ≤3s, throughput ≥100,000 TPS | QM | 全部 |
| REQ-005 | SHALL provide availability ≥99.9%, MTTR ≤30min, RTO ≤4h, RPO ≤1h, and hourly incremental backup | QM | 全部 |
| REQ-006 | SHALL enforce security: SE ≥EAL5+, TLS 1.3 with forward secrecy, MFA, SE/TEE key storage, UWB secure ranging anti-relay, anti-replay nonce, end-to-end encryption, audit retention ≥3 years | ASIL-B/A | 全部 |
| REQ-007 | SHALL support offline capability: NFC unlock independent of phone battery, valid offline keys, and auto-sync of offline records | QM | 全部 |
| REQ-008 | SHALL support ICCE (T/CA 110-2020) and CCC Digital Key 3.0 simultaneously with auto protocol negotiation and dual certificate management | QM | 全部 |
| REQ-009 | SHALL provide seamless UX: auto-unlock within 1s at ≤2m approach, auto-lock after 30s beyond 5m, background unlock, ≤5 keys per vehicle | QM | 全部 |
| REQ-010 | SHALL support KeyBind (1000/1001) with bidirectional BERTLV encoding, mandatory bind fields, 4 KeyTypes, 16-bit AccessLevel, and KeyId/VehiclePubkey/SharedSecret/VehicleCert response | ASIL-B | DKCS Core |
| REQ-011 | SHALL support KeyUnbind (1002/1003) with KeyId and Reason | ASIL-B | DKCS Core |
| REQ-012 | SHALL support KeyRevoke (1004/1005) including emergency mode, and SHALL reject unlock from revoked devices immediately | ASIL-B | DKCS Core |
| REQ-013 | SHALL support KeyList (1010/1011) with pagination | QM | DKCS Core |
| REQ-014 | SHALL support ShareCreate (2000/2001) with AccessLevel, ValidFrom, ValidUntil, MaxUses | ASIL-B | DKCS Core |
| REQ-015 | SHALL support VehicleCommand (3000/3001) with 11 actions and 5 sources (NFC/BLE/UWB/Remote/Edge) | ASIL-B | DKCS Core |
| REQ-016 | SHALL support VehicleStatusReport (3002) with LockStatus/EngineStatus/DoorStatus/BatteryPct/InteriorTemp/GPS | QM | DKCS Core |
| REQ-017 | SHALL support bidirectional Heartbeat (9000/9001) with Status/CpuUsage/MemUsage/ConnCount | QM | DKCS Core |
| REQ-018 | SHALL sign every message with HMAC-SHA256 over Header+Body in the E1 FF Trailer | ASIL-B | DKCS Core |
| REQ-019 | SHALL lowercase vendor and protocol strings before registry lookup with normalization in Register/Get without breaking existing matching | QM | Hub |
| REQ-020 | SHALL add nil safety checks before accessing RemoteControl and normalize vendor via strings.ToLower at lookup points | QM | Hub |
| REQ-021 | SHALL provide unit tests covering all 7 hub service source files ≥80% and logger ≥85% without modifying production logic | QM | Hub |
| REQ-022 | SHALL enforce CI coverage gate fail-under=60 for backend/dkcs and backend/cloud/hub | QM | Hub |
| REQ-023 | SHALL structure CI into 3 layers with L1 required for merge, separated integration tests, and gosec SAST on all Go code | QM | Hub |
| REQ-024 | SHALL use BERTLV encoding (Header E1 01 + Body + Trailer E1 FF) with length rules 00-7F/80-FF and mandatory header fields | QM | Protocol |
| REQ-025 | SHALL use MQTT topics digitalkey/{tsp_id}/{vehicle_id}/{resource}/{action} with QoS 2/1/0 mapping, mTLS and BERTLV payloads | ASIL-B | Protocol |
| REQ-026 | SHALL use HTTPS REST + BERTLV with OAuth2.0 token auth and HMAC-SHA256 request signing for key operations | QM | Protocol |
| REQ-027 | SHALL provide unified error codes across protocol layers: 1xxx request, 2xxx key, 3xxx vehicle, 4xxx vendor | QM | Protocol |
| REQ-028 | SHALL implement NFC stack (ST25R501): 13.56MHz field detection, ISO 14443-A/FeliCa, NDEF, NFC-F OOB pairing, ISO 7816-4 APDU | ASIL-B | Embedded |
| REQ-029 | SHALL implement BLE stack (KW47A): BLE 5.0 GATT 0xFFD1, NFC-assisted OOB pairing, LE Secure Connections, full CCC GATT profile | ASIL-B | Embedded |
| REQ-030 | SHALL implement UWB ranging (NCJ29D6): IEEE 802.15.4z, TWR, STS secure ranging, 5 distance zones, multi-anchor | ASIL-B(D) | Embedded |
| REQ-031 | SHALL implement ICCOA DK 3.0/4.0 stack: BLE broadcast, GATT 0xFEF5, DK3.0 frame format, DK4.0 UWB/multi-device/remote-share, BIND flow, 8 permission bits | ASIL-B | Embedded |
| REQ-032 | SHALL implement ICCE stack on KW47A with edge offline decisions, key caching (pubkey permanent/permission 24h/token 8h), OOB pairing, SM2/SM3/SM4 | ASIL-B | Embedded |
| REQ-033 | SHALL integrate SE050 with SCP03 secure channel, attestation, and vehicle-side verification (cert chain + ECDSA + validity + firmware hash) | ASIL-B | Embedded |
| REQ-034 | SHALL implement 5 power states with ACTIVE<15mA/SLEEP<100μA/DEEPSLEEP<10μA and wake-up <50ms NFC / <100ms BLE | QM | Embedded |
| REQ-035 | SHALL implement 4-stage secure boot (BL→OS→APP→COMPLETE) with per-stage signature verification and SE050 trust anchor | ASIL-B | Embedded |
| REQ-036 | SHALL provide Android SDK (min SDK 26) with KeyManager/VehicleController/ShareManager/ChannelManager/SecurityModule and BLE/UWB/NFC channels | QM | Frontend |
| REQ-037 | SHALL provide iOS SDK (min iOS 14.0) with CoreNFC/CoreBluetooth/CoreLocation and KeyManaging/VehicleControlling/ShareManaging/ChannelManaging/SecurityManaging | QM | Frontend |
| REQ-038 | SHALL provide REST API at /api/v1 with JWT Bearer auth over TLS 1.3 and unified code/message/data/requestId/timestamp response | QM | Frontend |
| REQ-039 | SHALL provide 4 gRPC microservices (KeyService/VehicleService/KMSService/EventService) with defined RPC sets | QM | Frontend |
| REQ-040 | SHALL provide 9 MQ topics with Avro Schema Registry and isolated consumer groups | QM | Frontend |
