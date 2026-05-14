-- 初始化数据库脚本
-- yuleDKCS 数字钥匙系统

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(100) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    phone VARCHAR(20),
    password VARCHAR(255) NOT NULL,
    role VARCHAR(20) DEFAULT 'user',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    deleted_at TIMESTAMP
);

-- 车辆表
CREATE TABLE IF NOT EXISTS vehicles (
    id SERIAL PRIMARY KEY,
    vin VARCHAR(17) UNIQUE NOT NULL,
    brand VARCHAR(50) NOT NULL,
    model VARCHAR(100) NOT NULL,
    year INTEGER,
    color VARCHAR(30),
    plate_number VARCHAR(20),
    owner_id INTEGER REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'active',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 数字钥匙表
CREATE TABLE IF NOT EXISTS keys (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) NOT NULL,
    vehicle_id INTEGER REFERENCES vehicles(id) NOT NULL,
    type VARCHAR(20) NOT NULL, -- owner, shared
    status VARCHAR(20) DEFAULT 'active', -- active, inactive, expired, revoked
    protocol VARCHAR(10) NOT NULL, -- CCC, ICCOA, ICCE
    key_identifier VARCHAR(255) UNIQUE NOT NULL,
    name VARCHAR(100),
    description TEXT,
    permissions JSONB DEFAULT '{}',
    expires_at TIMESTAMP,
    last_used_at TIMESTAMP,
    usage_count INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 钥匙分享表
CREATE TABLE IF NOT EXISTS key_shares (
    id SERIAL PRIMARY KEY,
    key_id INTEGER REFERENCES keys(id) NOT NULL,
    owner_id INTEGER REFERENCES users(id) NOT NULL,
    shared_to_id INTEGER REFERENCES users(id) NOT NULL,
    permissions JSONB DEFAULT '{}',
    status VARCHAR(20) DEFAULT 'active', -- active, revoked
    expires_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 钥匙使用日志表
CREATE TABLE IF NOT EXISTS key_usage_logs (
    id SERIAL PRIMARY KEY,
    key_id INTEGER REFERENCES keys(id) NOT NULL,
    user_id INTEGER REFERENCES users(id) NOT NULL,
    operation VARCHAR(50) NOT NULL, -- unlock, lock, start_engine, etc.
    status VARCHAR(20) NOT NULL, -- success, failed
    device_info JSONB,
    location JSONB,
    error_message TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 索引
CREATE INDEX IF NOT EXISTS idx_keys_user_id ON keys(user_id);
CREATE INDEX IF NOT EXISTS idx_keys_vehicle_id ON keys(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_keys_status ON keys(status);
CREATE INDEX IF NOT EXISTS idx_key_shares_key_id ON key_shares(key_id);
CREATE INDEX IF NOT EXISTS idx_key_shares_shared_to ON key_shares(shared_to_id);
CREATE INDEX IF NOT EXISTS idx_logs_key_id ON key_usage_logs(key_id);
CREATE INDEX IF NOT EXISTS idx_logs_created_at ON key_usage_logs(created_at);

-- 更新时间戳触发器
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

CREATE TRIGGER update_users_updated_at BEFORE UPDATE ON users
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_vehicles_updated_at BEFORE UPDATE ON vehicles
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_keys_updated_at BEFORE UPDATE ON keys
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_key_shares_updated_at BEFORE UPDATE ON key_shares
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
