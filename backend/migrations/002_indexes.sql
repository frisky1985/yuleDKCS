-- 数据库索引优化迁移
-- 执行方式: migrate -path migrations -database "postgresql://user:pass@localhost/dbname?sslmode=disable" up

-- 用户表索引
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_phone ON users(phone);
CREATE INDEX IF NOT EXISTS idx_users_created_at ON users(created_at);

-- 车辆表索引
CREATE INDEX IF NOT EXISTS idx_vehicles_vin ON vehicles(vin);
CREATE INDEX IF NOT EXISTS idx_vehicles_owner_id ON vehicles(owner_id);
CREATE INDEX IF NOT EXISTS idx_vehicles_status ON vehicles(status);
CREATE INDEX IF NOT EXISTS idx_vehicles_created_at ON vehicles(created_at);

-- 钥匙表索引
CREATE INDEX IF NOT EXISTS idx_keys_user_id ON keys(user_id);
CREATE INDEX IF NOT EXISTS idx_keys_vehicle_id ON keys(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_keys_status ON keys(status);
CREATE INDEX IF NOT EXISTS idx_keys_type ON keys(type);
CREATE INDEX IF NOT EXISTS idx_keys_expires_at ON keys(expires_at);
CREATE INDEX IF NOT EXISTS idx_keys_created_at ON keys(created_at);

-- 钥匙分享表索引
CREATE INDEX IF NOT EXISTS idx_key_shares_key_id ON key_shares(key_id);
CREATE INDEX IF NOT EXISTS idx_key_shares_shared_to ON key_shares(shared_to);
CREATE INDEX IF NOT EXISTS idx_key_shares_status ON key_shares(status);

-- 车辆命令表索引
CREATE INDEX IF NOT EXISTS idx_vehicle_commands_vehicle_id ON vehicle_commands(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_vehicle_commands_user_id ON vehicle_commands(user_id);
CREATE INDEX IF NOT EXISTS idx_vehicle_commands_status ON vehicle_commands(status);
CREATE INDEX IF NOT EXISTS idx_vehicle_commands_created_at ON vehicle_commands(created_at);

-- 车辆状态表索引
CREATE INDEX IF NOT EXISTS idx_vehicle_status_vehicle_id ON vehicle_status(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_vehicle_status_last_seen ON vehicle_status(last_seen);

-- 固件表索引
CREATE INDEX IF NOT EXISTS idx_firmwares_version ON firmwares(version);
CREATE INDEX IF NOT EXISTS idx_firmwares_type ON firmwares(type);

-- 复合索引 (用于常用查询)
CREATE INDEX IF NOT EXISTS idx_keys_user_status ON keys(user_id, status);
CREATE INDEX IF NOT EXISTS idx_keys_vehicle_status ON keys(vehicle_id, status);
CREATE INDEX IF NOT EXISTS idx_vehicles_owner_status ON vehicles(owner_id, status);

-- 创建部分索引注释说明
COMMENT ON INDEX idx_users_email IS '用于登录查询';
COMMENT ON INDEX idx_vehicles_vin IS '用于 VIN 查询车辆';
COMMENT ON INDEX idx_keys_user_status IS '用于获取用户有效钥匙';
