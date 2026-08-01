-- ============================================================
-- dkcs 服务 PostgreSQL 初始迁移 (0001_init)
--
-- 说明:
--   * 本迁移由 dkcs 服务启动时的轻量迁移执行器自动应用
--     (backend/dkcs/internal/migrate), 也可通过
--     scripts/migrate-dkcs.sh 手动执行。
--   * 表结构以 backend/dkcs/internal/repository 实际使用的
--     实体为准 (vehicles / digital_keys / events), 与
--     backend/db/schema.sql (设计参考) 中的扩展表不冲突。
--   * 方言: PostgreSQL 16+ (TIMESTAMPTZ / JSONB / DOUBLE PRECISION)。
-- ============================================================

-- ── vehicles 车辆表 ──
CREATE TABLE IF NOT EXISTS vehicles (
    id            TEXT PRIMARY KEY,              -- 车辆 ID (UUID 字符串)
    owner_id      TEXT,                          -- 车主用户 ID
    vin           TEXT UNIQUE,                   -- VIN 码
    brand         TEXT,
    model         TEXT,
    year          INTEGER DEFAULT 0,
    color         TEXT,
    plate_number  TEXT,
    tcu_id        TEXT,                          -- TCU 设备 ID
    protocol      TEXT,                          -- CCC / ICCOA / ICCE
    features      JSONB DEFAULT '[]'::jsonb,     -- 支持的功能列表
    is_online     BOOLEAN DEFAULT false,
    last_online   TIMESTAMPTZ,
    battery_level INTEGER DEFAULT 0,
    odometer      INTEGER DEFAULT 0,
    latitude      DOUBLE PRECISION DEFAULT 0,
    longitude     DOUBLE PRECISION DEFAULT 0,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_vehicles_owner ON vehicles(owner_id);
CREATE INDEX IF NOT EXISTS idx_vehicles_tcu   ON vehicles(tcu_id);

-- ── digital_keys 数字钥匙表 ──
CREATE TABLE IF NOT EXISTS digital_keys (
    id            TEXT PRIMARY KEY,              -- 钥匙 ID (UUID 字符串)
    vehicle_id    TEXT NOT NULL,                 -- 关联车辆
    user_id       TEXT NOT NULL,                 -- 关联用户
    key_type      TEXT NOT NULL,                 -- primary/friend/service/temporary
    status        TEXT NOT NULL DEFAULT 'pending', -- pending/active/suspended/expired/revoked
    permissions   JSONB DEFAULT '[]'::jsonb,     -- 权限列表
    secret        TEXT,                          -- 密钥密文 (加密存储)
    parent_key_id TEXT,                          -- 父钥匙 ID (分享链)
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    activated_at  TIMESTAMPTZ,
    expires_at    TIMESTAMPTZ,
    revoked_at    TIMESTAMPTZ,
    revoke_reason TEXT,
    metadata      JSONB DEFAULT '{}'::jsonb
);

CREATE INDEX IF NOT EXISTS idx_keys_vehicle ON digital_keys(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_keys_user    ON digital_keys(user_id);
CREATE INDEX IF NOT EXISTS idx_keys_status  ON digital_keys(status);

-- ── events 事件表 ──
CREATE TABLE IF NOT EXISTS events (
    id         TEXT PRIMARY KEY,                 -- 事件 ID (UUID 字符串)
    type       TEXT NOT NULL,                    -- 事件类型
    vehicle_id TEXT NOT NULL,
    user_id    TEXT NOT NULL,
    key_id     TEXT,
    data       JSONB DEFAULT '{}'::jsonb,        -- 事件负载
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_events_vehicle ON events(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_events_user    ON events(user_id);
CREATE INDEX IF NOT EXISTS idx_events_key     ON events(key_id);
CREATE INDEX IF NOT EXISTS idx_events_created ON events(created_at DESC);

-- ── updated_at 自动更新触发器 ──
CREATE OR REPLACE FUNCTION update_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_vehicles_updated_at ON vehicles;
CREATE TRIGGER trigger_vehicles_updated_at
    BEFORE UPDATE ON vehicles
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();
