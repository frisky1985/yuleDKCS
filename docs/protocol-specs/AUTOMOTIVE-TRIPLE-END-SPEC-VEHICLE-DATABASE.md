# 车辆与CCC数字钥匙数据库定义

> **适用项目**: yule-notion (车云一体化)  
> **数据库**: PostgreSQL 15+  
> **关联协议**: BER-TLV v1.2.0, CCC Digital Key

---

## 一、核心数据表

### 1.1 车辆表 (vehicles) ⭐

车辆基础信息，VIN为主键

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| vin | VARCHAR(17) | PK, UNIQUE | 车辆识别号 (ISO 3779) |
| user_id | UUID | FK → users | 车主用户 |
| vehicle_model | VARCHAR(100) | NOT NULL | 车型 |
| vehicle_brand | VARCHAR(50) | NOT NULL | 品牌 |
| license_plate | VARCHAR(20) | UNIQUE | 车牌号 |
| manufacture_date | DATE | | 生产日期 |
| color | VARCHAR(30) | | 车身颜色 |
| status | VARCHAR(20) | DEFAULT 'active' | active/inactive/sold |
| tbox_device_id | VARCHAR(64) | UNIQUE | T-Box设备标识 |
| ecu_version | VARCHAR(50) | | ECU固件版本 |
| last_online_at | TIMESTAMPTZ | | 最后在线时间 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 注册时间 |
| updated_at | TIMESTAMPTZ | DEFAULT NOW() | 更新时间 |

```sql
CREATE TABLE vehicles (
    vin VARCHAR(17) PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id),
    vehicle_model VARCHAR(100) NOT NULL,
    vehicle_brand VARCHAR(50) NOT NULL,
    license_plate VARCHAR(20) UNIQUE,
    manufacture_date DATE,
    color VARCHAR(30),
    status VARCHAR(20) DEFAULT 'active',
    tbox_device_id VARCHAR(64) UNIQUE,
    ecu_version VARCHAR(50),
    last_online_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 索引
CREATE INDEX idx_vehicles_user ON vehicles(user_id);
CREATE INDEX idx_vehicles_status ON vehicles(status);
CREATE INDEX idx_vehicles_tbox ON vehicles(tbox_device_id);
```

---

### 1.2 CCC Slot表 (ccc_slots)

CCC数字钥匙槽位管理

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| slot_id | UUID | PK | 槽位唯一标识 |
| vin | VARCHAR(17) | FK → vehicles | 所属车辆 |
| slot_identifier | INTEGER | NOT NULL, CHECK(0-7) | CCC槽位号 (0-7) |
| key_identifier | BYTEA | NOT NULL, UNIQUE | 钥匙标识 (8字节) |
| device_identifier | BYTEA | | 设备标识 (16字节) |
| slot_state | VARCHAR(20) | DEFAULT 'empty' | empty/active/suspended/revoked |
| key_type | VARCHAR(20) | | owner/friend/valet/repair |
| valid_from | TIMESTAMPTZ | | 生效时间 |
| valid_until | TIMESTAMPTZ | | 过期时间 |
| authorized_actions | JSONB | DEFAULT '[]' | 授权操作列表 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 创建时间 |
| updated_at | TIMESTAMPTZ | DEFAULT NOW() | 更新时间 |

```sql
CREATE TABLE ccc_slots (
    slot_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vin VARCHAR(17) NOT NULL REFERENCES vehicles(vin) ON DELETE CASCADE,
    slot_identifier INTEGER NOT NULL CHECK (slot_identifier BETWEEN 0 AND 7),
    key_identifier BYTEA NOT NULL UNIQUE,
    device_identifier BYTEA,
    slot_state VARCHAR(20) DEFAULT 'empty',
    key_type VARCHAR(20),
    valid_from TIMESTAMPTZ,
    valid_until TIMESTAMPTZ,
    authorized_actions JSONB DEFAULT '[]',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(vin, slot_identifier)
);

-- 索引
CREATE INDEX idx_ccc_slots_vin ON ccc_slots(vin);
CREATE INDEX idx_ccc_slots_key_id ON ccc_slots(key_identifier);
CREATE INDEX idx_ccc_slots_state ON ccc_slots(slot_state);
```

---

### 1.3 CCC证书表 (ccc_certificates)

存储CCC证书链

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| cert_id | UUID | PK | 证书ID |
| vin | VARCHAR(17) | FK → vehicles | 所属车辆 |
| cert_type | VARCHAR(30) | NOT NULL | vehicle/vehicleOEM/device/intermediate/root |
| cert_data | BYTEA | NOT NULL | X.509证书DER格式 |
| cert_hash | BYTEA | UNIQUE | 证书哈希 (SHA-256) |
| serial_number | VARCHAR(64) | UNIQUE | 证书序列号 |
| issuer | TEXT | | 颁发者DN |
| subject | TEXT | | 主题DN |
| valid_from | TIMESTAMPTZ | NOT NULL | 生效时间 |
| valid_until | TIMESTAMPTZ | NOT NULL | 过期时间 |
| is_custom | BOOLEAN | DEFAULT FALSE | 是否自定义证书 |
| oem_id | VARCHAR(20) | | 车厂ID |
| key_id | UUID | FK → ccc_slots | 关联钥匙(可选) |
| parent_cert_id | UUID | FK → ccc_certificates | 父证书(链) |
| status | VARCHAR(20) | DEFAULT 'active' | active/expired/revoked |
| revoked_at | TIMESTAMPTZ | | 吊销时间 |
| crl_url | TEXT | | CRL地址 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 创建时间 |

```sql
CREATE TABLE ccc_certificates (
    cert_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vin VARCHAR(17) REFERENCES vehicles(vin) ON DELETE CASCADE,
    cert_type VARCHAR(30) NOT NULL CHECK (
        cert_type IN ('vehicle', 'vehicleOEM', 'device', 'intermediate', 'root')
    ),
    cert_data BYTEA NOT NULL,
    cert_hash BYTEA UNIQUE,
    serial_number VARCHAR(64) UNIQUE,
    issuer TEXT,
    subject TEXT,
    valid_from TIMESTAMPTZ NOT NULL,
    valid_until TIMESTAMPTZ NOT NULL,
    is_custom BOOLEAN DEFAULT FALSE,
    oem_id VARCHAR(20),
    key_id UUID REFERENCES ccc_slots(slot_id) ON DELETE SET NULL,
    parent_cert_id UUID REFERENCES ccc_certificates(cert_id),
    status VARCHAR(20) DEFAULT 'active',
    revoked_at TIMESTAMPTZ,
    crl_url TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- 索引
CREATE INDEX idx_certs_vin ON ccc_certificates(vin);
CREATE INDEX idx_certs_type ON ccc_certificates(cert_type);
CREATE INDEX idx_certs_status ON ccc_certificates(status);
CREATE INDEX idx_certs_valid ON ccc_certificates(valid_until) WHERE status = 'active';
```

---

### 1.4 设备表 (devices)

手机/钥匙设备管理

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| device_id | UUID | PK | 设备ID |
| user_id | UUID | FK → users | 所属用户 |
| device_type | VARCHAR(20) | NOT NULL | ios/android/watch/card |
| device_name | VARCHAR(100) | | 设备名称 |
| device_model | VARCHAR(50) | | 设备型号 |
| device_identifier | BYTEA | UNIQUE | 16字节设备标识 |
| secure_element_id | BYTEA | | SE芯片ID (如SE050) |
| public_key | BYTEA | | 设备公钥 |
| attestation_cert | BYTEA | | 设备认证证书 |
| paired_at | TIMESTAMPTZ | | 配对时间 |
| last_used_at | TIMESTAMPTZ | | 最后使用时间 |
| is_active | BOOLEAN | DEFAULT TRUE | 是否激活 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 创建时间 |

```sql
CREATE TABLE devices (
    device_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    device_type VARCHAR(20) NOT NULL CHECK (device_type IN ('ios', 'android', 'watch', 'card')),
    device_name VARCHAR(100),
    device_model VARCHAR(50),
    device_identifier BYTEA UNIQUE,
    secure_element_id BYTEA,
    public_key BYTEA,
    attestation_cert BYTEA,
    paired_at TIMESTAMPTZ,
    last_used_at TIMESTAMPTZ,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_devices_user ON devices(user_id);
CREATE INDEX idx_devices_identifier ON devices(device_identifier);
```

---

### 1.5 钥匙授权记录表 (key_authorizations)

钥匙分享/授权历史

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| auth_id | UUID | PK | 授权记录ID |
| slot_id | UUID | FK → ccc_slots | 关联槽位 |
| owner_user_id | UUID | FK → users | 车主 |
| guest_user_id | UUID | FK → users | 被授权人 |
| guest_email | VARCHAR(254) | | 被授权人邮箱 |
| key_type | VARCHAR(20) | NOT NULL | friend/valet/repair |
| authorized_actions | JSONB | | 授权操作 |
| restrictions | JSONB | | 限制条件 |
| valid_from | TIMESTAMPTZ | NOT NULL | 生效时间 |
| valid_until | TIMESTAMPTZ | | 过期时间 |
| share_method | VARCHAR(20) | | email/qrcode/nfc |
| status | VARCHAR(20) | DEFAULT 'active' | active/expired/revoked |
| revoked_at | TIMESTAMPTZ | | 吊销时间 |
| revoke_reason | TEXT | | 吊销原因 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 创建时间 |

```sql
CREATE TABLE key_authorizations (
    auth_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    slot_id UUID NOT NULL REFERENCES ccc_slots(slot_id) ON DELETE CASCADE,
    owner_user_id UUID NOT NULL REFERENCES users(id),
    guest_user_id UUID REFERENCES users(id),
    guest_email VARCHAR(254),
    key_type VARCHAR(20) NOT NULL CHECK (key_type IN ('friend', 'valet', 'repair')),
    authorized_actions JSONB,
    restrictions JSONB,
    valid_from TIMESTAMPTZ NOT NULL,
    valid_until TIMESTAMPTZ,
    share_method VARCHAR(20),
    status VARCHAR(20) DEFAULT 'active',
    revoked_at TIMESTAMPTZ,
    revoke_reason TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_auth_owner ON key_authorizations(owner_user_id);
CREATE INDEX idx_auth_guest ON key_authorizations(guest_user_id);
CREATE INDEX idx_auth_slot ON key_authorizations(slot_id);
```

---

### 1.6 SE安全模块表 (secure_elements)

SE050等安全芯片管理

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| se_id | UUID | PK | SE记录ID |
| device_id | UUID | FK → devices | 关联设备 |
| se_type | VARCHAR(20) | NOT NULL | SE050/SE051/其他 |
| se_unique_id | BYTEA | UNIQUE | SE唯一标识 |
| scp03_key_version | INTEGER | | SCP03密钥版本 |
| key_diversification_data | BYTEA | | 密钥分散数据 |
| attestation_status | VARCHAR(20) | DEFAULT 'pending' | pending/verified/failed |
| provisioned_at | TIMESTAMPTZ | | 预置时间 |
| last_session_at | TIMESTAMPTZ | | 最后会话时间 |
| session_counter | INTEGER | DEFAULT 0 | 会话计数器 |
| created_at | TIMESTAMPTZ | DEFAULT NOW() | 创建时间 |

```sql
CREATE TABLE secure_elements (
    se_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    device_id UUID REFERENCES devices(device_id) ON DELETE SET NULL,
    se_type VARCHAR(20) NOT NULL,
    se_unique_id BYTEA UNIQUE,
    scp03_key_version INTEGER,
    key_diversification_data BYTEA,
    attestation_status VARCHAR(20) DEFAULT 'pending',
    provisioned_at TIMESTAMPTZ,
    last_session_at TIMESTAMPTZ,
    session_counter INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW()
);
```

---

## 二、数据关系图

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│     users       │       │    vehicles     │       │    devices      │
├─────────────────┤       ├─────────────────┤       ├─────────────────┤
│ id (PK)         │◄──────┤ user_id (FK)    │       │ device_id (PK)  │
│ email           │       │ vin (PK)        │◄─────┤│ user_id (FK)    │
│ password_hash   │       │ vehicle_model   │       │ device_type     │
│ name            │       │ vehicle_brand   │       │ device_identifier│
└─────────────────┘       │ tbox_device_id  │       │ secure_element_id│
         │                └─────────────────┘       └────────┬────────┘
         │                         │                        │
         │                         │                        ▼
         │                         │              ┌─────────────────┐
         │                         │              │ secure_elements │
         │                         │              ├─────────────────┤
         │                         │              │ se_id (PK)      │
         │                         │              │ device_id (FK)  │
         │                         │              │ se_unique_id    │
         │                         │              │ scp03_key_version│
         │                         │              └─────────────────┘
         │                         ▼
         │                ┌─────────────────┐
         │                │   ccc_slots     │
         │                ├─────────────────┤
         │                │ slot_id (PK)    │
         └───────────────►│ vin (FK)        │
                          │ slot_identifier │ (0-7)
                          │ key_identifier  │ (8 bytes)
                          │ device_identifier│ (16 bytes)
                          │ slot_state      │
                          │ key_type        │
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐       ┌─────────────────┐
                          │ ccc_certificates│       │key_authorizations│
                          ├─────────────────┤       ├─────────────────┤
                          │ cert_id (PK)    │       │ auth_id (PK)    │
                          │ vin (FK)        │       │ slot_id (FK)    │
                          │ cert_type       │       │ owner_user_id   │
                          │ cert_data       │       │ guest_user_id   │
                          │ cert_hash       │       │ key_type        │
                          │ serial_number   │       │ valid_from/until│
                          │ issuer/subject  │       │ status          │
                          │ is_custom       │       └─────────────────┘
                          │ oem_id          │
                          │ key_id (FK)     │
                          └─────────────────┘
```

---

## 三、关键标识符说明

| 标识符 | 长度 | 来源 | 存储位置 |
|--------|------|------|----------|
| **VIN** | 17字符 | 车辆制造 | vehicles.vin (PK) |
| **vehicle_identifier** | 16字节 | CCC规范 | vehicles.device_identifier |
| **slot_identifier** | 1字节 (0-7) | CCC分配 | ccc_slots.slot_identifier |
| **key_identifier** | 8字节 | CCC生成 | ccc_slots.key_identifier |
| **device_identifier** | 16字节 | 设备制造 | devices.device_identifier |
| **SE_unique_id** | 18字节 | SE芯片 | secure_elements.se_unique_id |

---

## 四、证书类型说明

| cert_type | 说明 | 来源 | is_custom |
|-----------|------|------|-----------|
| **vehicle** | 车辆证书 | 车厂预置 | FALSE |
| **vehicleOEM** | 车厂证书 | 车厂CA | FALSE |
| **device** | 设备证书 | 设备制造商 | FALSE |
| **intermediate** | 中间CA | PKI体系 | FALSE |
| **root** | 根证书 | CCC联盟 | FALSE |
| **custom** | 自定义证书 | 第三方 | TRUE |

---

## 五、典型查询

### 5.1 获取车辆所有钥匙
```sql
SELECT 
    v.vin, v.vehicle_model,
    cs.slot_identifier, cs.key_identifier,
    cs.slot_state, cs.key_type,
    c.cert_hash, c.valid_until
FROM vehicles v
LEFT JOIN ccc_slots cs ON v.vin = cs.vin
LEFT JOIN ccc_certificates c ON cs.slot_id = c.key_id
WHERE v.vin = 'LSVAG2180E2100001';
```

### 5.2 检查过期证书
```sql
SELECT 
    vin, cert_type, serial_number, valid_until
FROM ccc_certificates
WHERE status = 'active' 
  AND valid_until < NOW() + INTERVAL '30 days';
```

### 5.3 设备钥匙授权
```sql
SELECT 
    d.device_name, d.device_identifier,
    cs.key_type, cs.valid_until,
    ka.authorized_actions
FROM devices d
JOIN ccc_slots cs ON d.device_identifier = cs.device_identifier
JOIN key_authorizations ka ON cs.slot_id = ka.slot_id
WHERE d.user_id = ? AND ka.status = 'active';
```

---

## 六、与BER-TLV协议的映射

| TLV TAG | 数据库字段 | 表 |
|---------|-----------|-----|
| 0x41 (VIN) | vin | vehicles |
| 0x46 (SLOT_ID) | slot_identifier | ccc_slots |
| 0x47 (KEY_ID) | key_identifier | ccc_slots |
| 0x48 (DEVICE_ID) | device_identifier | devices |
| 0x49 (CERT_CHAIN) | cert_data | ccc_certificates |
| 0x4A (OEM_ID) | oem_id | ccc_certificates |

---

*文档版本: v1.0.0*  
*关联规范: CCC Digital Key Specification v3.0*
