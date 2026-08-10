# OpenSpec: 多设备配钥 (REQ-ID 标注版)

> **版本**: v1.0 (REQ-ID 追溯更新)  
> **源文件**: `docs/spec/spec-multi-device.md`  
> **追溯规范**: RS-xxx = 系统级需求, SWR-xxx = 软件级需求  
> **全文索引**: `specs/requirements-index.md`

---

## Requirement: 设备注册 → RS-001

<!-- REQ-ID: RS-001 -->
- **RS-001.1**: The system SHALL allow a user to register a device after authentication
- **RS-001.2**: The device registration SHALL include device capabilities (BLE/UWB/NFC/SE/OS)
- **RS-001.3**: The system SHALL assign a unique device_id per device registration
- **RS-001.4**: The system SHALL return registered vehicles and existing keys upon registration

**Validation**:
- Unit: DeviceRegistrationService 单元测试 (暂缺)
- E2E: 注册流程端到端测试 (暂缺)
- Status: ❌ 无测试

---

## Requirement: 多设备按需配钥 → RS-002

<!-- REQ-ID: RS-002 -->
- **RS-002.1**: The system SHALL provision a key to a new device based on the device's capabilities
- **RS-002.2**: The provisioning SHALL negotiate the optimal protocol between device and vehicle
- **RS-002.3**: The system SHALL generate a device-specific key (bound to this device's identity)
- **RS-002.4**: The system SHALL NOT duplicate keys—if device already has a key, return existing

**Protocol Reference**: `SWR-DKC-001` (KeyBind), `SWR-DKC-005` (ShareCreate)

**Validation**:
- Integration: KeyBind 流程集成测试 (暂缺)
- Scenario: 新设备配钥成功返回 KeyId ≠ 已有 key 时创建新条目
- Status: ❌ 无测试

---

## Requirement: 多设备管理 → RS-003

<!-- REQ-ID: RS-003 -->
- **RS-003.1**: The system SHALL allow a user to list all devices with provisioned keys
- **RS-003.2**: The system SHALL allow a user to remotely revoke a device's key(s)
- **RS-003.3**: The system SHALL notify the device when its key is revoked (via push)
- **RS-003.4**: The system SHALL limit the number of devices per user (5 minimum)

**Protocol Reference**: `SWR-DKC-004` (KeyList), `SWR-DKC-003` (KeyRevoke)

**Scenario Reference**: 用户丢失设备后吊销 (见下文)

**Validation**:
- Integration: KeyList + KeyRevoke 流程 (暂缺)
- Status: ❌ 无测试

---

## Scenario: 用户在新手机配钥

<!-- REQ-ID References: RS-001, RS-002, SWR-PRO-003 (App↔HUB) -->
- GIVEN 车主已登录云账号
- GIVEN 车辆已有绑定关系
- WHEN 车主在新手机上打开App
- THEN App自动注册设备并上报能力 (`RS-001.2`)
- THEN App请求为新设备配钥 (`RS-002.1`)
- THEN 云端协商最佳协议 (`RS-002.2`)
- THEN 生成专属密钥并下发 (`RS-002.3`)
- THEN 密钥存入设备SE/Keychain

---

## Scenario: 用户丢失设备后吊销

<!-- REQ-ID References: RS-003.2, RS-003.3, RS-007.33 -->
- GIVEN 车主发现旧手机丢失
- WHEN 车主在另一设备上吊销旧设备的钥匙
- THEN 云端标记该设备所有钥匙为revoked (`RS-003.2`)
- THEN 车端即时拒绝该设备的解锁请求 (`SWR-DKC-003`)

---

## 追溯汇总

| 场景 | 涉及 REQ-ID | 覆盖状态 |
|------|------------|---------|
| 设备注册 | RS-001.1~4 | ❌无测试 |
| 按需配钥 | RS-002.1~4 | ❌无测试 |
| 多设备管理 | RS-003.1~4 | ❌无测试 |
| 新手机配钥 | RS-001, RS-002, SWR-PRO-003 | ❌无测试 |
| 丢失设备吊销 | RS-003.2~3, RS-007, SWR-DKC-003 | ❌无测试 |
