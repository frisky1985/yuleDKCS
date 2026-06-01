# OpenSpec: 多设备配钥

## Requirement: 设备注册

- The system SHALL allow a user to register a device after authentication
- The device registration SHALL include device capabilities (BLE/UWB/NFC/SE/OS)
- The system SHALL assign a unique device_id per device registration
- The system SHALL return registered vehicles and existing keys upon registration

## Requirement: 多设备按需配钥

- The system SHALL provision a key to a new device based on the device's capabilities
- The provisioning SHALL negotiate the optimal protocol between device and vehicle
- The system SHALL generate a device-specific key (bound to this device's identity)
- The system SHALL NOT duplicate keys—if device already has a key, return existing

## Requirement: 多设备管理

- The system SHALL allow a user to list all devices with provisioned keys
- The system SHALL allow a user to remotely revoke a device's key(s)
- The system SHALL notify the device when its key is revoked (via push)
- The system SHALL limit the number of devices per user (5 minimum)

## Scenario: 用户在新手机配钥

- GIVEN 车主已登录云账号
- GIVEN 车辆已有绑定关系
- WHEN 车主在新手机上打开App
- THEN App自动注册设备并上报能力
- THEN App请求为新设备配钥
- THEN 云端协商最佳协议
- THEN 生成专属密钥并下发
- THEN 密钥存入设备SE/Keychain

## Scenario: 用户丢失设备后吊销

- GIVEN 车主发现旧手机丢失
- WHEN 车主在另一设备上吊销旧设备的钥匙
- THEN 云端标记该设备所有钥匙为revoked
- THEN 车端即时拒绝该设备的解锁请求
