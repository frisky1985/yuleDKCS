-- yuleDKCS Database Schema
-- Version: 1.0.0
-- Date: 2026-05-06

-- ============================================================
-- 扩展
-- ============================================================

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- ============================================================
-- 枚举类型
-- ============================================================

-- 密钥类型
CREATE TYPE key_type AS ENUM (
    'owner',      -- 车主密钥
    'family',     -- 家庭密钥
    'friend',     -- 朋友密钥
    'service',    -- 服务密钥
    'temporary'   -- 临时密钥
);

-- 密钥状态
CREATE TYPE key_state AS ENUM (
    'pending',    -- 待激活
    'active',     -- 已激活
    'suspended',  -- 已挂起
    'revoked',    -- 已吊销
    'expired'     -- 已过期
);

-- 协议类型
CREATE TYPE protocol_type AS ENUM (
    'ccc',        -- CCC 3.0
    'iccoa',      -- ICCOA DK3.0/DK4.0
    'icce'        -- ICCE
);

-- 指令类型
CREATE TYPE command_type AS ENUM (
    'unlock', 'lock', 'engine_start', 'engine_stop',
    'trunk_open', 'panic', 'find', 'climate_on', 'climate_off',
    'window_open', 'window_close'
);

-- 指令状态
CREATE TYPE command_state AS ENUM (
    'pending',    -- 待发送
    'sent',       -- 已发送
    'executing',  -- 执行中
    'completed',  -- 已完成
    'failed',     -- 失败
    'timeout',    -- 超时
    'rejected'    -- 被拒绝
);

-- 设备类型
CREATE TYPE device_type AS ENUM (
    'smartphone',
    'tablet',
    'wearable',
    'key_fob'
);

-- 车辆状态
CREATE TYPE vehicle_state AS ENUM (
    'online',
    'offline',
    'sleep',
    'maintenance'
);

-- ============================================================
-- 用户表
-- ============================================================

CREATE TABLE users (
    user_id         UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    phone_encrypted BYTEA,                    -- AES 加密的手机号
    phone_hash      BYTEA,                    -- 手机号哈希 (用于查询)
    email_encrypted BYTEA,                    -- AES 加密的邮箱
    email_hash      BYTEA,                    -- 邮箱哈希
    name            VARCHAR(100),
    avatar_url      VARCHAR(500),
    status          VARCHAR(20) DEFAULT 'active',
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_login_at   TIMESTAMP WITH TIME ZONE
);

CREATE INDEX idx_users_phone_hash ON users(phone_hash);
CREATE INDEX idx_users_email_hash ON users(email_hash);

-- ============================================================
-- 设备表
-- ============================================================

CREATE TABLE devices (
    device_id       UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id         UUID NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    device_type     device_type NOT NULL,
    platform        VARCHAR(20) NOT NULL,      -- android / ios
    platform_version VARCHAR(20),
    device_model    VARCHAR(100),
    device_name     VARCHAR(100),
    push_token      VARCHAR(500),              -- FCM / APNs Token
    capabilities    JSONB,                     -- {ble: true, nfc: true, uwb: true, se: true}
    is_active       BOOLEAN DEFAULT true,
    last_active_at  TIMESTAMP WITH TIME ZONE,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_devices_user_id ON devices(user_id);
CREATE INDEX idx_devices_platform ON devices(platform);

-- ============================================================
-- 车辆表
-- ============================================================

CREATE TABLE vehicles (
    vehicle_id      VARCHAR(17) PRIMARY KEY,   -- VIN
    owner_user_id   UUID REFERENCES users(user_id) ON DELETE SET NULL,
    vehicle_model   VARCHAR(100),
    vehicle_name    VARCHAR(100),
    vehicle_state   vehicle_state DEFAULT 'offline',
    protocol        protocol_type,             -- 支持的协议
    last_online_at  TIMESTAMP WITH TIME ZONE,
    last_location   JSONB,                     -- {latitude, longitude, accuracy}
    metadata        JSONB,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_vehicles_owner ON vehicles(owner_user_id);
CREATE INDEX idx_vehicles_state ON vehicles(vehicle_state);

-- ============================================================
-- 密钥表
-- ============================================================

CREATE TABLE keys (
    key_id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    vehicle_id      VARCHAR(17) NOT NULL REFERENCES vehicles(vehicle_id) ON DELETE CASCADE,
    device_id       UUID NOT NULL REFERENCES devices(device_id) ON DELETE CASCADE,
    user_id         UUID NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    key_type        key_type NOT NULL,
    key_state       key_state DEFAULT 'pending',
    protocol        protocol_type NOT NULL,
    permissions     JSONB NOT NULL,            -- ["unlock", "lock", ...]
    key_data_ref    VARCHAR(100),              -- SE050 密钥引用
    valid_from      TIMESTAMP WITH TIME ZONE,
    valid_until     TIMESTAMP WITH TIME ZONE,
    use_count       INTEGER DEFAULT 0,
    max_uses        INTEGER,                   -- NULL 表示无限制
    metadata        JSONB,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    activated_at    TIMESTAMP WITH TIME ZONE,
    revoked_at      TIMESTAMP WITH TIME ZONE,
    updated_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_keys_vehicle ON keys(vehicle_id);
CREATE INDEX idx_keys_device ON keys(device_id);
CREATE INDEX idx_keys_user ON keys(user_id);
CREATE INDEX idx_keys_state ON keys(key_state);
CREATE INDEX idx_keys_type ON keys(key_type);

-- ============================================================
-- 分享表
-- ============================================================

CREATE TABLE shares (
    share_id        UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    key_id          UUID NOT NULL REFERENCES keys(key_id) ON DELETE CASCADE,
    share_code      VARCHAR(50) UNIQUE NOT NULL,
    share_url       VARCHAR(500),
    recipient_phone_encrypted BYTEA,
    recipient_phone_hash BYTEA,
    recipient_email_encrypted BYTEA,
    recipient_email_hash BYTEA,
    share_type      key_type NOT NULL,
    share_state     key_state DEFAULT 'pending',
    valid_from      TIMESTAMP WITH TIME ZONE,
    valid_until     TIMESTAMP WITH TIME ZONE,
    max_uses        INTEGER,
    use_count       INTEGER DEFAULT 0,
    permissions     JSONB,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    accepted_at     TIMESTAMP WITH TIME ZONE,
    cancelled_at    TIMESTAMP WITH TIME ZONE
);

CREATE INDEX idx_shares_key ON shares(key_id);
CREATE INDEX idx_shares_code ON shares(share_code);
CREATE INDEX idx_shares_state ON shares(share_state);

-- ============================================================
-- 指令表
-- ============================================================

CREATE TABLE commands (
    command_id      UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    vehicle_id      VARCHAR(17) NOT NULL REFERENCES vehicles(vehicle_id),
    key_id          UUID NOT NULL REFERENCES keys(key_id),
    device_id       UUID NOT NULL REFERENCES devices(device_id),
    command_type    command_type NOT NULL,
    command_state   command_state DEFAULT 'pending',
    payload         BYTEA,                     -- BER-TLV 编码
    result          JSONB,
    error_code      INTEGER,
    error_message   TEXT,
    location        JSONB,
    timeout_ms      INTEGER DEFAULT 30000,
    trace_id        VARCHAR(100),
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    sent_at         TIMESTAMP WITH TIME ZONE,
    completed_at    TIMESTAMP WITH TIME ZONE
);

CREATE INDEX idx_commands_vehicle ON commands(vehicle_id);
CREATE INDEX idx_commands_key ON commands(key_id);
CREATE INDEX idx_commands_state ON commands(command_state);
CREATE INDEX idx_commands_created ON commands(created_at DESC);

-- ============================================================
-- 事件表
-- ============================================================

CREATE TABLE events (
    event_id        UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    event_type      VARCHAR(50) NOT NULL,
    vehicle_id      VARCHAR(17) REFERENCES vehicles(vehicle_id) ON DELETE SET NULL,
    key_id          UUID REFERENCES keys(key_id) ON DELETE SET NULL,
    device_id       UUID REFERENCES devices(device_id) ON DELETE SET NULL,
    user_id         UUID REFERENCES users(user_id) ON DELETE SET NULL,
    location        JSONB,
    payload         JSONB,
    result          VARCHAR(20),
    error_code      INTEGER,
    error_message   TEXT,
    trace_id        VARCHAR(100),
    timestamp       TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_events_type ON events(event_type);
CREATE INDEX idx_events_vehicle ON events(vehicle_id);
CREATE INDEX idx_events_user ON events(user_id);
CREATE INDEX idx_events_timestamp ON events(timestamp DESC);

-- 分区 (按月分区，提升查询性能)
-- CREATE TABLE events_2026_05 PARTITION OF events
--     FOR VALUES FROM ('2026-05-01') TO ('2026-06-01');

-- ============================================================
-- 审计日志表
-- ============================================================

CREATE TABLE audit_logs (
    log_id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    event_type      VARCHAR(50) NOT NULL,
    user_id         UUID,
    device_id       UUID,
    vehicle_id      VARCHAR(17),
    key_id          UUID,
    action          VARCHAR(100) NOT NULL,
    resource        VARCHAR(200),
    result          VARCHAR(20) NOT NULL,
    error_code      INTEGER,
    error_message   TEXT,
    source_ip       INET,
    user_agent      TEXT,
    trace_id        VARCHAR(100),
    metadata        JSONB,
    timestamp       TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_audit_timestamp ON audit_logs(timestamp DESC);
CREATE INDEX idx_audit_user ON audit_logs(user_id);
CREATE INDEX idx_audit_type ON audit_logs(event_type);

-- ============================================================
-- 会话表 (可选，用于 Token 黑名单)
-- ============================================================

CREATE TABLE sessions (
    session_id      UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id         UUID NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    device_id       UUID REFERENCES devices(device_id) ON DELETE CASCADE,
    token_jti       VARCHAR(100) UNIQUE NOT NULL,
    is_revoked      BOOLEAN DEFAULT false,
    expires_at      TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_sessions_user ON sessions(user_id);
CREATE INDEX idx_sessions_jti ON sessions(token_jti);

-- ============================================================
-- 触发器
-- ============================================================

-- 自动更新 updated_at
CREATE OR REPLACE FUNCTION update_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_users_updated_at
    BEFORE UPDATE ON users
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();

CREATE TRIGGER trigger_devices_updated_at
    BEFORE UPDATE ON devices
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();

CREATE TRIGGER trigger_vehicles_updated_at
    BEFORE UPDATE ON vehicles
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();

CREATE TRIGGER trigger_keys_updated_at
    BEFORE UPDATE ON keys
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();

-- ============================================================
-- 视图
-- ============================================================

-- 活跃密钥视图
CREATE VIEW active_keys AS
SELECT
    k.key_id,
    k.vehicle_id,
    k.device_id,
    k.user_id,
    k.key_type,
    k.protocol,
    k.permissions,
    k.valid_until,
    u.name AS user_name,
    d.device_name,
    v.vehicle_name
FROM keys k
JOIN users u ON k.user_id = u.user_id
JOIN devices d ON k.device_id = d.device_id
JOIN vehicles v ON k.vehicle_id = v.vehicle_id
WHERE k.key_state = 'active'
  AND (k.valid_until IS NULL OR k.valid_until > NOW());

-- 车辆密钥统计视图
CREATE VIEW vehicle_key_stats AS
SELECT
    vehicle_id,
    COUNT(*) AS total_keys,
    COUNT(*) FILTER (WHERE key_state = 'active') AS active_keys,
    COUNT(*) FILTER (WHERE key_state = 'pending') AS pending_keys,
    COUNT(*) FILTER (WHERE key_type = 'owner') AS owner_keys,
    COUNT(*) FILTER (WHERE key_type = 'friend') AS friend_keys
FROM keys
GROUP BY vehicle_id;

-- ============================================================
-- 初始数据
-- ============================================================

-- 插入测试用户 (可选)
-- INSERT INTO users (user_id, name, status)
-- VALUES (uuid_generate_v4(), 'Test User', 'active');

-- ============================================================
-- 注释
-- ============================================================

COMMENT ON TABLE users IS '用户表';
COMMENT ON TABLE devices IS '设备表';
COMMENT ON TABLE vehicles IS '车辆表';
COMMENT ON TABLE keys IS '密钥表';
COMMENT ON TABLE shares IS '分享表';
COMMENT ON TABLE commands IS '指令表';
COMMENT ON TABLE events IS '事件表';
COMMENT ON TABLE audit_logs IS '审计日志表';
COMMENT ON TABLE sessions IS '会话表';
