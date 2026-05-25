/******************************************************************************
 * Migration: 006_key_tracking.sql
 * Description: KTS (Key Tracking Service) 数据库表结构
 * Author: YuleTech
 * Date: 2026-05-16
 ******************************************************************************/

-- 启用 PostGIS 扩展 (如果需要地理位置功能)
-- CREATE EXTENSION IF NOT EXISTS postgis;

/******************************************************************************
 * 钥匙跟踪配置表
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS key_tracking_configs (
    id                      BIGSERIAL PRIMARY KEY,
    key_id                  VARCHAR(64) NOT NULL UNIQUE,
    enabled                 BOOLEAN DEFAULT true,
    track_location          BOOLEAN DEFAULT false,
    track_usage             BOOLEAN DEFAULT true,
    geofence_enabled        BOOLEAN DEFAULT false,
    geofence_center         JSONB,                              -- {经度, 纬度, 精度}
    geofence_radius         FLOAT DEFAULT 100.0,                -- 米
    anomaly_detection_enabled BOOLEAN DEFAULT true,
    max_usage_per_hour      INTEGER DEFAULT 30,
    created_at              TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at              TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

COMMENT ON TABLE key_tracking_configs IS '钥匙跟踪配置';
COMMENT ON COLUMN key_tracking_configs.key_id IS '钥匙唯一标识';
COMMENT ON COLUMN key_tracking_configs.enabled IS '是否启用跟踪';
COMMENT ON COLUMN key_tracking_configs.track_location IS '是否跟踪地理位置';
COMMENT ON COLUMN key_tracking_configs.geofence_enabled IS '是否启用地理围栏';
COMMENT ON COLUMN key_tracking_configs.max_usage_per_hour IS '每小时最大使用次数(用于异常检测)';

CREATE INDEX idx_tracking_configs_key_id ON key_tracking_configs(key_id);
CREATE INDEX idx_tracking_configs_enabled ON key_tracking_configs(enabled);

/******************************************************************************
 * 钥匙状态记录表
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS key_status_records (
    id              BIGSERIAL PRIMARY KEY,
    key_id          VARCHAR(64) NOT NULL,
    status          VARCHAR(32) NOT NULL,           -- active, suspended, revoked, expired, pending
    prev_status     VARCHAR(32),                    -- 上一个状态
    changed_by      VARCHAR(64) NOT NULL,           -- user_id, system, admin
    reason          TEXT,                           -- 状态变更原因
    device_id       VARCHAR(64),                    -- 发起变更的设备
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

COMMENT ON TABLE key_status_records IS '钥匙状态变化历史';
COMMENT ON COLUMN key_status_records.status IS '当前状态: active/suspended/revoked/expired/pending';
COMMENT ON COLUMN key_status_records.changed_by IS '变更发起者: user_id/system/admin';

CREATE INDEX idx_status_records_key_id ON key_status_records(key_id);
CREATE INDEX idx_status_records_created_at ON key_status_records(created_at);
CREATE INDEX idx_status_records_key_created ON key_status_records(key_id, created_at DESC);

/******************************************************************************
 * 钥匙使用记录表
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS key_usage_records (
    id                  BIGSERIAL PRIMARY KEY,
    key_id              VARCHAR(64) NOT NULL,
    usage_type          VARCHAR(32) NOT NULL,       -- unlock, lock, start, share, revoke, update, pairing, ranging
    vehicle_id          VARCHAR(64) NOT NULL,
    device_id           VARCHAR(64),                -- 移动设备ID
    user_id             VARCHAR(64) NOT NULL,
    location            JSONB,                      -- {经度, 纬度, 精度, 时间戳}
    success             BOOLEAN DEFAULT true,       -- 操作是否成功
    error_code          INTEGER,                    -- 错误码(失败时)
    error_message       TEXT,                       -- 错误信息
    session_id          VARCHAR(128) NOT NULL,      -- 会话ID
    protocol            VARCHAR(16),                -- CCC, ICCOA, ICCE
    ranging_distance    FLOAT,                      -- UWB测距结果(米)
    metadata            JSONB,                      -- 扩展字段
    created_at          TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

COMMENT ON TABLE key_usage_records IS '钥匙使用记录';
COMMENT ON COLUMN key_usage_records.usage_type IS '使用类型: unlock/lock/start/share/revoke/update/pairing/ranging';
COMMENT ON COLUMN key_usage_records.session_id IS '唯一会话标识';
COMMENT ON COLUMN key_usage_records.ranging_distance IS 'UWB测距结果(米)';

CREATE INDEX idx_usage_records_key_id ON key_usage_records(key_id);
CREATE INDEX idx_usage_records_user_id ON key_usage_records(user_id);
CREATE INDEX idx_usage_records_vehicle_id ON key_usage_records(vehicle_id);
CREATE INDEX idx_usage_records_session_id ON key_usage_records(session_id);
CREATE INDEX idx_usage_records_created_at ON key_usage_records(created_at);
CREATE INDEX idx_usage_records_key_created ON key_usage_records(key_id, created_at DESC);
CREATE INDEX idx_usage_records_type ON key_usage_records(usage_type);

-- 分区表（按月分区）
-- CREATE TABLE IF NOT EXISTS key_usage_records_2026_01 PARTITION OF key_usage_records
--     FOR VALUES FROM ('2026-01-01') TO ('2026-02-01');

/******************************************************************************
 * 钥匙使用计数器表 (用于异常检测)
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS key_usage_counters (
    id              BIGSERIAL PRIMARY KEY,
    key_id          VARCHAR(64) NOT NULL,
    hour_window     TIMESTAMP WITH TIME ZONE NOT NULL,  -- 小时窗口开始时间
    usage_count     INTEGER DEFAULT 0,
    success_count   INTEGER DEFAULT 0,
    fail_count      INTEGER DEFAULT 0,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(key_id, hour_window)
);

COMMENT ON TABLE key_usage_counters IS '钥匙使用计数(按小时)';
COMMENT ON COLUMN key_usage_counters.hour_window IS '小时窗口开始时间';

CREATE INDEX idx_usage_counters_key_hour ON key_usage_counters(key_id, hour_window);

/******************************************************************************
 * 异常事件表
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS anomaly_events (
    id              BIGSERIAL PRIMARY KEY,
    key_id          VARCHAR(64) NOT NULL,
    user_id         VARCHAR(64) NOT NULL,
    anomaly_type    VARCHAR(64) NOT NULL,       -- rapid_usage, unusual_location, unauthorized_use, replay_attack, outside_geofence
    severity        INTEGER NOT NULL CHECK (severity >= 1 AND severity <= 5),  -- 1-5
    description     TEXT NOT NULL,
    location        JSONB,                      -- 位置信息
    evidence        JSONB,                      -- 证据数据
    status          VARCHAR(32) DEFAULT 'new',  -- new, investigating, resolved, false_positive
    resolved_by     VARCHAR(64),                -- 解决人
    resolved_at     TIMESTAMP WITH TIME ZONE,   -- 解决时间
    resolution      TEXT,                       -- 解决方案
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

COMMENT ON TABLE anomaly_events IS '异常事件记录';
COMMENT ON COLUMN anomaly_events.anomaly_type IS '异常类型: rapid_usage/unusual_location/unauthorized_use/replay_attack/outside_geofence';
COMMENT ON COLUMN anomaly_events.severity IS '严重程度 1-5';
COMMENT ON COLUMN anomaly_events.status IS '状态: new/investigating/resolved/false_positive';

CREATE INDEX idx_anomaly_events_key_id ON anomaly_events(key_id);
CREATE INDEX idx_anomaly_events_user_id ON anomaly_events(user_id);
CREATE INDEX idx_anomaly_events_type ON anomaly_events(anomaly_type);
CREATE INDEX idx_anomaly_events_status ON anomaly_events(status);
CREATE INDEX idx_anomaly_events_severity ON anomaly_events(severity);
CREATE INDEX idx_anomaly_events_created_at ON anomaly_events(created_at);

/******************************************************************************
 * 钥匙实时状态表 (缓存表)
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS key_realtime_status (
    key_id                  VARCHAR(64) PRIMARY KEY,
    current_status          VARCHAR(32) DEFAULT 'active',
    last_usage_time         TIMESTAMP WITH TIME ZONE,
    last_usage_type         VARCHAR(32),
    last_location           JSONB,
    today_usage_count       INTEGER DEFAULT 0,
    total_usage_count       BIGINT DEFAULT 0,
    consecutive_failures    INTEGER DEFAULT 0,
    updated_at              TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

COMMENT ON TABLE key_realtime_status IS '钥匙实时状态(缓存)';
COMMENT ON COLUMN key_realtime_status.consecutive_failures IS '连续失败次数';

CREATE INDEX idx_realtime_status_updated ON key_realtime_status(updated_at);

/******************************************************************************
 * 触发器: 更新 updated_at
 ******************************************************************************/
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

-- 应用触发器
DROP TRIGGER IF EXISTS update_tracking_configs_updated_at ON key_tracking_configs;
CREATE TRIGGER update_tracking_configs_updated_at
    BEFORE UPDATE ON key_tracking_configs
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

DROP TRIGGER IF EXISTS update_usage_counters_updated_at ON key_usage_counters;
CREATE TRIGGER update_usage_counters_updated_at
    BEFORE UPDATE ON key_usage_counters
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

/******************************************************************************
 * 视图: 钥匙使用统计
 ******************************************************************************/
CREATE OR REPLACE VIEW key_usage_statistics AS
SELECT 
    key_id,
    DATE(created_at) as usage_date,
    usage_type,
    protocol,
    success,
    COUNT(*) as count,
    AVG(ranging_distance) as avg_distance
FROM key_usage_records
WHERE created_at >= CURRENT_DATE - INTERVAL '30 days'
GROUP BY key_id, DATE(created_at), usage_type, protocol, success;

/******************************************************************************
 * 视图: 活跃钥匙
 ******************************************************************************/
CREATE OR REPLACE VIEW active_keys_today AS
SELECT 
    k.key_id,
    k.user_id,
    k.vehicle_id,
    k.status,
    r.total_usage_count,
    r.today_usage_count,
    r.last_usage_time,
    r.consecutive_failures
FROM keys k
LEFT JOIN key_realtime_status r ON k.key_id = r.key_id
WHERE k.status = 'active'
  AND (r.last_usage_time >= CURRENT_DATE OR r.last_usage_time IS NULL);

/******************************************************************************
 * 数据清理任务
 ******************************************************************************/
-- 清理旧的计数器数据（保留30天）
CREATE OR REPLACE FUNCTION cleanup_old_counters()
RETURNS void AS $$
BEGIN
    DELETE FROM key_usage_counters
    WHERE hour_window < CURRENT_TIMESTAMP - INTERVAL '30 days';
END;
$$ LANGUAGE plpgsql;

-- 清理旧的使用记录（移到归档表）
CREATE OR REPLACE FUNCTION archive_old_usage_records()
RETURNS void AS $$
BEGIN
    -- 注意: 需要先创建归档表
    -- INSERT INTO key_usage_records_archive
    -- SELECT * FROM key_usage_records
    -- WHERE created_at < CURRENT_DATE - INTERVAL '90 days';
    
    -- DELETE FROM key_usage_records
    -- WHERE created_at < CURRENT_DATE - INTERVAL '90 days';
END;
$$ LANGUAGE plpgsql;
