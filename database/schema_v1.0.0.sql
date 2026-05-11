-- ============================================================
-- yuleDKCS 车云一体化数据库 Schema v1.0.0
-- 支持: CCC Digital Key + ICCOA + ICCE 协议
-- 数据库: PostgreSQL 15+
-- 时区: UTC
-- 编码: UTF-8
-- ============================================================

-- 扩展启用
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- ============================================================
-- 第一部分: 用户与基础数据 (原有)
-- ============================================================

CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email VARCHAR(254) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    name VARCHAR(50) NOT NULL,
    phone VARCHAR(20),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_users_email ON users(email);

-- ============================================================
-- 第二部分: 车辆管理 (CCC/ICCOA/ICCE 共用)
-- ============================================================

CREATE TABLE vehicles (
    vin VARCHAR(17) PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id),
    
    -- 车辆基本信息
    vehicle_model VARCHAR(100) NOT NULL,
    vehicle_brand VARCHAR(50) NOT NULL,
    license_plate VARCHAR(20) UNIQUE,
    manufacture_date DATE,
    color VARCHAR(30),
    
    -- 状态管理
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'inactive', 'sold', 'suspended')),
    
    -- T-Box/车机信息
    tbox_device_id VARCHAR(64) UNIQUE,
    tbox_model VARCHAR(50),
    ivi_device_id VARCHAR(64),
    ecu_version VARCHAR(50),
    
    -- 协议支持标记
    support_ccc BOOLEAN DEFAULT FALSE,
    support_iccoa BOOLEAN DEFAULT FALSE,
    support_icce BOOLEAN DEFAULT FALSE,
    
    -- 在线状态
    last_online_at TIMESTAMPTZ,
    ip_address INET,
    
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_vehicles_user ON vehicles(user_id);
CREATE INDEX idx_vehicles_status ON vehicles(status);
CREATE INDEX idx_vehicles_tbox ON vehicles(tbox_device_id);
CREATE INDEX idx_vehicles_online ON vehicles(last_online_at);

-- 车辆别名表 (支持多用户共享车辆时的个性化命名)
CREATE TABLE vehicle_aliases (
    alias_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vin VARCHAR(17) NOT NULL REFERENCES vehicles(vin) ON DELETE CASCADE,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    alias_name VARCHAR(100) NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(vin, user_id)
);

-- ============================================================
-- 第三部分: 数字钥匙槽位 (CCC标准)
-- ============================================================

CREATE TABLE ccc_slots (
    slot_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vin VARCHAR(17) NOT NULL REFERENCES vehicles(vin) ON DELETE CASCADE,
    
    -- CCC槽位标识 (0-7)
    slot_identifier INTEGER NOT NULL CHECK (slot_identifier BETWEEN 0 AND 7),
    
    -- 密钥标识
    key_identifier BYTEA NOT NULL,
    key_type VARCHAR(20) CHECK (key_type IN ('owner', 'friend', 'valet', 'repair', 'fleet')),
    
    -- 设备绑定
    device_identifier BYTEA,
    se_identifier BYTEA,
    
    -- 状态管理
    slot_state VARCHAR(20) DEFAULT 'empty' CHECK (slot_state IN ('empty', 'active', 'suspended', 'revoked')),
    
    -- 有效期
    valid_from TIMESTAMPTZ,
    valid_until TIMESTAMPTZ,
    
    -- 授权操作 (JSON数组: ["lock", "unlock", "engine_start", "trunk"])
    authorized_actions JSONB DEFAULT '["lock", "unlock"]'::jsonb,
    
    -- 共享限制
    sharing_count INTEGER DEFAULT 0,
    max_sharing INTEGER DEFAULT 3,
    
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    
    UNIQUE(vin, slot_identifier),
    UNIQUE(key_identifier)
);

CREATE INDEX idx_ccc_slots_vin ON ccc_slots(vin);
CREATE INDEX idx_ccc_slots_key_id ON ccc_slots(key_identifier);
CREATE INDEX idx_ccc_slots_state ON ccc_slots(slot_state);
CREATE INDEX idx_ccc_slots_device ON ccc_slots(device_identifier);

-- ============================================================
-- 第四部分: ICCOA钥匙管理
-- ============================================================

CREATE TABLE iccoa_keys (
    key_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vin VARCHAR(17) NOT NULL REFERENCES vehicles(vin) ON DELETE CASCADE,
    
    -- ICCOA特定标识
    phone_number VARCHAR(20),
    iccoa_key_id VARCHAR(64) UNIQUE,
    
    -- 密钥数据
    auth_key BYTEA,
    encrypt_key BYTEA,
    
    -- 设备信息
    device_type VARCHAR(20) CHECK (device_type IN ('ios', 'android')),
    device_token TEXT,
    
    -- 状态
    key_status VARCHAR(20) DEFAULT 'active' CHECK (key_status IN ('active', 'inactive', 'revoked')),
    
    -- 有效期
    valid_from TIMESTAMPTZ DEFAULT NOW(),
    valid_until TIMESTAMPTZ,
    
    -- 权限 (位掩码: 1=解锁 2=闭锁 4=启动 8=后备箱)
    permission_mask INTEGER DEFAULT 3,
    
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_iccoa_vin ON iccoa_keys(vin);
CREATE INDEX idx_iccoa_phone ON iccoa_keys(phone_number);
CREATE INDEX idx_iccoa_status ON iccoa_keys(key_status);

-- ============================================================
-- 第五部分: ICCE钥匙管理
-- ============================================================

CREATE TABLE icce_keys (
    key_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    vin VARCHAR(17) NOT NULL REFERENCES vehicles(vin) ON DELETE CASCADE,
    
    -- ICCE特定标识
    icce_key_identifier BYTEA UNIQUE,
    vehicle_pk BYTEA,
    
    -- 密钥数据
    device_sk BYTEA,
    session_key BYTEA,
    
    -- 设备绑定
    device_id VARCHAR(64),
    se_id VARCHAR(64),
    
    -- 状态
    key_status VARCHAR(20) DEFAULT 'active',
    
    -- 计数器 (防重放)
    vehicle_counter INTEGER DEFAULT 0,
    device_counter INTEGER DEFAULT 0,
    
    valid_from TIMESTAMPTZ DEFAULT NOW(),
    valid_until TIMESTAMPTZ,
    
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_icce_vin ON icce_keys(vin);
CREATE INDEX idx_icce_device ON icce_keys(device_id);

-- ============================================================
-- 第六部分: 设备管理 (跨协议共用)
-- ============================================================

CREATE TABLE devices (
    device_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    
    -- 设备类型
    device_type VARCHAR(20) NOT NULL CHECK (device_type IN ('ios', 'android', 'watch', 'card', 'fob')),
    
    -- 设备信息
    device_name VARCHAR(100),
    device_model VARCHAR(50),
    os_version VARCHAR(20),
    app_version VARCHAR(20),
    
    -- 设备标识 (16字节)
    device_identifier BYTEA UNIQUE,
    
    -- 安全芯片信息
    se_type VARCHAR(20), -- SE050/SE051/TEE/other
    se_identifier BYTEA,
    se_attestation BYTEA,
    
    -- 密钥信息
    public_key BYTEA,
    attestation_cert BYTEA,
    
    -- 协议支持
    support_ccc BOOLEAN DEFAULT FALSE,
    support_iccoa BOOLEAN DEFAULT FALSE,
    support_icce BOOLEAN DEFAULT FALSE,
    
    -- 状态
    is_active BOOLEAN DEFAULT TRUE,
    is_primary BOOLEAN DEFAULT FALSE, -- 主设备标记
    
    -- 时间戳
    paired_at TIMESTAMPTZ,
    last_used_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_devices_user ON devices(user_id);
CREATE INDEX idx_devices_identifier ON devices(device_identifier);
CREATE INDEX idx_devices_active ON devices(user_id, is_active);

-- ============================================================
-- 第七部分: 安全芯片详细表 (SE050等)
-- ============================================================

CREATE TABLE secure_elements (
    se_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    device_id UUID REFERENCES devices(device_id) ON DELETE SET NULL,
    
    -- SE类型
    se_type VARCHAR(20) NOT NULL CHECK (se_type IN ('SE050', 'SE051', 'SE210', 'TEE', 'other')),
    
    -- 唯一标识
    se_unique_id BYTEA UNIQUE,
    
    -- SCP03密钥版本
    scp03_key_version INTEGER DEFAULT 0x10,
    scp03_static_key BYTEA,
    scp03_session_counter INTEGER DEFAULT 0,
    
    -- 密钥分散数据
    key_diversification_data BYTEA,
    
    -- 认证状态
    attestation_status VARCHAR(20) DEFAULT 'pending' CHECK (attestation_status IN ('pending', 'verified', 'failed')),
    attestation_cert BYTEA,
    
    -- 时间戳
    provisioned_at TIMESTAMPTZ,
    last_session_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_se_device ON secure_elements(device_id);
CREATE INDEX idx_se_type ON secure_elements(se_type);

-- ============================================================
-- 第八部分: 证书管理 (CCC/ICCOA/ICCE)
-- ============================================================

CREATE TABLE certificates (
    cert_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 关联信息
    vin VARCHAR(17) REFERENCES vehicles(vin) ON DELETE CASCADE,
    device_id UUID REFERENCES devices(device_id) ON DELETE CASCADE,
    se_id UUID REFERENCES secure_elements(se_id) ON DELETE CASCADE,
    
    -- 证书类型
    cert_type VARCHAR(30) NOT NULL CHECK (
        cert_type IN ('root', 'intermediate', 'oem', 'vehicle', 'device', 'ecu', 'se', 'custom')
    ),
    
    -- 协议来源
    protocol VARCHAR(10) CHECK (protocol IN ('CCC', 'ICCOA', 'ICCE', 'OTHER')),
    
    -- 证书数据
    cert_data BYTEA NOT NULL,
    cert_hash BYTEA UNIQUE,
    serial_number VARCHAR(64) UNIQUE,
    
    -- 解析字段
    issuer_dn TEXT,
    subject_dn TEXT,
    subject_key_id BYTEA,
    authority_key_id BYTEA,
    
    -- 有效期
    valid_from TIMESTAMPTZ NOT NULL,
    valid_until TIMESTAMPTZ NOT NULL,
    
    -- 状态
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'expired', 'revoked')),
    revoked_at TIMESTAMPTZ,
    revocation_reason TEXT,
    
    -- CRL/OCSP
    crl_url TEXT,
    ocsp_url TEXT,
    
    -- 父证书 (链)
    parent_cert_id UUID REFERENCES certificates(cert_id),
    
    -- 自定义标记
    is_custom BOOLEAN DEFAULT FALSE,
    custom_source VARCHAR(50),
    
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_certs_vin ON certificates(vin);
CREATE INDEX idx_certs_device ON certificates(device_id);
CREATE INDEX idx_certs_type ON certificates(cert_type);
CREATE INDEX idx_certs_protocol ON certificates(protocol);
CREATE INDEX idx_certs_status ON certificates(status);
CREATE INDEX idx_certs_valid ON certificates(valid_until) WHERE status = 'active';

-- 证书链视图
CREATE VIEW certificate_chains AS
WITH RECURSIVE chain AS (
    SELECT 
        cert_id,
        vin,
        cert_type,
        protocol,
        serial_number,
        parent_cert_id,
        1 as level,
        ARRAY[cert_id] as chain_path
    FROM certificates
    WHERE parent_cert_id IS NULL
    
    UNION ALL
    
    SELECT 
        c.cert_id,
        c.vin,
        c.cert_type,
        c.protocol,
        c.serial_number,
        c.parent_cert_id,
        ch.level + 1,
        ch.chain_path || c.cert_id
    FROM certificates c
    JOIN chain ch ON c.parent_cert_id = ch.cert_id
)
SELECT * FROM chain;

-- ============================================================
-- 第九部分: 钥匙授权分享记录
-- ============================================================

CREATE TABLE key_shares (
    share_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 关联槽位
    slot_id UUID REFERENCES ccc_slots(slot_id) ON DELETE CASCADE,
    iccoa_key_id UUID REFERENCES iccoa_keys(key_id) ON DELETE CASCADE,
    
    -- 分享关系
    owner_user_id UUID NOT NULL REFERENCES users(id),
    guest_user_id UUID REFERENCES users(id),
    guest_email VARCHAR(254),
    guest_phone VARCHAR(20),
    
    -- 钥匙类型
    key_type VARCHAR(20) CHECK (key_type IN ('friend', 'valet', 'repair', 'temporary')),
    
    -- 权限配置
    authorized_actions JSONB,
    restrictions JSONB, -- 如时间限制、地理围栏
    
    -- 分享方式
    share_method VARCHAR(20) CHECK (share_method IN ('email', 'sms', 'qrcode', 'nfc', 'bt')),
    share_token VARCHAR(255),
    
    -- 有效期
    valid_from TIMESTAMPTZ NOT NULL,
    valid_until TIMESTAMPTZ,
    
    -- 使用统计
    use_count INTEGER DEFAULT 0,
    max_uses INTEGER,
    
    -- 状态
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('pending', 'active', 'accepted', 'rejected', 'expired', 'revoked')),
    
    -- 撤销信息
    revoked_at TIMESTAMPTZ,
    revoke_reason TEXT,
    revoked_by UUID REFERENCES users(id),
    
    created_at TIMESTAMPTZ DEFAULT NOW(),
    accepted_at TIMESTAMPTZ
);

CREATE INDEX idx_shares_owner ON key_shares(owner_user_id);
CREATE INDEX idx_shares_guest ON key_shares(guest_user_id);
CREATE INDEX idx_shares_status ON key_shares(status);

-- ============================================================
-- 第十部分: 操作审计日志
-- ============================================================

CREATE TABLE operation_logs (
    log_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 操作信息
    operation_type VARCHAR(50) NOT NULL,
    operation_detail JSONB,
    
    -- 关联实体
    vin VARCHAR(17) REFERENCES vehicles(vin),
    user_id UUID REFERENCES users(id),
    device_id UUID REFERENCES devices(device_id),
    
    -- 协议信息
    protocol VARCHAR(10),
    key_type VARCHAR(20),
    
    -- 结果
    result VARCHAR(20) CHECK (result IN ('success', 'failure', 'timeout')),
    error_code VARCHAR(50),
    error_message TEXT,
    
    -- 上下文
    ip_address INET,
    geo_location POINT,
    
    created_at TIMESTAMPTZ DEFAULT NOW()
) PARTITION BY RANGE (created_at);

-- 创建分区表 (按月)
CREATE TABLE operation_logs_2024_01 PARTITION OF operation_logs
    FOR VALUES FROM ('2024-01-01') TO ('2024-02-01');

CREATE INDEX idx_logs_vin ON operation_logs(vin);
CREATE INDEX idx_logs_user ON operation_logs(user_id);
CREATE INDEX idx_logs_time ON operation_logs(created_at);
CREATE INDEX idx_logs_type ON operation_logs(operation_type);

-- ============================================================
-- 第十一部分: 车辆遥测数据 (TimescaleDB)
-- ============================================================

CREATE TABLE vehicle_telemetry (
    time TIMESTAMPTZ NOT NULL,
    vin VARCHAR(17) NOT NULL REFERENCES vehicles(vin),
    
    -- 位置
    latitude DOUBLE PRECISION,
    longitude DOUBLE PRECISION,
    altitude DOUBLE PRECISION,
    heading DOUBLE PRECISION,
    speed DOUBLE PRECISION,
    
    -- 车辆状态
    engine_status VARCHAR(20),
    door_status JSONB,
    window_status JSONB,
    trunk_status VARCHAR(10),
    hood_status VARCHAR(10),
    
    -- 电池/油量
    battery_level INTEGER,
    fuel_level INTEGER,
    
    -- 胎压
    tire_pressure JSONB,
    
    -- 里程
    odometer BIGINT,
    trip_distance INTEGER,
    
    -- 信号强度
    signal_strength INTEGER,
    network_type VARCHAR(10),
    
    -- 原始TLV数据
    raw_data BYTEA
);

-- 转换为TimescaleDB hypertable (如使用TimescaleDB)
-- SELECT create_hypertable('vehicle_telemetry', 'time');

CREATE INDEX idx_telemetry_vin_time ON vehicle_telemetry(vin, time DESC);

-- ============================================================
-- 第十二部分: 消息队列表 (用于异步处理)
-- ============================================================

CREATE TABLE message_queue (
    msg_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    
    -- 消息类型
    msg_type VARCHAR(50) NOT NULL,
    
    -- 目标
    target_protocol VARCHAR(10) CHECK (target_protocol IN ('CCC', 'ICCOA', 'ICCE')),
    vin VARCHAR(17),
    device_id UUID,
    
    -- 消息内容
    payload BYTEA NOT NULL,
    payload_format VARCHAR(10) DEFAULT 'TLV',
    
    -- 优先级 (1-10, 1最高)
    priority INTEGER DEFAULT 5,
    
    -- 状态
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'processing', 'completed', 'failed', 'retry')),
    
    -- 重试
    retry_count INTEGER DEFAULT 0,
    max_retries INTEGER DEFAULT 3,
    next_retry_at TIMESTAMPTZ,
    
    -- 结果
    response_data BYTEA,
    error_info TEXT,
    
    -- 时间
    created_at TIMESTAMPTZ DEFAULT NOW(),
    processed_at TIMESTAMPTZ
);

CREATE INDEX idx_queue_status ON message_queue(status);
CREATE INDEX idx_queue_priority ON message_queue(priority DESC, created_at);
CREATE INDEX idx_queue_vin ON message_queue(vin);

-- ============================================================
-- 第十三部分: 触发器函数
-- ============================================================

-- 自动更新 updated_at
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- 应用到各表
CREATE TRIGGER update_vehicles_updated_at BEFORE UPDATE ON vehicles
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_ccc_slots_updated_at BEFORE UPDATE ON ccc_slots
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_iccoa_keys_updated_at BEFORE UPDATE ON iccoa_keys
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_icce_keys_updated_at BEFORE UPDATE ON icce_keys
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- 证书过期检查触发器
CREATE OR REPLACE FUNCTION check_cert_expiry()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.valid_until < NOW() THEN
        NEW.status = 'expired';
    END IF;
    RETURN NEW;
END;
$$ language 'plpgsql';

CREATE TRIGGER check_cert_expiry_trigger BEFORE INSERT OR UPDATE ON certificates
    FOR EACH ROW EXECUTE FUNCTION check_cert_expiry();

-- ============================================================
-- 第十四部分: 常用视图
-- ============================================================

-- 车辆完整信息视图
CREATE VIEW vehicle_full_info AS
SELECT 
    v.*,
    u.name as owner_name,
    u.email as owner_email,
    COUNT(DISTINCT cs.slot_id) as total_keys,
    COUNT(DISTINCT CASE WHEN cs.slot_state = 'active' THEN cs.slot_id END) as active_keys
FROM vehicles v
JOIN users u ON v.user_id = u.id
LEFT JOIN ccc_slots cs ON v.vin = cs.vin
GROUP BY v.vin, u.name, u.email;

-- 设备钥匙汇总视图
CREATE VIEW device_key_summary AS
SELECT 
    d.*,
    v.vin,
    v.vehicle_model,
    v.vehicle_brand,
    cs.slot_identifier,
    cs.key_type,
    cs.slot_state as key_state
FROM devices d
LEFT JOIN ccc_slots cs ON d.device_identifier = cs.device_identifier
LEFT JOIN vehicles v ON cs.vin = v.vin;

-- ============================================================
-- Schema版本记录
-- ============================================================

CREATE TABLE schema_version (
    version VARCHAR(20) PRIMARY KEY,
    applied_at TIMESTAMPTZ DEFAULT NOW(),
    description TEXT
);

INSERT INTO schema_version (version, description) VALUES 
    ('1.0.0', 'Initial schema with CCC/ICCOA/ICCE support');
